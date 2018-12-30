///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Sergey Kostanbaev, Fairwaves Inc.                          //
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

#include "util/simpleserializer.h"
#include "xtrxinputsettings.h"

XTRXInputSettings::XTRXInputSettings()
{
    resetToDefaults();
}

void XTRXInputSettings::resetToDefaults()
{
    m_centerFrequency = 435000*1000;
    m_devSampleRate = 5e6;
    m_log2HardDecim = 1;
    m_dcBlock = false;
    m_iqCorrection = false;
    m_log2SoftDecim = 0;
    m_lpfBW = 4.5e6f;
    m_gain = 50;
    m_ncoEnable = false;
    m_ncoFrequency = 0;
    m_antennaPath = XTRX_RX_L;
    m_gainMode = GAIN_AUTO;
    m_lnaGain = 15;
    m_tiaGain = 2;
    m_pgaGain = 16;
    m_extClock = false;
    m_extClockFreq = 0; // Auto
    m_pwrmode = 1;
}

QByteArray XTRXInputSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeDouble(1, m_devSampleRate);
    s.writeU32(2, m_log2HardDecim);
    s.writeBool(3, m_dcBlock);
    s.writeBool(4, m_iqCorrection);
    s.writeU32(5, m_log2SoftDecim);
    s.writeFloat(7, m_lpfBW);
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
    s.writeU32(20, m_pwrmode);

    return s.final();
}

bool XTRXInputSettings::deserialize(const QByteArray& data)
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

        d.readDouble(1, &m_devSampleRate, 5e6);
        d.readU32(2, &m_log2HardDecim, 2);
        d.readBool(3, &m_dcBlock, false);
        d.readBool(4, &m_iqCorrection, false);
        d.readU32(5, &m_log2SoftDecim, 0);
        d.readFloat(7, &m_lpfBW, 1.5e6);
        d.readU32(10, &m_gain, 50);
        d.readBool(11, &m_ncoEnable, false);
        d.readS32(12, &m_ncoFrequency, 0);
        d.readS32(13, &intval, 0);
        m_antennaPath = (xtrx_antenna_t) intval;
        d.readS32(14, &intval, 0);
        m_gainMode = (GainMode) intval;
        d.readU32(15, &m_lnaGain, 15);
        d.readU32(16, &m_tiaGain, 2);
        d.readU32(17, &m_pgaGain, 16);
        d.readBool(18, &m_extClock, false);
        d.readU32(19, &m_extClockFreq, 0);
        d.readU32(20, &m_pwrmode, 2);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }

}
