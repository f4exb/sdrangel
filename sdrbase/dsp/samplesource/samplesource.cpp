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
#include "util/simpleserializer.h"

SampleSource::GeneralSettings::GeneralSettings() :
	m_centerFrequency(100000000)
{
}

void SampleSource::GeneralSettings::resetToDefaults()
{
	m_centerFrequency = 100000000;
}

QByteArray SampleSource::GeneralSettings::serialize() const
{
	SimpleSerializer s(1);
	s.writeU64(1, m_centerFrequency);
	return s.final();
}

bool SampleSource::GeneralSettings::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if(!d.isValid()) {
		resetToDefaults();
		return false;
	}

	if(d.getVersion() == 1) {
		d.readU64(1, &m_centerFrequency, 100000000);
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}

SampleSource::SampleSource(MessageQueue* guiMessageQueue) :
	m_sampleFifo(),
	m_guiMessageQueue(guiMessageQueue)
{
}

SampleSource::~SampleSource()
{
}
