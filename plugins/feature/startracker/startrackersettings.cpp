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

#include "startrackersettings.h"

const QStringList StarTrackerSettings::m_pipeTypes = {
    QStringLiteral("RadioAstronomy")
};

const QStringList StarTrackerSettings::m_pipeURIs = {
    QStringLiteral("sdrangel.channel.radioastronomy")
};

StarTrackerSettings::StarTrackerSettings() :
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void StarTrackerSettings::resetToDefaults()
{
    m_ra = "";
    m_dec = "";
    m_latitude = 0.0;
    m_longitude = 0.0;
    m_target = "Sun";
    m_dateTime = "";
    m_refraction = "Positional Astronomy Library";
    m_pressure = 1010;
    m_temperature = 10;
    m_humidity = 80.0;
    m_heightAboveSeaLevel = 0.0;
    m_temperatureLapseRate = 6.49;
    m_frequency = 432000000;
    m_beamwidth = 25.0;
    m_enableServer = true;
    m_serverPort = 10001;
    m_azElUnits = DM;
    m_solarFluxData = DRAO_2800;
    m_solarFluxUnits = SFU;
    m_updatePeriod = 1.0;
    m_jnow = false;
    m_drawSunOnMap = true;
    m_drawMoonOnMap = true;
    m_drawStarOnMap = true;
    m_chartsDarkTheme = true;
    m_title = "Star Tracker";
    m_rgbColor = QColor(225, 25, 99).rgb();
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIFeatureSetIndex = 0;
    m_reverseAPIFeatureIndex = 0;
    m_az = 0.0;
    m_el = 0.0;
    m_l = 0.0;
    m_b = 0.0;
    m_link = false;
    m_owmAPIKey = "";
    m_weatherUpdatePeriod = 60;
    m_azimuthOffset = 0.0;
    m_elevationOffset = 0.0;
    m_drawSunOnSkyTempChart = true;
    m_drawMoonOnSkyTempChart = true;
    m_workspaceIndex = 0;
    m_drawRotators = MATCHING_TARGET;
}

QByteArray StarTrackerSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(1, m_ra);
    s.writeString(2, m_dec);
    s.writeDouble(3, m_latitude);
    s.writeDouble(4, m_longitude);
    s.writeString(5, m_target);
    s.writeString(6, m_dateTime);
    s.writeU32(7, m_enableServer);
    s.writeU32(8, m_serverPort);
    s.writeS32(9, m_azElUnits);
    s.writeFloat(10, m_updatePeriod);
    s.writeBool(11, m_jnow);
    s.writeString(12, m_refraction);
    s.writeDouble(13, m_pressure);
    s.writeDouble(14, m_temperature);
    s.writeDouble(15, m_humidity);
    s.writeDouble(16, m_heightAboveSeaLevel);
    s.writeDouble(17, m_temperatureLapseRate);
    s.writeDouble(18, m_frequency);
    s.writeBool(19, m_drawSunOnMap);
    s.writeBool(20, m_drawMoonOnMap);
    s.writeBool(21, m_drawStarOnMap);
    s.writeString(22, m_title);
    s.writeU32(23, m_rgbColor);
    s.writeBool(24, m_useReverseAPI);
    s.writeString(25, m_reverseAPIAddress);
    s.writeU32(26, m_reverseAPIPort);
    s.writeU32(27, m_reverseAPIFeatureSetIndex);
    s.writeU32(28, m_reverseAPIFeatureIndex);
    s.writeU32(29, m_solarFluxUnits);
    s.writeDouble(30, m_beamwidth);
    s.writeU32(31, m_solarFluxData);
    s.writeBool(32, m_chartsDarkTheme);
    s.writeDouble(33, m_az);
    s.writeDouble(34, m_el);
    s.writeDouble(35, m_l);
    s.writeDouble(36, m_b);
    s.writeBool(37, m_link);
    s.writeString(38, m_owmAPIKey);
    s.writeS32(39, m_weatherUpdatePeriod);
    s.writeDouble(40, m_azimuthOffset);
    s.writeDouble(41, m_elevationOffset);
    s.writeBool(42, m_drawSunOnSkyTempChart);
    s.writeBool(43, m_drawMoonOnSkyTempChart);

    if (m_rollupState) {
        s.writeBlob(44, m_rollupState->serialize());
    }

    s.writeS32(45, m_workspaceIndex);
    s.writeBlob(46, m_geometryBytes);
    s.writeS32(47, (int)m_drawRotators);

    return s.final();
}

bool StarTrackerSettings::deserialize(const QByteArray& data)
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

        d.readString(1, &m_ra, "");
        d.readString(2, &m_dec, "");
        d.readDouble(3, &m_latitude, 0.0);
        d.readDouble(4, &m_longitude, 0.0);
        d.readString(5, &m_target, "Sun");
        d.readString(6, &m_dateTime, "");
        d.readBool(7, &m_enableServer, true);
        d.readU32(8, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_serverPort = utmp;
        } else {
            m_serverPort = 10001;
        }

        d.readS32(9, (qint32 *)&m_azElUnits, DM);
        d.readFloat(10, &m_updatePeriod, 1.0f);
        d.readBool(11, &m_jnow, false);
        d.readString(12, &m_refraction, "Positional Astronomy Library");
        d.readDouble(13, &m_pressure, 1010);
        d.readDouble(14, &m_temperature, 10);
        d.readDouble(15, &m_humidity, 10);
        d.readDouble(16, &m_heightAboveSeaLevel, 80);
        d.readDouble(17, &m_temperatureLapseRate, 6.49);
        d.readDouble(18, &m_frequency, 435000000.0);
        d.readBool(19, &m_drawSunOnMap, true);
        d.readBool(20, &m_drawMoonOnMap, true);
        d.readBool(21, &m_drawStarOnMap, true);
        d.readString(22, &m_title, "Star Tracker");
        d.readU32(23, &m_rgbColor, QColor(225, 25, 99).rgb());
        d.readBool(24, &m_useReverseAPI, false);
        d.readString(25, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(26, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(27, &utmp, 0);
        m_reverseAPIFeatureSetIndex = utmp > 99 ? 99 : utmp;
        d.readU32(28, &utmp, 0);
        m_reverseAPIFeatureIndex = utmp > 99 ? 99 : utmp;
        d.readU32(29, (quint32 *)&m_solarFluxUnits, SFU);
        d.readDouble(30, &m_beamwidth, 25.0);
        d.readU32(31, (quint32 *)&m_solarFluxData, DRAO_2800);
        d.readBool(32, &m_chartsDarkTheme, true);
        d.readDouble(33, &m_az, 0.0);
        d.readDouble(34, &m_el, 0.0);
        d.readDouble(35, &m_l, 0.0);
        d.readDouble(36, &m_b, 0.0);
        d.readBool(37, &m_link, false);
        d.readString(38, &m_owmAPIKey, "");
        d.readS32(39, &m_weatherUpdatePeriod, 60);
        d.readDouble(40, &m_azimuthOffset, 0.0);
        d.readDouble(41, &m_elevationOffset, 0.0);
        d.readBool(42, &m_drawSunOnSkyTempChart, true);
        d.readBool(43, &m_drawMoonOnSkyTempChart, true);

        if (m_rollupState)
        {
            d.readBlob(44, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(45, &m_workspaceIndex, 0);
        d.readBlob(46, &m_geometryBytes);
        d.readS32(47, (int *)&m_drawRotators, MATCHING_TARGET);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void StarTrackerSettings::applySettings(const QStringList& settingsKeys, const StarTrackerSettings& settings)
{
    if (settingsKeys.contains("ra")) {
        m_ra = settings.m_ra;
    }
    if (settingsKeys.contains("dec")) {
        m_dec = settings.m_dec;
    }
    if (settingsKeys.contains("latitude")) {
        m_latitude = settings.m_latitude;
    }
    if (settingsKeys.contains("longitude")) {
        m_longitude = settings.m_longitude;
    }
    if (settingsKeys.contains("target")) {
        m_target = settings.m_target;
    }
    if (settingsKeys.contains("dateTime")) {
        m_dateTime = settings.m_dateTime;
    }
    if (settingsKeys.contains("refraction")) {
        m_refraction = settings.m_refraction;
    }
    if (settingsKeys.contains("pressure")) {
        m_pressure = settings.m_pressure;
    }
    if (settingsKeys.contains("temperature")) {
        m_temperature = settings.m_temperature;
    }
    if (settingsKeys.contains("humidity")) {
        m_humidity = settings.m_humidity;
    }
    if (settingsKeys.contains("heightAboveSeaLevel")) {
        m_heightAboveSeaLevel = settings.m_heightAboveSeaLevel;
    }
    if (settingsKeys.contains("temperatureLapseRate")) {
        m_temperatureLapseRate = settings.m_temperatureLapseRate;
    }
    if (settingsKeys.contains("frequency")) {
        m_frequency = settings.m_frequency;
    }
    if (settingsKeys.contains("beamwidth")) {
        m_beamwidth = settings.m_beamwidth;
    }
    if (settingsKeys.contains("serverPort")) {
        m_serverPort = settings.m_serverPort;
    }
    if (settingsKeys.contains("enableServer")) {
        m_enableServer = settings.m_enableServer;
    }
    if (settingsKeys.contains("azElUnits")) {
        m_azElUnits = settings.m_azElUnits;
    }
    if (settingsKeys.contains("solarFluxData")) {
        m_solarFluxData = settings.m_solarFluxData;
    }
    if (settingsKeys.contains("solarFluxUnits")) {
        m_solarFluxUnits = settings.m_solarFluxUnits;
    }
    if (settingsKeys.contains("updatePeriod")) {
        m_updatePeriod = settings.m_updatePeriod;
    }
    if (settingsKeys.contains("jnow")) {
        m_jnow = settings.m_jnow;
    }
    if (settingsKeys.contains("drawSunOnMap")) {
        m_drawSunOnMap = settings.m_drawSunOnMap;
    }
    if (settingsKeys.contains("drawMoonOnMap")) {
        m_drawMoonOnMap = settings.m_drawMoonOnMap;
    }
    if (settingsKeys.contains("drawStarOnMap")) {
        m_drawStarOnMap = settings.m_drawStarOnMap;
    }
    if (settingsKeys.contains("chartsDarkTheme")) {
        m_chartsDarkTheme = settings.m_chartsDarkTheme;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
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
    if (settingsKeys.contains("reverseAPIPort")) {
        m_reverseAPIPort = settings.m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIFeatureSetIndex")) {
        m_reverseAPIFeatureSetIndex = settings.m_reverseAPIFeatureSetIndex;
    }
    if (settingsKeys.contains("reverseAPIFeatureIndex")) {
        m_reverseAPIFeatureIndex = settings.m_reverseAPIFeatureIndex;
    }
    if (settingsKeys.contains("az")) {
        m_az = settings.m_az;
    }
    if (settingsKeys.contains("el")) {
        m_el = settings.m_el;
    }
    if (settingsKeys.contains("l")) {
        m_l = settings.m_l;
    }
    if (settingsKeys.contains("b")) {
        m_b = settings.m_b;
    }
    if (settingsKeys.contains("link")) {
        m_link = settings.m_link;
    }
    if (settingsKeys.contains("owmAPIKey")) {
        m_owmAPIKey = settings.m_owmAPIKey;
    }
    if (settingsKeys.contains("weatherUpdatePeriod")) {
        m_weatherUpdatePeriod = settings.m_weatherUpdatePeriod;
    }
    if (settingsKeys.contains("azimuthOffset")) {
        m_azimuthOffset = settings.m_azimuthOffset;
    }
    if (settingsKeys.contains("elevationOffset")) {
        m_elevationOffset = settings.m_elevationOffset;
    }
    if (settingsKeys.contains("drawSunOnSkyTempChart")) {
        m_drawSunOnSkyTempChart = settings.m_drawSunOnSkyTempChart;
    }
    if (settingsKeys.contains("drawMoonOnSkyTempChart")) {
        m_drawMoonOnSkyTempChart = settings.m_drawMoonOnSkyTempChart;
    }
    if (settingsKeys.contains("workspaceIndex")) {
        m_workspaceIndex = settings.m_workspaceIndex;
    }
    if (settingsKeys.contains("drawRotators")) {
        m_drawRotators = settings.m_drawRotators;
    }
}

QString StarTrackerSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("ra") || force) {
        ostr << " m_ra: " << m_ra.toStdString();
    }
    if (settingsKeys.contains("dec") || force) {
        ostr << " m_dec: " << m_dec.toStdString();
    }
    if (settingsKeys.contains("latitude") || force) {
        ostr << " m_latitude: " << m_latitude;
    }
    if (settingsKeys.contains("longitude") || force) {
        ostr << " m_longitude: " << m_longitude;
    }
    if (settingsKeys.contains("target") || force) {
        ostr << " m_target: " << m_target.toStdString();
    }
    if (settingsKeys.contains("dateTime") || force) {
        ostr << " m_dateTime: " << m_dateTime.toStdString();
    }
    if (settingsKeys.contains("refraction") || force) {
        ostr << " m_refraction: " << m_refraction.toStdString();
    }
    if (settingsKeys.contains("pressure") || force) {
        ostr << " m_pressure: " << m_pressure;
    }
    if (settingsKeys.contains("temperature") || force) {
        ostr << " m_temperature: " << m_temperature;
    }
    if (settingsKeys.contains("humidity") || force) {
        ostr << " m_humidity: " << m_humidity;
    }
    if (settingsKeys.contains("heightAboveSeaLevel") || force) {
        ostr << " m_heightAboveSeaLevel: " << m_heightAboveSeaLevel;
    }
    if (settingsKeys.contains("temperatureLapseRate") || force) {
        ostr << " m_temperatureLapseRate: " << m_temperatureLapseRate;
    }
    if (settingsKeys.contains("frequency") || force) {
        ostr << " m_frequency: " << m_frequency;
    }
    if (settingsKeys.contains("beamwidth") || force) {
        ostr << " m_beamwidth: " << m_beamwidth;
    }
    if (settingsKeys.contains("serverPort") || force) {
        ostr << " m_serverPort: " << m_serverPort;
    }
    if (settingsKeys.contains("enableServer") || force) {
        ostr << " m_enableServer: " << m_enableServer;
    }
    if (settingsKeys.contains("azElUnits") || force) {
        ostr << " m_azElUnits: " << m_azElUnits;
    }
    if (settingsKeys.contains("solarFluxData") || force) {
        ostr << " m_solarFluxData: " << m_solarFluxData;
    }
    if (settingsKeys.contains("solarFluxUnits") || force) {
        ostr << " m_solarFluxUnits: " << m_solarFluxUnits;
    }
    if (settingsKeys.contains("updatePeriod") || force) {
        ostr << " m_updatePeriod: " << m_updatePeriod;
    }
    if (settingsKeys.contains("jnow") || force) {
        ostr << " m_jnow: " << m_jnow;
    }
    if (settingsKeys.contains("drawSunOnMap") || force) {
        ostr << " m_drawSunOnMap: " << m_drawSunOnMap;
    }
    if (settingsKeys.contains("drawMoonOnMap") || force) {
        ostr << " m_drawMoonOnMap: " << m_drawMoonOnMap;
    }
    if (settingsKeys.contains("drawStarOnMap") || force) {
        ostr << " m_drawStarOnMap: " << m_drawStarOnMap;
    }
    if (settingsKeys.contains("chartsDarkTheme") || force) {
        ostr << " m_chartsDarkTheme: " << m_chartsDarkTheme;
    }
    if (settingsKeys.contains("title") || force) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("rgbColor") || force) {
        ostr << " m_rgbColor: " << m_rgbColor;
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
    if (settingsKeys.contains("az") || force) {
        ostr << " m_az: " << m_az;
    }
    if (settingsKeys.contains("el") || force) {
        ostr << " m_el: " << m_el;
    }
    if (settingsKeys.contains("l") || force) {
        ostr << " m_l: " << m_l;
    }
    if (settingsKeys.contains("b") || force) {
        ostr << " m_b: " << m_b;
    }
    if (settingsKeys.contains("link") || force) {
        ostr << " m_link: " << m_link;
    }
    if (settingsKeys.contains("owmAPIKey") || force) {
        ostr << " m_owmAPIKey: " << m_owmAPIKey.toStdString();
    }
    if (settingsKeys.contains("weatherUpdatePeriod") || force) {
        ostr << " m_weatherUpdatePeriod: " << m_weatherUpdatePeriod;
    }
    if (settingsKeys.contains("azimuthOffset") || force) {
        ostr << " m_azimuthOffset: " << m_azimuthOffset;
    }
    if (settingsKeys.contains("elevationOffset") || force) {
        ostr << " m_elevationOffset: " << m_elevationOffset;
    }
    if (settingsKeys.contains("drawSunOnSkyTempChart") || force) {
        ostr << " m_drawSunOnSkyTempChart: " << m_drawSunOnSkyTempChart;
    }
    if (settingsKeys.contains("drawMoonOnSkyTempChart") || force) {
        ostr << " m_drawMoonOnSkyTempChart: " << m_drawMoonOnSkyTempChart;
    }
    if (settingsKeys.contains("workspaceIndex") || force) {
        ostr << " m_workspaceIndex: " << m_workspaceIndex;
    }
    if (settingsKeys.contains("drawRotators") || force) {
        ostr << " m_drawRotators: " << m_drawRotators;
    }

    return QString(ostr.str().c_str());
}

