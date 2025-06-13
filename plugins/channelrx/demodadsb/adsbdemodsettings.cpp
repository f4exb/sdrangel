///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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
#include <QDebug>

#include "util/simpleserializer.h"
#include "settings/serializable.h"

#include "adsbdemodsettings.h"

ADSBDemodSettings::ADSBDemodSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void ADSBDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 2*1300000;
    m_correlationThreshold = 7.0f; //<! ones/zero powers correlation threshold in dB
    m_chipsThreshold = 0;
    m_samplesPerBit = 4;
    m_removeTimeout = 60;
    m_feedEnabled = false;
    m_exportClientEnabled = true;
    m_exportClientHost = "feed.adsbexchange.com";
    m_exportClientPort = 30005;
    m_exportClientFormat = BeastBinary;
    m_exportServerEnabled = false;
    m_exportServerPort = 30005;
    m_importEnabled = false;
    m_importHost = "opensky-network.org";
    m_importUsername = "";
    m_importPassword = "";
    m_importParameters = "";
    m_importPeriod = 10.0;
    m_importMinLatitude = "";
    m_importMaxLatitude = "";
    m_importMinLongitude = "";
    m_importMaxLongitude = "";
    m_rgbColor = QColor(244, 151, 57).rgb();
    m_title = "ADS-B Demodulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_airportRange = 100.0f;
    m_airportMinimumSize = AirportType::Medium;
    m_displayHeliports = false;
    m_flightPaths = true;
    m_allFlightPaths = false;
    m_siUnits = false;
    m_tableFontName = "Liberation Sans";
    m_tableFontSize = 9;
    m_displayDemodStats = false;
    m_demodModeS = true;
    m_autoResizeTableColumns = false;
    m_interpolatorPhaseSteps = 4;      // Higher than these two values will struggle to run in real-time
    m_interpolatorTapsPerPhase = 3.5f; // without gaining much improvement in PER
    m_aviationstackAPIKey = "";
    m_checkWXAPIKey = "";
    m_maptilerAPIKey = "";
    for (int i = 0; i < ADSBDEMOD_COLUMNS; i++)
    {
        m_columnIndexes[i] = i;
        m_columnSizes[i] = -1; // Autosize
    }
    m_logFilename = "adsb_log.csv";
    m_logEnabled = false;
    m_airspaces = QStringList({"CTR"});
    m_airspaceRange = 500.0f;
#ifdef LINUX
    m_mapProvider = "mapboxgl"; // osm maps do not work in some versions of Linux https://github.com/f4exb/sdrangel/issues/1169 & 1369
#else
    m_mapProvider = "osm";
#endif
    m_mapType = AVIATION_LIGHT;
    m_displayNavAids = true;
    m_displayPhotos = true;
    m_verboseModelMatching = false;
    m_aircraftMinZoom = 11;
    m_workspaceIndex = 0;
    m_hidden = false;
    m_atcLabels = true;
    m_atcCallsigns = true;
    m_transitionAlt = 6000; // Depends on airport. 18,000 in USA
    m_qnh = 1013.25;
    m_manualQNH = false;
    m_displayCoverage = false;
    m_displayChart = false;
    m_displayOrientation = false;
    m_displayRadius = false;
    for (int i = 0; i < ADSB_IC_MAX; i++) {
        m_displayIC[i] = false;
    }
    m_flightPathPaletteName = "Spectral";
    applyPalette();
    m_favourLivery = true;
}

QByteArray ADSBDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeReal(2, m_rfBandwidth);
    s.writeReal(3, m_correlationThreshold);
    s.writeS32(4, m_samplesPerBit);
    s.writeS32(5, m_removeTimeout);
    s.writeBool(6, m_feedEnabled);
    s.writeString(7, m_exportClientHost);
    s.writeU32(8, m_exportClientPort);
    s.writeU32(9, m_rgbColor);

    if (m_channelMarker) {
        s.writeBlob(10, m_channelMarker->serialize());
    }

    s.writeString(11, m_title);
    s.writeBool(12, m_useReverseAPI);
    s.writeString(13, m_reverseAPIAddress);
    s.writeU32(14, m_reverseAPIPort);
    s.writeU32(15, m_reverseAPIDeviceIndex);
    s.writeU32(16, m_reverseAPIChannelIndex);
    s.writeS32(17, m_streamIndex);

    s.writeFloat(18, m_airportRange);
    s.writeS32(19, (int)m_airportMinimumSize);
    s.writeBool(20, m_displayHeliports);
    s.writeBool(21, m_flightPaths);
    s.writeBool(23, m_siUnits);
    s.writeS32(24, (int)m_exportClientFormat);
    s.writeString(25, m_tableFontName);
    s.writeS32(26, m_tableFontSize);
    s.writeBool(27, m_displayDemodStats);
    s.writeBool(29, m_demodModeS);
    s.writeBool(30, m_autoResizeTableColumns);
    s.writeS32(31, m_interpolatorPhaseSteps);
    s.writeFloat(32, m_interpolatorTapsPerPhase);
    s.writeBool(33, m_allFlightPaths);

    s.writeBlob(34, serializeNotificationSettings(m_notificationSettings));
    s.writeString(35, m_aviationstackAPIKey);

    s.writeString(36, m_logFilename);
    s.writeBool(37, m_logEnabled);

    s.writeString(38, m_airspaces.join(" "));
    s.writeFloat(39, m_airspaceRange);
    s.writeS32(40, (int)m_mapType);
    s.writeBool(41, m_displayNavAids);
    s.writeBool(42, m_displayPhotos);

    if (m_rollupState) {
        s.writeBlob(43, m_rollupState->serialize());
    }

    s.writeBool(44, m_verboseModelMatching);

    s.writeBool(46, m_exportClientEnabled);
    s.writeBool(47, m_exportServerEnabled);
    s.writeBool(48, m_exportServerPort);
    s.writeBool(49, m_importEnabled);
    s.writeString(50, m_importHost);
    s.writeString(51, m_importUsername);
    s.writeString(52, m_importPassword);
    s.writeString(53, m_importParameters);
    s.writeFloat(54, m_importPeriod);
    s.writeString(55, m_importMinLatitude);
    s.writeString(56, m_importMaxLatitude);
    s.writeString(57, m_importMinLongitude);
    s.writeString(58, m_importMaxLongitude);
    s.writeS32(59, m_workspaceIndex);
    s.writeBlob(60, m_geometryBytes);
    s.writeBool(61, m_hidden);
    s.writeString(62, m_checkWXAPIKey);
    s.writeString(63, m_mapProvider);
    s.writeS32(64, m_aircraftMinZoom);

    s.writeBool(65, m_atcLabels);
    s.writeBool(66, m_atcCallsigns);
    s.writeS32(67, m_transitionAlt);
    s.writeString(68, m_amDemod);

    s.writeFloat(69, m_qnh);
    s.writeBool(70, m_manualQNH);

    s.writeBool(71, m_displayCoverage);
    s.writeBool(72, m_displayChart);
    s.writeBool(73, m_displayOrientation);
    s.writeBool(74, m_displayRadius);
    s.writeString(75, m_flightPathPaletteName);
    s.writeS32(76, m_chipsThreshold);
    s.writeBool(77, m_favourLivery);

    s.writeString(78, m_maptilerAPIKey);

    for (int i = 0; i < ADSBDEMOD_COLUMNS; i++) {
        s.writeS32(100 + i, m_columnIndexes[i]);
    }
    for (int i = 0; i < ADSBDEMOD_COLUMNS; i++) {
        s.writeS32(200 + i, m_columnSizes[i]);
    }

    return s.final();
}

bool ADSBDemodSettings::deserialize(const QByteArray& data)
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
        qint32 tmp;
        uint32_t utmp;
        QByteArray blob;
        QString string;

        if (m_channelMarker)
        {
            d.readBlob(10, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readS32(1, &tmp, 0);
        m_inputFrequencyOffset = tmp;
        d.readReal(2, &m_rfBandwidth, 2*1300000);
        d.readReal(3, &m_correlationThreshold, 7.0f);
        d.readS32(4, &m_samplesPerBit, 4);
        d.readS32(5, &m_removeTimeout, 60);
        d.readBool(6, &m_feedEnabled, false);
        d.readString(7, &m_exportClientHost, "feed.adsbexchange.com");
        d.readU32(8, &utmp, 0);
        if ((utmp > 1023) && (utmp < 65535)) {
            m_exportClientPort = utmp;
        } else {
            m_exportClientPort = 30005;
        }

        d.readU32(9, &m_rgbColor, QColor(244, 151, 57).rgb());
        d.readString(11, &m_title, "ADS-B Demodulator");
        d.readBool(12, &m_useReverseAPI, false);
        d.readString(13, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(14, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(15, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(16, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readS32(17, &m_streamIndex, 0);

        d.readFloat(18, &m_airportRange, 100.0f);
        d.readS32(19, (int *)&m_airportMinimumSize, AirportType::Medium);
        d.readBool(20, &m_displayHeliports, false);
        d.readBool(21, &m_flightPaths, true);
        d.readBool(23, &m_siUnits, false);
        d.readS32(24, (int *) &m_exportClientFormat, BeastBinary);
        d.readString(25, &m_tableFontName, "Liberation Sans");
        d.readS32(26, &m_tableFontSize, 9);
        d.readBool(27, &m_displayDemodStats, false);
        d.readBool(29, &m_demodModeS, true);
        d.readBool(30, &m_autoResizeTableColumns, false);
        d.readS32(31, &m_interpolatorPhaseSteps, 4);
        d.readFloat(32, &m_interpolatorTapsPerPhase, 3.5f);
        d.readBool(33, &m_allFlightPaths, false);

        d.readBlob(34, &blob);
        deserializeNotificationSettings(blob, m_notificationSettings);
        d.readString(35, &m_aviationstackAPIKey, "");

        d.readString(36, &m_logFilename, "adsb_log.csv");
        d.readBool(37, &m_logEnabled, false);

        d.readString(38, &string, "CTR");
        m_airspaces = string.split(" ");
        d.readFloat(39, &m_airspaceRange, 500.0f);
        d.readS32(40, (int *)&m_mapType, (int)AVIATION_LIGHT);
        d.readBool(41, &m_displayNavAids, true);
        d.readBool(42, &m_displayPhotos, true);

        if (m_rollupState)
        {
            d.readBlob(43, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readBool(44, &m_verboseModelMatching, false);

        d.readBool(46, &m_exportClientEnabled, true);
        d.readBool(47, &m_exportServerEnabled, true);
        d.readU32(48, &utmp, 0);
        if ((utmp > 1023) && (utmp < 65535)) {
            m_exportServerPort = utmp;
        } else {
            m_exportServerPort = 30005;
        }
        d.readBool(49, &m_importEnabled, false);
        d.readString(50, &m_importHost, "opensky-network.org");
        d.readString(51, &m_importUsername, "");
        d.readString(52, &m_importPassword, "");
        d.readString(53, &m_importParameters, "");
        d.readFloat(54, &m_importPeriod, 10.0f);
        d.readString(55, &m_importMinLatitude, "");
        d.readString(56, &m_importMaxLatitude, "");
        d.readString(57, &m_importMinLongitude, "");
        d.readString(58, &m_importMaxLongitude, "");
        d.readS32(59, &m_workspaceIndex, 0);
        d.readBlob(60, &m_geometryBytes);
        d.readBool(61, &m_hidden, false);
        d.readString(62, &m_checkWXAPIKey, "");
        d.readString(63, &m_mapProvider, "osm");
        d.readS32(64, &m_aircraftMinZoom, 11);

        d.readBool(65, &m_atcLabels, true);
        d.readBool(66, &m_atcCallsigns, true);
        d.readS32(67, &m_transitionAlt, 6000);
        d.readString(68, &m_amDemod);

        d.readFloat(69, &m_qnh, 1013.25);
        d.readBool(70, &m_manualQNH, false);

        d.readBool(71, &m_displayCoverage, false);
        d.readBool(72, &m_displayChart, false);
        d.readBool(73, &m_displayOrientation, false);
        d.readBool(74, &m_displayRadius, false);
        d.readString(75, &m_flightPathPaletteName, "Spectral");
        d.readS32(76, &m_chipsThreshold, 0);
        d.readBool(77, &m_favourLivery, true);

        d.readString(78, &m_maptilerAPIKey, "");

        applyPalette();

#ifdef LINUX
        if (m_mapProvider == "osm") {
            m_mapProvider = "mapboxgl";
        }
#endif

        for (int i = 0; i < ADSBDEMOD_COLUMNS; i++) {
            d.readS32(100 + i, &m_columnIndexes[i], i);
        }
        for (int i = 0; i < ADSBDEMOD_COLUMNS; i++) {
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

QDataStream& operator<<(QDataStream& out, const ADSBDemodSettings::NotificationSettings* settings)
{
    out << settings->m_matchColumn;
    out << settings->m_regExp;
    out << settings->m_speech;
    out << settings->m_command;
    out << settings->m_autoTarget;
    return out;
}

QDataStream& operator>>(QDataStream& in, ADSBDemodSettings::NotificationSettings*& settings)
{
    settings = new ADSBDemodSettings::NotificationSettings();
    in >> settings->m_matchColumn;
    in >> settings->m_regExp;
    in >> settings->m_speech;
    in >> settings->m_command;
    in >> settings->m_autoTarget;
    settings->updateRegularExpression();
    return in;
}

QByteArray ADSBDemodSettings::serializeNotificationSettings(QList<NotificationSettings *> notificationSettings) const
{
    QByteArray data;
    QDataStream *stream = new QDataStream(&data, QIODevice::WriteOnly);
    (*stream) << notificationSettings;
    delete stream;
    return data;
}

void ADSBDemodSettings::deserializeNotificationSettings(const QByteArray& data, QList<NotificationSettings *>& notificationSettings)
{
    QDataStream *stream = new QDataStream(data);
    (*stream) >> notificationSettings;
    delete stream;
}

ADSBDemodSettings::NotificationSettings::NotificationSettings()
{
    m_matchColumn = 0;
}

void ADSBDemodSettings::NotificationSettings::updateRegularExpression()
{
    m_regularExpression.setPattern(m_regExp);
    m_regularExpression.optimize();
    if (!m_regularExpression.isValid()) {
        qDebug() << "ADSBDemod: Regular expression is not valid: " << m_regExp;
    }
}

void ADSBDemodSettings::applySettings(const QStringList& settingsKeys, const ADSBDemodSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth")) {
        m_rfBandwidth = settings.m_rfBandwidth;
    }
    if (settingsKeys.contains("correlationThreshold")) {
        m_correlationThreshold = settings.m_correlationThreshold;
    }
    if (settingsKeys.contains("chipsThreshold")) {
        m_chipsThreshold = settings.m_chipsThreshold;
    }
    if (settingsKeys.contains("samplesPerBit")) {
        m_samplesPerBit = settings.m_samplesPerBit;
    }
    if (settingsKeys.contains("removeTimeout")) {
        m_removeTimeout = settings.m_removeTimeout;
    }
    if (settingsKeys.contains("feedEnabled")) {
        m_feedEnabled = settings.m_feedEnabled;
    }
    if (settingsKeys.contains("exportClientEnabled")) {
        m_exportClientEnabled = settings.m_exportClientEnabled;
    }
    if (settingsKeys.contains("exportClientHost")) {
        m_exportClientHost = settings.m_exportClientHost;
    }
    if (settingsKeys.contains("exportClientPort")) {
        m_exportClientPort = settings.m_exportClientPort;
    }
    if (settingsKeys.contains("exportClientFormat")) {
        m_exportClientFormat = settings.m_exportClientFormat;
    }
    if (settingsKeys.contains("exportServerEnabled")) {
        m_exportServerEnabled = settings.m_exportServerEnabled;
    }
    if (settingsKeys.contains("exportServerPort")) {
        m_exportServerPort = settings.m_exportServerPort;
    }
    if (settingsKeys.contains("importEnabled")) {
        m_importEnabled = settings.m_importEnabled;
    }
    if (settingsKeys.contains("importHost")) {
        m_importHost = settings.m_importHost;
    }
    if (settingsKeys.contains("importUsername")) {
        m_importUsername = settings.m_importUsername;
    }
    if (settingsKeys.contains("importPassword")) {
        m_importPassword = settings.m_importPassword;
    }
    if (settingsKeys.contains("importParameters")) {
        m_importParameters = settings.m_importParameters;
    }
    if (settingsKeys.contains("importPeriod")) {
        m_importPeriod = settings.m_importPeriod;
    }
    if (settingsKeys.contains("importMinLatitude")) {
        m_importMinLatitude = settings.m_importMinLatitude;
    }
    if (settingsKeys.contains("importMaxLatitude")) {
        m_importMaxLatitude = settings.m_importMaxLatitude;
    }
    if (settingsKeys.contains("importMinLongitude")) {
        m_importMinLongitude = settings.m_importMinLongitude;
    }
    if (settingsKeys.contains("importMaxLongitude")) {
        m_importMaxLongitude = settings.m_importMaxLongitude;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
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
    if (settingsKeys.contains("reverseAPIDeviceIndex")) {
        m_reverseAPIDeviceIndex = settings.m_reverseAPIDeviceIndex;
    }
    if (settingsKeys.contains("columnIndexes")) {
        std::copy(std::begin(settings.m_columnIndexes), std::end(settings.m_columnIndexes), std::begin(m_columnIndexes));
    }
    if (settingsKeys.contains("columnSizes")) {
        std::copy(std::begin(settings.m_columnSizes), std::end(settings.m_columnIndexes), std::begin(m_columnSizes));
    }
    if (settingsKeys.contains("airportRange")) {
        m_airportRange = settings.m_airportRange;
    }
    if (settingsKeys.contains("airportMinimumSize")) {
        m_airportMinimumSize = settings.m_airportMinimumSize;
    }
    if (settingsKeys.contains("displayHeliports")) {
        m_displayHeliports = settings.m_displayHeliports;
    }
    if (settingsKeys.contains("flightPaths")) {
        m_flightPaths = settings.m_flightPaths;
    }
    if (settingsKeys.contains("allFlightPaths")) {
        m_allFlightPaths = settings.m_allFlightPaths;
    }
    if (settingsKeys.contains("siUnits")) {
        m_siUnits = settings.m_siUnits;
    }
    if (settingsKeys.contains("tableFontName")) {
        m_tableFontName = settings.m_tableFontName;
    }
    if (settingsKeys.contains("tableFontSize")) {
        m_tableFontSize = settings.m_tableFontSize;
    }
    if (settingsKeys.contains("displayDemodStats")) {
        m_displayDemodStats = settings.m_displayDemodStats;
    }
    if (settingsKeys.contains("demodModeS")) {
        m_demodModeS = settings.m_demodModeS;
    }
    if (settingsKeys.contains("amDemod")) {
        m_amDemod = settings.m_amDemod;
    }
    if (settingsKeys.contains("autoResizeTableColumns")) {
        m_autoResizeTableColumns = settings.m_autoResizeTableColumns;
    }
    if (settingsKeys.contains("interpolatorPhaseSteps")) {
        m_interpolatorPhaseSteps = settings.m_interpolatorPhaseSteps;
    }
    if (settingsKeys.contains("interpolatorTapsPerPhase")) {
        m_interpolatorTapsPerPhase = settings.m_interpolatorTapsPerPhase;
    }
    if (settingsKeys.contains("notificationSettings")) {
        m_notificationSettings = settings.m_notificationSettings;
    }
    if (settingsKeys.contains("aviationstackAPIKey")) {
        m_aviationstackAPIKey = settings.m_aviationstackAPIKey;
    }
    if (settingsKeys.contains("checkWXAPIKey")) {
        m_checkWXAPIKey = settings.m_checkWXAPIKey;
    }
    if (settingsKeys.contains("maptilerAPIKey")) {
        m_maptilerAPIKey = settings.m_maptilerAPIKey;
    }
    if (settingsKeys.contains("logFilename")) {
        m_logFilename = settings.m_logFilename;
    }
    if (settingsKeys.contains("logEnabled")) {
        m_logEnabled = settings.m_logEnabled;
    }
    if (settingsKeys.contains("airspaces")) {
        m_airspaces = settings.m_airspaces;
    }
    if (settingsKeys.contains("airspaceRange")) {
        m_airspaceRange = settings.m_airspaceRange;
    }
    if (settingsKeys.contains("mapProvider")) {
        m_mapProvider = settings.m_mapProvider;
    }
    if (settingsKeys.contains("mapType")) {
        m_mapType = settings.m_mapType;
    }
    if (settingsKeys.contains("displayNavAids")) {
        m_displayNavAids = settings.m_displayNavAids;
    }
    if (settingsKeys.contains("displayPhotos")) {
        m_displayPhotos = settings.m_displayPhotos;
    }
    if (settingsKeys.contains("verboseModelMatching")) {
        m_verboseModelMatching = settings.m_verboseModelMatching;
    }
    if (settingsKeys.contains("aircraftMinZoom")) {
        m_aircraftMinZoom = settings.m_aircraftMinZoom;
    }
    if (settingsKeys.contains("atcLabels")) {
        m_atcLabels = settings.m_atcLabels;
    }
    if (settingsKeys.contains("atcCallsigns")) {
        m_atcCallsigns = settings.m_atcCallsigns;
    }
    if (settingsKeys.contains("transitionAlt")) {
        m_transitionAlt = settings.m_transitionAlt;
    }
    if (settingsKeys.contains("qnh")) {
        m_qnh = settings.m_qnh;
    }
    if (settingsKeys.contains("manualQNH")) {
        m_manualQNH = settings.m_manualQNH;
    }
    if (settingsKeys.contains("displayCoverage")) {
        m_displayCoverage = settings.m_displayCoverage;
    }
    if (settingsKeys.contains("displayChart")) {
        m_displayChart = settings.m_displayChart;
    }
    if (settingsKeys.contains("displayOrientation")) {
        m_displayOrientation = settings.m_displayOrientation;
    }
    if (settingsKeys.contains("displayRadius")) {
        m_displayRadius = settings.m_displayRadius;
    }
    if (settingsKeys.contains("flightPathPaletteName"))
    {
        m_flightPathPaletteName = settings.m_flightPathPaletteName;
        applyPalette();
    }
    if (settingsKeys.contains("favourLivery")) {
        m_favourLivery = settings.m_favourLivery;
    }
}

QString ADSBDemodSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth") || force) {
        ostr << " m_rfBandwidth: " << m_rfBandwidth;
    }
    if (settingsKeys.contains("correlationThreshold") || force) {
        ostr << " m_correlationThreshold: " << m_correlationThreshold;
    }
    if (settingsKeys.contains("chipsThreshold") || force) {
        ostr << " m_chipsThreshold: " << m_chipsThreshold;
    }
    if (settingsKeys.contains("samplesPerBit") || force) {
        ostr << " m_samplesPerBit: " << m_samplesPerBit;
    }
    if (settingsKeys.contains("removeTimeout") || force) {
        ostr << " m_removeTimeout: " << m_removeTimeout;
    }
    if (settingsKeys.contains("feedEnabled") || force) {
        ostr << " m_feedEnabled: " << m_feedEnabled;
    }
    if (settingsKeys.contains("exportClientEnabled") || force) {
        ostr << " m_exportClientEnabled: " << m_exportClientEnabled;
    }
    if (settingsKeys.contains("exportClientHost") || force) {
        ostr << " m_exportClientHost: " << m_exportClientHost.toStdString();
    }
    if (settingsKeys.contains("exportClientPort") || force) {
        ostr << " m_exportClientPort: " << m_exportClientPort;
    }
    if (settingsKeys.contains("exportClientFormat") || force) {
        ostr << " m_exportClientFormat: " << m_exportClientFormat;
    }
    if (settingsKeys.contains("exportServerEnabled") || force) {
        ostr << " m_exportServerEnabled: " << m_exportServerEnabled;
    }
    if (settingsKeys.contains("exportServerPort") || force) {
        ostr << " m_exportServerPort: " << m_exportServerPort;
    }
    if (settingsKeys.contains("importEnabled") || force) {
        ostr << " m_importEnabled: " << m_importEnabled;
    }
    if (settingsKeys.contains("importHost") || force) {
        ostr << " m_importHost: " << m_importHost.toStdString();
    }
    if (settingsKeys.contains("importUsername") || force) {
        ostr << " m_importUsername: " << m_importUsername.toStdString();
    }
    if (settingsKeys.contains("importPassword") || force) {
        ostr << " m_importPassword: " << m_importPassword.toStdString();
    }
    if (settingsKeys.contains("importParameters") || force) {
        ostr << " m_importParameters: " << m_importParameters.toStdString();
    }
    if (settingsKeys.contains("importPeriod") || force) {
        ostr << " m_importPeriod: " << m_importPeriod;
    }
    if (settingsKeys.contains("importMinLatitude") || force) {
        ostr << " m_importMinLatitude: " << m_importMinLatitude.toStdString();
    }
    if (settingsKeys.contains("importMaxLatitude") || force) {
        ostr << " m_importMaxLatitude: " << m_importMaxLatitude.toStdString();
    }
    if (settingsKeys.contains("importMinLongitude") || force) {
        ostr << " m_importMinLongitude: " << m_importMinLongitude.toStdString();
    }
    if (settingsKeys.contains("importMaxLongitude") || force) {
        ostr << " m_importMaxLongitude: " << m_importMaxLongitude.toStdString();
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
    if (settingsKeys.contains("reverseAPIDeviceIndex") || force) {
        ostr << " m_reverseAPIDeviceIndex: " << m_reverseAPIDeviceIndex;
    }
    if (settingsKeys.contains("airportRange") || force) {
        ostr << " m_airportRange: " << m_airportRange;
    }
    if (settingsKeys.contains("airportMinimumSize") || force) {
        ostr << " m_airportMinimumSize: " << m_airportMinimumSize;
    }
    if (settingsKeys.contains("displayHeliports") || force) {
        ostr << " m_displayHeliports: " << m_displayHeliports;
    }
    if (settingsKeys.contains("flightPaths") || force) {
        ostr << " m_flightPaths: " << m_flightPaths;
    }
    if (settingsKeys.contains("allFlightPaths") || force) {
        ostr << " m_allFlightPaths: " << m_allFlightPaths;
    }
    if (settingsKeys.contains("siUnits") || force) {
        ostr << " m_siUnits: " << m_siUnits;
    }
    if (settingsKeys.contains("tableFontName") || force) {
        ostr << " m_tableFontName: " << m_tableFontName.toStdString();
    }
    if (settingsKeys.contains("tableFontSize") || force) {
        ostr << " m_tableFontSize: " << m_tableFontSize;
    }
    if (settingsKeys.contains("displayDemodStats") || force) {
        ostr << " m_displayDemodStats: " << m_displayDemodStats;
    }
    if (settingsKeys.contains("demodModeS") || force) {
        ostr << " m_demodModeS: " << m_demodModeS;
    }
    if (settingsKeys.contains("amDemod") || force) {
        ostr << " m_amDemod: " << m_amDemod.toStdString();
    }
    if (settingsKeys.contains("autoResizeTableColumns") || force) {
        ostr << " m_autoResizeTableColumns: " << m_autoResizeTableColumns;
    }
    if (settingsKeys.contains("interpolatorPhaseSteps") || force) {
        ostr << " m_interpolatorPhaseSteps: " << m_interpolatorPhaseSteps;
    }
    if (settingsKeys.contains("notificationSettings") || force) {
        //ostr << " m_notificationSettings: " << m_notificationSettings.join(",").toStdString();
    }
    if (settingsKeys.contains("aviationstackAPIKey") || force) {
        ostr << " m_aviationstackAPIKey: " << m_aviationstackAPIKey.toStdString();
    }
    if (settingsKeys.contains("checkWXAPIKey") || force) {
        ostr << " m_checkWXAPIKey: " << m_checkWXAPIKey.toStdString();
    }
    if (settingsKeys.contains("maptilerAPIKey") || force) {
        ostr << " m_maptilerAPIKey: " << m_maptilerAPIKey.toStdString();
    }
    if (settingsKeys.contains("logFilename") || force) {
        ostr << " m_logFilename: " << m_logFilename.toStdString();
    }
    if (settingsKeys.contains("logEnabled") || force) {
        ostr << " m_logEnabled: " << m_logEnabled;
    }
    if (settingsKeys.contains("airspaces") || force) {
        ostr << " m_airspaces: " << m_airspaces.join(",").toStdString();
    }
    if (settingsKeys.contains("airspaceRange") || force) {
        ostr << " m_airspaceRange: " << m_airspaceRange;
    }
    if (settingsKeys.contains("mapProvider") || force) {
        ostr << " m_mapProvider: " << m_mapProvider.toStdString();
    }
    if (settingsKeys.contains("mapType") || force) {
        ostr << " m_mapType: " << m_mapType;
    }
    if (settingsKeys.contains("displayNavAids") || force) {
        ostr << " m_displayNavAids: " << m_displayNavAids;
    }
    if (settingsKeys.contains("displayPhotos") || force) {
        ostr << " m_displayPhotos: " << m_displayPhotos;
    }
    if (settingsKeys.contains("verboseModelMatching") || force) {
        ostr << " m_verboseModelMatching: " << m_verboseModelMatching;
    }
    if (settingsKeys.contains("aircraftMinZoom") || force) {
        ostr << " m_aircraftMinZoom: " << m_aircraftMinZoom;
    }
    if (settingsKeys.contains("atcLabels") || force) {
        ostr << " m_atcLabels: " << m_atcLabels;
    }
    if (settingsKeys.contains("atcCallsigns") || force) {
        ostr << " m_atcCallsigns: " << m_atcCallsigns;
    }
    if (settingsKeys.contains("transitionAlt") || force) {
        ostr << " m_transitionAlt: " << m_transitionAlt;
    }
    if (settingsKeys.contains("qnh") || force) {
        ostr << " m_qnh: " << m_qnh;
    }
    if (settingsKeys.contains("manualQNH") || force) {
        ostr << " m_manualQNH: " << m_manualQNH;
    }
    if (settingsKeys.contains("displayCoverage") || force) {
        ostr << " m_displayCoverage: " << m_displayCoverage;
    }
    if (settingsKeys.contains("displayChart") || force) {
        ostr << " m_displayChart: " << m_displayChart;
    }
    if (settingsKeys.contains("displayOrientation") || force) {
        ostr << " m_displayOrientation: " << m_displayOrientation;
    }
    if (settingsKeys.contains("displayRadius") || force) {
        ostr << " m_displayRadius: " << m_displayRadius;
    }
    if (settingsKeys.contains("flightPathPaletteName") || force) {
        ostr << " m_flightPathPaletteName: " << m_flightPathPaletteName.toStdString();
    }
    if (settingsKeys.contains("favourLivery") || force) {
        ostr << " m_favourLivery: " << m_favourLivery;
    }

    return QString(ostr.str().c_str());
}

void ADSBDemodSettings::applyPalette()
{
    if (m_palettes.contains(m_flightPathPaletteName)) {
        m_flightPathPalette = m_palettes.value(m_flightPathPaletteName);
    } else {
        m_flightPathPalette = m_rainbowPalette;
    }
}

const QVariant ADSBDemodSettings::m_rainbowPalette[8] = {
    QVariant(QColor(0xff, 0x00, 0x00)),
    QVariant(QColor(0xff, 0x7f, 0x00)),
    QVariant(QColor(0xff, 0xff, 0x00)),
    QVariant(QColor(0xf7, 0xff, 0x00)),
    QVariant(QColor(0x00, 0xff, 0x00)),
    QVariant(QColor(0x00, 0xff, 0x7f)),
    QVariant(QColor(0x00, 0xff, 0xff)),
    QVariant(QColor(0x00, 0x7f, 0xff)),
};

const QVariant ADSBDemodSettings::m_pastelPalette[8] = {
    QVariant(QColor(0xff, 0xad, 0xad)),
    QVariant(QColor(0xff, 0xd6, 0xa5)),
    QVariant(QColor(0xfd, 0xff, 0xb6)),
    QVariant(QColor(0xca, 0xff, 0xbf)),
    QVariant(QColor(0x9b, 0xf6, 0xff)),
    QVariant(QColor(0xa0, 0xc4, 0xff)),
    QVariant(QColor(0xbd, 0xb2, 0xff)),
    QVariant(QColor(0xff, 0xc6, 0xff)),
};

const QVariant ADSBDemodSettings::m_spectralPalette[8] = {
    QVariant(QColor(0xd5, 0x3e, 0x4f)),
    QVariant(QColor(0xf4, 0x6d, 0x43)),
    QVariant(QColor(0xfd, 0xae, 0x61)),
    QVariant(QColor(0xfe, 0xe0, 0x8b)),
    QVariant(QColor(0xe6, 0xf5, 0x98)),
    QVariant(QColor(0xab, 0xdd, 0xa4)),
    QVariant(QColor(0x66, 0xc2, 0xa5)),
    QVariant(QColor(0x32, 0x88, 0xbd)),
};

const QVariant ADSBDemodSettings::m_bluePalette[8] = {
    QVariant(QColor(0xde, 0xeb, 0xf7)),
    QVariant(QColor(0xc6, 0xdb, 0xef)),
    QVariant(QColor(0x9e, 0xca, 0xe1)),
    QVariant(QColor(0x6b, 0xae, 0xd6)),
    QVariant(QColor(0x42, 0x92, 0xc6)),
    QVariant(QColor(0x21, 0x71, 0xb5)),
    QVariant(QColor(0x08, 0x51, 0x9c)),
    QVariant(QColor(0x08, 0x30, 0x6b)),
};

const QVariant ADSBDemodSettings::m_purplePalette[8] = {
    QVariant(QColor(0xcc, 0xaf, 0xf2)),
    QVariant(QColor(0xb6, 0x99, 0xe0)),
    QVariant(QColor(0xa0, 0x84, 0xcf)),
    QVariant(QColor(0x8a, 0x6f, 0xbd)),
    QVariant(QColor(0x76, 0x5a, 0xac)),
    QVariant(QColor(0x62, 0x45, 0x9a)),
    QVariant(QColor(0x4f, 0x30, 0x89)),
    QVariant(QColor(0x3e, 0x18, 0x78)),
};

const QVariant ADSBDemodSettings::m_greyPalette[8] = {
    QVariant(QColor(0x80, 0x80, 0x80)),
    QVariant(QColor(0x80, 0x80, 0x80)),
    QVariant(QColor(0x80, 0x80, 0x80)),
    QVariant(QColor(0x80, 0x80, 0x80)),
    QVariant(QColor(0x80, 0x80, 0x80)),
    QVariant(QColor(0x80, 0x80, 0x80)),
    QVariant(QColor(0x80, 0x80, 0x80)),
    QVariant(QColor(0x80, 0x80, 0x80)),
};

const QHash<QString, const QVariant *> ADSBDemodSettings::m_palettes = {
    {"Rainbow", &ADSBDemodSettings::m_rainbowPalette[0]},
    {"Pastel", &ADSBDemodSettings::m_pastelPalette[0]},
    {"Spectral", &ADSBDemodSettings::m_spectralPalette[0]},
    {"Blues", &ADSBDemodSettings::m_bluePalette[0]},
    {"Purples", &ADSBDemodSettings::m_purplePalette[0]},
    {"Grey", &ADSBDemodSettings::m_greyPalette[0]},
};