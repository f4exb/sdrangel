///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "chanalyzerng.h"

#include <dsp/downchannelizer.h>
#include <QTime>
#include <QDebug>
#include <stdio.h>
#include "audio/audiooutput.h"


MESSAGE_CLASS_DEFINITION(ChannelAnalyzerNG::MsgConfigureChannelAnalyzer, Message)

ChannelAnalyzerNG::ChannelAnalyzerNG(BasebandSampleSink* sampleSink) :
	m_sampleSink(sampleSink),
	m_settingsMutex(QMutex::Recursive)
{
	m_Bandwidth = 5000;
	m_LowCutoff = 300;
	m_spanLog2 = 3;
	m_inputSampleRate = 96000;
	m_frequency = 0;
	m_nco.setFreq(m_frequency, m_inputSampleRate);
	m_undersampleCount = 0;
	m_sum = 0;
	m_usb = true;
	m_ssb = true;
	m_magsq = 0;
	SSBFilter = new fftfilt(m_LowCutoff / m_inputSampleRate, m_Bandwidth / m_inputSampleRate, ssbFftLen);
	DSBFilter = new fftfilt(m_Bandwidth / m_inputSampleRate, 2*ssbFftLen);
}

ChannelAnalyzerNG::~ChannelAnalyzerNG()
{
	if (SSBFilter) delete SSBFilter;
	if (DSBFilter) delete DSBFilter;
}

void ChannelAnalyzerNG::configure(MessageQueue* messageQueue,
		int channelSampleRate,
		Real Bandwidth,
		Real LowCutoff,
		int  spanLog2,
		bool ssb)
{
	Message* cmd = MsgConfigureChannelAnalyzer::create(channelSampleRate, Bandwidth, LowCutoff, spanLog2, ssb);
	messageQueue->push(cmd);
}

void ChannelAnalyzerNG::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly)
{
	fftfilt::cmplx *sideband;
	int n_out;
	int decim = 1<<m_spanLog2;
	unsigned char decim_mask = decim - 1; // counter LSB bit mask for decimation by 2^(m_scaleLog2 - 1)

	m_settingsMutex.lock();

	for(SampleVector::const_iterator it = begin; it < end; ++it)
	{
		//Complex c(it->real() / 32768.0f, it->imag() / 32768.0f);
		Complex c(it->real(), it->imag());
		c *= m_nco.nextIQ();

		if (m_ssb)
		{
			n_out = SSBFilter->runSSB(c, &sideband, m_usb);
		}
		else
		{
			n_out = DSBFilter->runDSB(c, &sideband);
		}

		for (int i = 0; i < n_out; i++)
		{
			// Downsample by 2^(m_scaleLog2 - 1) for SSB band spectrum display
			// smart decimation with bit gain using float arithmetic (23 bits significand)

			m_sum += sideband[i];

			if (!(m_undersampleCount++ & decim_mask))
			{
				m_sum /= decim;
				m_magsq = (m_sum.real() * m_sum.real() + m_sum.imag() * m_sum.imag())/ (1<<30);

				if (m_ssb & !m_usb)
				{ // invert spectrum for LSB
					//m_sampleBuffer.push_back(Sample(m_sum.imag() * 32768.0, m_sum.real() * 32768.0));
					m_sampleBuffer.push_back(Sample(m_sum.imag(), m_sum.real()));
				}
				else
				{
					//m_sampleBuffer.push_back(Sample(m_sum.real() * 32768.0, m_sum.imag() * 32768.0));
					m_sampleBuffer.push_back(Sample(m_sum.real(), m_sum.imag()));
				}

				m_sum = 0;
			}
		}
	}

	if(m_sampleSink != NULL)
	{
		m_sampleSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), m_ssb); // m_ssb = positive only
	}

	m_sampleBuffer.clear();

	m_settingsMutex.unlock();
}

void ChannelAnalyzerNG::start()
{
}

void ChannelAnalyzerNG::stop()
{
}

bool ChannelAnalyzerNG::handleMessage(const Message& cmd)
{
	float bandwidth, lowCutoff;

	qDebug() << "ChannelAnalyzerNG::handleMessage: " << cmd.getIdentifier();

	if (DownChannelizer::MsgChannelizerNotification::match(cmd))
	{
		DownChannelizer::MsgChannelizerNotification& notif = (DownChannelizer::MsgChannelizerNotification&) cmd;

		m_inputSampleRate = notif.getSampleRate();
		m_nco.setFreq(-notif.getFrequencyOffset(), m_inputSampleRate);

		qDebug() << "ChannelAnalyzerNG::handleMessage: MsgChannelizerNotification: m_sampleRate: " << m_inputSampleRate
				<< " frequencyOffset: " << notif.getFrequencyOffset();

		return true;
	}
	else if (MsgConfigureChannelAnalyzer::match(cmd))
	{
		MsgConfigureChannelAnalyzer& cfg = (MsgConfigureChannelAnalyzer&) cmd;

		bandwidth = cfg.getBandwidth();
		lowCutoff = cfg.getLoCutoff();

		if (bandwidth < 0)
		{
			bandwidth = -bandwidth;
			lowCutoff = -lowCutoff;
			m_usb = false;
		}
		else
		{
			m_usb = true;
		}

		if (bandwidth < 100.0f)
		{
			bandwidth = 100.0f;
			lowCutoff = 0;
		}

		m_settingsMutex.lock();

		m_Bandwidth = bandwidth;
		m_LowCutoff = lowCutoff;

		SSBFilter->create_filter(m_LowCutoff / m_inputSampleRate, m_Bandwidth / m_inputSampleRate);
		DSBFilter->create_dsb_filter(m_Bandwidth / m_inputSampleRate);

		m_spanLog2 = cfg.getSpanLog2();
		m_ssb = cfg.getSSB();

		m_settingsMutex.unlock();

		qDebug() << "ChannelAnalyzerNG::handleMessage: MsgConfigureChannelAnalyzer:"
		        << " m_Bandwidth: " << m_Bandwidth
				<< " m_LowCutoff: " << m_LowCutoff
				<< " m_spanLog2: " << m_spanLog2
				<< " m_ssb: " << m_ssb;

		return true;
	}
	else
	{
		if (m_sampleSink != 0)
		{
		   return m_sampleSink->handleMessage(cmd);
		}
		else
		{
			return false;
		}
	}
}
