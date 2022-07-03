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
