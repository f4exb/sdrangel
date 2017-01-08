///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include "hackrfoutputsettings.h"

#include <QtGlobal>
#include "util/simpleserializer.h"


HackRFOutputSettings::HackRFOutputSettings()
{
	resetToDefaults();
}

void HackRFOutputSettings::resetToDefaults()
{
	m_centerFrequency = 435000 * 1000;
	m_LOppmTenths = 0;
	m_devSampleRate = 2400000;
	m_biasT = false;
	m_log2Interp = 0;
	m_lnaExt = false;
	m_vgaGain = 30;
	m_bandwidth = 1750000;
	m_txvgaGain = 22;
}

QByteArray HackRFOutputSettings::serialize() const
{
	SimpleSerializer s(1);

	s.writeS32(1, m_LOppmTenths);
	s.writeU32(2, m_devSampleRate);
	s.writeBool(3, m_biasT);
	s.writeU32(4, m_log2Interp);
	s.writeBool(5, m_lnaExt);
	s.writeU32(6, m_vgaGain);
	s.writeU32(7, m_bandwidth);
	s.writeU32(8, m_txvgaGain);

	return s.final();
}

bool HackRFOutputSettings::deserialize(const QByteArray& data)
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
		d.readU32(2, &m_devSampleRate, 2400000);
		d.readBool(3, &m_biasT, false);
		d.readU32(4, &m_log2Interp, 0);
		d.readBool(5, &m_lnaExt, false);
		d.readU32(6, &m_vgaGain, 30);
		d.readU32(7, &m_bandwidth, 1750000);
		d.readU32(8, &m_txvgaGain, 22);

		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}
