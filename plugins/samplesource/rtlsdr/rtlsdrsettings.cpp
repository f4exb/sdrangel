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
	m_lowSampleRate = false;
	m_centerFrequency = 435000*1000;
	m_gain = 0;
	m_loPpmCorrection = 0;
	m_log2Decim = 4;
	m_fcPos = FC_POS_CENTER;
	m_dcBlock = false;
	m_iqImbalance = false;
	m_agc = false;
	m_noModMode = false;
    m_transverterMode = false;
	m_transverterDeltaFrequency = 0;
	m_rfBandwidth = 2500 * 1000; // Hz
}

QByteArray RTLSDRSettings::serialize() const
{
	SimpleSerializer s(1);

	s.writeS32(2, m_gain);
	s.writeS32(3, m_loPpmCorrection);
	s.writeU32(4, m_log2Decim);
	s.writeBool(5, m_dcBlock);
	s.writeBool(6, m_iqImbalance);
	s.writeS32(7, (int) m_fcPos);
    s.writeS32(8, m_devSampleRate);
    s.writeBool(9, m_lowSampleRate);
    s.writeBool(10, m_agc);
    s.writeBool(11, m_noModMode);
    s.writeBool(12, m_transverterMode);
    s.writeS64(13, m_transverterDeltaFrequency);
    s.writeU32(14, m_rfBandwidth);

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

		d.readS32(2, &m_gain, 0);
		d.readS32(3, &m_loPpmCorrection, 0);
		d.readU32(4, &m_log2Decim, 4);
		d.readBool(5, &m_dcBlock, false);
		d.readBool(6, &m_iqImbalance, false);
		d.readS32(7, &intval, 0);
		m_fcPos = (fcPos_t) intval;
        d.readS32(8, &m_devSampleRate, 1024*1000);
        d.readBool(9, &m_lowSampleRate, false);
        d.readBool(10, &m_agc, false);
        d.readBool(11, &m_noModMode, false);
        d.readBool(12, &m_transverterMode, false);
        d.readS64(13, &m_transverterDeltaFrequency, 0);
        d.readU32(4, &m_rfBandwidth, 2500 * 1000);

		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}


