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

#include <QDebug>

#include <complex.h>

#include "audio/audiooutputdevice.h"
#include "dsp/fftfilt.h"
#include "dsp/datafifo.h"
#include "util/db.h"
#include "util/stepfunctions.h"
#include "util/messagequeue.h"
#include "maincore.h"

#include "amdemodsink.h"

AMDemodSink::AMDemodSink() :
        m_channelSampleRate(48000),
        m_channelFrequencyOffset(0),
        m_audioSampleRate(48000),
        m_squelchCount(0),
        m_squelchOpen(false),
        m_squelchDelayLine(9600),
        m_magsqSum(0.0f),
        m_magsqPeak(0.0f),
        m_magsqCount(0),
        m_volumeAGC(0.003),
        m_syncAMAGC(12000, 0.1, 1e-2),
        m_audioFifo(48000)
{
	m_audioBuffer.resize(1<<14);
	m_audioBufferFill = 0;
    m_demodBuffer.resize(1<<12);
    m_demodBufferFill = 0;

	m_magsq = 0.0;

    DSBFilter = new fftfilt((2.0f * m_settings.m_rfBandwidth) / m_audioSampleRate, 2 * 1024);
    SSBFilter = new fftfilt(0.0f, m_settings.m_rfBandwidth / m_audioSampleRate, 1024);
    m_syncAMAGC.setThresholdEnable(false);
    m_syncAMAGC.resize(12000, 6000, 0.1);

    m_pllFilt.create(101, m_audioSampleRate, 200.0);
    m_pll.computeCoefficients(0.05, 0.707, 1000);
    m_syncAMBuffIndex = 0;

	applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

AMDemodSink::~AMDemodSink()
{
    delete DSBFilter;
    delete SSBFilter;
}

void AMDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
	Complex ci;

	for (SampleVector::const_iterator it = begin; it != end; ++it)
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
		else // decimate
		{
	        if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
	        {
	            processOneSample(ci);
	            m_interpolatorDistanceRemain += m_interpolatorDistance;
	        }
		}
	}
}

void AMDemodSink::processOneSample(Complex &ci)
{
    Real re = ci.real() / SDR_RX_SCALEF;
    Real im = ci.imag() / SDR_RX_SCALEF;
    Real magsq = re*re + im*im;
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();
    m_magsqSum += magsq;

    if (magsq > m_magsqPeak)
    {
        m_magsqPeak = magsq;
    }

    m_magsqCount++;

    m_squelchDelayLine.write(magsq);

    if (m_magsq < m_squelchLevel)
    {
        if (m_squelchCount > 0) {
            m_squelchCount--;
        }
    }
    else
    {
        if (m_squelchCount < m_audioSampleRate / 10) {
            m_squelchCount++;
        }
    }

    qint16 sample;

    m_squelchOpen = (m_squelchCount >= m_audioSampleRate / 20);

    if (m_squelchOpen && !m_settings.m_audioMute)
    {
        Real demod;

        if (m_settings.m_pll)
        {
            std::complex<float> s(re, im);
            s = m_pllFilt.filter(s);
            m_pll.feed(s.real(), s.imag());
            float yr = re * m_pll.getImag() - im * m_pll.getReal();
            float yi = re * m_pll.getReal() + im * m_pll.getImag();

            fftfilt::cmplx *sideband;
            std::complex<float> cs(yr, yi);
            int n_out;

            if (m_settings.m_syncAMOperation == AMDemodSettings::SyncAMDSB) {
                n_out = DSBFilter->runDSB(cs, &sideband, false);
            } else {
                n_out = SSBFilter->runSSB(cs, &sideband, m_settings.m_syncAMOperation == AMDemodSettings::SyncAMUSB, false);
            }

            for (int i = 0; i < n_out; i++)
            {
                float agcVal = m_syncAMAGC.feedAndGetValue(sideband[i]);
                fftfilt::cmplx z = sideband[i] * agcVal; // * m_syncAMAGC.getStepValue();

                if (m_settings.m_syncAMOperation == AMDemodSettings::SyncAMDSB) {
                    m_syncAMBuff[i] = (z.real() + z.imag());
                } else if (m_settings.m_syncAMOperation == AMDemodSettings::SyncAMUSB) {
                    m_syncAMBuff[i] = (z.real() + z.imag());
                } else {
                    m_syncAMBuff[i] = (z.real() + z.imag());
                }

                m_syncAMBuffIndex = 0;
            }

            m_syncAMBuffIndex = m_syncAMBuffIndex < 2*1024 ? m_syncAMBuffIndex : 0;
            demod = m_syncAMBuff[m_syncAMBuffIndex++]*4.0f; // mos pifometrico
        }
        else
        {
            demod = sqrt(m_squelchDelayLine.readBack(m_audioSampleRate/20));
            m_volumeAGC.feed(demod);
            demod = (demod - m_volumeAGC.getValue()) / m_volumeAGC.getValue();
        }

        if (m_settings.m_bandpassEnable)
        {
            demod = m_bandpass.filter(demod);
        }
        else
        {
            demod = m_lowpass.filter(demod);
        }

        Real attack = (m_squelchCount - 0.05f * m_audioSampleRate) / (0.05f * m_audioSampleRate);
        sample = demod * StepFunctions::smootherstep(attack) * (m_audioSampleRate/24) * m_settings.m_volume;
    }
    else
    {
        sample = 0;
    }

    m_audioBuffer[m_audioBufferFill].l = sample;
    m_audioBuffer[m_audioBufferFill].r = sample;
    ++m_audioBufferFill;

    if (m_audioBufferFill >= m_audioBuffer.size())
    {
        std::size_t res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], std::min(m_audioBufferFill, m_audioBuffer.size()));

        if (res != m_audioBufferFill)
        {
            qDebug("AMDemodSink::processOneSample: %lu/%lu audio samples written", res, m_audioBufferFill);
            m_audioFifo.clear();
        }

        m_audioBufferFill = 0;
    }

    m_demodBuffer[m_demodBufferFill] = sample;
    ++m_demodBufferFill;

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

                if (fifo) {
                    fifo->write((quint8*) &m_demodBuffer[0], m_demodBuffer.size() * sizeof(qint16), DataFifo::DataTypeI16);
                }
            }
        }

        m_demodBufferFill = 0;
    }
}

void AMDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "AMDemodSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset
            << " m_audioSampleRate: " << m_audioSampleRate;

    if ((m_channelFrequencyOffset != channelFrequencyOffset) ||
        (m_channelSampleRate != channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((m_channelSampleRate != channelSampleRate) || force)
    {
        m_interpolator.create(16, channelSampleRate, m_settings.m_rfBandwidth / 2.2f);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) channelSampleRate / (Real) m_audioSampleRate;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void AMDemodSink::applySettings(const AMDemodSettings& settings, bool force)
{
    qDebug() << "AMDemodSink::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_volume: " << settings.m_volume
            << " m_squelch: " << settings.m_squelch
            << " m_audioMute: " << settings.m_audioMute
            << " m_bandpassEnable: " << settings.m_bandpassEnable
            << " m_audioDeviceName: " << settings.m_audioDeviceName
            << " m_pll: " << settings.m_pll
            << " m_syncAMOperation: " << (int) settings.m_syncAMOperation
            << " force: " << force;

    if((m_settings.m_rfBandwidth != settings.m_rfBandwidth) ||
        (m_settings.m_bandpassEnable != settings.m_bandpassEnable) || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_rfBandwidth / 2.2f);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) m_audioSampleRate;
        m_bandpass.create(301, m_audioSampleRate, 300.0, settings.m_rfBandwidth / 2.0f);
        m_lowpass.create(301, m_audioSampleRate,  settings.m_rfBandwidth / 2.0f);
        DSBFilter->create_dsb_filter((2.0f * settings.m_rfBandwidth) / (float) m_audioSampleRate);
    }

    if ((m_settings.m_squelch != settings.m_squelch) || force) {
        m_squelchLevel = CalcDb::powerFromdB(settings.m_squelch);
    }

    if ((m_settings.m_pll != settings.m_pll) || force)
    {
        if (settings.m_pll)
        {
            m_volumeAGC.resizeNew(m_audioSampleRate/4, 0.003);
            m_syncAMBuffIndex = 0;
        }
        else
        {
            m_volumeAGC.resizeNew(m_audioSampleRate/10, 0.003);
        }
    }

    if ((m_settings.m_syncAMOperation != settings.m_syncAMOperation) || force) {
        m_syncAMBuffIndex = 0;
    }

    m_settings = settings;
}

void AMDemodSink::applyAudioSampleRate(int sampleRate)
{
    if (sampleRate < 0)
    {
        qWarning("AMDemodSink::applyAudioSampleRate: invalid sample rate: %d", sampleRate);
        return;
    }

    qDebug("AMDemodSink::applyAudioSampleRate: sampleRate: %d m_channelSampleRate: %d", sampleRate, m_channelSampleRate);

    m_interpolator.create(16, m_channelSampleRate, m_settings.m_rfBandwidth / 2.2f);
    m_interpolatorDistanceRemain = 0;
    m_interpolatorDistance = (Real) m_channelSampleRate / (Real) sampleRate;
    m_bandpass.create(301, sampleRate, 300.0, m_settings.m_rfBandwidth / 2.0f);
    m_lowpass.create(301, sampleRate,  m_settings.m_rfBandwidth / 2.0f);
    m_audioFifo.setSize(sampleRate);
    m_squelchDelayLine.resize(sampleRate/5);
    DSBFilter->create_dsb_filter((2.0f * m_settings.m_rfBandwidth) / (float) sampleRate);
    m_pllFilt.create(101, sampleRate, 200.0);

    if (m_settings.m_pll) {
        m_volumeAGC.resizeNew(sampleRate, 0.003);
    } else {
        m_volumeAGC.resizeNew(sampleRate/10, 0.003);
    }

    m_syncAMAGC.resize(sampleRate/4, sampleRate/8, 0.1);
    m_pll.setSampleRate(sampleRate);

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_channel, "reportdemod", pipes);

    if (pipes.size() > 0)
    {
        for (const auto& pipe : pipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(m_channel, sampleRate);
            messageQueue->push(msg);
        }
    }

    m_audioSampleRate = sampleRate;
}
