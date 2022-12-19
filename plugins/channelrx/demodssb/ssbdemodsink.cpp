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

#include <stdio.h>

#include <QTime>
#include <QDebug>

#include "audio/audiooutputdevice.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplemimo.h"
#include "dsp/spectrumvis.h"
#include "dsp/datafifo.h"
#include "device/deviceapi.h"
#include "util/db.h"
#include "util/messagequeue.h"
#include "maincore.h"

#include "ssbdemodsink.h"

const int SSBDemodSink::m_ssbFftLen = 1024;
const int SSBDemodSink::m_agcTarget = 3276; // 32768/10 -10 dB amplitude => -20 dB power: center of normal signal

SSBDemodSink::SSBDemodSink() :
        m_audioBinaual(false),
        m_audioFlipChannels(false),
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
        m_audioFifo(24000),
        m_audioSampleRate(48000)
{
	m_Bandwidth = 5000;
	m_LowCutoff = 300;
	m_volume = 2.0;
	m_spanLog2 = 3;
	m_channelSampleRate = 48000;
	m_channelFrequencyOffset = 0;

	m_audioBuffer.resize(1<<14);
	m_audioBufferFill = 0;
	m_undersampleCount = 0;
	m_sum = 0;

    m_demodBuffer.resize(1<<12);
    m_demodBufferFill = 0;

	m_usb = true;
	m_magsq = 0.0f;
	m_magsqSum = 0.0f;
	m_magsqPeak = 0.0f;
	m_magsqCount = 0;

	m_agc.setClampMax(SDR_RX_SCALED/100.0);
	m_agc.setClamping(m_agcClamping);

	SSBFilter = new fftfilt(m_LowCutoff / m_audioSampleRate, m_Bandwidth / m_audioSampleRate, m_ssbFftLen);
	DSBFilter = new fftfilt((2.0f * m_Bandwidth) / m_audioSampleRate, 2 * m_ssbFftLen);

    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
	applySettings(m_settings, true);
}

SSBDemodSink::~SSBDemodSink()
{
    delete SSBFilter;
    delete DSBFilter;
}

void SSBDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
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

void SSBDemodSink::processOneSample(Complex &ci)
{
	fftfilt::cmplx *sideband;
	int n_out = 0;
	int decim = 1<<(m_spanLog2 - 1);
	unsigned char decim_mask = decim - 1; // counter LSB bit mask for decimation by 2^(m_scaleLog2 - 1)

    if (m_dsb) {
        n_out = DSBFilter->runDSB(ci, &sideband);
    } else {
        n_out = SSBFilter->runSSB(ci, &sideband, m_usb);
    }

    for (int i = 0; i < n_out; i++)
    {
        // Downsample by 2^(m_scaleLog2 - 1) for SSB band spectrum display
        // smart decimation with bit gain using float arithmetic (23 bits significand)

        m_sum += sideband[i];

        if (!(m_undersampleCount++ & decim_mask))
        {
            Real avgr = m_sum.real() / decim;
            Real avgi = m_sum.imag() / decim;
            m_magsq = (avgr * avgr + avgi * avgi) / (SDR_RX_SCALED*SDR_RX_SCALED);

            m_magsqSum += m_magsq;

            if (m_magsq > m_magsqPeak)
            {
                m_magsqPeak = m_magsq;
            }

            m_magsqCount++;

            if (!m_dsb & !m_usb)
            { // invert spectrum for LSB
                m_sampleBuffer.push_back(Sample(avgi, avgr));
            }
            else
            {
                m_sampleBuffer.push_back(Sample(avgr, avgi));
            }

            m_sum.real(0.0);
            m_sum.imag(0.0);
        }

        float agcVal = m_agcActive ? m_agc.feedAndGetValue(sideband[i]) : 0.1;
        fftfilt::cmplx& delayedSample = m_squelchDelayLine.readBack(m_agc.getStepDownDelay());
        m_audioActive = delayedSample.real() != 0.0;
        m_squelchDelayLine.write(sideband[i]*agcVal);

        if (m_audioMute)
        {
            m_audioBuffer[m_audioBufferFill].r = 0;
            m_audioBuffer[m_audioBufferFill].l = 0;
        }
        else
        {
            fftfilt::cmplx z = m_agcActive ? delayedSample * m_agc.getStepValue() : delayedSample;

            if (m_audioBinaual)
            {
                if (m_audioFlipChannels)
                {
                    m_audioBuffer[m_audioBufferFill].r = (qint16)(z.imag() * m_volume);
                    m_audioBuffer[m_audioBufferFill].l = (qint16)(z.real() * m_volume);
                }
                else
                {
                    m_audioBuffer[m_audioBufferFill].r = (qint16)(z.real() * m_volume);
                    m_audioBuffer[m_audioBufferFill].l = (qint16)(z.imag() * m_volume);
                }

                m_demodBuffer[m_demodBufferFill++] = z.real();
                m_demodBuffer[m_demodBufferFill++] = z.imag();
            }
            else
            {
                Real demod = (z.real() + z.imag()) * 0.7;
                qint16 sample = (qint16)(demod * m_volume);
                m_audioBuffer[m_audioBufferFill].l = sample;
                m_audioBuffer[m_audioBufferFill].r = sample;
                m_demodBuffer[m_demodBufferFill++] = (z.real() + z.imag()) * 0.7;
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
        }

        ++m_audioBufferFill;

        if (m_audioBufferFill >= m_audioBuffer.size())
        {
            uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill);

            if (res != m_audioBufferFill) {
                qDebug("SSBDemodSink::processOneSample: %u/%u samples written", res, m_audioBufferFill);
            }

            m_audioBufferFill = 0;
        }
    }

	if (m_spectrumSink && (m_sampleBuffer.size() != 0))
    {
		m_spectrumSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), !m_dsb);
    	m_sampleBuffer.clear();
	}
}

void SSBDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "SSBDemodSink::applyChannelSettings:"
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
        m_interpolatorDistance = (Real) channelSampleRate / (Real) m_audioSampleRate;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void SSBDemodSink::applyAudioSampleRate(int sampleRate)
{
    qDebug("SSBDemodSink::applyAudioSampleRate: %d", sampleRate);

    Real interpolatorBandwidth = (m_Bandwidth * 1.5f) > m_channelSampleRate ? m_channelSampleRate : (m_Bandwidth * 1.5f);
    m_interpolator.create(16, m_channelSampleRate, interpolatorBandwidth, 2.0f);
    m_interpolatorDistanceRemain = 0;
    m_interpolatorDistance = (Real) m_channelSampleRate / (Real) sampleRate;

    SSBFilter->create_filter(m_LowCutoff / (float) sampleRate, m_Bandwidth / (float) sampleRate, m_settings.m_filterBank[m_settings.m_filterIndex].m_fftWindow);
    DSBFilter->create_dsb_filter(m_Bandwidth / (float) sampleRate, m_settings.m_filterBank[m_settings.m_filterIndex].m_fftWindow);

    int agcNbSamples = (sampleRate / 1000) * (1<<m_settings.m_agcTimeLog2);
    int agcThresholdGate = (sampleRate / 1000) * m_settings.m_agcThresholdGate; // ms

    if (m_agcNbSamples != agcNbSamples)
    {
        m_agc.resize(agcNbSamples, agcNbSamples/2, m_agcTarget);
        m_agc.setStepDownDelay(agcNbSamples);
        m_agcNbSamples = agcNbSamples;
    }

    if (m_agcThresholdGate != agcThresholdGate)
    {
        m_agc.setGate(agcThresholdGate);
        m_agcThresholdGate = agcThresholdGate;
    }

    m_audioFifo.setSize(sampleRate);
    m_audioSampleRate = sampleRate;


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

void SSBDemodSink::applySettings(const SSBDemodSettings& settings, bool force)
{
    qDebug() << "SSBDemodSink::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_filterIndex: " << settings.m_filterIndex
            << " [m_spanLog2: " << settings.m_filterBank[settings.m_filterIndex].m_spanLog2
            << " m_rfBandwidth: " << settings.m_filterBank[settings.m_filterIndex].m_rfBandwidth
            << " m_lowCutoff: " << settings.m_filterBank[settings.m_filterIndex].m_lowCutoff
            << " m_fftWindow: " << settings.m_filterBank[settings.m_filterIndex].m_fftWindow << "]"
            << " m_volume: " << settings.m_volume
            << " m_audioBinaual: " << settings.m_audioBinaural
            << " m_audioFlipChannels: " << settings.m_audioFlipChannels
            << " m_dsb: " << settings.m_dsb
            << " m_audioMute: " << settings.m_audioMute
            << " m_agcActive: " << settings.m_agc
            << " m_agcClamping: " << settings.m_agcClamping
            << " m_agcTimeLog2: " << settings.m_agcTimeLog2
            << " agcPowerThreshold: " << settings.m_agcPowerThreshold
            << " agcThresholdGate: " << settings.m_agcThresholdGate
            << " m_audioDeviceName: " << settings.m_audioDeviceName
            << " m_streamIndex: " << settings.m_streamIndex
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIPort: " << settings.m_reverseAPIPort
            << " m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex
            << " m_reverseAPIChannelIndex: " << settings.m_reverseAPIChannelIndex
            << " force: " << force;

    if((m_settings.m_filterBank[m_settings.m_filterIndex].m_rfBandwidth != settings.m_filterBank[settings.m_filterIndex].m_rfBandwidth) ||
        (m_settings.m_filterBank[m_settings.m_filterIndex].m_lowCutoff != settings.m_filterBank[settings.m_filterIndex].m_lowCutoff) ||
        (m_settings.m_filterBank[m_settings.m_filterIndex].m_fftWindow != settings.m_filterBank[settings.m_filterIndex].m_fftWindow) || force)
    {
        float band, lowCutoff;

        band = settings.m_filterBank[settings.m_filterIndex].m_rfBandwidth;
        lowCutoff = settings.m_filterBank[settings.m_filterIndex].m_lowCutoff;

        if (band < 0) {
            band = -band;
            lowCutoff = -lowCutoff;
            m_usb = false;
        } else {
            m_usb = true;
        }

        if (band < 100.0f)
        {
            band = 100.0f;
            lowCutoff = 0;
        }

        m_Bandwidth = band;
        m_LowCutoff = lowCutoff;

        Real interpolatorBandwidth = (m_Bandwidth * 1.5f) > m_channelSampleRate ? m_channelSampleRate : (m_Bandwidth * 1.5f);
        m_interpolator.create(16, m_channelSampleRate, interpolatorBandwidth, 2.0f);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) m_audioSampleRate;
        SSBFilter->create_filter(m_LowCutoff / (float) m_audioSampleRate, m_Bandwidth / (float) m_audioSampleRate, settings.m_filterBank[settings.m_filterIndex].m_fftWindow);
        DSBFilter->create_dsb_filter(m_Bandwidth / (float) m_audioSampleRate, settings.m_filterBank[settings.m_filterIndex].m_fftWindow);
    }

    if ((m_settings.m_volume != settings.m_volume) || force)
    {
        m_volume = settings.m_volume;
        m_volume /= 4.0; // for 3276.8
    }

    if ((m_settings.m_agcTimeLog2 != settings.m_agcTimeLog2) ||
        (m_settings.m_agcPowerThreshold != settings.m_agcPowerThreshold) ||
        (m_settings.m_agcThresholdGate != settings.m_agcThresholdGate) ||
        (m_settings.m_agcClamping != settings.m_agcClamping) || force)
    {
        int agcNbSamples = (m_audioSampleRate / 1000) * (1<<settings.m_agcTimeLog2);
        m_agc.setThresholdEnable(settings.m_agcPowerThreshold != -SSBDemodSettings::m_minPowerThresholdDB);
        double agcPowerThreshold = CalcDb::powerFromdB(settings.m_agcPowerThreshold) * (SDR_RX_SCALED*SDR_RX_SCALED);
        int agcThresholdGate = (m_audioSampleRate / 1000) * settings.m_agcThresholdGate; // ms
        bool agcClamping = settings.m_agcClamping;

        if (m_agcNbSamples != agcNbSamples)
        {
            m_agc.resize(agcNbSamples, agcNbSamples/2, m_agcTarget);
            m_agc.setStepDownDelay(agcNbSamples);
            m_agcNbSamples = agcNbSamples;
        }

        if (m_agcPowerThreshold != agcPowerThreshold)
        {
            m_agc.setThreshold(agcPowerThreshold);
            m_agcPowerThreshold = agcPowerThreshold;
        }

        if (m_agcThresholdGate != agcThresholdGate)
        {
            m_agc.setGate(agcThresholdGate);
            m_agcThresholdGate = agcThresholdGate;
        }

        if (m_agcClamping != agcClamping)
        {
            m_agc.setClamping(agcClamping);
            m_agcClamping = agcClamping;
        }

        qDebug() << "SBDemodSink::applySettings: AGC:"
            << " agcNbSamples: " << agcNbSamples
            << " agcPowerThreshold: " << agcPowerThreshold
            << " agcThresholdGate: " << agcThresholdGate
            << " agcClamping: " << agcClamping;
    }

    m_spanLog2 = settings.m_filterBank[settings.m_filterIndex].m_spanLog2;
    m_audioBinaual = settings.m_audioBinaural;
    m_audioFlipChannels = settings.m_audioFlipChannels;
    m_dsb = settings.m_dsb;
    m_audioMute = settings.m_audioMute;
    m_agcActive = settings.m_agc;
    m_settings = settings;
}

