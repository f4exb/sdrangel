///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

#ifndef INCLUDE_TETRADEMOD_H
#define INCLUDE_TETRADEMOD_H

#include "dsp/samplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "util/message.h"

class MessageQueue;

class TetraDemod : public SampleSink {
public:
	TetraDemod(SampleSink* sampleSink);
	~TetraDemod();

	void configure(MessageQueue* messageQueue);

	void feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool firstOfBurst);
	void start();
	void stop();
	bool handleMessage(Message* cmd);

private:
	class MsgConfigureTetraDemod : public Message {
	public:
		static MessageRegistrator ID;

		static MsgConfigureTetraDemod* create()
		{
			return new MsgConfigureTetraDemod();
		}

	private:
		MsgConfigureTetraDemod() :
			Message(ID())
		{ }
	};

	int m_sampleRate;
	int m_frequency;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_sampleDistanceRemain;

	SampleSink* m_sampleSink;
	SampleVector m_sampleBuffer;
};

#endif // INCLUDE_TETRADEMOD_H
