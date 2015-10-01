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
#include "rtlsdrsettings.h"

RTLSDRSettings::RTLSDRSettings()
{
	resetToDefaults();
}

void RTLSDRSettings::resetToDefaults()
{
	m_devSampleRate = 1024*1000;
	m_centerFrequency = 435000*1000;
	m_gain = 0;
	m_loPpmCorrection = 0;
	m_log2Decim = 4;
	m_fcPos = FC_POS_CENTER;
	m_dcBlock = false;
	m_iqImbalance = false;
}

QByteArray RTLSDRSettings::serialize() const
{
	SimpleSerializer s(1);

	s.writeS32(1, m_devSampleRate);
	s.writeS32(2, m_gain);
	s.writeS32(3, m_loPpmCorrection);
	s.writeU32(4, m_log2Decim);
	s.writeBool(5, m_dcBlock);
	s.writeBool(6, m_iqImbalance);
	s.writeS32(7, (int) m_fcPos);

	return s.final();
}

bool RTLSDRSettings::deserialize(const QByteArray& data)
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

		d.readS32(1, &m_devSampleRate, 0);
		d.readS32(2, &m_gain, 0);
		d.readS32(3, &m_loPpmCorrection, 0);
		d.readU32(4, &m_log2Decim, 4);
		d.readBool(5, &m_dcBlock, false);
		d.readBool(6, &m_iqImbalance, false);
		d.readS32(7, &intval, 0);
		m_fcPos = (fcPos_t) intval;

		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}


