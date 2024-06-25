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
                float avgr = m_sum.real() / decim;
                float avgi = m_sum.imag() / decim;

                if (!m_dsb & !m_usb)
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
	m_volume = 2.0;
	m_channelSampleRate = 48000;
	m_channelFrequencyOffset = 0;

	m_audioBuffer.resize(m_audioSampleRate / 10);
	m_audioBufferFill = 0;
	m_undersampleCount = 0;

    m_demodBuffer.resize(1<<12);
    m_demodBufferFill = 0;

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
    m_rxa->get_inbuff()[2*m_inCount] = ci.imag() / SDR_RX_SCALEF;
    m_rxa->get_inbuff()[2*m_inCount+1] = ci.real() / SDR_RX_SCALEF;

    if (++m_inCount == m_rxa->get_insize())
    {
        WDSP::RXA::xrxa(m_rxa);

        m_sCount = m_wdspBufSize;
        m_sAvg = WDSP::METER::GetMeter(*m_rxa, WDSP::RXA::RXA_S_AV);
        m_sPeak = WDSP::METER::GetMeter(*m_rxa, WDSP::RXA::RXA_S_PK);

        for (int i = 0; i < m_rxa->get_outsize(); i++)
        {
            if (m_settings.m_audioMute)
            {
                m_audioBuffer[m_audioBufferFill].r = 0;
                m_audioBuffer[m_audioBufferFill].l = 0;
            }
            else
            {
                const double& cr = m_rxa->get_outbuff()[2*i+1];
                const double& ci = m_rxa->get_outbuff()[2*i];
                qint16 zr = cr * 32768.0;
                qint16 zi = ci * 32768.0;
                m_audioBuffer[m_audioBufferFill].r = zr * m_volume;
                m_audioBuffer[m_audioBufferFill].l = zi * m_volume;

                if (m_settings.m_audioBinaural)
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
            << " m_audioBinaural: " << settings.m_audioBinaural
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
            m_spectrumProbe.setUSB(false);
        }
        else
        {
            m_spectrumProbe.setUSB(true);
        }

        m_Bandwidth = band;

        if (high < low)
        {
            if (settings.m_dsb)
            {
                fLow = high;
                fHigh = -high;
                m_spectrumProbe.setDSB(true);
            }
            else
            {
                fLow = high;
                fHigh = low;
                m_spectrumProbe.setDSB(false);
            }
        }
        else
        {
            if (settings.m_dsb)
            {
                fLow = -high;
                fHigh = high;
                m_spectrumProbe.setDSB(true);
            }
            else
            {
                fLow = low;
                fHigh = high;
                m_spectrumProbe.setDSB(false);
            }
        }

        Real interpolatorBandwidth = (m_Bandwidth * 1.5f) > m_channelSampleRate ? m_channelSampleRate : (m_Bandwidth * 1.5f);
        m_interpolator.create(16, m_channelSampleRate, interpolatorBandwidth, 2.0f);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) m_audioSampleRate;

        WDSP::RXA::SetPassband(*m_rxa, fLow, fHigh);
        WDSP::NBP::NBPSetWindow(*m_rxa, m_settings.m_filterBank[m_settings.m_filterIndex].m_fftWindow);
    }

    if ((m_settings.m_filterBank[settings.m_filterIndex].m_spanLog2 != settings.m_filterBank[settings.m_filterIndex].m_spanLog2) || force) {
        m_spectrumProbe.setSpanLog2(settings.m_filterBank[settings.m_filterIndex].m_spanLog2);
    }

    if ((m_settings.m_agc != settings.m_agc)
    || (m_settings.m_agcMode != settings.m_agcMode)
    || (m_settings.m_agcSlope != settings.m_agcSlope)
    || (m_settings.m_agcHangThreshold != settings.m_agcHangThreshold)
    || (m_settings.m_agcGain != settings.m_agcGain) || force)
    {
        WDSP::WCPAGC::SetAGCMode(*m_rxa, settings.m_agc ? (int) settings.m_agcMode + 1 : 0);  // SetRXAAGCMode(id, agc);
        WDSP::WCPAGC::SetAGCSlope(*m_rxa, settings.m_agcSlope); // SetRXAAGCSlope(id, rx->agc_slope);
        WDSP::WCPAGC::SetAGCTop(*m_rxa, (float) settings.m_agcGain); // SetRXAAGCTop(id, rx->agc_gain);

        if (settings.m_agc)
        {
            switch (settings.m_agcMode)
            {
            case WDSPRxSettings::AGCMode::AGCLong:
                WDSP::WCPAGC::SetAGCAttack(*m_rxa, 2);   // SetRXAAGCAttack(id, 2);
                WDSP::WCPAGC::SetAGCHang(*m_rxa, 2000);  // SetRXAAGCHang(id, 2000);
                WDSP::WCPAGC::SetAGCDecay(*m_rxa, 2000); // SetRXAAGCDecay(id, 2000);
                WDSP::WCPAGC::SetAGCHangThreshold(*m_rxa, settings.m_agcHangThreshold); // SetRXAAGCHangThreshold(id, (int)rx->agc_hang_threshold);
                break;
            case WDSPRxSettings::AGCMode::AGCSlow:
                WDSP::WCPAGC::SetAGCAttack(*m_rxa, 2);   // SetRXAAGCAttack(id, 2);
                WDSP::WCPAGC::SetAGCHang(*m_rxa, 1000);  // SetRXAAGCHang(id, 1000);
                WDSP::WCPAGC::SetAGCDecay(*m_rxa, 500);  // SetRXAAGCDecay(id, 500);
                WDSP::WCPAGC::SetAGCHangThreshold(*m_rxa, settings.m_agcHangThreshold); // SetRXAAGCHangThreshold(id, (int)rx->agc_hang_threshold);
                break;
            case WDSPRxSettings::AGCMode::AGCMedium:
                WDSP::WCPAGC::SetAGCAttack(*m_rxa, 2);   // SetRXAAGCAttack(id, 2);
                WDSP::WCPAGC::SetAGCHang(*m_rxa, 0);     // SetRXAAGCHang(id, 0);
                WDSP::WCPAGC::SetAGCDecay(*m_rxa, 250);  // SetRXAAGCDecay(id, 250);
                WDSP::WCPAGC::SetAGCHangThreshold(*m_rxa, settings.m_agcHangThreshold); // SetRXAAGCHangThreshold(id, 100);
                break;
            case WDSPRxSettings::AGCMode::AGCFast:
                WDSP::WCPAGC::SetAGCAttack(*m_rxa, 2);   // SetRXAAGCAttack(id, 2);
                WDSP::WCPAGC::SetAGCHang(*m_rxa, 0);     // SetRXAAGCHang(id, 0);
                WDSP::WCPAGC::SetAGCDecay(*m_rxa, 50);   // SetRXAAGCDecay(id, 50);
                WDSP::WCPAGC::SetAGCHangThreshold(*m_rxa, settings.m_agcHangThreshold); // SetRXAAGCHangThreshold(id, 100);
                break;
            }

            WDSP::WCPAGC::SetAGCFixed(*m_rxa, 60.0f); // default
        }
        else
        {
            WDSP::WCPAGC::SetAGCFixed(*m_rxa, (float) settings.m_agcGain);
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

    m_settings = settings;
}
