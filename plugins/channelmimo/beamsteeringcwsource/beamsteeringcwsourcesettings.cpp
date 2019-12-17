///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
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

#include "beamsteeringcwsourcesettings.h"

#include <QColor>

#include "util/simpleserializer.h"
#include "settings/serializable.h"


BeamSteeringCWSourceSettings::BeamSteeringCWSourceSettings()
{
    resetToDefaults();
}

void BeamSteeringCWSourceSettings::resetToDefaults()
{
    m_steerDegrees = 90;
    m_rgbColor = QColor(140, 4, 4).rgb();
    m_title = "Local Source";
    m_log2Interp = 0;
    m_filterChainHash = 0;
    m_channelMarker = nullptr;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
}

QByteArray BeamSteeringCWSourceSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_steerDegrees);
    s.writeU32(5, m_rgbColor);
    s.writeString(6, m_title);
    s.writeBool(7, m_useReverseAPI);
    s.writeString(8, m_reverseAPIAddress);
    s.writeU32(9, m_reverseAPIPort);
    s.writeU32(10, m_reverseAPIDeviceIndex);
    s.writeU32(11, m_reverseAPIChannelIndex);
    s.writeU32(12, m_log2Interp);
    s.writeU32(13, m_filterChainHash);

    return s.final();
}

bool BeamSteeringCWSourceSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if(!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1)
    {
        uint32_t tmp;
        int stmp;
        QString strtmp;

        d.readS32(1, &stmp, 90);
        m_steerDegrees = stmp < 0 ? -0 : stmp > 180 ? 180 : stmp;
        d.readU32(5, &m_rgbColor, QColor(0, 255, 255).rgb());
        d.readString(6, &m_title, "Local sink");
        d.readBool(7, &m_useReverseAPI, false);
        d.readString(8, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(9, &tmp, 0);

        if ((tmp > 1023) && (tmp < 65535)) {
            m_reverseAPIPort = tmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(10, &tmp, 0);
        m_reverseAPIDeviceIndex = tmp > 99 ? 99 : tmp;
        d.readU32(11, &tmp, 0);
        m_reverseAPIChannelIndex = tmp > 99 ? 99 : tmp;
        d.readU32(12, &tmp, 0);
        m_log2Interp = tmp > 6 ? 6 : tmp;
        d.readU32(13, &m_filterChainHash, 0);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}





