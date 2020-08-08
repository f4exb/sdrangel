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

#include "perseussettings.h"
#include "util/simpleserializer.h"


PerseusSettings::PerseusSettings()
{
    resetToDefaults();
}

void PerseusSettings::resetToDefaults()
{
    m_centerFrequency = 7150*1000;
    m_LOppmTenths = 0;
    m_devSampleRateIndex = 0;
    m_log2Decim = 0;
    m_transverterMode = false;
    m_transverterDeltaFrequency = 0;
    m_iqOrder = true;
    m_adcDither = false;
    m_adcPreamp = false;
    m_wideBand = false;
    m_attenuator = Attenuator_None;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray PerseusSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeU32(1, m_devSampleRateIndex);
    s.writeS32(2, m_LOppmTenths);
    s.writeU32(3, m_log2Decim);
    s.writeBool(4, m_transverterMode);
    s.writeS64(5, m_transverterDeltaFrequency);
    s.writeBool(6, m_adcDither);
    s.writeBool(7, m_adcPreamp);
    s.writeBool(8, m_wideBand);
    s.writeS32(9, (int) m_attenuator);
    s.writeBool(10, m_useReverseAPI);
    s.writeString(11, m_reverseAPIAddress);
    s.writeU32(12, m_reverseAPIPort);
    s.writeU32(13, m_reverseAPIDeviceIndex);
    s.writeBool(14, m_iqOrder);

    return s.final();
}

bool PerseusSettings::deserialize(const QByteArray& data)
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

        d.readU32(1, &m_devSampleRateIndex, 0);
        d.readS32(2, &m_LOppmTenths, 0);
        d.readU32(3, &m_log2Decim, 0);
        d.readBool(4, &m_transverterMode, false);
        d.readS64(5, &m_transverterDeltaFrequency, 0);
        d.readBool(6, &m_adcDither, false);
        d.readBool(7, &m_adcPreamp, false);
        d.readBool(8, &m_wideBand, false);
        d.readS32(9, &intval, 0);

        if ((intval >= 0) && (intval < (int) Attenuator_last)) {
            m_attenuator = (Attenuator) intval;
        } else {
            m_attenuator = Attenuator_None;
        }

        d.readBool(10, &m_useReverseAPI, false);
        d.readString(11, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(12, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(13, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;
        d.readBool(14, &m_iqOrder, true);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

