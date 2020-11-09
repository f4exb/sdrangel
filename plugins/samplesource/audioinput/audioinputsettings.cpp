///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#include <QtGlobal>
#include "util/simpleserializer.h"
#include "audioinputsettings.h"

AudioInputSettings::AudioInputSettings()
{
    resetToDefaults();
}

void AudioInputSettings::resetToDefaults()
{
    m_deviceName = "";
    m_sampleRate = 48000;
    m_volume = 1.0f;
    m_log2Decim = 0;
    m_iqMapping = L;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray AudioInputSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(1, m_deviceName);
    s.writeS32(2, m_sampleRate);
    s.writeFloat(3, m_volume);
    s.writeU32(4, m_log2Decim);
    s.writeS32(5, (int)m_iqMapping);

    s.writeBool(24, m_useReverseAPI);
    s.writeString(25, m_reverseAPIAddress);
    s.writeU32(26, m_reverseAPIPort);
    s.writeU32(27, m_reverseAPIDeviceIndex);

    return s.final();
}

bool AudioInputSettings::deserialize(const QByteArray& data)
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

        d.readString(1, &m_deviceName, "");
        d.readS32(2, &m_sampleRate, 48000);
        d.readFloat(3, &m_volume, 1.0f);
        d.readU32(4, &m_log2Decim, 0);
        d.readS32(5, (int *)&m_iqMapping, IQMapping::L);

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

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
