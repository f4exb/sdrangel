///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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
	m_biasT = false;
	m_log2Interp = 0;
    m_fcPos = FC_POS_CENTER;
	m_lnaExt = false;
	m_vgaGain = 22;
	m_bandwidth = 1750000;
	m_devSampleRate = 2400000;
    m_transverterMode = false;
	m_transverterDeltaFrequency = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray HackRFOutputSettings::serialize() const
{
	SimpleSerializer s(1);

	s.writeS32(1, m_LOppmTenths);
    s.writeS32(2, (int) m_fcPos);
	s.writeBool(3, m_biasT);
	s.writeU32(4, m_log2Interp);
	s.writeBool(5, m_lnaExt);
	s.writeU32(6, m_vgaGain);
	s.writeU32(7, m_bandwidth);
	s.writeU64(8, m_devSampleRate);
    s.writeBool(9, m_useReverseAPI);
    s.writeString(10, m_reverseAPIAddress);
    s.writeU32(11, m_reverseAPIPort);
    s.writeU32(12, m_reverseAPIDeviceIndex);
    s.writeBool(13, m_transverterMode);
    s.writeS64(14, m_transverterDeltaFrequency);

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
		uint32_t uintval;
        int32_t intval;

		d.readS32(1, &m_LOppmTenths, 0);
        d.readS32(2, &intval, 2);
        m_fcPos = (fcPos_t) (intval < 0 ? 0 : intval > 2 ? 2 : intval);
		d.readBool(3, &m_biasT, false);
		d.readU32(4, &m_log2Interp, 0);
		d.readBool(5, &m_lnaExt, false);
		d.readU32(6, &m_vgaGain, 30);
		d.readU32(7, &m_bandwidth, 1750000);
		d.readU64(8, &m_devSampleRate, 2400000);
        d.readBool(9, &m_useReverseAPI, false);
        d.readString(10, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(11, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(12, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;
        d.readBool(13, &m_transverterMode, false);
        d.readS64(14, &m_transverterDeltaFrequency, 0);

		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}
