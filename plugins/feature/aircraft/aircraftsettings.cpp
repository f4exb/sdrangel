///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include <QStandardPaths>

#include "aircraftsettings.h"

AircraftSettings::AircraftSettings() :
    m_rollupState(nullptr)
{
    resetToDefaults();
}

// The demodulators that report aircraft to this feature. Anything listed is accepted
// unless the user turns it off, so a demodulator added later is included by default.
const QStringList& AircraftSettings::sourceURIs()
{
    // Also the list the feature connects its pipes to, so a demodulator added here is
    // both heard and offered in the settings dialog
    static const QStringList uris = {
        "sdrangel.channel.adsbdemod",
        "sdrangel.channel.acarsdemod",
        "sdrangel.channel.selcaldemod"
    };
    return uris;
}

QString AircraftSettings::sourceName(const QString& uri)
{
    if (uri == "sdrangel.channel.adsbdemod") {
        return "ADS-B Demodulator";
    } else if (uri == "sdrangel.channel.acarsdemod") {
        return "ACARS Demodulator";
    } else if (uri == "sdrangel.channel.selcaldemod") {
        return "SELCAL Demodulator";
    }
    return uri;
}

QString AircraftSettings::defaultDatabaseFilename()
{
    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    return locations[0] + "/aircraft.db";
}

void AircraftSettings::resetToDefaults()
{
    m_title = "Aircraft";
    m_rgbColor = QColor(0, 105, 105).rgb();
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIFeatureSetIndex = 0;
    m_reverseAPIFeatureIndex = 0;
    m_workspaceIndex = 0;
    // HFDL reporting can be very sparse, so aircraft linger well beyond the
    // ADS-B style timeout - they may reappear on HF a long way away
    m_removalMins = 180;
    m_adsbPositionMins = 1;
    m_acarsPositionMins = 10;
    m_retentionDays = 0;    // Keep everything until the user chooses a limit
    m_flightRetentionDays = 0;
    m_atcLabels = true;
    m_findDatalinkAircraft = false;
    m_datalinkNoFindElements = m_datalinkNoFindDefaults;
    m_displayMaxRangeOnMap = false;
    m_atcCallsigns = false;
    m_statistics = QByteArray();
    m_splitterStates = QByteArray();
    m_displayStatistics = true;
    m_displayChart = false;
    m_favourLivery = true;
    m_useLiveryIcons = false;
    m_feedEnabled = false;
    m_feedStationId = "";
    m_feedAirframes = true;
    m_feedAirframesHost = "feed.airframes.io";
    m_feedAirframesPort = 5550;
    m_feedAirframesTcp = false;
    m_feedAvdelphi = false;
    m_feedAvdelphiHost = "data.avdelphi.com";
    m_feedAvdelphiPort = 5556;
    m_feedAvdelphiTcp = false;
    m_displayCoverage = false;
    m_coverageHiddenProtocols = 0;
    m_databaseFilename = "";
    m_disabledSources.clear();
    // Rules that speak and run commands are exactly the kind of thing a reset is asked
    // for to be rid of, and leaving them behind means the reset does not do what it says
    m_notificationSettings.clear();

    for (int i = 0; i < FLIGHT_COLUMNS; i++)
    {
        m_flightColumnIndexes[i] = i;
        m_flightColumnSizes[i] = -1;    // Autosize
    }
    for (int i = 0; i < AIRCRAFT_COLUMNS; i++)
    {
        m_columnIndexes[i] = i;
        m_columnSizes[i] = -1; // Autosize
    }
}

QByteArray AircraftSettings::serialize() const
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
    s.writeS32(30, m_removalMins);
    s.writeS32(41, m_adsbPositionMins);
    s.writeS32(42, m_acarsPositionMins);
    s.writeBlob(31, serializeNotificationSettings(m_notificationSettings));
    // Key 31 used to carry this as well, underneath the blob above, so it never survived
    // a restart - readS32() was being handed a blob and falling back to the default
    s.writeS32(36, m_retentionDays);
    s.writeBool(32, m_atcLabels);
    s.writeBool(44, m_displayMaxRangeOnMap);
    s.writeBool(33, m_atcCallsigns);
    s.writeString(34, m_databaseFilename);
    s.writeList(35, m_disabledSources);
    s.writeBlob(37, m_statistics);
    s.writeBool(38, m_displayStatistics);
    s.writeBool(39, m_displayChart);
    s.writeBlob(43, m_splitterStates);
    s.writeBool(40, m_favourLivery);
    s.writeBool(45, m_useLiveryIcons);
    s.writeBool(46, m_feedEnabled);
    s.writeString(47, m_feedStationId);
    s.writeBool(48, m_feedAirframes);
    s.writeString(49, m_feedAirframesHost);
    s.writeS32(50, m_feedAirframesPort);
    s.writeBool(51, m_feedAirframesTcp);
    s.writeBool(52, m_feedAvdelphi);
    s.writeString(53, m_feedAvdelphiHost);
    s.writeS32(54, m_feedAvdelphiPort);
    s.writeBool(55, m_feedAvdelphiTcp);
    s.writeBool(56, m_displayCoverage);
    s.writeU32(57, m_coverageHiddenProtocols);
    s.writeS32(58, m_flightRetentionDays);
    s.writeBool(59, m_findDatalinkAircraft);
    s.writeList(60, m_datalinkNoFindElements);

    for (int i = 0; i < AIRCRAFT_COLUMNS; i++) {
        s.writeS32(300 + i, m_columnIndexes[i]);
    }
    for (int i = 0; i < FLIGHT_COLUMNS; i++) {
        // 700/800 rather than 500/600: the flight columns were reordered, so a layout
        // saved against the old order describes different columns and must not be
        // applied to them. Reading from new keys makes an old preset fall back to
        // the defaults, which are the new order, exactly once.
        s.writeS32(700 + i, m_flightColumnIndexes[i]);
    }
    for (int i = 0; i < FLIGHT_COLUMNS; i++) {
        s.writeS32(800 + i, m_flightColumnSizes[i]);
    }

    for (int i = 0; i < AIRCRAFT_COLUMNS; i++) {
        s.writeS32(400 + i, m_columnSizes[i]);
    }

    return s.final();
}

bool AircraftSettings::deserialize(const QByteArray& data)
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

        d.readString(20, &m_title, "Aircraft");
        d.readU32(21, &m_rgbColor, QColor(0, 105, 105).rgb());
        d.readBool(22, &m_useReverseAPI, false);
        d.readString(23, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(24, &utmp, 0);

        if ((utmp > 1023) && (utmp <= 65535)) {
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
        d.readS32(30, &m_removalMins, 180);
        d.readS32(41, &m_adsbPositionMins, 1);
        d.readS32(42, &m_acarsPositionMins, 10);
        d.readS32(36, &m_retentionDays, 0);
        d.readS32(58, &m_flightRetentionDays, 0);
        d.readBool(59, &m_findDatalinkAircraft, false);
        d.readList(60, &m_datalinkNoFindElements, m_datalinkNoFindDefaults);
        d.readBlob(31, &bytetmp);
        deserializeNotificationSettings(bytetmp, m_notificationSettings);
        d.readBool(32, &m_atcLabels, true);
        d.readBool(44, &m_displayMaxRangeOnMap, false);
        d.readBool(33, &m_atcCallsigns, false);
        d.readString(34, &m_databaseFilename, "");
        d.readList(35, &m_disabledSources);
        d.readBlob(37, &m_statistics);
        d.readBool(38, &m_displayStatistics, true);
        d.readBool(39, &m_displayChart, false);
        d.readBlob(43, &m_splitterStates);
        d.readBool(40, &m_favourLivery, true);
        d.readBool(45, &m_useLiveryIcons, false);
        d.readBool(46, &m_feedEnabled, false);
        d.readString(47, &m_feedStationId, "");
        d.readBool(48, &m_feedAirframes, true);
        d.readString(49, &m_feedAirframesHost, "feed.airframes.io");
        d.readS32(50, &m_feedAirframesPort, 5550);
        // A port out of range would be refused by the socket and the feed would sit there
        // failing, so a corrupt or hand edited preset falls back to the published one
        if ((m_feedAirframesPort <= 1023) || (m_feedAirframesPort >= 65535)) {
            m_feedAirframesPort = 5550;
        }
        d.readBool(51, &m_feedAirframesTcp, false);
        d.readBool(52, &m_feedAvdelphi, false);
        d.readString(53, &m_feedAvdelphiHost, "data.avdelphi.com");
        d.readS32(54, &m_feedAvdelphiPort, 5556);
        if ((m_feedAvdelphiPort <= 1023) || (m_feedAvdelphiPort >= 65535)) {
            m_feedAvdelphiPort = 5556;
        }
        d.readBool(55, &m_feedAvdelphiTcp, false);
        d.readBool(56, &m_displayCoverage, false);
        d.readU32(57, &m_coverageHiddenProtocols, 0);

        for (int i = 0; i < FLIGHT_COLUMNS; i++) {
            d.readS32(700 + i, &m_flightColumnIndexes[i], i);
        }
        for (int i = 0; i < FLIGHT_COLUMNS; i++) {
            d.readS32(800 + i, &m_flightColumnSizes[i], -1);
        }
        for (int i = 0; i < AIRCRAFT_COLUMNS; i++) {
            d.readS32(300 + i, &m_columnIndexes[i], i);
        }

        for (int i = 0; i < AIRCRAFT_COLUMNS; i++) {
            d.readS32(400 + i, &m_columnSizes[i], -1);
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void AircraftSettings::applySettings(const QStringList& settingsKeys, const AircraftSettings& settings)
{
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
    if (settingsKeys.contains("workspaceIndex")) {
        m_workspaceIndex = settings.m_workspaceIndex;
    }
    if (settingsKeys.contains("removalMins")) {
        m_removalMins = settings.m_removalMins;
    }
    if (settingsKeys.contains("adsbPositionMins")) {
        m_adsbPositionMins = settings.m_adsbPositionMins;
    }
    if (settingsKeys.contains("acarsPositionMins")) {
        m_acarsPositionMins = settings.m_acarsPositionMins;
    }
    if (settingsKeys.contains("retentionDays")) {
        m_retentionDays = settings.m_retentionDays;
    }
    if (settingsKeys.contains("flightRetentionDays")) {
        m_flightRetentionDays = settings.m_flightRetentionDays;
    }
    if (settingsKeys.contains("notificationSettings")) {
        m_notificationSettings = settings.m_notificationSettings;
    }
    if (settingsKeys.contains("atcLabels")) {
        m_atcLabels = settings.m_atcLabels;
    }
    if (settingsKeys.contains("findDatalinkAircraft")) {
        m_findDatalinkAircraft = settings.m_findDatalinkAircraft;
    }
    if (settingsKeys.contains("datalinkNoFindElements")) {
        m_datalinkNoFindElements = settings.m_datalinkNoFindElements;
    }
    if (settingsKeys.contains("displayMaxRangeOnMap")) {
        m_displayMaxRangeOnMap = settings.m_displayMaxRangeOnMap;
    }
    if (settingsKeys.contains("displayStatistics")) {
        m_displayStatistics = settings.m_displayStatistics;
    }
    if (settingsKeys.contains("displayChart")) {
        m_displayChart = settings.m_displayChart;
    }
    if (settingsKeys.contains("displayCoverage")) {
        m_displayCoverage = settings.m_displayCoverage;
    }
    if (settingsKeys.contains("coverageHiddenProtocols")) {
        m_coverageHiddenProtocols = settings.m_coverageHiddenProtocols;
    }
    if (settingsKeys.contains("favourLivery")) {
        m_favourLivery = settings.m_favourLivery;
    }
    if (settingsKeys.contains("useLiveryIcons")) {
        m_useLiveryIcons = settings.m_useLiveryIcons;
    }
    if (settingsKeys.contains("feedEnabled")) {
        m_feedEnabled = settings.m_feedEnabled;
    }
    if (settingsKeys.contains("feedStationId")) {
        m_feedStationId = settings.m_feedStationId;
    }
    if (settingsKeys.contains("feedAirframes")) {
        m_feedAirframes = settings.m_feedAirframes;
    }
    if (settingsKeys.contains("feedAirframesHost")) {
        m_feedAirframesHost = settings.m_feedAirframesHost;
    }
    if (settingsKeys.contains("feedAirframesPort")) {
        m_feedAirframesPort = settings.m_feedAirframesPort;
    }
    if (settingsKeys.contains("feedAirframesTcp")) {
        m_feedAirframesTcp = settings.m_feedAirframesTcp;
    }
    if (settingsKeys.contains("feedAvdelphi")) {
        m_feedAvdelphi = settings.m_feedAvdelphi;
    }
    if (settingsKeys.contains("feedAvdelphiHost")) {
        m_feedAvdelphiHost = settings.m_feedAvdelphiHost;
    }
    if (settingsKeys.contains("feedAvdelphiPort")) {
        m_feedAvdelphiPort = settings.m_feedAvdelphiPort;
    }
    if (settingsKeys.contains("feedAvdelphiTcp")) {
        m_feedAvdelphiTcp = settings.m_feedAvdelphiTcp;
    }
    if (settingsKeys.contains("statistics")) {
        m_statistics = settings.m_statistics;
    }
    if (settingsKeys.contains("atcCallsigns")) {
        m_atcCallsigns = settings.m_atcCallsigns;
    }
    if (settingsKeys.contains("databaseFilename")) {
        m_databaseFilename = settings.m_databaseFilename;
    }
    if (settingsKeys.contains("disabledSources")) {
        m_disabledSources = settings.m_disabledSources;
    }

    if (settingsKeys.contains("flightColumnIndexes"))
    {
        for (int i = 0; i < FLIGHT_COLUMNS; i++) {
            m_flightColumnIndexes[i] = settings.m_flightColumnIndexes[i];
        }
    }
    if (settingsKeys.contains("flightColumnSizes"))
    {
        for (int i = 0; i < FLIGHT_COLUMNS; i++) {
            m_flightColumnSizes[i] = settings.m_flightColumnSizes[i];
        }
    }
    if (settingsKeys.contains("columnIndexes"))
    {
        for (int i = 0; i < AIRCRAFT_COLUMNS; i++) {
            m_columnIndexes[i] = settings.m_columnIndexes[i];
        }
    }

    if (settingsKeys.contains("columnSizes"))
    {
        for (int i = 0; i < AIRCRAFT_COLUMNS; i++) {
            m_columnSizes[i] = settings.m_columnSizes[i];
        }
    }
}

QDataStream& operator<<(QDataStream& out, const QSharedPointer<AircraftSettings::NotificationSettings>& settings)
{
    out << settings->m_matchColumn;
    out << settings->m_regExp;
    out << settings->m_speech;
    out << settings->m_command;
    return out;
}

QDataStream& operator>>(QDataStream& in, QSharedPointer<AircraftSettings::NotificationSettings>& settings)
{
    settings = QSharedPointer<AircraftSettings::NotificationSettings>::create();
    in >> settings->m_matchColumn;
    in >> settings->m_regExp;
    in >> settings->m_speech;
    in >> settings->m_command;
    settings->updateRegularExpression();
    return in;
}

QByteArray AircraftSettings::serializeNotificationSettings(const QList<QSharedPointer<NotificationSettings>>& notificationSettings) const
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << notificationSettings;
    return data;
}

void AircraftSettings::deserializeNotificationSettings(const QByteArray& data, QList<QSharedPointer<NotificationSettings>>& notificationSettings)
{
    QDataStream stream(data);
    stream >> notificationSettings;
}

AircraftSettings::NotificationSettings::NotificationSettings() :
    m_matchColumn(MATCH_REG)
{
}

void AircraftSettings::NotificationSettings::updateRegularExpression()
{
    m_regularExpression.setPattern(m_regExp);
    m_regularExpression.optimize();
    if (!m_regularExpression.isValid()) {
        qDebug() << "Aircraft: Regular expression is not valid: " << m_regExp;
    }
}

QString AircraftSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("title") || force) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("rgbColor") || force) {
        ostr << " m_rgbColor: " << m_rgbColor;
    }
    if (settingsKeys.contains("removalMins") || force) {
        ostr << " m_removalMins: " << m_removalMins;
    }
    if (settingsKeys.contains("adsbPositionMins") || force) {
        ostr << " m_adsbPositionMins: " << m_adsbPositionMins;
    }
    if (settingsKeys.contains("acarsPositionMins") || force) {
        ostr << " m_acarsPositionMins: " << m_acarsPositionMins;
    }
    if (settingsKeys.contains("retentionDays") || force) {
        ostr << " m_retentionDays: " << m_retentionDays;
    }
    if (settingsKeys.contains("flightRetentionDays") || force) {
        ostr << " m_flightRetentionDays: " << m_flightRetentionDays;
    }
    if (settingsKeys.contains("findDatalinkAircraft") || force) {
        ostr << " m_findDatalinkAircraft: " << m_findDatalinkAircraft;
    }
    if (settingsKeys.contains("datalinkNoFindElements") || force) {
        ostr << " m_datalinkNoFindElements: " << m_datalinkNoFindElements.join(", ").toStdString();
    }
    if (settingsKeys.contains("atcCallsigns") || force) {
        ostr << " m_atcCallsigns: " << m_atcCallsigns;
    }
    if (settingsKeys.contains("databaseFilename") || force) {
        ostr << " m_databaseFilename: " << m_databaseFilename.toStdString();
    }
    if (settingsKeys.contains("disabledSources") || force) {
        ostr << " m_disabledSources: " << m_disabledSources.join(",").toStdString();
    }

    return QString(ostr.str().c_str());
}

QDateTime AircraftSettings::messageCutoff() const
{
    int days = m_retentionDays;
    if ((m_flightRetentionDays > 0) && ((days <= 0) || (m_flightRetentionDays < days))) {
        days = m_flightRetentionDays;
    }
    return days > 0 ? QDateTime::currentDateTime().addDays(-days) : QDateTime();
}

// The routine replies and housekeeping, as the two decoders spell them: WILCO and ROGER
// on both paths, and the ATN-only logical acknowledgement and data authority reports.
// ROGER is offered but not checked to start with: it acknowledges a message without
// complying with it, so it can be the end of an exchange worth seeing
const QStringList AircraftSettings::m_datalinkNoFindChoices = {
    "WILCO",
    "ROGER",
    "LOGICAL ACKNOWLEDGEMENT",
    "CURRENT DATA AUTHORITY"
};

const QStringList AircraftSettings::m_datalinkNoFindDefaults = {
    "WILCO",
    "LOGICAL ACKNOWLEDGEMENT",
    "CURRENT DATA AUTHORITY"
};

// Whether a datalink message should find its aircraft: not when every element of it is one
// of those chosen not to. The elements are separated by new lines in the ACARS (FANS-1/A)
// decode and by commas in the VDL-2 ATN one. An element with a comma of its own splits into
// pieces that match nothing, so finds, which is the safe way round
bool AircraftSettings::datalinkFinds(const QString& text) const
{
    static const QRegularExpression separators("\\n|, ");
    const QStringList elements = text.split(separators, Qt::SkipEmptyParts);
    for (const QString& element : elements)
    {
        if (!m_datalinkNoFindElements.contains(element.trimmed(), Qt::CaseInsensitive)) {
            return true;
        }
    }
    return elements.isEmpty();
}
