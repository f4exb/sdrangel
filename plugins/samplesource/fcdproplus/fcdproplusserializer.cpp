///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#include "fcdproplusserializer.h"

void FCDProPlusSerializer::writeSerializedData(const FCDData& data, QByteArray& serializedData)
{
	QByteArray sampleSourceSerialized;
	SampleSourceSerializer::writeSerializedData(data.m_data, sampleSourceSerialized);

	SimpleSerializer s(1);

	s.writeBlob(1, sampleSourceSerialized);
	s.writeS32(2, data.m_bias);
	s.writeS32(3, data.m_range);

	serializedData = s.final();
}

bool FCDProPlusSerializer::readSerializedData(const QByteArray& serializedData, FCDData& data)
{
	bool valid = SampleSourceSerializer::readSerializedData(serializedData, data.m_data);

	QByteArray sampleSourceSerialized;

	SimpleDeserializer d(serializedData);

	if (!d.isValid())
	{
		setDefaults(data);
		return false;
	}

	if (d.getVersion() == SampleSourceSerializer::getSerializerVersion())
	{
		int intval;

		d.readBlob(1, &sampleSourceSerialized);
		d.readS32(2, &data.m_bias);
		d.readS32(3, &data.m_range);

		return SampleSourceSerializer::readSerializedData(sampleSourceSerialized, data.m_data);
	}
	else
	{
		setDefaults(data);
		return false;
	}
}

void FCDProPlusSerializer::setDefaults(FCDData& data)
{
	data.m_range = 0;
	data.m_bias = 0;
}
