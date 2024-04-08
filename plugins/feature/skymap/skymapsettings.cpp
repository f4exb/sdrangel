///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2017, 2019-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
// Copyright (C) 2020-2024 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2022 CRD716 <crd716@gmail.com>                                  //
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
#include <QDebug>

#include "util/simpleserializer.h"
#include "settings/serializable.h"

#include "skymapsettings.h"

const QStringList SkyMapSettings::m_pipeURIs = {
    QStringLiteral("sdrangel.feature.startracker"),
    QStringLiteral("sdrangel.feature.satellitetracker"),
    QStringLiteral("sdrangel.feature.gs232controller"),
    QStringLiteral("sdrangel.feature.map")
};

SkyMapSettings::SkyMapSettings() :
    m_rollupState(nullptr)
{
    resetToDefaults();
}

SkyMapSettings::~SkyMapSettings()
{
}

void SkyMapSettings::resetToDefaults()
{
    m_map = "WWT";
    m_displayNames = true;
    m_displayConstellations = true;
    m_displayReticle = true;
    m_displayGrid = false;
    m_displayAntennaFoV = false;
    m_projection = "";
    m_source = "";
    m_track = false;
    m_hpbw = 10.0f;
    m_latitude = 0.0f;
    m_longitude = 0.0f;
    m_altitude = 0.0f;
    m_useMyPosition = false;
    m_wwtSettings = QHash<QString, QVariant>({
        {"constellationBoundaries", false},
        {"constellationFigures", true},
        {"constellationLabels", true},
        {"constellationPictures", false},
        {"constellationSelection", false},
        {"ecliptic", false},
        {"eclipticOverviewText", false},
        {"eclipticGrid", false},
        {"eclipticGridText", true},
        {"altAzGrid", true},
        {"altAzGridText", true},
        {"galacticGrid", false},
        {"galacticGridText", true},
        {"elevationModel", false},
        {"earthSky", false},
        {"horizon", false},
        {"iss", false},
        {"precessionChart", false},
        {"skyGrids", false},
        {"skyNode", false},
        {"skyOverlays", false},
        {"solarSystemCosmos", false},
        {"solarSystemLighting", true},
        {"solarSystemMilkyWay", true},
        {"solarSystemMinorOrbits", false},
        {"solarSystemMinorPlanets", false},
        {"solarSystemMultiRes", true},
        {"solarSystemOrbits", true},
        {"solarSystemOverlays", false},
        {"solarSystemPlanets", true},
        {"solarSystemStars", true},

    });
    m_title = "Sky Map";
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIFeatureSetIndex = 0;
    m_reverseAPIFeatureIndex = 0;
    m_workspaceIndex = 0;
}

QByteArray SkyMapSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(2, m_map);
    s.writeBool(1, m_displayNames);
    s.writeBool(15, m_displayConstellations);
    s.writeBool(17, m_displayReticle);
    s.writeBool(18, m_displayGrid);
    s.writeBool(21, m_displayAntennaFoV);
    s.writeString(3, m_projection);
    s.writeString(4, m_source);
    s.writeBool(20, m_track);
    s.writeFloat(22, m_hpbw);
    s.writeFloat(23, m_latitude);
    s.writeFloat(24, m_longitude);
    s.writeFloat(25, m_altitude);
    s.writeBool(26, m_useMyPosition);
    s.writeHash(27, m_wwtSettings);

    s.writeString(8, m_title);
    s.writeU32(9, m_rgbColor);
    s.writeBool(10, m_useReverseAPI);
    s.writeString(11, m_reverseAPIAddress);
    s.writeU32(12, m_reverseAPIPort);
    s.writeU32(13, m_reverseAPIFeatureSetIndex);
    s.writeU32(14, m_reverseAPIFeatureIndex);

    if (m_rollupState) {
        s.writeBlob(19, m_rollupState->serialize());
    }

    s.writeS32(33, m_workspaceIndex);
    s.writeBlob(34, m_geometryBytes);

    return s.final();
}

bool SkyMapSettings::deserialize(const QByteArray& data)
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
        QString string;

        d.readString(2, &m_map, "WWT");
        d.readBool(1, &m_displayNames, true);
        d.readBool(15, &m_displayConstellations, true);
        d.readBool(17, &m_displayReticle, true);
        d.readBool(18, &m_displayGrid, true);
        d.readBool(21, &m_displayAntennaFoV, true);
        d.readString(3, &m_projection, "");
        d.readString(4, &m_source, "");
        d.readBool(20, &m_track, false);
        d.readFloat(22, &m_hpbw, 10.0f);
        d.readFloat(23, &m_latitude, 0.0f);
        d.readFloat(24, &m_longitude, 0.0f);
        d.readFloat(25, &m_altitude, 0.0f);
        d.readBool(26, &m_useMyPosition, false);
        d.readHash(27, &m_wwtSettings);

        d.readString(8, &m_title, "Sky Map");
        d.readU32(9, &m_rgbColor, QColor(225, 25, 99).rgba());
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

        if (m_rollupState)
        {
            d.readBlob(19, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(33, &m_workspaceIndex, 0);
        d.readBlob(34, &m_geometryBytes);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void SkyMapSettings::applySettings(const QStringList& settingsKeys, const SkyMapSettings& settings)
{
    if (settingsKeys.contains("map")) {
        m_map = settings.m_map;
    }
    if (settingsKeys.contains("displayNames")) {
        m_displayNames = settings.m_displayNames;
    }
    if (settingsKeys.contains("displayConstellations")) {
        m_displayConstellations = settings.m_displayConstellations;
    }
    if (settingsKeys.contains("displayReticle")) {
        m_displayReticle = settings.m_displayReticle;
    }
    if (settingsKeys.contains("displayGrid")) {
        m_displayGrid = settings.m_displayGrid;
    }
    if (settingsKeys.contains("displayAntennaFoV")) {
        m_displayAntennaFoV = settings.m_displayAntennaFoV;
    }
    if (settingsKeys.contains("background")) {
        m_background = settings.m_background;
    }
    if (settingsKeys.contains("projection")) {
        m_projection = settings.m_projection;
    }
    if (settingsKeys.contains("source")) {
        m_source = settings.m_source;
    }
    if (settingsKeys.contains("track")) {
        m_track = settings.m_track;
    }
    if (settingsKeys.contains("hpbw")) {
        m_hpbw = settings.m_hpbw;
    }
    if (settingsKeys.contains("latitude")) {
        m_latitude = settings.m_latitude;
    }
    if (settingsKeys.contains("longitude")) {
        m_longitude = settings.m_longitude;
    }
    if (settingsKeys.contains("altitude")) {
        m_altitude = settings.m_altitude;
    }
    if (settingsKeys.contains("useMyPosition")) {
        m_useMyPosition = settings.m_useMyPosition;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("useReverseAPI")) {
        m_useReverseAPI = settings.m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress")) {
        m_reverseAPIAddress = settings.m_reverseAPIAddress;
    }
    if (settingsKeys.contains("reverseAPIPort")) {
        m_reverseAPIPort = settings.m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIFeatureSetIndex")) {
        m_reverseAPIFeatureSetIndex = settings.m_reverseAPIFeatureSetIndex;
    }
    if (settingsKeys.contains("reverseAPIFeatureIndex")) {
        m_reverseAPIFeatureIndex = settings.m_reverseAPIFeatureIndex;
    }
    if (settingsKeys.contains("workspaceIndex")) {
        m_workspaceIndex = settings.m_workspaceIndex;
    }
}

QString SkyMapSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("map") || force) {
        ostr << " m_map: " << m_map.toStdString();
    }
    if (settingsKeys.contains("displayNames") || force) {
        ostr << " m_displayNames: " << m_displayNames;
    }
    if (settingsKeys.contains("displayConstellations") || force) {
        ostr << " m_displayConstellations: " << m_displayConstellations;
    }
    if (settingsKeys.contains("displayReticle") || force) {
        ostr << " m_displayReticle: " << m_displayReticle;
    }
    if (settingsKeys.contains("displayAntennaFoV") || force) {
        ostr << " m_displayAntennaFoV: " << m_displayAntennaFoV;
    }
    if (settingsKeys.contains("background") || force) {
        ostr << " m_background: " << m_background.toStdString();
    }
    if (settingsKeys.contains("projection") || force) {
        ostr << " m_projection: " << m_projection.toStdString();
    }
    if (settingsKeys.contains("source") || force) {
        ostr << " m_source: " << m_source.toStdString();
    }
    if (settingsKeys.contains("track") || force) {
        ostr << " m_track: " << m_track;
    }
    if (settingsKeys.contains("hpbw") || force) {
        ostr << " m_hpbw: " << m_hpbw;
    }
    if (settingsKeys.contains("latitude") || force) {
        ostr << " m_latitude: " << m_latitude;
    }
    if (settingsKeys.contains("longitude") || force) {
        ostr << " m_longitude: " << m_longitude;
    }
    if (settingsKeys.contains("altitude") || force) {
        ostr << " m_altitude: " << m_altitude;
    }
    if (settingsKeys.contains("useMyPosition") || force) {
        ostr << " m_useMyPosition: " << m_useMyPosition;
    }
    if (settingsKeys.contains("title") || force) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("useReverseAPI") || force) {
        ostr << " m_useReverseAPI: " << m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress") || force) {
        ostr << " m_reverseAPIAddress: " << m_reverseAPIAddress.toStdString();
    }
    if (settingsKeys.contains("reverseAPIPort") || force) {
        ostr << " m_reverseAPIPort: " << m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIFeatureSetIndex") || force) {
        ostr << " m_reverseAPIFeatureSetIndex: " << m_reverseAPIFeatureSetIndex;
    }
    if (settingsKeys.contains("reverseAPIFeatureIndex") || force) {
        ostr << " m_reverseAPIFeatureIndex: " << m_reverseAPIFeatureIndex;
    }
    if (settingsKeys.contains("workspaceIndex") || force) {
        ostr << " m_workspaceIndex: " << m_workspaceIndex;
    }

    return QString(ostr.str().c_str());
}

