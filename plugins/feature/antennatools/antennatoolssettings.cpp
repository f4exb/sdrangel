///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include "antennatoolssettings.h"

AntennaToolsSettings::AntennaToolsSettings() :
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void AntennaToolsSettings::resetToDefaults()
{
    m_dipoleFrequencyMHz = 435.0;
    m_dipoleFrequencySelect = 0;
    m_dipoleEndEffectFactor = 0.95;
    m_dipoleLengthUnits = CM;
    m_dishFrequencyMHz = 1700.0;
    m_dishFrequencySelect = 0;
    m_dishDiameter = 240.0;
    m_dishDepth = 30.0;
    m_dishEfficiency = 60;
    m_dishLengthUnits = CM;
    m_dishSurfaceError = 0.0;
    m_title = "Antenna Tools";
    m_rgbColor = QColor(225, 25, 99).rgb();
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIFeatureSetIndex = 0;
    m_reverseAPIFeatureIndex = 0;
    m_workspaceIndex = 0;
}

QByteArray AntennaToolsSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeDouble(1, m_dipoleFrequencyMHz);
    s.writeS32(2, m_dipoleFrequencySelect);
    s.writeDouble(3, m_dipoleEndEffectFactor);
    s.writeS32(4, (int)m_dipoleLengthUnits);
    s.writeDouble(5, m_dishFrequencyMHz);
    s.writeS32(6, m_dishFrequencySelect);
    s.writeDouble(7, m_dishDiameter);
    s.writeDouble(8, m_dishDepth);
    s.writeS32(9, m_dishEfficiency);
    s.writeS32(10, (int)m_dishLengthUnits);
    s.writeDouble(18, m_dishSurfaceError);
    s.writeString(11, m_title);
    s.writeU32(12, m_rgbColor);
    s.writeBool(13, m_useReverseAPI);
    s.writeString(14, m_reverseAPIAddress);
    s.writeU32(15, m_reverseAPIPort);
    s.writeU32(16, m_reverseAPIFeatureSetIndex);
    s.writeU32(17, m_reverseAPIFeatureIndex);

    if (m_rollupState) {
        s.writeBlob(19, m_rollupState->serialize());
    }

    s.writeS32(20, m_workspaceIndex);
    s.writeBlob(21, m_geometryBytes);

    return s.final();
}

bool AntennaToolsSettings::deserialize(const QByteArray& data)
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

        d.readDouble(1, &m_dipoleFrequencyMHz, 435.0);
        d.readS32(2, &m_dipoleFrequencySelect, 0);
        d.readDouble(3, &m_dipoleEndEffectFactor, 0.95);
        d.readS32(4, (int*)&m_dipoleLengthUnits, (int)CM);
        d.readDouble(5, &m_dishFrequencyMHz, 1700.0);
        d.readS32(6, &m_dishFrequencySelect, 0);
        d.readDouble(7, &m_dishDiameter, 240.0);
        d.readDouble(8, &m_dishDepth, 30.0);
        d.readS32(9, &m_dishEfficiency, 60);
        d.readS32(10, (int*)&m_dishLengthUnits, (int)CM);
        d.readDouble(18, &m_dishSurfaceError, 0.0);
        d.readString(11, &m_title, "Antenna Tools");
        d.readU32(12, &m_rgbColor, QColor(225, 25, 99).rgb());
        d.readBool(13, &m_useReverseAPI, false);
        d.readString(14, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(15, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(16, &utmp, 0);
        m_reverseAPIFeatureSetIndex = utmp > 99 ? 99 : utmp;
        d.readU32(17, &utmp, 0);
        m_reverseAPIFeatureIndex = utmp > 99 ? 99 : utmp;

        if (m_rollupState)
        {
            d.readBlob(19, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(20, &m_workspaceIndex, 0);
        d.readBlob(21, &m_geometryBytes);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
