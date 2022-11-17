///////////////////////////////////////////////////////////////////////////////////
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

#ifndef INCLUDE_APRS_H
#define INCLUDE_APRS_H

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QDebug>

#include "export.h"
#include "ax25.h"
#include "util/units.h"

struct SDRBASE_API APRSPacket {
    QString m_from;
    QString m_to;
    QString m_via;
    QString m_data;             // Original ASCII data

    QDateTime m_dateTime;       // Date/time of reception / decoding

    // Timestamp (where fields are not transmitted, time of decoding is used)
    QDateTime m_timestamp;
    bool m_utc; // Whether UTC (true) or local time (false)
    bool m_hasTimestamp;

    // Position
    float m_latitude;
    float m_longitude;
    bool m_hasPosition;

    float m_altitudeFt;
    bool m_hasAltitude;

    // Symbol
    char m_symbolTable;
    char m_symbolCode;
    bool m_hasSymbol;
    QString m_symbolImage;      // Image filename for the symbol

    // Course and speed
    int m_course;
    int m_speed;
    bool m_hasCourseAndSpeed;

    // Power, antenna height, gain, directivity
    int m_powerWatts;
    int m_antennaHeightFt;
    int m_antennaGainDB;
    QString m_antennaDirectivity; // Omni, or N, NE...
    bool m_hasStationDetails;

    // Radio range
    int m_radioRangeMiles;
    bool m_hasRadioRange;

    // Omni-DF
    int m_dfStrength;
    int m_dfHeightFt;
    int m_dfGainDB;
    QString m_dfAntennaDirectivity;
    bool m_hasDf;

    QString m_objectName;       // Also used for items
    bool m_objectLive;
    bool m_objectKilled;

    QString m_comment;

    // Weather reports
    int m_windDirection;        // In degrees
    bool m_hasWindDirection;
    int m_windSpeed;            // In mph
    bool m_hasWindSpeed;
    int m_gust;                 // Peak wind speed in last 5 minutes in mph
    bool m_hasGust;
    int m_temp;                 // Fahrenheit, can be negative down to -99
    bool m_hasTemp;
    int m_rainLastHr;           // Hundreths of an inch
    bool m_hasRainLastHr;
    int m_rainLast24Hrs;
    bool m_hasRainLast24Hrs;
    int m_rainSinceMidnight;
    bool m_hasRainSinceMidnight;
    int m_humidity;             // %
    bool m_hasHumidity;
    int m_barometricPressure;   // Tenths of millibars / tenths of hPascal
    bool m_hasBarometricPressure;
    int m_luminosity;           // Watts per m^2
    bool m_hasLuminsoity;
    int m_snowfallLast24Hrs;    // In inches
    bool m_hasSnowfallLast24Hrs;
    int m_rawRainCounter;
    bool m_hasRawRainCounter;
    int m_radiationLevel;
    bool m_hasRadiationLevel;
    int m_floodLevel;           // Tenths of a foot. Can be negative
    bool m_hasFloodLevel;
    int m_batteryVolts;         // Tenths of a volt
    bool m_hasBatteryVolts;
    QString m_weatherUnitType;
    bool m_hasWeather;

    int m_stormDirection;
    int m_stormSpeed;
    QString m_stormType;
    int m_stormSustainedWindSpeed;      // knots
    int m_stormPeakWindGusts;           // knots
    int m_stormCentralPresure;          // millibars/hPascal
    int m_stormRadiusHurricanWinds;     // nautical miles
    int m_stormRadiusTropicalStormWinds;// nautical miles
    int m_stormRadiusWholeGail;         // nautical miles
    bool m_hasStormRadiusWholeGail;
    bool m_hasStormData;

    // Status messages
    QString m_status;
    QString m_maidenhead;
    int m_beamHeading;
    int m_beamPower;
    bool m_hasBeam;
    bool m_hasStatus;

    // Messages
    QString m_addressee;
    QString m_message;
    QString m_messageNo;
    bool m_hasMessage;
    QList <QString> m_telemetryNames;
    QList <QString> m_telemetryLabels;
    double m_telemetryCoefficientsA[5];
    double m_telemetryCoefficientsB[5];
    double m_telemetryCoefficientsC[5];
    int m_hasTelemetryCoefficients;
    int m_telemetryBitSense[8];
    bool m_hasTelemetryBitSense;
    QString m_telemetryProjectName;

    // Telemetry
    int m_seqNo;
    bool m_hasSeqNo;
    int m_a1;
    bool m_a1HasValue;
    int m_a2;
    bool m_a2HasValue;
    int m_a3;
    bool m_a3HasValue;
    int m_a4;
    bool m_a4HasValue;
    int m_a5;
    bool m_a5HasValue;
    bool m_b[8];
    bool m_bHasValue;
    QString m_telemetryComment;
    bool m_hasTelemetry;

    bool decode(AX25Packet packet);

    APRSPacket() :
        m_hasTimestamp(false),
        m_hasPosition(false),
        m_hasAltitude(false),
        m_hasSymbol(false),
        m_hasCourseAndSpeed(false),
        m_hasStationDetails(false),
        m_hasRadioRange(false),
        m_hasDf(false),
        m_objectLive(false),
        m_objectKilled(false),
        m_hasWindDirection(false),
        m_hasWindSpeed(false),
        m_hasGust(false),
        m_hasTemp(false),
        m_hasRainLastHr(false),
        m_hasRainLast24Hrs(false),
        m_hasRainSinceMidnight(false),
        m_hasHumidity(false),
        m_hasBarometricPressure(false),
        m_hasLuminsoity(false),
        m_hasSnowfallLast24Hrs(false),
        m_hasRawRainCounter(false),
        m_hasRadiationLevel(false),
        m_hasFloodLevel(false),
        m_hasBatteryVolts(false),
        m_hasWeather(false),
        m_hasStormRadiusWholeGail(false),
        m_hasStormData(false),
        m_hasBeam(false),
        m_hasStatus(false),
        m_hasMessage(false),
        m_hasTelemetryCoefficients(0),
        m_hasTelemetryBitSense(false),
        m_hasSeqNo(false),
        m_a1HasValue(false),
        m_a2HasValue(false),
        m_a3HasValue(false),
        m_a4HasValue(false),
        m_a5HasValue(false),
        m_bHasValue(false),
        m_hasTelemetry(false)
    {
    }

    QString date()
    {
        if (m_hasTimestamp)
            return m_timestamp.date().toString("yyyy/MM/dd");
        else
            return QString("");
    }

    QString time()
    {
        if (m_hasTimestamp)
            return m_timestamp.time().toString("hh:mm:ss");
        else
            return QString("");
    }

    QString dateTime()
    {
        return QString("%1 %2").arg(date()).arg(time());
    }

    QString position()
    {
        return QString("%1,%2").arg(m_latitude).arg(m_longitude);
    }

    QString toTNC2(QString igateCallsign)
    {
        return m_from + ">" + m_to + (m_via.isEmpty() ? "" : ("," + m_via)) + ",qAR," + igateCallsign + ":" + m_data + "\r\n";
    }

    // Convert a TNC2 formatted packet (as sent by APRS-IS Igates) to an AX25 byte array
    static QByteArray toByteArray(QString tnc2)
    {
        QByteArray bytes;
        QString tmp = "";
        QString from;
        int state = 0;

        for (int i = 0; i < tnc2.length(); i++)
        {
            if (state == 0)
            {
                // From
                if (tnc2[i] == '>')
                {
                    from = tmp;
                    tmp = "";
                    state = 1;
                }
                else
                    tmp.append(tnc2[i]);
            }
            else if (state == 1)
            {
                 // To
                 if (tnc2[i] == ':')
                 {
                    bytes.append(AX25Packet::encodeAddress(tmp));
                    bytes.append(AX25Packet::encodeAddress(from, 1));
                    state = 3;
                 }
                 else if (tnc2[i] == ',')
                 {
                    bytes.append(AX25Packet::encodeAddress(tmp));
                    bytes.append(AX25Packet::encodeAddress(from));
                    tmp = "";
                    state = 2;
                 }
                 else
                    tmp.append(tnc2[i]);
            }
            else if (state == 2)
            {
                 // Via
                 if (tnc2[i] == ':')
                 {
                    bytes.append(AX25Packet::encodeAddress(tmp, 1));
                    state = 3;
                 }
                 else if (tnc2[i] == ',')
                 {
                    bytes.append(AX25Packet::encodeAddress(tmp));
                    tmp = "";
                 }
                 else
                    tmp.append(tnc2[i]);
            }
            else if (state == 3)
            {
                // UI Type and PID
                bytes.append(3);
                bytes.append(-16); // 0xf0
                // APRS message
                bytes.append(tnc2.mid(i).toLatin1().trimmed());
                // CRC
                bytes.append((char)0);
                bytes.append((char)0);
                break;
            }
        }

        return bytes;
    }

    QString toText(bool includeFrom=true, bool includePosition=false, char separator='\n',
                        bool altitudeMetres=false, int speedUnits=0, bool tempCelsius=false, bool rainfallMM=false)
    {
        QStringList text;

        if (!m_objectName.isEmpty())
        {
            text.append(QString("%1 (%2)").arg(m_objectName).arg(m_from));
        }
        else
        {
            if (includeFrom)
                text.append(m_from);
        }

        if (m_hasTimestamp)
        {
            QStringList time;
            time.append(this->date());
            time.append(this->time());
            if (m_utc)
                time.append("UTC");
            else
                time.append("local");
            text.append(time.join(' '));
        }

        if (includePosition && m_hasPosition)
            text.append(QString("Latitude: %1 Longitude: %2").arg(m_latitude).arg(m_longitude));
        if (m_hasAltitude)
        {
            if (altitudeMetres)
                text.append(QString("Altitude: %1 m").arg((int)std::round(Units::feetToMetres(m_altitudeFt))));
            else
                text.append(QString("Altitude: %1 ft").arg(m_altitudeFt));
        }
        if (m_hasCourseAndSpeed)
        {
            if (speedUnits == 0)
                text.append(QString("Course: %1%3 Speed: %2 knts").arg(m_course).arg(m_speed).arg(QChar(0xb0)));
            else if (speedUnits == 1)
                text.append(QString("Course: %1%3 Speed: %2 mph").arg(m_course).arg(Units::knotsToIntegerMPH(m_speed)).arg(QChar(0xb0)));
            else
                text.append(QString("Course: %1%3 Speed: %2 kph").arg(m_course).arg(Units::knotsToIntegerKPH(m_speed)).arg(QChar(0xb0)));
        }

        if (m_hasStationDetails)
            text.append(QString("TX Power: %1 Watts Antenna Height: %2 m Gain: %3 dB Direction: %4").arg(m_powerWatts).arg(std::round(Units::feetToMetres(m_antennaHeightFt))).arg(m_antennaGainDB).arg(m_antennaDirectivity));
        if (m_hasRadioRange)
            text.append(QString("Range: %1 km").arg(Units::milesToKilometres(m_radioRangeMiles)));
        if (m_hasDf)
            text.append(QString("DF Strength: S %1 Height: %2 m Gain: %3 dB Direction: %4").arg(m_dfStrength).arg(std::round(Units::feetToMetres(m_dfHeightFt))).arg(m_dfGainDB).arg(m_dfAntennaDirectivity));

        if (m_hasWeather)
        {
            QStringList weather, wind, air, rain;

            wind.append(QString("Wind"));
            if (m_hasWindDirection)
                wind.append(QString("%1%2").arg(m_windDirection).arg(QChar(0xb0)));
            if (m_hasWindSpeed)
                wind.append(QString("%1 mph").arg(m_windSpeed));
            if (m_hasGust)
                wind.append(QString("Gusts %1 mph").arg(m_gust));
            weather.append(wind.join(' '));

            if (m_hasTemp || m_hasHumidity || m_hasBarometricPressure)
            {
                air.append("Air");
                if (m_hasTemp)
                {
                    if (tempCelsius)
                        air.append(QString("Temperature %1C").arg(Units::fahrenheitToCelsius(m_temp), 0, 'f', 1));
                    else
                        air.append(QString("Temperature %1F").arg(m_temp));
                }
                if (m_hasHumidity)
                    air.append(QString("Humidity %1%").arg(m_humidity));
                if (m_hasBarometricPressure)
                    air.append(QString("Pressure %1 mbar").arg(m_barometricPressure/10.0));
                weather.append(air.join(' '));
            }

            if (m_hasRainLastHr || m_hasRainLast24Hrs || m_hasRainSinceMidnight)
            {
                rain.append("Rain");
                if (m_hasRainLastHr)
                {
                    if (rainfallMM)
                        rain.append(QString("%1 mm last hour").arg(std::round(Units::inchesToMilimetres(m_rainLastHr/100.0))));
                    else
                        rain.append(QString("%1 1/100\" last hour").arg(m_rainLastHr));
                }
                if (m_hasRainLast24Hrs)
                {
                    if (rainfallMM)
                        rain.append(QString("%1 mm last 24 hours").arg(std::round(Units::inchesToMilimetres(m_rainLast24Hrs/100.0))));
                    else
                        rain.append(QString("%1 1/100\" last 24 hours").arg(m_rainLast24Hrs));
                }
                if (m_hasRainSinceMidnight)
                {
                    if (rainfallMM)
                        rain.append(QString("%1 mm since midnight").arg(std::round(Units::inchesToMilimetres(m_rainSinceMidnight/100.0))));
                    else
                        rain.append(QString("%1 1/100\" since midnight").arg(m_rainSinceMidnight));
                }
                weather.append(rain.join(' '));
            }

            if (!m_weatherUnitType.isEmpty())
                weather.append(m_weatherUnitType);

            text.append(weather.join(separator));
        }

        if (m_hasStormData)
        {
            QStringList storm;

            storm.append(m_stormType);

            storm.append(QString("Direction: %1%3 Speed: %2").arg(m_stormDirection).arg(m_stormSpeed).arg(QChar(0xb0)));
            storm.append(QString("Sustained wind speed: %1 knots Peak wind gusts: %2 knots Central pressure: %3 mbar").arg(m_stormSustainedWindSpeed).arg(m_stormPeakWindGusts).arg(m_stormCentralPresure));
            storm.append(QString("Hurrican winds radius: %1 nm Tropical storm winds radius: %2 nm%3").arg(m_stormRadiusHurricanWinds).arg(m_stormRadiusTropicalStormWinds).arg(m_hasStormRadiusWholeGail ? QString("") : QString(" Whole gail radius: %3 nm").arg(m_stormRadiusWholeGail)));

            text.append(storm.join(separator));
        }

        if (!m_comment.isEmpty())
            text.append(m_comment);

        return text.join(separator);
    }

private:
    int charToInt(QString &s, int idx);
    bool parseTime(QString& info, int& idx);
    bool parseTimeMDHM(QString& info, int& idx);
    bool isLatLongChar(const QChar c);
    bool parsePosition(QString& info, int& idx);
    bool parseDataExension(QString& info, int& idx);
    bool parseComment(QString& info, int& idx);
    bool parseInt(QString& info, int& idx, int chars, int& value, bool& hasValue);
    bool parseWeather(QString& info, int& idx, bool positionLess);
    bool parseStorm(QString& info, int& idx);
    bool parseObject(QString& info, int& idx);
    bool parseItem(QString& info, int& idx);
    bool parseStatus(QString& info, int& idx);
    bool parseMessage(QString& info, int& idx);
    bool parseTelemetry(QString& info, int& idx);
    bool parseMicE(QString& info, int& idx, QString& dest);
};

#endif // INCLUDE_APRS_H
