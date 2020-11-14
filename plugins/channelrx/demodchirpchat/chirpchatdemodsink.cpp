///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <QTime>
#include <QDebug>
#include <stdio.h>

#include "dsp/dsptypes.h"
#include "dsp/basebandsamplesink.h"
#include "dsp/dspengine.h"
#include "dsp/fftfactory.h"
#include "dsp/fftengine.h"
#include "util/db.h"

#include "chirpchatdemodmsg.h"
#include "chirpchatdemodsink.h"

ChirpChatDemodSink::ChirpChatDemodSink() :
    m_decodeMsg(nullptr),
    m_decoderMsgQueue(nullptr),
    m_fftSequence(-1),
    m_fftSFDSequence(-1),
    m_downChirps(nullptr),
    m_upChirps(nullptr),
    m_spectrumLine(nullptr),
    m_spectrumSink(nullptr),
    m_spectrumBuffer(nullptr)
{
    m_demodActive = false;
	m_bandwidth = ChirpChatDemodSettings::bandwidths[0];
	m_channelSampleRate = 96000;
	m_channelFrequencyOffset = 0;
	m_nco.setFreq(m_channelFrequencyOffset, m_channelSampleRate);
	m_interpolator.create(16, m_channelSampleRate, m_bandwidth / 1.9f);
	m_interpolatorDistance = (Real) m_channelSampleRate / (Real) m_bandwidth;
    m_sampleDistanceRemain = 0;

    m_state = ChirpChatStateReset;
	m_chirp = 0;
	m_chirp0 = 0;

    initSF(m_settings.m_spreadFactor, m_settings.m_deBits, m_settings.m_fftWindow);
}

ChirpChatDemodSink::~ChirpChatDemodSink()
{
    FFTFactory *fftFactory = DSPEngine::instance()->getFFTFactory();

    if (m_fftSequence >= 0)
    {
        fftFactory->releaseEngine(m_interpolatedFFTLength, false, m_fftSequence);
        fftFactory->releaseEngine(m_interpolatedFFTLength, false, m_fftSFDSequence);
    }

    delete[] m_downChirps;
    delete[] m_upChirps;
    delete[] m_spectrumBuffer;
    delete[] m_spectrumLine;
}

void ChirpChatDemodSink::initSF(unsigned int sf, unsigned int deBits, FFTWindow::Function fftWindow)
{
    if (m_downChirps) {
        delete[] m_downChirps;
    }
    if (m_upChirps) {
        delete[] m_upChirps;
    }
    if (m_spectrumBuffer) {
        delete[] m_spectrumBuffer;
    }
    if (m_spectrumLine) {
        delete[] m_spectrumLine;
    }

    FFTFactory *fftFactory = DSPEngine::instance()->getFFTFactory();

    if (m_fftSequence >= 0)
    {
        fftFactory->releaseEngine(m_interpolatedFFTLength, false, m_fftSequence);
        fftFactory->releaseEngine(m_interpolatedFFTLength, false, m_fftSFDSequence);
    }

    m_nbSymbols = 1 << sf;
    m_nbSymbolsEff = 1 << (sf - deBits);
    m_deLength = 1 << deBits;
    m_fftLength = m_nbSymbols;
    m_fftWindow.create(fftWindow, m_fftLength);
    m_fftWindow.setKaiserAlpha(M_PI);
    m_interpolatedFFTLength = m_fftInterpolation*m_fftLength;
    m_preambleTolerance = (m_deLength*m_fftInterpolation)/2;
    m_fftSequence = fftFactory->getEngine(m_interpolatedFFTLength, false, &m_fft);
    m_fftSFDSequence = fftFactory->getEngine(m_interpolatedFFTLength, false, &m_fftSFD);
    m_state = ChirpChatStateReset;
    m_sfdSkip = m_fftLength / 4;
    m_downChirps = new Complex[2*m_nbSymbols]; // Each table is 2 chirps long to allow processing from arbitrary offsets.
    m_upChirps = new Complex[2*m_nbSymbols];
    m_spectrumBuffer = new Complex[m_nbSymbols];
    m_spectrumLine = new Complex[m_nbSymbols];
    std::fill(m_spectrumLine, m_spectrumLine+m_nbSymbols, Complex(std::polar(1e-6*SDR_RX_SCALED, 0.0)));

    float halfAngle = M_PI;
    float phase = -halfAngle;
    double accumulator = 0;

    for (unsigned int i = 0; i < m_fftLength; i++)
    {
        accumulator = fmod(accumulator + phase, 2*M_PI);
        m_downChirps[i] = Complex(std::conj(std::polar(1.0, accumulator)));
        m_upChirps[i] = Complex(std::polar(1.0, accumulator));
        phase += (2*halfAngle) / m_nbSymbols;
    }

    // Duplicate table to allow processing from arbitrary offsets
    std::copy(m_downChirps, m_downChirps+m_fftLength, m_downChirps+m_fftLength);
    std::copy(m_upChirps, m_upChirps+m_fftLength, m_upChirps+m_fftLength);
}

void ChirpChatDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
	Complex ci;

	for (SampleVector::const_iterator it = begin; it < end; ++it)
	{
		Complex c(it->real() / SDR_RX_SCALEF, it->imag() / SDR_RX_SCALEF);
		c *= m_nco.nextIQ();

		if (m_interpolator.decimate(&m_sampleDistanceRemain, c, &ci))
		{
            processSample(ci);
			m_sampleDistanceRemain += m_interpolatorDistance;
		}
	}
}

void ChirpChatDemodSink::processSample(const Complex& ci)
{
    if (m_state == ChirpChatStateReset) // start over
    {
        m_demodActive = false;
        reset();
        std::queue<double>().swap(m_magsqQueue); // this clears the queue
        m_state = ChirpChatStateDetectPreamble;
    }
    else if (m_state == ChirpChatStateDetectPreamble) // look for preamble
    {
        m_fft->in()[m_fftCounter++] = ci * m_downChirps[m_chirp]; // de-chirp the up ramp

        if (m_fftCounter == m_fftLength)
        {
            m_fftWindow.apply(m_fft->in());
            std::fill(m_fft->in()+m_fftLength, m_fft->in()+m_interpolatedFFTLength, Complex{0.0, 0.0});
            m_fft->transform();
            m_fftCounter = 0;
            double magsq, magsqTotal;

            unsigned int imax = argmax(
                m_fft->out(),
                m_fftInterpolation,
                m_fftLength,
                magsq,
                magsqTotal,
                m_spectrumBuffer,
                m_fftInterpolation
            ) / m_fftInterpolation;

            if (m_magsqQueue.size() > m_settings.m_preambleChirps) {
                m_magsqQueue.pop();
            }

            m_magsqTotalAvg(magsqTotal);
            m_magsqQueue.push(magsq);
            m_argMaxHistory[m_argMaxHistoryCounter++] = imax;

            if (m_argMaxHistoryCounter == m_requiredPreambleChirps)
            {
                m_argMaxHistoryCounter = 0;
                bool preambleFound = true;

                for (unsigned int i = 1; i < m_requiredPreambleChirps; i++)
                {
                    int delta = m_argMaxHistory[i] - m_argMaxHistory[i-1];
                    // qDebug("ChirpChatDemodSink::processSample: search: delta: %d / %d", delta, m_deLength);

                    if ((delta < -m_preambleTolerance) || (delta > m_preambleTolerance))
                    {
                        preambleFound = false;
                        break;
                    }

                    // if (m_argMaxHistory[0] != m_argMaxHistory[i])
                    // {
                    //     preambleFound = false;
                    //     break;
                    // }
                }

                if ((preambleFound) && (magsq > 1e-9))
                {
                    if (m_spectrumSink) {
                        m_spectrumSink->feed(m_spectrumBuffer, m_nbSymbols);
                    }

                    qDebug("ChirpChatDemodSink::processSample: preamble found: %u|%f", m_argMaxHistory[0], magsq);
                    m_chirp = m_argMaxHistory[0];
                    m_fftCounter = m_chirp;
                    m_chirp0 = 0;
                    m_chirpCount = 0;
                    m_state = ChirpChatStatePreambleResyc;
                }
                else
                {
                    m_magsqOffAvg(m_magsqQueue.front());
                }
            }
        }
    }
    else if (m_state == ChirpChatStatePreambleResyc)
    {
        m_fftCounter++;

        if (m_fftCounter == m_fftLength)
        {
            if (m_spectrumSink) {
                m_spectrumSink->feed(m_spectrumLine, m_nbSymbols);
            }

            m_fftCounter = 0;
            m_demodActive = true;
            m_state = ChirpChatStatePreamble;
        }
    }
    else if (m_state == ChirpChatStatePreamble) // preamble found look for SFD start
    {
        m_fft->in()[m_fftCounter] = ci * m_downChirps[m_chirp];  // de-chirp the up ramp
        m_fftSFD->in()[m_fftCounter] = ci * m_upChirps[m_chirp]; // de-chirp the down ramp
        m_fftCounter++;

        if (m_fftCounter == m_fftLength)
        {
            m_fftWindow.apply(m_fft->in());
            std::fill(m_fft->in()+m_fftLength, m_fft->in()+m_interpolatedFFTLength, Complex{0.0, 0.0});
            m_fft->transform();

            m_fftWindow.apply(m_fftSFD->in());
            std::fill(m_fftSFD->in()+m_fftLength, m_fftSFD->in()+m_interpolatedFFTLength, Complex{0.0, 0.0});
            m_fftSFD->transform();

            m_fftCounter = 0;
            double magsqPre, magsqSFD;
            double magsqTotal, magsqSFDTotal;

            unsigned int imaxSFD = argmax(
                m_fftSFD->out(),
                m_fftInterpolation,
                m_fftLength,
                magsqSFD,
                magsqTotal,
                nullptr,
                m_fftInterpolation
            ) / m_fftInterpolation;

            unsigned int imax = argmax(
                m_fft->out(),
                m_fftInterpolation,
                m_fftLength,
                magsqPre,
                magsqSFDTotal,
                m_spectrumBuffer,
                m_fftInterpolation
            ) / m_fftInterpolation;

            m_preambleHistory[m_chirpCount] = imax;
            m_chirpCount++;

            if (magsqPre <  magsqSFD) // preamble drop
            {
                m_magsqTotalAvg(magsqSFDTotal);

                if (m_chirpCount < 1 + (m_settings.hasSyncWord() ? 2 : 0)) // too early
                {
                    m_state = ChirpChatStateReset;
                    qDebug("ChirpChatDemodSink::processSample: SFD search: signal drop is too early");
                }
                else
                {
                    if (m_settings.hasSyncWord())
                    {
                        m_syncWord = round(m_preambleHistory[m_chirpCount-2] / 8.0);
                        m_syncWord += 16 * round(m_preambleHistory[m_chirpCount-3] / 8.0);
                        qDebug("ChirpChatDemodSink::processSample: SFD found:  pre: %4u|%11.6f - sfd: %4u|%11.6f sync: %x", imax, magsqPre, imaxSFD, magsqSFD, m_syncWord);
                    }
                    else
                    {
                        qDebug("ChirpChatDemodSink::processSample: SFD found:  pre: %4u|%11.6f - sfd: %4u|%11.6f", imax, magsqPre, imaxSFD, magsqSFD);
                    }

                    int sadj = 0;
                    int nadj = 0;
                    int zadj;
                    int sfdSkip = m_sfdSkip;

                    for (unsigned int i = 0; i < m_chirpCount - 1 - (m_settings.hasSyncWord() ? 2 : 0); i++)
                    {
                        sadj += m_preambleHistory[i] > m_nbSymbols/2 ? m_preambleHistory[i] - m_nbSymbols : m_preambleHistory[i];
                        nadj++;
                    }

                    zadj = nadj == 0 ? 0 : sadj / nadj;
                    zadj = zadj < -(sfdSkip/2) ? -(sfdSkip/2) : zadj > sfdSkip/2 ? sfdSkip/2 : zadj;
                    qDebug("ChirpChatDemodSink::processSample: zero adjust: %d (%d)", zadj, nadj);

                    m_sfdSkipCounter = 0;
                    m_fftCounter = m_fftLength - m_sfdSkip + zadj;
                    m_chirp += zadj;
                    m_state = ChirpChatStateSkipSFD; //ChirpChatStateSlideSFD;
                }
            }
            else if (m_chirpCount > (m_settings.m_preambleChirps - m_requiredPreambleChirps + 2)) // SFD missed start over
            {
                qDebug("ChirpChatDemodSink::processSample: SFD search: number of possible chirps exceeded");
                m_magsqTotalAvg(magsqTotal);
                m_state = ChirpChatStateReset;
            }
            else
            {
                if (m_spectrumSink) {
                    m_spectrumSink->feed(m_spectrumBuffer, m_nbSymbols);
                }

                qDebug("ChirpChatDemodSink::processSample: SFD search: pre: %4u|%11.6f - sfd: %4u|%11.6f", imax, magsqPre, imaxSFD, magsqSFD);
                m_magsqTotalAvg(magsqTotal);
                m_magsqOnAvg(magsqPre);
            }
        }
    }
    else if (m_state == ChirpChatStateSkipSFD) // Just skip the rest of SFD
    {
        m_fftCounter++;

        if (m_fftCounter == m_fftLength)
        {
            m_fftCounter = m_fftLength - m_sfdSkip;
            m_sfdSkipCounter++;

            if (m_sfdSkipCounter == m_settings.getNbSFDFourths() - 4U) // SFD chips fourths less one full period
            {
                qDebug("ChirpChatDemodSink::processSample: SFD skipped");
                m_chirp = m_chirp0;
                m_fftCounter = 0;
                m_chirpCount = 0;
                m_magsqMax = 0.0;
                m_decodeMsg = ChirpChatDemodMsg::MsgDecodeSymbols::create();
                m_decodeMsg->setSyncWord(m_syncWord);
                m_state = ChirpChatStateReadPayload;
            }
        }
    }
    else if (m_state == ChirpChatStateReadPayload)
    {
        m_fft->in()[m_fftCounter] = ci * m_downChirps[m_chirp]; // de-chirp the up ramp
        m_fftCounter++;

        if (m_fftCounter == m_fftLength)
        {
            m_fftWindow.apply(m_fft->in());
            std::fill(m_fft->in()+m_fftLength, m_fft->in()+m_interpolatedFFTLength, Complex{0.0, 0.0});
            m_fft->transform();
            m_fftCounter = 0;
            double magsq, magsqTotal;

            unsigned short symbol = evalSymbol(
                argmax(
                    m_fft->out(),
                    m_fftInterpolation,
                    m_fftLength,
                    magsq,
                    magsqTotal,
                    m_spectrumBuffer,
                    m_fftInterpolation
                )
            ) % m_nbSymbolsEff;

            if (m_spectrumSink) {
                m_spectrumSink->feed(m_spectrumBuffer, m_nbSymbols);
            }

            if (magsq > m_magsqMax) {
                m_magsqMax = magsq;
            }

            m_magsqTotalAvg(magsq);

            m_decodeMsg->pushBackSymbol(symbol);

            if ((m_chirpCount == 0)
            ||  (m_settings.m_eomSquelchTenths == 121) // max - disable squelch
            || ((m_settings.m_eomSquelchTenths*magsq)/10.0 > m_magsqMax))
            {
                qDebug("ChirpChatDemodSink::processSample: symbol %02u: %4u|%11.6f", m_chirpCount, symbol, magsq);
                m_magsqOnAvg(magsq);
                m_chirpCount++;

                if (m_chirpCount > m_settings.m_nbSymbolsMax)
                {
                    qDebug("ChirpChatDemodSink::processSample: message length exceeded");
                    m_state = ChirpChatStateReset;
                    m_decodeMsg->setSignalDb(CalcDb::dbPower(m_magsqOnAvg.asDouble() / (1<<m_settings.m_spreadFactor)));
                    m_decodeMsg->setNoiseDb(CalcDb::dbPower(m_magsqOffAvg.asDouble() / (1<<m_settings.m_spreadFactor)));

                    if (m_decoderMsgQueue && m_settings.m_decodeActive) {
                        m_decoderMsgQueue->push(m_decodeMsg);
                    } else {
                        delete m_decodeMsg;
                    }
                }
            }
            else
            {
                qDebug("ChirpChatDemodSink::processSample: end of message");
                m_state = ChirpChatStateReset;
                m_decodeMsg->popSymbol(); // last symbol is garbage
                m_decodeMsg->setSignalDb(CalcDb::dbPower(m_magsqOnAvg.asDouble() / (1<<m_settings.m_spreadFactor)));
                m_decodeMsg->setNoiseDb(CalcDb::dbPower(m_magsqOffAvg.asDouble() / (1<<m_settings.m_spreadFactor)));

                if (m_decoderMsgQueue && m_settings.m_decodeActive) {
                    m_decoderMsgQueue->push(m_decodeMsg);
                } else {
                    delete m_decodeMsg;
                }
            }
        }
    }
    else
    {
        m_state = ChirpChatStateReset;
    }

    m_chirp++;

    if (m_chirp >= m_chirp0 + m_nbSymbols) {
        m_chirp = m_chirp0;
    }
}

void ChirpChatDemodSink::reset()
{
    m_chirp = 0;
    m_chirp0 = 0;
    m_fftCounter = 0;
    m_argMaxHistoryCounter = 0;
    m_sfdSkipCounter = 0;
}

unsigned int ChirpChatDemodSink::argmax(
    const Complex *fftBins,
    unsigned int fftMult,
    unsigned int fftLength,
    double& magsqMax,
    double& magsqTotal,
    Complex *specBuffer,
    unsigned int specDecim)
{
    magsqMax = 0.0;
    magsqTotal = 0.0;
    unsigned int imax = 0;
    double magSum = 0.0;

    for (unsigned int i = 0; i < fftMult*fftLength; i++)
    {
        double magsq = std::norm(fftBins[i]);
        magsqTotal += magsq;

        if (magsq > magsqMax)
        {
            imax = i;
            magsqMax = magsq;
        }

        if (specBuffer)
        {
            magSum += magsq;

            if (i % specDecim == specDecim - 1)
            {
                specBuffer[i/specDecim] = Complex(std::polar(magSum, 0.0));
                magSum = 0.0;
            }
        }
    }

    magsqTotal /= fftMult*fftLength;

    return imax;
}

unsigned int ChirpChatDemodSink::argmaxSpreaded(
    const Complex *fftBins,
    unsigned int fftMult,
    unsigned int fftLength,
    double& magsqMax,
    double& magsqNoise,
    double& magSqTotal,
    Complex *specBuffer,
    unsigned int specDecim)
{
    magsqMax = 0.0;
    magsqNoise = 0.0;
    magSqTotal = 0.0;
    unsigned int imax = 0;
    double magSum = 0.0;
    double magSymbol = 0.0;
    unsigned int spread = fftMult * (1<<m_settings.m_deBits);
    unsigned int istart = fftMult*fftLength - spread/2 + 1;

    for (unsigned int i2 = istart; i2 < istart + fftMult*fftLength; i2++)
    {
        unsigned int i = i2 % (fftMult*fftLength);
        double magsq = std::norm(fftBins[i]);
        magSymbol += magsq;
        magSqTotal += magsq;

        if (i % spread == spread/2) // boundary (inclusive)
        {
            if (magSymbol > magsqMax)
            {
                magsqMax = magSymbol;
                imax = i;
            }

            magsqNoise += magSymbol;
            magSymbol = 0.0;
        }

        if (specBuffer)
        {
            magSum += magsq;

            if (i % specDecim == specDecim - 1)
            {
                specBuffer[i/specDecim] = Complex(std::polar(magSum, 0.0));
                magSum = 0.0;
            }
        }
    }

    magsqNoise -= magsqMax;
    magsqNoise /= fftLength;
    magSqTotal /= fftMult*fftLength;

    return imax / spread;
}

void ChirpChatDemodSink::decimateSpectrum(Complex *in, Complex *out, unsigned int size, unsigned int decimation)
{
    for (unsigned int i = 0; i < size; i++)
    {
        if (i % decimation == 0) {
            out[i/decimation] = in[i];
        }
    }
}

int ChirpChatDemodSink::toSigned(int u, int intSize)
{
    if (u > intSize/2) {
        return u - intSize;
    } else {
        return u;
    }
}

unsigned int ChirpChatDemodSink::evalSymbol(unsigned int rawSymbol)
{
    unsigned int spread = m_fftInterpolation * (1<<m_settings.m_deBits);

    if (spread < 2 ) {
        return rawSymbol;
    } else {
        return (rawSymbol + spread/2 - 1) / spread; // middle point goes to symbol below (smear to the right)
    }
}

void ChirpChatDemodSink::applyChannelSettings(int channelSampleRate, int bandwidth, int channelFrequencyOffset, bool force)
{
    qDebug() << "ChirpChatDemodSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset
            << " bandwidth: " << bandwidth;

    if ((channelFrequencyOffset != m_channelFrequencyOffset) ||
        (channelSampleRate != m_channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((channelSampleRate != m_channelSampleRate) ||
        (bandwidth != m_bandwidth) || force)
    {
        m_interpolator.create(16, channelSampleRate, bandwidth / 1.25f);
        m_interpolatorDistance = (Real) channelSampleRate / (Real) bandwidth;
        m_sampleDistanceRemain = 0;
        qDebug() << "ChirpChatDemodSink::applyChannelSettings: m_interpolator.create:"
            << " m_interpolatorDistance: " << m_interpolatorDistance;
    }

    m_channelSampleRate = channelSampleRate;
    m_bandwidth = bandwidth;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void ChirpChatDemodSink::applySettings(const ChirpChatDemodSettings& settings, bool force)
{
    qDebug() << "ChirpChatDemodSink::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_bandwidthIndex: " << settings.m_bandwidthIndex
            << " m_spreadFactor: " << settings.m_spreadFactor
            << " m_rgbColor: " << settings.m_rgbColor
            << " m_title: " << settings.m_title
            << " force: " << force;

    if ((settings.m_spreadFactor != m_settings.m_spreadFactor)
     || (settings.m_deBits != m_settings.m_deBits)
     || (settings.m_fftWindow != m_settings.m_fftWindow) || force) {
        initSF(settings.m_spreadFactor, settings.m_deBits, settings.m_fftWindow);
    }

    m_settings = settings;
}
