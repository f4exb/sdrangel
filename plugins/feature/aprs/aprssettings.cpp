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

#include "aprssettings.h"

const QStringList APRSSettings::m_pipeTypes = {
    QStringLiteral("PacketDemod"),
    QStringLiteral("ChirpChatDemod"),
    QStringLiteral("M17Demod")
};

const QStringList APRSSettings::m_pipeURIs = {
    QStringLiteral("sdrangel.channel.packetdemod"),
    QStringLiteral("sdrangel.channel.chirpchatdemod"),
    QStringLiteral("sdrangel.channel.m17demod")
};

const QStringList APRSSettings::m_altitudeUnitNames = {
    QStringLiteral("Feet"),
    QStringLiteral("Metres")
};

const QStringList APRSSettings::m_speedUnitNames = {
    QStringLiteral("Knots"),
    QStringLiteral("MPH"),
    QStringLiteral("KPH")
};

const QStringList APRSSettings::m_temperatureUnitNames = {
    QStringLiteral("F"),
    QStringLiteral("C")
};


APRSSettings::APRSSettings() :
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void APRSSettings::resetToDefaults()
{
    m_igateServer = "noam.aprs2.net";
    m_igatePort = 14580;
    m_igateCallsign = "";
    m_igatePasscode = "";
    m_igateFilter = "m/10";
    m_igateEnabled = false;
    m_stationFilter = ALL;
    m_filterAddressee = "";
    m_altitudeUnits = FEET;
    m_speedUnits = KNOTS;
    m_temperatureUnits = FAHRENHEIT;
    m_rainfallUnits = HUNDREDTHS_OF_AN_INCH;
    m_title = "APRS";
    m_rgbColor = QColor(225, 25, 99).rgb();
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIFeatureSetIndex = 0;
    m_reverseAPIFeatureIndex = 0;
    m_workspaceIndex = 0;

    for (int i = 0; i < APRS_PACKETS_TABLE_COLUMNS; i++)
    {
        m_packetsTableColumnIndexes[i] = i;
        m_packetsTableColumnSizes[i] = -1; // Autosize
    }

    for (int i = 0; i < APRS_WEATHER_TABLE_COLUMNS; i++)
    {
        m_weatherTableColumnIndexes[i] = i;
        m_weatherTableColumnSizes[i] = -1; // Autosize
    }

    for (int i = 0; i < APRS_STATUS_TABLE_COLUMNS; i++)
    {
        m_statusTableColumnIndexes[i] = i;
        m_statusTableColumnSizes[i] = -1; // Autosize
    }

    for (int i = 0; i < APRS_MESSAGES_TABLE_COLUMNS; i++)
    {
        m_messagesTableColumnIndexes[i] = i;
        m_messagesTableColumnSizes[i] = -1; // Autosize
    }

    for (int i = 0; i < APRS_TELEMETRY_TABLE_COLUMNS; i++)
    {
        m_telemetryTableColumnIndexes[i] = i;
        m_telemetryTableColumnSizes[i] = -1; // Autosize
    }

    for (int i = 0; i < APRS_MOTION_TABLE_COLUMNS; i++)
    {
        m_motionTableColumnIndexes[i] = i;
        m_motionTableColumnSizes[i] = -1; // Autosize
    }
}

QByteArray APRSSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(1, m_igateServer);
    s.writeS32(2, m_igatePort);
    s.writeString(3, m_igateCallsign);
    s.writeString(4, m_igatePasscode);
    s.writeString(5, m_igateFilter);
    s.writeBool(6, m_igateEnabled);
    s.writeS32(7, m_stationFilter);
    s.writeString(8, m_filterAddressee);
    s.writeString(9, m_title);
    s.writeU32(10, m_rgbColor);
    s.writeBool(11, m_useReverseAPI);
    s.writeString(12, m_reverseAPIAddress);
    s.writeU32(13, m_reverseAPIPort);
    s.writeU32(14, m_reverseAPIFeatureSetIndex);
    s.writeU32(15, m_reverseAPIFeatureIndex);
    s.writeS32(16, (int)m_altitudeUnits);
    s.writeS32(17, (int)m_speedUnits);
    s.writeS32(18, (int)m_temperatureUnits);
    s.writeS32(19, (int)m_rainfallUnits);

    if (m_rollupState) {
        s.writeBlob(20, m_rollupState->serialize());
    }

    s.writeS32(21, m_workspaceIndex);
    s.writeBlob(22, m_geometryBytes);

    for (int i = 0; i < APRS_PACKETS_TABLE_COLUMNS; i++)
        s.writeS32(100 + i, m_packetsTableColumnIndexes[i]);
    for (int i = 0; i < APRS_PACKETS_TABLE_COLUMNS; i++)
        s.writeS32(200 + i, m_packetsTableColumnSizes[i]);
    for (int i = 0; i < APRS_WEATHER_TABLE_COLUMNS; i++)
        s.writeS32(300 + i, m_weatherTableColumnIndexes[i]);
    for (int i = 0; i < APRS_WEATHER_TABLE_COLUMNS; i++)
        s.writeS32(400 + i, m_weatherTableColumnSizes[i]);
    for (int i = 0; i < APRS_STATUS_TABLE_COLUMNS; i++)
        s.writeS32(500 + i, m_statusTableColumnIndexes[i]);
    for (int i = 0; i < APRS_STATUS_TABLE_COLUMNS; i++)
        s.writeS32(600 + i, m_statusTableColumnSizes[i]);
    for (int i = 0; i < APRS_MESSAGES_TABLE_COLUMNS; i++)
        s.writeS32(700 + i, m_messagesTableColumnIndexes[i]);
    for (int i = 0; i < APRS_MESSAGES_TABLE_COLUMNS; i++)
        s.writeS32(800 + i, m_messagesTableColumnSizes[i]);
    for (int i = 0; i < APRS_TELEMETRY_TABLE_COLUMNS; i++)
        s.writeS32(900 + i, m_telemetryTableColumnIndexes[i]);
    for (int i = 0; i < APRS_TELEMETRY_TABLE_COLUMNS; i++)
        s.writeS32(1000 + i, m_telemetryTableColumnSizes[i]);
    for (int i = 0; i < APRS_MOTION_TABLE_COLUMNS; i++)
        s.writeS32(1100 + i, m_motionTableColumnIndexes[i]);
    for (int i = 0; i < APRS_MOTION_TABLE_COLUMNS; i++)
        s.writeS32(1200 + i, m_motionTableColumnSizes[i]);

    return s.final();
}

bool APRSSettings::deserialize(const QByteArray& data)
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

        d.readString(1, &m_igateServer, "noam.aprs2.net");
        d.readS32(2, &m_igatePort, 14580);
        d.readString(3, &m_igateCallsign, "");
        d.readString(4, &m_igatePasscode, "");
        d.readString(5, &m_igateFilter, "");
        d.readBool(6, &m_igateEnabled, false);
        d.readS32(7, (int*)&m_stationFilter, 0);
        d.readString(8, &m_filterAddressee, "");
        d.readString(9, &m_title, "APRS");
        d.readU32(10, &m_rgbColor, QColor(225, 25, 99).rgb());
        d.readBool(11, &m_useReverseAPI, false);
        d.readString(12, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(13, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(14, &utmp, 0);
        m_reverseAPIFeatureSetIndex = utmp > 99 ? 99 : utmp;
        d.readU32(15, &utmp, 0);
        m_reverseAPIFeatureIndex = utmp > 99 ? 99 : utmp;
        d.readS32(16, (int *)&m_altitudeUnits, (int)FEET);
        d.readS32(17, (int *)&m_speedUnits, (int)KNOTS);
        d.readS32(18, (int *)&m_temperatureUnits, (int)FAHRENHEIT);
        d.readS32(19, (int *)&m_rainfallUnits, (int)HUNDREDTHS_OF_AN_INCH);

        if (m_rollupState)
        {
            d.readBlob(20, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(21, &m_workspaceIndex, 0);
        d.readBlob(22, &m_geometryBytes);

        for (int i = 0; i < APRS_PACKETS_TABLE_COLUMNS; i++)
            d.readS32(100 + i, &m_packetsTableColumnIndexes[i], i);
        for (int i = 0; i < APRS_PACKETS_TABLE_COLUMNS; i++)
            d.readS32(200 + i, &m_packetsTableColumnSizes[i], -1);
        for (int i = 0; i < APRS_WEATHER_TABLE_COLUMNS; i++)
            d.readS32(300 + i, &m_weatherTableColumnIndexes[i], i);
        for (int i = 0; i < APRS_WEATHER_TABLE_COLUMNS; i++)
            d.readS32(400 + i, &m_weatherTableColumnSizes[i], -1);
        for (int i = 0; i < APRS_STATUS_TABLE_COLUMNS; i++)
            d.readS32(500 + i, &m_statusTableColumnIndexes[i], i);
        for (int i = 0; i < APRS_STATUS_TABLE_COLUMNS; i++)
            d.readS32(600 + i, &m_statusTableColumnSizes[i], -1);
        for (int i = 0; i < APRS_MESSAGES_TABLE_COLUMNS; i++)
            d.readS32(700 + i, &m_messagesTableColumnIndexes[i], i);
        for (int i = 0; i < APRS_MESSAGES_TABLE_COLUMNS; i++)
            d.readS32(800 + i, &m_messagesTableColumnSizes[i], -1);
        for (int i = 0; i < APRS_TELEMETRY_TABLE_COLUMNS; i++)
            d.readS32(900 + i, &m_telemetryTableColumnIndexes[i], i);
        for (int i = 0; i < APRS_TELEMETRY_TABLE_COLUMNS; i++)
            d.readS32(1000 + i, &m_telemetryTableColumnSizes[i], -1);
        for (int i = 0; i < APRS_MOTION_TABLE_COLUMNS; i++)
            d.readS32(1100 + i, &m_motionTableColumnIndexes[i], i);
        for (int i = 0; i < APRS_MOTION_TABLE_COLUMNS; i++)
            d.readS32(1200 + i, &m_motionTableColumnSizes[i], -1);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void APRSSettings::applySettings(const QStringList& settingsKeys, const APRSSettings& settings)
{
    if (settingsKeys.contains("igateServer")) {
        m_igateServer = settings.m_igateServer;
    }
    if (settingsKeys.contains("igatePort")) {
        m_igatePort = settings.m_igatePort;
    }
    if (settingsKeys.contains("igateCallsign")) {
        m_igateCallsign = settings.m_igateCallsign;
    }
    if (settingsKeys.contains("igatePasscode")) {
        m_igatePasscode = settings.m_igatePasscode;
    }
    if (settingsKeys.contains("igateFilter")) {
        m_igateFilter = settings.m_igateFilter;
    }
    if (settingsKeys.contains("igateEnabled")) {
        m_igateEnabled = settings.m_igateEnabled;
    }
    if (settingsKeys.contains("stationFilter")) {
        m_stationFilter = settings.m_stationFilter;
    }
    if (settingsKeys.contains("filterAddressee")) {
        m_filterAddressee = settings.m_filterAddressee;
    }
    if (settingsKeys.contains("altitudeUnits")) {
        m_altitudeUnits = settings.m_altitudeUnits;
    }
    if (settingsKeys.contains("speedUnits")) {
        m_speedUnits = settings.m_speedUnits;
    }
    if (settingsKeys.contains("temperatureUnits")) {
        m_temperatureUnits = settings.m_temperatureUnits;
    }
    if (settingsKeys.contains("rainfallUnits")) {
        m_rainfallUnits = settings.m_rainfallUnits;
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
    if (settingsKeys.contains("workspaceIndex")) {
        m_workspaceIndex = settings.m_workspaceIndex;
    }
}

QString APRSSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("igateServer") || force) {
        ostr << " m_igateServer: " << m_igateServer.toStdString();
    }
    if (settingsKeys.contains("igatePort") || force) {
        ostr << " m_igatePort: " << m_igatePort;
    }
    if (settingsKeys.contains("igateCallsign") || force) {
        ostr << " m_igateCallsign: " << m_igateCallsign.toStdString();
    }
    if (settingsKeys.contains("igatePasscode") || force) {
        ostr << " m_igatePasscode: " << m_igatePasscode.toStdString();
    }
    if (settingsKeys.contains("igateFilter") || force) {
        ostr << " m_igateFilter: " << m_igateFilter.toStdString();
    }
    if (settingsKeys.contains("igateEnabled") || force) {
        ostr << " m_igateEnabled: " << m_igateEnabled;
    }
    if (settingsKeys.contains("stationFilter") || force) {
        ostr << " m_stationFilter: " << m_stationFilter;
    }
    if (settingsKeys.contains("filterAddressee") || force) {
        ostr << " m_filterAddressee: " << m_filterAddressee.toStdString();
    }
    if (settingsKeys.contains("altitudeUnits") || force) {
        ostr << " m_altitudeUnits: " << m_altitudeUnits;
    }
    if (settingsKeys.contains("speedUnits") || force) {
        ostr << " m_speedUnits: " << m_speedUnits;
    }
    if (settingsKeys.contains("temperatureUnits") || force) {
        ostr << " m_temperatureUnits: " << m_temperatureUnits;
    }
    if (settingsKeys.contains("rainfallUnits") || force) {
        ostr << " m_rainfallUnits: " << m_rainfallUnits;
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
    if (settingsKeys.contains("workspaceIndex") || force) {
        ostr << " m_workspaceIndex: " << m_workspaceIndex;
    }

    // 1
    if (settingsKeys.contains("packetsTableColumnIndexes"))
    {
        ostr << "m_packetsTableColumnIndexes:";

        for (int i = 0; i < APRS_PACKETS_TABLE_COLUMNS; i++) {
            ostr << " " << m_packetsTableColumnIndexes[i];
        }
    }

    // 2
    if (settingsKeys.contains("packetsTableColumnSizes"))
    {
        ostr << "m_packetsTableColumnSizes:";

        for (int i = 0; i < APRS_PACKETS_TABLE_COLUMNS; i++) {
            ostr << " " << m_packetsTableColumnSizes[i];
        }
    }

    // 3
    if (settingsKeys.contains("weatherTableColumnIndexes"))
    {
        ostr << "m_weatherTableColumnIndexes:";

        for (int i = 0; i < APRS_WEATHER_TABLE_COLUMNS; i++) {
            ostr << " " << m_weatherTableColumnIndexes[i];
        }
    }

    // 4
    if (settingsKeys.contains("weatherTableColumnSizes"))
    {
        ostr << "m_weatherTableColumnSizes:";

        for (int i = 0; i < APRS_WEATHER_TABLE_COLUMNS; i++) {
            ostr << " " << m_weatherTableColumnSizes[i];
        }
    }

    // 5
    if (settingsKeys.contains("statusTableColumnIndexes"))
    {
        ostr << "m_statusTableColumnIndexes:";

        for (int i = 0; i < APRS_STATUS_TABLE_COLUMNS; i++) {
            ostr << " " << m_statusTableColumnIndexes[i];
        }
    }

    // 6
    if (settingsKeys.contains("statusTableColumnSizes"))
    {
        ostr << "m_statusTableColumnSizes:";

        for (int i = 0; i < APRS_STATUS_TABLE_COLUMNS; i++) {
            ostr << " " << m_statusTableColumnSizes[i];
        }
    }

    // 7
    if (settingsKeys.contains("messagesTableColumnIndexes"))
    {
        ostr << "m_messagesTableColumnIndexes:";

        for (int i = 0; i < APRS_MESSAGES_TABLE_COLUMNS; i++) {
            ostr << " " << m_messagesTableColumnIndexes[i];
        }
    }

    // 8
    if (settingsKeys.contains("messagesTableColumnSizes"))
    {
        ostr << "m_messagesTableColumnSizes:";

        for (int i = 0; i < APRS_MESSAGES_TABLE_COLUMNS; i++) {
            ostr << " " << m_messagesTableColumnSizes[i];
        }
    }

    // 9
    if (settingsKeys.contains("telemetryTableColumnIndexes"))
    {
        ostr << "m_telemetryTableColumnIndexes:";

        for (int i = 0; i < APRS_TELEMETRY_TABLE_COLUMNS; i++) {
            ostr << " " << m_telemetryTableColumnIndexes[i];
        }
    }

    // 10
    if (settingsKeys.contains("telemetryTableColumnSizes"))
    {
        ostr << "m_telemetryTableColumnSizes:";

        for (int i = 0; i < APRS_TELEMETRY_TABLE_COLUMNS; i++) {
            ostr << " " << m_telemetryTableColumnSizes[i];
        }
    }

    // 11
    if (settingsKeys.contains("motionTableColumnIndexes"))
    {
        ostr << "m_telemetryTableColumnSizes:";

        for (int i = 0; i < APRS_MOTION_TABLE_COLUMNS; i++) {
            ostr << " " << m_motionTableColumnIndexes[i];
        }
    }

    // 12
    if (settingsKeys.contains("motionTableColumnSizes"))
    {
        ostr << "m_motionTableColumnSizes:";

        for (int i = 0; i < APRS_MOTION_TABLE_COLUMNS; i++) {
            ostr << " " << m_motionTableColumnSizes[i];
        }
    }

    return QString(ostr.str().c_str());
}
