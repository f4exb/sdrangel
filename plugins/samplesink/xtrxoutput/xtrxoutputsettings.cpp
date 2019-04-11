///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include "../xtrxoutput/xtrxoutputsettings.h"

#include "util/simpleserializer.h"

XTRXOutputSettings::XTRXOutputSettings()
{
    resetToDefaults();
}

void XTRXOutputSettings::resetToDefaults()
{
    m_centerFrequency = 435000*1000;
    m_devSampleRate = 5e6;
    m_log2HardInterp = 2;
    m_log2SoftInterp = 4;
    m_lpfBW = 4.5e6f;
    m_gain = 20;
    m_ncoEnable = true;
    m_ncoFrequency = 500000;
    m_antennaPath = XTRX_TX_W;
    m_extClock = false;
    m_extClockFreq = 0; // Auto
    m_pwrmode = 1;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray XTRXOutputSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeDouble(1, m_devSampleRate);
    s.writeU32(2, m_log2HardInterp);
    s.writeU32(3, m_log2SoftInterp);
    s.writeFloat(4, m_lpfBW);
    s.writeU32(5, m_gain);
    s.writeBool(6, m_ncoEnable);
    s.writeS32(7, m_ncoFrequency);
    s.writeS32(8, (int) m_antennaPath);
    s.writeBool(9, m_extClock);
    s.writeU32(10, m_extClockFreq);
    s.writeU32(11, m_pwrmode);
    s.writeBool(12, m_useReverseAPI);
    s.writeString(13, m_reverseAPIAddress);
    s.writeU32(14, m_reverseAPIPort);
    s.writeU32(15, m_reverseAPIDeviceIndex);

    return s.final();
}

bool XTRXOutputSettings::deserialize(const QByteArray& data)
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

        d.readDouble(1, &m_devSampleRate, 5e6);
        d.readU32(2, &m_log2HardInterp, 2);
        d.readU32(3, &m_log2SoftInterp, 0);
        d.readFloat(4, &m_lpfBW, 1.5e6);
        d.readU32(5, &m_gain, 20);
        d.readBool(6, &m_ncoEnable, true);
        d.readS32(7, &m_ncoFrequency, 500000);
        d.readS32(8, &intval, 0);
        m_antennaPath = (xtrx_antenna_t) intval;
        d.readBool(9, &m_extClock, false);
        d.readU32(10, &m_extClockFreq, 0);
        d.readU32(11, &m_pwrmode, 2);
        d.readBool(12, &m_useReverseAPI, false);
        d.readString(13, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(14, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(15, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }

}
