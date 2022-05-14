///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include <QColor>

#include "util/simpleserializer.h"
#include "settings/serializable.h"

#include "rigctlserversettings.h"

MESSAGE_CLASS_DEFINITION(RigCtlServerSettings::MsgChannelIndexChange, Message)

RigCtlServerSettings::RigCtlServerSettings() :
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void RigCtlServerSettings::resetToDefaults()
{
    m_rigCtlPort = 4532;
    m_maxFrequencyOffset = 10000;
    m_deviceIndex = -1;
    m_channelIndex = -1;
    m_title = "RigCtl Server";
    m_rgbColor = QColor(225, 25, 99).rgb();
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIFeatureSetIndex = 0;
    m_reverseAPIFeatureIndex = 0;
    m_workspaceIndex = 0;
}

QByteArray RigCtlServerSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeU32(1, m_rigCtlPort);
    s.writeS32(2, m_maxFrequencyOffset);
    s.writeS32(3, m_deviceIndex);
    s.writeS32(4, m_channelIndex);
    s.writeString(5, m_title);
    s.writeU32(6, m_rgbColor);
    s.writeBool(7, m_useReverseAPI);
    s.writeString(8, m_reverseAPIAddress);
    s.writeU32(9, m_reverseAPIPort);
    s.writeU32(10, m_reverseAPIFeatureSetIndex);
    s.writeU32(11, m_reverseAPIFeatureIndex);

    if (m_rollupState) {
        s.writeBlob(12, m_rollupState->serialize());
    }

    s.writeS32(13, m_workspaceIndex);
    s.writeBlob(14, m_geometryBytes);

    return s.final();
}

bool RigCtlServerSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray bytetmp;
        uint32_t utmp;
        QString strtmp;

        d.readU32(2, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_rigCtlPort = utmp;
        } else {
            m_rigCtlPort = 4532;
        }

        d.readS32(2, &m_maxFrequencyOffset, 10000);
        d.readS32(3, &m_deviceIndex, -1);
        d.readS32(4, &m_channelIndex, -1);
        d.readString(5, &m_title, "RigCtl Server");
        d.readU32(6, &m_rgbColor, QColor(225, 25, 99).rgb());
        d.readBool(7, &m_useReverseAPI, false);
        d.readString(8, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(9, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(10, &utmp, 0);
        m_reverseAPIFeatureSetIndex = utmp > 99 ? 99 : utmp;
        d.readU32(11, &utmp, 0);
        m_reverseAPIFeatureIndex = utmp > 99 ? 99 : utmp;

        if (m_rollupState)
        {
            d.readBlob(12, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(13, &m_workspaceIndex, 0);
        d.readBlob(14, &m_geometryBytes);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
