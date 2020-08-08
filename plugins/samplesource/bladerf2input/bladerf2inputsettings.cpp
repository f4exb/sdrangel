///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include "bladerf2inputsettings.h"

#include "util/simpleserializer.h"

BladeRF2InputSettings::BladeRF2InputSettings()
{
    resetToDefaults();
}

void BladeRF2InputSettings::resetToDefaults()
{
    m_centerFrequency = 435000*1000;
    m_LOppmTenths = 0;
    m_devSampleRate = 3072000;
    m_bandwidth = 1500000;
    m_gainMode = 0;
    m_globalGain = 0;
    m_biasTee = false;
    m_log2Decim = 0;
    m_fcPos = FC_POS_INFRA;
    m_dcBlock = false;
    m_iqCorrection = false;
    m_transverterMode = false;
    m_transverterDeltaFrequency = 0;
    m_iqOrder = true;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray BladeRF2InputSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_devSampleRate);
    s.writeS32(2, m_bandwidth);
    s.writeS32(3, m_gainMode);
    s.writeS32(4, m_globalGain);
    s.writeBool(5, m_biasTee);
    s.writeU32(6, m_log2Decim);
    s.writeS32(7, (int) m_fcPos);
    s.writeBool(8, m_dcBlock);
    s.writeBool(9, m_iqCorrection);
    s.writeS32(10, m_LOppmTenths);
    s.writeBool(11, m_transverterMode);
    s.writeS64(12, m_transverterDeltaFrequency);
    s.writeBool(13, m_useReverseAPI);
    s.writeString(14, m_reverseAPIAddress);
    s.writeU32(15, m_reverseAPIPort);
    s.writeU32(16, m_reverseAPIDeviceIndex);
    s.writeBool(17, m_iqOrder);

    return s.final();
}

bool BladeRF2InputSettings::deserialize(const QByteArray& data)
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

        d.readS32(1, &m_devSampleRate, 3072000);
        d.readS32(2, &m_bandwidth);
        d.readS32(3, &m_gainMode);
        d.readS32(4, &m_globalGain);
        d.readBool(5, &m_biasTee);
        d.readU32(6, &m_log2Decim);
        d.readS32(7, &intval);
        m_fcPos = (fcPos_t) intval;
        d.readBool(8, &m_dcBlock);
        d.readBool(9, &m_iqCorrection);
        d.readS32(10, &m_LOppmTenths);
        d.readBool(11, &m_transverterMode, false);
        d.readS64(12, &m_transverterDeltaFrequency, 0);
        d.readBool(13, &m_useReverseAPI, false);
        d.readString(14, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(15, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(16, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;
        d.readBool(17, &m_iqOrder, true);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}


