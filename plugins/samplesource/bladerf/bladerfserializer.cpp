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

#include "bladerfinput.h"
#include "bladerfserializer.h"

void BladeRFSerializer::writeSerializedData(const BladeRFData& data, QByteArray& serializedData)
{
	const QByteArray& sampleSourceSerialized = SampleSourceSerializer::writeSerializedData(data.m_data);

	SimpleSerializer s(1);

	s.writeBlob(1, sampleSourceSerialized);
	s.writeBool(2, data.m_xb200);
	s.writeS32(3, (int) data.m_xb200Path);
	s.writeS32(4, (int) data.m_xb200Filter);

	serializedData = s.final();
}

bool BladeRFSerializer::readSerializedData(const QByteArray& serializedData, BladeRFData& data)
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
		d.readBool(2, &data.m_xb200);
		d.readS32(3, &intval);
		data.m_xb200Path = (bladerf_xb200_path) intval;
		d.readS32(4, &intval);
		data.m_xb200Filter = (bladerf_xb200_filter) intval;

		return SampleSourceSerializer::readSerializedData(sampleSourceSerialized, data.m_data);
	}
	else
	{
		setDefaults(data);
		return false;
	}
}

void BladeRFSerializer::setDefaults(BladeRFData& data)
{
	data.m_xb200 = false;
	data.m_xb200Path = BLADERF_XB200_MIX;
	data.m_xb200Filter = BLADERF_XB200_AUTO_1DB;
}
