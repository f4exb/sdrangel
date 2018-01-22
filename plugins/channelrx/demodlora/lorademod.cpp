///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// (c) 2015 John Greb
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

#include <dsp/downchannelizer.h>
#include "dsp/threadedbasebandsamplesink.h"
#include <device/devicesourceapi.h>

#include "lorademod.h"
#include "lorabits.h"

MESSAGE_CLASS_DEFINITION(LoRaDemod::MsgConfigureLoRaDemod, Message)
MESSAGE_CLASS_DEFINITION(LoRaDemod::MsgConfigureChannelizer, Message)

const QString LoRaDemod::m_channelIdURI = "de.maintech.sdrangelove.channel.lora";
const QString LoRaDemod::m_channelId = "LoRaDemod";

LoRaDemod::LoRaDemod(DeviceSourceAPI* deviceAPI) :
        ChannelSinkAPI(m_channelIdURI),
        m_deviceAPI(deviceAPI),
        m_sampleSink(0),
        m_settingsMutex(QMutex::Recursive)
{
	setObjectName(m_channelId);

	m_Bandwidth = LoRaDemodSettings::bandwidths[0];
	m_sampleRate = 96000;
	m_frequency = 0;
	m_nco.setFreq(m_frequency, m_sampleRate);
	m_interpolator.create(16, m_sampleRate, m_Bandwidth/1.9);
	m_sampleDistanceRemain = (Real)m_sampleRate / m_Bandwidth;

	m_chirp = 0;
	m_angle = 0;
	m_bin = 0;
	m_result = 0;
	m_count = 0;
	m_header = 0;
	m_time = 0;
	m_tune = 0;

	loraFilter = new sfft(LORA_SFFT_LEN);
	negaFilter = new sfft(LORA_SFFT_LEN);

	mov = new float[4*LORA_SFFT_LEN];
	history = new short[1024];
	finetune = new short[16];

    m_channelizer = new DownChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer);
    m_deviceAPI->addThreadedSink(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);
}

LoRaDemod::~LoRaDemod()
{
	if (loraFilter)
		delete loraFilter;
	if (negaFilter)
		delete negaFilter;
	if (mov)
		delete [] mov;
	if (history)
		delete [] history;
	if (finetune)
		delete [] finetune;

	m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
}

void LoRaDemod::dumpRaw()
{
	short bin, j, max;
	char text[256];

	max = m_time / 4 - 3;

	if (max > 140)
	{
		max = 140; // about 2 symbols to each char
	}

	for ( j=0; j < max; j++)
	{
		bin = (history[(j + 1)  * 4] + m_tune ) & (LORA_SFFT_LEN - 1);
		text[j] = toGray(bin >> 1);
	}

	prng6(text, max);
	// First block is always 8 symbols
	interleave6(text, 6);
	interleave6(&text[8], max);
	hamming6(text, 6);
	hamming6(&text[8], max);

	for ( j=0; j < max / 2; j++)
	{
		text[j] = (text[j * 2 + 1] << 4) | (0xf & text[j * 2 + 0]);

		if ((text[j] < 32 )||( text[j] > 126))
		{
			text[j] = 0x5f;
		}
	}

	text[3] = text[2];
	text[2] = text[1];
	text[1] = text[0];
	text[j] = 0;

	printf("%s\n", &text[1]);
}

short LoRaDemod::synch(short bin)
{
	short i, j;

	if (bin < 0)
	{
		if (m_time > 70)
		{
			dumpRaw();
		}

		m_time = 0;
		return -1;
	}

	history[m_time] = bin;

	if (m_time > 12)
	{
		if (bin == history[m_time - 6])
		{
			if (bin == history[m_time - 12])
			{
				m_tune = LORA_SFFT_LEN - bin;
				j = 0;

				for (i=0; i<12; i++)
				{
					j += finetune[15 & (m_time - i)];
				}

				if (j < 0)
				{
					m_tune += 1;
				}

				m_tune &= (LORA_SFFT_LEN - 1);
				m_time = 0;
				return -1;
			}
		}
	}

	m_time++;
	m_time &= 1023;

	if (m_time & 3)
	{
		return -1;
	}

	return (bin + m_tune) & (LORA_SFFT_LEN - 1);
}

int LoRaDemod::detect(Complex c, Complex a)
{
	int p, q;
	short i, result, negresult, movpoint;
	float peak, negpeak, tfloat;
	float mag[LORA_SFFT_LEN];
	float rev[LORA_SFFT_LEN];

	loraFilter->run(c * a);
	negaFilter->run(c * conj(a));

	// process spectrum twice in FFTLEN
	if (++m_count & ((1 << DATA_BITS) - 1))
	{
		return m_result;
	}

	movpoint = 3 & (m_count >> DATA_BITS);

	loraFilter->fetch(mag);
	negaFilter->fetch(rev);
	peak = negpeak = 0.0f;
	result = negresult = 0;

	for (i = 0; i < LORA_SFFT_LEN; i++)
	{
		if (rev[i] > negpeak)
		{
			negpeak = rev[i];
			negresult = i;
		}

		tfloat = mov[i] + mov[LORA_SFFT_LEN + i] +mov[2 * LORA_SFFT_LEN + i]
				+ mov[3 * LORA_SFFT_LEN + i] + mag[i];

		if (tfloat > peak)
		{
			peak = tfloat;
			result = i;
		}

		mov[movpoint * LORA_SFFT_LEN + i] = mag[i];
	}

	p = (result - 1 + LORA_SFFT_LEN) & (LORA_SFFT_LEN -1);
	q = (result + 1) & (LORA_SFFT_LEN -1);
	finetune[15 & m_time] = (mag[p] > mag[q]) ? -1 : 1;

	if (peak < negpeak * LORA_SQUELCH)
	{
		result = -1;
	}

	result = synch(result);

	if (result >= 0)
	{
		m_result = result;
	}

	return m_result;
}

void LoRaDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool pO __attribute__((unused)))
{
	int newangle;
	Complex ci;

	m_sampleBuffer.clear();

	m_settingsMutex.lock();

	for(SampleVector::const_iterator it = begin; it < end; ++it)
	{
		Complex c(it->real() / SDR_RX_SCALEF, it->imag() / SDR_RX_SCALEF);
		c *= m_nco.nextIQ();

		if(m_interpolator.decimate(&m_sampleDistanceRemain, c, &ci))
		{
			m_chirp = (m_chirp + 1) & (SPREADFACTOR - 1);
                        m_angle = (m_angle + m_chirp) & (SPREADFACTOR - 1);
			Complex cangle(cos(M_PI*2*m_angle/SPREADFACTOR),-sin(M_PI*2*m_angle/SPREADFACTOR));
			newangle = detect(ci, cangle);

			m_bin = (m_bin + newangle) & (LORA_SFFT_LEN - 1);
			Complex nangle(cos(M_PI*2*m_bin/LORA_SFFT_LEN),sin(M_PI*2*m_bin/LORA_SFFT_LEN));
			m_sampleBuffer.push_back(Sample(nangle.real() * 100, nangle.imag() * 100));
			m_sampleDistanceRemain += (Real)m_sampleRate / m_Bandwidth;
		}
	}

	if(m_sampleSink != 0)
	{
		m_sampleSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), false);
	}

	m_settingsMutex.unlock();
}

void LoRaDemod::start()
{
}

void LoRaDemod::stop()
{
}

bool LoRaDemod::handleMessage(const Message& cmd)
{
	qDebug() << "LoRaDemod::handleMessage";

	if (DownChannelizer::MsgChannelizerNotification::match(cmd))
	{
		DownChannelizer::MsgChannelizerNotification& notif = (DownChannelizer::MsgChannelizerNotification&) cmd;

		m_settingsMutex.lock();

		m_sampleRate = notif.getSampleRate();
		m_nco.setFreq(-notif.getFrequencyOffset(), m_sampleRate);
		m_interpolator.create(16, m_sampleRate, m_Bandwidth/1.9);
		m_sampleDistanceRemain = m_sampleRate / m_Bandwidth;

		m_settingsMutex.unlock();

		qDebug() << "LoRaDemod::handleMessage: MsgChannelizerNotification: m_sampleRate: " << m_sampleRate
				<< " frequencyOffset: " << notif.getFrequencyOffset();

		return true;
	}
    else if (MsgConfigureChannelizer::match(cmd))
    {
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;

        m_channelizer->configure(m_channelizer->getInputMessageQueue(),
            cfg.getSampleRate(),
            cfg.getCenterFrequency());

        qDebug() << "LoRaDemod::handleMessage: MsgConfigureChannelizer: sampleRate: " << cfg.getSampleRate()
                << " centerFrequency: " << cfg.getCenterFrequency();

        return true;
    }
	else if (MsgConfigureLoRaDemod::match(cmd))
	{
		MsgConfigureLoRaDemod& cfg = (MsgConfigureLoRaDemod&) cmd;

		m_settingsMutex.lock();

		LoRaDemodSettings settings = cfg.getSettings();

		m_Bandwidth = LoRaDemodSettings::bandwidths[settings.m_bandwidthIndex];
		m_interpolator.create(16, m_sampleRate, m_Bandwidth/1.9);

		m_settingsMutex.unlock();

		m_settings = settings;
		qDebug() << "LoRaDemod::handleMessage: MsgConfigureLoRaDemod: m_Bandwidth: " << m_Bandwidth;

		return true;
	}
	else
	{
		if(m_sampleSink != 0)
		{
			 return m_sampleSink->handleMessage(cmd);
		}
		else
		{
			return false;
		}
	}
}

QByteArray LoRaDemod::serialize() const
{
    return m_settings.serialize();
}

bool LoRaDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureLoRaDemod *msg = MsgConfigureLoRaDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureLoRaDemod *msg = MsgConfigureLoRaDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

