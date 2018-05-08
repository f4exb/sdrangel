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

#include "../hackrfinput/hackrfinputsettings.h"

#include <QtGlobal>
#include "util/simpleserializer.h"


HackRFInputSettings::HackRFInputSettings()
{
	resetToDefaults();
}

void HackRFInputSettings::resetToDefaults()
{
	m_centerFrequency = 435000 * 1000;
	m_LOppmTenths = 0;
	m_biasT = false;
	m_log2Decim = 0;
	m_fcPos = FC_POS_CENTER;
	m_lnaExt = false;
	m_lnaGain = 16;
	m_bandwidth = 1750000;
	m_vgaGain = 16;
	m_dcBlock = false;
	m_iqCorrection = false;
	m_devSampleRate = 2400000;
	m_linkTxFrequency = false;
	m_fileRecordName = "";
}

QByteArray HackRFInputSettings::serialize() const
{
	SimpleSerializer s(1);

	s.writeS32(1, m_LOppmTenths);
	s.writeBool(3, m_biasT);
	s.writeU32(4, m_log2Decim);
	s.writeS32(5, m_fcPos);
	s.writeBool(6, m_lnaExt);
	s.writeU32(7, m_lnaGain);
	s.writeU32(8, m_bandwidth);
	s.writeU32(9, m_vgaGain);
	s.writeBool(10, m_dcBlock);
	s.writeBool(11, m_iqCorrection);
	s.writeU64(12, m_devSampleRate);
    s.writeBool(13, m_linkTxFrequency);

	return s.final();
}

bool HackRFInputSettings::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if (!d.isValid())
	{
		resetToDefaults();
		return false;
	}

	if (d.getVersion() == 1)
	{
		int intval;

		d.readS32(1, &m_LOppmTenths, 0);
		d.readBool(3, &m_biasT, false);
		d.readU32(4, &m_log2Decim, 0);
		d.readS32(5, &intval, 0);
		m_fcPos = (fcPos_t) intval;
		d.readBool(6, &m_lnaExt, false);
		d.readU32(7, &m_lnaGain, 16);
		d.readU32(8, &m_bandwidth, 1750000);
		d.readU32(9, &m_vgaGain, 16);
		d.readBool(10, &m_dcBlock, false);
		d.readBool(11, &m_iqCorrection, false);
		d.readU64(12, &m_devSampleRate, 2400000U);
        d.readBool(11, &m_linkTxFrequency, false);

		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}
