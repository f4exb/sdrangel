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
#include "limesdrinputsettings.h"

LimeSDRInputSettings::LimeSDRInputSettings()
{
    resetToDefaults();
}

void LimeSDRInputSettings::resetToDefaults()
{
    m_centerFrequency = 435000*1000;
    m_devSampleRate = 5000000;
    m_log2HardDecim = 3;
    m_dcBlock = false;
    m_iqCorrection = false;
    m_log2SoftDecim = 0;
    m_lpfBW = 4.5e6f;
    m_lpfFIREnable = false;
    m_lpfFIRBW = 2.5e6f;
    m_gain = 50;
    m_ncoEnable = false;
    m_ncoFrequency = 0;
    m_antennaPath = PATH_RFE_NONE;
    m_gainMode = GAIN_AUTO;
    m_lnaGain = 15;
    m_tiaGain = 2;
    m_pgaGain = 16;
    m_extClock = false;
    m_extClockFreq = 10000000; // 10 MHz
    m_transverterMode = false;
    m_transverterDeltaFrequency = 0;
    m_iqOrder = true;
    m_gpioDir = 0;
    m_gpioPins = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray LimeSDRInputSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_devSampleRate);
    s.writeU32(2, m_log2HardDecim);
    s.writeBool(3, m_dcBlock);
    s.writeBool(4, m_iqCorrection);
    s.writeU32(5, m_log2SoftDecim);
    s.writeFloat(7, m_lpfBW);
    s.writeBool(8, m_lpfFIREnable);
    s.writeFloat(9, m_lpfFIRBW);
    s.writeU32(10, m_gain);
    s.writeBool(11, m_ncoEnable);
    s.writeS32(12, m_ncoFrequency);
    s.writeS32(13, (int) m_antennaPath);
    s.writeS32(14, (int) m_gainMode);
    s.writeU32(15, m_lnaGain);
    s.writeU32(16, m_tiaGain);
    s.writeU32(17, m_pgaGain);
    s.writeBool(18, m_extClock);
    s.writeU32(19, m_extClockFreq);
    s.writeBool(20, m_transverterMode);
    s.writeS64(21, m_transverterDeltaFrequency);
    s.writeU32(22, m_gpioDir);
    s.writeU32(23, m_gpioPins);
    s.writeBool(24, m_useReverseAPI);
    s.writeString(25, m_reverseAPIAddress);
    s.writeU32(26, m_reverseAPIPort);
    s.writeU32(27, m_reverseAPIDeviceIndex);
    s.writeBool(28, m_iqOrder);

    return s.final();
}

bool LimeSDRInputSettings::deserialize(const QByteArray& data)
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
        d.readU32(2, &m_log2HardDecim, 2);
        d.readBool(3, &m_dcBlock, false);
        d.readBool(4, &m_iqCorrection, false);
        d.readU32(5, &m_log2SoftDecim, 0);
        d.readFloat(7, &m_lpfBW, 1.5e6);
        d.readBool(8, &m_lpfFIREnable, false);
        d.readFloat(9, &m_lpfFIRBW, 1.5e6);
        d.readU32(10, &m_gain, 50);
        d.readBool(11, &m_ncoEnable, false);
        d.readS32(12, &m_ncoFrequency, 0);
        d.readS32(13, &intval, 0);
        m_antennaPath = (PathRFE) intval;
        d.readS32(14, &intval, 0);
        m_gainMode = (GainMode) intval;
        d.readU32(15, &m_lnaGain, 15);
        d.readU32(16, &m_tiaGain, 2);
        d.readU32(17, &m_pgaGain, 16);
        d.readBool(18, &m_extClock, false);
        d.readU32(19, &m_extClockFreq, 10000000);
        d.readBool(20, &m_transverterMode, false);
        d.readS64(21, &m_transverterDeltaFrequency, 0);
        d.readU32(22, &uintval, 0);
        m_gpioDir = uintval & 0xFF;
        d.readU32(23, &uintval, 0);
        m_gpioPins = uintval & 0xFF;
        d.readBool(24, &m_useReverseAPI, false);
        d.readString(25, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(26, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(27, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;
        d.readBool(28, &m_iqOrder, true);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }

}


