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

#include "ft8buffer.h"
#include "ft8demodsink.h"

const int FT8DemodSink::m_ssbFftLen = 1024;
const int FT8DemodSink::m_agcTarget = 3276; // 32768/10 -10 dB amplitude => -20 dB power: center of normal signal

FT8DemodSink::LevelRMS::LevelRMS()
{
    m_sum = 0.0f;
    m_peak = 0.0f;
    m_n = 0;
    m_reset = true;
}

void FT8DemodSink::LevelRMS::accumulate(float level)
{
    if (m_reset)
    {
        m_sum = level * level;
        m_peak = std::fabs(level);
        m_n = 1;
        m_reset = false;
    }
    else
    {
        m_sum += level * level;
        m_peak = std::max(m_peak, std::fabs(level));
        m_n++;
    }
}

FT8DemodSink::FT8DemodSink() :
        m_agc(12000, m_agcTarget, 1e-2),
        m_agcActive(false),
        m_audioActive(false),
        m_spectrumSink(nullptr),
        m_ft8Buffer(nullptr),
        m_levelInNbSamples(1200) // 100 ms
{
	m_Bandwidth = 5000;
	m_LowCutoff = 300;
	m_volume = 2.0;
	m_spanLog2 = 3;
	m_channelSampleRate = 48000;
	m_channelFrequencyOffset = 0;

	m_undersampleCount = 0;
	m_sum = 0;

    m_demodBuffer.resize(1<<12);
    m_demodBufferFill = 0;

	m_usb = true;
	m_magsq = 0.0f;
	m_magsqSum = 0.0f;
	m_magsqPeak = 0.0f;
	m_magsqCount = 0;

    m_agc.setThresholdEnable(false); // no squelch
	m_agc.setClamping(false); // no clamping

	SSBFilter = new fftfilt(m_LowCutoff / FT8DemodSettings::m_ft8SampleRate, m_Bandwidth / FT8DemodSettings::m_ft8SampleRate, m_ssbFftLen);

    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
	applySettings(m_settings, true);
}

FT8DemodSink::~FT8DemodSink()
{
    delete SSBFilter;
}

void FT8DemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
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

void FT8DemodSink::processOneSample(Complex &ci)
{
	fftfilt::cmplx *sideband;
	int n_out = 0;
	int decim = 1<<(m_spanLog2 - 1);
	unsigned char decim_mask = decim - 1; // counter LSB bit mask for decimation by 2^(m_scaleLog2 - 1)

    n_out = SSBFilter->runSSB(ci, &sideband, m_usb);

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
            m_sampleBuffer.push_back(Sample(avgr, avgi));
            m_sum.real(0.0);
            m_sum.imag(0.0);
        }

        float agcVal = m_agcActive ? m_agc.feedAndGetValue(sideband[i]) : 0.1;
        fftfilt::cmplx z = sideband[i]*agcVal;
        m_audioActive = z.real() != 0.0;

        Real demod = (z.real() + z.imag()) * 0.7;
        qint16 sample = (qint16)(demod * m_volume);

        if (m_ft8Buffer) {
            m_ft8Buffer->feed(sample);
        }

        m_demodBuffer[m_demodBufferFill++] = sample;
        calculateLevel(sample);

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
                            DataFifo::DataTypeI16
                        );
                    }
                }
            }

            m_demodBufferFill = 0;
        }
    }

	if (m_spectrumSink && (m_sampleBuffer.size() != 0))
    {
		m_spectrumSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), true);
    	m_sampleBuffer.clear();
	}
}

void FT8DemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "FT8DemodSink::applyChannelSettings:"
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
        m_interpolatorDistance = (Real) channelSampleRate / (Real) FT8DemodSettings::m_ft8SampleRate;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void FT8DemodSink::applyFT8SampleRate()
{
    qDebug("FT8DemodSink::applyFT8SampleRate: %d", FT8DemodSettings::m_ft8SampleRate);

    Real interpolatorBandwidth = (m_Bandwidth * 1.5f) > m_channelSampleRate ? m_channelSampleRate : (m_Bandwidth * 1.5f);
    m_interpolator.create(16, m_channelSampleRate, interpolatorBandwidth, 2.0f);
    m_interpolatorDistanceRemain = 0;
    m_interpolatorDistance = (Real) m_channelSampleRate / (Real) FT8DemodSettings::m_ft8SampleRate;
    SSBFilter->create_filter(m_LowCutoff / (float) FT8DemodSettings::m_ft8SampleRate, m_Bandwidth / (float) FT8DemodSettings::m_ft8SampleRate, m_settings.m_filterBank[m_settings.m_filterIndex].m_fftWindow);
    m_levelInNbSamples = FT8DemodSettings::m_ft8SampleRate / 10; // 100 ms

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_channel, "reportdemod", pipes);

    if (pipes.size() > 0)
    {
        for (const auto& pipe : pipes)
        {
            MessageQueue* messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);

            if (messageQueue)
            {
                MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(m_channel, FT8DemodSettings::m_ft8SampleRate);
                messageQueue->push(msg);
            }
        }
    }
}

void FT8DemodSink::applySettings(const FT8DemodSettings& settings, bool force)
{
    qDebug() << "FT8DemodSink::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_filterIndex: " << settings.m_filterIndex
            << " [m_spanLog2: " << settings.m_filterBank[settings.m_filterIndex].m_spanLog2
            << " m_rfBandwidth: " << settings.m_filterBank[settings.m_filterIndex].m_rfBandwidth
            << " m_lowCutoff: " << settings.m_filterBank[settings.m_filterIndex].m_lowCutoff
            << " m_fftWindow: " << settings.m_filterBank[settings.m_filterIndex].m_fftWindow << "]"
            << " m_volume: " << settings.m_volume
            << " m_agcActive: " << settings.m_agc
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
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) FT8DemodSettings::m_ft8SampleRate;
        SSBFilter->create_filter(m_LowCutoff / (float) FT8DemodSettings::m_ft8SampleRate, m_Bandwidth / (float) FT8DemodSettings::m_ft8SampleRate, settings.m_filterBank[settings.m_filterIndex].m_fftWindow);
    }

    if ((m_settings.m_volume != settings.m_volume) || force)
    {
        m_volume = settings.m_volume;
        m_volume /= 4.0; // for 3276.8
    }

    m_spanLog2 = settings.m_filterBank[settings.m_filterIndex].m_spanLog2;
    m_agcActive = settings.m_agc;
    m_settings = settings;
}

void FT8DemodSink::calculateLevel(int16_t& sample)
{
    if (m_levelIn.m_n >= m_levelInNbSamples)
    {
        m_rmsLevel = sqrt(m_levelIn.m_sum / m_levelInNbSamples);
        m_peakLevel = m_levelIn.m_peak;
        m_levelIn.m_reset = true;
    }

    m_levelIn.accumulate(sample/29491.2f); // scale on 90% (0.9 * 32768.0)
}
