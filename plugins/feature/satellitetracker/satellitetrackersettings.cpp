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
#include <QDataStream>
#include <QIODevice>

#include "util/simpleserializer.h"
#include "settings/serializable.h"

#include "satellitetrackersettings.h"

#define DEAFULT_TARGET                  "ISS"
#define DEFAULT_TLES                    {"https://db.satnogs.org/api/tle/", "https://www.amsat.org/tle/current/nasabare.txt", "https://www.celestrak.com/NORAD/elements/goes.txt"}
#define DEFAULT_DATE_FORMAT              "yyyy/MM/dd"
#define DEFAULT_AOS_SPEECH              "${name} is visible for ${duration} minutes. Max elevation, ${elevation} degrees."
#define DEFAULT_LOS_SPEECH              "${name} is no longer visible."

SatelliteTrackerSettings::SatelliteTrackerSettings() :
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void SatelliteTrackerSettings::resetToDefaults()
{
    m_latitude = 0.0;
    m_longitude = 0.0;
    m_heightAboveSeaLevel = 0.0;
    m_target = DEAFULT_TARGET;
    m_satellites = {QString(DEAFULT_TARGET)};
    m_tles = DEFAULT_TLES;
    m_dateTime = "";
    m_minAOSElevation = 5;
    m_minPassElevation = 15;
    m_rotatorMaxAzimuth = 450;
    m_rotatorMaxElevation = 180;
    m_azElUnits = DM;
    m_groundTrackPoints = 100;
    m_dateFormat = DEFAULT_DATE_FORMAT;
    m_utc = false;
    m_updatePeriod = 1.0f;
    m_dopplerPeriod = 10.0f;
    m_defaultFrequency = 100000000.0f;
    m_drawOnMap = true;
    m_autoTarget = true;
    m_aosSpeech = DEFAULT_AOS_SPEECH;
    m_losSpeech = DEFAULT_LOS_SPEECH;
    m_aosCommand = "";
    m_losCommand = "";
    m_predictionPeriod = 5;
    m_passStartTime = QTime(0,0);
    m_passFinishTime = QTime(23,59,59);
    m_title = "Satellite Tracker";
    m_rgbColor = QColor(225, 25, 99).rgb();
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIFeatureSetIndex = 0;
    m_reverseAPIFeatureIndex = 0;
    m_chartsDarkTheme = true;
    m_replayEnabled = false;
    m_sendTimeToMap = true;
    m_dateTimeSelect = NOW;
    m_mapFeature = "";
    m_fileInputDevice = "";
    m_workspaceIndex = 0;
    m_columnSort = -1;
    m_columnSortOrder = Qt::AscendingOrder;
    for (int i = 0; i < SAT_COL_COLUMNS; i++)
    {
        m_columnIndexes[i] = i;
        m_columnSizes[i] = -1; // Autosize
    }
}

QByteArray SatelliteTrackerSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeDouble(1, m_latitude);
    s.writeDouble(2, m_longitude);
    s.writeDouble(3, m_heightAboveSeaLevel);
    s.writeString(4, m_target);
    s.writeBlob(5, serializeStringList(m_satellites));
    s.writeBlob(6, serializeStringList(m_tles));
    s.writeString(7, m_dateTime);
    s.writeS32(8, m_minAOSElevation);
    s.writeS32(9, m_minPassElevation);
    s.writeS32(10, m_rotatorMaxAzimuth);
    s.writeS32(11, m_rotatorMaxElevation);
    s.writeS32(12, m_azElUnits);
    s.writeS32(13, m_groundTrackPoints);
    s.writeString(14, m_dateFormat);
    s.writeBool(15, m_utc);
    s.writeFloat(16, m_updatePeriod);
    s.writeFloat(17, m_dopplerPeriod);
    s.writeS32(18, m_predictionPeriod);
    s.writeString(19, m_passStartTime.toString());
    s.writeString(20, m_passFinishTime.toString());
    s.writeFloat(21, m_defaultFrequency);
    s.writeBool(22, m_drawOnMap);
    s.writeBool(23, m_autoTarget);
    s.writeString(24, m_aosSpeech);
    s.writeString(25, m_losSpeech);
    s.writeString(26, m_aosCommand);
    s.writeString(27, m_losCommand);
    s.writeBlob(28, serializeDeviceSettings(m_deviceSettings));
    s.writeString(29, m_title);
    s.writeU32(30, m_rgbColor);
    s.writeBool(31, m_useReverseAPI);
    s.writeString(32, m_reverseAPIAddress);
    s.writeU32(33, m_reverseAPIPort);
    s.writeU32(34, m_reverseAPIFeatureSetIndex);
    s.writeU32(35, m_reverseAPIFeatureIndex);
    s.writeBool(36, m_chartsDarkTheme);
    if (m_rollupState) {
        s.writeBlob(37, m_rollupState->serialize());
    }
    s.writeBool(38, m_replayEnabled);
    s.writeString(39, m_replayStartDateTime.toString(Qt::ISODate));
    s.writeBool(41, m_sendTimeToMap);
    s.writeS32(42, (int)m_dateTimeSelect);
    s.writeString(43, m_mapFeature);
    s.writeString(44, m_fileInputDevice);
    s.writeS32(45, m_workspaceIndex);
    s.writeBlob(46, m_geometryBytes);
    s.writeS32(47, m_columnSort);
    s.writeS32(48, (int)m_columnSortOrder);

    for (int i = 0; i < SAT_COL_COLUMNS; i++) {
        s.writeS32(100 + i, m_columnIndexes[i]);
    }

    for (int i = 0; i < SAT_COL_COLUMNS; i++) {
        s.writeS32(200 + i, m_columnSizes[i]);
    }

    return s.final();
}

bool SatelliteTrackerSettings::deserialize(const QByteArray& data)
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

        d.readDouble(1, &m_latitude, 0.0);
        d.readDouble(2, &m_longitude, 0.0);
        d.readDouble(3, &m_heightAboveSeaLevel, 0.0);
        d.readString(4, &m_target, DEAFULT_TARGET);
        d.readBlob(5, &blob);
        deserializeStringList(blob, m_satellites);
        d.readBlob(6, &blob);
        deserializeStringList(blob, m_tles);
        d.readString(7, &m_dateTime, "");
        d.readS32(8, &m_minAOSElevation, 5);
        d.readS32(9, &m_minPassElevation, 15);
        d.readS32(10, &m_rotatorMaxAzimuth, 450);
        d.readS32(11, &m_rotatorMaxElevation, 180);
        d.readS32(12, (qint32 *)&m_azElUnits, DM);
        d.readS32(13, &m_groundTrackPoints, 100);
        d.readString(14, &m_dateFormat, DEFAULT_DATE_FORMAT);
        d.readBool(15, &m_utc, false);
        d.readFloat(16, &m_updatePeriod, 1.0f);
        d.readFloat(17, &m_dopplerPeriod, 10.0f);
        d.readS32(18, &m_predictionPeriod, 5);
        d.readString(19, &strtmp, "00:00:00");
        m_passStartTime = QTime::fromString(strtmp);
        d.readString(20, &strtmp, "23:59:59");
        m_passFinishTime = QTime::fromString(strtmp);
        d.readFloat(21, &m_defaultFrequency, 100000000.0f);
        d.readBool(22, &m_drawOnMap, true);
        d.readBool(23, &m_autoTarget, true);
        d.readString(24, &m_aosSpeech, DEFAULT_AOS_SPEECH);
        d.readString(25, &m_losSpeech, DEFAULT_LOS_SPEECH);
        d.readString(26, &m_aosCommand, "");
        d.readString(27, &m_losCommand, "");
        d.readBlob(28, &blob);
        deserializeDeviceSettings(blob, m_deviceSettings);
        d.readString(29, &m_title, "Satellite Tracker");
        d.readU32(30, &m_rgbColor, QColor(225, 25, 99).rgb());
        d.readBool(31, &m_useReverseAPI, false);
        d.readString(32, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(33, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(34, &utmp, 0);
        m_reverseAPIFeatureSetIndex = utmp > 99 ? 99 : utmp;
        d.readU32(35, &utmp, 0);
        m_reverseAPIFeatureIndex = utmp > 99 ? 99 : utmp;
        d.readBool(36, &m_chartsDarkTheme, true);
        if (m_rollupState)
        {
            d.readBlob(37, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }
        d.readBool(38, &m_replayEnabled, false);
        d.readString(39, &strtmp);
        m_replayStartDateTime = QDateTime::fromString(strtmp, Qt::ISODate);
        d.readBool(41, &m_sendTimeToMap, true);
        d.readS32(42, (int *)&m_dateTimeSelect, (int)NOW);
        d.readString(43, &m_mapFeature, "");
        d.readString(44, &m_fileInputDevice, "");
        d.readS32(45, &m_workspaceIndex, 0);
        d.readBlob(46, &m_geometryBytes);
        d.readS32(47, &m_columnSort, -1);
        d.readS32(48, (int *)&m_columnSortOrder, (int)Qt::AscendingOrder);

        for (int i = 0; i < SAT_COL_COLUMNS; i++) {
            d.readS32(100 + i, &m_columnIndexes[i], i);
        }

        for (int i = 0; i < SAT_COL_COLUMNS; i++) {
            d.readS32(200 + i, &m_columnSizes[i], -1);
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

QByteArray SatelliteTrackerSettings::serializeStringList(const QList<QString>& strings) const
{
    QByteArray data;
    QDataStream *stream = new QDataStream(&data,  QIODevice::WriteOnly);
    (*stream) << strings;
    delete stream;
    return data;
}

void SatelliteTrackerSettings::deserializeStringList(const QByteArray& data, QList<QString>& strings)
{
    QDataStream *stream = new QDataStream(data);
    (*stream) >> strings;
    delete stream;
}

QDataStream& operator<<(QDataStream& out, const SatelliteTrackerSettings::SatelliteDeviceSettings* settings)
{
    out << settings->m_deviceSetIndex;
    out << settings->m_presetGroup;
    out << settings->m_presetFrequency;
    out << settings->m_presetDescription;
    out << settings->m_doppler;
    out << settings->m_startOnAOS;
    out << settings->m_stopOnLOS;
    out << settings->m_startStopFileSink;
    out << settings->m_frequency;
    out << settings->m_aosCommand;
    out << settings->m_losCommand;
    return out;
}

QDataStream& operator>>(QDataStream& in, SatelliteTrackerSettings::SatelliteDeviceSettings*& settings)
{
    settings = new SatelliteTrackerSettings::SatelliteDeviceSettings();
    in >> settings->m_deviceSetIndex;
    in >> settings->m_presetGroup;
    in >> settings->m_presetFrequency;
    in >> settings->m_presetDescription;
    in >> settings->m_doppler;
    in >> settings->m_startOnAOS;
    in >> settings->m_stopOnLOS;
    in >> settings->m_startStopFileSink;
    in >> settings->m_frequency;
    in >> settings->m_aosCommand;
    in >> settings->m_losCommand;
    return in;
}

QDataStream& operator<<(QDataStream& out, const QList<SatelliteTrackerSettings::SatelliteDeviceSettings *> *list)
{
    out << *list;
    return out;
}

QDataStream& operator>>(QDataStream& in, QList<SatelliteTrackerSettings::SatelliteDeviceSettings *>*& list)
{
    list = new QList<SatelliteTrackerSettings::SatelliteDeviceSettings *>();
    in >> *list;
    return in;
}

QByteArray SatelliteTrackerSettings::serializeDeviceSettings(QHash<QString, QList<SatelliteDeviceSettings *> *> deviceSettings) const
{
    QByteArray data;
    QDataStream *stream = new QDataStream(&data,  QIODevice::WriteOnly);
    (*stream) << deviceSettings;
    delete stream;
    return data;
}

void SatelliteTrackerSettings::deserializeDeviceSettings(const QByteArray& data, QHash<QString, QList<SatelliteDeviceSettings *> *>& deviceSettings)
{
    QDataStream *stream = new QDataStream(data);
    (*stream) >> deviceSettings;
    delete stream;
}

SatelliteTrackerSettings::SatelliteDeviceSettings::SatelliteDeviceSettings()
{
    m_deviceSetIndex = 0;
    m_presetFrequency = 0;
    m_startOnAOS = true;
    m_stopOnLOS = true;
    m_startStopFileSink = true;
    m_frequency = 0;
    m_aosCommand = "";
    m_losCommand = "";
}

void SatelliteTrackerSettings::applySettings(const QStringList& settingsKeys, const SatelliteTrackerSettings& settings)
{
    if (settingsKeys.contains("latitude")) {
        m_latitude = settings.m_latitude;
    }
    if (settingsKeys.contains("longitude")) {
        m_longitude = settings.m_longitude;
    }
    if (settingsKeys.contains("heightAboveSeaLevel")) {
        m_heightAboveSeaLevel = settings.m_heightAboveSeaLevel;
    }
    if (settingsKeys.contains("target")) {
        m_target = settings.m_target;
    }
    if (settingsKeys.contains("satellites")) {
        m_satellites = settings.m_satellites;
    }
    if (settingsKeys.contains("tles")) {
        m_tles = settings.m_tles;
    }
    if (settingsKeys.contains("dateTime")) {
        m_dateTime = settings.m_dateTime;
    }
    if (settingsKeys.contains("minAOSElevation")) {
        m_minAOSElevation = settings.m_minAOSElevation;
    }
    if (settingsKeys.contains("minPassElevation")) {
        m_minPassElevation = settings.m_minPassElevation;
    }
    if (settingsKeys.contains("rotatorMaxAzimuth")) {
        m_rotatorMaxAzimuth = settings.m_rotatorMaxAzimuth;
    }
    if (settingsKeys.contains("rotatorMaxElevation")) {
        m_rotatorMaxElevation = settings.m_rotatorMaxElevation;
    }
    if (settingsKeys.contains("azElUnits")) {
        m_azElUnits = settings.m_azElUnits;
    }
    if (settingsKeys.contains("groundTrackPoints")) {
        m_groundTrackPoints = settings.m_groundTrackPoints;
    }
    if (settingsKeys.contains("dateFormat")) {
        m_dateFormat = settings.m_dateFormat;
    }
    if (settingsKeys.contains("utc")) {
        m_utc = settings.m_utc;
    }
    if (settingsKeys.contains("updatePeriod")) {
        m_updatePeriod = settings.m_updatePeriod;
    }
    if (settingsKeys.contains("dopplerPeriod")) {
        m_dopplerPeriod = settings.m_dopplerPeriod;
    }
    if (settingsKeys.contains("defaultFrequency")) {
        m_defaultFrequency = settings.m_defaultFrequency;
    }
    if (settingsKeys.contains("drawOnMap")) {
        m_drawOnMap = settings.m_drawOnMap;
    }
    if (settingsKeys.contains("autoTarget")) {
        m_autoTarget = settings.m_autoTarget;
    }
    if (settingsKeys.contains("aosSpeech")) {
        m_aosSpeech = settings.m_aosSpeech;
    }
    if (settingsKeys.contains("aosCommand")) {
        m_aosCommand = settings.m_aosCommand;
    }
    if (settingsKeys.contains("losCommand")) {
        m_losCommand = settings.m_losCommand;
    }
    if (settingsKeys.contains("predictionPeriod")) {
        m_predictionPeriod = settings.m_predictionPeriod;
    }
    if (settingsKeys.contains("passStartTime")) {
        m_passStartTime = settings.m_passStartTime;
    }
    if (settingsKeys.contains("passFinishTime")) {
        m_passFinishTime = settings.m_passFinishTime;
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
    if (settingsKeys.contains("reverseAPIFeatureSetIndex")) {
        m_reverseAPIFeatureSetIndex = settings.m_reverseAPIFeatureSetIndex;
    }
    if (settingsKeys.contains("reverseAPIFeatureIndex")) {
        m_reverseAPIFeatureIndex = settings.m_reverseAPIFeatureIndex;
    }
    if (settingsKeys.contains("chartsDarkTheme")) {
        m_chartsDarkTheme = settings.m_chartsDarkTheme;
    }
    if (settingsKeys.contains("replayEnabled")) {
        m_replayEnabled = settings.m_replayEnabled;
    }
    if (settingsKeys.contains("sendTimeToMap")) {
        m_sendTimeToMap = settings.m_sendTimeToMap;
    }
    if (settingsKeys.contains("dateTimeSelect")) {
        m_dateTimeSelect = settings.m_dateTimeSelect;
    }
    if (settingsKeys.contains("columnSort")) {
        m_columnSort = settings.m_columnSort;
    }
    if (settingsKeys.contains("columnSortOrder")) {
        m_columnSortOrder = settings.m_columnSortOrder;
    }

    if (settingsKeys.contains("columnIndexes"))
    {
        for (int i = 0; i < SAT_COL_COLUMNS; i++) {
            m_columnIndexes[i] = settings.m_columnIndexes[i];
        }
    }

    if (settingsKeys.contains("columnSizes"))
    {
        for (int i = 0; i < SAT_COL_COLUMNS; i++) {
            m_columnSizes[i] = settings.m_columnSizes[i];
        }
    }

    if (settingsKeys.contains("deviceSettings")) {
        m_deviceSettings = settings.m_deviceSettings;
    }
}

QString SatelliteTrackerSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("latitude") || force) {
        ostr << " m_latitude: " << m_latitude;
    }
    if (settingsKeys.contains("longitude") || force) {
        ostr << " m_longitude: " << m_longitude;
    }
    if (settingsKeys.contains("heightAboveSeaLevel") || force) {
        ostr << " m_heightAboveSeaLevel: " << m_heightAboveSeaLevel;
    }

    if (settingsKeys.contains("satellites") || force)
    {
        ostr << "m_satellites:";

        for (auto satellite : m_satellites) {
            ostr << " " << satellite.toStdString();
        }
    }

    if (settingsKeys.contains("tles") || force)
    {
        ostr << " m_tles:";

        for (auto tle : m_tles) {
            ostr << " " << tle.toStdString();
        }
    }

    if (settingsKeys.contains("dateTime") || force) {
        ostr << " m_dateTime: " << m_dateTime.toStdString();
    }
    if (settingsKeys.contains("minAOSElevation") || force) {
        ostr << " m_minAOSElevation: " << m_minAOSElevation;
    }
    if (settingsKeys.contains("minPassElevation") || force) {
        ostr << " m_minPassElevation: " << m_minPassElevation;
    }
    if (settingsKeys.contains("rotatorMaxAzimuth") || force) {
        ostr << " m_rotatorMaxAzimuth: " << m_rotatorMaxAzimuth;
    }
    if (settingsKeys.contains("rotatorMaxElevation") || force) {
        ostr << " m_rotatorMaxElevation: " << m_rotatorMaxElevation;
    }
    if (settingsKeys.contains("azElUnits") || force) {
        ostr << " m_azElUnits: " << m_azElUnits;
    }
    if (settingsKeys.contains("groundTrackPoints") || force) {
        ostr << " m_groundTrackPoints: " << m_groundTrackPoints;
    }
    if (settingsKeys.contains("dateFormat") || force) {
        ostr << " m_dateFormat: " << m_dateFormat.toStdString();
    }
    if (settingsKeys.contains("utc") || force) {
        ostr << " m_utc: " << m_utc;
    }
    if (settingsKeys.contains("updatePeriod") || force) {
        ostr << " m_updatePeriod: " << m_updatePeriod;
    }
    if (settingsKeys.contains("dopplerPeriod") || force) {
        ostr << " m_dopplerPeriod: " << m_dopplerPeriod;
    }
    if (settingsKeys.contains("defaultFrequency") || force) {
        ostr << " m_defaultFrequency: " << m_defaultFrequency;
    }
    if (settingsKeys.contains("drawOnMap") || force) {
        ostr << " m_drawOnMap: " << m_drawOnMap;
    }
    if (settingsKeys.contains("autoTarget") || force) {
        ostr << " m_autoTarget: " << m_autoTarget;
    }
    if (settingsKeys.contains("aosSpeech") || force) {
        ostr << " m_aosSpeech: " << m_aosSpeech.toStdString();
    }
    if (settingsKeys.contains("losSpeech") || force) {
        ostr << " m_losSpeech: " << m_losSpeech.toStdString();
    }
    if (settingsKeys.contains("aosCommand") || force) {
        ostr << " m_aosCommand: " << m_aosCommand.toStdString();
    }
    if (settingsKeys.contains("losCommand") || force) {
        ostr << " m_losCommand: " << m_losCommand.toStdString();
    }
    if (settingsKeys.contains("predictionPeriod") || force) {
        ostr << " m_predictionPeriod: " << m_predictionPeriod;
    }
    if (settingsKeys.contains("passStartTime") || force) {
        ostr << " m_passStartTime: " << m_passStartTime.toString().toStdString();
    }
    if (settingsKeys.contains("passFinishTime") || force) {
        ostr << " m_passFinishTime: " << m_passFinishTime.toString().toStdString();
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
    if (settingsKeys.contains("chartsDarkTheme") || force) {
        ostr << " m_chartsDarkTheme: " << m_chartsDarkTheme;
    }
    if (settingsKeys.contains("replayEnabled") || force) {
        ostr << " m_replayEnabled: " << m_replayEnabled;
    }
    if (settingsKeys.contains("sendTimeToMap") || force) {
        ostr << " m_sendTimeToMap: " << m_sendTimeToMap;
    }
    if (settingsKeys.contains("dateTimeSelect") || force) {
        ostr << " m_dateTimeSelect: " << m_dateTimeSelect;
    }
    if (settingsKeys.contains("mapFeature") || force) {
        ostr << " m_mapFeature: " << m_mapFeature.toStdString();
    }
    if (settingsKeys.contains("fileInputDevice") || force) {
        ostr << " m_fileInputDevice: " << m_fileInputDevice.toStdString();
    }
    if (settingsKeys.contains("workspaceIndex") || force) {
        ostr << " m_workspaceIndex: " << m_workspaceIndex;
    }
    if (settingsKeys.contains("columnSort") || force) {
        ostr << " m_columnSort: " << m_columnSort;
    }
    if (settingsKeys.contains("columnSortOrder") || force) {
        ostr << " m_columnSortOrder: " << m_columnSortOrder;
    }

    if (settingsKeys.contains("columnIndexes"))
    {
        ostr << "m_columnIndexes:";

        for (int i = 0; i < SAT_COL_COLUMNS; i++) {
            ostr << " " << m_columnIndexes[i];
        }
    }

    if (settingsKeys.contains("columnSizes"))
    {
        ostr << "m_columnSizes:";

        for (int i = 0; i < SAT_COL_COLUMNS; i++) {
            ostr << " " << m_columnSizes[i];
        }
    }

    if (settingsKeys.contains("deviceSettings"))
    {
        ostr << "m_deviceSettings: [";

        for (auto deviceSettingList : m_deviceSettings)
        {
            ostr << "[";

            for (auto deviceSettings : *deviceSettingList) {
                deviceSettings->getDebugString(ostr);
            }

            ostr << "]";
        }

        ostr << "]";
    }

    return QString(ostr.str().c_str());
}


void SatelliteTrackerSettings::SatelliteDeviceSettings::getDebugString(std::ostringstream& ostr)
{
    ostr << " m_deviceSetIndex: " << m_deviceSetIndex
        << " m_presetGroup: " << m_presetGroup.toStdString()
        << " m_presetFrequency: " << m_presetFrequency
        << " m_presetDescription" <<  m_presetDescription.toStdString()
        << " m_doppler: [";

    for (auto doppler : m_doppler) {
        ostr << " " << doppler;
    }

    ostr << "] m_startOnAOS: " << m_startOnAOS
        << " m_stopOnLOS: " << m_stopOnLOS
        << " m_startStopFileSink: " << m_startStopFileSink
        << " m_frequency: " << m_frequency
        << " m_aosCommand: " << m_aosCommand.toStdString()
        << " m_losCommand: " << m_losCommand.toStdString();
}
