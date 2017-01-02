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

#include <QtGlobal>
#include "util/simpleserializer.h"
#include "bladerfoutputsettings.h"


BladeRFOutputSettings::BladeRFOutputSettings()
{
	resetToDefaults();
}

void BladeRFOutputSettings::resetToDefaults()
{
	m_centerFrequency = 435000*1000;
	m_devSampleRate = 3072000;
	m_vga1 = 20;
	m_vga2 = 9;
	m_bandwidth = 1500000;
	m_log2Interp = 0;
	m_xb200 = false;
	m_xb200Path = BLADERF_XB200_MIX;
	m_xb200Filter = BLADERF_XB200_AUTO_1DB;
}

QByteArray BladeRFOutputSettings::serialize() const
{
	SimpleSerializer s(1);

	s.writeS32(1, m_devSampleRate);
	s.writeS32(2, m_vga1);
	s.writeS32(3, m_vga2);
	s.writeS32(4, m_bandwidth);
	s.writeU32(5, m_log2Interp);
	s.writeBool(6, m_xb200);
	s.writeS32(7, (int) m_xb200Path);
	s.writeS32(8, (int) m_xb200Filter);

	return s.final();
}

bool BladeRFOutputSettings::deserialize(const QByteArray& data)
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

		d.readS32(1, &m_devSampleRate);
		d.readS32(2, &m_vga1);
		d.readS32(3, &m_vga2);
		d.readS32(4, &m_bandwidth);
		d.readU32(5, &m_log2Interp);
		d.readBool(6, &m_xb200);
		d.readS32(7, &intval);
		m_xb200Path = (bladerf_xb200_path) intval;
		d.readS32(8, &intval);
		m_xb200Filter = (bladerf_xb200_filter) intval;

		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}
