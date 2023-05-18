///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#include <QDebug>

#include "util/dsc.h"
#include "util/popcount.h"

// "short" strings are meant to be compatible with YaDDNet

QMap<DSCMessage::FormatSpecifier, QString> DSCMessage::m_formatSpecifierStrings = {
    {GEOGRAPHIC_CALL, "Geographic call"},
    {DISTRESS_ALERT,  "Distress alert"},
    {GROUP_CALL,      "Group call"},
    {ALL_SHIPS,       "All ships"},
    {SELECTIVE_CALL,  "Selective call"},
    {AUTOMATIC_CALL,  "Automatic call"}
};

QMap<DSCMessage::FormatSpecifier, QString> DSCMessage::m_formatSpecifierShortStrings = {
    {GEOGRAPHIC_CALL, "AREA"},
    {DISTRESS_ALERT,  "DIS"},
    {GROUP_CALL,      "GRP"},
    {ALL_SHIPS,       "ALL"},
    {SELECTIVE_CALL,  "SEL"},
    {AUTOMATIC_CALL,  "AUT"}
};

QMap<DSCMessage::Category, QString> DSCMessage::m_categoryStrings = {
    {ROUTINE,       "Routine"},
    {SAFETY,        "Safety"},
    {URGENCY,       "Urgency"},
    {DISTRESS,      "Distress"}
};

QMap<DSCMessage::Category, QString> DSCMessage::m_categoryShortStrings = {
    {ROUTINE,       "RTN"},
    {SAFETY,        "SAF"},
    {URGENCY,       "URG"},
    {DISTRESS,      "DIS"}
};

QMap<DSCMessage::FirstTelecommand, QString> DSCMessage::m_telecommand1Strings = {
    {F3E_G3E_ALL_MODES_TP,  "F3E (FM speech)/G3E (phase modulated speech) all modes telephony"},
    {F3E_G3E_DUPLEX_TP,     "F3E (FM speech)/G3E (phase modulated speech) duplex telephony"},
    {POLLING,               "Polling"},
    {UNABLE_TO_COMPLY,      "Unable to comply"},
    {END_OF_CALL,           "End of call"},
    {DATA,                  "Data"},
    {J3E_TP,                "J3E (SSB) telephony"},
    {DISTRESS_ACKNOWLEDGEMENT, "Distress acknowledgement"},
    {DISTRESS_ALERT_RELAY,  "Distress alert relay"},
    {F1B_J2B_TTY_FEC,       "F1B (FSK) J2B (FSK via SSB) TTY FEC"},
    {F1B_J2B_TTY_AQR,       "F1B (FSK) J2B (FSK via SSB) TTY AQR"},
    {TEST,                  "Test"},
    {POSITION_UPDATE,       "Position update"},
    {NO_INFORMATION,        "No information"}
};

QMap<DSCMessage::FirstTelecommand, QString> DSCMessage::m_telecommand1ShortStrings = {
    {F3E_G3E_ALL_MODES_TP,  "F3E/G3E"},
    {F3E_G3E_DUPLEX_TP,     "F3E/G3E, Duplex TP"},
    {POLLING,               "POLL"},
    {UNABLE_TO_COMPLY,      "UNABLE TO COMPLY"},
    {END_OF_CALL,           "EOC"},
    {DATA,                  "DATA"},
    {J3E_TP,                "J3E TP"},
    {DISTRESS_ACKNOWLEDGEMENT, "DISTRESS ACK"},
    {DISTRESS_ALERT_RELAY,  "DISTRESS RELAY"},
    {F1B_J2B_TTY_FEC,       "F1B/J2B TTY-FEC"},
    {F1B_J2B_TTY_AQR,       "F1B/J2B TTY-ARQ"},
    {TEST,                  "TEST"},
    {POSITION_UPDATE,       "POSUPD"},
    {NO_INFORMATION,        "NOINF"}
};

QMap<DSCMessage::SecondTelecommand, QString> DSCMessage::m_telecommand2Strings = {
    {NO_REASON,                 "No reason"},
    {CONGESTION,                "Congestion at switching centre"},
    {BUSY,                      "Busy"},
    {QUEUE,                     "Queue indication"},
    {BARRED,                    "Station barred"},
    {NO_OPERATOR,               "No operator available"},
    {OPERATOR_UNAVAILABLE,      "Operator temporarily unavailable"},
    {EQUIPMENT_DISABLED,        "Equipment disabled"},
    {UNABLE_TO_USE_CHANNEL,     "Unable to use proposed channel"},
    {UNABLE_TO_USE_MODE,        "Unable to use proposed mode"},
    {NOT_PARTIES_TO_CONFLICT,   "Ships and aircraft of States not parties to an armed conflict"},
    {MEDICAL_TRANSPORTS,        "Medical transports"},
    {PAY_PHONE,                 "Pay-phone/public call office"},
    {FAX,                       "Facsimile"},
    {NO_INFORMATION_2,          "No information"}
};

QMap<DSCMessage::SecondTelecommand, QString> DSCMessage::m_telecommand2ShortStrings = {
    {NO_REASON,                 "NO REASON GIVEN"},
    {CONGESTION,                "CONGESTION AT MARITIME CENTRE"},
    {BUSY,                      "BUSY"},
    {QUEUE,                     "QUEUE INDICATION"},
    {BARRED,                    "STATION BARRED"},
    {NO_OPERATOR,               "NO OPERATOR AVAILABLE"},
    {OPERATOR_UNAVAILABLE,      "OPERATOR TEMPORARILY UNAVAILABLE"},
    {EQUIPMENT_DISABLED,        "EQUIPMENT DISABLED"},
    {UNABLE_TO_USE_CHANNEL,     "UNABLE TO USE PROPOSED CHANNEL"},
    {UNABLE_TO_USE_MODE,        "UNABLE TO USE PROPOSED MODE"},
    {NOT_PARTIES_TO_CONFLICT,   "SHIPS/AIRCRAFT OF STATES NOT PARTIES TO ARMED CONFLICT"},
    {MEDICAL_TRANSPORTS,        "MEDICAL TRANSPORTS"},
    {PAY_PHONE,                 "PAY-PHONE/PUBLIC CALL OFFICE"},
    {FAX,                       "FAX/DATA ACCORDING ITU-R M1081"},
    {NO_INFORMATION_2,          "NOINF"}
};

QMap<DSCMessage::DistressNature, QString> DSCMessage::m_distressNatureStrings = {
    {FIRE,              "Fire, explosion"},
    {FLOODING,          "Flooding"},
    {COLLISION,         "Collision"},
    {GROUNDING,         "Grounding"},
    {LISTING,           "Listing"},
    {SINKING,           "Sinking"},
    {ADRIFT,            "Adrift"},
    {UNDESIGNATED,      "Undesignated"},
    {ABANDONING_SHIP,   "Abandoning ship"},
    {PIRACY,            "Piracy, armed attack"},
    {MAN_OVERBOARD,     "Man overboard"},
    {EPIRB,             "EPIRB"}
};

QMap<DSCMessage::EndOfSignal, QString> DSCMessage::m_endOfSignalStrings = {
    {REQ, "Req ACK"},
    {ACK, "ACK"},
    {EOS, "EOS"}
};

QMap<DSCMessage::EndOfSignal, QString> DSCMessage::m_endOfSignalShortStrings = {
    {REQ, "REQ"},
    {ACK, "ACK"},
    {EOS, "EOS"}
};

DSCMessage::DSCMessage(const QByteArray& data, QDateTime dateTime) :
    m_dateTime(dateTime),
    m_data(data)
{
    decode(data);
}

QString DSCMessage::toString(const QString separator) const
{
    QStringList s;

    s.append(QString("Format specifier: %1").arg(formatSpecifier()));

    if (m_hasAddress) {
        s.append(QString("Address: %1").arg(m_address));
    }
    if (m_hasCategory) {
        s.append(QString("Category: %1").arg(category()));
    }

    s.append(QString("Self Id: %1").arg(m_selfId));

    if (m_hasTelecommand1) {
        s.append(QString("Telecommand 1: %1").arg(telecommand1(m_telecommand1)));
    }
    if (m_hasTelecommand2) {
        s.append(QString("Telecommand 2: %1").arg(telecommand2(m_telecommand2)));
    }

    if (m_hasDistressId) {
        s.append(QString("Distress Id: %1").arg(m_distressId));
    }
    if (m_hasDistressNature)
    {
        s.append(QString("Distress nature: %1").arg(distressNature(m_distressNature)));
        s.append(QString("Distress coordinates: %1").arg(m_position));
    }
    else if (m_hasPosition)
    {
        s.append(QString("Position: %1").arg(m_position));
    }

    if (m_hasFrequency1) {
        s.append(QString("RX Frequency: %1Hz").arg(m_frequency1));
    }
    if (m_hasChannel1) {
        s.append(QString("RX Channel: %1").arg(m_channel1));
    }
    if (m_hasFrequency2) {
        s.append(QString("TX Frequency: %1Hz").arg(m_frequency2));
    }
    if (m_hasChannel2) {
        s.append(QString("TX Channel: %1").arg(m_channel2));
    }
    if (m_hasNumber) {
        s.append(QString("Phone Number: %1").arg(m_number));
    }

    if (m_hasTime) {
        s.append(QString("Time: %1").arg(m_time.toString()));
    }
    if (m_hasSubsequenceComms) {
        s.append(QString("Subsequent comms: %1").arg(telecommand1(m_subsequenceComms)));
    }

    return s.join(separator);
}

QString DSCMessage::toYaddNetFormat(const QString& id, qint64 frequency) const
{
    QStringList s;

    // rx_id
    s.append(QString("[%1]").arg(id));
    // rx_freq
    float frequencyKHZ = frequency / 1000.0f;
    s.append(QString("%1").arg(frequencyKHZ, 0, 'f', 1));
    // fmt
    s.append(formatSpecifier(true));
    // to
    if (m_hasAddress)
    {
        if (m_formatSpecifier == GEOGRAPHIC_CALL)
        {
            char ns = m_addressLatitude >= 0 ? 'N' : 'S';
            char ew = m_addressLongitude >= 0 ? 'E' : 'W';
            int lat = abs(m_addressLatitude);
            int lon = abs(m_addressLongitude);
            s.append(QString("AREA %2%1%6=>%4%1 %3%1%7=>%5%1")
                                .arg(QChar(0xb0))  // degree
                                .arg(lat, 2, 10, QChar('0'))
                                .arg(lon, 3, 10, QChar('0'))
                                .arg(m_addressLatAngle, 2, 10, QChar('0'))
                                .arg(m_addressLonAngle, 2, 10, QChar('0'))
                                .arg(ns)
                                .arg(ew));
        }
        else
        {
            s.append(m_address);
        }
    }
    else
    {
        s.append("");
    }
    // cat
    s.append(category(true));
    // from
    s.append(m_selfId);

    // tc1
    if (m_hasTelecommand1) {
        s.append(telecommand1(m_telecommand1, true));
    } else {
        s.append("--");
    }
    // tc2
    if (m_hasTelecommand2) {
        s.append(telecommand2(m_telecommand2, true));
    } else {
        s.append("--");
    }
    // distress fields don't appear to be used!
    // freq
    if (m_hasFrequency1 && m_hasFrequency2) {
        s.append(QString("%1/%2KHz").arg(m_frequency1/1000.0, 7, 'f', 1, QChar('0')).arg(m_frequency2/1000.0, 7, 'f', 1, QChar('0')));
    } else if (m_hasFrequency1) {
        s.append(QString("%1KHz").arg(m_frequency1/1000.0, 7, 'f', 1, QChar('0')));
    } else if (m_hasFrequency2) {
        s.append(QString("%1KHz").arg(m_frequency2/1000.0, 7, 'f', 1, QChar('0')));
    } else if (m_hasChannel1 && m_hasChannel2) {
        s.append(QString("%1/%2").arg(m_channel1).arg(m_channel2));
    } else if (m_hasChannel1) {
        s.append(QString("%1").arg(m_channel1));
    } else if (m_hasChannel2) {
        s.append(QString("%1").arg(m_channel2));
    } else {
        s.append("--");
    }
    // pos
    if (m_hasPosition) {
        s.append(m_position);   // FIXME: Format??
    } else {
        s.append("--");         // Sometimes this is " -- ". in YaDD Why?
    }

    // eos
    s.append(endOfSignal(m_eos, true));
    // ecc
    s.append(QString("ECC %1 %2").arg(m_calculatedECC).arg(m_eccOk ? "OK" : "ERR"));

    return s.join(";");
}

QString DSCMessage::formatSpecifier(bool shortString) const
{
    if (shortString)
    {
        if (m_formatSpecifierShortStrings.contains(m_formatSpecifier)) {
            return m_formatSpecifierShortStrings[m_formatSpecifier];
        } else {
            return QString("UNK/ERR").arg(m_formatSpecifier);
        }
    }
    else
    {
        if (m_formatSpecifierStrings.contains(m_formatSpecifier)) {
            return m_formatSpecifierStrings[m_formatSpecifier];
        } else {
            return QString("Unknown (%1)").arg(m_formatSpecifier);
        }
    }
}


QString DSCMessage::category(bool shortString) const
{
    if (shortString)
    {
        if (m_categoryShortStrings.contains(m_category)) {
            return m_categoryShortStrings[m_category];
        } else {
            return QString("UNK/ERR").arg(m_category);
        }
    }
    else
    {
        if (!m_hasCategory) {
            return "N/A";
        } else if (m_categoryStrings.contains(m_category)) {
            return m_categoryStrings[m_category];
        } else {
            return QString("Unknown (%1)").arg(m_category);
        }
    }
}

QString DSCMessage::telecommand1(FirstTelecommand telecommand, bool shortString)
{
    if (shortString)
    {
        if (m_telecommand1ShortStrings.contains(telecommand)) {
            return m_telecommand1ShortStrings[telecommand];
        } else {
            return QString("UNK/ERR").arg(telecommand);
        }
    }
    else
    {
        if (m_telecommand1Strings.contains(telecommand)) {
            return m_telecommand1Strings[telecommand];
        } else {
            return QString("Unknown (%1)").arg(telecommand);
        }
    }
}

QString DSCMessage::telecommand2(SecondTelecommand telecommand, bool shortString)
{
    if (shortString)
    {
        if (m_telecommand2ShortStrings.contains(telecommand)) {
            return m_telecommand2ShortStrings[telecommand];
        } else {
            return QString("UNK/ERR").arg(telecommand);
        }
    }
    else
    {
        if (m_telecommand2Strings.contains(telecommand)) {
            return m_telecommand2Strings[telecommand];
        } else {
            return QString("Unknown (%1)").arg(telecommand);
        }
    }
}

QString DSCMessage::distressNature(DistressNature nature)
{
    if (m_distressNatureStrings.contains(nature)) {
        return m_distressNatureStrings[nature];
    } else {
        return QString("Unknown (%1)").arg(nature);
    }
}

QString DSCMessage::endOfSignal(EndOfSignal eos, bool shortString)
{
    if (shortString)
    {
        if (m_endOfSignalShortStrings.contains(eos)) {
            return m_endOfSignalShortStrings[eos];
        } else {
            return QString("UNK/ERR").arg(eos);
        }
    }
    else
    {
        if (m_endOfSignalStrings.contains(eos)) {
            return m_endOfSignalStrings[eos];
        } else {
            return QString("Unknown (%1)").arg(eos);
        }
    }
}


QString DSCMessage::symbolsToDigits(const QByteArray data, int startIdx, int length)
{
    QString s;

    for (int i = 0; i < length; i++)
    {
        QString digits = QString("%1").arg((int)data[startIdx+i], 2, 10, QChar('0'));
        s = s.append(digits);
    }

    return s;
}

QString DSCMessage::formatCoordinates(int latitude, int longitude)
{
    QString lat, lon;
    if (latitude >= 0) {
        lat = QString("%1%2N").arg(latitude).arg(QChar(0xb0));
    } else {
        lat = QString("%1%2S").arg(-latitude).arg(QChar(0xb0));
    }
    if (longitude >= 0) {
        lon = QString("%1%2E").arg(longitude).arg(QChar(0xb0));
    } else {
        lon = QString("%1%2W").arg(-longitude).arg(QChar(0xb0));
    }
    return QString("%1 %2").arg(lat).arg(lon);
}

void DSCMessage::decode(const QByteArray& data)
{
    int idx = 0;

    // Format specifier
    m_formatSpecifier = (FormatSpecifier) data[idx++];
    m_formatSpecifierMatch = m_formatSpecifier == data[idx++];

    // Address and category
    if (m_formatSpecifier != DISTRESS_ALERT)
    {
        if (m_formatSpecifier != ALL_SHIPS)
        {
            m_address = symbolsToDigits(data, idx, 5);
            idx += 5;
            m_hasAddress = true;

            if (m_formatSpecifier == SELECTIVE_CALL)
            {
                m_address = formatAddress(m_address);
            }
            else if (m_formatSpecifier == GEOGRAPHIC_CALL)
            {
                // Address definines a geographic rectangle. We have NW coord + 2 angles
                QChar azimuthSector = m_address[0];
                m_addressLatitude = m_address[1].digitValue() * 10 + m_address[2].digitValue(); // In degrees
                m_addressLongitude = m_address[3].digitValue() * 100 + m_address[4].digitValue() * 10 + m_address[5].digitValue(); // In degrees
                switch (azimuthSector.toLatin1())
                {
                case '0':   // NE
                    break;
                case '1':   // NW
                    m_addressLongitude = -m_addressLongitude;
                    break;
                case '2':   // SE
                    m_addressLatitude = -m_addressLatitude;
                    break;
                case '3':   // SW
                    m_addressLongitude = -m_addressLongitude;
                    m_addressLatitude = -m_addressLatitude;
                    break;
                default:
                    break;
                }
                m_addressLatAngle = m_address[6].digitValue() * 10 + m_address[7].digitValue();
                m_addressLonAngle = m_address[8].digitValue() * 10 + m_address[9].digitValue();

                int latitude2 = m_addressLatitude + m_addressLatAngle;
                int longitude2 = m_addressLongitude + m_addressLonAngle;

                /*m_address = QString("Lat %2%1 Lon %3%1 %4%5%6%1 %4%7%8%1")
                                .arg(QChar(0xb0))  // degree
                                .arg(m_addressLatitude)
                                .arg(m_addressLongitude)
                                .arg(QChar(0x0394)) // delta
                                .arg(QChar(0x03C6)) // phi
                                .arg(m_addressLatAngle)
                                .arg(QChar(0x03BB)) // lambda
                                .arg(m_addressLonAngle);*/
                m_address = QString("%1 - %2")
                            .arg(formatCoordinates(m_addressLatitude, m_addressLongitude))
                            .arg(formatCoordinates(latitude2, longitude2));
            }
        }
        else
        {
            m_hasAddress = false;
        }
        m_category = (Category) data[idx++];
        m_hasCategory = true;
    }
    else
    {
        m_hasAddress = false;
        m_hasCategory = true;
    }

    // Self Id
    m_selfId = symbolsToDigits(data, idx, 5);
    m_selfId = formatAddress(m_selfId);
    idx += 5;

    // Telecommands
    if (m_formatSpecifier != DISTRESS_ALERT)
    {
        m_telecommand1 = (FirstTelecommand) data[idx++];
        m_hasTelecommand1 = true;

        if (m_category != DISTRESS)  // Not Distress Alert Ack / Relay
        {
            m_telecommand2 = (SecondTelecommand) data[idx++];
            m_hasTelecommand2 = true;
        }
        else
        {
            m_hasTelecommand2 = false;
        }
    }
    else
    {
         m_hasTelecommand1 = false;
         m_hasTelecommand2 = false;
    }

    // ID of source of distress for relays and acks
    if (m_hasCategory && m_category == DISTRESS)
    {
        m_distressId = symbolsToDigits(data, idx, 5);
        m_distressId = formatAddress(m_distressId);
        idx += 5;
        m_hasDistressId = true;
    }
    else
    {
        m_hasDistressId = false;
    }

    if (m_formatSpecifier == DISTRESS_ALERT)
    {
        m_distressNature = (DistressNature) data[idx++];
        m_position = formatCoordinates(symbolsToDigits(data, idx, 5));
        idx += 5;
        m_hasDistressNature = true;
        m_hasPosition = true;

        m_hasFrequency1 = false;
        m_hasChannel1 = false;
        m_hasFrequency2 = false;
        m_hasChannel2 = false;
    }
    else if (m_hasCategory && (m_category != DISTRESS))
    {
        m_hasDistressNature = false;
        // Frequency or position
        if (data[idx] == 55)
        {
            // Position 6
            m_position = formatCoordinates(symbolsToDigits(data, idx, 5));
            idx += 5;
            m_hasPosition = true;

            m_hasFrequency1 = false;
            m_hasChannel1 = false;
            m_hasFrequency2 = false;
            m_hasChannel2 = false;
        }
        else
        {
            m_hasPosition = false;
            // Frequency
            m_frequency1 = 0;
            decodeFrequency(data, idx, m_frequency1, m_channel1);
            m_hasFrequency1 = m_frequency1 != 0;
            m_hasChannel1 = !m_channel1.isEmpty();

            if (m_formatSpecifier != AUTOMATIC_CALL)
            {
                m_frequency2 = 0;
                decodeFrequency(data, idx, m_frequency2, m_channel2);
                m_hasFrequency2 = m_frequency2 != 0;
                m_hasChannel2 = !m_channel2.isEmpty();
            }
            else
            {
                m_hasFrequency2 = false;
                m_hasChannel2 = false;
            }
        }
    }
    else
    {
        m_hasDistressNature = false;
        m_hasPosition = false;
        m_hasFrequency1 = false;
        m_hasChannel1 = false;
        m_hasFrequency2 = false;
        m_hasChannel2 = false;
    }

    if (m_formatSpecifier == AUTOMATIC_CALL)
    {
        signed char oddEven = data[idx++];
        int len = data.size() - idx - 2; // EOS + ECC
        m_number = symbolsToDigits(data, idx, len);
        idx += len;
        if (oddEven == 105) { // Is number an odd number?
            m_number = m_number.mid(1); // Drop leading digit (which should be a 0)
        }
        m_hasNumber = true;
    }
    else
    {
        m_hasNumber = false;
    }

    // Time
    if (   (m_formatSpecifier == DISTRESS_ALERT)
        || (m_hasCategory && (m_category == DISTRESS))
        //|| (m_formatSpecifier == SELECTIVE_CALL) && (m_category == SAFETY) && (m_telecommand1 == POSITION_UPDATE) && (m_telecommand2 == 126) && (m_frequency == pos4))
       )
    {
        // 8 8 8 8 for no time
        QString time = symbolsToDigits(data, idx, 2);
        if (time != "8888")
        {
            m_time = QTime(time.left(2).toInt(), time.right(2).toInt());
            m_hasTime = true;
        }
        else
        {
            m_hasTime = false;
        }
        // FIXME: Convert to QTime?
    }
    else
    {
        m_hasTime = false;
    }

    // Subsequent communications
    if ((m_formatSpecifier == DISTRESS_ALERT) || (m_hasCategory && (m_category == DISTRESS)))
    {
        m_subsequenceComms = (FirstTelecommand)data[idx++];
        m_hasSubsequenceComms = true;
    }
    else
    {
        m_hasSubsequenceComms = false;
    }

    m_eos = (EndOfSignal) data[idx++];
    m_ecc = data[idx++];

    checkECC(data);

    // Indicate message as being invalid if any unexpected data, too long, or ECC didn't match
    if (   m_formatSpecifierStrings.contains(m_formatSpecifier)
        && (!m_hasCategory || (m_hasCategory && m_categoryStrings.contains(m_category)))
        && (!m_hasTelecommand1 || (m_hasTelecommand1 && m_telecommand1Strings.contains(m_telecommand1)))
        && (!m_hasTelecommand2 || (m_hasTelecommand2 && m_telecommand2Strings.contains(m_telecommand2)))
        && (!m_hasDistressNature || (m_hasDistressNature && m_distressNatureStrings.contains(m_distressNature)))
        && m_endOfSignalStrings.contains(m_eos)
        && (!data.contains(-1))
        && (data.size() < DSCDecoder::m_maxBytes)
        && m_eccOk
       ) {
        m_valid = true;
    } else {
        m_valid = false;
    }

}

void DSCMessage::checkECC(const QByteArray& data)
{
    m_calculatedECC = 0;
    // Only use one format specifier and one EOS
    for (int i = 1; i < data.size() - 1; i++) {
        m_calculatedECC ^= data[i];
    }
    m_eccOk = m_calculatedECC == m_ecc;
}

void DSCMessage::decodeFrequency(const QByteArray& data, int& idx, int& frequency, QString& channel)
{
    // No frequency information is indicated by 126 repeated 3 times
    if ((data[idx] == 126) && (data[idx+1] == 126) && (data[idx+2] == 126))
    {
        idx += 3;
        return;
    }

    // Extract frequency digits
    QString s = symbolsToDigits(data, idx, 3);
    idx += 3;
    if (s[0] == '4')
    {
        s = s.append(symbolsToDigits(data, idx, 1));
        idx++;
    }

    if ((s[0] == '0') || (s[0] == '1') || (s[0] == '2'))
    {
        frequency = s.toInt() * 100;
    }
    else if (s[0] == '3')
    {
        channel = "CH" + s.mid(1); // HF/MF
    }
    else if (s[0] == '4')
    {
        frequency = s.mid(1).toInt() * 10; // Frequency in multiples of 10Hz
    }
    else if (s[0] == '9')
    {
        channel = "CH" + s.mid(2) + "VHF";  // VHF
    }
}

QString DSCMessage::formatAddress(const QString &address) const
{
    // First 9 digits should be MMSI
    // Last digit should always be 0, except for ITU-R M.1080, which allows 10th digit to specify different equipement on same vessel
    if (address.right(1) == "0") {
        return address.left(9);
    } else {
        return QString("%1-%2").arg(address.left(9)).arg(address.right(1));
    }
}

QString DSCMessage::formatCoordinates(const QString& coords)
{
    if (coords == "9999999999")
    {
        return "Not available";
    }
    else
    {
        QChar quadrant = coords[0];
        QString latitude = QString("%1%3%2\'")
            .arg(coords.mid(1, 2))
            .arg(coords.mid(3, 2))
            .arg(QChar(0xb0));
        QString longitude = QString("%1%3%2\'")
            .arg(coords.mid(1, 3))
            .arg(coords.mid(4, 2))
            .arg(QChar(0xb0));
        switch (quadrant.toLatin1())
        {
        case '0':
            latitude = latitude.append('N');
            longitude = longitude.append('E');
            break;
        case '1':
            latitude = latitude.append('N');
            longitude = longitude.append('W');
            break;
        case '2':
            latitude = latitude.append('S');
            longitude = longitude.append('E');
            break;
        case '3':
            latitude = latitude.append('S');
            longitude = longitude.append('W');
            break;
        }
        return QString("%1 %2").arg(latitude).arg(longitude);
    }
}

// Doesn't include 125 111 125 as these will have be detected already, in DSDDemodSink
const signed char DSCDecoder::m_expectedSymbols[] = {
    110,
    125, 109,
    125, 108,
    125, 107,
    125, 106
};

int DSCDecoder::m_maxBytes = 40; // Max bytes in any message

void DSCDecoder::init(int offset)
{
    if (offset == 0)
    {
        m_state = FILL_DX;
    }
    else
    {
        m_phaseIdx = offset;
        m_state = PHASING;
    }
    m_idx = 0;
    m_errors = 0;
    m_bytes = QByteArray();
    m_eos = false;
}

bool DSCDecoder::decodeSymbol(signed char symbol)
{
    bool ret = false;

    switch (m_state)
    {
    case PHASING:
        // Check if received phasing signals are as expected
        if (symbol != m_expectedSymbols[9-m_phaseIdx]) {
            m_errors++;
        }
        m_phaseIdx--;
        if (m_phaseIdx == 0) {
            m_state = FILL_DX;
        }
        break;

    case FILL_DX:
        // Fill up buffer
        m_buf[m_idx++] = symbol;
        if (m_idx == BUFFER_SIZE)
        {
            m_state = RX;
            m_idx = 0;
        }
        else
        {
            m_state = FILL_RX;
        }
        break;

    case FILL_RX:
        if (   ((m_idx == 1) && (symbol != 106))
            || ((m_idx == 2) && (symbol != 105))
           )
        {
            m_errors++;
        }
        m_state = FILL_DX;
        break;

    case RX:
        {
            signed char a = selectSymbol(m_buf[m_idx], symbol);

            if (DSCMessage::m_endOfSignalStrings.contains((DSCMessage::EndOfSignal) a)) {
                m_state = DX_EOS;
            } else {
                m_state = DX;
            }

            if (m_bytes.size() > m_maxBytes)
            {
                ret = true;
                m_state = NO_EOS;
            }
        }
        break;

    case DX:
        // Save received character in buffer
        m_buf[m_idx] = symbol;
        m_idx = (m_idx + 1) % BUFFER_SIZE;
        m_state = RX;
        break;

    case DX_EOS:
        // Save, EOS symbol
        m_buf[m_idx] = symbol;
        m_idx = (m_idx + 1) % BUFFER_SIZE;
        m_state = RX_EOS;
        break;

    case RX_EOS:
        selectSymbol(m_buf[m_idx], symbol);
        m_state = DONE;
        ret = true;
        break;

    case DONE:
    case NO_EOS:
        break;

    }

    return ret;
}

// Reverse order of bits in a byte
unsigned char DSCDecoder::reverse(unsigned char b)
{
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

// Convert 10 bits to a symbol
// Returns -1 if error detected
signed char DSCDecoder::bitsToSymbol(unsigned int bits)
{
    signed char data = reverse(bits >> 3) >> 1;
    int zeros = 7-popcount(data);
    int expectedZeros = bits & 0x7;
    if (zeros == expectedZeros) {
        return data;
    } else {
        return -1;
    }
}

// Decode 10-bits to symbols then remove errors using repeated symbols
bool DSCDecoder::decodeBits(int bits)
{
    signed char symbol = bitsToSymbol(bits);
    //qDebug() << "Bits2sym: " << Qt::hex << bits << Qt::hex << symbol;
    return decodeSymbol(symbol);
}

// Select time diversity symbol without errors
signed char DSCDecoder::selectSymbol(signed char dx, signed char rx)
{
    signed char s;
    if (dx != -1)
    {
        s = dx;  // First received character has no detectable error
        if (dx != rx) {
            m_errors++;
        }
    }
    else if (rx != -1)
    {
        s = rx;  // Second received character has no detectable error
        m_errors++;
    }
    else
    {
        s = '*'; // Both received characters have errors
        m_errors += 2;
    }
    m_bytes.append(s);

    return s;
}
