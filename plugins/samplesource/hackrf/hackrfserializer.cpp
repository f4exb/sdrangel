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

#include "hackrfserializer.h"

void HackRFSerializer::writeSerializedData(const AirspyData& data, QByteArray& serializedData)
{
	QByteArray sampleSourceSerialized;
	SampleSourceSerializer::writeSerializedData(data.m_data, sampleSourceSerialized);

	SimpleSerializer s(1);

	s.writeBlob(1, sampleSourceSerialized);
	s.writeS32(2, data.m_LOppmTenths);
	s.writeU32(3, data.m_sampleRateIndex);
	s.writeU32(4, data.m_log2Decim);
	s.writeS32(5, data.m_fcPos);
	s.writeU32(6, data.m_lnaGain);
	s.writeU32(7, data.m_mixerGain);
	s.writeU32(8, data.m_vgaGain);
	s.writeBool(9, data.m_biasT);

	serializedData = s.final();
}

bool HackRFSerializer::readSerializedData(const QByteArray& serializedData, AirspyData& data)
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
		d.readS32(2, &data.m_LOppmTenths, 0);
		d.readU32(3, &data.m_sampleRateIndex, 0);
		d.readU32(4, &data.m_log2Decim, 0);
		d.readS32(5, &data.m_fcPos, 0);
		d.readU32(6, &data.m_lnaGain, 14);
		d.readU32(7, &data.m_mixerGain, 15);
		d.readU32(8, &data.m_vgaGain, 4);
		d.readBool(9, &data.m_biasT, false);

		return SampleSourceSerializer::readSerializedData(sampleSourceSerialized, data.m_data);
	}
	else
	{
		setDefaults(data);
		return false;
	}
}

void HackRFSerializer::setDefaults(AirspyData& data)
{
	data.m_LOppmTenths = 0;
	data.m_sampleRateIndex = 0;
	data.m_log2Decim = 0;
	data.m_fcPos = 0;
	data.m_lnaGain = 14;
	data.m_mixerGain = 15;
	data.m_vgaGain = 4;
	data.m_biasT = false;
}
