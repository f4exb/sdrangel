///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include "dsp/basebandsamplesource.h"
#include "util/message.h"

BasebandSampleSource::BasebandSampleSource() :
    m_guiMessageQueue(0),
	m_sampleFifo(48000) // arbitrary, will be adjusted to match device sink FIFO size
{
	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
	connect(&m_sampleFifo, SIGNAL(dataWrite(int)), this, SLOT(handleWriteToFifo(int)));
}

BasebandSampleSource::~BasebandSampleSource()
{
}

void BasebandSampleSource::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != 0)
	{
		if (handleMessage(*message))
		{
			delete message;
		}
	}
}

void BasebandSampleSource::handleWriteToFifo(int nbSamples)
{
    SampleVector::iterator writeAt;
    m_sampleFifo.getWriteIterator(writeAt);
    pullAudio(nbSamples); // Pre-fetch input audio samples this is mandatory to keep things running smoothly

    for (int i = 0; i < nbSamples; i++)
    {
        pull((*writeAt));
        m_sampleFifo.bumpIndex(writeAt);
    }
}


