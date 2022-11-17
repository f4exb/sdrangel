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
#include <QRegularExpression>
#include <QStringList>
#include <QDateTime>

#include "aprs.h"
#include "util/units.h"

inline bool inRange(unsigned low, unsigned high, unsigned x)
{
    return (low <= x && x <= high);
}

// Required for Mic-E Decoding
inline int charToIntAscii(QString&s, int idx)
{
    char c = s.toLatin1().at(idx);
    return int(c);
}


// See: http://www.aprs.org/doc/APRS101.PDF

// Currently we only decode what we want to display on the map
bool APRSPacket::decode(AX25Packet packet)
{
    // Check type, PID and length of packet
    if ((packet.m_type == "UI") && (packet.m_pid == "f0") && (packet.m_dataASCII.length() >= 1))
    {
        // Check destination address
        QRegularExpression re("^(AIR.*|ALL.*|AP.*|BEACON|CQ.*|GPS.*|DF.*|DGPS.*|DRILL.*|DX.*|ID.*|JAVA.*|MAIL.*|MICE.*|QST.*|QTH.*|RTCM.*|SKY.*|SPACE.*|SPC.*|SYM.*|TEL.*|TEST.*|TLM.*|WX.*|ZIP.*)");
        QRegularExpression re_mice("^[A-LP-Z0-9]{3}[L-Z0-9]{3}.?$"); // Mic-E Encoded Destination, 6-7 bytes
        if (re.match(packet.m_to).hasMatch() || re_mice.match(packet.m_to).hasMatch())
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
            case '`': // Mic-E Information Field Data (current)
            case '\'': // Mic-E Information Field Data (old)
                parseMicE(packet.m_dataASCII, idx, m_to);
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
        } else {
            qDebug() << "APRSPacket::decode: AX.25 Destination did not match known regexp " << m_to;
        }
    } else {
        qDebug() << "APRSPacket::decode: Invalid value in type=" << packet.m_type << " pid=" << packet.m_pid << " length of " << packet.m_dataASCII;
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
bool APRSPacket::isLatLongChar(const QChar c)
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

// Mic-E Implementation by Peter Beckman KM4BBB github:ooglek
bool APRSPacket::parseMicE(QString& info, int& idx, QString& dest)
{
    info = info.toLatin1();
    // Mic-E Location data is encoded in the AX.25 Destination Address
    if (dest.length() < 6) {
        qDebug() << "APRSPacket::parseMicE: Destination invalid length " << dest;
        return false;
    }

    // Mic-E Information data is 8 bytes minimum, 13-14 with altitude
    if (info.length() < idx+8) {
        qDebug() << "APRSPacket::parseMicE: Information Data invalid length " << info;
        return false;
    }

    QString latDigits = "";
    QString messageBits = "";
    int messageType = 0; // 0 = Standard, 1 = Custom
    int longitudeOffset = 0;
    // Assume South & East, as North & West are encoded using consecutive Characters, easier and shorter to code
    float latitudeDirection = -1;  // South
    float longitudeDirection = 1;  // East

    QHash<QString, QString> messageTypeLookup = {
        {"111", "Off Duty"},
        {"110", "En Route"},
        {"101", "In Service"},
        {"100", "Returning"},
        {"011", "Committed"},
        {"010", "Special"},
        {"001", "Priority"},
        {"000", "Emergency"}
    };

    QRegularExpression re("^[A-LP-Z0-9]{3}[L-Z0-9]{3}.?$"); // 6-7 bytes
    if (re.match(dest).hasMatch()) {
        m_comment = "Mic-E";
        for (int i = 0; i < 6; i++) {
            int charInt = charToIntAscii(dest, i);
            if (inRange(48, 57, charInt)) {
                latDigits.append(QString::number(charInt % 48));
            } else if (inRange(65, 74, charInt)) {
                latDigits.append(QString::number(charInt % 65));
            } else if (inRange(80, 89, charInt)) {
                latDigits.append(QString::number(charInt % 80));
            } else {
                latDigits.append('0'); // Standard states "space" but we put a zero for math
            }

            // Message Type is encoded in 3 bits
            if (i < 3) {
                if (inRange(48, 57, charInt) || charInt == 76) { // 0-9 or L
                    messageBits.append('0');
                } else if (inRange(80, 90, charInt)) { // P-Z, Standard
                    messageBits.append('1');
                    messageType = 0;
                } else if (inRange(65, 75, charInt)) { // A-K, Custom
                    messageBits.append('1');
                    messageType = 1;
                }
            }

            // Latitude Direction
            if (i == 3 && inRange(80, 90, charInt)) {
                latitudeDirection = 1; // North
            }

            // Longitude Offset
            if (i == 4 && inRange(80, 90, charInt)) {
                longitudeOffset = 100;
            }

            // Longitude Direction
            if (i == 5 && inRange(80, 90, charInt)) {
                longitudeDirection = -1; // West
            }
        }

        if (messageTypeLookup.find(messageBits) != messageTypeLookup.end()) {
            m_status = messageTypeLookup[messageBits];
            if (messageType == 1) {
                m_status.append(" (custom)");
            }
            m_hasStatus = true;
        }
        m_latitude = (latDigits.mid(0, 2).toFloat() + latDigits.mid(2, 2).toFloat()/60.00 + latDigits.mid(4, 2).toFloat()/60.0/100.0) * latitudeDirection;
        m_hasPosition = true;
    } else {
        qDebug() << "APRSPacket::parseMicE: Destination invalid regexp match " << dest;
        return false;
    }

    // Mic-E Data is encoded in ASCII Characters
    if (inRange(38, 127, charToIntAscii(info, idx))       // 0: Longitude Degrees, 0-360
        && inRange(38, 97, charToIntAscii(info, idx+1))   // 1: Longitude Minutes, 0-59
        && inRange(28, 127, charToIntAscii(info, idx+2))  // 2: Longitude Hundreths of a minute, 0-99
        && inRange(28, 127, charToIntAscii(info, idx+3))  // 3: Speed (tens), 0-800
        && inRange(28, 125, charToIntAscii(info, idx+4))  // 4: Speed (ones), 0-9, and Course (hundreds), {0, 100, 200, 300}
        && inRange(28, 127, charToIntAscii(info, idx+5))  // 5: Course, 0-99 degrees
       )
    {
        // Longitude; Degrees plus offset encoded in the AX.25 Destination
        // Destination Byte 5, ASCII P through Z indicates an offset of +100
        int deg = (charToIntAscii(info, idx) - 28) + longitudeOffset;
        if (inRange(180, 189, deg))
            deg -= 80;
        if (inRange(190, 199, deg))
            deg -= 190;

        int min = (charToIntAscii(info, idx+1) - 28) % 60;
        int hundreths = charToIntAscii(info, idx+2);

        // Course and Speed
        // Speed (SP+28, units of 10) can use two encodings: ASCII 28-47 and 108-127 are the same
        // Speed & Course (DC+28, Speed units of 1, Course units of 100 e.g. 0, 100, 200, 300) uses two encodings
        int speed = ((charToIntAscii(info, idx+3) - 28) * 10) % 800; // Speed in 10 kts units
        float decoded_speed_course = (float)(charToIntAscii(info, idx+4) - 28) / 10.0;
        speed += floor(decoded_speed_course); // Speed in 1 kt units, added to above
        int course = (((charToIntAscii(info, idx+4) - 28) % 10) * 100) % 400;
        course += charToIntAscii(info, idx+5) - 28;

        m_longitude = (((float)deg) + min/60.00 + hundreths/60.0/100.0) * longitudeDirection;
        m_hasPosition = true;
        m_course = course;
        m_speed = speed;
        m_hasCourseAndSpeed = true;
    } else {
        qDebug() << "APRSPacket::parseMicE: Information Data invalid ASCII range " << info;
        return false;
    }

    // 6: Symbol Code
    // 7: Symbol Table ID, / = standard, \ = alternate, "," = Telemetry
    if (inRange(33, 126, charToIntAscii(info, idx+6))
        && (charToIntAscii(info, idx+7) == 47 || charToIntAscii(info, idx+7) == 92)
       )
    {
        m_symbolTable = info[idx+7].toLatin1();
        m_symbolCode = info[idx+6].toLatin1();
        m_hasSymbol = true;
    }

    // Altitude, encoded in Status Message in meters, converted to feet, above -10000 meters
    // e.g. "4T} -> Doublequote is 34, digit 4 is 52, Capital T is 84. Subtract 33 from each -> 1, 19, 51
    //  Multiply -> (1 * 91 * 91) + (19 * 91) + (51 * 1) - 10000 = 61 meters Mean Sea Level (MSL)
    // ASCII Integer Character Range is 33 to 127
    float altitude = -10000;

    // 4-5 bytes, we only need the 3 to get altitude, e.g. "4T}
    // Some HTs prefix the altitude with ']' or '>', so we match that optionally but ignore it
    QRegularExpression re_mice_altitude("[\\]>]?(.{3})}");
    QRegularExpressionMatch altitude_str = re_mice_altitude.match(info);
    if (altitude_str.hasMatch()) {
        QList<int> micEAltitudeMultipliers = {91 * 91, 91, 1};

        for (int i = 0; i < 3; i++) {
            QString altmatch = altitude_str.captured(1);
            int charInt = charToIntAscii(altmatch, i);
            if (!inRange(33, 127, charInt)) {
                qDebug() << "APRSPacket::parseMicE: Invalid Altitude Byte Found pos:" << QString::number(i) << " ascii int:" << QString::number(charInt);
                break;
            }
            altitude += (float)(charInt - 33) * micEAltitudeMultipliers.at(i);
        }

        // Assume that the Mic-E transmission is Above Ground Level
        if (altitude >= 0) {
            m_altitudeFt = std::round(Units::metresToFeet(altitude));
            m_hasAltitude = true;
        }
    }

    // Mic-E Text Format
    if (info.length() >= 9) {
        QString mice_status = info.mid(9);
        if (altitude_str.hasMatch() && mice_status.indexOf(altitude_str.captured(0)) != -1) {
            mice_status.replace(altitude_str.captured(0), "");
        }

        m_comment += " " + mice_status;
        // TODO Implement the APRS 1.2 Mic-E Text Format http://www.aprs.org/aprs12/mic-e-types.txt
        // Consider the Kenwood leading characters
    }

    // TODO Implement Mic-E Telemetry Data -- need to modify regexp for the Symbol Table Identifier to include comma (,)

    return true;
}

