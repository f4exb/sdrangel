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
	m_autoBBF = true;
	m_devSampleRate = 2400000;
    m_transverterMode = false;
	m_transverterDeltaFrequency = 0;
    m_iqOrder = true;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
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
    s.writeBool(14, m_useReverseAPI);
    s.writeString(15, m_reverseAPIAddress);
    s.writeU32(16, m_reverseAPIPort);
    s.writeU32(17, m_reverseAPIDeviceIndex);
    s.writeBool(18, m_transverterMode);
    s.writeS64(19, m_transverterDeltaFrequency);
    s.writeBool(20, m_iqOrder);
    s.writeBool(21, m_autoBBF);

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
		uint32_t uintval;

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
        d.readBool(18, &m_transverterMode, false);
        d.readS64(19, &m_transverterDeltaFrequency, 0);
        d.readBool(20, &m_iqOrder, true);
        d.readBool(21, &m_autoBBF, true);

		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}
