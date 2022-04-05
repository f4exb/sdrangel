///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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
#include <QDataStream>

#include "util/simpleserializer.h"
#include "settings/serializable.h"

#include "radiosondesettings.h"

const QStringList RadiosondeSettings::m_pipeTypes = {
    QStringLiteral("RadiosondeDemod")
};

const QStringList RadiosondeSettings::m_pipeURIs = {
    QStringLiteral("sdrangel.channel.radiosondedemod")
};

RadiosondeSettings::RadiosondeSettings() :
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void RadiosondeSettings::resetToDefaults()
{
    m_title = "Radiosonde";
    m_rgbColor = QColor(102, 0, 102).rgb();
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIFeatureSetIndex = 0;
    m_reverseAPIFeatureIndex = 0;

    m_y1 = ALTITUDE;
    m_y2 = TEMPERATURE;

    for (int i = 0; i < RADIOSONDES_COLUMNS; i++)
    {
        m_radiosondesColumnIndexes[i] = i;
        m_radiosondesColumnSizes[i] = -1; // Autosize
    }
}

QByteArray RadiosondeSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(1, m_title);
    s.writeU32(2, m_rgbColor);
    s.writeBool(3, m_useReverseAPI);
    s.writeString(4, m_reverseAPIAddress);
    s.writeU32(5, m_reverseAPIPort);
    s.writeU32(6, m_reverseAPIFeatureSetIndex);
    s.writeU32(7, m_reverseAPIFeatureIndex);

    if (m_rollupState) {
        s.writeBlob(8, m_rollupState->serialize());
    }

    s.writeS32(10, (int)m_y1);
    s.writeS32(11, (int)m_y2);
    s.writeS32(12, m_workspaceIndex);
    s.writeBlob(13, m_geometryBytes);

    for (int i = 0; i < RADIOSONDES_COLUMNS; i++) {
        s.writeS32(300 + i, m_radiosondesColumnIndexes[i]);
    }

    for (int i = 0; i < RADIOSONDES_COLUMNS; i++) {
        s.writeS32(400 + i, m_radiosondesColumnSizes[i]);
    }

    return s.final();
}

bool RadiosondeSettings::deserialize(const QByteArray& data)
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
        QByteArray blob;

        d.readString(1, &m_title, "Radiosonde");
        d.readU32(2, &m_rgbColor, QColor(102, 0, 102).rgb());
        d.readBool(3, &m_useReverseAPI, false);
        d.readString(4, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(5, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(6, &utmp, 0);
        m_reverseAPIFeatureSetIndex = utmp > 99 ? 99 : utmp;
        d.readU32(7, &utmp, 0);
        m_reverseAPIFeatureIndex = utmp > 99 ? 99 : utmp;

        if (m_rollupState)
        {
            d.readBlob(8, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(10, (int *)&m_y1, (int)ALTITUDE);
        d.readS32(11, (int *)&m_y2, (int)TEMPERATURE);
        d.readS32(12, &m_workspaceIndex, 0);
        d.readBlob(13, &m_geometryBytes);

        for (int i = 0; i < RADIOSONDES_COLUMNS; i++) {
            d.readS32(300 + i, &m_radiosondesColumnIndexes[i], i);
        }

        for (int i = 0; i < RADIOSONDES_COLUMNS; i++) {
            d.readS32(400 + i, &m_radiosondesColumnSizes[i], -1);
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
