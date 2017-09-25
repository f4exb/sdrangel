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
#include "fcdproplussettings.h"

FCDProPlusSettings::FCDProPlusSettings()
{
	resetToDefaults();
}

void FCDProPlusSettings::resetToDefaults()
{
	m_centerFrequency = 435000000;
	m_rangeLow = true;
	m_lnaGain = true;
	m_biasT = false;
	m_ifGain = 0;
	m_rfFilterIndex = 0;
	m_ifFilterIndex = 0;
	m_LOppmTenths = 0;
	m_dcBlock = false;
	m_iqImbalance = false;
    m_transverterMode = false;
    m_transverterDeltaFrequency = 0;
}

QByteArray FCDProPlusSettings::serialize() const
{
	SimpleSerializer s(1);

	s.writeBool(1, m_biasT);
	s.writeBool(2, m_rangeLow);
	s.writeBool(3, m_mixGain);
	s.writeS32(4, m_ifFilterIndex);
	s.writeS32(5, m_rfFilterIndex);
	s.writeBool(6, m_dcBlock);
	s.writeBool(7, m_iqImbalance);
	s.writeS32(8, m_LOppmTenths);
	s.writeU32(9, m_ifGain);
    s.writeBool(10, m_transverterMode);
    s.writeS64(11, m_transverterDeltaFrequency);

	return s.final();
}

bool FCDProPlusSettings::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if (!d.isValid())
	{
		resetToDefaults();
		return false;
	}

	if (d.getVersion() == 1)
	{
		d.readBool(1, &m_biasT, false);
		d.readBool(2, &m_rangeLow, false);
		d.readBool(3, &m_mixGain, true);
		d.readS32(4, &m_ifFilterIndex, 0);
		d.readS32(5, &m_rfFilterIndex, 0);
		d.readBool(6, &m_dcBlock, false);
		d.readBool(7, &m_iqImbalance, false);
		d.readS32(8, &m_LOppmTenths, 0);
		d.readU32(9, &m_ifGain, 0);
        d.readBool(10, &m_transverterMode, false);
        d.readS64(11, &m_transverterDeltaFrequency, 0);

		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}

