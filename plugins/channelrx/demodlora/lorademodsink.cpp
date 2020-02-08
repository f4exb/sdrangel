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
#include "dsp/fftengine.h"

#include "lorademodsink.h"

LoRaDemodSink::LoRaDemodSink() :
    m_spectrumSink(nullptr),
    m_spectrumBuffer(nullptr),
    m_downChirps(nullptr),
    m_upChirps(nullptr),
    m_fftBuffer(nullptr)
{
	m_bandwidth = LoRaDemodSettings::bandwidths[0];
	m_channelSampleRate = 96000;
	m_channelFrequencyOffset = 0;
	m_nco.setFreq(m_channelFrequencyOffset, m_channelSampleRate);
	m_interpolator.create(16, m_channelSampleRate, m_bandwidth / 1.9f);
	m_interpolatorDistance = (Real) m_channelSampleRate / (Real) m_bandwidth;
    m_sampleDistanceRemain = 0;

    m_state = LoRaStateReset;
	m_chirp = 0;
	m_chirp0 = 0;

    m_fft = FFTEngine::create();
    m_fftSFD = FFTEngine::create();

    initSF(m_settings.m_spreadFactor);
}

LoRaDemodSink::~LoRaDemodSink()
{
    delete m_fft;
    delete m_fftSFD;
    delete[] m_downChirps;
    delete[] m_upChirps;
    delete[] m_spectrumBuffer;
}

void LoRaDemodSink::initSF(unsigned int sf)
{
    if (m_downChirps) {
        delete[] m_downChirps;
    }
    if (m_upChirps) {
        delete[] m_upChirps;
    }
    if (m_fftBuffer) {
        delete[] m_fftBuffer;
    }
    if (m_spectrumBuffer) {
        delete[] m_spectrumBuffer;
    }

    m_nbSymbols = 1 << sf;
    m_fftLength = m_nbSymbols;
    m_fft->configure(m_fftInterpolation*m_fftLength, false);
    m_fftSFD->configure(m_fftInterpolation*m_fftLength, false);
    m_state = LoRaStateReset;
    m_sfdSkip = m_fftLength / 4;
    m_fftWindow.create(FFTWindow::Function::Kaiser, m_fftLength);
    m_fftWindow.setKaiserAlpha(M_PI);
    m_downChirps = new Complex[2*m_nbSymbols]; // Each table is 2 chirps long to allow memcpying from arbitrary offsets.
    m_upChirps = new Complex[2*m_nbSymbols];
    m_fftBuffer = new Complex[m_fftInterpolation*m_fftLength];
    m_spectrumBuffer = new Complex[m_nbSymbols];

    float halfAngle = M_PI;
    float phase = -halfAngle;
    double accumulator = 0;

    for (int i = 0; i < 2*m_nbSymbols; i++)
    {
        accumulator = fmod(accumulator + phase, 2*M_PI);
        m_downChirps[i] = Complex(std::conj(std::polar(1.0, accumulator)));
        m_upChirps[i] = Complex(std::polar(1.0, accumulator));
        phase += (2*halfAngle) / m_nbSymbols;
    }
}

void LoRaDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
	int newangle;
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

void LoRaDemodSink::processSample(const Complex& ci)
{
    if (m_state == LoRaStateReset) // start over
    {
        reset();
        m_state = LoRaStateDetectPreamble;
    }
    else if (m_state == LoRaStateDetectPreamble) // look for preamble
    {
        m_fft->in()[m_fftCounter++] = ci * m_downChirps[m_chirp]; // de-chirp the up ramp

        if (m_fftCounter == m_fftLength)
        {
            m_fftWindow.apply(m_fft->in());
            std::fill(m_fft->in()+m_fftLength, m_fft->in()+m_fftInterpolation*m_fftLength, Complex{0.0, 0.0});
            m_fft->transform();
            m_fftCounter = 0;

            unsigned int imax = argmax(
                m_fft->out(),
                m_fftInterpolation,
                m_fftLength,
                m_magsq,
                m_spectrumBuffer,
                m_fftInterpolation
            ) / m_fftInterpolation;

            // Debug:
            // if (m_spectrumSink) {
            //     m_spectrumSink->feed(m_spectrumBuffer, m_nbSymbols);
            // }

            m_argMaxHistory[m_argMaxHistoryCounter++] = imax;

            if (m_argMaxHistoryCounter == m_requiredPreambleChirps)
            {
                m_argMaxHistoryCounter = 0;
                bool preambleFound = true;

                for (int i = 1; i < m_requiredPreambleChirps; i++)
                {
                    if (m_argMaxHistory[0] != m_argMaxHistory[i])
                    {
                        preambleFound = false;
                        break;
                    }
                }

                if (preambleFound)
                {
                    if (m_spectrumSink) {
                        m_spectrumSink->feed(m_spectrumBuffer, m_nbSymbols);
                    }

                    qDebug("LoRaDemodSink::processSample: preamble found: %u|%f", m_argMaxHistory[0], m_magsq);
                    m_chirp0 = m_argMaxHistory[0];
                    m_chirp = m_chirp0;
                    m_fftCounter = 0;
                    m_chirpCount = 0;
                    m_state = LoRaStatePreamble;
                }
            }
        }
    }
    else if (m_state == LoRaStatePreamble) // preamble found look for SFD start
    {
        m_fft->in()[m_fftCounter] = ci * m_downChirps[m_chirp];  // de-chirp the up ramp
        m_fftSFD->in()[m_fftCounter] = ci * m_upChirps[m_chirp]; // de-chiro the down ramp
        m_fftCounter++;

        if (m_fftCounter == m_fftLength)
        {
            std::copy(m_fftSFD->in(), m_fftSFD->in() + m_fftLength, m_fftBuffer); // save for later

            m_fftWindow.apply(m_fft->in());
            std::fill(m_fft->in()+m_fftLength, m_fft->in()+m_fftInterpolation*m_fftLength, Complex{0.0, 0.0});
            m_fft->transform();

            m_fftWindow.apply(m_fftSFD->in());
            std::fill(m_fftSFD->in()+m_fftLength, m_fftSFD->in()+m_fftInterpolation*m_fftLength, Complex{0.0, 0.0});
            m_fftSFD->transform();

            m_fftCounter = 0;
            double magsq, magsqSFD;

            unsigned int imaxSFD = argmax(
                m_fftSFD->out(),
                m_fftInterpolation,
                m_fftLength,
                magsqSFD,
                nullptr,
                m_fftInterpolation
            ) / m_fftInterpolation;

            unsigned int imax = argmax(
                m_fft->out(),
                m_fftInterpolation,
                m_fftLength,
                magsq,
                m_spectrumBuffer,
                m_fftInterpolation
            ) / m_fftInterpolation;

            m_preambleHistory[m_chirpCount] = imax;
            m_chirpCount++;

            if (magsq <  magsqSFD) // preamble drop
            {
                if (m_chirpCount < 3) // too early
                {
                    m_state = LoRaStateReset;
                }
                else
                {
                    m_syncWord = round(m_preambleHistory[m_chirpCount-2] / 8.0);
                    m_syncWord += 16 * round(m_preambleHistory[m_chirpCount-3] / 8.0);
                    qDebug("LoRaDemodSink::processSample: SFD found:  up: %u|%f down: %u|%f sync: %x", imax, magsq, imaxSFD, magsqSFD, m_syncWord);
                    m_sfdSkipCounter = 0;
                    m_fftCounter = m_fftLength - m_sfdSkip;
                    std::copy(m_fftBuffer+m_sfdSkip, m_fftBuffer+(m_fftLength-m_sfdSkip), m_fftBuffer); // prepare sliding fft
                    m_state = LoRaStateSlideSFD;
                    m_magsq = magsqSFD;
                }
            }
            else if (m_chirpCount > m_maxSFDSearchChirps) // SFD missed start over
            {
                m_state = LoRaStateReset;
            }
            else
            {
                if (m_spectrumSink) {
                    m_spectrumSink->feed(m_spectrumBuffer, m_nbSymbols);
                }

                qDebug("LoRaDemodSink::processSample: SFD search: up: %u|%f down: %u|%f", imax, magsq, imaxSFD, magsqSFD);
                m_magsq = magsq;
            }
        }
    }
    else if (m_state == LoRaStateSkipSFD) // Just skip SFD
    {
        m_fftCounter++;

        if (m_fftCounter == m_fftLength)
        {
            m_fftCounter = m_fftLength - m_sfdSkip;
            m_sfdSkipCounter++;

            if (m_sfdSkipCounter == m_sfdFourths) // 1.25 SFD chips left
            {
                m_chirp = m_chirp0;
                m_fftCounter = 0;
                m_chirpCount = 0;
                int correction = 0;
                qDebug("LoRaDemodSink::processSample: SFD skipped");
                m_state = LoRaStateReadPayload; //LoRaStateReadPayload;
            }
        }
    }
    else if (m_state == LoRaStateSlideSFD) // perform sliding FFTs over the rest of the SFD period
    {
        m_fftBuffer[m_fftCounter] = ci * m_upChirps[m_chirp]; // de-chirp the down ramp
        m_fftCounter++;

        if (m_fftCounter == m_fftLength)
        {
            std::copy(m_fftBuffer, m_fftBuffer + m_fftLength, m_fftSFD->in());
            std::fill(m_fftSFD->in()+m_fftLength, m_fftSFD->in()+m_fftInterpolation*m_fftLength, Complex{0.0, 0.0});
            m_fftSFD->transform();
            std::copy(m_fftBuffer+m_sfdSkip, m_fftBuffer+(m_fftLength-m_sfdSkip), m_fftBuffer); // prepare next sliding fft
            m_fftCounter = m_fftLength - m_sfdSkip;
            m_sfdSkipCounter++;

            double magsqSFD;

            unsigned int imaxSFD = argmax(
                m_fftSFD->out(),
                m_fftInterpolation,
                m_fftLength,
                magsqSFD,
                m_spectrumBuffer,
                m_fftInterpolation
            ) / m_fftInterpolation;

            qDebug("LoRaDemodSink::processSample: SFD slide %u %u|%f", m_sfdSkipCounter, imaxSFD, magsqSFD);

            if (m_sfdSkipCounter == m_sfdFourths) // 1.25 SFD chips length
            {
                m_chirp = m_chirp0;
                m_fftCounter = 0;
                m_chirpCount = 0;
                int correction = 0;
                qDebug("LoRaDemodSink::processSample: SFD done");
                m_state = LoRaStateReadPayload; //LoRaStateReadPayload;
            }
        }
    }
    else if (m_state == LoRaStateReadPayload)
    {
        m_fft->in()[m_fftCounter] = ci * m_downChirps[m_chirp]; // de-chirp the up ramp
        m_fftCounter++;

        if (m_fftCounter == m_fftLength)
        {
            m_fftWindow.apply(m_fft->in());
            std::fill(m_fft->in()+m_fftLength, m_fft->in()+m_fftInterpolation*m_fftLength, Complex{0.0, 0.0});
            m_fft->transform();
            m_fftCounter = 0;
            double magsq;

            unsigned int symbol = round(
                argmax(
                    m_fft->out(),
                    m_fftInterpolation,
                    m_fftLength,
                    magsq,
                    m_spectrumBuffer,
                    m_fftInterpolation
                ) / ((float) m_fftInterpolation * (1<<m_settings.m_deBits))
            );

            if (m_spectrumSink) {
                m_spectrumSink->feed(m_spectrumBuffer, m_nbSymbols);
            }

            if ((m_chirpCount == 0) || (10.0*magsq > m_magsq))
            {
                qDebug("LoRaDemodSink::processSample: symbol %02u: %4u|%11.6f", m_chirpCount, symbol, magsq);
                m_magsq = magsq;
                m_chirpCount++;

                if (m_chirpCount > 255)
                {
                    qDebug("LoRaDemodSink::processSample: message length exceeded");
                    m_state = LoRaStateReset;
                }
            }
            else
            {
                qDebug("LoRaDemodSink::processSample: end of message");
                m_state = LoRaStateReset;
            }
        }
    }
    else
    {
        m_state = LoRaStateReset;
    }

    m_chirp++;

    if (m_chirp >= m_chirp0 + m_nbSymbols) {
        m_chirp = m_chirp0;
    }
}

void LoRaDemodSink::reset()
{
    m_chirp = 0;
    m_chirp0 = 0;
    m_fftCounter = 0;
    m_argMaxHistoryCounter = 0;
    m_sfdSkipCounter = 0;
}

unsigned int LoRaDemodSink::argmax(
    const Complex *fftBins,
    unsigned int fftMult,
    unsigned int fftLength,
    double& magsqMax,
    Complex *specBuffer,
    unsigned int specDecim)
{
    magsqMax = 0.0;
    unsigned int imax;
    double magSum = 0.0;

    for (unsigned int i = 0; i < fftMult*fftLength; i++)
    {
        double magsq = std::norm(fftBins[i]);

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

    return imax;
}

void LoRaDemodSink::decimateSpectrum(Complex *in, Complex *out, unsigned int size, unsigned int decimation)
{
    for (unsigned int i = 0; i < size; i++)
    {
        if (i % decimation == 0) {
            out[i/decimation] = in[i];
        }
    }
}

int LoRaDemodSink::toSigned(int u, int intSize)
{
    if (u > intSize/2) {
        return u - intSize;
    } else {
        return u;
    }
}

void LoRaDemodSink::applyChannelSettings(int channelSampleRate, int bandwidth, int channelFrequencyOffset, bool force)
{
    qDebug() << "LoRaDemodSink::applyChannelSettings:"
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
        m_interpolator.create(16, channelSampleRate, bandwidth / 1.9f);
        m_interpolatorDistance = (Real) channelSampleRate / (Real) bandwidth;
        m_sampleDistanceRemain = 0;
        qDebug() << "LoRaDemodSink::applyChannelSettings: m_interpolator.create:"
            << " m_interpolatorDistance: " << m_interpolatorDistance;
    }

    m_channelSampleRate = channelSampleRate;
    m_bandwidth = bandwidth;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void LoRaDemodSink::applySettings(const LoRaDemodSettings& settings, bool force)
{
    qDebug() << "LoRaDemodSink::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_bandwidthIndex: " << settings.m_bandwidthIndex
            << " m_spreadFactor: " << settings.m_spreadFactor
            << " m_rgbColor: " << settings.m_rgbColor
            << " m_title: " << settings.m_title
            << " force: " << force;

    if ((settings.m_spreadFactor != m_settings.m_spreadFactor) || force) {
        initSF(settings.m_spreadFactor);
    }

    m_settings = settings;
}
