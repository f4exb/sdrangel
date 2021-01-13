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

#include <QRegExp>
#include <QStringList>
#include <QDateTime>

#include "aprs.h"

// See: http://www.aprs.org/doc/APRS101.PDF

// Currently we only decode what we want to display on the map
bool APRSPacket::decode(AX25Packet packet)
{
    // Check type, PID and length of packet
    if ((packet.m_type == "UI") && (packet.m_pid == "f0") && (packet.m_dataASCII.length() >= 1))
    {
        // Check destination address
        QRegExp re("^(AIR.*|ALL.*|AP.*|BEACON|CQ.*|GPS.*|DF.*|DGPS.*|DRILL.*|DX.*|ID.*|JAVA.*|MAIL.*|MICE.*|QST.*|QTH.*|RTCM.*|SKY.*|SPACE.*|SPC.*|SYM.*|TEL.*|TEST.*|TLM.*|WX.*|ZIP.*)");
        if (re.exactMatch(packet.m_to))
        {
            m_from = packet.m_from;
            m_to = packet.m_to;
            m_via = packet.m_via;
            m_data = packet.m_dataASCII;

            if (packet.m_to.startsWith("GPS") || packet.m_to.startsWith("SPC") || packet.m_to.startsWith("SYM"))
            {
                // FIXME: Trailing letters xyz specify a symbol
            }

            // Source address SSID can be used to specify a symbol

            // First byte of information field is data type ID
            char dataType = packet.m_dataASCII[0].toLatin1();
            int idx = 1;
            switch (dataType)
            {
            case '!': // Position without timestamp or  Ultimeter 2000 WX Station
                parsePosition(packet.m_dataASCII, idx);
                if (m_symbolCode == '_')
                    parseWeather(packet.m_dataASCII, idx, false);
                else if (m_symbolCode == '@')
                    parseStorm(packet.m_dataASCII, idx);
                else
                {
                    parseDataExension(packet.m_dataASCII, idx);
                    parseComment(packet.m_dataASCII, idx);
                }
                break;
            case '#': // Peet Bros U-II Weather Station
            case '$': // Raw GPS data or Ultimeter 2000
            case '%': // Agrelo DFJr / MicroFinder
                break;
            case ')': // Item
                parseItem(packet.m_dataASCII, idx);
                parsePosition(packet.m_dataASCII, idx);
                parseDataExension(packet.m_dataASCII, idx);
                parseComment(packet.m_dataASCII, idx);
                break;
            case '*': // Peet Bros U-II Weather Station
                break;
            case '/': // Position with timestamp (no APRS messaging)
                parseTime(packet.m_dataASCII, idx);
                parsePosition(packet.m_dataASCII, idx);
                if (m_symbolCode == '_')
                    parseWeather(packet.m_dataASCII, idx, false);
                else if (m_symbolCode == '@')
                    parseStorm(packet.m_dataASCII, idx);
                else
                {
                    parseDataExension(packet.m_dataASCII, idx);
                    parseComment(packet.m_dataASCII, idx);
                }
                break;
            case ':': // Message
                parseMessage(packet.m_dataASCII, idx);
                break;
            case ';': // Object
                parseObject(packet.m_dataASCII, idx);
                parseTime(packet.m_dataASCII, idx);
                parsePosition(packet.m_dataASCII, idx);
                if (m_symbolCode == '_')
                    parseWeather(packet.m_dataASCII, idx, false);
                else if (m_symbolCode == '@')
                    parseStorm(packet.m_dataASCII, idx);
                else
                {
                    parseDataExension(packet.m_dataASCII, idx);
                    parseComment(packet.m_dataASCII, idx);
                }
                break;
            case '<': // Station Capabilities
                break;
            case '=': // Position without timestamp (with APRS messaging)
                parsePosition(packet.m_dataASCII, idx);
                if (m_symbolCode == '_')
                    parseWeather(packet.m_dataASCII, idx, false);
                else if (m_symbolCode == '@')
                    parseStorm(packet.m_dataASCII, idx);
                else
                {
                    parseDataExension(packet.m_dataASCII, idx);
                    parseComment(packet.m_dataASCII, idx);
                }
                break;
            case '>': // Status
                parseStatus(packet.m_dataASCII, idx);
                break;
            case '?': // Query
                break;
            case '@': // Position with timestamp (with APRS messaging)
                parseTime(packet.m_dataASCII, idx);
                parsePosition(packet.m_dataASCII, idx);
                if (m_symbolCode == '_')
                    parseWeather(packet.m_dataASCII, idx, false);
                else if (m_symbolCode == '@')
                    parseStorm(packet.m_dataASCII, idx);
                else
                {
                    parseDataExension(packet.m_dataASCII, idx);
                    parseComment(packet.m_dataASCII, idx);
                }
                break;
            case 'T': // Telemetry data
                parseTelemetry(packet.m_dataASCII, idx);
                break;
            case '_': // Weather report (without position)
                parseTimeMDHM(packet.m_dataASCII, idx);
                parseWeather(packet.m_dataASCII, idx, true);
                break;
            case '{': // User-defined APRS packet format
                break;
            default:
                return false;
            }

            if (m_hasSymbol)
            {
                int num = m_symbolCode - '!';
                m_symbolImage = QString("aprs/aprs/aprs-symbols-24-%1-%2.png").arg(m_symbolTable == '/' ? 0 : 1).arg(num, 2, 10, QChar('0'));
            }

            return true;
        }
    }

    return false;
}

int APRSPacket::charToInt(QString&s, int idx)
{
    char c = s[idx].toLatin1();
    return c == ' ' ? 0 : c - '0';
}

bool APRSPacket::parseTime(QString& info, int& idx)
{
    if (info.length() < idx+7)
        return false;

    QDateTime currentDateTime;

    if (info[idx+6]=='h')
    {
        // HMS format
        if (info[idx].isDigit()
            && info[idx+1].isDigit()
            && info[idx+2].isDigit()
            && info[idx+3].isDigit()
            && info[idx+4].isDigit()
            && info[idx+5].isDigit())
        {
            int hour = charToInt(info, idx) * 10 + charToInt(info, idx+1);
            int min = charToInt(info, idx+2) * 10 + charToInt(info, idx+3);
            int sec = charToInt(info, idx+4) * 10 + charToInt(info, idx+5);

            if (hour > 23)
                return false;
            if (min > 59)
                return false;
            if (sec > 60) // Can have 60 seconds when there's a leap second
                return false;

            m_utc = true;
            m_timestamp = QDateTime(QDate::currentDate(), QTime(hour, min, sec));
            m_hasTimestamp = true;

            idx += 7;
            return true;
        }
        else
            return false;
    }
    else if ((info[idx+6]=='z') || (info[idx+6]=='/'))
    {
        // DHM format
        if (info[idx].isDigit()
            && info[idx+1].isDigit()
            && info[idx+2].isDigit()
            && info[idx+3].isDigit()
            && info[idx+4].isDigit()
            && info[idx+5].isDigit())
        {
            int day = charToInt(info, idx) * 10 + charToInt(info, idx+1);
            int hour = charToInt(info, idx+2) * 10 + charToInt(info, idx+3);
            int min = charToInt(info, idx+4) * 10 + charToInt(info, idx+5);

            if (day > 31)
                return false;
            if (hour > 23)
                return false;
            if (min > 59)
                return false;

            m_utc = info[idx+6]=='z';
            currentDateTime = m_utc ? QDateTime::currentDateTimeUtc() : QDateTime::currentDateTime();
            m_timestamp = QDateTime(QDate(currentDateTime.date().year(), currentDateTime.date().month(), day), QTime(hour, min, 0));
            m_hasTimestamp = true;

            idx += 7;
            return true;
        }
        else
            return false;
    }
    else
        return false;
}

// Time format used in weather reports without position
bool APRSPacket::parseTimeMDHM(QString& info, int& idx)
{
    if (info.length() < idx+8)
        return false;

    if (info[idx].isDigit()
        && info[idx+1].isDigit()
        && info[idx+2].isDigit()
        && info[idx+3].isDigit()
        && info[idx+4].isDigit()
        && info[idx+5].isDigit()
        && info[idx+6].isDigit()
        && info[idx+7].isDigit())
    {
        int month = charToInt(info, idx) * 10 + charToInt(info, idx+1);
        int day = charToInt(info, idx+2) * 10 + charToInt(info, idx+3);
        int hour = charToInt(info, idx+4) * 10 + charToInt(info, idx+5);
        int min = charToInt(info, idx+6) * 10 + charToInt(info, idx+7);

        if (month > 12)
            return false;
        if (day > 31)
            return false;
        if (hour > 23)
            return false;
        if (min > 59)
            return false;

        m_utc = true;
        QDateTime currentDateTime = QDateTime::currentDateTimeUtc();
        m_timestamp = QDateTime(QDate(currentDateTime.date().year(), month, day), QTime(hour, min, 0));
        m_hasTimestamp = true;

        return true;
    }
    else
        return false;
}

// Position ambigutiy can be specified by using spaces instead of digits in lats and longs
bool APRSPacket::isLatLongChar(QCharRef c)
{
    return (c.isDigit() || c == ' ');
}

bool APRSPacket::parsePosition(QString& info, int& idx)
{
    float latitude;
    float longitude;
    char table;
    char code;

    if (info.length() < idx+8+1+9+1)
        return false;

    // Latitude
    if (info[idx].isDigit()
        && info[idx+1].isDigit()
        && isLatLongChar(info[idx+2])
        && isLatLongChar(info[idx+3])
        && (info[idx+4]=='.')
        && isLatLongChar(info[idx+5])
        && isLatLongChar(info[idx+6])
        && ((info[idx+7]=='N') || (info[idx+7]=='S')))
    {
        int deg = charToInt(info, idx) * 10 + charToInt(info, idx+1);
        int min = charToInt(info, idx+2) * 10 + charToInt(info, idx+3);
        int hundreths = charToInt(info, idx+5) * 10 + charToInt(info, idx+6);
        bool north = (info[idx+7]=='N');
        if (deg > 90)
            return false;
        else if ((deg == 90) && ((min != 0) || (hundreths != 0)))
            return false;
        latitude = ((float)deg) + min/60.0 + hundreths/60.0/100.0;
        if (!north)
            latitude = -latitude;
        idx += 8;
    }
    else
        return false;

    // Symbol table identifier
    table = info[idx++].toLatin1();

    // Longitude
    if (info[idx].isDigit()
        && info[idx+1].isDigit()
        && info[idx+2].isDigit()
        && isLatLongChar(info[idx+3])
        && isLatLongChar(info[idx+4])
        && (info[idx+5]=='.')
        && isLatLongChar(info[idx+6])
        && isLatLongChar(info[idx+7])
        && ((info[idx+8]=='E') || (info[idx+8]=='W')))
    {
        int deg = charToInt(info, idx) * 100 + charToInt(info, idx+1) * 10 + charToInt(info, idx+2);
        int min = charToInt(info, idx+3) * 10 + charToInt(info, idx+4);
        int hundreths = charToInt(info, idx+6) * 10 + charToInt(info, idx+7);
        bool east = (info[idx+8]=='E');
        if (deg > 180)
            return false;
        else if ((deg == 180) && ((min != 0) || (hundreths != 0)))
            return false;
        longitude = ((float)deg) + min/60.0 + hundreths/60.0/100.0;
        if (!east)
            longitude = -longitude;
        idx += 9;
    }
    else
        return false;

    // Symbol table code
    code = info[idx++].toLatin1();

    // Update state as we have a valid position
    m_latitude = latitude;
    m_longitude = longitude;
    m_hasPosition = true;
    m_symbolTable = table;
    m_symbolCode = code;
    m_hasSymbol = true;
    return true;
}

bool APRSPacket::parseDataExension(QString& info, int& idx)
{
    int heightMap[] = {10, 20, 40, 80, 160, 320, 640, 1280, 2560, 5120};
    QStringList directivityMap = {"Omni", "NE", "E", "SE", "S", "SW", "W", "NW", "N", ""};

    int remainingLength = info.length() - idx;
    if (remainingLength < 7)
        return true;
    QString s = info.right(remainingLength);

    // Course and speed
    QRegExp courseSpeed("^([0-9]{3})\\/([0-9]{3})");
    if (courseSpeed.indexIn(s) >= 0)
    {
        m_course = courseSpeed.capturedTexts()[1].toInt();
        m_speed = courseSpeed.capturedTexts()[2].toInt();
        m_hasCourseAndSpeed = true;
        idx += 7;
        return true;
    }

    // Station radio details
    QRegExp phg("^PHG([0-9])([0-9])([0-9])([0-9])");
    if (phg.indexIn(s) >= 0)
    {
        // Transmitter power
        int powerCode = phg.capturedTexts()[1].toInt();
        int powerMap[] = {0, 1, 4, 9, 16, 25, 36, 49, 64, 81};
        m_powerWatts = powerMap[powerCode];

        // Antenna height
        int heightCode = phg.capturedTexts()[2].toInt();
        m_antennaHeightFt = heightMap[heightCode];

        // Antenna gain
        m_antennaGainDB = phg.capturedTexts()[3].toInt();

        // Antenna directivity
        int directivityCode = phg.capturedTexts()[4].toInt();
        m_antennaDirectivity = directivityMap[directivityCode];

        m_hasStationDetails = true;

        idx += 7;
        return true;
    }

    // Radio range
    QRegExp rng("^RNG([0-9]{4})");
    if (rng.indexIn(s) >= 0)
    {
        m_radioRangeMiles = rng.capturedTexts()[1].toInt();
        m_hasRadioRange = true;
        idx += 7;
        return true;
    }

    // Omni-DF strength
    QRegExp dfs("^DFS([0-9])([0-9])([0-9])([0-9])");
    if (dfs.indexIn(s) >= 0)
    {
        // Strength S-points
        m_dfStrength = dfs.capturedTexts()[1].toInt();

        // Antenna height
        int heightCode = dfs.capturedTexts()[2].toInt();
        m_dfHeightFt = heightMap[heightCode];

        // Antenna gain
        m_dfGainDB = dfs.capturedTexts()[3].toInt();

        // Antenna directivity
        int directivityCode = dfs.capturedTexts()[4].toInt();
        m_dfAntennaDirectivity = directivityMap[directivityCode];

        m_hasDf = true;
        idx += 7;
        return true;
    }

    return true;
}

bool APRSPacket::parseComment(QString& info, int& idx)
{
    int commentLength = info.length() - idx;
    if (commentLength > 0)
    {
        m_comment = info.right(commentLength);

        // Comment can contain altitude anywhere in it. Of the form /A=001234 in feet
        QRegExp re("\\/A=([0-9]{6})");
        int pos = re.indexIn(m_comment);
        if (pos >= 0)
        {
            m_altitudeFt = re.capturedTexts()[1].toInt();
            m_hasAltitude = true;
            // Strip it out of comment if at start of string
            if (pos == 0)
                m_comment = m_comment.mid(9);
        }
    }

    return true;
}

bool APRSPacket::parseInt(QString& info, int& idx, int chars, int& value, bool& hasValue)
{
    int total = 0;
    bool negative = false;
    bool noValue = false;

    for (int i = 0; i < chars; i++)
    {
        if (info[idx].isDigit())
        {
            total = total * 10;
            total += info[idx].toLatin1() - '0';
        }
        else if ((i == 0) && (info[idx] == '-'))
            negative = true;
        else if ((info[idx] == '.') || (info[idx] == ' '))
            noValue = true;
        else
            return false;
        idx++;
    }
    if (!noValue)
    {
        if (negative)
            value = -total;
        else
            value = total;
        hasValue = true;
    }
    else
        hasValue = false;
    return true;
}

bool APRSPacket::parseWeather(QString& info, int& idx, bool positionLess)
{
    if (!positionLess)
    {
        if (!parseInt(info, idx, 3, m_windDirection, m_hasWindDirection))
            return false;
        if (info[idx++] != '/')
            return false;
        if (!parseInt(info, idx, 3, m_windSpeed, m_hasWindSpeed))
            return false;
    }

    // Weather data
    bool done = false;
    while (!done && (idx < info.length()))
    {
        switch (info[idx++].toLatin1())
        {
        case 'c': // Wind direction
            if (!parseInt(info, idx, 3, m_windDirection, m_hasWindDirection))
                return false;
            break;
        case 's': // Wind speed
            if (!parseInt(info, idx, 3, m_windSpeed, m_hasWindSpeed))
                return false;
            break;
        case 'g': // Gust
            if (!parseInt(info, idx, 3, m_gust, m_hasGust))
                return false;
            break;
        case 't': // Temp
            if (!parseInt(info, idx, 3, m_temp, m_hasTemp))
            {
            qDebug() << "Failed parseing temp: idx" << idx;
                return false;
        }
            break;
        case 'r': // Rain last hour
            if (!parseInt(info, idx, 3, m_rainLastHr, m_hasRainLastHr))
                return false;
            break;
        case 'p': // Rain last 24 hours
            if (!parseInt(info, idx, 3, m_rainLast24Hrs, m_hasRainLast24Hrs))
                return false;
            break;
        case 'P': // Rain since midnight
            if (!parseInt(info, idx, 3, m_rainSinceMidnight, m_hasRainSinceMidnight))
                return false;
            break;
        case 'h': // Humidity
            if (!parseInt(info, idx, 2, m_humidity, m_hasHumidity))
                return false;
            break;
        case 'b': // Barometric pressure
            if (!parseInt(info, idx, 5, m_barometricPressure, m_hasBarometricPressure))
                return false;
            break;
        case 'L': // Luminosity <999
            if (!parseInt(info, idx, 3, m_luminosity, m_hasLuminsoity))
                return false;
            break;
        case 'l': // Luminosity >= 1000
            if (!parseInt(info, idx, 3, m_luminosity, m_hasLuminsoity))
                return false;
            m_luminosity += 1000;
            break;
        case 'S': // Snowfall
            if (!parseInt(info, idx, 3, m_snowfallLast24Hrs, m_hasSnowfallLast24Hrs))
                return false;
            break;
        case '#': // Raw rain counter
            if (!parseInt(info, idx, 3, m_rawRainCounter, m_hasRawRainCounter))
                return false;
            break;
        case 'X': // Radiation level
            if (!parseInt(info, idx, 3, m_radiationLevel, m_hasRadiationLevel))
                return false;
            break;
        case 'F': // Floor water level
            if (!parseInt(info, idx, 4, m_floodLevel, m_hasFloodLevel))
                return false;
            break;
        case 'V': // Battery volts
            if (!parseInt(info, idx, 3, m_batteryVolts, m_hasBatteryVolts))
                return false;
            break;
        default:
            done = true;
            break;
        }
    }
    if (done)
    {
        // APRS 1.1 spec says remaining fields are s/w and weather unit type
        // But few real-world packets actually seem to conform to the spec
        idx--;
        int remaining = info.length() - idx;
        m_weatherUnitType = info.right(remaining);
        idx += remaining;
    }

    m_hasWeather = true;
    return true;
}


bool APRSPacket::parseStorm(QString& info, int& idx)
{
    bool unused;

    if (!parseInt(info, idx, 3, m_stormDirection, unused))
        return false;
    if (info[idx++] != '/')
        return false;
    if (!parseInt(info, idx, 3, m_stormSpeed, unused))
        return false;
    if (info[idx++] != '/')
        return false;
    QString type = info.mid(idx, 2);
    idx += 2;
    if (type == "TS")
        m_stormType = "Tropical storm";
    else if (type == "HC")
        m_stormType = "Hurrican";
    else if (type == "TD")
        m_stormType = "Tropical depression";
    else
        m_stormType = type;

    if (info[idx++] != '/') // Sustained wind speed
        return false;
    if (!parseInt(info, idx, 3, m_stormSustainedWindSpeed, unused))
        return false;
    if (info[idx++] != '^') // Peak wind gusts
        return false;
    if (!parseInt(info, idx, 3, m_stormPeakWindGusts, unused))
        return false;
    if (info[idx++] != '/') // Central pressure
        return false;
    if (!parseInt(info, idx, 4, m_stormCentralPresure, unused))
        return false;
    if (info[idx++] != '>') // Radius hurrican winds
        return false;
    if (!parseInt(info, idx, 3, m_stormRadiusHurricanWinds, unused))
        return false;
    if (info[idx++] != '&') // Radius tropical storm winds
        return false;
    if (!parseInt(info, idx, 3, m_stormRadiusTropicalStormWinds, unused))
        return false;
    m_hasStormData = true;
    // Optional field
    if (info.length() >= idx + 4)
    {
        if (info[idx] != '%') // Radius whole gail
            return true;
        idx++;
        if (!parseInt(info, idx, 3, m_stormRadiusWholeGail, m_hasStormRadiusWholeGail))
            return false;
    }
    return true;
}

bool APRSPacket::parseObject(QString& info, int& idx)
{
    if (info.length() < idx+10)
        return false;

    // Object names are 9 chars
    m_objectName = info.mid(idx, 9).trimmed();
    idx += 9;

    if (info[idx] == '*')
        m_objectLive = true;
    else if (info[idx] == '_')
        m_objectKilled = true;
    else
        return false;
    idx++;

    return true;
}

bool APRSPacket::parseItem(QString& info, int& idx)
{
    if (info.length() < idx+3)
        return false;

    // Item names are 3-9 chars long, excluding ! or _
    m_objectName = "";
    int i;
    for (i = 0; i < 10; i++)
    {
        if (info.length() >= idx)
        {
            QChar c = info[idx];
            if (c == '!' || c == '_')
                break;
            else
            {
                m_objectName.append(c);
                idx++;
            }
        }
    }
    if (i == 11)
        return false;
    if (info[idx] == '!')
        m_objectLive = true;
    else if (info[idx] == '_')
        m_objectKilled = true;
    idx++;

    return true;
}

bool APRSPacket::parseStatus(QString& info, int& idx)
{
    QString remaining = info.mid(idx);

    QRegExp timestampRE("^([0-9]{6})z");                         // DHM timestamp
    QRegExp maidenheadRE("^([A-Z]{2}[0-9]{2}[A-Z]{0,2})[/\\\\]."); // Maidenhead grid locator and symbol

    if (timestampRE.indexIn(remaining) >= 0)
    {
        parseTime(info, idx);
        m_status = info.mid(idx);
        idx += m_status.length();
    }
    else if (maidenheadRE.indexIn(remaining) >= 0)
    {
        m_maidenhead = maidenheadRE.capturedTexts()[1];
        idx += m_maidenhead.length();
        m_symbolTable = info[idx++].toLatin1();
        m_symbolCode = info[idx++].toLatin1();
        m_hasSymbol = true;
        if (info[idx] == ' ')
        {
            idx++;
            m_status = info.mid(idx);
            idx += m_status.length();
        }
    }
    else
    {
        m_status = remaining;
        idx += m_status.length();
    }
    m_hasStatus = true;

    // Check for beam heading and power in meteor scatter status reports
    int len = m_status.length();
    if (len >= 3)
    {
        if (m_status[len-3] == '^')
        {
            bool error = false;
            char h = m_status[len-2].toLatin1();
            char p = m_status[len-1].toLatin1();

            if (isdigit(h))
                m_beamHeading = (h - '0') * 10;
            else if (isupper(h))
                m_beamHeading = (h - 'A') * 10 + 100;
            else
                error = true;

            switch (p)
            {
            case '1': m_beamPower = 10; break;
            case '2': m_beamPower = 40; break;
            case '3': m_beamPower = 90; break;
            case '4': m_beamPower = 160; break;
            case '5': m_beamPower = 250; break;
            case '6': m_beamPower = 360; break;
            case '7': m_beamPower = 490; break;
            case '8': m_beamPower = 640; break;
            case '9': m_beamPower = 810; break;
            case ':': m_beamPower = 1000; break;
            case ';': m_beamPower = 1210; break;
            case '<': m_beamPower = 1440; break;
            case '=': m_beamPower = 1690; break;
            case '>': m_beamPower = 1960; break;
            case '?': m_beamPower = 2250; break;
            case '@': m_beamPower = 2560; break;
            case 'A': m_beamPower = 2890; break;
            case 'B': m_beamPower = 3240; break;
            case 'C': m_beamPower = 3610; break;
            case 'D': m_beamPower = 4000; break;
            case 'E': m_beamPower = 4410; break;
            case 'F': m_beamPower = 4840; break;
            case 'G': m_beamPower = 5290; break;
            case 'H': m_beamPower = 5760; break;
            case 'I': m_beamPower = 6250; break;
            case 'J': m_beamPower = 6760; break;
            case 'K': m_beamPower = 7290; break;
            default:  error = true; break;
            }
            if (!error)
            {
                m_hasBeam = true;
                m_status = m_status.left(len - 3);
            }
        }
    }
    return true;
}

bool APRSPacket::parseMessage(QString& info, int& idx)
{
    if (info.length() < idx+10)
        return false;

    // Addressee is fixed width
    if (info[idx+9] != ':')
        return false;
    m_addressee = info.mid(idx, 9).trimmed();
    idx += 10;

    // Message
    m_message = info.mid(idx);
    idx += m_message.length();

    // Check if telemetry parameter/unit names
    if (m_message.startsWith("PARM."))
    {
        bool done = false;
        QString s("");
        int i = 5;
        while (!done)
        {
            if (i >= m_message.length())
            {
                if (!s.isEmpty())
                   m_telemetryNames.append(s);
                done = true;
            }
            else if (m_message[i] == ',')
            {
                if (!s.isEmpty())
                   m_telemetryNames.append(s);
                i++;
                s = "";
            }
            else
                s.append(m_message[i++]);
        }
    }
    else if (m_message.startsWith("UNIT."))
    {
        bool done = false;
        QString s("");
        int i = 5;
        while (!done)
        {
            if (i >= m_message.length())
            {
                if (!s.isEmpty())
                   m_telemetryLabels.append(s);
                done = true;
            }
            else if (m_message[i] == ',')
            {
                if (!s.isEmpty())
                   m_telemetryLabels.append(s);
                i++;
                s = "";
            }
            else
                s.append(m_message[i++]);
        }
    }
    else if (m_message.startsWith("EQNS."))
    {
        bool done = false;
        QString s("");
        int i = 5;
        QList <QString> telemetryCoefficients;
        while (!done)
        {
            if (i >= m_message.length())
            {
                if (!s.isEmpty())
                   telemetryCoefficients.append(s);
                done = true;
            }
            else if (m_message[i] == ',')
            {
                if (!s.isEmpty())
                   telemetryCoefficients.append(s);
                i++;
                s = "";
            }
            else
                s.append(m_message[i++]);
        }
        m_hasTelemetryCoefficients = 0;
        for (int j = 0; j < telemetryCoefficients.length() / 3; j++)
        {
            m_telemetryCoefficientsA[j] = telemetryCoefficients[j*3].toDouble();
            m_telemetryCoefficientsB[j] = telemetryCoefficients[j*3+1].toDouble();
            m_telemetryCoefficientsC[j] = telemetryCoefficients[j*3+2].toDouble();
            m_hasTelemetryCoefficients++;
        }
    }
    else if (m_message.startsWith("BITS."))
    {
        QString s("");
        int i = 5;
        for (int j = 0; j < 8; j++)
        {
            if (i >= m_message.length())
                m_telemetryBitSense[j] = m_message[i] == '1';
            else
                m_telemetryBitSense[j] = true;
            i++;
        }
        m_hasTelemetryBitSense = true;
        m_telemetryProjectName = m_message.mid(i);
        i += m_telemetryProjectName.length();
    }
    else
    {
        // Check for message number
        QRegExp noRE("\\{([0-9]{1,5})$");
        if (noRE.indexIn(m_message) >= 0)
        {
            m_messageNo = noRE.capturedTexts()[1];
            m_message = m_message.left(m_message.length() - m_messageNo.length() - 1);
        }
    }
    m_hasMessage = true;

    return true;
}

bool APRSPacket::parseTelemetry(QString& info, int& idx)
{
    if (info[idx] == '#')
    {
        // Telemetry report
        idx++;
        if ((info[idx] == 'M') && (info[idx+1] == 'I') && (info[idx+2] == 'C'))
            idx += 3;
        else if (isdigit(info[idx].toLatin1()) && isdigit(info[idx+1].toLatin1()) && isdigit(info[idx+2].toLatin1()))
        {
            m_seqNo = info.mid(idx, 3).toInt();
            m_hasSeqNo = true;
            idx += 3;
        }
        else
            return false;

        if (info[idx] == ',')
            idx++;
        parseInt(info, idx, 3, m_a1, m_a1HasValue);
        if (info[idx++] != ',')
            return false;
        parseInt(info, idx, 3, m_a2, m_a2HasValue);
        if (info[idx++] != ',')
            return false;
        parseInt(info, idx, 3, m_a3, m_a3HasValue);
        if (info[idx++] != ',')
            return false;
        parseInt(info, idx, 3, m_a4, m_a4HasValue);
        if (info[idx++] != ',')
            return false;
        parseInt(info, idx, 3, m_a5, m_a5HasValue);
        if (info[idx++] != ',')
            return false;
        for (int i = 0; i < 8; i++)
            m_b[i] = info[idx++] == '1';
        m_bHasValue = true;

        m_telemetryComment = info.mid(idx);
        idx += m_telemetryComment.length();
        m_hasTelemetry = true;
        return true;
    }
    else
        return false;
}
