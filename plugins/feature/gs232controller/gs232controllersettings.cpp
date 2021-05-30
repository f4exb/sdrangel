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

#include "gs232controllersettings.h"

const QStringList GS232ControllerSettings::m_pipeTypes = {
    QStringLiteral("ADSBDemod"),
    QStringLiteral("Map"),
    QStringLiteral("StarTracker"),
    QStringLiteral("SatelliteTracker")
};

const QStringList GS232ControllerSettings::m_pipeURIs = {
    QStringLiteral("sdrangel.channel.adsbdemod"),
    QStringLiteral("sdrangel.feature.map"),
    QStringLiteral("sdrangel.feature.startracker"),
    QStringLiteral("sdrangel.feature.satellitetracker")
};

GS232ControllerSettings::GS232ControllerSettings()
{
    resetToDefaults();
}

void GS232ControllerSettings::resetToDefaults()
{
    m_azimuth = 0.0f;
    m_elevation = 0.0f;
    m_serialPort = "";
    m_baudRate = 9600;
    m_track = false;
    m_target = "";
    m_title = "GS-232 Rotator Controller";
    m_rgbColor = QColor(225, 25, 99).rgb();
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIFeatureSetIndex = 0;
    m_reverseAPIFeatureIndex = 0;
    m_azimuthOffset = 0;
    m_elevationOffset = 0;
    m_azimuthMin = 0;
    m_azimuthMax = 450;
    m_elevationMin = 0;
    m_elevationMax = 180;
    m_tolerance = 0;
    m_protocol = GS232;
}

QByteArray GS232ControllerSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeFloat(1, m_azimuth);
    s.writeFloat(2, m_elevation);
    s.writeString(3, m_serialPort);
    s.writeS32(4, m_baudRate);
    s.writeBool(5, m_track);
    s.writeString(6, m_target);
    s.writeString(8, m_title);
    s.writeU32(9, m_rgbColor);
    s.writeBool(10, m_useReverseAPI);
    s.writeString(11, m_reverseAPIAddress);
    s.writeU32(12, m_reverseAPIPort);
    s.writeU32(13, m_reverseAPIFeatureSetIndex);
    s.writeU32(14, m_reverseAPIFeatureIndex);
    s.writeS32(15, m_azimuthOffset);
    s.writeS32(16, m_elevationOffset);
    s.writeS32(17, m_azimuthMin);
    s.writeS32(18, m_azimuthMax);
    s.writeS32(19, m_elevationMin);
    s.writeS32(20, m_elevationMax);
    s.writeS32(21, m_tolerance);
    s.writeS32(22, (int)m_protocol);

    return s.final();
}

bool GS232ControllerSettings::deserialize(const QByteArray& data)
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

        d.readFloat(1, &m_azimuth, 0);
        d.readFloat(2, &m_elevation, 0);
        d.readString(3, &m_serialPort, "");
        d.readS32(4, &m_baudRate, 9600);
        d.readBool(5, &m_track, false);
        d.readString(6, &m_target, "");
        d.readString(8, &m_title, "GS-232 Rotator Controller");
        d.readU32(9, &m_rgbColor, QColor(225, 25, 99).rgb());
        d.readBool(10, &m_useReverseAPI, false);
        d.readString(11, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(12, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(13, &utmp, 0);
        m_reverseAPIFeatureSetIndex = utmp > 99 ? 99 : utmp;
        d.readU32(14, &utmp, 0);
        m_reverseAPIFeatureIndex = utmp > 99 ? 99 : utmp;
        d.readS32(15, &m_azimuthOffset, 0);
        d.readS32(16, &m_elevationOffset, 0);
        d.readS32(17, &m_azimuthMin, 0);
        d.readS32(18, &m_azimuthMax, 450);
        d.readS32(19, &m_elevationMin, 0);
        d.readS32(20, &m_elevationMax, 180);
        d.readS32(21, &m_tolerance, 0);
        d.readS32(22, (int*)&m_protocol, GS232);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
