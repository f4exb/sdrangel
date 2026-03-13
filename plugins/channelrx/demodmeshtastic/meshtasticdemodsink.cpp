///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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
#include <algorithm>
#include <cmath>
#include <vector>

#include "dsp/dsptypes.h"
#include "dsp/basebandsamplesink.h"
#include "dsp/dspengine.h"
#include "dsp/fftfactory.h"
#include "dsp/fftengine.h"
#include "util/db.h"

#include "meshtasticdemodmsg.h"
#include "meshtasticdemoddecoderlora.h"
#include "meshtasticdemodsink.h"

MeshtasticDemodSink::MeshtasticDemodSink() :
    m_decodeMsg(nullptr),
    m_decoderMsgQueue(nullptr),
    m_fftSequence(-1),
    m_fftSFDSequence(-1),
    m_downChirps(nullptr),
    m_upChirps(nullptr),
    m_spectrumLine(nullptr),
    m_headerLocked(false),
    m_expectedSymbols(0),
    m_waitHeaderFeedback(false),
    m_headerFeedbackWaitSteps(0U),
    m_loRaFrameId(0U),
    m_osFactor(MeshtasticDemodSettings::oversampling > 0 ? MeshtasticDemodSettings::oversampling : 1),
    m_osCenterPhase((MeshtasticDemodSettings::oversampling > 1 ? MeshtasticDemodSettings::oversampling / 2 : 0)),
    m_osCounter(0),
    m_loRaState(LoRaStateDetect),
    m_loRaSyncState(LoRaSyncNetId1),
    m_loRaSymbolCnt(1),
    m_loRaBinIdx(0),
    m_loRaKHat(0),
    m_loRaDownVal(0),
    m_loRaCFOInt(0),
    m_loRaNetIdOff(0),
    m_loRaAdditionalUpchirps(0),
    m_loRaUpSymbToUse(0),
    m_loRaRequiredUpchirps(0),
    m_loRaSymbolSpan(0),
    m_loRaFrameSymbolCount(0),
    m_loRaCFOFrac(0.0f),
    m_loRaSTOFrac(0.0f),
    m_loRaSFOHat(0.0f),
    m_loRaSFOCum(0.0f),
    m_loRaCFOSTOEstimated(false),
    m_loRaReceivedHeader(false),
    m_loRaOneSymbolOff(false),
    m_spectrumSink(nullptr),
    m_spectrumBuffer(nullptr)
{
    m_demodActive = false;
	m_bandwidth = MeshtasticDemodSettings::bandwidths[0];
	m_channelSampleRate = 96000;
	m_channelFrequencyOffset = 0;
    m_deviceCenterFrequency = 0;
	m_nco.setFreq(m_channelFrequencyOffset, m_channelSampleRate);
	m_interpolator.create(16, m_channelSampleRate, m_bandwidth / 1.9f);
    m_interpolatorDistance = (Real) m_channelSampleRate / (Real) m_bandwidth;
    m_sampleDistanceRemain = 0;
    const unsigned int ctorConfiguredPreamble = m_settings.m_preambleChirps > 0U
        ? m_settings.m_preambleChirps
        : m_minRequiredPreambleChirps;
    const unsigned int ctorTargetRequired = ctorConfiguredPreamble > 3U
        ? (ctorConfiguredPreamble - 3U)
        : m_minRequiredPreambleChirps;
    m_requiredPreambleChirps = std::max(
        m_minRequiredPreambleChirps,
        std::min(ctorTargetRequired, m_maxRequiredPreambleChirps)
    );
    m_fftInterpolation = (m_settings.m_codingScheme == MeshtasticDemodSettings::CodingLoRa)
        ? m_loRaFFTInterpolation
        : m_legacyFFTInterpolation;

    m_state = ChirpChatStateReset;
	m_chirp = 0;
	m_chirp0 = 0;

    initSF(m_settings.m_spreadFactor, m_settings.m_deBits, m_settings.m_fftWindow);
}

MeshtasticDemodSink::~MeshtasticDemodSink()
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

void MeshtasticDemodSink::initSF(unsigned int sf, unsigned int deBits, FFTWindow::Function fftWindow)
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
    m_preambleTolerance = std::max(1, (m_deLength*static_cast<int>(m_fftInterpolation))/2);
    m_fftSequence = fftFactory->getEngine(m_interpolatedFFTLength, false, &m_fft);
    m_fftSFDSequence = fftFactory->getEngine(m_interpolatedFFTLength, false, &m_fftSFD);
    m_state = ChirpChatStateReset;
    m_sfdSkip = m_fftLength / 4;
    m_downChirps = new Complex[2*m_nbSymbols]; // Each table is 2 chirps long to allow processing from arbitrary offsets.
    m_upChirps = new Complex[2*m_nbSymbols];
    m_spectrumBuffer = new Complex[m_nbSymbols];
    m_spectrumLine = new Complex[m_nbSymbols];
    std::fill(m_spectrumLine, m_spectrumLine+m_nbSymbols, Complex(std::polar(1e-6*SDR_RX_SCALED, 0.0)));
    m_loRaSymbolSpan = m_nbSymbols * m_osFactor;
    m_loRaRequiredUpchirps = m_requiredPreambleChirps;
    m_loRaUpSymbToUse = (m_loRaRequiredUpchirps > 0U) ? static_cast<int>(m_loRaRequiredUpchirps - 1U) : 0;
    m_loRaInDown.assign(m_nbSymbols, Complex{0.0f, 0.0f});
    m_loRaPreambleRaw.assign(m_nbSymbols * m_loRaRequiredUpchirps, Complex{0.0f, 0.0f});
    m_loRaPreambleRawUp.assign((m_settings.m_preambleChirps + 3U) * m_loRaSymbolSpan, Complex{0.0f, 0.0f});
    m_loRaPreambleUpchirps.assign(m_nbSymbols * m_loRaRequiredUpchirps, Complex{0.0f, 0.0f});
    m_loRaCFOFracCorrec.assign(m_nbSymbols, Complex{1.0f, 0.0f});
    m_loRaPayloadDownchirp.assign(m_nbSymbols, Complex{1.0f, 0.0f});
    m_loRaSymbCorr.assign(m_nbSymbols, Complex{0.0f, 0.0f});
    m_loRaNetIdSamp.assign((m_loRaSymbolSpan * 5U) / 2U + m_loRaSymbolSpan, Complex{0.0f, 0.0f});
    m_loRaAdditionalSymbolSamp.assign(m_loRaSymbolSpan * 2U, Complex{0.0f, 0.0f});
    m_loRaPreambleVals.assign(m_loRaRequiredUpchirps, 0);
    m_loRaNetIds.assign(2, 0);
    m_loRaSampleFifo.clear();
    m_loRaState = LoRaStateDetect;
    m_loRaSyncState = LoRaSyncNetId1;
    m_loRaSymbolCnt = 1;
    m_loRaBinIdx = 0;
    m_loRaKHat = 0;
    m_loRaAdditionalUpchirps = 0;
    m_loRaCFOSTOEstimated = false;
    m_loRaReceivedHeader = false;
    m_loRaFrameSymbolCount = 0;

    // Canonical gr-lora_sdr reference chirps (utilities::build_ref_chirps, id=0, os_factor=1).
    for (unsigned int i = 0; i < m_fftLength; i++)
    {
        const double n = static_cast<double>(i);
        const double N = static_cast<double>(m_nbSymbols);
        const double phase = 2.0 * M_PI * ((n * n) / (2.0 * N) - 0.5 * n);
        m_upChirps[i] = Complex(std::cos(phase), std::sin(phase));
        m_downChirps[i] = std::conj(m_upChirps[i]);
    }

    // Duplicate table to allow processing from arbitrary offsets
    std::copy(m_downChirps, m_downChirps+m_fftLength, m_downChirps+m_fftLength);
    std::copy(m_upChirps, m_upChirps+m_fftLength, m_upChirps+m_fftLength);
}

void MeshtasticDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
	Complex ci;

	for (SampleVector::const_iterator it = begin; it < end; ++it)
	{
		Complex c(it->real() / SDR_RX_SCALEF, it->imag() / SDR_RX_SCALEF);
		c *= m_nco.nextIQ();

		if (m_interpolator.decimate(&m_sampleDistanceRemain, c, &ci))
		{
            if (m_settings.m_codingScheme == MeshtasticDemodSettings::CodingLoRa)
            {
                processSample(ci);
            }
            else if (m_osFactor <= 1U)
            {
                processSample(ci);
            }
            else
            {
                if ((m_osCounter % m_osFactor) == m_osCenterPhase) {
                    processSample(ci);
                }
                m_osCounter++;
            }
			m_sampleDistanceRemain += m_interpolatorDistance;
		}
	}
}

void MeshtasticDemodSink::processSample(const Complex& ci)
{
    if (m_settings.m_codingScheme == MeshtasticDemodSettings::CodingLoRa)
    {
        processSampleLoRa(ci);
        return;
    }

    if (m_state == ChirpChatStateReset) // start over
    {
        m_demodActive = false;
        reset();
        std::queue<double>().swap(m_magsqQueue); // this clears the queue
        m_state = ChirpChatStateDetectPreamble;
    }
    else if (m_state == ChirpChatStateDetectPreamble) // look for preamble
    {
        m_fft->in()[m_fftCounter++] = ci * (m_settings.m_invertRamps ? m_upChirps[m_chirp] : m_downChirps[m_chirp]); // de-chirp the preamble ramp

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

            // When ramps are inverted, FFT output interpretation is reversed
            if (m_settings.m_invertRamps) {
                imax = (m_nbSymbols - imax) % m_nbSymbols;
            }

            if (m_magsqQueue.size() > m_settings.m_preambleChirps) {
                m_magsqQueue.pop();
            }

            m_magsqTotalAvg(magsqTotal);
            m_magsqQueue.push(magsq);

            if (m_havePrevPreambleBin)
            {
                const int delta = circularBinDelta(imax, m_prevPreambleBin);

                if (std::abs(delta) <= m_preambleTolerance)
                {
                    m_preambleConsecutive++;
                    m_preambleBinHistory.push_back(imax);
                }
                else
                {
                    m_preambleConsecutive = 1;
                    m_preambleBinHistory.clear();
                    m_preambleBinHistory.push_back(imax);
                }
            }
            else
            {
                m_havePrevPreambleBin = true;
                m_preambleConsecutive = 1;
                m_preambleBinHistory.clear();
                m_preambleBinHistory.push_back(imax);
            }

            if (m_preambleBinHistory.size() > m_requiredPreambleChirps) {
                m_preambleBinHistory.pop_front();
            }

            m_prevPreambleBin = imax;

            // gr-lora_sdr-style rolling detect: lock after enough consecutive near-equal upchirps.
            if ((m_preambleConsecutive >= m_requiredPreambleChirps) && (magsq > 1e-9))
            {
                const unsigned int preambleBin = getPreambleModeBin();

                if (m_spectrumSink) {
                    m_spectrumSink->feed(m_spectrumBuffer, m_nbSymbols);
                }

                qInfo("MeshtasticDemodSink::processSample: preamble found: %u|%f (consecutive=%u)",
                    preambleBin, magsq, m_preambleConsecutive);
                m_chirp = preambleBin;
                m_fftCounter = m_chirp;
                m_chirp0 = 0;
                m_chirpCount = 0;
                m_state = ChirpChatStatePreambleResyc;
            }
            else if (!m_magsqQueue.empty())
            {
                m_magsqOffAvg(m_magsqQueue.front());
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
        m_fft->in()[m_fftCounter] = ci * (m_settings.m_invertRamps ? m_upChirps[m_chirp] : m_downChirps[m_chirp]);  // de-chirp the preamble ramp
        m_fftSFD->in()[m_fftCounter] = ci * (m_settings.m_invertRamps ? m_downChirps[m_chirp] : m_upChirps[m_chirp]); // de-chirp the SFD ramp
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

            // When ramps are inverted, FFT output interpretation is reversed
            if (m_settings.m_invertRamps) {
                imax = (m_nbSymbols - imax) % m_nbSymbols;
            }

            if (m_chirpCount < m_maxSFDSearchChirps)
            {
                m_preambleHistory[m_chirpCount] = imax;
                m_chirpCount++;
            }
            else
            {
                // Protect against history overflow when long preambles are configured.
                qWarning("MeshtasticDemodSink::processSample: SFD search history overflow (%u >= %u)",
                    m_chirpCount, m_maxSFDSearchChirps);
                m_state = ChirpChatStateReset;
                return;
            }
            const double preDrop = magsqPre - magsqSFD;
            const double dropRatio = (magsqSFD > 1e-18) ? (-preDrop / magsqSFD) : 0.0;
            const bool sfdDominant = magsqSFD > (magsqPre * 1.05); // less strict than legacy 50% jump
            const bool sfdBinAligned = (imaxSFD <= 2U) || (imaxSFD >= (m_nbSymbols - 2U));

            if (sfdDominant && sfdBinAligned) // preamble -> SFD transition candidate
            {
                m_magsqTotalAvg(magsqSFDTotal);

                if (m_chirpCount < 1 + (m_settings.hasSyncWord() ? 2 : 0)) // too early
                {
                    m_state = ChirpChatStateReset;
                    qDebug("MeshtasticDemodSink::processSample: SFD search: signal drop is too early");
                }
                else
                {
                    if (m_settings.hasSyncWord())
                    {
                        m_syncWord = round(m_preambleHistory[m_chirpCount-2] / 8.0);
                        m_syncWord += 16 * round(m_preambleHistory[m_chirpCount-3] / 8.0);
                        qInfo("MeshtasticDemodSink::processSample: SFD found: pre=%4u|%11.6f sfd=%4u|%11.6f ratio=%8.4f sync=%x",
                              imax, magsqPre, imaxSFD, magsqSFD, dropRatio, m_syncWord);
                    }
                    else
                    {
                        m_syncWord = 0;
                        qInfo("MeshtasticDemodSink::processSample: SFD found: pre=%4u|%11.6f sfd=%4u|%11.6f ratio=%8.4f",
                              imax, magsqPre, imaxSFD, magsqSFD, dropRatio);
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
                    qDebug("MeshtasticDemodSink::processSample: zero adjust: %d (%d)", zadj, nadj);

                    m_sfdSkipCounter = 0;
                    m_fftCounter = m_fftLength - m_sfdSkip + zadj;
                    m_chirp += zadj;
                    m_state = ChirpChatStateSkipSFD; //ChirpChatStateSlideSFD;
                }
            }
            else // SFD missed start over
            {
                const unsigned int preambleForWindow = std::max(m_settings.m_preambleChirps, m_requiredPreambleChirps);
                unsigned int sfdSearchWindow = preambleForWindow - m_requiredPreambleChirps + 2U;
                sfdSearchWindow = std::max(
                    m_requiredPreambleChirps,
                    std::min(sfdSearchWindow, m_maxSFDSearchChirps)
                );

                if (m_chirpCount <= sfdSearchWindow) {
                    if (m_spectrumSink) {
                        m_spectrumSink->feed(m_spectrumBuffer, m_nbSymbols);
                    }

                    qDebug("MeshtasticDemodSink::processSample: SFD search: pre=%4u|%11.6f sfd=%4u|%11.6f ratio=%8.4f",
                           imax, magsqPre, imaxSFD, magsqSFD, dropRatio);
                    m_magsqTotalAvg(magsqTotal);
                    m_magsqOnAvg(magsqPre);
                    return;
                }

                qDebug("MeshtasticDemodSink::processSample: SFD search: number of possible chirps exceeded");
                m_magsqTotalAvg(magsqTotal);
                m_state = ChirpChatStateReset;
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
                qInfo("MeshtasticDemodSink::processSample: SFD skipped");
                m_chirp = m_chirp0;
                m_fftCounter = 0;
                m_chirpCount = 0;
                m_magsqMax = 0.0;
                m_decodeMsg = MeshtasticDemodMsg::MsgDecodeSymbols::create();
                m_decodeMsg->setFrameId(0U);
                m_decodeMsg->setSyncWord(m_syncWord);
                clearSpectrumHistoryForNewFrame();
                m_state = ChirpChatStateReadPayload;
            }
        }
    }
    else if (m_state == ChirpChatStateReadPayload)
    {
        m_fft->in()[m_fftCounter] = ci * (m_settings.m_invertRamps ? m_upChirps[m_chirp] : m_downChirps[m_chirp]);
        m_fftCounter++;

        if (m_fftCounter == m_fftLength)
        {
            m_fftWindow.apply(m_fft->in());
            std::fill(m_fft->in()+m_fftLength, m_fft->in()+m_interpolatedFFTLength, Complex{0.0, 0.0});
            m_fft->transform();
            m_fftCounter = 0;
            double magsq, magsqTotal;
            unsigned short symbol;

            if (m_settings.m_codingScheme == MeshtasticDemodSettings::CodingFT)
            {
                std::vector<float> magnitudes;
                symbol = evalSymbol(
                    extractMagnitudes(
                        magnitudes,
                        m_fft->out(),
                        m_fftInterpolation,
                        m_fftLength,
                        magsq,
                        magsqTotal,
                        m_spectrumBuffer,
                        m_fftInterpolation
                    )
                ) % m_nbSymbolsEff;
                m_decodeMsg->pushBackSymbol(symbol);
                m_decodeMsg->pushBackMagnitudes(magnitudes);
            }
            else
            {
                int imax;

                if (m_settings.m_deBits > 0)
                {
                    double magSqNoise;
                    imax = argmaxSpreaded(
                        m_fft->out(),
                        m_fftInterpolation,
                        m_fftLength,
                        magsq,
                        magSqNoise,
                        magsqTotal,
                        m_spectrumBuffer,
                        m_fftInterpolation
                    );
                }
                else
                {
                    imax = argmax(
                        m_fft->out(),
                        m_fftInterpolation,
                        m_fftLength,
                        magsq,
                        magsqTotal,
                        m_spectrumBuffer,
                        m_fftInterpolation
                    );
                }

                if (m_settings.m_invertRamps) {
                    imax = (m_nbSymbols * m_fftInterpolation - imax) % (m_nbSymbols * m_fftInterpolation);
                }

                const bool headerSymbol = (m_settings.m_codingScheme == MeshtasticDemodSettings::CodingLoRa)
                    && m_settings.m_hasHeader
                    && (m_chirpCount < 8U);
                symbol = evalSymbol(imax, headerSymbol) % m_nbSymbolsEff;
                m_decodeMsg->pushBackSymbol(symbol);
            }

            if (m_spectrumSink) {
                m_spectrumSink->feed(m_spectrumBuffer, m_nbSymbols);
            }

            if (magsq > m_magsqMax) {
                m_magsqMax = magsq;
            }

            m_magsqTotalAvg(magsq);

            const bool inHeaderBlock = (m_settings.m_codingScheme == MeshtasticDemodSettings::CodingLoRa)
                && m_settings.m_hasHeader
                && (m_chirpCount < 8U);

            if (m_headerLocked)
            {
                // Header-locked: accept every symbol, terminate at exact expected count
                qDebug("MeshtasticDemodSink::processSample: symbol %02u: %4u|%11.6f (%u/%u locked)",
                       m_chirpCount, symbol, magsq, m_chirpCount + 1, m_expectedSymbols);
                m_magsqOnAvg(magsq);
                m_chirpCount++;

                if (m_chirpCount >= m_expectedSymbols)
                {
                    qInfo("MeshtasticDemodSink::processSample: header-locked frame complete (%u symbols)", m_chirpCount);
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
            else if (inHeaderBlock
                || (m_chirpCount == 0)
                || (m_settings.m_eomSquelchTenths == 121)
                || ((m_settings.m_eomSquelchTenths*magsq)/10.0 > m_magsqMax))
            {
                qDebug("MeshtasticDemodSink::processSample: symbol %02u: %4u|%11.6f", m_chirpCount, symbol, magsq);
                m_magsqOnAvg(magsq);
                m_chirpCount++;

                // Attempt header lock after the 8-symbol header block
                if (m_chirpCount == 8
                    && m_settings.m_codingScheme == MeshtasticDemodSettings::CodingLoRa
                    && m_settings.m_hasHeader)
                {
                    tryHeaderLock();
                }

                if (m_chirpCount > m_settings.m_nbSymbolsMax)
                {
                    qInfo("MeshtasticDemodSink::processSample: message length reached");
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
                qInfo("MeshtasticDemodSink::processSample: end of message (EOM squelch)");
                m_state = ChirpChatStateReset;
                m_decodeMsg->popSymbol();
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

void MeshtasticDemodSink::reset()
{
    resetLoRaFrameSync();
    m_chirp = 0;
    m_chirp0 = 0;
    m_fftCounter = 0;
    m_preambleConsecutive = 0;
    m_havePrevPreambleBin = false;
    m_prevPreambleBin = 0;
    m_preambleBinHistory.clear();
    m_sfdSkipCounter = 0;
    m_syncWord = 0;
    m_headerLocked = false;
    m_expectedSymbols = 0;
    m_waitHeaderFeedback = false;
    m_headerFeedbackWaitSteps = 0;
    m_osCounter = 0;
}

unsigned int MeshtasticDemodSink::argmax(
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
    std::vector<double> spectrumBucketPowers;

    if (specBuffer) {
        spectrumBucketPowers.reserve((fftMult * fftLength) / std::max(1U, specDecim));
    }

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
                spectrumBucketPowers.push_back(magSum);
                magSum = 0.0;
            }
        }
    }

    const double magsqAvgRaw = magsqTotal / static_cast<double>(fftMult * fftLength);
    magsqTotal = magsqAvgRaw;

    if (specBuffer && !spectrumBucketPowers.empty())
    {
        const double noisePerBucket = magsqAvgRaw * static_cast<double>(std::max(1U, specDecim));
        const double floorCut = noisePerBucket * 1.05; // suppress steady floor
        const double boost = 12.0;                     // emphasize peaks over residual floor

        for (size_t i = 0; i < spectrumBucketPowers.size(); ++i)
        {
            const double enhancedPower = std::max(0.0, spectrumBucketPowers[i] - floorCut) * boost;
            const double specAmp = std::sqrt(enhancedPower) * static_cast<double>(m_nbSymbols);
            specBuffer[i] = Complex(std::polar(specAmp, 0.0));
        }
    }

    return imax;
}

unsigned int MeshtasticDemodSink::extractMagnitudes(
    std::vector<float>& magnitudes,
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
    std::vector<double> spectrumBucketPowers;

    if (specBuffer) {
        spectrumBucketPowers.reserve((fftMult * fftLength) / std::max(1U, specDecim));
    }

    unsigned int spread = fftMult * (1<<m_settings.m_deBits);
    unsigned int istart = fftMult*fftLength - spread/2 + 1;
    float magnitude = 0.0;

    for (unsigned int i2 = istart; i2 < istart + fftMult*fftLength; i2++)
    {
        int i = i2 % (fftMult*fftLength);
        double magsq = std::norm(fftBins[i]);
        magsqTotal += magsq;
        magnitude += magsq;

        if (i % spread == (spread/2)-1) // boundary (inclusive)
        {
            if (magnitude > magsqMax)
            {
                imax = (i/spread)*spread;
                magsqMax = magnitude;
            }

            magnitudes.push_back(magnitude);
            magnitude = 0.0;
        }

        if (specBuffer)
        {
            magSum += magsq;

            if (i % specDecim == specDecim - 1)
            {
                spectrumBucketPowers.push_back(magSum);
                magSum = 0.0;
            }
        }
    }

    const double magsqAvgRaw = magsqTotal / static_cast<double>(fftMult * fftLength);
    magsqTotal = magsqAvgRaw;

    if (specBuffer && !spectrumBucketPowers.empty())
    {
        const double noisePerBucket = magsqAvgRaw * static_cast<double>(std::max(1U, specDecim));
        const double floorCut = noisePerBucket * 1.05;
        const double boost = 12.0;

        for (size_t i = 0; i < spectrumBucketPowers.size(); ++i)
        {
            const double enhancedPower = std::max(0.0, spectrumBucketPowers[i] - floorCut) * boost;
            const double specAmp = std::sqrt(enhancedPower) * static_cast<double>(m_nbSymbols);
            specBuffer[i] = Complex(std::polar(specAmp, 0.0));
        }
    }

    return imax;
}

unsigned int MeshtasticDemodSink::argmaxSpreaded(
    const Complex *fftBins,
    unsigned int fftMult,
    unsigned int fftLength,
    double& magsqMax,
    double& magsqNoise,
    double& magsqTotal,
    Complex *specBuffer,
    unsigned int specDecim)
{
    magsqMax = 0.0;
    magsqNoise = 0.0;
    magsqTotal = 0.0;
    unsigned int imax = 0;
    double magSum = 0.0;
    std::vector<double> spectrumBucketPowers;

    if (specBuffer) {
        spectrumBucketPowers.reserve((fftMult * fftLength) / std::max(1U, specDecim));
    }

    unsigned int nbsymbols = 1<<(m_settings.m_spreadFactor - m_settings.m_deBits);
    unsigned int spread = fftMult * (1<<m_settings.m_deBits);
    unsigned int istart = fftMult*fftLength - spread/2 + 1;
    double magSymbol = 0.0;

    for (unsigned int i2 = istart; i2 < istart + fftMult*fftLength; i2++)
    {
        int i = i2 % (fftMult*fftLength);
        double magsq = std::norm(fftBins[i]);
        magsqTotal += magsq;
        magSymbol += magsq;

        if (i % spread == (spread/2)-1) // boundary (inclusive)
        {
            if (magSymbol > magsqMax)
            {
                imax = (i/spread)*spread;
                magsqMax = magSymbol;
            }

            magsqNoise += magSymbol;
            magSymbol = 0.0;
        }

        if (specBuffer)
        {
            magSum += magsq;

            if (i % specDecim == specDecim - 1)
            {
                spectrumBucketPowers.push_back(magSum);
                magSum = 0.0;
            }
        }
    }

    const double magsqAvgRaw = magsqTotal / static_cast<double>(fftMult * fftLength);

    if (specBuffer && !spectrumBucketPowers.empty())
    {
        const double noisePerBucket = magsqAvgRaw * static_cast<double>(std::max(1U, specDecim));
        const double floorCut = noisePerBucket * 1.05;
        const double boost = 12.0;

        for (size_t i = 0; i < spectrumBucketPowers.size(); ++i)
        {
            const double enhancedPower = std::max(0.0, spectrumBucketPowers[i] - floorCut) * boost;
            const double specAmp = std::sqrt(enhancedPower) * static_cast<double>(m_nbSymbols);
            specBuffer[i] = Complex(std::polar(specAmp, 0.0));
        }
    }

    magsqNoise -= magsqMax;
    magsqNoise /= (nbsymbols - 1);
    magsqTotal /= nbsymbols;
        // magsqNoise /= fftLength;
        // magsqTotal /= fftMult*fftLength;

    return imax;
}

void MeshtasticDemodSink::decimateSpectrum(Complex *in, Complex *out, unsigned int size, unsigned int decimation)
{
    for (unsigned int i = 0; i < size; i++)
    {
        if (i % decimation == 0) {
            out[i/decimation] = in[i];
        }
    }
}

int MeshtasticDemodSink::toSigned(int u, int intSize)
{
    if (u > intSize/2) {
        return u - intSize;
    } else {
        return u;
    }
}

int MeshtasticDemodSink::circularBinDelta(unsigned int current, unsigned int previous) const
{
    const int bins = static_cast<int>(m_nbSymbols);
    if (bins <= 0) {
        return 0;
    }

    int delta = static_cast<int>(current) - static_cast<int>(previous);
    const int half = bins / 2;

    if (delta > half) {
        delta -= bins;
    } else if (delta < -half) {
        delta += bins;
    }

    return delta;
}

unsigned int MeshtasticDemodSink::getPreambleModeBin() const
{
    if (m_preambleBinHistory.empty()) {
        return 0U;
    }

    std::vector<unsigned int> counts(m_nbSymbols, 0U);
    unsigned int bestBin = m_preambleBinHistory.back() % m_nbSymbols;
    unsigned int bestCount = 0U;

    for (unsigned int bin : m_preambleBinHistory)
    {
        const unsigned int b = bin % m_nbSymbols;
        const unsigned int c = ++counts[b];

        if (c > bestCount)
        {
            bestCount = c;
            bestBin = b;
        }
    }

    return bestBin;
}

unsigned int MeshtasticDemodSink::evalSymbol(unsigned int rawSymbol, bool headerSymbol)
{
    unsigned int spread = m_fftInterpolation * (1U << m_settings.m_deBits);
    const unsigned int symbolBins = m_fftInterpolation * m_nbSymbols;

    if (symbolBins == 0U) {
        return rawSymbol;
    }

    // In gr-lora_sdr, explicit-header symbols are always reduced by 2 extra bits
    // (sf_app = sf-2), independently of payload LDRO selection.
    if (headerSymbol)
    {
        const int de = m_settings.m_deBits;
        if (de < 2) {
            spread <<= (2 - de);
        }
    }

    // Match gr-lora_sdr hard-decoding symbol mapping:
    //   s = mod(raw_bin - 1, 2^SF * os_factor) / (os_factor * 2^DE)
    const unsigned int shifted = (rawSymbol + symbolBins - 1U) % symbolBins;

    if (spread == 0U) {
        return shifted;
    }

    return shifted / spread;
}

void MeshtasticDemodSink::tryHeaderLock()
{
    const std::vector<unsigned short>& symbols = m_decodeMsg->getSymbols();

    if (symbols.size() < 8) {
        return;
    }

    const unsigned int sf = m_settings.m_spreadFactor;

    if (sf < 7) {
        return;
    }

    const unsigned int headerNbSymbolBits = sf - 2U;

    if (headerNbSymbolBits < 5) {
        return;
    }

    bool hasCRC = true;
    unsigned int nbParityBits = 1U;
    unsigned int packetLength = 0U;
    int headerParityStatus = (int) MeshtasticDemodSettings::ParityUndefined;
    bool headerCRCStatus = false;

    MeshtasticDemodDecoderLoRa::decodeHeader(
        symbols,
        headerNbSymbolBits,
        hasCRC,
        nbParityBits,
        packetLength,
        headerParityStatus,
        headerCRCStatus
    );

    if (!headerCRCStatus || packetLength == 0U || nbParityBits < 1U || nbParityBits > 4U)
    {
        qDebug("MeshtasticDemodSink::tryHeaderLock: header invalid (CRC=%d len=%u CR=%u parity=%d)",
               headerCRCStatus ? 1 : 0,
               packetLength,
               nbParityBits,
               headerParityStatus);
        return;
    }

    const double symbolDurationMs = (double)(1U << sf) * 1000.0 / (double)m_bandwidth;
    const bool ldro = symbolDurationMs > 16.0;
    const unsigned int sfDenom = sf - (ldro ? 2U : 0U);

    // gr-lora_sdr formula: symb_numb = 8 + ceil(max(0, 2*pay_len - sf + 2 + 5 + has_crc*4) / (sf - 2*ldro)) * (4 + cr)
    const int numerator = 2 * (int)packetLength - (int)sf + 2 + 5 + (hasCRC ? 4 : 0);
    unsigned int payloadBlocks = 0;

    if (numerator > 0 && sfDenom > 0) {
        payloadBlocks = ((unsigned int)numerator + sfDenom - 1U) / sfDenom;
    }

    m_expectedSymbols = 8U + payloadBlocks * (4U + nbParityBits);

    if (m_expectedSymbols > m_settings.m_nbSymbolsMax)
    {
        qDebug("MeshtasticDemodSink::tryHeaderLock: expected %u > max %u, falling back to EOM",
               m_expectedSymbols, m_settings.m_nbSymbolsMax);
        return;
    }

    m_headerLocked = true;

    qDebug("MeshtasticDemodSink::tryHeaderLock: LOCKED len=%u CR=%u CRC=%s LDRO=%s expected=%u symbols",
           packetLength, nbParityBits, hasCRC ? "on" : "off", ldro ? "on" : "off", m_expectedSymbols);
}

bool MeshtasticDemodSink::sendLoRaHeaderProbe()
{
    if (!m_decodeMsg || !m_decoderMsgQueue) {
        return false;
    }

    const std::vector<unsigned short>& symbols = m_decodeMsg->getSymbols();

    if (symbols.size() < 8U || !m_settings.m_hasHeader) {
        return false;
    }

    std::vector<unsigned short> headerSymbols(symbols.begin(), symbols.begin() + 8);
    const unsigned int payloadNbSymbolBits = (m_settings.m_spreadFactor > m_settings.m_deBits)
        ? (m_settings.m_spreadFactor - m_settings.m_deBits)
        : 1U;
    const unsigned int headerNbSymbolBits = (static_cast<unsigned int>(m_settings.m_spreadFactor) > 2U)
        ? (m_settings.m_spreadFactor - 2U)
        : payloadNbSymbolBits;

    MeshtasticDemodMsg::MsgLoRaHeaderProbe *probe = MeshtasticDemodMsg::MsgLoRaHeaderProbe::create(
        m_loRaFrameId,
        headerSymbols,
        payloadNbSymbolBits,
        headerNbSymbolBits,
        m_settings.m_spreadFactor,
        static_cast<unsigned int>(std::max(1, m_bandwidth)),
        m_settings.m_hasHeader,
        m_settings.m_hasCRC
    );
    m_decoderMsgQueue->push(probe);
    return true;
}

void MeshtasticDemodSink::applyLoRaHeaderFeedback(
    uint32_t frameId,
    bool valid,
    bool hasCRC,
    unsigned int nbParityBits,
    unsigned int packetLength,
    bool ldro,
    unsigned int expectedSymbols,
    int headerParityStatus,
    bool headerCRCStatus)
{
    (void) hasCRC;
    (void) ldro;
    (void) headerParityStatus;

    if ((m_settings.m_codingScheme != MeshtasticDemodSettings::CodingLoRa)
        || (m_loRaState != LoRaStateSFOCompensation))
    {
        return;
    }

    if (frameId != m_loRaFrameId) {
        return;
    }

    m_waitHeaderFeedback = false;
    m_headerFeedbackWaitSteps = 0;

    if (!valid || !headerCRCStatus || (packetLength == 0U) || (nbParityBits < 1U) || (nbParityBits > 4U))
    {
        qDebug("MeshtasticDemodSink::applyLoRaHeaderFeedback: invalid header -> reset frame");
        resetLoRaFrameSync();
        return;
    }

    if (expectedSymbols > m_settings.m_nbSymbolsMax)
    {
        qDebug("MeshtasticDemodSink::applyLoRaHeaderFeedback: expected %u > max %u, fallback to EOM",
            expectedSymbols, m_settings.m_nbSymbolsMax);
        return;
    }

    m_expectedSymbols = expectedSymbols;
    m_headerLocked = true;
    m_loRaReceivedHeader = true;
}

int MeshtasticDemodSink::loRaMod(int a, int b) const
{
    if (b <= 0) {
        return 0;
    }

    return (a % b + b) % b;
}

int MeshtasticDemodSink::loRaRound(float number) const
{
    return (number > 0.0f) ? static_cast<int>(number + 0.5f) : static_cast<int>(std::ceil(number - 0.5f));
}

void MeshtasticDemodSink::resetLoRaFrameSync()
{
    if ((m_settings.m_codingScheme == MeshtasticDemodSettings::CodingLoRa)
        && m_decodeMsg
        && (m_loRaState != LoRaStateDetect))
    {
        delete m_decodeMsg;
        m_decodeMsg = nullptr;
    }

    m_loRaState = LoRaStateDetect;
    m_loRaSyncState = LoRaSyncNetId1;
    m_loRaSampleFifo.clear();
    m_loRaSymbolCnt = 1;
    m_loRaBinIdx = 0;
    m_loRaKHat = 0;
    m_loRaDownVal = 0;
    m_loRaCFOInt = 0;
    m_loRaNetIdOff = 0;
    m_loRaAdditionalUpchirps = 0;
    m_loRaCFOFrac = 0.0f;
    m_loRaSTOFrac = 0.0f;
    m_loRaSFOHat = 0.0f;
    m_loRaSFOCum = 0.0f;
    m_loRaCFOSTOEstimated = false;
    m_loRaReceivedHeader = false;
    m_loRaOneSymbolOff = false;
    m_loRaFrameSymbolCount = 0;
    m_demodActive = false;
    m_headerLocked = false;
    m_expectedSymbols = 0;
    m_waitHeaderFeedback = false;
    m_headerFeedbackWaitSteps = 0;
    m_syncWord = 0;

    if (!m_loRaPreambleVals.empty()) {
        std::fill(m_loRaPreambleVals.begin(), m_loRaPreambleVals.end(), 0);
    }

    if (!m_loRaCFOFracCorrec.empty()) {
        std::fill(m_loRaCFOFracCorrec.begin(), m_loRaCFOFracCorrec.end(), Complex{1.0f, 0.0f});
    }
}

void MeshtasticDemodSink::clearSpectrumHistoryForNewFrame()
{
    if (!m_spectrumSink || !m_spectrumLine) {
        return;
    }

    // Insert a short floor separator between frames to make packet boundaries
    // visible even when consecutive packets arrive with a short idle gap.
    static constexpr unsigned int kSeparatorLines = 16U;
    for (unsigned int i = 0; i < kSeparatorLines; ++i) {
        m_spectrumSink->feed(m_spectrumLine, m_nbSymbols);
    }
}

unsigned int MeshtasticDemodSink::getLoRaSymbolVal(
    const Complex *samples,
    const Complex *refChirp,
    std::vector<float> *symbolMagnitudes,
    bool publishSpectrum
)
{
    for (unsigned int i = 0; i < m_nbSymbols; i++) {
        m_fft->in()[i] = samples[i] * refChirp[i];
    }

    // Canonical gr-lora_sdr demod uses a rectangular symbol window for
    // frame_sync/fft_demod symbol decisions. Do not apply user-selected FFT
    // windows in LoRa mode, otherwise header symbols drift and CRC checks fail.

    if (m_interpolatedFFTLength > m_fftLength) {
        std::fill(m_fft->in() + m_fftLength, m_fft->in() + m_interpolatedFFTLength, Complex{0.0f, 0.0f});
    }

    m_fft->transform();

    if (symbolMagnitudes)
    {
        symbolMagnitudes->assign(m_nbSymbols, 0.0f);

        for (unsigned int i = 0; i < m_nbSymbols; i++) {
            (*symbolMagnitudes)[i] = static_cast<float>(std::norm(m_fft->out()[i]));
        }
    }

    double magsq = 0.0;
    double magsqTotal = 0.0;
    const bool canCaptureSpectrum = (m_spectrumBuffer != nullptr);
    const bool publishSpectrumNow = publishSpectrum && (m_spectrumSink != nullptr) && canCaptureSpectrum;
    const unsigned int imax = argmax(
        m_fft->out(),
        m_fftInterpolation,
        m_fftLength,
        magsq,
        magsqTotal,
        canCaptureSpectrum ? m_spectrumBuffer : nullptr,
        m_fftInterpolation
    );

    if (publishSpectrumNow) {
        m_spectrumSink->feed(m_spectrumBuffer, m_nbSymbols);
    }

    return imax / m_fftInterpolation;
}

float MeshtasticDemodSink::estimateLoRaCFOFracBernier(const Complex *samples)
{
    if (m_loRaUpSymbToUse <= 1) {
        return 0.0f;
    }

    std::vector<int> k0(m_loRaUpSymbToUse, 0);
    std::vector<double> k0Mag(m_loRaUpSymbToUse, 0.0);
    std::vector<Complex> fftVal(m_loRaUpSymbToUse * m_nbSymbols);
    std::vector<Complex> dechirped(m_nbSymbols);

    for (int i = 0; i < m_loRaUpSymbToUse; i++)
    {
        const Complex *sym = samples + i * m_nbSymbols;

        for (unsigned int j = 0; j < m_nbSymbols; j++) {
            dechirped[j] = sym[j] * m_downChirps[j];
            m_fft->in()[j] = dechirped[j];
        }

        if (m_interpolatedFFTLength > m_fftLength) {
            std::fill(m_fft->in() + m_fftLength, m_fft->in() + m_interpolatedFFTLength, Complex{0.0f, 0.0f});
        }

        m_fft->transform();

        double magsq = 0.0;
        double magsqTotal = 0.0;
        const unsigned int imax = argmax(
            m_fft->out(),
            m_fftInterpolation,
            m_fftLength,
            magsq,
            magsqTotal,
            nullptr,
            m_fftInterpolation
        ) / m_fftInterpolation;
        k0[i] = static_cast<int>(imax);
        k0Mag[i] = magsq;

        for (unsigned int j = 0; j < m_nbSymbols; j++) {
            fftVal[j + i * m_nbSymbols] = m_fft->out()[j];
        }
    }

    const int idxMax = k0[std::distance(k0Mag.begin(), std::max_element(k0Mag.begin(), k0Mag.end()))];
    Complex fourCum(0.0f, 0.0f);

    for (int i = 0; i < m_loRaUpSymbToUse - 1; i++) {
        fourCum += fftVal[idxMax + m_nbSymbols * i] * std::conj(fftVal[idxMax + m_nbSymbols * (i + 1)]);
    }

    const float cfoFrac = -std::arg(fourCum) / (2.0f * static_cast<float>(M_PI));
    const unsigned int corrCount = static_cast<unsigned int>(m_loRaUpSymbToUse) * m_nbSymbols;

    for (unsigned int n = 0; n < corrCount && n < m_loRaPreambleUpchirps.size(); n++)
    {
        const float phase = -2.0f * static_cast<float>(M_PI) * cfoFrac * static_cast<float>(n) / static_cast<float>(m_nbSymbols);
        m_loRaPreambleUpchirps[n] = samples[n] * Complex(std::cos(phase), std::sin(phase));
    }

    return cfoFrac;
}

float MeshtasticDemodSink::estimateLoRaSTOFrac()
{
    if (m_loRaUpSymbToUse <= 0) {
        return 0.0f;
    }

    FFTFactory *fftFactory = DSPEngine::instance()->getFFTFactory();
    FFTEngine *fft2N = nullptr;
    const unsigned int fft2NLen = 2U * m_nbSymbols;
    const int fft2NSeq = fftFactory->getEngine(fft2NLen, false, &fft2N);
    std::vector<double> fftMagSq(fft2NLen, 0.0);
    std::vector<Complex> dechirped(m_nbSymbols);

    for (int i = 0; i < m_loRaUpSymbToUse; i++)
    {
        const Complex *sym = m_loRaPreambleUpchirps.data() + i * m_nbSymbols;

        for (unsigned int j = 0; j < m_nbSymbols; j++) {
            dechirped[j] = sym[j] * m_downChirps[j];
            fft2N->in()[j] = dechirped[j];
        }

        std::fill(fft2N->in() + m_nbSymbols, fft2N->in() + fft2NLen, Complex{0.0f, 0.0f});
        fft2N->transform();

        for (unsigned int j = 0; j < fft2NLen; j++) {
            fftMagSq[j] += std::norm(fft2N->out()[j]);
        }
    }

    fftFactory->releaseEngine(fft2NLen, false, fft2NSeq);

    const int k0 = static_cast<int>(std::distance(fftMagSq.begin(), std::max_element(fftMagSq.begin(), fftMagSq.end())));
    const double Y_1 = fftMagSq[loRaMod(k0 - 1, static_cast<int>(fft2NLen))];
    const double Y0 = fftMagSq[k0];
    const double Y1 = fftMagSq[loRaMod(k0 + 1, static_cast<int>(fft2NLen))];
    const double u = 64.0 * m_nbSymbols / 406.5506497;
    const double v = u * 2.4674;
    const double wa = (Y1 - Y_1) / (u * (Y1 + Y_1) + v * Y0 + 1e-12);
    const double ka = wa * m_nbSymbols / M_PI;
    const double kres = std::fmod((k0 + ka) / 2.0, 1.0);

    return static_cast<float>(kres - (kres > 0.5 ? 1.0 : 0.0));
}

void MeshtasticDemodSink::buildLoRaPayloadDownchirp()
{
    const int N = static_cast<int>(m_nbSymbols);
    const int id = loRaMod(m_loRaCFOInt, N);

    for (int n = 0; n < N; n++)
    {
        const int nFold = N - id;
        const double nD = static_cast<double>(n);
        const double ND = static_cast<double>(N);
        double phase;

        if (n < nFold) {
            phase = 2.0 * M_PI * ((nD * nD) / (2.0 * ND) + (static_cast<double>(id) / ND - 0.5) * nD);
        } else {
            phase = 2.0 * M_PI * ((nD * nD) / (2.0 * ND) + (static_cast<double>(id) / ND - 1.5) * nD);
        }

        const Complex up(std::cos(phase), std::sin(phase));
        Complex ref = m_settings.m_invertRamps ? up : std::conj(up);
        const float cfoPhase = -2.0f * static_cast<float>(M_PI) * m_loRaCFOFrac * static_cast<float>(n) / static_cast<float>(N);
        ref *= Complex(std::cos(cfoPhase), std::sin(cfoPhase));
        m_loRaPayloadDownchirp[n] = ref;
    }
}

void MeshtasticDemodSink::finalizeLoRaFrame()
{
    if (!m_decodeMsg) {
        resetLoRaFrameSync();
        return;
    }

    qDebug(
        "MeshtasticDemodSink::finalizeLoRaFrame: frameId=%u symbols=%u headerLocked=%d expected=%u",
        m_loRaFrameId,
        m_loRaFrameSymbolCount,
        m_headerLocked ? 1 : 0,
        m_expectedSymbols
    );

    m_decodeMsg->setSignalDb(CalcDb::dbPower(m_magsqOnAvg.asDouble() / (1 << m_settings.m_spreadFactor)));
    m_decodeMsg->setNoiseDb(CalcDb::dbPower(m_magsqOffAvg.asDouble() / (1 << m_settings.m_spreadFactor)));

    if (m_decoderMsgQueue && m_settings.m_decodeActive) {
        m_decoderMsgQueue->push(m_decodeMsg);
    } else {
        delete m_decodeMsg;
    }

    m_decodeMsg = nullptr;
    resetLoRaFrameSync();
}

void MeshtasticDemodSink::processSampleLoRa(const Complex& ci)
{
    m_loRaSampleFifo.push_back(ci);

    while (true)
    {
        const unsigned int needed = (m_loRaState == LoRaStateSync) ? (3U * m_loRaSymbolSpan) : m_loRaSymbolSpan;

        if (m_loRaSampleFifo.size() < needed) {
            return;
        }

        int consumed = processLoRaFrameSyncStep();

        if (consumed <= 0) {
            consumed = 1;
        }

        consumed = std::min(consumed, static_cast<int>(m_loRaSampleFifo.size()));

        for (int i = 0; i < consumed; i++) {
            m_loRaSampleFifo.pop_front();
        }
    }
}

int MeshtasticDemodSink::processLoRaFrameSyncStep()
{
    const int stoShift = loRaRound(m_loRaSTOFrac * static_cast<float>(m_osFactor));

    for (unsigned int ii = 0; ii < m_nbSymbols; ii++)
    {
        int idx = static_cast<int>(m_osCenterPhase + m_osFactor * ii) - stoShift;
        idx = std::max(0, std::min(idx, static_cast<int>(m_loRaSymbolSpan) - 1));
        m_loRaInDown[ii] = m_loRaSampleFifo[static_cast<size_t>(idx)];
    }

    if (m_loRaState == LoRaStateDetect)
    {
        const Complex *detectRef = m_settings.m_invertRamps ? m_upChirps : m_downChirps;
        // Keep last decoded packet visible until next frame starts.
        const int binNew = static_cast<int>(getLoRaSymbolVal(m_loRaInDown.data(), detectRef, nullptr, false));
        const int detectDelta = std::abs(loRaMod(std::abs(binNew - m_loRaBinIdx) + 1, static_cast<int>(m_nbSymbols)) - 1);
        const bool isConsecutive = (detectDelta <= 1);
        double symbolPower = 0.0;

        for (const Complex &s : m_loRaInDown) {
            symbolPower += std::norm(s);
        }

        symbolPower /= std::max(1U, m_nbSymbols);
        m_magsqTotalAvg(symbolPower);

        if (isConsecutive)
        {
            if (m_loRaSymbolCnt == 1 && !m_loRaPreambleVals.empty()) {
                m_loRaPreambleVals[0] = m_loRaBinIdx;
            }

            if ((m_loRaSymbolCnt >= 0) && (m_loRaSymbolCnt < static_cast<int>(m_loRaPreambleVals.size()))) {
                m_loRaPreambleVals[m_loRaSymbolCnt] = binNew;
            }

            const size_t sOfs = static_cast<size_t>(m_loRaSymbolCnt) * m_nbSymbols;
            if (sOfs + m_nbSymbols <= m_loRaPreambleRaw.size()) {
                std::copy_n(m_loRaInDown.begin(), m_nbSymbols, m_loRaPreambleRaw.begin() + sOfs);
            }

            const size_t upOfs = static_cast<size_t>(m_loRaSymbolCnt) * m_loRaSymbolSpan;
            if (upOfs + m_loRaSymbolSpan <= m_loRaPreambleRawUp.size()) {
                std::copy_n(m_loRaSampleFifo.begin(), m_loRaSymbolSpan, m_loRaPreambleRawUp.begin() + upOfs);
            }

            m_loRaSymbolCnt++;
        }
        else
        {
            m_magsqOffAvg(symbolPower);

            if (m_loRaPreambleRaw.size() >= m_nbSymbols) {
                std::copy_n(m_loRaInDown.begin(), m_nbSymbols, m_loRaPreambleRaw.begin());
            }

            if (m_loRaPreambleRawUp.size() >= m_loRaSymbolSpan) {
                std::copy_n(m_loRaSampleFifo.begin(), m_loRaSymbolSpan, m_loRaPreambleRawUp.begin());
            }

            m_loRaSymbolCnt = 1;
        }

        m_loRaBinIdx = binNew;

        if ((m_loRaSymbolCnt >= static_cast<int>(m_loRaRequiredUpchirps))
            && !m_loRaPreambleVals.empty())
        {
            m_loRaAdditionalUpchirps = 0;
            m_loRaState = LoRaStateSync;
            m_loRaSyncState = LoRaSyncNetId1;
            m_loRaSymbolCnt = 0;
            m_loRaCFOSTOEstimated = false;

            std::vector<unsigned int> hist(m_nbSymbols, 0U);
            unsigned int bestBin = 0U;
            unsigned int bestCount = 0U;

            for (int v : m_loRaPreambleVals)
            {
                const unsigned int b = static_cast<unsigned int>(loRaMod(v, static_cast<int>(m_nbSymbols)));
                const unsigned int c = ++hist[b];

                if (c > bestCount) {
                    bestCount = c;
                    bestBin = b;
                }
            }

            m_loRaKHat = static_cast<int>(bestBin);
            const int netStart = static_cast<int>(0.75f * static_cast<float>(m_loRaSymbolSpan)) - m_loRaKHat * static_cast<int>(m_osFactor);

            for (unsigned int i = 0; i < m_loRaSymbolSpan / 4U; i++)
            {
                const int src = std::max(0, std::min(netStart + static_cast<int>(i), static_cast<int>(m_loRaSampleFifo.size()) - 1));
                if (i < m_loRaNetIdSamp.size()) {
                    m_loRaNetIdSamp[i] = m_loRaSampleFifo[static_cast<size_t>(src)];
                }
            }

            return static_cast<int>(m_osFactor * (m_nbSymbols - bestBin));
        }

        return static_cast<int>(m_loRaSymbolSpan);
    }

    if (m_loRaState == LoRaStateSync)
    {
        if (!m_loRaCFOSTOEstimated)
        {
            const int cfoStart = std::max(0, static_cast<int>(m_nbSymbols) - m_loRaKHat);

            if (cfoStart < static_cast<int>(m_loRaPreambleRaw.size())) {
                m_loRaCFOFrac = estimateLoRaCFOFracBernier(m_loRaPreambleRaw.data() + cfoStart);
            } else {
                m_loRaCFOFrac = 0.0f;
            }

            m_loRaSTOFrac = estimateLoRaSTOFrac();

            for (unsigned int n = 0; n < m_nbSymbols; n++)
            {
                const float phase = -2.0f * static_cast<float>(M_PI) * m_loRaCFOFrac * static_cast<float>(n) / static_cast<float>(m_nbSymbols);
                m_loRaCFOFracCorrec[n] = Complex(std::cos(phase), std::sin(phase));
            }

            m_loRaCFOSTOEstimated = true;
        }

        for (unsigned int i = 0; i < m_nbSymbols; i++) {
            m_loRaSymbCorr[i] = m_loRaInDown[i] * m_loRaCFOFracCorrec[i];
        }

        const Complex *syncRef = m_settings.m_invertRamps ? m_upChirps : m_downChirps;
        const int binIdx = static_cast<int>(getLoRaSymbolVal(m_loRaSymbCorr.data(), syncRef, nullptr, true));

        switch (m_loRaSyncState)
        {
        case LoRaSyncNetId1:
            if ((binIdx == 0) || (binIdx == 1) || (binIdx == static_cast<int>(m_nbSymbols) - 1))
            {
                const size_t dstOfs = static_cast<size_t>(m_loRaRequiredUpchirps + m_loRaAdditionalUpchirps) * m_loRaSymbolSpan;

                if (dstOfs + m_loRaSymbolSpan <= m_loRaPreambleRawUp.size()) {
                    std::copy_n(m_loRaSampleFifo.begin(), m_loRaSymbolSpan, m_loRaPreambleRawUp.begin() + dstOfs);
                }

                m_loRaAdditionalUpchirps = std::min(m_loRaAdditionalUpchirps + 1, 3);
            }
            else
            {
                m_loRaSyncState = LoRaSyncNetId2;
                m_loRaNetIds[0] = binIdx;
            }
            break;
        case LoRaSyncNetId2:
            m_loRaSyncState = LoRaSyncDownchirp1;
            m_loRaNetIds[1] = binIdx;
            break;
        case LoRaSyncDownchirp1:
            m_loRaSyncState = LoRaSyncDownchirp2;
            break;
        case LoRaSyncDownchirp2:
            m_loRaDownVal = static_cast<int>(getLoRaSymbolVal(m_loRaSymbCorr.data(), m_settings.m_invertRamps ? m_downChirps : m_upChirps, nullptr, true));
            if (m_loRaAdditionalSymbolSamp.size() >= m_loRaSymbolSpan) {
                std::copy_n(m_loRaSampleFifo.begin(), m_loRaSymbolSpan, m_loRaAdditionalSymbolSamp.begin());
            }
            m_loRaSyncState = LoRaSyncQuarterDown;
            break;
        case LoRaSyncQuarterDown:
        default:
            if (m_loRaAdditionalSymbolSamp.size() >= 2U * m_loRaSymbolSpan) {
                std::copy_n(m_loRaSampleFifo.begin(), m_loRaSymbolSpan, m_loRaAdditionalSymbolSamp.begin() + m_loRaSymbolSpan);
            }

            if (static_cast<unsigned int>(m_loRaDownVal) < m_nbSymbols / 2U) {
                m_loRaCFOInt = static_cast<int>(std::floor(m_loRaDownVal / 2.0));
            } else {
                m_loRaCFOInt = static_cast<int>(std::floor((m_loRaDownVal - static_cast<int>(m_nbSymbols)) / 2.0));
            }

            const unsigned int upSymCount = std::min<unsigned int>(
                static_cast<unsigned int>(std::max(0, m_loRaUpSymbToUse)),
                static_cast<unsigned int>(m_loRaPreambleUpchirps.size() / std::max(1U, m_nbSymbols))
            );
            const unsigned int corrLen = upSymCount * m_nbSymbols;

            if (corrLen > 0U)
            {
                const int cfoIntMod = loRaMod(m_loRaCFOInt, static_cast<int>(m_nbSymbols));
                std::rotate(
                    m_loRaPreambleUpchirps.begin(),
                    m_loRaPreambleUpchirps.begin() + cfoIntMod,
                    m_loRaPreambleUpchirps.begin() + corrLen
                );

                std::vector<Complex> cfoIntCorrec(corrLen, Complex{1.0f, 0.0f});
                for (unsigned int n = 0; n < corrLen; n++)
                {
                    const float phase = -2.0f * static_cast<float>(M_PI)
                        * static_cast<float>(m_loRaCFOInt)
                        * static_cast<float>(n)
                        / static_cast<float>(m_nbSymbols);
                    cfoIntCorrec[n] = Complex(std::cos(phase), std::sin(phase));
                }

                for (unsigned int n = 0; n < corrLen; n++) {
                    m_loRaPreambleUpchirps[n] *= cfoIntCorrec[n];
                }

                if (m_deviceCenterFrequency > 0) {
                    m_loRaSFOHat =
                        (static_cast<float>(m_loRaCFOInt) + m_loRaCFOFrac)
                        * static_cast<float>(m_bandwidth)
                        / static_cast<float>(m_deviceCenterFrequency);
                } else {
                    m_loRaSFOHat = 0.0f;
                }

                std::vector<Complex> sfoCorrec(corrLen, Complex{1.0f, 0.0f});
                const double clkOff = static_cast<double>(m_loRaSFOHat) / static_cast<double>(m_nbSymbols);
                const double fs = static_cast<double>(m_bandwidth);
                const double fsP = fs * (1.0 - clkOff);
                const int N = static_cast<int>(m_nbSymbols);

                for (unsigned int n = 0; n < corrLen; n++)
                {
                    const double nMod = static_cast<double>(loRaMod(static_cast<int>(n), N));
                    const double nFloor = std::floor(static_cast<double>(n) / static_cast<double>(N));
                    const double q1 = (nMod * nMod) / (2.0 * static_cast<double>(N))
                        * ((m_bandwidth / fsP) * (m_bandwidth / fsP) - (m_bandwidth / fs) * (m_bandwidth / fs));
                    const double q2 = (nFloor * ((m_bandwidth / fsP) * (m_bandwidth / fsP) - (m_bandwidth / fsP))
                        + m_bandwidth / 2.0 * (1.0 / fs - 1.0 / fsP)) * nMod;
                    const double phase = -2.0 * M_PI * (q1 + q2);
                    sfoCorrec[n] = Complex(std::cos(phase), std::sin(phase));
                }

                for (unsigned int n = 0; n < corrLen; n++) {
                    m_loRaPreambleUpchirps[n] *= sfoCorrec[n];
                }

                const float tmpSto = estimateLoRaSTOFrac();
                const float diffSto = m_loRaSTOFrac - tmpSto;

                if (std::abs(diffSto) <= (static_cast<float>(m_osFactor) - 1.0f) / static_cast<float>(m_osFactor)) {
                    m_loRaSTOFrac = tmpSto;
                }

                std::vector<Complex> netIdsDec(2U * m_nbSymbols, Complex{0.0f, 0.0f});
                const int startOff = static_cast<int>(m_osFactor / 2U)
                    - loRaRound(m_loRaSTOFrac * static_cast<float>(m_osFactor))
                    + static_cast<int>(m_osFactor)
                        * (static_cast<int>(0.25f * static_cast<float>(m_nbSymbols)) + m_loRaCFOInt);

                for (unsigned int i = 0; i < 2U * m_nbSymbols; i++)
                {
                    const int idx = std::max(
                        0,
                        std::min(startOff + static_cast<int>(i * m_osFactor), static_cast<int>(m_loRaNetIdSamp.size()) - 1)
                    );
                    netIdsDec[i] = m_loRaNetIdSamp[static_cast<size_t>(idx)];
                }

                for (unsigned int i = 0; i < 2U * m_nbSymbols; i++)
                {
                    netIdsDec[i] *= cfoIntCorrec[i % std::max(1U, corrLen)];

                    const float phase = -2.0f * static_cast<float>(M_PI)
                        * m_loRaCFOFrac
                        * static_cast<float>(i % m_nbSymbols)
                        / static_cast<float>(m_nbSymbols);
                    netIdsDec[i] *= Complex(std::cos(phase), std::sin(phase));
                }

                const int netid1 = static_cast<int>(getLoRaSymbolVal(netIdsDec.data(), m_settings.m_invertRamps ? m_upChirps : m_downChirps));
                const int netid2 = static_cast<int>(getLoRaSymbolVal(netIdsDec.data() + m_nbSymbols, m_settings.m_invertRamps ? m_upChirps : m_downChirps));
                m_loRaNetIds[0] = netid1;
                m_loRaNetIds[1] = netid2;
                m_loRaNetIdOff = netid1;
            }
            else
            {
                if (m_deviceCenterFrequency > 0) {
                    m_loRaSFOHat =
                        (static_cast<float>(m_loRaCFOInt) + m_loRaCFOFrac)
                        * static_cast<float>(m_bandwidth)
                        / static_cast<float>(m_deviceCenterFrequency);
                } else {
                    m_loRaSFOHat = 0.0f;
                }
            }

            buildLoRaPayloadDownchirp();
            m_loRaFrameId++;
            m_decodeMsg = MeshtasticDemodMsg::MsgDecodeSymbols::create();
            m_decodeMsg->setFrameId(m_loRaFrameId);
            m_decodeMsg->setSyncWord(0U);
            clearSpectrumHistoryForNewFrame();
            m_loRaFrameSymbolCount = 0U;
            m_magsqMax = 0.0;
            m_headerLocked = false;
            m_expectedSymbols = 0;
            m_waitHeaderFeedback = false;
            m_headerFeedbackWaitSteps = 0U;
            m_demodActive = true;
            m_loRaSTOFrac += m_loRaSFOHat * 4.25f;
            if (std::abs(m_loRaSTOFrac) > 0.5f) {
                m_loRaSTOFrac += (m_loRaSTOFrac > 0.0f) ? -1.0f : 1.0f;
            }
            const float stoQuant = static_cast<float>(loRaRound(m_loRaSTOFrac * static_cast<float>(m_osFactor)));
            m_loRaSFOCum = ((m_loRaSTOFrac * static_cast<float>(m_osFactor)) - stoQuant) / static_cast<float>(m_osFactor);
            m_loRaState = LoRaStateSFOCompensation;
            m_loRaSyncState = LoRaSyncNetId1;
            return std::max(1, static_cast<int>(m_loRaSymbolSpan / 4U + static_cast<int>(m_osFactor) * m_loRaCFOInt));
        }

        return static_cast<int>(m_loRaSymbolSpan);
    }

    if (!m_decodeMsg)
    {
        resetLoRaFrameSync();
        return static_cast<int>(m_loRaSymbolSpan);
    }

    if (m_settings.m_hasHeader
        && !m_headerLocked
        && (m_loRaFrameSymbolCount >= 8U)
        && m_waitHeaderFeedback)
    {
        const unsigned int maxWaitSteps = std::max(1U, m_headerFeedbackMaxWaitSteps);

        if (++m_headerFeedbackWaitSteps > maxWaitSteps) {
            // Safety fallback when async feedback is delayed.
            m_waitHeaderFeedback = false;
            qDebug("MeshtasticDemodSink::processLoRaFrameSyncStep: header feedback timeout -> local fallback");
            tryHeaderLock();
        }
    }

    std::vector<float> symbolMags;
    const unsigned int rawSymbol = getLoRaSymbolVal(m_loRaInDown.data(), m_loRaPayloadDownchirp.data(), &symbolMags, true);
    const bool headerSymbol = m_settings.m_hasHeader && (m_loRaFrameSymbolCount < 8U);
    const unsigned short symbol = evalSymbol(rawSymbol, headerSymbol) % m_nbSymbolsEff;
    m_decodeMsg->pushBackSymbol(symbol);
    m_decodeMsg->pushBackMagnitudes(symbolMags);

    if (m_spectrumBuffer)
    {
        std::vector<float> spectrumLine;
        spectrumLine.reserve(m_nbSymbols);

        for (unsigned int i = 0; i < m_nbSymbols; ++i) {
            spectrumLine.push_back(static_cast<float>(std::norm(m_spectrumBuffer[i])));
        }

        m_decodeMsg->pushBackDechirpedSpectrumLine(spectrumLine);
    }

    double magsq = 0.0;
    for (const Complex &s : m_loRaInDown) {
        magsq += std::norm(s);
    }
    magsq /= std::max(1U, m_nbSymbols);

    if (magsq > m_magsqMax) {
        m_magsqMax = magsq;
    }

    m_magsqTotalAvg(magsq);
    m_magsqOnAvg(magsq);
    m_loRaFrameSymbolCount++;

    if (!m_headerLocked
        && m_settings.m_hasHeader
        && (m_loRaFrameSymbolCount == 8U))
    {
        if (sendLoRaHeaderProbe()) {
            m_waitHeaderFeedback = true;
            m_headerFeedbackWaitSteps = 0U;
        } else {
            tryHeaderLock();
        }
    }

    if (m_headerLocked)
    {
        if (m_loRaFrameSymbolCount >= m_expectedSymbols) {
            finalizeLoRaFrame();
        }
    }
    else if (m_loRaFrameSymbolCount >= m_settings.m_nbSymbolsMax)
    {
        finalizeLoRaFrame();
    }

    int itemsToConsume = static_cast<int>(m_loRaSymbolSpan);

    if (std::abs(m_loRaSFOCum) > (1.0f / (2.0f * static_cast<float>(m_osFactor))))
    {
        const int step = std::signbit(m_loRaSFOCum) ? -1 : 1;
        itemsToConsume -= step;
        m_loRaSFOCum -= step * (1.0f / static_cast<float>(m_osFactor));
    }

    m_loRaSFOCum += m_loRaSFOHat;
    return std::max(1, itemsToConsume);
}

void MeshtasticDemodSink::applyChannelSettings(int channelSampleRate, int bandwidth, int channelFrequencyOffset, bool force)
{
    qDebug() << "MeshtasticDemodSink::applyChannelSettings:"
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
        const int targetFrameSyncRate = std::max(1, bandwidth * static_cast<int>(m_osFactor));
        // Keep the anti-alias/channel filter narrow around the configured LoRa bandwidth.
        // A too-wide cutoff destabilizes preamble bin tracking in DETECT.
        m_interpolator.create(16, channelSampleRate, m_bandwidth / 1.9f);
        m_interpolatorDistance = (Real) channelSampleRate / (Real) targetFrameSyncRate;
        m_sampleDistanceRemain = 0;
        m_osCounter = 0;
        qDebug() << "MeshtasticDemodSink::applyChannelSettings: m_interpolator.create:"
            << " m_interpolatorDistance: " << m_interpolatorDistance
            << " targetFrameSyncRate: " << targetFrameSyncRate
            << " osFactor: " << m_osFactor;
    }

    m_channelSampleRate = channelSampleRate;
    m_bandwidth = bandwidth;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void MeshtasticDemodSink::applySettings(const MeshtasticDemodSettings& settings, bool force)
{
    qDebug() << "MeshtasticDemodSink::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_bandwidthIndex: " << settings.m_bandwidthIndex
            << " m_spreadFactor: " << settings.m_spreadFactor
            << " m_rgbColor: " << settings.m_rgbColor
            << " m_title: " << settings.m_title
            << " force: " << force;

    const unsigned int desiredFFTInterpolation = (settings.m_codingScheme == MeshtasticDemodSettings::CodingLoRa)
        ? m_loRaFFTInterpolation
        : m_legacyFFTInterpolation;
    const bool fftInterpChanged = desiredFFTInterpolation != m_fftInterpolation;

    if ((settings.m_spreadFactor != m_settings.m_spreadFactor)
     || (settings.m_deBits != m_settings.m_deBits)
     || (settings.m_fftWindow != m_settings.m_fftWindow)
     || fftInterpChanged
     || force)
    {
        m_fftInterpolation = desiredFFTInterpolation;
        initSF(settings.m_spreadFactor, settings.m_deBits, settings.m_fftWindow);
    }

    const unsigned int configuredPreamble = settings.m_preambleChirps > 0U
        ? settings.m_preambleChirps
        : m_minRequiredPreambleChirps;
    const unsigned int targetRequired = configuredPreamble > 3U
        ? (configuredPreamble - 3U)
        : m_minRequiredPreambleChirps;
    m_requiredPreambleChirps = std::max(
        m_minRequiredPreambleChirps,
        std::min(targetRequired, m_maxRequiredPreambleChirps)
    );
    qDebug() << "MeshtasticDemodSink::applySettings:"
            << " requiredPreambleChirps: " << m_requiredPreambleChirps
            << " configuredPreamble: " << settings.m_preambleChirps
            << " fftInterpolation: " << m_fftInterpolation;

    m_settings = settings;
    m_loRaRequiredUpchirps = m_requiredPreambleChirps;
    m_loRaUpSymbToUse = (m_loRaRequiredUpchirps > 0U) ? static_cast<int>(m_loRaRequiredUpchirps - 1U) : 0;
    m_loRaPreambleVals.assign(m_loRaRequiredUpchirps, 0);
    m_loRaPreambleRaw.assign(m_nbSymbols * m_loRaRequiredUpchirps, Complex{0.0f, 0.0f});
    m_loRaPreambleUpchirps.assign(m_nbSymbols * m_loRaRequiredUpchirps, Complex{0.0f, 0.0f});
    m_loRaPreambleRawUp.assign((m_settings.m_preambleChirps + 3U) * m_loRaSymbolSpan, Complex{0.0f, 0.0f});
    m_loRaCFOFracCorrec.assign(m_nbSymbols, Complex{1.0f, 0.0f});
    m_loRaPayloadDownchirp.assign(m_nbSymbols, Complex{1.0f, 0.0f});
    m_loRaSymbCorr.assign(m_nbSymbols, Complex{0.0f, 0.0f});
    m_loRaNetIdSamp.assign((m_loRaSymbolSpan * 5U) / 2U + m_loRaSymbolSpan, Complex{0.0f, 0.0f});
    m_loRaAdditionalSymbolSamp.assign(m_loRaSymbolSpan * 2U, Complex{0.0f, 0.0f});
    resetLoRaFrameSync();
}
