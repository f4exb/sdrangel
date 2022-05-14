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
#include <QDataStream>

#include "util/simpleserializer.h"
#include "settings/serializable.h"

#include "aissettings.h"

const QStringList AISSettings::m_pipeTypes = {
    QStringLiteral("AISDemod")
};

const QStringList AISSettings::m_pipeURIs = {
    QStringLiteral("sdrangel.channel.aisdemod")
};

AISSettings::AISSettings() :
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void AISSettings::resetToDefaults()
{
    m_title = "AIS";
    m_rgbColor = QColor(102, 0, 0).rgb();
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIFeatureSetIndex = 0;
    m_reverseAPIFeatureIndex = 0;
    m_workspaceIndex = 0;

    for (int i = 0; i < AIS_VESSEL_COLUMNS; i++)
    {
        m_vesselColumnIndexes[i] = i;
        m_vesselColumnSizes[i] = -1; // Autosize
    }
}

QByteArray AISSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(20, m_title);
    s.writeU32(21, m_rgbColor);
    s.writeBool(22, m_useReverseAPI);
    s.writeString(23, m_reverseAPIAddress);
    s.writeU32(24, m_reverseAPIPort);
    s.writeU32(25, m_reverseAPIFeatureSetIndex);
    s.writeU32(26, m_reverseAPIFeatureIndex);

    if (m_rollupState) {
        s.writeBlob(27, m_rollupState->serialize());
    }

    s.writeS32(28, m_workspaceIndex);
    s.writeBlob(29, m_geometryBytes);

    for (int i = 0; i < AIS_VESSEL_COLUMNS; i++) {
        s.writeS32(300 + i, m_vesselColumnIndexes[i]);
    }

    for (int i = 0; i < AIS_VESSEL_COLUMNS; i++) {
        s.writeS32(400 + i, m_vesselColumnSizes[i]);
    }

    return s.final();
}

bool AISSettings::deserialize(const QByteArray& data)
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

        d.readString(20, &m_title, "AIS");
        d.readU32(21, &m_rgbColor, QColor(102, 0, 0).rgb());
        d.readBool(22, &m_useReverseAPI, false);
        d.readString(23, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(24, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(25, &utmp, 0);
        m_reverseAPIFeatureSetIndex = utmp > 99 ? 99 : utmp;
        d.readU32(26, &utmp, 0);
        m_reverseAPIFeatureIndex = utmp > 99 ? 99 : utmp;

        if (m_rollupState)
        {
            d.readBlob(27, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(28, &m_workspaceIndex, 0);
        d.readBlob(29, &m_geometryBytes);

        for (int i = 0; i < AIS_VESSEL_COLUMNS; i++) {
            d.readS32(300 + i, &m_vesselColumnIndexes[i], i);
        }

        for (int i = 0; i < AIS_VESSEL_COLUMNS; i++) {
            d.readS32(400 + i, &m_vesselColumnSizes[i], -1);
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
