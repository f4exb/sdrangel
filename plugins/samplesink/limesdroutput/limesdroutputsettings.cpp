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

#include "util/simpleserializer.h"

#include "limesdroutputsettings.h"

LimeSDROutputSettings::LimeSDROutputSettings()
{
    resetToDefaults();
}

void LimeSDROutputSettings::resetToDefaults()
{
    m_centerFrequency = 435000*1000;
    m_devSampleRate = 5000000;
    m_log2HardInterp = 3;
    m_log2SoftInterp = 0;
    m_lpfBW = 5.5e6f;
    m_lpfFIREnable = false;
    m_lpfFIRBW = 2.5e6f;
    m_gain = 4;
    m_ncoEnable = false;
    m_ncoFrequency = 0;
    m_antennaPath = PATH_RFE_NONE;
    m_extClock = false;
    m_extClockFreq = 10000000; // 10 MHz
    m_transverterMode = false;
    m_transverterDeltaFrequency = 0;
    m_gpioDir = 0;
    m_gpioPins = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray LimeSDROutputSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_devSampleRate);
    s.writeU32(2, m_log2HardInterp);
    s.writeU32(5, m_log2SoftInterp);
    s.writeFloat(7, m_lpfBW);
    s.writeBool(8, m_lpfFIREnable);
    s.writeFloat(9, m_lpfFIRBW);
    s.writeU32(10, m_gain);
    s.writeBool(11, m_ncoEnable);
    s.writeS32(12, m_ncoFrequency);
    s.writeS32(13, (int) m_antennaPath);
    s.writeBool(14, m_extClock);
    s.writeU32(15, m_extClockFreq);
    s.writeBool(16, m_transverterMode);
    s.writeS64(17, m_transverterDeltaFrequency);
    s.writeU32(18, m_gpioDir);
    s.writeU32(19, m_gpioPins);
    s.writeBool(20, m_useReverseAPI);
    s.writeString(21, m_reverseAPIAddress);
    s.writeU32(22, m_reverseAPIPort);
    s.writeU32(23, m_reverseAPIDeviceIndex);

    return s.final();
}

bool LimeSDROutputSettings::deserialize(const QByteArray& data)
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

        d.readS32(1, &m_devSampleRate, 5000000);
        d.readU32(2, &m_log2HardInterp, 2);
        d.readU32(5, &m_log2SoftInterp, 0);
        d.readFloat(7, &m_lpfBW, 1.5e6);
        d.readBool(8, &m_lpfFIREnable, false);
        d.readFloat(9, &m_lpfFIRBW, 1.5e6);
        d.readU32(10, &m_gain, 4);
        d.readBool(11, &m_ncoEnable, false);
        d.readS32(12, &m_ncoFrequency, 0);
        d.readS32(13, &intval, 0);
        m_antennaPath = (PathRFE) intval;
        d.readBool(14, &m_extClock, false);
        d.readU32(15, &m_extClockFreq, 10000000);
        d.readBool(16, &m_transverterMode, false);
        d.readS64(17, &m_transverterDeltaFrequency, 0);
        d.readU32(18, &uintval, 0);
        m_gpioDir = uintval & 0xFF;
        d.readU32(19, &uintval, 0);
        m_gpioPins = uintval & 0xFF;
        d.readBool(20, &m_useReverseAPI, false);
        d.readString(21, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(22, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(23, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }

}


