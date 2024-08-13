///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include <stdio.h>

#include <QTime>
#include <QDebug>

#include "dsp/spectrumvis.h"
#include "dsp/datafifo.h"
#include "util/db.h"
#include "util/messagequeue.h"
#include "maincore.h"
#include "RXA.hpp"
#include "meter.hpp"
#include "patchpanel.hpp"
#include "wcpAGC.hpp"
#include "anr.hpp"
#include "emnr.hpp"
#include "snba.hpp"
#include "anf.hpp"
#include "anb.hpp"
#include "nob.hpp"
#include "amd.hpp"
#include "fmd.hpp"
#include "ssql.hpp"
#include "amsq.hpp"
#include "fmsq.hpp"
#include "eqp.hpp"
#include "shift.hpp"
#include "speak.hpp"

#include "wdsprxsink.h"

const int WDSPRxSink::m_ssbFftLen = 2048;
const int WDSPRxSink::m_wdspSampleRate = 48000;
const int WDSPRxSink::m_wdspBufSize = 512;

WDSPRxSink::SpectrumProbe::SpectrumProbe(SampleVector& sampleVector) :
    m_sampleVector(sampleVector),
    m_spanLog2(0),
    m_dsb(false),
    m_usb(true),
    m_sum(0)
{}

void WDSPRxSink::SpectrumProbe::setSpanLog2(int spanLog2)
{
    m_spanLog2 = spanLog2;
}

void WDSPRxSink::SpectrumProbe::proceed(const float *in, int nb_samples)
{
    int decim = 1<<(m_spanLog2 - 1);
    unsigned char decim_mask = decim - 1; // counter LSB bit mask for decimation by 2^(m_scaleLog2 - 1)

    for (int i = 0; i < nb_samples; i++)
    {
        float cr = in[2*i+1];
        float ci = in[2*i];
        m_sum += std::complex<float>{cr, ci};

        if (decim == 1)
        {
            m_sampleVector.push_back(Sample(cr*SDR_RX_SCALEF, ci*SDR_RX_SCALEF));
        }
        else
        {
            if (!(m_undersampleCount++ & decim_mask))
            {
                float avgr = m_sum.real() / (float) decim;
                float avgi = m_sum.imag() / (float) decim;

                if (!m_dsb && !m_usb)
                { // invert spectrum for LSB
                    m_sampleVector.push_back(Sample(avgi*SDR_RX_SCALEF, avgr*SDR_RX_SCALEF));
                }
                else
                {
                    m_sampleVector.push_back(Sample(avgr*SDR_RX_SCALEF, avgi*SDR_RX_SCALEF));
                }

                m_sum = 0;
            }
        }
    }
}

WDSPRxSink::WDSPRxSink() :
        m_squelchDelayLine(2*48000),
        m_audioActive(false),
        m_spectrumSink(nullptr),
        m_spectrumProbe(m_sampleBuffer),
        m_inCount(0),
        m_audioFifo(24000),
        m_audioSampleRate(48000)
{
	m_Bandwidth = 5000;
	m_channelSampleRate = 48000;
	m_channelFrequencyOffset = 0;

	m_audioBuffer.resize(m_audioSampleRate / 10);
	m_audioBufferFill = 0;
	m_undersampleCount = 0;

    m_demodBuffer.resize(1<<12);
    m_demodBufferFill = 0;

    m_sAvg = 0.0;
    m_sPeak = 0.0;
    m_sCount = m_wdspBufSize;

    m_rxa = new WDSP::RXA(
        m_wdspSampleRate, // input samplerate
        m_wdspSampleRate, // output samplerate
        m_wdspSampleRate, // sample rate for mainstream dsp processing (dsp)
        m_wdspBufSize     // number complex samples processed per buffer in mainstream dsp processing
    );
    m_rxa->setSpectrumProbe(&m_spectrumProbe);
    m_rxa->setPassband(0, m_Bandwidth);

    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
	applySettings(m_settings, true);
}

WDSPRxSink::~WDSPRxSink()
{
    delete m_rxa;
}

void WDSPRxSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    if (m_channelSampleRate == 0) {
        return;
    }

    Complex ci;

	for(SampleVector::const_iterator it = begin; it < end; ++it)
	{
		Complex c(it->real(), it->imag());
		c *= m_nco.nextIQ();

        if (m_interpolatorDistance < 1.0f) // interpolate
        {
            while (!m_interpolator.interpolate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
        else
        {
            if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
    }
}

void WDSPRxSink::getMagSqLevels(double& avg, double& peak, int& nbSamples) const
{
    avg = m_sAvg;
    peak = m_sPeak;
    nbSamples = m_sCount;
}

void WDSPRxSink::processOneSample(const Complex &ci)
{
    m_rxa->get_inbuff()[2*m_inCount] = ci.imag() / SDR_RX_SCALEF;
    m_rxa->get_inbuff()[2*m_inCount+1] = ci.real() / SDR_RX_SCALEF;

    if (++m_inCount == m_rxa->get_insize())
    {
        m_rxa->execute();

        m_sCount = m_wdspBufSize;
        m_sAvg = m_rxa->smeter->getMeter(WDSP::RXA::RXA_S_AV);
        m_sPeak = m_rxa->smeter->getMeter(WDSP::RXA::RXA_S_PK);

        for (int i = 0; i < m_rxa->get_outsize(); i++)
        {
            if (m_settings.m_audioMute)
            {
                m_audioBuffer[m_audioBufferFill].r = 0;
                m_audioBuffer[m_audioBufferFill].l = 0;
            }
            else
            {
                const double& dr = m_rxa->get_outbuff()[2*i+1];
                const double& di = m_rxa->get_outbuff()[2*i];
                qint16 zr = dr * 32768.0;
                qint16 zi = di * 32768.0;
                m_audioBuffer[m_audioBufferFill].r = zr;
                m_audioBuffer[m_audioBufferFill].l = zi;

                if (m_settings.m_audioBinaural)
                {
                    m_demodBuffer[m_demodBufferFill++] = zr;
                    m_demodBuffer[m_demodBufferFill++] = zi;
                }
                else
                {
                    Real demod = (zr + zi) * 0.7;
                    auto sample = (qint16)(demod);
                    m_demodBuffer[m_demodBufferFill++] = sample;
                }

                if (m_demodBufferFill >= m_demodBuffer.size())
                {
                    QList<ObjectPipe*> dataPipes;
                    MainCore::instance()->getDataPipes().getDataPipes(m_channel, "demod", dataPipes);

                    if (!dataPipes.empty())
                    {
                        for (auto dataPipe : dataPipes)
                        {
                            DataFifo *fifo = qobject_cast<DataFifo*>(dataPipe->m_element);

                            if (fifo)
                            {
                                fifo->write(
                                    (quint8*) &m_demodBuffer[0],
                                    m_demodBuffer.size() * sizeof(qint16),
                                    m_settings.m_audioBinaural ? DataFifo::DataTypeCI16 : DataFifo::DataTypeI16
                                );
                            }
                        }
                }

                    m_demodBufferFill = 0;
                }
            } // audio sample

            if (++m_audioBufferFill == m_audioBuffer.size())
            {
                std::size_t res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], std::min(m_audioBufferFill, m_audioBuffer.size()));

                if (res != m_audioBufferFill) {
                    qDebug("WDSPRxSink::processOneSample: %lu/%lu samples written", res, m_audioBufferFill);
                }

                m_audioBufferFill = 0;
            }
        } // result loop

        if (m_spectrumSink && (m_sampleBuffer.size() != 0))
        {
            m_spectrumSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), !m_settings.m_dsb);
            m_sampleBuffer.clear();
        }

        m_inCount = 0;
    }
}

void WDSPRxSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "WDSPRxSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((m_channelFrequencyOffset != channelFrequencyOffset) ||
        (m_channelSampleRate != channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((m_channelSampleRate != channelSampleRate) || force)
    {
        Real interpolatorBandwidth = (m_Bandwidth * 1.5f) > channelSampleRate ? channelSampleRate : (m_Bandwidth * 1.5f);
        m_interpolator.create(16, channelSampleRate, interpolatorBandwidth, 2.0f);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) channelSampleRate / (Real) m_wdspSampleRate;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void WDSPRxSink::applyAudioSampleRate(int sampleRate)
{
    qDebug("WDSPRxSink::applyAudioSampleRate: %d", sampleRate);

    Real interpolatorBandwidth = (m_Bandwidth * 1.5f) > m_channelSampleRate ? m_channelSampleRate : (m_Bandwidth * 1.5f);
    m_interpolator.create(16, m_channelSampleRate, interpolatorBandwidth, 2.0f);
    m_interpolatorDistanceRemain = 0;
    m_interpolatorDistance = (Real) m_channelSampleRate / (Real) m_wdspSampleRate;

    m_rxa->setOutputSamplerate(sampleRate);

    m_audioFifo.setSize(sampleRate);
    m_audioSampleRate = sampleRate;
    m_audioBuffer.resize(sampleRate / 10);
    m_audioBufferFill = 0;

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_channel, "reportdemod", pipes);

    if (!pipes.empty())
    {
        for (const auto& pipe : pipes)
        {
            MessageQueue* messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);

            if (messageQueue)
            {
                MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(m_channel, sampleRate);
                messageQueue->push(msg);
            }
        }
    }
}

void WDSPRxSink::applySettings(const WDSPRxSettings& settings, bool force)
{
    qDebug() << "WDSPRxSink::applySettings:"
            << " m_demod: " << settings.m_demod
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_profileIndex: " << settings.m_profileIndex
            << " m_spanLog2: " << settings.m_profiles[settings.m_profileIndex].m_spanLog2
            << " m_highCutoff: " << settings.m_profiles[settings.m_profileIndex].m_highCutoff
            << " m_lowCutoff: " << settings.m_profiles[settings.m_profileIndex].m_lowCutoff
            << " m_fftWindow: " << settings.m_profiles[settings.m_profileIndex].m_fftWindow << "]"
            << " m_volume: " << settings.m_volume
            << " m_audioBinaural: " << settings.m_audioBinaural
            << " m_audioFlipChannels: " << settings.m_audioFlipChannels
            << " m_dsb: " << settings.m_dsb
            << " m_audioMute: " << settings.m_audioMute
            << " m_agc: " << settings.m_agc
            << " m_agcMode: " << settings.m_agcMode
            << " m_agcGain: " << settings.m_agcGain
            << " m_agcSlope: " << settings.m_agcSlope
            << " m_agcHangThreshold: " << settings.m_agcHangThreshold
            << " m_audioDeviceName: " << settings.m_audioDeviceName
            << " m_dnr: " << settings.m_dnr
            << " m_nrScheme: " << settings.m_nrScheme
            << " m_nrPosition: "<< settings.m_nrPosition
            << " m_nr2Gain: " << settings.m_nr2Gain
            << " m_nr2NPE: " << settings.m_nr2NPE
            << " m_nr2ArtifactReduction: " << settings.m_nr2ArtifactReduction
            << " m_rit: " << settings.m_rit
            << " m_ritFrequency: " <<  settings.m_ritFrequency
            << " m_streamIndex: " << settings.m_streamIndex
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIPort: " << settings.m_reverseAPIPort
            << " m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex
            << " m_reverseAPIChannelIndex: " << settings.m_reverseAPIChannelIndex
            << " force: " << force;


    // RIT

    if ((m_settings.m_rit != settings.m_rit) || (m_settings.m_ritFrequency != settings.m_ritFrequency) || force)
    {
        m_rxa->shift->SetFreq(settings.m_ritFrequency);
        m_rxa->shift->SetRun(settings.m_rit ? 1 : 0);
    }

    // Filter and mode

    if((m_settings.m_profiles[m_settings.m_profileIndex].m_highCutoff != settings.m_profiles[settings.m_profileIndex].m_highCutoff) ||
        (m_settings.m_profiles[m_settings.m_profileIndex].m_lowCutoff != settings.m_profiles[settings.m_profileIndex].m_lowCutoff) ||
        (m_settings.m_profiles[m_settings.m_profileIndex].m_fftWindow != settings.m_profiles[settings.m_profileIndex].m_fftWindow) ||
        (m_settings.m_demod != settings.m_demod) ||
        (m_settings.m_dsb != settings.m_dsb) || force)
    {
        float band;
        float low;
        float high;
        float fLow;
        float fHigh;
        bool usb;
        bool dsb;

        band = settings.m_profiles[settings.m_profileIndex].m_highCutoff;
        high = band;
        low = settings.m_profiles[settings.m_profileIndex].m_lowCutoff;

        if (band < 0)
        {
            band = -band;
            m_spectrumProbe.setUSB(false);
            usb = false;
        }
        else
        {
            m_spectrumProbe.setUSB(true);
            usb = true;
        }

        m_Bandwidth = band;

        if (high < low)
        {
            if (settings.m_dsb)
            {
                fLow = high;
                fHigh = -high;
                m_spectrumProbe.setDSB(true);
                dsb = true;
            }
            else
            {
                fLow = high;
                fHigh = low;
                m_spectrumProbe.setDSB(false);
                dsb = false;
            }
        }
        else
        {
            if (settings.m_dsb)
            {
                fLow = -high;
                fHigh = high;
                m_spectrumProbe.setDSB(true);
                dsb = true;
            }
            else
            {
                fLow = low;
                fHigh = high;
                m_spectrumProbe.setDSB(false);
                dsb = false;
            }
        }

        Real interpolatorBandwidth = (m_Bandwidth * 1.5f) > m_channelSampleRate ? m_channelSampleRate : (m_Bandwidth * 1.5f);
        m_interpolator.create(16, m_channelSampleRate, interpolatorBandwidth, 2.0f);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) m_audioSampleRate;

        m_rxa->setPassband(fLow, fHigh);
        m_rxa->nbpSetWindow(m_settings.m_profiles[m_settings.m_profileIndex].m_fftWindow);

        if (settings.m_demod == WDSPRxProfile::DemodSSB)
        {
            if (dsb) {
                m_rxa->setMode(WDSP::RXA::RXA_DSB);
            } else {
                m_rxa->setMode(usb ? WDSP::RXA::RXA_USB : WDSP::RXA::RXA_LSB);
            }
        }
        else if (settings.m_demod == WDSPRxProfile::DemodAM)
        {
            m_rxa->setMode(WDSP::RXA::RXA_AM);
        }
        else if (settings.m_demod == WDSPRxProfile::DemodSAM)
        {
            m_rxa->setMode(WDSP::RXA::RXA_SAM);

            if (dsb) {
                m_rxa->amd->setSBMode(0);
            } else {
                m_rxa->amd->setSBMode(usb ? 2 : 1);
            }
        }
        else if (settings.m_demod == WDSPRxProfile::DemodFMN)
        {
            m_rxa->setMode(WDSP::RXA::RXA_FM);
        }
    }

    if ((m_settings.m_profiles[settings.m_profileIndex].m_spanLog2 != settings.m_profiles[settings.m_profileIndex].m_spanLog2) || force) {
        m_spectrumProbe.setSpanLog2(settings.m_profiles[settings.m_profileIndex].m_spanLog2);
    }

    // Noise Reduction

    if ((m_settings.m_dnr != settings.m_dnr)
    || (m_settings.m_nrScheme != settings.m_nrScheme) || force)
    {
        m_rxa->setANRRun(0);
        m_rxa->setEMNRRun(0);

        if (settings.m_dnr)
        {
            switch (settings.m_nrScheme)
            {
            case WDSPRxProfile::NRSchemeNR:
                m_rxa->setANRRun(1);
                break;
            case WDSPRxProfile::NRSchemeNR2:
                m_rxa->setEMNRRun(1);
                break;
            default:
                break;
            }
        }
    }

    if ((m_settings.m_nrPosition != settings.m_nrPosition) || force)
    {
        switch (settings.m_nrPosition)
        {
        case WDSPRxProfile::NRPositionPreAGC:
            m_rxa->setANRPosition(0);
            m_rxa->setEMNRPosition(0);
            break;
        case WDSPRxProfile::NRPositionPostAGC:
            m_rxa->setANRPosition(1);
            m_rxa->setEMNRPosition(1);
            break;
        default:
            break;
        }
    }

    if ((m_settings.m_nr2Gain != settings.m_nr2Gain) || force)
    {
        switch (settings.m_nr2Gain)
        {
        case WDSPRxProfile::NR2GainLinear:
            m_rxa->emnr->setGainMethod(0);
            break;
        case WDSPRxProfile::NR2GainLog:
            m_rxa->emnr->setGainMethod(1);
            break;
        case WDSPRxProfile::NR2GainGamma:
            m_rxa->emnr->setGainMethod(2);
            break;
        default:
            break;
        }
    }

    if ((m_settings.m_nr2NPE != settings.m_nr2NPE) || force)
    {
        switch (settings.m_nr2NPE)
        {
        case WDSPRxProfile::NR2NPEOSMS:
            m_rxa->emnr->setNpeMethod(0);
            break;
        case WDSPRxProfile::NR2NPEMMSE:
            m_rxa->emnr->setNpeMethod(1);
            break;
        default:
            break;
        }
    }

    if ((m_settings.m_nr2ArtifactReduction != settings.m_nr2ArtifactReduction) || force) {
        m_rxa->emnr->setAeRun(settings.m_nr2ArtifactReduction ? 1 : 0);
    }

    if ((m_settings.m_anf != settings.m_anf) || force) {
        m_rxa->setANFRun(settings.m_anf ? 1 : 0);
    }

    // Caution: Causes corruption
    if ((m_settings.m_snb != settings.m_snb) || force) {
        m_rxa->setSNBARun(settings.m_snb ? 1 : 0);
    }

    // CW Peaking

    if ((m_settings.m_cwPeaking != settings.m_cwPeaking) || force) {
        m_rxa->speak->setRun(settings.m_cwPeaking ? 1 : 0);
    }

    if ((m_settings.m_cwPeakFrequency != settings.m_cwPeakFrequency) || force) {
        m_rxa->speak->setFreq(settings.m_cwPeakFrequency);
    }

    if ((m_settings.m_cwBandwidth != settings.m_cwBandwidth) || force) {
        m_rxa->speak->setBandwidth(settings.m_cwBandwidth);
    }

    if ((m_settings.m_cwGain != settings.m_cwGain) || force) {
        m_rxa->speak->setGain(settings.m_cwGain);
    }

    // Noise Blanker

    if ((m_settings.m_dnb != settings.m_dnb)
    || (m_settings.m_nbScheme != settings.m_nbScheme) || force)
    {
        m_rxa->anb->setRun(0);
        m_rxa->nob->setRun(0);

        if (settings.m_dnb)
        {
            switch(settings.m_nbScheme)
            {
            case WDSPRxProfile::NBSchemeNB:
                m_rxa->anb->setRun(1);
                break;
            case WDSPRxProfile::NBSchemeNB2:
                m_rxa->nob->setRun(1);
                break;
            default:
                break;
            }
        }
    }

    if ((m_settings.m_nbSlewTime != settings.m_nbSlewTime) || force)
    {
        m_rxa->anb->setTau(settings.m_nbSlewTime * 0.001);
        m_rxa->nob->setTau(settings.m_nbSlewTime * 0.001);
    }

    if ((m_settings.m_nbLeadTime != settings.m_nbLeadTime) || force)
    {
        m_rxa->anb->setAdvtime(settings.m_nbLeadTime * 0.001);
        m_rxa->nob->setAdvtime(settings.m_nbLeadTime * 0.001);
    }

    if ((m_settings.m_nbLagTime != settings.m_nbLagTime) || force)
    {
        m_rxa->anb->setHangtime(settings.m_nbLagTime * 0.001);
        m_rxa->nob->setHangtime(settings.m_nbLagTime * 0.001);
    }

    if ((m_settings.m_nbThreshold != settings.m_nbThreshold) || force)
    {
        m_rxa->anb->setThreshold(settings.m_nbThreshold);
        m_rxa->nob->setThreshold(settings.m_nbThreshold);
    }

    if ((m_settings.m_nbAvgTime != settings.m_nbAvgTime) || force)
    {
        m_rxa->anb->setBacktau(settings.m_nbAvgTime * 0.001);
        m_rxa->nob->setBacktau(settings.m_nbAvgTime * 0.001);
    }

    // AM option

    if ((m_settings.m_amFadeLevel != settings.m_amFadeLevel) || force) {
        m_rxa->amd->setFadeLevel(settings.m_amFadeLevel);
    }

    // FM options

    if ((m_settings.m_fmDeviation != settings.m_fmDeviation) || force) {
        m_rxa->fmd->setDeviation(settings.m_fmDeviation);
    }

    if ((m_settings.m_fmAFLow != settings.m_fmAFLow)
    || (m_settings.m_fmAFHigh != settings.m_fmAFHigh) || force)
    {
        m_rxa->fmd->setAFFilter(settings.m_fmAFLow, settings.m_fmAFHigh);
    }

    if ((m_settings.m_fmAFLimiter != settings.m_fmAFLimiter) || force) {
        m_rxa->fmd->setLimRun(settings.m_fmAFLimiter ? 1 : 0);
    }

    if ((m_settings.m_fmAFLimiterGain != settings.m_fmAFLimiterGain) || force) {
        m_rxa->fmd->setLimGain(settings.m_fmAFLimiterGain);
    }

    if ((m_settings.m_fmCTCSSNotch != settings.m_fmCTCSSNotch) || force) {
        m_rxa->fmd->setCTCSSRun(settings.m_fmCTCSSNotch ? 1 : 0);
    }

    if ((m_settings.m_fmCTCSSNotchFrequency != settings.m_fmCTCSSNotchFrequency) || force) {
        m_rxa->fmd->setCTCSSFreq(settings.m_fmCTCSSNotchFrequency);
    }

    // Squelch

    if ((m_settings.m_squelch != settings.m_squelch)
    || (m_settings.m_squelchThreshold != settings.m_squelchThreshold)
    || (m_settings.m_squelchMode != settings.m_squelchMode) || force)
    {
        m_rxa->ssql->setRun(0);
        m_rxa->amsq->setRun(0);
        m_rxa->fmsq->setRun(0);

        if (settings.m_squelch)
        {
            switch(settings.m_squelchMode)
            {
            case WDSPRxProfile::SquelchModeVoice:
            {
                m_rxa->ssql->setRun(1);
                double threshold = 0.0075 * settings.m_squelchThreshold;
                m_rxa->ssql->setThreshold(threshold);
            }
                break;
            case WDSPRxProfile::SquelchModeAM:
            {
                m_rxa->amsq->setRun(1);
                double threshold = ((settings.m_squelchThreshold / 100.0) * 160.0) - 160.0;
                m_rxa->amsq->setThreshold(threshold);
            }
                break;
            case WDSPRxProfile::SquelchModeFM:
            {
                m_rxa->fmsq->setRun(1);
                double threshold = pow(10.0, -2.0 * ((double) settings.m_squelchThreshold) / 100.0);
                qDebug("WDSPRxSink::applySettings: FM squelch %lf", threshold);
                m_rxa->fmsq->setThreshold(threshold);
            }
                break;
            default:
                break;
            }
        }
    }

    if ((m_settings.m_ssqlTauMute != settings.m_ssqlTauMute) || force) {
        m_rxa->ssql->setTauMute(settings.m_ssqlTauMute);
    }

    if ((m_settings.m_ssqlTauUnmute != settings.m_ssqlTauUnmute) || force) {
        m_rxa->ssql->setTauUnMute(settings.m_ssqlTauUnmute);
    }

    if ((m_settings.m_amsqMaxTail != settings.m_amsqMaxTail) || force) {
        m_rxa->amsq->setMaxTail(settings.m_amsqMaxTail);
    }

    // Equalizer

    if ((m_settings.m_equalizer != settings.m_equalizer) || force) {
        m_rxa->eqp->setRun(settings.m_equalizer ? 1 : 0);
    }

    if ((m_settings.m_eqF != settings.m_eqF)
    || (m_settings.m_eqG != settings.m_eqG) || force)
    {
        m_rxa->eqp->setProfile(10, settings.m_eqF.data(), settings.m_eqG.data());
    }

    // Audio panel

    if ((m_settings.m_volume != settings.m_volume) || force) {
        m_rxa->panel->setGain1(settings.m_volume);
    }

    if ((m_settings.m_audioBinaural != settings.m_audioBinaural)
    || (m_settings.m_audioPan != settings.m_audioPan)
    || (m_settings.m_audioFlipChannels != settings.m_audioFlipChannels) || force)
    {
        if (settings.m_audioBinaural)
        {
            m_rxa->panel->setCopy(settings.m_audioFlipChannels ? 3 : 0);
            m_rxa->panel->setPan(settings.m_audioPan);
        }
        else
        {
            m_rxa->panel->setCopy(settings.m_audioFlipChannels ? 2 : 1);
            m_rxa->panel->setPan(0.5);
        }
    }

    // AGC

    if ((m_settings.m_agc != settings.m_agc)
    || (m_settings.m_agcMode != settings.m_agcMode)
    || (m_settings.m_agcSlope != settings.m_agcSlope)
    || (m_settings.m_agcHangThreshold != settings.m_agcHangThreshold)
    || (m_settings.m_agcGain != settings.m_agcGain) || force)
    {
        m_rxa->agc->setSlope(settings.m_agcSlope);
        m_rxa->agc->setTop((float) settings.m_agcGain);

        if (settings.m_agc)
        {
            switch (settings.m_agcMode)
            {
            case WDSPRxProfile::WDSPRxAGCMode::AGCLong:
                m_rxa->agc->setMode(1);
                m_rxa->agc->setAttack(2);
                m_rxa->agc->setHang(2000);
                m_rxa->agc->setDecay(2000);
                m_rxa->agc->setHangThreshold(settings.m_agcHangThreshold);
                break;
            case WDSPRxProfile::WDSPRxAGCMode::AGCSlow:
                m_rxa->agc->setMode(2);
                m_rxa->agc->setAttack(2);
                m_rxa->agc->setHang(1000);
                m_rxa->agc->setDecay(500);
                m_rxa->agc->setHangThreshold(settings.m_agcHangThreshold);
                break;
            case WDSPRxProfile::WDSPRxAGCMode::AGCMedium:
                m_rxa->agc->setMode(3);
                m_rxa->agc->setAttack(2);
                m_rxa->agc->setHang(0);
                m_rxa->agc->setDecay(250);
                m_rxa->agc->setHangThreshold(settings.m_agcHangThreshold);
                break;
            case WDSPRxProfile::WDSPRxAGCMode::AGCFast:
                m_rxa->agc->setMode(4);
                m_rxa->agc->setAttack(2);
                m_rxa->agc->setHang(0);
                m_rxa->agc->setDecay(50);
                m_rxa->agc->setHangThreshold(settings.m_agcHangThreshold);
                break;
            }
        }
        else
        {
            m_rxa->agc->setMode(0);
        }
    }

    m_settings = settings;
}
