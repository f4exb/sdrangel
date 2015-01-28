///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// (C) 2015 John Greb                                                            //
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

#ifndef INCLUDE_LoRaDEMOD_H
#define INCLUDE_LoRaDEMOD_H

#include <vector>
#include "dsp/samplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "util/message.h"
#include "dsp/fftfilt.h"

#define DATA_BITS (6)
#define SAMPLEBITS (DATA_BITS + 2)
#define SPREADFACTOR (1 << SAMPLEBITS)
#define LORA_SFFT_LEN (SPREADFACTOR / 2)

class LoRaDemod : public SampleSink {
public:
	LoRaDemod(SampleSink* sampleSink);
	~LoRaDemod();

	void configure(MessageQueue* messageQueue, Real Bandwidth);

	void feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool pO);
	void start();
	void stop();
	bool handleMessage(Message* cmd);

private:
	int  detect(Complex sample, Complex angle);
	void interleave(short* inout);
	void dumpRaw(void);
	short synch (short bin);
	short toGray(short bin);
	class MsgConfigureLoRaDemod : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		Real getBandwidth() const { return m_Bandwidth; }

		static MsgConfigureLoRaDemod* create(Real Bandwidth)
		{
			return new MsgConfigureLoRaDemod(Bandwidth);
		}

	private:
		Real m_Bandwidth;

		MsgConfigureLoRaDemod(Real Bandwidth) :
			Message(),
			m_Bandwidth(Bandwidth)
		{
		}
	};

	Real m_Bandwidth;
	int m_sampleRate;
	int m_frequency;
	int m_chirp;
	int m_angle;
	int m_bin;
	int m_result;
	int m_count;
	int m_header;
	int m_time;
	short m_tune;

	sfft* loraFilter;
	sfft* negaFilter;
	float* mov;
	short* history;
	short* finetune;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_sampleDistanceRemain;

	SampleSink* m_sampleSink;
	SampleVector m_sampleBuffer;
};

#endif // INCLUDE_LoRaDEMOD_H
