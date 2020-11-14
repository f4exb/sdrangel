///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include "util/simpleserializer.h"

#include "usrpoutputsettings.h"

USRPOutputSettings::USRPOutputSettings()
{
    resetToDefaults();
}

void USRPOutputSettings::resetToDefaults()
{
    m_masterClockRate = -1; // Calculated by UHD
    m_centerFrequency = 435000*1000;
    m_devSampleRate = 3000000;
    m_loOffset = 0;
    m_log2SoftInterp = 0;
    m_lpfBW = 10e6f;
    m_gain = 50;
    m_antennaPath = "TX/RX";
    m_clockSource = "internal";
    m_transverterMode = false;
    m_transverterDeltaFrequency = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray USRPOutputSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_devSampleRate);
    s.writeU32(2, m_log2SoftInterp);
    s.writeFloat(3, m_lpfBW);
    s.writeU32(4, m_gain);
    s.writeString(5, m_antennaPath);
    s.writeString(6, m_clockSource);
    s.writeBool(7, m_transverterMode);
    s.writeS64(8, m_transverterDeltaFrequency);
    s.writeBool(9, m_useReverseAPI);
    s.writeString(10, m_reverseAPIAddress);
    s.writeU32(11, m_reverseAPIPort);
    s.writeU32(12, m_reverseAPIDeviceIndex);
    s.writeS32(13, m_loOffset);

    return s.final();
}

bool USRPOutputSettings::deserialize(const QByteArray& data)
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

        d.readS32(1, &m_devSampleRate, 5000000);
        d.readU32(2, &m_log2SoftInterp, 0);
        d.readFloat(3, &m_lpfBW, 1.5e6);
        d.readU32(4, &m_gain, 4);
        d.readString(5, &m_antennaPath, "TX/RX");
        d.readString(6, &m_clockSource, "internal");
        d.readBool(7, &m_transverterMode, false);
        d.readS64(8, &m_transverterDeltaFrequency, 0);
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
        d.readS32(13, &m_loOffset, 0);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }

}


