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
#include "fcdprosettings.h"

FCDProSettings::FCDProSettings()
{
	resetToDefaults();
}

void FCDProSettings::resetToDefaults()
{
	m_centerFrequency = 435000 * 1000;
	m_dcBlock = false;
	m_iqCorrection = false;
	m_LOppmTenths = 0;
	m_lnaGainIndex = 0;
	m_rfFilterIndex = 0;
	m_lnaEnhanceIndex = 0;
	m_bandIndex = 0;
	m_mixerGainIndex = 0;
	m_mixerFilterIndex = 0;
	m_biasCurrentIndex = 0;
	m_modeIndex = 0;
	m_gain1Index = 0;
	m_rcFilterIndex = 0;
	m_gain2Index = 0;
	m_gain3Index = 0;
	m_gain4Index = 0;
	m_ifFilterIndex = 0;
	m_gain5Index = 0;
	m_gain6Index = 0;
    m_transverterMode = false;
    m_transverterDeltaFrequency = 0;
}

QByteArray FCDProSettings::serialize() const
{
	SimpleSerializer s(1);

	s.writeBool(1, m_dcBlock);
	s.writeBool(2, m_iqCorrection);
	s.writeS32(3, m_LOppmTenths);
	s.writeS32(4, m_lnaGainIndex);
	s.writeS32(5, m_rfFilterIndex);
	s.writeS32(6, m_lnaEnhanceIndex);
	s.writeS32(7, m_bandIndex);
	s.writeS32(8, m_mixerGainIndex);
	s.writeS32(9, m_mixerFilterIndex);
	s.writeS32(10, m_biasCurrentIndex);
	s.writeS32(11, m_modeIndex);
	s.writeS32(12, m_gain1Index);
	s.writeS32(13, m_rcFilterIndex);
	s.writeS32(14, m_gain2Index);
	s.writeS32(15, m_gain3Index);
	s.writeS32(16, m_gain4Index);
	s.writeS32(17, m_ifFilterIndex);
	s.writeS32(18, m_gain5Index);
	s.writeS32(19, m_gain6Index);
    s.writeBool(20, m_transverterMode);
    s.writeS64(21, m_transverterDeltaFrequency);

	return s.final();
}

bool FCDProSettings::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if (!d.isValid())
	{
		resetToDefaults();
		return false;
	}

	if (d.getVersion() == 1)
	{
		d.readBool(1, &m_dcBlock, false);
		d.readBool(2, &m_iqCorrection, false);
		d.readS32(3, &m_LOppmTenths, 0);
		d.readS32(4, &m_lnaGainIndex, 0);
		d.readS32(5, &m_rfFilterIndex, 0);
		d.readS32(6, &m_lnaEnhanceIndex, 0);
		d.readS32(7, &m_bandIndex, 0);
		d.readS32(8, &m_mixerGainIndex, 0);
		d.readS32(9, &m_mixerFilterIndex, 0);
		d.readS32(10, &m_biasCurrentIndex, 0);
		d.readS32(11, &m_modeIndex, 0);
		d.readS32(12, &m_gain1Index, 0);
		d.readS32(13, &m_rcFilterIndex, 0);
		d.readS32(14, &m_gain2Index, 0);
		d.readS32(15, &m_gain3Index, 0);
		d.readS32(16, &m_gain4Index, 0);
		d.readS32(17, &m_ifFilterIndex, 0);
		d.readS32(18, &m_gain5Index, 0);
		d.readS32(19, &m_gain6Index, 0);
        d.readBool(20, &m_transverterMode, false);
        d.readS64(21, &m_transverterDeltaFrequency, 0);

		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}
