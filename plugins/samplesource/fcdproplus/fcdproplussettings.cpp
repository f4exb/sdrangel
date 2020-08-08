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
	m_mixGain = 0;
	m_rfFilterIndex = 0;
	m_ifFilterIndex = 0;
	m_LOppmTenths = 0;
	m_log2Decim = 0;
	m_fcPos = FC_POS_CENTER;
	m_dcBlock = false;
	m_iqImbalance = false;
    m_transverterMode = false;
    m_transverterDeltaFrequency = 0;
    m_iqOrder = true;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray FCDProPlusSettings::serialize() const
{
	SimpleSerializer s(1);

	s.writeBool(1, m_biasT);
	s.writeBool(2, m_rangeLow);
	s.writeBool(3, m_mixGain);
	s.writeS32(4, m_ifFilterIndex);
	s.writeS32(5, m_rfFilterIndex);
	s.writeU32(6, m_log2Decim);
	s.writeS32(7, (int) m_fcPos);
	s.writeBool(8, m_dcBlock);
	s.writeBool(9, m_iqImbalance);
	s.writeS32(10, m_LOppmTenths);
	s.writeU32(11, m_ifGain);
    s.writeBool(12, m_transverterMode);
    s.writeS64(13, m_transverterDeltaFrequency);
    s.writeBool(14, m_useReverseAPI);
    s.writeString(15, m_reverseAPIAddress);
    s.writeU32(16, m_reverseAPIPort);
    s.writeU32(17, m_reverseAPIDeviceIndex);
    s.writeBool(18, m_iqOrder);

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
		int intval;
		uint32_t uintval;

		d.readBool(1, &m_biasT, false);
		d.readBool(2, &m_rangeLow, false);
		d.readBool(3, &m_mixGain, true);
		d.readS32(4, &m_ifFilterIndex, 0);
		d.readS32(5, &m_rfFilterIndex, 0);
		d.readU32(6, &m_log2Decim, 0);
		d.readS32(7, &intval, 2);
		m_fcPos = (fcPos_t) intval;
		d.readBool(8, &m_dcBlock, false);
		d.readBool(9, &m_iqImbalance, false);
		d.readS32(10, &m_LOppmTenths, 0);
		d.readU32(11, &m_ifGain, 0);
        d.readBool(12, &m_transverterMode, false);
        d.readS64(13, &m_transverterDeltaFrequency, 0);
        d.readBool(14, &m_useReverseAPI, false);
        d.readString(15, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(16, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(17, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;
        d.readBool(18, &m_iqOrder, true);

		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}

