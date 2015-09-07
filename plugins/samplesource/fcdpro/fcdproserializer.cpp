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

#include "fcdproserializer.h"

void FCDProSerializer::writeSerializedData(const FCDData& data, QByteArray& serializedData)
{
	QByteArray sampleSourceSerialized;
	SampleSourceSerializer::writeSerializedData(data.m_data, sampleSourceSerialized);

	SimpleSerializer s(1);

	s.writeBlob(1, sampleSourceSerialized);
	s.writeS32(2, data.m_LOppmTenths);
	s.writeBool(3, data.m_biasT);
	s.writeS32(4, data.m_lnaGainIndex);
	s.writeS32(5, data.m_rfFilterIndex);
	s.writeS32(6, data.m_lnaEnhanceIndex);
	s.writeS32(7, data.m_bandIndex);
	s.writeS32(8, data.m_mixerGainIndex);
	s.writeS32(9, data.m_mixerFilterIndex);
	s.writeS32(10, data.m_biasCurrentIndex);
	s.writeS32(11, data.m_modeIndex);
	s.writeS32(12, data.m_gain1Index);
	s.writeS32(13, data.m_rcFilterIndex);
	s.writeS32(14, data.m_gain2Index);
	s.writeS32(15, data.m_gain3Index);
	s.writeS32(16, data.m_gain4Index);
	s.writeS32(17, data.m_ifFilterIndex);
	s.writeS32(18, data.m_gain5Index);
	s.writeS32(19, data.m_gain6Index);

	serializedData = s.final();
}

bool FCDProSerializer::readSerializedData(const QByteArray& serializedData, FCDData& data)
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
		d.readS32(2, &data.m_LOppmTenths);
		d.readBool(3, &data.m_biasT);
		d.readS32(4, &data.m_lnaGainIndex);
		d.readS32(5, &data.m_rfFilterIndex);
		d.readS32(6, &data.m_lnaEnhanceIndex);
		d.readS32(7, &data.m_bandIndex);
		d.readS32(8, &data.m_mixerGainIndex);
		d.readS32(9, &data.m_mixerFilterIndex);
		d.readS32(10, &data.m_biasCurrentIndex);
		d.readS32(11, &data.m_modeIndex);
		d.readS32(12, &data.m_gain1Index);
		d.readS32(13, &data.m_rcFilterIndex);
		d.readS32(14, &data.m_gain2Index);
		d.readS32(15, &data.m_gain3Index);
		d.readS32(16, &data.m_gain4Index);
		d.readS32(17, &data.m_ifFilterIndex);
		d.readS32(18, &data.m_gain5Index);
		d.readS32(19, &data.m_gain6Index);

		return SampleSourceSerializer::readSerializedData(sampleSourceSerialized, data.m_data);
	}
	else
	{
		setDefaults(data);
		return false;
	}
}

void FCDProSerializer::setDefaults(FCDData& data)
{
	data.m_LOppmTenths = 0;
	data.m_biasT = false;
	data.m_lnaGainIndex = 0;
	data.m_rfFilterIndex = 0;
	data.m_lnaEnhanceIndex = 0;
	data.m_bandIndex = 0;
	data.m_mixerGainIndex = 0;
	data.m_mixerFilterIndex = 0;
	data.m_biasCurrentIndex = 0;
	data.m_modeIndex = 0;
	data.m_gain1Index = 0;
	data.m_rcFilterIndex = 0;
	data.m_gain2Index = 0;
	data.m_gain3Index = 0;
	data.m_gain4Index = 0;
	data.m_ifFilterIndex = 0;
	data.m_gain5Index = 0;
	data.m_gain6Index = 0;
}
