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

#include "mapsettings.h"

const QStringList MapSettings::m_pipeTypes = {
    QStringLiteral("ADSBDemod"),
    QStringLiteral("AIS"),
    QStringLiteral("APRS"),
    QStringLiteral("StarTracker"),
    QStringLiteral("SatelliteTracker")
};

const QStringList MapSettings::m_pipeURIs = {
    QStringLiteral("sdrangel.channel.adsbdemod"),
    QStringLiteral("sdrangel.feature.ais"),
    QStringLiteral("sdrangel.feature.aprs"),
    QStringLiteral("sdrangel.feature.startracker"),
    QStringLiteral("sdrangel.feature.satellitetracker")
};

// GUI combo box should match ordering in this list
const QStringList MapSettings::m_mapProviders = {
    QStringLiteral("osm"),
    QStringLiteral("esri"),
    QStringLiteral("mapbox"),
    QStringLiteral("mapboxgl")
};

MapSettings::MapSettings()
{
    resetToDefaults();
}

void MapSettings::resetToDefaults()
{
    m_displayNames = true;
    m_mapProvider = "osm";
    m_mapBoxApiKey = "";
    m_mapBoxStyles = "";
    m_sources = -1;
    m_displaySelectedGroundTracks = true;
    m_displayAllGroundTracks = true;
    m_groundTrackColor = QColor(150, 0, 20).rgb();
    m_predictedGroundTrackColor = QColor(225, 0, 50).rgb();
    m_title = "Map";
    m_rgbColor = QColor(225, 25, 99).rgb();
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIFeatureSetIndex = 0;
    m_reverseAPIFeatureIndex = 0;
}

QByteArray MapSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeBool(1, m_displayNames);
    s.writeString(2, m_mapProvider);
    s.writeString(3, m_mapBoxApiKey);
    s.writeString(4, m_mapBoxStyles);
    s.writeU32(5, m_sources);
    s.writeU32(6, m_groundTrackColor);
    s.writeU32(7, m_predictedGroundTrackColor);
    s.writeString(8, m_title);
    s.writeU32(9, m_rgbColor);
    s.writeBool(10, m_useReverseAPI);
    s.writeString(11, m_reverseAPIAddress);
    s.writeU32(12, m_reverseAPIPort);
    s.writeU32(13, m_reverseAPIFeatureSetIndex);
    s.writeU32(14, m_reverseAPIFeatureIndex);
    s.writeBool(15, m_displaySelectedGroundTracks);
    s.writeBool(16, m_displayAllGroundTracks);

    return s.final();
}

bool MapSettings::deserialize(const QByteArray& data)
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

        d.readBool(1, &m_displayNames, true);
        d.readString(2, &m_mapProvider, "osm");
        d.readString(3, &m_mapBoxApiKey, "");
        d.readString(4, &m_mapBoxStyles, "");
        d.readU32(5, &m_sources, -1);
        d.readU32(6, &m_groundTrackColor, QColor(150, 0, 20).rgb());
        d.readU32(7, &m_predictedGroundTrackColor, QColor(225, 0, 50).rgb());
        d.readString(8, &m_title, "Map");
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
        d.readBool(15, &m_displaySelectedGroundTracks, true);
        d.readBool(16, &m_displayAllGroundTracks, true);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
