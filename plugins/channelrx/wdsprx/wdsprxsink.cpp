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
#include "nbp.hpp"
#include "meter.hpp"
#include "patchpanel.hpp"
#include "wcpAGC.hpp"

#include "wdsprxsink.h"

const int WDSPRxSink::m_ssbFftLen = 2048;
const int WDSPRxSink::m_agcTarget = 3276; // 32768/10 -10 dB amplitude => -20 dB power: center of normal signal
const int WDSPRxSink::m_wdspSampleRate = 48000;
const int WDSPRxSink::m_wdspBufSize = 512;

WDSPRxSink::SpectrumProbe::SpectrumProbe(SampleVector& sampleVector) :
    m_sampleVector(sampleVector)
{}

void WDSPRxSink::SpectrumProbe::proceed(const double *in, int nb_samples)
{
    for (int i = 0; i < nb_samples; i++) {
        m_sampleVector.push_back(Sample{static_cast<FixReal>(in[2*i]*SDR_RX_SCALED), static_cast<FixReal>(in[2*i+1]*SDR_RX_SCALED)});
    }
}

WDSPRxSink::WDSPRxSink() :
        m_audioBinaual(false),
        m_dsb(false),
        m_audioMute(false),
        m_agc(12000, m_agcTarget, 1e-2),
        m_agcActive(false),
        m_agcClamping(false),
        m_agcNbSamples(12000),
        m_agcPowerThreshold(1e-2),
        m_agcThresholdGate(0),
        m_squelchDelayLine(2*48000),
        m_audioActive(false),
        m_spectrumSink(nullptr),
        m_spectrumProbe(m_sampleBuffer),
        m_inCount(0),
        m_audioFifo(24000),
        m_audioSampleRate(48000)
{
	m_Bandwidth = 5000;
	m_volume = 2.0;
	m_channelSampleRate = 48000;
	m_channelFrequencyOffset = 0;

	m_audioBuffer.resize(m_audioSampleRate / 10);
	m_audioBufferFill = 0;
	m_undersampleCount = 0;
	m_sum = 0;

    m_demodBuffer.resize(1<<12);
    m_demodBufferFill = 0;

	m_usb = true;
    m_sAvg = 0.0;
    m_sPeak = 0.0;
    m_sCount = 1;

    m_rxa = WDSP::RXA::create_rxa(
        m_wdspSampleRate, // input samplerate
        m_wdspSampleRate, // output samplerate
        m_wdspSampleRate, // sample rate for mainstream dsp processing (dsp)
        m_wdspBufSize     // number complex samples processed per buffer in mainstream dsp processing
    );
    m_rxa->setSpectrumProbe(&m_spectrumProbe);
    WDSP::RXA::SetPassband(*m_rxa, 0, m_Bandwidth);

    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
	applySettings(m_settings, true);
}

WDSPRxSink::~WDSPRxSink()
{
    WDSP::RXA::destroy_rxa(m_rxa);
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

void WDSPRxSink::getMagSqLevels(double& avg, double& peak, int& nbSamples)
{
    avg = m_sAvg;
    peak = m_sPeak;
    nbSamples = m_sCount;
}

void WDSPRxSink::processOneSample(Complex &ci)
{
    m_rxa->get_inbuff()[2*m_inCount] = ci.real() / SDR_RX_SCALED;
    m_rxa->get_inbuff()[2*m_inCount+1] = ci.imag() / SDR_RX_SCALED;

    if (++m_inCount == m_rxa->get_insize())
    {
        WDSP::RXA::xrxa(m_rxa);

        for (int i = 0; i < m_rxa->get_outsize(); i++)
        {
            if (i == 0)
            {
                m_sCount = m_wdspBufSize;
                m_sAvg = WDSP::METER::GetMeter(*m_rxa, WDSP::RXA::RXA_S_AV);
                m_sPeak = WDSP::METER::GetMeter(*m_rxa, WDSP::RXA::RXA_S_PK);
            }

            if (m_audioMute)
            {
                m_audioBuffer[m_audioBufferFill].r = 0;
                m_audioBuffer[m_audioBufferFill].l = 0;
            }
            else
            {
                const double& cr = m_rxa->get_outbuff()[2*i];
                const double& ci = m_rxa->get_outbuff()[2*i+1];
                qint16 zr = cr * 32768.0;
                qint16 zi = ci * 32768.0;
                m_audioBuffer[m_audioBufferFill].r = zr * m_volume;
                m_audioBuffer[m_audioBufferFill].l = zi * m_volume;

                if (m_audioBinaual)
                {
                    m_demodBuffer[m_demodBufferFill++] = zr * m_volume;
                    m_demodBuffer[m_demodBufferFill++] = zi * m_volume;
                }
                else
                {
                    Real demod = (zr + zi) * 0.7;
                    qint16 sample = (qint16)(demod * m_volume);
                    m_demodBuffer[m_demodBufferFill++] = sample;
                }

                if (m_demodBufferFill >= m_demodBuffer.size())
                {
                    QList<ObjectPipe*> dataPipes;
                    MainCore::instance()->getDataPipes().getDataPipes(m_channel, "demod", dataPipes);

                    if (dataPipes.size() > 0)
                    {
                        QList<ObjectPipe*>::iterator it = dataPipes.begin();

                        for (; it != dataPipes.end(); ++it)
                        {
                            DataFifo *fifo = qobject_cast<DataFifo*>((*it)->m_element);

                            if (fifo)
                            {
                                fifo->write(
                                    (quint8*) &m_demodBuffer[0],
                                    m_demodBuffer.size() * sizeof(qint16),
                                    m_audioBinaual ? DataFifo::DataTypeCI16 : DataFifo::DataTypeI16
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
            m_spectrumSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), false);
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

    WDSP::RXA::setOutputSamplerate(m_rxa, sampleRate);

    m_audioFifo.setSize(sampleRate);
    m_audioSampleRate = sampleRate;
    m_audioBuffer.resize(sampleRate / 10);
    m_audioBufferFill = 0;

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_channel, "reportdemod", pipes);

    if (pipes.size() > 0)
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
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_filterIndex: " << settings.m_filterIndex
            << " m_spanLog2: " << settings.m_filterBank[settings.m_filterIndex].m_spanLog2
            << " m_highCutoff: " << settings.m_filterBank[settings.m_filterIndex].m_highCutoff
            << " m_lowCutoff: " << settings.m_filterBank[settings.m_filterIndex].m_lowCutoff
            << " m_fftWindow: " << settings.m_filterBank[settings.m_filterIndex].m_fftWindow << "]"
            << " m_volume: " << settings.m_volume
            << " m_audioBinaual: " << settings.m_audioBinaural
            << " m_audioFlipChannels: " << settings.m_audioFlipChannels
            << " m_dsb: " << settings.m_dsb
            << " m_audioMute: " << settings.m_audioMute
            << " m_agcActive: " << settings.m_agc
            << " m_agcMode: " << settings.m_agcMode
            << " m_agcGain: " << settings.m_agcGain
            << " m_agcSlope: " << settings.m_agcSlope
            << " m_agcHangThreshold: " << settings.m_agcHangThreshold
            << " m_audioDeviceName: " << settings.m_audioDeviceName
            << " m_streamIndex: " << settings.m_streamIndex
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIPort: " << settings.m_reverseAPIPort
            << " m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex
            << " m_reverseAPIChannelIndex: " << settings.m_reverseAPIChannelIndex
            << " force: " << force;

    if((m_settings.m_filterBank[m_settings.m_filterIndex].m_highCutoff != settings.m_filterBank[settings.m_filterIndex].m_highCutoff) ||
        (m_settings.m_filterBank[m_settings.m_filterIndex].m_lowCutoff != settings.m_filterBank[settings.m_filterIndex].m_lowCutoff) ||
        (m_settings.m_filterBank[m_settings.m_filterIndex].m_fftWindow != settings.m_filterBank[settings.m_filterIndex].m_fftWindow) ||
        (m_settings.m_dsb != settings.m_dsb) || force)
    {
        float band, low, high, fLow, fHigh;

        band = settings.m_filterBank[settings.m_filterIndex].m_highCutoff;
        high = band;
        low = settings.m_filterBank[settings.m_filterIndex].m_lowCutoff;

        if (band < 0)
        {
            band = -band;
            m_usb = false;
        }
        else
        {
            m_usb = true;
        }

        m_Bandwidth = band;

        if (high < low)
        {
            if (settings.m_dsb)
            {
                fLow = high;
                fHigh = -high;
            }
            else
            {
                fLow = high;
                fHigh = low;
            }
        }
        else
        {
            if (settings.m_dsb)
            {
                fLow = -high;
                fHigh = high;
            }
            else
            {
                fLow = low;
                fHigh = high;
            }
        }

        Real interpolatorBandwidth = (m_Bandwidth * 1.5f) > m_channelSampleRate ? m_channelSampleRate : (m_Bandwidth * 1.5f);
        m_interpolator.create(16, m_channelSampleRate, interpolatorBandwidth, 2.0f);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) m_audioSampleRate;

        WDSP::RXA::SetPassband(*m_rxa, fLow, fHigh);
        WDSP::NBP::NBPSetWindow(*m_rxa, m_settings.m_filterBank[m_settings.m_filterIndex].m_fftWindow);
    }

    if ((m_settings.m_agc != settings.m_agc)
    || (m_settings.m_agcMode != settings.m_agcMode)
    || (m_settings.m_agcGain != settings.m_agcGain) || force)
    {
        if (settings.m_agc)
        {
            WDSP::WCPAGC::SetAGCMode(*m_rxa, (int) settings.m_agcMode + 1);
            WDSP::WCPAGC::SetAGCFixed(*m_rxa, (double) settings.m_agcGain);
        }
        else
        {
            WDSP::WCPAGC::SetAGCMode(*m_rxa, 0);
            WDSP::WCPAGC::SetAGCTop(*m_rxa, (double) settings.m_agcGain);
        }
    }

    if ((m_settings.m_volume != settings.m_volume) || force)
    {
        m_volume = settings.m_volume;
        m_volume /= 4.0; // for 3276.8
    }

    if ((m_settings.m_audioBinaural != settings.m_audioBinaural) || force) {
        WDSP::PANEL::SetPanelBinaural(*m_rxa, settings.m_audioBinaural ? 1 : 0);
    }

    if ((m_settings.m_audioFlipChannels != settings.m_audioFlipChannels) || force) {
        WDSP::PANEL::SetPanelCopy(*m_rxa, settings.m_audioFlipChannels ? 3 : 0);
    }

    m_audioBinaual = settings.m_audioBinaural;
    m_dsb = settings.m_dsb;
    m_audioMute = settings.m_audioMute;
    m_agcActive = settings.m_agc;
    m_settings = settings;
}
