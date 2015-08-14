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

#include "dsp/samplesource/samplesource.h"
#include "dsp/dspcommands.h"

#include <QDebug>

SampleSource::SampleSource() :
	m_sampleRate(0),
	m_centerFrequency(0)
{
}

SampleSource::~SampleSource()
{
}

void SampleSource::setSampleRate(int sampleRate)
{
	if (sampleRate != m_sampleRate)
	{
		// TODO: adjust FIFO size
		m_sampleRate = sampleRate;
		sendNewData();
	}
}

void SampleSource::setCenterFrequency(quint64 centerFrequency)
{
	if (centerFrequency != m_centerFrequency)
	{
		m_centerFrequency = centerFrequency;
		sendNewData();
	}
}

void SampleSource::sendNewData()
{
	DSPSignalNotification *notif = new DSPSignalNotification(m_sampleRate, m_centerFrequency);
	m_outputMessageQueue.push(notif);
}
