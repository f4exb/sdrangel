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

#include <QTime>
#include <QDebug>
#include <stdio.h>

#include "dsp/dsptypes.h"
#include "dsp/basebandsamplesink.h"

#include "lorademodsink.h"

const int LoRaDemodSink::DATA_BITS = 6;
const int LoRaDemodSink::SAMPLEBITS = LoRaDemodSink::DATA_BITS + 2;
const int LoRaDemodSink::SPREADFACTOR = (1 << LoRaDemodSink::SAMPLEBITS);
const int LoRaDemodSink::LORA_SFFT_LEN = (LoRaDemodSink::SPREADFACTOR / 2);
const int LoRaDemodSink::LORA_SQUELCH = 3;

LoRaDemodSink::LoRaDemodSink() :
        m_spectrumSink(nullptr)
{
	m_Bandwidth = LoRaDemodSettings::bandwidths[0];
	m_channelSampleRate = 96000;
	m_channelFrequencyOffset = 0;
	m_nco.setFreq(m_channelFrequencyOffset, m_channelSampleRate);
	m_interpolator.create(16, m_channelSampleRate, m_Bandwidth/1.9);
	m_sampleDistanceRemain = (Real) m_channelSampleRate / m_Bandwidth;

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
}

LoRaDemodSink::~LoRaDemodSink()
{
    delete loraFilter;
	delete negaFilter;
	delete [] mov;
	delete [] history;
	delete [] finetune;
}

void LoRaDemodSink::dumpRaw()
{
	short bin, j, max;
	char text[256];

	max = m_time / 4 - 3;

	if (max > 140) {
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

		if ((text[j] < 32 )||( text[j] > 126)) {
			text[j] = 0x5f;
		}
	}

	text[3] = text[2];
	text[2] = text[1];
	text[1] = text[0];
	text[j] = 0;

	qDebug("LoRaDemodSink::dumpRaw: %s", &text[1]);
}

short LoRaDemodSink::synch(short bin)
{
	short i, j;

	if (bin < 0)
	{
		if (m_time > 70) {
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

				for (i=0; i<12; i++) {
					j += finetune[15 & (m_time - i)];
				}

				if (j < 0) {
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

	if (m_time & 3) {
		return -1;
	}

	return (bin + m_tune) & (LORA_SFFT_LEN - 1);
}

int LoRaDemodSink::detect(Complex c, Complex a)
{
	int p, q;
	short i, result, negresult, movpoint;
	float peak, negpeak, tfloat;
	float mag[LORA_SFFT_LEN];
	float rev[LORA_SFFT_LEN];

	loraFilter->run(c * a);
	negaFilter->run(c * conj(a));

	// process spectrum twice in FFTLEN
	if (++m_count & ((1 << DATA_BITS) - 1)) {
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

	if (result >= 0) {
		m_result = result;
	}

	return m_result;
}

void LoRaDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
	int newangle;
	Complex ci;

	m_sampleBuffer.clear();

	for (SampleVector::const_iterator it = begin; it < end; ++it)
	{
		Complex c(it->real() / SDR_RX_SCALEF, it->imag() / SDR_RX_SCALEF);
		c *= m_nco.nextIQ();

		if (m_interpolator.decimate(&m_sampleDistanceRemain, c, &ci))
		{
			m_chirp = (m_chirp + 1) & (SPREADFACTOR - 1);
                        m_angle = (m_angle + m_chirp) & (SPREADFACTOR - 1);
			Complex cangle(cos(M_PI*2*m_angle/SPREADFACTOR),-sin(M_PI*2*m_angle/SPREADFACTOR));
			newangle = detect(ci, cangle);

			m_bin = (m_bin + newangle) & (LORA_SFFT_LEN - 1);
			Complex nangle(cos(M_PI*2*m_bin/LORA_SFFT_LEN),sin(M_PI*2*m_bin/LORA_SFFT_LEN));
			m_sampleBuffer.push_back(Sample(nangle.real() * 100, nangle.imag() * 100));
			m_sampleDistanceRemain += (Real) m_channelSampleRate / m_Bandwidth;
		}
	}

	if (m_spectrumSink) {
		m_spectrumSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), false);
	}
}

void LoRaDemodSink::applyChannelSettings(int channelSampleRate, int bandwidth, int channelFrequencyOffset, bool force)
{
    qDebug() << "LoRaDemodSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if((channelFrequencyOffset != m_channelFrequencyOffset) ||
        (channelSampleRate != m_channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((channelSampleRate != m_channelSampleRate) || force)
    {
        qDebug() << "LoRaDemodSink::applyChannelSettings: m_interpolator.create";
        m_interpolator.create(16, channelSampleRate, bandwidth / 1.9f);
        m_sampleDistanceRemain = (Real) channelSampleRate / bandwidth;
    }

    m_channelSampleRate = channelSampleRate;
    m_Bandwidth = bandwidth;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void LoRaDemodSink::applySettings(const LoRaDemodSettings& settings, bool force)
{
    qDebug() << "LoRaDemodSink::applySettings:"
            << " m_centerFrequency: " << settings.m_centerFrequency
            << " m_bandwidthIndex: " << settings.m_bandwidthIndex
            << " m_spread: " << settings.m_spread
            << " m_rgbColor: " << settings.m_rgbColor
            << " m_title: " << settings.m_title
            << " force: " << force;

    m_settings = settings;
}
