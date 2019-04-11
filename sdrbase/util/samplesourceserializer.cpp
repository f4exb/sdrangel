///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#include "util/samplesourceserializer.h"

const uint SampleSourceSerializer::m_version = 1;

void SampleSourceSerializer::writeSerializedData(const Data& data, QByteArray& serializedData)
{
	SimpleSerializer s(1);

	s.writeU64(1, data.m_frequency);
	s.writeS32(2, data.m_correction);
	s.writeS32(3, data.m_rate);
	s.writeU32(4, data.m_log2Decim);
	s.writeS32(5, data.m_bandwidth);
	s.writeS32(6, data.m_fcPosition);
	s.writeS32(7, data.m_lnaGain);
	s.writeS32(8, data.m_RxGain1);
	s.writeS32(9, data.m_RxGain2);
	s.writeS32(10, data.m_RxGain3);

	serializedData = s.final();
}

bool SampleSourceSerializer::readSerializedData(const QByteArray& serializedData, Data& data)
{
	SimpleDeserializer d(serializedData);

	if (!d.isValid())
	{
		setDefaults(data);
		return false;
	}

	if (d.getVersion() == m_version)
	{
		d.readU64(1, &data.m_frequency, 0);
		d.readS32(2, &data.m_correction, 0);
		d.readS32(3, &data.m_rate, 0);
		d.readU32(4, &data.m_log2Decim, 0);
		d.readS32(5, &data.m_bandwidth, 0);
		d.readS32(6, &data.m_fcPosition, 0);
		d.readS32(7, &data.m_lnaGain, 0);
		d.readS32(8, &data.m_RxGain1, 0);
		d.readS32(9, &data.m_RxGain2, 0);
		d.readS32(10, &data.m_RxGain3, 0);

		return true;
	}
	else
	{
		setDefaults(data);
		return false;
	}
}

void SampleSourceSerializer::setDefaults(Data& data)
{
	data.m_frequency = 0;
	data.m_correction = 0;
	data.m_rate = 0;
	data.m_log2Decim = 0;
	data.m_bandwidth = 0;
	data.m_fcPosition = 0;
	data.m_lnaGain = 0;
	data.m_RxGain1 = 0;
	data.m_RxGain2 = 0;
	data.m_RxGain3 = 0;
}
