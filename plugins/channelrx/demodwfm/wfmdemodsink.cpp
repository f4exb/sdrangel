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
#include <complex.h>

#include <QTime>
#include <QDebug>

#include "audio/audiooutputdevice.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplemimo.h"
#include "dsp/datafifo.h"
#include "util/db.h"
#include "util/messagequeue.h"
#include "maincore.h"

#include "wfmdemodsink.h"

const unsigned int WFMDemodSink::m_rfFilterFftLength = 1024;

WFMDemodSink::WFMDemodSink() :
    m_channelSampleRate(384000),
    m_channelFrequencyOffset(0),
    m_audioSampleRate(48000),
    m_squelchState(0),
    m_squelchOpen(false),
    m_magsq(0.0f),
    m_magsqSum(0.0f),
    m_magsqPeak(0.0f),
    m_magsqCount(0),
    m_audioFifo(250000)
{
	m_rfFilter = new fftfilt(-50000.0 / 384000.0, 50000.0 / 384000.0, m_rfFilterFftLength);
	m_phaseDiscri.setFMScaling(384000/75000);

	m_audioBuffer.resize(1<<14);
	m_audioBufferFill = 0;

    m_demodBuffer.resize(1<<12);
    m_demodBufferFill = 0;

	applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

WFMDemodSink::~WFMDemodSink()
{
    delete m_rfFilter;
}

void WFMDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
	Complex ci;
	fftfilt::cmplx *rf;
	int rf_out;
	Real demod;
	double msq;
	float fmDev;

	for (SampleVector::const_iterator it = begin; it != end; ++it)
	{
		Complex c(it->real(), it->imag());
		c *= m_nco.nextIQ();

		rf_out = m_rfFilter->runFilt(c, &rf); // filter RF before demod

		for (int i = 0 ; i < rf_out; i++)
		{
		    msq = rf[i].real()*rf[i].real() + rf[i].imag()*rf[i].imag();
		    Real magsq = msq / (SDR_RX_SCALED*SDR_RX_SCALED);
		    m_magsqSum += magsq;
		    m_movingAverage(magsq);

            if (magsq > m_magsqPeak) {
                m_magsqPeak = magsq;
            }

            m_magsqCount++;

            if (magsq >= m_squelchLevel)
            {
                if (m_squelchState < m_settings.m_rfBandwidth / 10) { // twice attack and decay rate
                    m_squelchState++;
                }
            }
            else
            {
                if (m_squelchState > 0) {
                    m_squelchState--;
                }
            }

			m_squelchOpen = (m_squelchState > (m_settings.m_rfBandwidth / 20));

			if (m_squelchOpen && !m_settings.m_audioMute) { // squelch open and not mute
                demod = m_phaseDiscri.phaseDiscriminatorDelta(rf[i], msq, fmDev);
            } else {
                demod = 0;
            }

            Complex e(demod, 0);

			if (m_interpolator.decimate(&m_interpolatorDistanceRemain, e, &ci))
			{
				qint16 sample = (qint16)(ci.real() * 3276.8f * m_settings.m_volume);
				m_audioBuffer[m_audioBufferFill].l = sample;
				m_audioBuffer[m_audioBufferFill].r = sample;

				++m_audioBufferFill;

				if(m_audioBufferFill >= m_audioBuffer.size())
				{
					std::size_t res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], std::min(m_audioBufferFill, m_audioBuffer.size()));

					if (res != m_audioBufferFill) {
						qDebug("WFMDemodSink::feed: %lu/%lu audio samples written", res, m_audioBufferFill);
					}

					m_audioBufferFill = 0;
				}

				m_interpolatorDistanceRemain += m_interpolatorDistance;
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
		}
	}
}

void WFMDemodSink::applyAudioSampleRate(int sampleRate)
{
    if (sampleRate < 0)
    {
        qWarning("WFMDemodSink::applyAudioSampleRate: invalid sample rate: %d", sampleRate);
        return;
    }

    qDebug("WFMDemodSink::applyAudioSampleRate: %u", sampleRate);

    m_interpolator.create(16, m_channelSampleRate, m_settings.m_afBandwidth);
    m_interpolatorDistanceRemain = (Real) m_channelSampleRate / sampleRate;
    m_interpolatorDistance =  (Real) m_channelSampleRate / (Real) sampleRate;
    m_audioSampleRate = sampleRate;

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_channel, "reportdemod", pipes);

    if (pipes.size() > 0)
    {
        for (const auto& pipe : pipes)
        {
            MessageQueue* messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(m_channel, sampleRate);
            messageQueue->push(msg);
        }
    }
}

void WFMDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "WFMDemodSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if((channelFrequencyOffset != m_channelFrequencyOffset) ||
        (channelSampleRate != m_channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((channelSampleRate != m_channelSampleRate) || force)
    {
        qDebug() << "WFMDemod::applyChannelSettings: m_interpolator.create";
        m_interpolator.create(16, channelSampleRate, m_settings.m_afBandwidth);
        m_interpolatorDistanceRemain = (Real) channelSampleRate / (Real) m_audioSampleRate;
        m_interpolatorDistance =  (Real) channelSampleRate / (Real) m_audioSampleRate;
        qDebug() << "WFMDemod::applySettings: m_rfFilter->create_filter";
        Real lowCut = -(m_settings.m_rfBandwidth / 2.0) / channelSampleRate;
        Real hiCut  = (m_settings.m_rfBandwidth / 2.0) / channelSampleRate;
        m_rfFilter->create_filter(lowCut, hiCut);
        //m_fmExcursion = m_settings.m_rfBandwidth / (Real) channelSampleRate;
        m_phaseDiscri.setFMScaling((float) channelSampleRate / ((float) 2*m_fmExcursion));
        qDebug("WFMDemod::applySettings: m_fmExcursion: %f", m_fmExcursion);
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void WFMDemodSink::applySettings(const WFMDemodSettings& settings, bool force)
{
    qDebug() << "WFMDemodSink::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_afBandwidth: " << settings.m_afBandwidth
            << " m_volume: " << settings.m_volume
            << " m_squelch: " << settings.m_squelch
            << " m_audioDeviceName: " << settings.m_audioDeviceName
            << " m_audioMute: " << settings.m_audioMute
            << " m_streamIndex: " << settings.m_streamIndex
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIPort: " << settings.m_reverseAPIPort
            << " m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex
            << " m_reverseAPIChannelIndex: " << settings.m_reverseAPIChannelIndex
            << " force: " << force;

    if((settings.m_afBandwidth != m_settings.m_afBandwidth) ||
       (settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force)
    {
        qDebug() << "WFMDemodSink::applySettings: m_interpolator.create";
        m_interpolator.create(16, m_channelSampleRate, settings.m_afBandwidth);
        m_interpolatorDistanceRemain = (Real) m_channelSampleRate / (Real) m_audioSampleRate;
        m_interpolatorDistance =  (Real) m_channelSampleRate / (Real) m_audioSampleRate;
        qDebug() << "WFMDemodSink::applySettings: m_rfFilter->create_filter";
        Real lowCut = -(settings.m_rfBandwidth / 2.0) / m_channelSampleRate;
        Real hiCut  = (settings.m_rfBandwidth / 2.0) / m_channelSampleRate;
        m_rfFilter->create_filter(lowCut, hiCut);
        m_fmExcursion = (settings.m_rfBandwidth / 2) - m_settings.m_afBandwidth;
        m_fmExcursion = m_fmExcursion < 2500 ? 2500 : m_fmExcursion;
        //m_fmExcursion = settings.m_rfBandwidth / (Real) m_channelSampleRate;
        m_phaseDiscri.setFMScaling((float) m_channelSampleRate / ((float) 2*m_fmExcursion));
        qDebug("WFMDemodSink::applySettings: m_fmExcursion: %f", m_fmExcursion);
    }

    if ((settings.m_squelch != m_settings.m_squelch) || force)
    {
        qDebug() << "WFMDemodSink::applySettings: set m_squelchLevel";
        m_squelchLevel = pow(10.0, settings.m_squelch / 10.0);
    }

    m_settings = settings;
}
