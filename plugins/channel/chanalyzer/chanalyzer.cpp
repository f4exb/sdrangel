///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// (c) 2014 Modified by John Greb
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

#include <QTime>
#include <QDebug>
#include <stdio.h>
#include "audio/audiooutput.h"
#include "dsp/channelizer.h"

#include "chanalyzer.h"

MESSAGE_CLASS_DEFINITION(ChannelAnalyzer::MsgConfigureChannelAnalyzer, Message)

ChannelAnalyzer::ChannelAnalyzer(SampleSink* sampleSink) :
	m_sampleSink(sampleSink)
{
	m_Bandwidth = 5000;
	m_LowCutoff = 300;
	m_spanLog2 = 3;
	m_sampleRate = 96000;
	m_frequency = 0;
	m_nco.setFreq(m_frequency, m_sampleRate);
	m_nco_test.setFreq(m_frequency, m_sampleRate);
	m_undersampleCount = 0;
	m_usb = true;
	m_ssb = true;
	SSBFilter = new fftfilt(m_LowCutoff / m_sampleRate, m_Bandwidth / m_sampleRate, ssbFftLen);
	DSBFilter = new fftfilt(m_Bandwidth / m_sampleRate, 2*ssbFftLen);
}

ChannelAnalyzer::~ChannelAnalyzer()
{
	if (SSBFilter) delete SSBFilter;
	if (DSBFilter) delete DSBFilter;
}

void ChannelAnalyzer::configure(MessageQueue* messageQueue,
		Real Bandwidth,
		Real LowCutoff,
		int  spanLog2,
		bool ssb)
{
	Message* cmd = MsgConfigureChannelAnalyzer::create(Bandwidth, LowCutoff, spanLog2, ssb);
	messageQueue->push(cmd);
}

void ChannelAnalyzer::feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool positiveOnly)
{
	fftfilt::cmplx *sideband, sum;
	int n_out;
	int decim = 1<<m_spanLog2;
	unsigned char decim_mask = decim - 1; // counter LSB bit mask for decimation by 2^(m_scaleLog2 - 1)

	for(SampleVector::const_iterator it = begin; it < end; ++it)
	{
		Complex c(it->real() / 32768.0, it->imag() / 32768.0);
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

			sum += sideband[i];

			if (!(m_undersampleCount++ & decim_mask))
			{
				sum /= decim;

				if (m_ssb & !m_usb)
				{ // invert spectrum for LSB
					m_sampleBuffer.push_back(Sample(sum.imag() * 32768.0, sum.real() * 32768.0));
				}
				else
				{
					m_sampleBuffer.push_back(Sample(sum.real() * 32768.0, sum.imag() * 32768.0));
				}

				sum = 0;
			}
		}
	}

	if(m_sampleSink != NULL)
	{
		m_sampleSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), m_ssb); // m_ssb = positive only
	}

	m_sampleBuffer.clear();
}

void ChannelAnalyzer::start()
{
}

void ChannelAnalyzer::stop()
{
}

bool ChannelAnalyzer::handleMessage(const Message& cmd)
{
	float band, lowCutoff;

	qDebug() << "ChannelAnalyzer::handleMessage";

	if (Channelizer::MsgChannelizerNotification::match(cmd))
	{
		Channelizer::MsgChannelizerNotification& notif = (Channelizer::MsgChannelizerNotification&) cmd;

		m_sampleRate = notif.getSampleRate();
		m_nco.setFreq(-notif.getFrequencyOffset(), m_sampleRate);

		qDebug() << "ChannelAnalyzer::handleMessage: MsgChannelizerNotification: m_sampleRate: " << m_sampleRate
				<< " frequencyOffset: " << notif.getFrequencyOffset();

		return true;
	}
	else if (MsgConfigureChannelAnalyzer::match(cmd))
	{
		MsgConfigureChannelAnalyzer& cfg = (MsgConfigureChannelAnalyzer&) cmd;

		band = cfg.getBandwidth();
		lowCutoff = cfg.getLoCutoff();

		if (band < 0)
		{
			band = -band;
			lowCutoff = -lowCutoff;
			m_usb = false;
		}
		else
		{
			m_usb = true;
		}

		if (band < 100.0f)
		{
			band = 100.0f;
			lowCutoff = 0;
		}

		m_Bandwidth = band;
		m_LowCutoff = lowCutoff;

		SSBFilter->create_filter(m_LowCutoff / m_sampleRate, m_Bandwidth / m_sampleRate);
		DSBFilter->create_dsb_filter(m_Bandwidth / m_sampleRate);

		m_spanLog2 = cfg.getSpanLog2();
		m_ssb = cfg.getSSB();

		qDebug() << "  - MsgConfigureChannelAnalyzer: m_Bandwidth: " << m_Bandwidth
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
