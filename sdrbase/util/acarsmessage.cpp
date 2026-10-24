///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#include "util/acarsmessage.h"

#include <cmath>
#include <mutex>

#include <QDebug>
#include <QRegularExpression>

#include "util/do219.h"
// For the OOOI event kinds, which the short movement reports map their label on to
#include "util/aircraftreport.h"

// Imbedded Message Identifiers - ARINC 622: Table 2-1
const QHash<QString, QString> m_imis = {
    {"AD1", "Automatic dependent surveillance (ARINC 745-1)"},
    {"ADS", "Automatic dependent surveillance (ARINC 745-2)"},
    {"DIS", "Automatic dependent surveillance (ARINC 745-1/2)"},
    {"AT0", "Air traffic control communication (Preceeding DO-219)"},
    {"AT1", "Air traffic control communication"},     // DO-219 / CPCLD
    {"CR1", "Air traffic control communication - Connect request"},
    {"CC1", "Air traffic control communication - Connect confirm"},
    {"DR1", "Air traffic control communication - Disconnect request"}
};

#include "util/ourairportsdb.h"

QSharedPointer<const QHash<QString, AirportInformation *>> m_airportInfo;

void acarsInitAirportInfo()
{
    static std::once_flag airportInfoInit;
    std::call_once(airportInfoInit, []() {
        m_airportInfo = OurAirportsDB::getAirportsByIdent();
    });
}

static QString icaoAirportName(const QString &icao)
{
    if (m_airportInfo)
    {
        if (m_airportInfo->contains(icao)) {
            return m_airportInfo->value(icao)->m_name;
        }
    }
    return "";
}

// Some formats name airports in three letter IATA where everything downstream - the
// map, the flight plan, the Aircraft feature - works in four letter ICAO. Translated on
// the way in rather than at each use, so only this one place has to know.
static QString icaoFromIata(const QString &iata)
{
    if (iata.size() != 3) {
        return QString();
    }
    QSharedPointer<const QHash<QString, AirportInformation *>> airports =
        OurAirportsDB::getAirportsByIata();
    const AirportInformation *airport = airports ? airports->value(iata, nullptr) : nullptr;
    return airport ? airport->m_ident : QString();
}

static QString icaoAndAirportName(const QString &icao)
{
    QString airportName = icaoAirportName(icao);
    if (airportName.isEmpty()) {
        return icao;
    } else {
        return QString("%1 (%2)").arg(icao).arg(airportName);
    }
}

AcarsMessage::AcarsMessage(const QByteArray &message)
{
    // Every field below is read at a fixed offset, so a block shorter than the smallest
    // legal one would index off the end - and QString/QByteArray operator[] only asserts
    // in a debug build, so a release build reads whatever is there instead of failing.
    // decode() checks the length before it builds one of these, but nothing obliged it
    // to: the test harness constructs an AcarsFlightStatus from an empty QByteArray to
    // exercise decode() on its own. A short block leaves every field empty.
    if (message.size() < ACARS_MIN_BYTES) {
        return;
    }

    // Mode is destination of message
    // 2 = All ground stations / aircraft - Other characters are specific station / aircraft
    // and country specific
    m_mode = QString("%1").arg(message[1]);

    // Address is aircraft registration. Prefixed with '.'s if registration is shorter than field width
    m_address = QString(message.mid(2, 7));
    while (m_address.startsWith(".")) {
        m_address = m_address.mid(1); // Remove prefix
    }

    m_ack = message[9];

    // Label (type of message)
    m_label = message.mid(10, 2);

    // Block ID indicates if downlink (0-9) or uplink (A-Z or NUL)
    m_blockId = message[12];
    m_uplink = isUplink(m_blockId);

    // Extract text
    qsizetype textSize = message.length() - 14 - 1 - 2 - 1;
    if (textSize >= 10)
    {
        m_text = message.mid(14, textSize);

        if (!m_uplink)
        {
            // Extract downlink fields
            m_originator = m_text[0];
            m_messageNumber = m_text.mid(1, 2);
            m_blockSequence = m_text[3];
            m_flight = m_text.mid(4, 6);
            // Remove extracted fields from text
            m_text = m_text.mid(10);
            // Is there a sublabel? (Typically for H1 label)
            if ((m_text.size() >= 3) && (m_text[0] == '#'))
            {
                m_subLabel = m_text.mid(1, 2);
                m_text = m_text.mid(3);
                // An FMC sublabel is THREE characters - #M1B, #M3B - and taking only two
                // of them left the third at the front of the block's text.
                //
                // On a single block message that is harmless, because the report parser
                // recognises a stray character before POS or FPN/ and drops it. On a
                // multi block message it is not: every block carries the prefix, so every
                // block after the first left its supplementary character buried in the
                // MIDDLE of the reassembled text, where nothing looks for it. Off air
                // that put a stray B every 207 characters - one per block boundary -
                // through the middle of numbers in flight plans and predicted routes,
                // on four different aircraft.
                //
                // The supplementary character is a literal B, on EVERY sublabel and not
                // only the FMC ones. That was first fixed for M1 alone, from the one
                // message that showed it; measured across 5532 blocks off air it is B in
                // 2247 cases out of 2247, spread over ten different sublabels - DF 1932,
                // M1 126, CF 70, 10 60, EI 27, and T2, T3, T5, T6, T8. Never any other
                // character, which a body starting at that position could not manage.
                //
                // Reading the bodies confirms it rather than resting on the count: #CFB
                // is followed by ACMF210_TAKEOFF..., the aircraft condition monitoring
                // function, #EIB by RPT28;PG1;REAL, and #T5B and #T6B by a path beginning
                // with a slash. With the B left on they would read BACMF210, BRPT28 and
                // B/ILNGE.
                //
                // So the M<digit> restriction was leaving it in place for 2121 blocks -
                // and, on a message that arrives in several, at every block boundary in
                // the middle of the text. Matched on the character itself rather than on
                // "any capital", so a sublabel whose body genuinely begins with some
                // other letter cannot lose it.
                if ((m_subLabel.size() == 2) && !m_text.isEmpty() && (m_text[0] == 'B'))
                {
                    m_subLabelSupplement = m_text.left(1);
                    m_text = m_text.mid(1);
                }
            }
        }
        else
        {
            if (m_text.startsWith("- #"))
            {
                m_subLabel = m_text.mid(3, 2);
                m_text = m_text.mid(5);
            }
        }
    }

    m_multiBlock = message[message.length() - 4] == ASCII_ETB;
}

QString AcarsMessage::toString()
{
    return "";   // Return nothing, as we don't know how to decode this message
}

AcarsMessage *AcarsMessage::decode(const QByteArray &message)
{
    return decodeBlock(message, nullptr);
}

AcarsMessage *AcarsMessage::decode(const QByteArray &message, const QString &text)
{
    return decodeBlock(message, &text);
}

AcarsMessage *AcarsMessage::decodeBlock(const QByteArray &message, const QString *textOverride)
{
    // A no-text block is 17 bytes and has ETX directly after the block identifier. STX is
    // present only when text follows (ARINC 618 Appendix B).
    const bool noText = (message.size() == 17) && ((message[13] & 0x7f) == ASCII_ETX);
    const bool hasText = (message.size() >= 18) && ((message[13] & 0x7f) == ASCII_STX);

    if ((noText || hasText)
        && (message[0] == (char)ASCII_SOH)
        && (message[message.size()-1] == (char)ASCII_DEL))
    {
        // Remove parity bit from MSB
        QByteArray ascii = message;
        for (int i = 0; i < message.length(); i++) {
            ascii[i] = ascii[i] & 0x7f;
        }

        // Extract label (type of message)
        QString label(ascii.mid(10, 2));

        // Extract block identifier to determine if uplink or downlink
        QChar blockId(ascii[12]);
        bool uplink = isUplink(blockId);

        // Extract message text
        int start = 14;
        if (!uplink) {
            start += 10; // Skip msg num and flight
        }
        
        QString text;
        qsizetype textSize = message.length() - start - 1 - 2 - 1;
        if (textSize > 0) {
            text = ascii.mid(start, textSize);
        }

        return decode(ascii, label, textOverride ? *textOverride : text, uplink);
    }
    else
    {
        return nullptr;
    }
}

// A runway designator without the padding an FMS writes when there is none.
//
// The field is a fixed three characters, so a runway with no parallel gets an O to fill
// it out: 27O is runway 27, 02O is runway 02. L, R and C are real designators and stay.
// Seen off air as 27O and 02O against genuine 19L, 19R and 26R - and neither Heraklion
// nor Alicante, the two that produced them, has a second parallel runway to designate.
//
// Applied to each designator in the field rather than to the field as a whole, because a
// plan gives the departure runway with the arrival's in brackets after it - 27O(19O).
static QString stripRunwayFiller(const QString& runway)
{
    static const QRegularExpression fillerRe(R"((^|[^0-9A-Z])([0-9]{2})O(?![0-9A-Z]))");
    QString stripped = runway;
    // Repeated because the matches can abut through the bracket, and a single pass
    // consumes the character between them
    QString previous;
    while (stripped != previous)
    {
        previous = stripped;
        stripped.replace(fillerRe, "\\1\\2");
    }
    return stripped;
}

AcarsLoadsheet::AcarsLoadsheet(const QByteArray &message) :
    AcarsMessage(message)
{
}

// The figures out of a loadsheet. Every one is optional - a preliminary sheet carries
// less than a final, and airlines differ in what they bother to send - so anything not
// found simply stays unset rather than making the whole thing fail to decode.
bool AcarsLoadsheet::decode(const QString &text)
{
    m_text = text;
    m_final = text.contains("LOADSHEET FINAL");

    // WEIGHT nnnnn [MAX nnnnn]. The spacing varies between airlines and between fields
    // in the same sheet, so it is matched rather than counted.
    auto weight = [&text](const char *name, int *value, int *maximum)
    {
        const QRegularExpression re(QString(R"(\b%1\s+(\d{3,6})(?:\s+MAX\s+(\d{3,6}))?)")
                                        .arg(name));
        const QRegularExpressionMatch match = re.match(text);
        if (!match.hasMatch()) {
            return;
        }
        *value = match.captured(1).toInt();
        if (maximum && !match.captured(2).isEmpty()) {
            *maximum = match.captured(2).toInt();
        }
    };

    weight("ZFW", &m_zeroFuelWeight, &m_zeroFuelWeightMax);
    weight("TOW", &m_takeoffWeight, &m_takeoffWeightMax);
    weight("LAW", &m_landingWeight, &m_landingWeightMax);
    weight("TOF", &m_takeoffFuel, nullptr);
    weight("TIF", &m_tripFuel, nullptr);
    weight("UNDLD", &m_underload, nullptr);

    // PAX/16/120 TTL 137 - the classes and then the total, which is the useful one
    static const QRegularExpression paxRe(R"(\bPAX/[0-9/]*\s*TTL\s+(\d{1,3}))");
    const QRegularExpressionMatch pax = paxRe.match(text);
    if (pax.hasMatch()) {
        m_passengers = pax.captured(1).toInt();
    }

    static const QRegularExpression macZfwRe(R"(\bMACZFW\s+([0-9]+\.[0-9]))");
    const QRegularExpressionMatch macZfw = macZfwRe.match(text);
    if (macZfw.hasMatch()) {
        m_macZeroFuel = macZfw.captured(1).toFloat();
    }
    static const QRegularExpression macTowRe(R"(\bMACTOW\s+([0-9]+\.[0-9]))");
    const QRegularExpressionMatch macTow = macTowRe.match(text);
    if (macTow.hasMatch()) {
        m_macTakeoff = macTow.captured(1).toFloat();
    }

    // The route line, which is three letter airports and the registration without its
    // dash - "VLC LHR GEUPG 2/4". Anchored to a line of its own so that three capitals
    // in the free text at the bottom cannot be mistaken for it.
    static const QRegularExpression routeRe(
        R"((?:^|\n)\s*([A-Z]{3})\s+([A-Z]{3})\s+([A-Z0-9]{5,7})\b)");
    const QRegularExpressionMatch route = routeRe.match(text);
    if (route.hasMatch())
    {
        m_from = route.captured(1);
        m_to = route.captured(2);
        m_registration = route.captured(3);
    }

    return true;
}

QString AcarsLoadsheet::toString()
{
    QStringList lines;
    lines.append(QString("<b>Loadsheet:</b> %1").arg(m_final ? "final" : "preliminary"));
    if (!m_from.isEmpty() && !m_to.isEmpty()) {
        lines.append(QString("<b>Route:</b> %1 to %2").arg(m_from).arg(m_to));
    }
    if (!m_registration.isEmpty()) {
        lines.append(QString("<b>Registration:</b> %1").arg(m_registration));
    }

    // Each with its limit where the sheet gave one, since the margin is the point of it
    auto weightLine = [&lines](const char *what, int value, int maximum)
    {
        if (value < 0) {
            return;
        }
        if (maximum > 0) {
            lines.append(QString("<b>%1:</b> %2 (max %3)").arg(what).arg(value).arg(maximum));
        } else {
            lines.append(QString("<b>%1:</b> %2").arg(what).arg(value));
        }
    };
    weightLine("Zero fuel weight", m_zeroFuelWeight, m_zeroFuelWeightMax);
    weightLine("Take off weight", m_takeoffWeight, m_takeoffWeightMax);
    weightLine("Landing weight", m_landingWeight, m_landingWeightMax);
    weightLine("Take off fuel", m_takeoffFuel, -1);
    weightLine("Trip fuel", m_tripFuel, -1);
    weightLine("Underload", m_underload, -1);
    if (m_passengers >= 0) {
        lines.append(QString("<b>Passengers:</b> %1").arg(m_passengers));
    }
    if (m_macZeroFuel >= 0.0f) {
        lines.append(QString("<b>MAC zero fuel:</b> %1%").arg(m_macZeroFuel, 0, 'f', 1));
    }
    if (m_macTakeoff >= 0.0f) {
        lines.append(QString("<b>MAC take off:</b> %1%").arg(m_macTakeoff, 0, 'f', 1));
    }

    // The sheet itself after the summary: it carries the crew, the baggage and the
    // remarks, none of which is worth a field of its own but all of which is worth reading
    lines.append(QString());
    lines.append(m_text);
    return lines.join("<br>");
}

AcarsMessage *AcarsMessage::decode(const QByteArray &message, const QString &label, const QString &text, bool uplink)
{
    AcarsMessage *acarsMessage;
    // Before the label dispatch: a loadsheet arrives under whatever label the airline
    // uses for it - 10 and 30 have both been seen - so its content is what identifies it
    // "173442,19231,2010, 23200,N 51.892,..." - see decodeCompanyPosition()
    static const QRegularExpression poxOrCompanyPosition(
        R"RX(^\d{6},\s*\d+,\d{4},[- \d]+,[NS][ \d.]+,[EW][ \d.]+,)RX");
    if (text.contains("LOADSHEET")) {
        acarsMessage = new AcarsLoadsheet(message);
    } else if (AcarsDepartureClearanceResponse::isPlainLanguage(text)) {
        // Chosen on the text, because the plain language clearance turns up under at
        // least five labels - RA, C1, 21, 4P and 3T were all seen in one week of real
        // traffic - and under none of the two the coded dialects use.
        acarsMessage = new AcarsDepartureClearanceResponse(message);
    } else if (label == "H2") {
        acarsMessage = new AcarsMeteorologicalReport(message);
    } else if (label == "SA") {
        acarsMessage = new AcarsMediaAdvisory(message);
    } else if (label == ":;") {
        acarsMessage = new AcarsDataTransceiverAutoTune(message);
    } else if (label == "52") {
        acarsMessage = new AcarsGroundUTCRequest(message);
    } else if (label == "5U") {
        acarsMessage = new AcarsWeatherRequest(message);
    } else if ((label == "10") || (label == "12") || (label == "22") || (label == "24") || (label == "44")
               // Airline formats that carry a position under a label registered as
               // something else entirely - see decodeAirlinePosition(). Each of these
               // labels carries other traffic too, and a text that is not one of the
               // formats simply falls through to being shown as it came.
               || (label == "11") || (label == "21") || (label == "36") || (label == "45")
               || (label == "8H") || (label == "4N") || (label == "32")
               || (label == "83") || (label == "30")) {
        acarsMessage = new AcarsPositionReport(message);
    } else if (label == "80") {
        acarsMessage = new AcarsOpsReport(message);
    } else if (((label == "17") || (label == "49")) && text.contains("01CLER")) {
        // The cruise report, which names both airports - see decodeCruiseReport(). Label
        // 49 is not otherwise decoded at all, and 17's other traffic is handled below.
        acarsMessage = new AcarsPositionReport(message);
    } else if ((label == "17") && poxOrCompanyPosition.match(text).hasMatch()) {
        // Label 17 carries a company position report as well as event reports, and is
        // chosen on the text for the same reason label 18 is just below
        acarsMessage = new AcarsPositionReport(message);
    } else if ((label == "18") && text.startsWith("POX,")) {
        // Label 18 carries an airline position report as well as event reports, so it is
        // chosen on the text - as labels 15 and 16 already are. Filed as an event it was
        // titled "OOOI" in the documents while carrying no OOOI time at all, and its
        // position, route and level were all discarded.
        acarsMessage = new AcarsPositionReport(message);
    } else if ((label == "8E") || (label == "8S")) {
        // The destination and ETA form - see AcarsEventReport::decode()
        acarsMessage = new AcarsEventReport(message);
    } else if ((label == "13") || (label == "14") || (label == "17") || (label == "18")) {
        acarsMessage = new AcarsEventReport(message);
    } else if (label == "15") {
        // Label 15 carries both FST01 flight status and /15 ON EVENT landing reports
        if (text.startsWith("/15")) {
            acarsMessage = new AcarsEventReport(message);
        } else {
            acarsMessage = new AcarsFlightStatus(message);
        }
    } else if (label == "16") {
        // Label 16 carries both N space position reports and /16 IN EVENT gate reports
        if (text.startsWith("/16")) {
            acarsMessage = new AcarsEventReport(message);
        } else {
            acarsMessage = new AcarsPositionReport(message);
        }
    } else if (label == "A0") {
        acarsMessage = new AcarsATSFacilitiesNotification(message);
    } else if (label == "A1") {
        acarsMessage = new AcarsOceanicClearanceResponse(message);
    } else if ((label == "A3") || (label == "B4")) {
        // A3 delivers the clearance, B4 reads it back, and both carry the same text
        acarsMessage = new AcarsDepartureClearanceResponse(message);
    } else if (label == "A4") {
        acarsMessage = new AcarsFlightSystemsMessage(message);
    } else if (label == "A9") {
        acarsMessage = new AcarsATISReport(message);
    } else if (label == "B0") {
        acarsMessage = new AcarsATSFacilitiesNotification(message);
    } else if (label == "B1") {
        acarsMessage = new AcarsRequestOceanicClearance(message);
    } else if (label == "B2") {
        acarsMessage = new AcarsOceanicClearanceReadback(message);
    } else if (label == "B9") {
        acarsMessage = new AcarsRequestATISReport(message);
    } else if (label == "AA") {
        acarsMessage = new AcarsATCCommunications(message);
    } else if (label == "BA") {
        acarsMessage = new AcarsATCCommunications(message);
    } else if (label == "H1") {
        acarsMessage = new AcarsMessageFromSubsystem(message);
    } else if (label == "QP") {
        acarsMessage = new AcarsOutReport(message);
    } else if (label == "QQ") {
        acarsMessage = new AcarsOffReport(message);
    } else if (label == "QR") {
        acarsMessage = new AcarsOnReport(message);
    } else if (label == "QS") {
        acarsMessage = new AcarsInReport(message);
    } else if (label == "QT") {
        acarsMessage = new AcarsOutReturnInReport(message);
    } else if ((label == "QA") || (label == "QB") || (label == "QD") || (label == "QE")
               || (label == "QF") || (label == "QG") || (label == "Q1")) {
        // The three character station form of the movement reports - see the note on
        // AcarsShortOooiReport. QP to QT above are the four character ARINC 620 form.
        acarsMessage = new AcarsShortOooiReport(message);
    } else if (label == "SQ") {
        acarsMessage = new AcarsSquitter(message);
    } else {
        acarsMessage = new AcarsMessage(message);
    }
    acarsMessage->decode(text);
    return acarsMessage;
}

// Block ID is 0-9 for downlink and A-Z for uplink
bool AcarsMessage::isUplink(QChar blockId)
{
    return !blockId.isDigit();
}

AcarsDataTransceiverAutoTune::AcarsDataTransceiverAutoTune(const QByteArray &message) :
    AcarsMessage(message)
{
}

bool AcarsDataTransceiverAutoTune::decode(const QString &text)
{
    if (text.size() == 6)
    {
        m_frequencyHz = text.toInt() * 1000;
        m_precision = 3;
        return true;
    }
    else if (text.size() == 9)
    {
        m_frequencyHz = text.toInt();
        m_precision = 6;
        return true;
    }
    else
    {
        qDebug() << "AcarsDataTransceiverAutoTune: Can't decode: " << text;
        m_frequencyHz = -1;
        m_precision = 0;
        return false;
    }
}

QString AcarsDataTransceiverAutoTune::toString()
{
    if (m_frequencyHz != -1) {
        return QString("<b>Tune to:</b> %1MHz").arg(m_frequencyHz/1000000.0, 0, 'f', m_precision);
    } else {
        return "";
    }
}

AcarsGroundUTCRequest::AcarsGroundUTCRequest(const QByteArray &message) :
    AcarsMessage(message)
{
}

bool AcarsGroundUTCRequest::decode(const QString &text)
{
    if (m_uplink)
    {
        QRegularExpression re(R"((\d\d)(\d\d)(\d\d)(\d)(\d\d)(\d\d)(\d\d))");
        QRegularExpressionMatch match = re.match(text);
        if (match.hasMatch())
        {
            m_date = QDate(match.captured(1).toInt() + 2000, match.captured(2).toInt(), match.captured(3).toInt());
            m_day = match.captured(4).toInt();
            m_time = QTime(match.captured(5).toInt(), match.captured(6).toInt(), match.captured(7).toInt());
            return true;
        }
        else
        {
            qDebug() << "AcarsGroundUTCRequest: Failed to match " << text;
            return false;
        }
    }
    else
    {
        return true;
    }
}

QString AcarsGroundUTCRequest::toString()
{
    if (m_uplink && m_date.isValid() && m_time.isValid())
    {
        const QStringList days = {"", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
        return QString("<b>UTC:</b> %1 %2 %3")
                    .arg(m_date.toString("yyyy-MM-dd"))
                    .arg(((m_day >= 0) && (m_day < days.size())) ? days[m_day] : QString::number(m_day))
                    .arg(m_time.toString());
    }
    else
    {
        return "";
    }
}

// See Table C-2 in Appendix C of ARINC 623
AcarsOceanicClearanceResponse::AcarsOceanicClearanceResponse(const QByteArray &message) :
    AcarsMessage(message)
{
}

bool AcarsOceanicClearanceResponse::decode(const QString &text)
{
    // \r\n not always positioned as spec, so just replace with spaces
    QRegularExpression re(R"(CLX (\d\d)(\d\d) (\d\d)(\d\d)(\d\d) (\w\w\w\w) CLRNCE (\d\d\d) (\w*) CLRD TO (\w\w\w\w) VIA (\w*) (\w* \w*) ([A-Za-z0-9/ ]*)FM (\w*)\/(\d\d)(\d\d) MNTN F(\d\d\d) M(\d\d\d))");
    QRegularExpressionMatch match = re.match(text.simplified());
    if (match.hasMatch())
    {
        m_messageTime = QTime(match.captured(1).toInt(), match.captured(2).toInt());
        m_messageDate = QDate(match.captured(3).toInt() + 2000, match.captured(4).toInt(), match.captured(5).toInt());
        m_atcCenter = match.captured(6);
        m_clearanceNumber = match.captured(7).toInt();
        m_aircraft = match.captured(8);
        m_destination = match.captured(9);
        m_via = match.captured(10);
        m_routeId = match.captured(11);
        m_route = match.captured(12).simplified();
        m_entryPoint = match.captured(13);
        m_entryTime = QTime(match.captured(14).toInt(), match.captured(15).toInt());
        m_flightLevel = match.captured(16).toInt();
        m_mach = match.captured(17).toInt() / 100.0f;

        // TODO: ATC RECLEARANCE

/*        if (match.lastCapturedIndex() >= 9) {
            m_freeText = match.captured(9).trimmed();
        }*/
        return true;
    }
    else
    {
        qDebug() << "AcarsOceanicClearanceReadback: Failed to match " << text;
        return false;
    }
}

QString AcarsOceanicClearanceResponse::toString()
{
    if (m_messageTime.isValid())
    {
        QString s = QString("<b>Time:</b> %1<br><b>Date:</b> %2<br><b>ATC center:</b> %3<br><b>Clearance number:</b> %4<br><b>Aircraft:</b> %5<br><b>Destination:</b> %6<br><b>Via:</b> %7<br><b>Route Id:</b> %8<br><b>Route:</b> %9<br><b>Entry point:</b> %10<br><b>Entry time:</b> %11<br><b>Flight level:</b> %12<br><b>Mach:</b> %13")
                    .arg(m_messageTime.toString("hh:mm"))
                    .arg(m_messageDate.toString())
                    .arg(icaoAndAirportName(m_atcCenter))
                    .arg(m_clearanceNumber)
                    .arg(m_aircraft)
                    .arg(icaoAndAirportName(m_destination))
                    .arg(m_via)
                    .arg(m_routeId)
                    .arg(m_route)
                    .arg(m_entryPoint)
                    .arg(m_entryTime.toString("hh:mm"))
                    .arg(m_flightLevel)
                    .arg(m_mach)
                    ;
        if (!m_freeText.isEmpty()) {
            s.append(QString("<br><b>Free text:</b> %1").arg(m_freeText));
        }
        return s;
    }
    else
    {
        return "";
    }
}

// See Table D-2 in Appendix D of ARINC 623
AcarsDepartureClearanceResponse::AcarsDepartureClearanceResponse(const QByteArray &message) :
    AcarsMessage(message)
{
}

// The heading is the only thing the plain language dialects share, and even that is
// written four ways. Static because decode() is not the only caller - the dispatch has to
// choose this class on the text, the labels being no guide.
bool AcarsDepartureClearanceResponse::isPlainLanguage(const QString &text)
{
    const QString flat = text.simplified();
    return flat.contains("PRE-DEPARTURE CLEARANCE")        // also *PRE-DEPARTURE CLEARANCE*
        || flat.contains("PRE-DEPARTURE ATC CLEARANCE")
        || flat.contains("**DEPARTURE CLEARANCE**")
        || flat.contains("NO DEPARTURE CLEARANCE");        // the refusal
}

bool AcarsDepartureClearanceResponse::decode(const QString &text)
{
    // A pre departure clearance, on label A3 where it is delivered and B4 where it is
    // read back. Two dialects share one core: North America writes CLD and a departure
    // time type, Europe writes CDA and gives an initial altitude, a TSAT, a QNH and the
    // frequency to call. So the core is matched first and each of the trailing items is
    // then looked for on its own - a single regex over the lot matched neither dialect
    // reliably, and one missing field threw the whole clearance away.
    //
    //   CLD 1442 260829 LSZH PDC 256 SWR3EC CLRD TO LICJ OFF 28 VIA VEBIT1Y
    //   ALT 5000 FT SQUAWK 1000 ATIS P AIRBORNE FREQ 125.955 TSAT 1500
    //
    // \r\n is not always where the spec says, so the text is flattened to spaces first.
    const QString flat = text.simplified();

    // The third family, and the one that says the most: rather than a coded line, North
    // American and Canadian airports send the clearance as plain language. There is no
    // single format - four dialects were seen in one week of real traffic - so each field
    // is looked for on its own and a missing one costs only itself.
    //
    //   PRE-DEPARTURE CLEARANCE                  PRE-DEPARTURE ATC CLEARANCE
    //   SKW5371 DEPART ORD 1745Z                 WJA228  DEPART YYC AT 1825Z FL 390
    //   FL 150 CRJ2/L XPNDR 7263                 M/B737/W TRANSPONDER 3330
    //   ROUTE KORD PMPKN NEATO DLLAN KGRB        ROUTE:
    //   CLEARED ORD9 DEPARTURE                   NOSIV Q909 DESNU VINBU SEGAP BEFAN5
    //                                            REMARKS:
    //   **DEPARTURE CLEARANCE**                  USE SID CWBOY2
    //   FLT 1357-30 KSEA-KSNA                    DEPARTURE RUNWAY 35R
    //   ASA1357 KSEA                             DESTINATION CYWG
    //   B738/L P1828
    //   REQUESTED FL 370                         *PRE-DEPARTURE CLEARANCE*
    //   XPDR 1525                                FLT ACA025    CYVR
    //   ***FILED ROUTE***                        H/B789/W FILED FL320
    //   KSEA SUMMA2 SUMMA Q9 REBRG OHSEA3 KSNA   XPRD 7017
    //   ***ADDL INSTRUCTIONS***                  USE SID FSR9
    //   CLEARED SUMMA2 DEPARTURE                 DEPARTURE RUNWAY 08R
    //
    // Worth having for the route alone: where it runs between two airports it can be
    // drawn, and the coded dialects give only the SID.
    if (isPlainLanguage(text))
    {
        m_text = text;
        m_plainLanguage = true;

        // A refusal is a departure clearance response too, and showing it is the point -
        // it tells the crew to call. But it names no route, so it is marked so that
        // nothing downstream tries to draw one.
        if (flat.contains("NO DEPARTURE CLEARANCE"))
        {
            m_clearanceAvailable = false;
            m_freeText = flat;
            return true;
        }

        // "SKW5371 DEPART ORD 1745Z" and "WJA228 DEPART YYC AT 1825Z". The airport is
        // three letter IATA as often as four letter ICAO.
        static const QRegularExpression departRe(
            R"((\S+) DEPART (?:FROM )?([A-Z]{3,4})(?: AT)? (\d{4})Z)");
        const QRegularExpressionMatch depart = departRe.match(flat);
        if (depart.hasMatch())
        {
            m_flight = depart.captured(1);
            m_departureAirport = depart.captured(2);
            m_departureTime = depart.captured(3);
            m_departureTimeType = "DEPART";
        }

        // "FLT 1357-30 KSEA-KSNA" names both ends outright
        static const QRegularExpression sectorRe(
            R"(\bFLT\s+\S+\s+([A-Z]{4})-([A-Z]{4})\b)");
        const QRegularExpressionMatch sector = sectorRe.match(flat);
        if (sector.hasMatch())
        {
            m_departureAirport = sector.captured(1);
            m_destination = sector.captured(2);
        }
        if (m_flight.isEmpty())
        {
            // "FLT ACA025 CYVR", then the Alaska form, where the callsign simply
            // precedes the departure airport on a line of its own
            static const QRegularExpression fltRe(R"(\bFLT\s+([A-Z]{2,3}\d{1,4})\b)");
            static const QRegularExpression callsignRe(R"(\b([A-Z]{3}\d{1,4})\s+[A-Z]{4}\b)");
            const QRegularExpressionMatch flt = fltRe.match(flat);
            const QRegularExpressionMatch callsign = callsignRe.match(flat);
            if (flt.hasMatch()) {
                m_flight = flt.captured(1);
            } else if (callsign.hasMatch()) {
                m_flight = callsign.captured(1);
            }
        }
        if (m_departureAirport.isEmpty())
        {
            static const QRegularExpression fltFromRe(R"(\bFLT\s+\S+\s+([A-Z]{4})\b)");
            const QRegularExpressionMatch fltFrom = fltFromRe.match(flat);
            if (fltFrom.hasMatch()) {
                m_departureAirport = fltFrom.captured(1);
            }
        }

        // "FL 150", "FILED FL320" and "REQUESTED FL 370" - the space is optional
        static const QRegularExpression levelRe(R"(\bFL\s*(\d{2,3})\b)");
        const QRegularExpressionMatch level = levelRe.match(flat);
        if (level.hasMatch()) {
            m_initialAltitudeFt = level.captured(1).toInt() * 100;
        }

        // The type and its equipment suffix: CRJ2/L, B738/L, and with a weight category
        // in front of it M/B737/W and H/B789/W
        static const QRegularExpression typeRe(
            R"(\b((?:[A-Z]/)?[A-Z][A-Z0-9]{2,4}/[A-Z])\b)");
        const QRegularExpressionMatch type = typeRe.match(flat);
        if (type.hasMatch()) {
            m_aircraftType = type.captured(1);
        }

        // Four spellings of the same field, all seen in one week
        static const QRegularExpression squawkRe(
            R"(\b(?:XPNDR|XPDR|XPRD|TRANSPONDER|SQUAWK|BCN)\s+(\d{4})\b)");
        const QRegularExpressionMatch squawk = squawkRe.match(flat);
        if (squawk.hasMatch()) {
            m_squawk = squawk.captured(1);
        }

        // The route runs from the ROUTE heading to whatever announces the next section.
        // It wraps across lines, which simplified() has already turned into single
        // spaces. @PBN is not a section heading but an equipment note appended to the
        // route itself, and must not be drawn.
        static const QRegularExpression routeRe(
            R"(\bROUTE\**:?\s+(.*?)\s*(?:\*\*\*|CLEARED\b|CLIMB\b|MAINTAIN\b|EXPECT\b|REMARKS\b|USE SID\b|DEPARTURE RUNWAY\b|DESTINATION\b|DPFRQ\b|CTC\b|@PBN|\*END-PDC|$))");
        const QRegularExpressionMatch route = routeRe.match(flat);
        if (route.hasMatch())
        {
            m_route = route.captured(1).simplified();

            // An amended clearance writes the changed segment in dashes and then the
            // whole route after it:
            //
            //   ROUTE -MOBLE ADIME SEWTO KFWA-
            //   KORD MOBLE ADIME SEWTO FWA KFWA
            //
            // The segment is a subset of what follows, so keeping both would draw those
            // fixes twice and hide the airports at the ends. 14 of 256 route-carrying
            // clearances in a week of real traffic are written this way, on two
            // different labels.
            static const QRegularExpression amendedRe(R"(^-[^-]*-\s*(\S.*)$)");
            const QRegularExpressionMatch amended = amendedRe.match(m_route);
            if (amended.hasMatch()) {
                m_route = amended.captured(1);
            }

            // Where both ends are airports, that is what it is flown between
            static const QRegularExpression endsRe(R"(^([A-Z]{4})\b.*\b([A-Z]{4})$)");
            const QRegularExpressionMatch ends = endsRe.match(m_route);
            if (ends.hasMatch())
            {
                m_departureAirport = ends.captured(1);
                m_destination = ends.captured(2);
            }
        }

        // "CLEARED ORD9 DEPARTURE" and "USE SID FSR9"
        static const QRegularExpression clearedSidRe(
            R"(\bCLEARED\s+(\S+)\s+DEPARTURE\b)");
        static const QRegularExpression useSidRe(R"(\bUSE SID\s+(\S+))");
        const QRegularExpressionMatch clearedSid = clearedSidRe.match(flat);
        const QRegularExpressionMatch useSid = useSidRe.match(flat);
        if (clearedSid.hasMatch()) {
            m_sid = clearedSid.captured(1);
        } else if (useSid.hasMatch()) {
            m_sid = useSid.captured(1);
        }

        static const QRegularExpression runwayRe(R"(\bDEPARTURE RUNWAY\s+(\S+))");
        const QRegularExpressionMatch runway = runwayRe.match(flat);
        if (runway.hasMatch()) {
            m_runway = stripRunwayFiller(runway.captured(1));
        }

        // Stated outright, so it beats whatever the ends of the route suggested - the
        // dialect that writes this one is also the one whose route is a list of fixes
        // with no airport at either end.
        static const QRegularExpression destRe(R"(\bDESTINATION\s+([A-Z]{4})\b)");
        const QRegularExpressionMatch dest = destRe.match(flat);
        if (dest.hasMatch()) {
            m_destination = dest.captured(1);
        }

        // Whichever end was written in IATA becomes ICAO, so that one clearance cannot
        // hand the map a three letter code and the next a four letter one
        const QString depIcao = icaoFromIata(m_departureAirport);
        if (!depIcao.isEmpty()) {
            m_departureAirport = depIcao;
        }
        const QString destIcao = icaoFromIata(m_destination);
        if (!destIcao.isEmpty()) {
            m_destination = destIcao;
        }

        return true;
    }

    static const QRegularExpression coreRe(
        R"((?:CLD|CDA) (\d\d)(\d\d) (\d\d)(\d\d)(\d\d) (\w{4}) PDC (\d+) (\S+) CLRD TO (\w{4}) OFF (\S+) VIA (\S+))");
    const QRegularExpressionMatch match = coreRe.match(flat);
    if (!match.hasMatch())
    {
        // These labels carry other traffic, so not matching is ordinary. The caller
        // shows the text as it came.
        return false;
    }

    m_messageTime = QTime(match.captured(1).toInt(), match.captured(2).toInt());
    m_messageDate = QDate(match.captured(3).toInt() + 2000, match.captured(4).toInt(), match.captured(5).toInt());
    m_departureAirport = match.captured(6);
    m_clearanceNumber = match.captured(7).toInt();
    m_flight = match.captured(8);
    m_destination = match.captured(9);
    m_runway = match.captured(10);
    m_sid = match.captured(11);

    // Everything the core did not take. Each item is removed from the tail as it is
    // recognised, so that what is left over really is free text - taking the tail whole
    // showed the altitude, squawk and frequency twice, once parsed and once raw.
    QString tail = flat.mid(match.capturedEnd(0));
    auto scavenge = [&tail](const char *pattern, QString& into)
    {
        const QRegularExpression re(pattern);
        const QRegularExpressionMatch m = re.match(tail);
        if (m.hasMatch())
        {
            into = m.captured(1);
            tail.remove(m.capturedStart(0), m.capturedLength(0));
        }
    };
    scavenge(R"(SQUAWK (\d{4}))", m_squawk);
    scavenge(R"(ATIS (\w)\b)", m_atis);
    scavenge(R"(TSAT (\d{4}))", m_tsat);
    scavenge(R"(QNH (\d{3,4}))", m_qnh);
    scavenge(R"((?:AIRBORNE|NEXT|CONTACT) FREQ (\d+\.\d+))", m_frequency);

    QString altitude;
    // "ALT 5000 FT" and "CLIMB TO 4000 FT" are the same thing said two ways
    scavenge(R"((?:ALT|CLIMB TO) (\d+) ?FT)", altitude);
    if (!altitude.isEmpty()) {
        m_initialAltitudeFt = altitude.toInt();
    }

    // The departure time comes with its type, so the two are taken together
    {
        static const QRegularExpression re(R"(\b(ADT|MDI) (\S+))");
        const QRegularExpressionMatch m = re.match(tail);
        if (m.hasMatch())
        {
            m_departureTimeType = m.captured(1);
            m_departureTime = m.captured(2);
            tail.remove(m.capturedStart(0), m.capturedLength(0));
        }
    }

    // What an airport adds in its own words - "WITHIN TOBT +-5MIN", "REPORT READY"
    m_freeText = tail.simplified();
    return true;
}

QString AcarsDepartureClearanceResponse::toString()
{
    if (!m_messageTime.isValid() && !m_plainLanguage) {
        return "";
    }

    // Only what the clearance actually said. Printed unconditionally, the fields the
    // other dialect does not use came out as empty headings - "Departure time:" with
    // nothing after it on every European clearance. The plain language dialects carry no
    // message time or clearance number at all, so those two go the same way; keying the
    // whole function on the time is what left them printing nothing whatsoever.
    QStringList lines;
    if (m_messageTime.isValid())
    {
        lines.append(QString("<b>Time:</b> %1").arg(m_messageTime.toString("hh:mm")));
        lines.append(QString("<b>Date:</b> %1").arg(m_messageDate.toString()));
    }
    if (!m_clearanceAvailable) {
        lines.append("<b>Clearance:</b> none on file, contact clearance delivery by voice");
    }
    if (!m_departureAirport.isEmpty()) {
        lines.append(QString("<b>Departure airport:</b> %1").arg(icaoAndAirportName(m_departureAirport)));
    }
    if (m_clearanceNumber > 0) {
        lines.append(QString("<b>Clearance number:</b> %1").arg(m_clearanceNumber));
    }
    if (!m_flight.isEmpty()) {
        lines.append(QString("<b>Flight:</b> %1").arg(m_flight));
    }
    if (!m_destination.isEmpty()) {
        lines.append(QString("<b>Destination:</b> %1").arg(icaoAndAirportName(m_destination)));
    }
    if (!m_aircraftType.isEmpty()) {
        lines.append(QString("<b>Aircraft type:</b> %1").arg(m_aircraftType));
    }
    if (!m_runway.isEmpty()) {
        lines.append(QString("<b>Runway:</b> %1").arg(m_runway));
    }
    if (!m_sid.isEmpty()) {
        lines.append(QString("<b>SID:</b> %1").arg(m_sid));
    }
    if (!m_route.isEmpty()) {
        lines.append(QString("<b>Route:</b> %1").arg(m_route));
    }
    if (m_initialAltitudeFt >= 0) {
        lines.append(QString("<b>Initial altitude:</b> %1 ft").arg(m_initialAltitudeFt));
    }
    if (!m_squawk.isEmpty()) {
        lines.append(QString("<b>Squawk:</b> %1").arg(m_squawk));
    }
    if (!m_departureTime.isEmpty()) {
        lines.append(QString("<b>Departure time:</b> %1 %2").arg(m_departureTimeType).arg(m_departureTime));
    }
    if (!m_tsat.isEmpty()) {
        lines.append(QString("<b>TSAT:</b> %1").arg(m_tsat));
    }
    if (!m_atis.isEmpty()) {
        lines.append(QString("<b>ATIS:</b> %1").arg(m_atis));
    }
    if (!m_qnh.isEmpty()) {
        lines.append(QString("<b>QNH:</b> %1").arg(m_qnh));
    }
    if (!m_frequency.isEmpty()) {
        lines.append(QString("<b>Frequency:</b> %1 MHz").arg(m_frequency));
    }
    if (!m_freeText.isEmpty()) {
        lines.append(QString("<b>Free text:</b> %1").arg(m_freeText));
    }
    return lines.join("<br>");
}

// See Attachment 9 in ARINC 623
AcarsFlightSystemsMessage::AcarsFlightSystemsMessage(const QByteArray &message) :
    AcarsMessage(message)
{
}

bool AcarsFlightSystemsMessage::decode(const QString &text)
{
    QRegularExpression re(R"(FSM (\d\d)(\d\d) (\d\d)(\d\d)(\d\d) (\w\w\w\w) (\w*) ([\w\s]*)?)");
    QRegularExpressionMatch match = re.match(text.simplified());
    if (match.hasMatch())
    {
        m_messageTime = QTime(match.captured(1).toInt(), match.captured(2).toInt());
        m_messageDate = QDate(match.captured(3).toInt() + 2000, match.captured(4).toInt(), match.captured(5).toInt());
        m_departureAirport = match.captured(6);
        m_flight = match.captured(7);
        if (match.lastCapturedIndex() >= 8) {
            m_freeText = match.captured(8).trimmed(); // Sometimes last 4 digits are CRC, but not always
        }
        return true;
    }
    else
    {
        qDebug() << "AcarsFlightSystemsMessage: Failed to match " << text;
        return false;
    }
}

QString AcarsFlightSystemsMessage::toString()
{
    QString s;
    if (m_messageTime.isValid())
    {
        s.append(QString("<b>Time %1<br><b>Date:</b> %2<br><b>Departure airport:</b> %3<br><b>Flight:</b> %4")
                    .arg(m_messageTime.toString("hh:mm"))
                    .arg(m_messageDate.toString())
                    .arg(icaoAndAirportName(m_departureAirport))
                    .arg(m_flight));
        if (!m_freeText.isEmpty()) {
            s.append(QString("<br><b>Free text:</b> %1").arg(m_freeText));
        }
    }
    return s;
}

// Appendix B in ARINC 623
AcarsATISReport::AcarsATISReport(const QByteArray &message) :
    AcarsMessage(message)
{
}

bool AcarsATISReport::decode(const QString &text)
{
    QRegularExpression re(R"(TI2\/(\w\w\w\w)\s+(\w\w\w)\s+ATIS\s+(\w)\s+(\d\d)(\d\d)Z\s+([\s\S]*))");
    QRegularExpressionMatch match = re.match(text);
    if (match.hasMatch())
    {
        m_airport = match.captured(1);
        m_indicator = match.captured(2);
        m_atisId = match.captured(3);
        m_time = QTime(match.captured(4).toInt(), match.captured(5).toInt());
        m_atisInformation = match.captured(6);
        return true;
    }
    else
    {
        qDebug() << "AcarsATISReport: Failed to match " << text;
        return false;
    }
}

QString AcarsATISReport::toString()
{
    if (!m_airport.isEmpty())
    {
        // These are the only 3 in ARINC 623, but EGKK uses COM
        const QHash<QString, QString> indicators = {
            {"ARR", "Arrival"},
            {"DEP", "Departure"},
            {"ENR", "Enroute/VOLMET"}
        };
        QString indicator = m_indicator;
        if (indicators.contains(indicator)) {
            indicator = indicators[indicator];
        }
        QString s = QString("<b>Airport:</b> %1<br><b>Arrival/departure indicator:</b> %2<br><b>ATIS Id:</b> %3<br><b>Time:</b> %4")
                            .arg(icaoAndAirportName(m_airport))
                            .arg(indicator)
                            .arg(m_atisId)
                            .arg(m_time.toString("hh:mm"));
        s.append("\n");
        s.append(m_atisInformation);
        if (m_multiBlock)
        {
            s.append("");
        }
        return s;
    }
    else
    {
        return "";
    }
}

// See section 3 in ARINC 622
AcarsATSFacilitiesNotification::AcarsATSFacilitiesNotification(const QByteArray &message) :
    AcarsMessage(message)
{
}

bool AcarsATSFacilitiesNotification::decode(const QString &text)
{
    QRegularExpression re(R"(\/(\w*)\.AFN\/FMH(\w*),([\.\-\w]*),(\w*)(,(\d\d)(\d\d)(\d\d))?)");
    QRegularExpressionMatch match = re.match(text);
    if (match.hasMatch())
    {
        m_atcCenter = match.captured(1);
        m_flight = match.captured(2);
        m_aircraftRegistration = match.captured(3);
        while (m_aircraftRegistration.startsWith(".")) {
            m_aircraftRegistration = m_aircraftRegistration.mid(1); // Remove prefix
        }
        m_aircraftICAO = match.captured(4);         // 24-bit ICAO hex id
        if (match.lastCapturedIndex() >= 7) {
            m_time = QTime(match.captured(5).toInt(), match.captured(6).toInt(), match.captured(7).toInt());
        }

        int offset = match.capturedStart(0) + match.captured(0).length() + 1; // Skip matched string and /
        m_mti = text.mid(offset, 3);
        return true;
    }
    else
    {
        qDebug() << "AcarsATSFacilitiesNotification: Failed to match " << text;
        return false;
    }
}

QString AcarsATSFacilitiesNotification::toString()
{
    QString s = QString("<b>ATC center:</b> %1<br><b>Flight:</b> %2<br><b>Aircraft:</b> %3")
                    .arg(icaoAndAirportName(m_atcCenter))
                    .arg(m_flight)
                    .arg(m_aircraftRegistration);
    if (!m_aircraftICAO.isEmpty()) {
        s.append(QString("<br><b>Aircraft ICAO:</b> %1").arg(m_aircraftICAO));
    }
    if (m_time.isValid()) {
        s.append(QString("<br><b>Time:</b> %1").arg(m_time.toString("hh:mm:ss")));
    }
    if (!m_mti.isEmpty())
    {
        const QHash<QString, QString> mtis = {
            {"FPO", "AFN contact"},
            {"FAK", "AFN acknowledge"},
            {"FCA", "AFN contact advisory"},
            {"FRP", "AFN response"},
            {"FCP", "AFN complete"},
        };
        s.append(QString("<br><b>MTI:</b> %1 (%2)").arg(m_mti).arg(mtis[m_mti]));
    }
    return s;
}

AcarsRequestOceanicClearance::AcarsRequestOceanicClearance(const QByteArray &message) :
    AcarsMessage(message)
{
}

bool AcarsRequestOceanicClearance::decode(const QString &text)
{
    // Mostly start OC1/RCL, but occasionally just RCL
    QRegularExpression re(R"(RCL (\d\d\d)\ ([\w ]*)-([\w ]*)\/(\d\d)(\d\d) M(\d\d\d)F(\d\d\d)( -RMK\/(.*))?)");
    QRegularExpressionMatch match = re.match(text.simplified());
    if (match.hasMatch())
    {
        m_avionicsIndicator = match.captured(1);
        m_aircraft = match.captured(2);
        m_entryPoint = match.captured(3);
        m_entryTime = QTime(match.captured(4).toInt(), match.captured(5).toInt());
        m_mach =  match.captured(6).toInt() / 100.0f;
        m_flightLevel = match.captured(7).toInt();
        if (match.lastCapturedIndex() >= 9) {
            m_freeText = match.captured(9).trimmed();
        }
        return true;
    }
    else
    {
        qDebug() << "AcarsRequestOceanicClearance: Failed to match " << text;
        return false;
    }
}

QString AcarsRequestOceanicClearance::toString()
{
    QString s = QString("<b>Avionics rows:</b> %1<br><b>Aircraft:</b> %2<br><b>Entry point:</b> %3<br><b>Time:</b> %4<br><b>Mach:</b> %5<br><b>Flight level:</b> %6")
                    .arg(m_avionicsIndicator)
                    .arg(m_aircraft)
                    .arg(m_entryPoint)
                    .arg(m_entryTime.toString("hh:mm"))
                    .arg(m_mach, 0, 'f', 2)
                    .arg(m_flightLevel);
    if (!m_freeText.isEmpty()) {
        s.append(QString("<br><b>Free text:</b> %1").arg(m_freeText));
    }
    return s;
}

AcarsOceanicClearanceReadback::AcarsOceanicClearanceReadback(const QByteArray &message) :
    AcarsMessage(message)
{
}

bool AcarsOceanicClearanceReadback::decode(const QString &text)
{
    // \r<br><b> not always positioned as spec, so just replace with spaces
    // Mostly messages start OC1/CLA, but occasionally just CLA
    QRegularExpression re(R"(CLA (\d\d)(\d\d) (\d\d)(\d\d)(\d\d) (\w\w\w\w) CLRNCE (\d\d\d) (\w*) CLRD TO (\w\w\w\w) VIA (\w*) (\w* \w*) ([A-Za-z0-9/ ]*)FM (\w*)\/(\d\d)(\d\d) MNTN F(\d\d\d) M(\d\d\d))");
    QRegularExpressionMatch match = re.match(text.simplified());
    if (match.hasMatch())
    {
        m_messageTime = QTime(match.captured(1).toInt(), match.captured(2).toInt());
        m_messageDate = QDate(match.captured(3).toInt() + 2000, match.captured(4).toInt(), match.captured(5).toInt());
        m_atcCenter = match.captured(6);
        m_clearanceNumber = match.captured(7).toInt();
        m_aircraft = match.captured(8);
        m_destination = match.captured(9);
        m_via = match.captured(10);
        m_routeId = match.captured(11);
        m_route = match.captured(12).simplified();
        m_entryPoint = match.captured(13);
        m_entryTime = QTime(match.captured(14).toInt(), match.captured(15).toInt());
        m_flightLevel = match.captured(16).toInt();
        m_mach = match.captured(17).toInt() / 100.0f;

        // TODO: ATC RECLEARANCE

/*        if (match.lastCapturedIndex() >= 9) {
            m_freeText = match.captured(9).trimmed();
        }*/
        return true;
    }
    else
    {
        qDebug() << "AcarsOceanicClearanceReadback: Failed to match " << text;
        return false;
    }
}

QString AcarsOceanicClearanceReadback::toString()
{
    QString s = QString("<b>Time:</b> %1<br><b>Date:</b> %2<br><b>ATC center:</b> %3<br><b>Clearance number:</b> %4<br><b>Aircraft:</b> %5<br><b>Destination:</b> %6<br><b>Via:</b> %7<br><b>Route Id:</b> %8<br><b>Route:</b> %9<br><b>Entry point:</b> %10<br><b>Entry time:</b> %11<br><b>Flight level:</b> %12<br><b>Mach:</b> %13")
                    .arg(m_messageTime.toString("hh:mm"))
                    .arg(m_messageDate.toString())
                    .arg(icaoAndAirportName(m_atcCenter))
                    .arg(m_clearanceNumber)
                    .arg(m_aircraft)
                    .arg(icaoAndAirportName(m_destination))
                    .arg(m_via)
                    .arg(m_routeId)
                    .arg(m_route)
                    .arg(m_entryPoint)
                    .arg(m_entryTime.toString("hh:mm"))
                    .arg(m_flightLevel)
                    .arg(m_mach)
                    ;
    if (!m_freeText.isEmpty()) {
        s.append(QString("<br><b>Free text:</b> %1").arg(m_freeText));
    }
    return s;
}

AcarsRequestATISReport::AcarsRequestATISReport(const QByteArray &message) :
    AcarsMessage(message)
{
}

bool AcarsRequestATISReport::decode(const QString &text)
{
    int imiIdx = text.indexOf("TI2/");
    if (imiIdx != -1)
    {
        m_avionicsIndicator = text.mid(imiIdx + 4, 3);
        m_airport = text.mid(imiIdx + 7, 4).trimmed();
        m_arrivalDepartureIndicator = text.mid(imiIdx + 11, 1);
        return true;
    }
    else if (!text.contains('/'))
    {
        m_avionicsIndicator = text.mid(0, 3);
        m_airport = text.mid(3, 4).trimmed();
        m_arrivalDepartureIndicator = text.mid(7, 1);
        return true;
    }
    else
    {
        qDebug() << "AcarsRequestATISReport: Failed to match " << text;
        return false;
    }
}

QString AcarsRequestATISReport::toString()
{
    // C and T are a pair: C subscribes to arrival ATIS updates and T cancels that
    // subscription, which is why T exists at all and why it is rare.
    //
    // Measured over 982 requests off air: A 797, C 121, D 24, T 4, E 1, across 146
    // airports. 117 of those airports only ever appear with one letter, and C is heavily
    // Canadian - 26 of 26 at CYUL and 19 of 26 at CYYZ - so the automatic update service
    // is evidently offered in some places and not others.
    //
    // Note none of these says whether the airport is where the flight is GOING. Arrival
    // ATIS is requested for an alternate as readily as for a destination, which is why
    // the airport here is not taken as a destination: of 710 A requests whose flight had
    // a known route, 572 named the arrival but 118 named neither end of it.
    static const QHash<QString, QString> indicators = {
        {"A", "Arrival ATIS"},
        {"D", "Departure ATIS"},
        {"C", "Arrival with automatic update"},
        {"E", "Automatic enroute information service (AEIS) or VOLMET"},
        {"T", "Terminate automatic update of ATIS"}
    };
    // The raw letter when it is not one of those, rather than an empty field: the set is
    // not closed, and a letter says more than a blank
    const QString type = indicators.contains(m_arrivalDepartureIndicator)
        ? indicators.value(m_arrivalDepartureIndicator)
        : m_arrivalDepartureIndicator;
    return QString("<b>Avionics rows:</b> %1<br><b>Airport:</b> %2<br><b>Request type:</b> %3")
                .arg(m_avionicsIndicator)
                .arg(icaoAndAirportName(m_airport))
                .arg(type);
}

AcarsWeatherRequest::AcarsWeatherRequest(const QByteArray &message) :
    AcarsMessage(message)
{
}

bool AcarsWeatherRequest::decode(const QString &text)
{
    if (text.contains("WXRQ"))
    {
        // E.g: 01 WXRQ   47EZ/20 LGRP/EGGP .G-JZDE /TYP 1/STA EGGP/STA EGCC/STA EGNX
        QRegularExpression staRe(R"(\/STA\s+([A-Z0-9]{4}))");
        QRegularExpressionMatchIterator it = staRe.globalMatch(text);
        while (it.hasNext()) {
            m_airports.append(it.next().captured(1));
        }
        QRegularExpression typRe(R"(\/TYP\s+(\w+))");
        QRegularExpressionMatch typMatch = typRe.match(text);
        if (typMatch.hasMatch()) {
            m_reportType = typMatch.captured(1);
        }
        // Flight route and registration: LGRP/EGGP .G-JZDE
        QRegularExpression routeRe(R"(([A-Z]{4})\/([A-Z]{4})\s+\.([A-Z0-9\-]+))");
        QRegularExpressionMatch routeMatch = routeRe.match(text);
        if (routeMatch.hasMatch())
        {
            m_origin = routeMatch.captured(1);
            m_destination = routeMatch.captured(2);
            m_registration = routeMatch.captured(3);
        }
        if (!m_airports.isEmpty()) {
            return true;
        }
    }
    else
    {
        // A list of airports and the MET report code wanted, which airlines write both
        // ways round and separate with either spaces or commas:
        //
        //   LFLL LFPG LFSG EGKK-SA      the code last, after a dash
        //   EGNM EGCC EGNT -SA          the same, with the columns padded out
        //   SA EGPH,EGPF,EGPK,EGCC      the code first
        //   SA/EGCC EGNX EGBB EGGP      the code first, after a slash
        //   SAEIDW,EINN,EGAA,           the code first, run straight into the list
        //    BHX  LPL  NCL-SA           three letter IATA, right aligned in four columns
        //
        // The trailing form is tried first because it is unambiguous. Leading it is not:
        // the first two letters of EGKK are as good a match for a report code as SA is,
        // so a leading code is only accepted when it is one that exists - which is what
        // reportTypeNames() is for.
        const QString simplified = text.simplified();
        QString list;

        static const QRegularExpression trailingRe(R"(^([A-Z0-9 ,]+?)\s*-\s*([A-Z]{2})$)");
        const QRegularExpressionMatch trailing = trailingRe.match(simplified);
        if (trailing.hasMatch())
        {
            list = trailing.captured(1);
            m_reportType = trailing.captured(2);
        }
        else
        {
            static const QRegularExpression leadingRe(R"(^([A-Z]{2})[ /]?([A-Z0-9][A-Z0-9 ,]*)$)");
            const QRegularExpressionMatch leading = leadingRe.match(simplified);
            if (leading.hasMatch() && reportTypeNames().contains(leading.captured(1)))
            {
                m_reportType = leading.captured(1);
                list = leading.captured(2);
            }
        }

        if (!list.isEmpty())
        {
            // Airports are four letter ICAO or three letter IATA, and a message may
            // carry both. IATA is translated here so that everything downstream - the
            // name in the decode, the Weather tab - only ever sees ICAO.
            static const QRegularExpression separatorRe(R"([ ,]+)");
            const QStringList fields = list.split(separatorRe, Qt::SkipEmptyParts);
            for (const QString &field : fields)
            {
                if (field.size() == 4)
                {
                    m_airports.append(field);
                }
                else if (field.size() == 3)
                {
                    // An IATA code that is not in the database is kept as it stands
                    // rather than dropped - it still says which airport was asked about
                    const QString icao = icaoFromIata(field);
                    m_airports.append(icao.isEmpty() ? field : icao);
                }
                // Anything else is a corrupted field. One airport arriving mangled is
                // not a reason to lose the rest of the request, so it is skipped
            }
            if (!m_airports.isEmpty()) {
                return true;
            }
        }
    }
    qDebug() << "AcarsWeatherRequest: Failed to match " << text;
    return false;
}

// WMO MET report type codes as used in ARINC 620 weather requests. Shared with decode(),
// which needs the set of them to tell a leading report code from an airport.
const QHash<QString, QString>& AcarsWeatherRequest::reportTypeNames()
{
    static const QHash<QString, QString> reportTypes = {
        {"SA", "METAR"},
        {"SP", "SPECI"},
        {"FT", "TAF"},
        {"FC", "Short TAF"},
        {"FA", "Area forecast"},
        {"WS", "SIGMET"},
        {"NO", "NOTAM"}
    };
    return reportTypes;
}

QString AcarsWeatherRequest::toString()
{
    if (m_airports.isEmpty()) {
        return "";
    }
    const QHash<QString, QString>& reportTypes = reportTypeNames();
    QStringList lines;
    if (!m_registration.isEmpty()) {
        lines.append(QString("<b>Aircraft:</b> %1").arg(m_registration));
    }
    if (!m_origin.isEmpty()) {
        lines.append(QString("<b>Origin:</b> %1").arg(icaoAndAirportName(m_origin)));
    }
    if (!m_destination.isEmpty()) {
        lines.append(QString("<b>Destination:</b> %1").arg(icaoAndAirportName(m_destination)));
    }
    if (!m_reportType.isEmpty())
    {
        QString type = m_reportType;
        if (reportTypes.contains(type)) {
            type = QString("%1 (%2)").arg(reportTypes[type]).arg(m_reportType);
        }
        lines.append(QString("<b>Report type:</b> %1").arg(type));
    }
    for (const QString &airport : m_airports) {
        lines.append(QString("<b>Airport:</b> %1").arg(icaoAndAirportName(airport)));
    }
    return lines.join("<br>");
}

AcarsFlightStatus::AcarsFlightStatus(const QByteArray &message) :
    AcarsMessage(message)
{
}

// Flight status report, as used by British Airways on label 15. The format is not
// publicly documented; the field identification here is inferred from off-air
// messages and consistent with the examples at
// https://github.com/airframesio/acars-message-documentation/blob/main/research/15.md
//
// After FST01 come the departure and arrival airports and the position, then a
// fixed-width block - altitude in flight levels, fuel on board, fuel used and static
// air temperature - and finally wind, heading, track and ground speed.
//
// The position is NOT a fixed width. Latitude carries three or four decimal places
// (five or six digits) and longitude three or four (six or seven), and which is used
// varies by aircraft rather than by value, so trailing digits alone cannot say where
// the longitude ends. What pins it is the temperature: it always sits eleven
// characters past the position and is five characters ending in 'C', or five spaces
// when not reported. Only one longitude width puts it there. Across 54 off-air
// messages spanning three field layouts that resolved every one unambiguously.
//
// Assuming seven digits, as this did until 2026-08-26, shifts every field after the
// position one place left for the aircraft that send six. That read altitudes of
// 68000 and 77000 ft, fuel of 62.7 t on an E190 that cannot carry 13, and dropped the
// sign from the temperature - all from messages whose positions still looked right,
// which is why it went unnoticed. E.g. G-LCYJ out of London City:
//   FST01EGLCLEIB N51057 E000313 168 0627 0081 M010C 023 223 203 201 352  1244 1037
//                                   FL168 6270kg 810kg -10C  23kt/223  hdg/trk  352kt
//
// The two layouts differ in more than the position width. The compact one zero-pads
// its fields, signs the temperature adjacently, and counts fuel in 10 kg; the padded
// one right-aligns in spaces and counts fuel in 100 kg. E.g:
//   FST01EHAMEGLLN51659E0001153 92  30  19     23246      230  13431335
//   FST01KLAXEGLLN513366W0014766140 1691219 M10C025091098097344  14031345
// the second being a 747 from Los Angeles reporting 121900 kg burnt, which only makes
// sense at 100 kg a count.
bool AcarsFlightStatus::decode(const QString &text)
{
    // Departure and arrival airports, then latitude and longitude as degrees with a
    // variable number of decimal places
    static const QRegularExpression re(
        R"(^FST01([A-Z0-9]{4})([A-Z0-9]{4})([NS])(\d{4,6})([EW])(\d{6,}))");
    QRegularExpressionMatch match = re.match(text);
    if (!match.hasMatch())
    {
        qDebug() << "AcarsFlightStatus: Failed to match " << text;
        return false;
    }
    m_departureAirport = match.captured(1);
    m_arrivalAirport = match.captured(2);

    // Everything from the hemisphere character on, which is where the offsets below
    // are measured from
    const QString body = text.mid(13);
    const int latDigits = match.captured(4).size();

    // Find the longitude width by looking for the temperature where each candidate
    // would put it. Five spaces means the aircraft did not report one.
    static const QRegularExpression tempRe(R"(^(\s{5}|\s*[MP]?\s*(\d{1,3})C)$)");
    int lonDigits = 0;
    bool ambiguous = false;
    for (int candidate = 6; candidate <= 7; candidate++)
    {
        const int width = 2 + latDigits + candidate;
        if (body.size() < width + 16) {
            continue;
        }
        if (tempRe.match(body.mid(width + 11, 5)).hasMatch())
        {
            ambiguous = lonDigits != 0;
            lonDigits = candidate;
        }
    }

    // The position itself is usable either way - the two candidates differ by metres -
    // so report it even when the layout is unrecognised, but leave the fields whose
    // meaning depends on the width unset rather than guess at them
    const bool widthKnown = (lonDigits != 0) && !ambiguous;
    if (!widthKnown)
    {
        qDebug() << "AcarsFlightStatus: Could not locate the field boundary in " << text;
        lonDigits = 7;
    }

    QString latDigitStr = match.captured(4);
    m_latitude = (latDigitStr.left(2) + "." + latDigitStr.mid(2)).toFloat();
    if (match.captured(3) == "S") {
        m_latitude = -m_latitude;
    }
    QString lonDigitStr = match.captured(6).left(lonDigits);
    m_longitude = (lonDigitStr.left(3) + "." + lonDigitStr.mid(3)).toFloat();
    if (match.captured(5) == "W") {
        m_longitude = -m_longitude;
    }
    m_positionValid = true;
    if (!widthKnown) {
        return true;
    }

    // Fixed width fields: altitude in flight levels, then fuel on board and fuel used
    const bool compact = (lonDigits == 6);
    const int fuelUnitKg = compact ? 10 : 100;
    QString rest = body.mid(2 + latDigits + lonDigits);
    auto takeField = [&rest](int width)
    {
        QString field = rest.left(width).trimmed();
        rest = rest.mid(width);
        return field;
    };
    bool ok;
    int value = takeField(3).toInt(&ok);
    // Guard against a layout we have not seen putting something else here. No airliner
    // sending these reports cruises above FL430
    if (ok && (value * 100 <= ACARS_MAX_REPORTED_ALTITUDE_FT)) {
        m_altitudeFt = value * 100;
    }
    value = takeField(4).toInt(&ok);
    if (ok) {
        m_fuelOnBoardKg = value * fuelUnitKg;
    }
    value = takeField(4).toInt(&ok);
    if (ok) {
        m_fuelUsedKg = value * fuelUnitKg;
    }

    // Static air temperature, e.g. "M 58C" is -58C and "P018C" is +18C
    QString tempField = takeField(5);
    QRegularExpressionMatch tempMatch = tempRe.match(tempField);
    if (tempMatch.hasMatch() && !tempMatch.captured(2).isEmpty())
    {
        m_temperatureC = tempMatch.captured(2).toInt();
        if (tempField.contains('M')) {
            m_temperatureC = -m_temperatureC;
        }
        m_temperatureValid = true;
    }

    // The trailing eight digits are the ETA then the message time
    QString middle = rest.trimmed();
    static const QRegularExpression timesRe(R"((\d\d)(\d\d)(\d\d)(\d\d)$)");
    QRegularExpressionMatch timesMatch = timesRe.match(middle);
    if (timesMatch.hasMatch())
    {
        m_eta = QTime(timesMatch.captured(1).toInt(), timesMatch.captured(2).toInt());
        m_time = QTime(timesMatch.captured(3).toInt(), timesMatch.captured(4).toInt());
        middle = middle.left(middle.size() - 8).trimmed();
    }

    // Wind speed and direction, then heading, track and ground speed. The compact
    // layout packs all five as three digits each; the padded one gives two digits to
    // the wind speed and leaves heading and track blank
    static const QRegularExpression compactRe(R"(^(\d{3})(\d{3})(\d{3})(\d{3})(\d{3})$)");
    static const QRegularExpression paddedRe(R"(^(\d{1,2})(\d{3})\s+(\d{1,3})$)");
    QRegularExpressionMatch windMatch = compactRe.match(middle);
    if (compact && windMatch.hasMatch() && (windMatch.captured(2).toInt() <= 360))
    {
        m_windSpeedKts = windMatch.captured(1).toInt();
        m_windDirectionDeg = windMatch.captured(2).toInt();
        m_headingDeg = windMatch.captured(3).toInt();
        m_trackDeg = windMatch.captured(4).toInt();
        m_speedKts = windMatch.captured(5).toInt();
    }
    else if ((windMatch = paddedRe.match(middle)).hasMatch()
             && (windMatch.captured(2).toInt() <= 360))
    {
        m_windSpeedKts = windMatch.captured(1).toInt();
        m_windDirectionDeg = windMatch.captured(2).toInt();
        m_speedKts = windMatch.captured(3).toInt();
    }
    else if (!middle.isEmpty())
    {
        m_additionalData = middle;
    }
    return true;
}

QString AcarsFlightStatus::toString()
{
    if (m_departureAirport.isEmpty()) {
        return "";
    }
    QStringList lines;
    lines.append(QString("<b>Departure:</b> %1").arg(icaoAndAirportName(m_departureAirport)));
    lines.append(QString("<b>Arrival:</b> %1").arg(icaoAndAirportName(m_arrivalAirport)));
    lines.append(QString("<b>Position:</b> %1%2 %3%4")
                    .arg(std::abs(m_latitude), 0, 'f', 4)
                    .arg(m_latitude < 0.0f ? "S" : "N")
                    .arg(std::abs(m_longitude), 0, 'f', 4)
                    .arg(m_longitude < 0.0f ? "W" : "E"));
    if (m_altitudeFt >= 0) {
        lines.append(QString("<b>Altitude:</b> %1 ft").arg(m_altitudeFt));
    }
    if (m_fuelOnBoardKg >= 0) {
        lines.append(QString("<b>Fuel on board:</b> %1 t").arg(m_fuelOnBoardKg / 1000.0, 0, 'f', 2));
    }
    if (m_fuelUsedKg >= 0) {
        lines.append(QString("<b>Fuel used:</b> %1 t").arg(m_fuelUsedKg / 1000.0, 0, 'f', 2));
    }
    if (m_temperatureValid) {
        lines.append(QString("<b>Temperature:</b> %1%2C").arg(m_temperatureC).arg(QChar(0xB0)));
    }
    if (m_windSpeedKts >= 0) {
        lines.append(QString("<b>Wind:</b> %1%2 %3 kts").arg(m_windDirectionDeg).arg(QChar(0xB0)).arg(m_windSpeedKts));
    }
    if (m_headingDeg >= 0) {
        lines.append(QString("<b>Heading:</b> %1%2").arg(m_headingDeg).arg(QChar(0xB0)));
    }
    if (m_trackDeg >= 0) {
        lines.append(QString("<b>Track:</b> %1%2").arg(m_trackDeg).arg(QChar(0xB0)));
    }
    if (m_speedKts >= 0) {
        lines.append(QString("<b>Ground speed:</b> %1 kts").arg(m_speedKts));
    }
    if (m_eta.isValid()) {
        lines.append(QString("<b>ETA:</b> %1").arg(m_eta.toString("hh:mm")));
    }
    if (m_time.isValid()) {
        lines.append(QString("<b>Time:</b> %1").arg(m_time.toString("hh:mm")));
    }
    if (!m_additionalData.isEmpty()) {
        lines.append(QString("<b>Additional data:</b> %1").arg(m_additionalData));
    }
    return lines.join("<br>");
}

bool AcarsFlightStatus::getPosition(float& latitude, float& longitude, int& altitudeFt) const
{
    if (!m_positionValid) {
        return false;
    }
    latitude = m_latitude;
    longitude = m_longitude;
    altitudeFt = m_altitudeFt;
    return true;
}

static QString positionString(float latitude, float longitude)
{
    return QString("%1%2 %3%4")
                .arg(std::abs(latitude), 0, 'f', 4)
                .arg(latitude < 0.0f ? "S" : "N")
                .arg(std::abs(longitude), 0, 'f', 4)
                .arg(longitude < 0.0f ? "W" : "E");
}

AcarsPositionReport::AcarsPositionReport(const QByteArray &message) :
    AcarsMessage(message)
{
}

// A family of related airline position report formats, one per label. Formats
// cross-checked against airframes.io's acars-decoder-typescript test vectors.

// Latitude or longitude written as degrees and decimal minutes without a separator -
// N2725.8 is 27 degrees 25.8 minutes north, W08149.2 is 81 degrees 49.2 minutes west.
// Latitude takes two digits of degrees and longitude three, which is what tells the two
// apart when the minutes run to a leading zero.
static bool decodeDegreesMinutes(const QString& field, bool isLongitude, float& out)
{
    static const QRegularExpression re(R"(^([NSEW])\s*(\d+)\.(\d)$)");
    const QRegularExpressionMatch match = re.match(field.trimmed());
    if (!match.hasMatch()) {
        return false;
    }
    const QString digits = match.captured(2);
    const int degreeDigits = isLongitude ? 3 : 2;
    if (digits.size() != degreeDigits + 2) {
        return false;
    }
    const float degrees = digits.left(degreeDigits).toFloat();
    const float minutes = digits.mid(degreeDigits).toFloat() + match.captured(3).toFloat() / 10.0f;
    out = degrees + minutes / 60.0f;
    if ((match.captured(1) == "S") || (match.captured(1) == "W")) {
        out = -out;
    }
    return true;
}

// "N 51.410" or "-83.764" - decimal degrees, with the hemisphere either as a letter in
// front or as the sign
static bool decodeDecimalDegrees(const QString& field, float& out)
{
    static const QRegularExpression re(R"(^([NSEW])?\s*(-?\d+(?:\.\d+)?)$)");
    const QRegularExpressionMatch match = re.match(field.trimmed());
    if (!match.hasMatch()) {
        return false;
    }
    out = match.captured(2).toFloat();
    if ((match.captured(1) == "S") || (match.captured(1) == "W")) {
        out = -out;
    }
    return true;
}

// "28AUG26" - the month as the usual three letter abbreviation, 0 when it is not one
static int monthFromName(const QString& name)
{
    static const QStringList months = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                       "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    return months.indexOf(name.toUpper()) + 1;
}

static QTime decodeHms(const QString& field)
{
    const QString t = field.trimmed();
    if (t.size() == 6) {
        return QTime(t.mid(0, 2).toInt(), t.mid(2, 2).toInt(), t.mid(4, 2).toInt());
    }
    if (t.size() == 4) {
        return QTime(t.mid(0, 2).toInt(), t.mid(2, 2).toInt());
    }
    return QTime();
}

// An airport code, or nothing if the field is not one. Four characters is ICAO, which is
// what these formats use; a runway or a count is not accepted just for being the right
// length, because every character has to be a letter.
static QString airportField(const QString& field)
{
    static const QRegularExpression re("^[A-Z]{4}$");
    const QString t = field.trimmed();
    return re.match(t).hasMatch() ? t : QString();
}


// Position reports in the formats airlines put on labels 11, 21, 36, 45 and 8H.
//
// None of those labels is registered as a position report - 11 is "In Range Arrival",
// 21 is "Takeoff Data Calculation", 8H is "SAS Loadsheet Request" - and each of them
// also carries other traffic, so every one of these tests has to be certain before it
// claims the message. Where none matches, the caller falls through to the ARINC formats
// and finally to showing the text as it came.
//
// Formats and field meanings were taken from several hundred messages in the
// airframes.io archive, August 2026. Fields whose meaning could not be established from
// the sample are deliberately left unparsed rather than guessed at.
bool AcarsPositionReport::decodeAirlinePosition(const QString &text)
{
    // ---- Label 11: a self describing list, which is the easiest of them to be sure of
    //      POS
    //      CAS 278,LAT N 25.734,LON E120.484,ALT 32090,FOB 21800,UTC 151636,
    //      PD ----.--,WD 110,WS   29,TAT -  2
    if (text.startsWith("POS\n") || text.startsWith("POS\r") || text.startsWith("CAS "))
    {
        bool haveLat = false, haveLon = false;
        for (const QString& field : text.mid(text.indexOf("CAS ")).split(','))
        {
            const QString f = field.trimmed();
            const int space = f.indexOf(' ');
            if (space < 0) {
                continue;
            }
            const QString key = f.left(space);
            const QString value = f.mid(space + 1).trimmed();
            if (value.contains('-') && !value.startsWith('-')) {
                continue;       // ----.-- means the aircraft did not report it
            }

            if (key == "LAT") {
                haveLat = decodeDecimalDegrees(value, m_latitude);
            } else if (key == "LON") {
                haveLon = decodeDecimalDegrees(value, m_longitude);
            } else if (key == "ALT") {
                m_altitudeFt = value.toInt();
            } else if (key == "UTC") {
                m_time = decodeHms(value);
            } else if (key == "CAS") {
                m_speedKts = value.toInt();
            } else if (key == "WD") {
                m_windDirDeg = value.toInt();
            } else if (key == "WS") {
                m_windSpeedKts = value.toInt();
            } else if (key == "FOB") {
                m_fuelOnBoard = value;
            } else if (key == "TAT") {
                // Written with the sign detached from the digits, "-  2"
                bool ok = false;
                const int t = QString(value).remove(' ').toInt(&ok);
                if (ok) { m_temperatureValid = true; m_temperatureC = t; }
            }
        }
        if (haveLat && haveLon)
        {
            m_positionValid = true;
            m_reportType = "POS";
            return true;
        }
        return false;
    }

    // ---- Label 21: POS then the hemisphere letters, with no separator between them
    //      POSN 35.240W 85.333, 191,153530,38006, 1761,  30,-52,170406,KDFW
    {
        static const QRegularExpression re(
            R"(^POS([NS])\s*(\d{1,2}\.\d+)([EW])\s*(\d{1,3}\.\d+),(.*)$)");
        const QRegularExpressionMatch match = re.match(text);
        if (match.hasMatch())
        {
            m_latitude = match.captured(2).toFloat() * (match.captured(1) == "S" ? -1.0f : 1.0f);
            m_longitude = match.captured(4).toFloat() * (match.captured(3) == "W" ? -1.0f : 1.0f);
            m_positionValid = true;
            m_reportType = "POS";

            // Track, time, altitude, then fields whose meaning the sample did not
            // settle, and the destination last
            const QStringList fields = match.captured(5).split(',');
            if (fields.size() > 0) { m_trackDeg = fields[0].trimmed().toInt(); }
            if (fields.size() > 1) { m_time = decodeHms(fields[1]); }
            if (fields.size() > 2) { m_altitudeFt = fields[2].trimmed().toInt(); }
            if (fields.size() > 5)
            {
                bool ok = false;
                const int t = fields[5].trimmed().remove(' ').toInt(&ok);
                if (ok) { m_temperatureValid = true; m_temperatureC = t; }
            }
            if (fields.size() > 6) { m_eta = decodeHms(fields[6]); }
            if (fields.size() > 7) { m_arrivalAirport = airportField(fields[7]); }
            return true;
        }
    }

    // ---- Label 8H: position, time, and BOTH airports
    //      N 51.410,W  1.275,0616,SMJP,EHAM,32200,  510625,35999,25151,  42
    {
        const QStringList fields = text.split(',');
        if (fields.size() >= 8)
        {
            float latitude = 0.0f, longitude = 0.0f;
            const QString dep = airportField(fields[3]);
            const QString arr = airportField(fields[4]);
            if (!dep.isEmpty() && !arr.isEmpty()
                // The hemisphere letters have to be there: without them this would
                // claim any comma list whose first two fields happen to be numbers
                && fields[0].trimmed().startsWith('N') != fields[0].trimmed().startsWith('S')
                && fields[1].trimmed().startsWith('E') != fields[1].trimmed().startsWith('W')
                && decodeDecimalDegrees(fields[0], latitude)
                && decodeDecimalDegrees(fields[1], longitude))
            {
                m_latitude = latitude;
                m_longitude = longitude;
                m_positionValid = true;
                m_reportType = "POS";
                m_time = decodeHms(fields[2]);
                m_departureAirport = dep;
                m_arrivalAirport = arr;
                // Field 8 is the only one of the trailing numbers that reads as an
                // altitude across the whole sample; the others are left alone
                m_altitudeFt = fields[7].trimmed().toInt();
                return true;
            }
        }
    }

    return decodeInRangePosition(text) || decodeWaypointPosition(text);
}


// The in range formats on label 36, of which there are three. All give a position; two
// give the airports and the runways as well.
bool AcarsPositionReport::decodeInRangePosition(const QString &text)
{
    const QStringList lines = text.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);

    // ---- Three lines: date and tail, then position and altitude, then fuel
    //      28AUG26,141148,EI-DVG
    //      N 49.041,W  4.957,36022
    //      ------,  401250
    if (lines.size() >= 2)
    {
        static const QRegularExpression dateRe(R"(^(\d{2})([A-Z]{3})(\d{2}),(\d{6}),(\S+)$)");
        const QRegularExpressionMatch head = dateRe.match(lines[0].trimmed());
        const QStringList second = lines[1].split(',');
        float latitude = 0.0f, longitude = 0.0f;
        if (head.hasMatch() && (second.size() >= 3)
            && decodeDecimalDegrees(second[0], latitude)
            && decodeDecimalDegrees(second[1], longitude))
        {
            m_day = head.captured(1).toInt();
            m_month = monthFromName(head.captured(2));
            m_time = decodeHms(head.captured(4));
            m_tail = head.captured(5);
            m_latitude = latitude;
            m_longitude = longitude;
            m_positionValid = true;
            m_altitudeFt = second[2].trimmed().toInt();
            m_reportType = "POS";
            if (lines.size() >= 3)
            {
                const QStringList third = lines[2].split(',');
                if ((third.size() >= 2) && !third[1].contains('-')) {
                    m_fuelOnBoard = third[1].trimmed();
                }
            }
            return true;
        }
    }

    const QStringList fields = text.split(',');

    // ---- Position, altitude, fuel, then departure, destination and alternate, then
    //      the runways. The position is decimal degrees in one variant and degrees and
    //      decimal minutes in the other.
    //      28,E,29AUG26,144335,N2725.8,W08149.2,23010,11175,KMCO,KFLL,KFLL,10L/,10R/
    //      195,C,29AUG26,153523,42.057,-83.764,31079,5375,KIND,CYYZ,CYYZ,29,24R/-
    if (fields.size() >= 11)
    {
        float latitude = 0.0f, longitude = 0.0f;
        const bool placed =
               (decodeDegreesMinutes(fields[4], false, latitude)
                && decodeDegreesMinutes(fields[5], true, longitude))
            || (decodeDecimalDegrees(fields[4], latitude)
                && decodeDecimalDegrees(fields[5], longitude));
        const QString dep = airportField(fields[8]);
        const QString arr = airportField(fields[9]);
        if (placed && !dep.isEmpty() && !arr.isEmpty())
        {
            m_latitude = latitude;
            m_longitude = longitude;
            m_positionValid = true;
            m_reportType = "POS";
            m_time = decodeHms(fields[3]);
            m_altitudeFt = fields[6].trimmed().toInt();
            m_fuelOnBoard = fields[7].trimmed();
            m_departureAirport = dep;
            m_arrivalAirport = arr;
            m_alternateAirport = airportField(fields[10]);
            // The runways are written with a trailing slash and the pair they were
            // planned and cleared for, e.g. "10L/" or "24R/-"
            if (fields.size() > 11) { m_departureRunway = fields[11].trimmed().remove('/').remove('-'); }
            if (fields.size() > 12) { m_arrivalRunway = fields[12].trimmed().remove('/').remove('-'); }
            return true;
        }
    }

    // ---- The commonest of the three, and the least self explanatory: a fixed column
    //      list with the position given twice, the wind as a single ddd/ss number and
    //      the flight level in hundreds of feet
    //      71,M,1544,1,1,KLGA,40.86,-75.07,40.86,-75.07,17,281024,-8,68.3,7196,16,30,334,358,564CFA5
    if (fields.size() >= 19)
    {
        float latitude = 0.0f, longitude = 0.0f;
        const QString airport = airportField(fields[5]);
        if (!airport.isEmpty()
            && decodeDecimalDegrees(fields[6], latitude)
            && decodeDecimalDegrees(fields[7], longitude)
            && (fields[11].trimmed().size() == 6))
        {
            m_latitude = latitude;
            m_longitude = longitude;
            m_positionValid = true;
            m_reportType = "POS";
            m_time = decodeHms(fields[2]);
            m_arrivalAirport = airport;
            // Hundreds of feet - the sample runs from 17 with an outside air temperature
            // of -8 to 38 with -50, which no other reading of the column fits
            m_altitudeFt = fields[10].trimmed().toInt() * 100;
            const QString wind = fields[11].trimmed();
            m_windDirDeg = wind.left(3).toInt();
            m_windSpeedKts = wind.mid(3).toInt();
            bool ok = false;
            const int t = fields[12].trimmed().toInt(&ok);
            if (ok) { m_temperatureValid = true; m_temperatureC = t; }
            m_speedKts = fields[17].trimmed().toInt();
            m_groundSpeedKts = fields[18].trimmed().toInt();
            return true;
        }
    }

    return false;
}

// Labels 45 and 36 also carry a report that names the fixes either side of the aircraft,
// which no other ACARS position format does:
//
//     291545 KATL MMMY7
//     /FN 1929
//     0340      STEIT         MERDN         N32232W088483
//
// The day and time, the departure and destination, the flight number, then the fix just
// passed, the one coming up, and the position. The position is in the same degrees and
// tenths of a minute form the ARINC 702A route inserts use, so N32232W088483 is
// N32 23.2 W088 48.3. Where the aircraft could not place itself the field is dashes.
bool AcarsPositionReport::decodeWaypointPosition(const QString &text)
{
    const QStringList lines = text.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
    if (lines.size() < 3) {
        return false;
    }

    static const QRegularExpression headRe(R"(^(\d{2})(\d{4})\s+([A-Z]{4})\s+([A-Z]{4})\d?\s*$)");
    const QRegularExpressionMatch head = headRe.match(lines[0]);
    if (!head.hasMatch() || !lines[1].startsWith("/FN")) {
        return false;
    }

    // The position closes the third line, and is what makes this worth reporting
    static const QRegularExpression posRe(R"(([NS])(\d{2})(\d{3})([EW])(\d{3})(\d{3})\s*$)");
    const QRegularExpressionMatch pos = posRe.match(lines[2]);
    if (!pos.hasMatch()) {
        return false;
    }

    m_day = head.captured(1).toInt();
    m_time = decodeHms(head.captured(2));
    m_departureAirport = head.captured(3);
    m_arrivalAirport = head.captured(4);
    m_reportFlight = lines[1].mid(3).trimmed();
    m_reportType = "POS";

    m_latitude = pos.captured(2).toFloat() + pos.captured(3).toFloat() / 600.0f;
    if (pos.captured(1) == "S") { m_latitude = -m_latitude; }
    m_longitude = pos.captured(5).toFloat() + pos.captured(6).toFloat() / 600.0f;
    if (pos.captured(4) == "W") { m_longitude = -m_longitude; }
    m_positionValid = true;

    // Whatever is between the leading number and the position, in fixed width columns.
    // One fix means the one just passed; two means passed and next.
    const QString middle = lines[2].left(lines[2].size() - pos.captured(0).size());
    QStringList names;
    static const QRegularExpression nameRe("^[A-Z]{3,5}$");
    for (const QString& token : middle.split(' ', Qt::SkipEmptyParts))
    {
        if (nameRe.match(token).hasMatch()) {
            names.append(token);
        }
    }
    if (names.size() >= 1) { m_lastWaypoint = names[0]; }
    if (names.size() >= 2) { m_nextWaypoint = names[1]; }
    return true;
}

// The 01CLER cruise report, on labels 17 and 49. It carries no latitude or longitude,
// so it places nothing on the map - but it names both airports, which is a route pair
// straight from the aircraft, and the altitude it is at.
//
//   01CLERAIC118/302042EGBBVIAR        01CLER    ETD078/310934EGCCOMAA
//   34726 287.6 -12.9                  +34918291.3- 13.4
//
// Two ways of writing the same thing. The callsign is left aligned on label 17 and right
// aligned on 49; the figures are space separated on 17 and packed together on 49, where
// they are also blank padded in the way label 17's position reports are - "+ 5231" is
// 5231 feet, not 5231 with something missing.
//
// UAL900 is the check that the altitude reading is right: it went off at 10:07:39 by its
// own OOOI message, and this report at 10:10 has it at 5231 feet, three minutes into the
// climb. The two figures after the altitude are left alone - around 250 to 300 and -28 to
// +22, so heading and an ISA deviation would fit, but nothing in the samples says so.
bool AcarsPositionReport::decodeCruiseReport(const QString &text)
{
    static const QRegularExpression re(
        R"RX(01CLER\s*(\S+?)\s*/(\d{2})(\d{2})(\d{2})([A-Z]{4})([A-Z]{4})\s*[\r\n]+\s*(?:([+-])([ \d]{5})|(\d{4,5})[ ]))RX");
    const QRegularExpressionMatch match = re.match(text);
    if (!match.hasMatch()) {
        return false;
    }

    m_reportFlight = match.captured(1);
    m_day = match.captured(2).toInt();
    m_time = QTime(match.captured(3).toInt(), match.captured(4).toInt());
    m_departureAirport = match.captured(5);
    m_arrivalAirport = match.captured(6);

    // The packed form pads with blanks where a leading zero would be
    if (!match.captured(8).isEmpty())
    {
        const int feet = QString(match.captured(8)).replace(' ', '0').toInt();
        m_altitudeFt = (match.captured(7) == "-") ? -feet : feet;
    }
    else
    {
        m_altitudeFt = match.captured(9).toInt();
    }
    m_reportType = "CRZ";

    return true;
}

// The company position report on label 17 - another label the dispatch sends to
// AcarsEventReport, so it was titled "OOOI" while carrying no OOOI time and nothing was
// taken from it:
//
//   173442,19231,2010, 23200,N 51.892,E   .642,29AUG26,26736,  41,- 15
//
// Time, a figure this format does not identify, an ETA, the altitude in feet, the
// position, the date, and three more unidentified figures.
//
// EVERY NUMBER IS BLANK PADDED, AND A BLANK IS A ZERO. That is the whole difficulty of
// the format and it is not a guess: across 900 coordinate fields the fraction is three
// digits 888 times and " dd" the other 12, with the blank ALWAYS leading and never in
// the middle or at the end, and the integer part is right aligned the same way. So
// "E   .642" is 0.642 and "E   . 73" is 0.073 - not 0.73, which is how it first read and
// would have put the aircraft seven times too far east.
//
// The ETA is read because it is a valid time in all 452 samples that parse; the figures
// either side of it are not, because nothing in the samples says what they are.
//
// "MMMM.MMM" in either coordinate means the position is not available, and "------" the
// same for the altitude.
bool AcarsPositionReport::decodeCompanyPosition(const QString &text)
{
    // The same report in a shorter form on label 16, which is a thousand messages here
    // and the largest single block of traffic that was going undecoded:
    //
    //   202900,28645,2100,  98,N 50.191 W   .870
    //
    // Time, an unidentified figure, an ETA, a second unidentified figure, and the
    // position - which is separated by a SPACE here where label 17 uses a comma, and
    // carries no date or trailing figures.
    //
    // The blank padding is label 17's, and the positions are right: 411 of them checked
    // against the same airframe's ADS-B within a minute came out with a median error of
    // 0.010 degrees of latitude and 0.016 of longitude, about a kilometre.
    //
    // The fourth field is NOT the altitude, which is what it looks like beside label
    // 17's. Read as hundreds of feet it misses the ADS-B altitude by 3587 ft at the
    // median, and one aircraft reported 128 and then 77 while ADS-B had it at 10,600 ft
    // both times. It is not the distance to the destination either - 154 nm out at the
    // median. Within one leg it falls steadily as the ETA nears and resets for the next,
    // so fuel is the likely answer, but nothing here establishes it and it is left alone
    // rather than labelled wrongly.
    if (m_label == "16")
    {
        static const QRegularExpression shortRe(
            R"RX(^(\d{2})(\d{2})(\d{2}),[^,]*,(\d{2})(\d{2}),\s*\d{1,4},([NS])([ \d]{2,3}\.[ \d]{3}) ([EW])([ \d]{3}\.[ \d]{3}))RX");
        const QRegularExpressionMatch shortMatch = shortRe.match(text);
        if (!shortMatch.hasMatch()) {
            return false;
        }

        auto blankPadded = [](const QString& field)
        {
            return QString(field).replace(' ', '0').toFloat();
        };

        m_time = QTime(shortMatch.captured(1).toInt(), shortMatch.captured(2).toInt(),
            shortMatch.captured(3).toInt());
        m_eta = QTime(shortMatch.captured(4).toInt(), shortMatch.captured(5).toInt());
        m_latitude = blankPadded(shortMatch.captured(7))
            * (shortMatch.captured(6) == "S" ? -1.0f : 1.0f);
        m_longitude = blankPadded(shortMatch.captured(9))
            * (shortMatch.captured(8) == "W" ? -1.0f : 1.0f);
        m_positionValid = true;
        m_reportType = "POS";
        return true;
    }

    if (m_label != "17") {
        return false;
    }

    static const QRegularExpression re(
        R"RX(^(\d{2})(\d{2})(\d{2}),\s*\d+,(\d{2})(\d{2}),([- \d]+),([NS])([ \d]{2,3}\.[ \d]{3}),([EW])([ \d]{3}\.[ \d]{3}),(\d{2})([A-Z]{3})(\d{2}),)RX");
    const QRegularExpressionMatch match = re.match(text);
    if (!match.hasMatch()) {
        return false;
    }

    // A blank stands for a zero, so it cannot simply be stripped
    auto blankPadded = [](const QString& field)
    {
        return QString(field).replace(' ', '0').toFloat();
    };

    m_time = QTime(match.captured(1).toInt(), match.captured(2).toInt(),
        match.captured(3).toInt());
    m_eta = QTime(match.captured(4).toInt(), match.captured(5).toInt());
    m_day = match.captured(11).toInt();
    m_latitude = blankPadded(match.captured(8)) * (match.captured(7) == "S" ? -1.0f : 1.0f);
    m_longitude = blankPadded(match.captured(10)) * (match.captured(9) == "W" ? -1.0f : 1.0f);
    m_positionValid = true;
    const QString altitude = match.captured(6).trimmed();
    if (!altitude.isEmpty() && !altitude.contains('-')) {
        m_altitudeFt = altitude.toInt();
    }
    m_reportType = "POS";

    return true;
}

// The POX position report, which arrives on label 18 - a label the dispatch otherwise
// sends to AcarsEventReport, so it was filed as an OOOI event and nothing was taken from
// it at all:
//
//   POX,CZ0673,31AUG26,161247,ZBAD,EGLL,N51.745,E001.690,240,108,0.639,-084,-056.3,053,-027,1635,
//
// Flight, date, time, both airports, the position, then the flight level, a figure that
// falls slowly through a flight, and the Mach number.
//
// The flight level is not a guess: taking one aircraft's reports in order gives a steady
// descent - 240, 201, 167, 144, 122, 090, 076, 067, 047 into Heathrow - and B-32EW's ADS-B
// puts it at 23000 ft twelve seconds after the report reading 240. The three fields after
// the Mach are left alone; one of them tracks temperature and another looks like an ETA,
// but the samples do not settle which is which.
//
// A second form of the same report carries no hemisphere letters and space pads its
// fields - "POX,CZ0689,31AUG26,050542,ZGGG,EGKK, 51.040,    .89,..." - and is deliberately
// not decoded: without a hemisphere there is nothing to say which side of the equator or
// the meridian it is on, and half the longitude digits are missing as well.
bool AcarsPositionReport::decodePoxPosition(const QString &text)
{
    static const QRegularExpression re(
        R"RX(^POX,(\S*),(\d{2})([A-Z]{3})(\d{2}),(\d{2})(\d{2})(\d{2}),([A-Z]{4}),([A-Z]{4}),([NS])(\d+\.\d+),([EW])(\d+\.\d+),(\d{1,3}),\d+,(\d\.\d+),)RX");
    const QRegularExpressionMatch match = re.match(text);
    if (!match.hasMatch()) {
        return false;
    }

    m_reportFlight = match.captured(1);
    m_day = match.captured(2).toInt();
    m_time = QTime(match.captured(5).toInt(), match.captured(6).toInt(),
        match.captured(7).toInt());
    m_departureAirport = match.captured(8);
    m_arrivalAirport = match.captured(9);
    m_latitude = match.captured(11).toFloat() * (match.captured(10) == "S" ? -1.0f : 1.0f);
    m_longitude = match.captured(13).toFloat() * (match.captured(12) == "W" ? -1.0f : 1.0f);
    m_positionValid = true;
    m_altitudeFt = match.captured(14).toInt() * 100;
    m_reportType = "POS";

    return true;
}

// Label 83 carries two position formats, which together are nine tenths of the label.
//
//   001PR30193303N2746.0W08244.51193040029
//   EPWA,KJFK,301917, 42.48,- 71.07,41002,246,-138.7, 10200
//
// The first is a sequence number, PR, the day, the time, the position in degrees and
// decimal minutes, then ten digits of which the first four are the altitude in tens of
// feet. That reading is not a guess: taking one aircraft's reports in time order gives a
// textbook climb and level-off - 29540, 38140, 39940, 39940, 40040, 40040, 39940 ft over
// an hour - and across 176 reports the figure never leaves 1410 to 45050 ft.
//
// The remaining six digits are left alone. The last group counts down about one per
// minute, so it is almost certainly time to destination, but the samples do not settle
// where the field boundary falls, and a mislabelled number is worse than an absent one.
//
// The second is the same report written by a different operator: both airports, the day
// and time, position in decimal degrees, altitude in feet and track. Note the sign of the
// longitude is separated from its digits by a space, which no ordinary conversion
// accepts.
bool AcarsPositionReport::decodeLabel83Position(const QString &text)
{
    if (m_label != "83") {
        return false;
    }

    static const QRegularExpression sequencedRe(
        R"(^(\d{3})PR(\d{2})(\d{2})(\d{2})(\d{2})([NS])(\d{2})(\d{2}\.\d)([EW])(\d{3})(\d{2}\.\d)(\d{4}))");
    const QRegularExpressionMatch sequenced = sequencedRe.match(text);
    if (sequenced.hasMatch())
    {
        m_day = sequenced.captured(2).toInt();
        m_time = QTime(sequenced.captured(3).toInt(), sequenced.captured(4).toInt(),
            sequenced.captured(5).toInt());
        m_latitude = (sequenced.captured(7).toFloat() + sequenced.captured(8).toFloat() / 60.0f)
            * (sequenced.captured(6) == "S" ? -1.0f : 1.0f);
        m_longitude = (sequenced.captured(10).toFloat() + sequenced.captured(11).toFloat() / 60.0f)
            * (sequenced.captured(9) == "W" ? -1.0f : 1.0f);
        m_positionValid = true;
        m_altitudeFt = sequenced.captured(12).toInt() * 10;
        m_reportType = "POS";
        return true;
    }

    static const QRegularExpression routedRe(
        R"(^([A-Z]{4}),([A-Z]{4}),(\d{2})(\d{2})(\d{2}),\s*(-?\s*\d+\.\d+),\s*(-?\s*\d+\.\d+),\s*(\d+),\s*(\d+),)");
    const QRegularExpressionMatch routed = routedRe.match(text);
    if (routed.hasMatch())
    {
        m_departureAirport = routed.captured(1);
        m_arrivalAirport = routed.captured(2);
        m_day = routed.captured(3).toInt();
        m_time = QTime(routed.captured(4).toInt(), routed.captured(5).toInt());
        // "- 71.07" - the minus sign is separated from the digits, so the spaces have to
        // go before the conversion, which would otherwise read it as +71.07 and put the
        // aircraft on the wrong side of the Greenwich meridian
        m_latitude = QString(routed.captured(6)).remove(' ').toFloat();
        m_longitude = QString(routed.captured(7)).remove(' ').toFloat();
        m_positionValid = true;
        m_altitudeFt = routed.captured(8).toInt();
        m_trackDeg = routed.captured(9).toInt();
        m_reportType = "POS";
        return true;
    }

    return false;
}

// Label 30, of which this is about a fifth:
//
//   182,HG,1932,5,KSTL,N 38.690,W 77.747,,, 70
//
// The time and the position are unambiguous and are what is taken. The airport and the
// trailing figure are not: the airport is consistent with being the destination in every
// sample but never stated to be, and the figure could be a flight level or a time to run.
// Neither is decoded on that basis.
bool AcarsPositionReport::decodeStationPosition(const QString &text)
{
    if (m_label != "30") {
        return false;
    }

    static const QRegularExpression re(
        R"(^\d+,[A-Z]{2},(\d{2})(\d{2}),\d+,[A-Z]{4},\s*([NS])\s*(\d+\.\d+),\s*([EW])\s*(\d+\.\d+),)");
    const QRegularExpressionMatch match = re.match(text);
    if (!match.hasMatch()) {
        return false;
    }

    m_time = QTime(match.captured(1).toInt(), match.captured(2).toInt());
    m_latitude = match.captured(4).toFloat() * (match.captured(3) == "S" ? -1.0f : 1.0f);
    m_longitude = match.captured(6).toFloat() * (match.captured(5) == "W" ? -1.0f : 1.0f);
    m_positionValid = true;
    m_reportType = "POS";

    return true;
}

// The comma separated position report on label 32, in the three variants seen off air:
//
//   P,E,2026-08-30 19:19:04,5820,55888,N 41.877 W 087.840,6999,254,034,15,316,281,426,1932,
//   P,R,2026-08-30 19:18:05,5692,70900,N41.890 W088.268,34996,325,29,-45,467,440,748,,
//   P,H,30AUG26 18:14:25,2904,  43680,N 45.571 W 93.609,11225,264, 31,  24,335375,282.4,520,1826,
//
// The subtype letter, a timestamp written two different ways, two figures this format
// does not identify, the position, the altitude in feet and the track. The columns after
// the track disagree between the variants, so nothing beyond it is read.
//
// Half of one hundred consecutive label 32 messages are this format and all fifty match;
// the other half are several airline formats that share the label, which is ordinary for
// these labels and why this returns false rather than assuming.
bool AcarsPositionReport::decodeCsvPosition(const QString &text)
{
    if (m_label != "32") {
        return false;
    }

    static const QRegularExpression re(
        R"(^P,([A-Z]),([^,]+),\d+,\s*\d+,\s*([NS])\s*(\d+\.\d+)\s+([EW])\s*(\d+\.\d+),\s*(\d+),\s*(\d+),)");
    const QRegularExpressionMatch match = re.match(text);
    if (!match.hasMatch()) {
        return false;
    }

    m_latitude = match.captured(4).toFloat() * (match.captured(3) == "S" ? -1.0f : 1.0f);
    m_longitude = match.captured(6).toFloat() * (match.captured(5) == "W" ? -1.0f : 1.0f);
    m_positionValid = true;
    m_altitudeFt = match.captured(7).toInt();
    m_trackDeg = match.captured(8).toInt();
    m_reportType = "POS";

    // "2026-08-30 19:19:04" and "30AUG26 18:14:25" - the same instant written by two
    // different operators, so the day is taken from whichever end carries it
    const QString when = match.captured(2);
    static const QRegularExpression timeRe(R"((\d{2}):(\d{2}):(\d{2}))");
    const QRegularExpressionMatch time = timeRe.match(when);
    if (time.hasMatch())
    {
        m_time = QTime(time.captured(1).toInt(), time.captured(2).toInt(),
            time.captured(3).toInt());
    }
    static const QRegularExpression dayRe(R"(^(?:\d{4}-\d{2}-(\d{2})|(\d{1,2})[A-Z]{3}))");
    const QRegularExpressionMatch day = dayRe.match(when);
    if (day.hasMatch())
    {
        m_day = day.captured(1).isEmpty() ? day.captured(2).toInt()
                                          : day.captured(1).toInt();
    }

    return true;
}

// Label 4N: a short position report that also names the sector being flown.
//
//   30194N  LGA  HSV1
//   0068  0087 N038544 W079020   360
//
// Day and hour, the label repeated, origin and destination in three letter IATA and a
// part number; then two figures, the position in degrees and decimal minutes, and the
// flight level. Checked against 100 consecutive label 4N messages off air, every one of
// which matches, and the positions fall where the named sector says they should - an HPN
// to DTW report at flight level 037 sits over Detroit.
//
// The two leading figures are deliberately not decoded. Minutes remaining and fuel are
// both plausible readings and the samples do not settle it, and a mislabelled number is
// worse than an undisplayed one.
bool AcarsPositionReport::decodeRoutedPosition(const QString &text)
{
    if (m_label != "4N") {
        return false;
    }

    static const QRegularExpression re(
        R"(^(\d{2})(\d{2})4N\s+([A-Z]{3})\s+([A-Z]{3})\d\s*[\r\n]+\s*\d{4}\s+\d{4}\s+([NS])(\d{3})(\d{3})\s+([EW])(\d{3})(\d{3})\s+(\d{3}))");
    const QRegularExpressionMatch match = re.match(text);
    if (!match.hasMatch()) {
        return false;
    }

    m_day = match.captured(1).toInt();
    m_time = QTime(match.captured(2).toInt(), 0);

    // Written in IATA, and everything downstream works in ICAO
    const QString from = icaoFromIata(match.captured(3));
    const QString to = icaoFromIata(match.captured(4));
    m_departureAirport = from.isEmpty() ? match.captured(3) : from;
    m_arrivalAirport = to.isEmpty() ? match.captured(4) : to;

    // Degrees and decimal minutes, so the minutes are tenths and there are 600 of them
    // to the degree
    m_latitude = (match.captured(6).toFloat() + match.captured(7).toFloat() / 600.0f)
        * (match.captured(5) == "S" ? -1.0f : 1.0f);
    m_longitude = (match.captured(9).toFloat() + match.captured(10).toFloat() / 600.0f)
        * (match.captured(8) == "W" ? -1.0f : 1.0f);
    m_positionValid = true;
    m_altitudeFt = match.captured(11).toInt() * 100;
    m_reportType = "POS";

    return true;
}

bool AcarsPositionReport::decode(const QString &text)
{
    if (decodeAirlinePosition(text)) {
        return true;
    }

    if (decodeRoutedPosition(text)) {
        return true;
    }

    if (decodeCsvPosition(text)) {
        return true;
    }

    if (decodeLabel83Position(text)) {
        return true;
    }

    if (decodeStationPosition(text)) {
        return true;
    }

    if (decodePoxPosition(text)) {
        return true;
    }

    if (decodeCompanyPosition(text)) {
        return true;
    }

    if (decodeCruiseReport(text)) {
        return true;
    }

    if ((m_label == "12") || (m_label == "16"))
    {
        // E.g: N 42.150,W121.187,39000,161859, 109,.C-GWSO,1742   (12)
        //      N 44.203,W 86.546,31965,6, 290                     (16)
        //      N 28.177/W 96.055                                  (16)
        QRegularExpression re(R"(^([NS])\s*(\d{1,2}\.\d+)\s*[,/]\s*([EW])\s*(\d{1,3}\.\d+)(?:,\s*(\d+))?)");
        QRegularExpressionMatch match = re.match(text);
        if (match.hasMatch())
        {
            m_latitude = match.captured(2).toFloat() * (match.captured(1) == "S" ? -1.0f : 1.0f);
            m_longitude = match.captured(4).toFloat() * (match.captured(3) == "W" ? -1.0f : 1.0f);
            m_positionValid = true;
            if (!match.captured(5).isEmpty()) {
                m_altitudeFt = match.captured(5).toInt();
            }
            if (m_label == "12")
            {
                // The fourth field is the time as HHMMSS
                QStringList fields = text.split(',');
                if (fields.size() >= 4)
                {
                    QString t = fields[3].trimmed();
                    if (t.size() == 6) {
                        m_time = QTime(t.mid(0, 2).toInt(), t.mid(2, 2).toInt(), t.mid(4, 2).toInt());
                    }
                }
            }
            return true;
        }
    }
    else if (m_label == "10")
    {
        // E.g: POS082150, N 3885,W 7841,---,308,26922,  51,22290, 529,  19,-225,6
        // The time is HHMMSS; latitude and longitude are degrees x100; the eighth
        // field is the altitude in feet
        QRegularExpression re(R"(^POS(\d{6}),\s*([NS])\s*(\d+),\s*([EW])\s*(\d+),)");
        QRegularExpressionMatch match = re.match(text);
        if (match.hasMatch())
        {
            m_time = QTime(match.captured(1).mid(0, 2).toInt(),
                           match.captured(1).mid(2, 2).toInt(),
                           match.captured(1).mid(4, 2).toInt());
            m_latitude = match.captured(3).toFloat() / 100.0f * (match.captured(2) == "S" ? -1.0f : 1.0f);
            m_longitude = match.captured(5).toFloat() / 100.0f * (match.captured(4) == "W" ? -1.0f : 1.0f);
            m_positionValid = true;
            QStringList fields = text.split(',');
            if (fields.size() >= 8)
            {
                bool ok;
                int alt = fields[7].trimmed().toInt(&ok);
                if (ok) {
                    m_altitudeFt = alt;
                }
            }
            return true;
        }
    }
    else if (m_label == "22")
    {
        // E.g: N 370824W 760010,-------,194936,30418, ,      , ,M 42,27335  42, 107,
        // Latitude and longitude are degrees x10000; the third field is the time as
        // HHMMSS and the fourth the altitude in feet
        QRegularExpression re(R"(^([NS])\s*(\d{6})\s*([EW])\s*(\d{6}),)");
        QRegularExpressionMatch match = re.match(text);
        if (match.hasMatch())
        {
            m_latitude = match.captured(2).toFloat() / 10000.0f * (match.captured(1) == "S" ? -1.0f : 1.0f);
            m_longitude = match.captured(4).toFloat() / 10000.0f * (match.captured(3) == "W" ? -1.0f : 1.0f);
            m_positionValid = true;
            QStringList fields = text.split(',');
            if ((fields.size() >= 3) && (fields[2].trimmed().size() == 6))
            {
                QString t = fields[2].trimmed();
                m_time = QTime(t.mid(0, 2).toInt(), t.mid(2, 2).toInt(), t.mid(4, 2).toInt());
            }
            if (fields.size() >= 4)
            {
                bool ok;
                int alt = fields[3].trimmed().toInt(&ok);
                if (ok) {
                    m_altitudeFt = alt;
                }
            }
            return true;
        }
    }
    else if (m_label == "44")
    {
        // E.g: POS02,N38171W077507,319,KJFK,KUZA,0926,0245,0327,004.6
        // Coordinates are degrees then minutes x10 (38 degrees 17.1 minutes);
        // altitude is in flight levels; then departure and arrival airports, the
        // date as MMDD, the time and ETA as HHMM, and fuel in tonnes
        QRegularExpression re(R"(^(?:00)?(POS|ETA|IN|OFF|ON)(\d{2}),([NS])(\d\d)(\d{3})([EW])(\d{3})(\d{3}),(\d{1,3}),(\w{4}),(\w{4}),(\d{4}),(\d{4}),(\d{4}),(\d+\.?\d*))");
        QRegularExpressionMatch match = re.match(text);
        if (match.hasMatch())
        {
            m_reportType = match.captured(1);
            m_latitude = match.captured(4).toFloat() + match.captured(5).toFloat() / 10.0f / 60.0f;
            if (match.captured(3) == "S") {
                m_latitude = -m_latitude;
            }
            m_longitude = match.captured(7).toFloat() + match.captured(8).toFloat() / 10.0f / 60.0f;
            if (match.captured(6) == "W") {
                m_longitude = -m_longitude;
            }
            m_positionValid = true;
            m_altitudeFt = match.captured(9).toInt() * 100;
            m_departureAirport = match.captured(10);
            m_arrivalAirport = match.captured(11);
            m_month = match.captured(12).mid(0, 2).toInt();
            m_day = match.captured(12).mid(2, 2).toInt();
            m_time = QTime(match.captured(13).mid(0, 2).toInt(), match.captured(13).mid(2, 2).toInt());
            m_eta = QTime(match.captured(14).mid(0, 2).toInt(), match.captured(14).mid(2, 2).toInt());
            m_fuelTons = match.captured(15).toFloat();
            return true;
        }
    }
    else if (m_label == "24")
    {
        // E.g: /241710/1021/04WM/34962/N53.13/E001.33/3374/1056/
        // Day and time, then flight, altitude in feet, position and ETA
        QRegularExpression re(R"(^/24(\d{4})/(\d+)/(\w+)/(\d+)/([NS])(\d{1,2}\.\d+)/([EW])(\d{1,3}\.\d+)/(\d+)/(\d{4})/?)");
        QRegularExpressionMatch match = re.match(text);
        if (match.hasMatch())
        {
            m_time = QTime(match.captured(1).mid(0, 2).toInt(), match.captured(1).mid(2, 2).toInt());
            m_reportFlight = match.captured(3);
            m_altitudeFt = match.captured(4).toInt();
            m_latitude = match.captured(6).toFloat() * (match.captured(5) == "S" ? -1.0f : 1.0f);
            m_longitude = match.captured(8).toFloat() * (match.captured(7) == "W" ? -1.0f : 1.0f);
            m_positionValid = true;
            m_eta = QTime(match.captured(10).mid(0, 2).toInt(), match.captured(10).mid(2, 2).toInt());
            return true;
        }
    }
    qDebug() << "AcarsPositionReport: Failed to match label" << m_label << text;
    return false;
}

QString AcarsPositionReport::toString()
{
    if (!m_positionValid) {
        return "";
    }
    const QHash<QString, QString> reportTypes = {
        {"POS", "Position report"},
        {"ETA", "ETA report"},
        {"IN", "In gate report"},
        {"OFF", "Takeoff report"},
        {"ON", "Landing report"}
    };
    QStringList lines;
    if (!m_reportType.isEmpty()) {
        lines.append(QString("<b>Report:</b> %1").arg(reportTypes.value(m_reportType, m_reportType)));
    }
    if (!m_reportFlight.isEmpty()) {
        lines.append(QString("<b>Flight:</b> %1").arg(m_reportFlight));
    }
    if (!m_departureAirport.isEmpty()) {
        lines.append(QString("<b>Departure:</b> %1").arg(icaoAndAirportName(m_departureAirport)));
    }
    if (!m_arrivalAirport.isEmpty()) {
        lines.append(QString("<b>Arrival:</b> %1").arg(icaoAndAirportName(m_arrivalAirport)));
    }
    lines.append(QString("<b>Position:</b> %1").arg(positionString(m_latitude, m_longitude)));
    if (m_altitudeFt >= 0) {
        lines.append(QString("<b>Altitude:</b> %1 ft").arg(m_altitudeFt));
    }
    if (m_day >= 0) {
        lines.append(QString("<b>Date:</b> %1/%2").arg(m_day, 2, 10, QChar('0')).arg(m_month, 2, 10, QChar('0')));
    }
    if (m_time.isValid()) {
        lines.append(QString("<b>Time:</b> %1").arg(m_time.toString(m_time.second() ? "hh:mm:ss" : "hh:mm")));
    }
    if (m_eta.isValid()) {
        lines.append(QString("<b>ETA:</b> %1").arg(m_eta.toString("hh:mm")));
    }
    if (m_fuelTons >= 0.0f) {
        lines.append(QString("<b>Fuel on board:</b> %1 t").arg(m_fuelTons, 0, 'f', 1));
    }
    // As reported - see m_fuelOnBoard, the units are the operator's
    if (!m_fuelOnBoard.isEmpty()) {
        lines.append(QString("<b>Fuel on board:</b> %1").arg(m_fuelOnBoard));
    }
    if (!m_tail.isEmpty()) {
        lines.append(QString("<b>Registration:</b> %1").arg(m_tail));
    }
    if (!m_alternateAirport.isEmpty()) {
        lines.append(QString("<b>Alternate:</b> %1").arg(icaoAndAirportName(m_alternateAirport)));
    }
    if (!m_departureRunway.isEmpty()) {
        lines.append(QString("<b>Departure runway:</b> %1").arg(m_departureRunway));
    }
    if (!m_arrivalRunway.isEmpty()) {
        lines.append(QString("<b>Arrival runway:</b> %1").arg(m_arrivalRunway));
    }
    if (!m_lastWaypoint.isEmpty()) {
        lines.append(QString("<b>Last waypoint:</b> %1").arg(m_lastWaypoint));
    }
    if (!m_nextWaypoint.isEmpty()) {
        lines.append(QString("<b>Next waypoint:</b> %1").arg(m_nextWaypoint));
    }
    if (m_trackDeg >= 0) {
        lines.append(QString("<b>Track:</b> %1%2").arg(m_trackDeg).arg(QChar(0xB0)));
    }
    if (m_speedKts >= 0) {
        lines.append(QString("<b>Airspeed:</b> %1 kn").arg(m_speedKts));
    }
    if (m_groundSpeedKts >= 0) {
        lines.append(QString("<b>Ground speed:</b> %1 kn").arg(m_groundSpeedKts));
    }
    if ((m_windDirDeg >= 0) && (m_windSpeedKts >= 0)) {
        lines.append(QString("<b>Wind:</b> %1%2 / %3 kn")
            .arg(m_windDirDeg, 3, 10, QChar('0')).arg(QChar(0xB0)).arg(m_windSpeedKts));
    }
    if (m_temperatureValid) {
        lines.append(QString("<b>Temperature:</b> %1%2C").arg(m_temperatureC).arg(QChar(0xB0)));
    }
    return lines.join("<br>");
}

bool AcarsPositionReport::getPosition(float& latitude, float& longitude, int& altitudeFt) const
{
    if (!m_positionValid) {
        return false;
    }
    latitude = m_latitude;
    longitude = m_longitude;
    altitudeFt = m_altitudeFt;
    return true;
}

AcarsEventReport::AcarsEventReport(const QByteArray &message) :
    AcarsMessage(message)
{
}

// OOOI event reports: /13 OUT EVENT      / KSFO KEWR 12 231445/TIME 2314
// with optional additional lines such as /LOC N373703,W1222251 (degrees, minutes,
// seconds), /LOC +38.9603,-077.4595 (decimal) and /FOB 0215 (fuel, 100kg)
bool AcarsEventReport::decode(const QString &text)
{
    // Not every label 13 message is an OUT event. Six here read:
    //
    //   EGLL,0639,0659
    //
    // and the first time is the minute the message was RECEIVED in all six, while the
    // second is about twenty minutes ahead. HB-JPA sent "EGLL,1034,1034" while ADS-B had
    // it at 178 feet - on the runway, its ETA reached. The airport is the arrival in five
    // of the six, the sixth having a doubtful flight attribution.
    //
    // So it is a destination and an ETA, not a pair of movement times. Taken as OUT and
    // OFF - which is what the label suggests, and what it looks like - it would have
    // recorded an aircraft leaving the gate at the moment it was landing somewhere else.
    // Label 8E sends the same thing with the "time now" left off - "EGKK,0829" - and 189
    // of them here are all the destination and an ETA: the time is in the FUTURE in every
    // single one, from 4 to 152 minutes ahead and a median of 30, and the airport is the
    // arrival in 145 against the departure in 2. So they are not an OUT or an OFF, which
    // are times that have already happened.
    // Label 8S sends the same destination and ETA with flags and figures after it -
    // "EGJJ,0851,N,Y,N,0,0,1" - so the ETA is not the end of the line there. All 106 of
    // those have the time in the future too, 5 to 220 minutes ahead and a median of 55,
    // with the airport the arrival in 75 against the departure in 1.
    //
    // What the three flags and three figures are is not established - they are operator
    // specific and the samples do not say - so only the destination and the ETA are read
    // and the rest is left in the message text.
    static const QRegularExpression etaRe(
        R"RX(^([A-Z]{4}),(?:(\d{2})(\d{2}),)?(\d{2})(\d{2})(?:,|$))RX");
    const QRegularExpressionMatch etaMatch = etaRe.match(text.trimmed());
    if (etaMatch.hasMatch())
    {
        m_arrivalAirport = etaMatch.captured(1);
        if (!etaMatch.captured(2).isEmpty()) {
            m_eventTime = QTime(etaMatch.captured(2).toInt(), etaMatch.captured(3).toInt());
        }
        m_eta = QTime(etaMatch.captured(4).toInt(), etaMatch.captured(5).toInt());
        m_eventDescription = "ETA report";
        // Deliberately NOT an OOOI event: m_event is left alone, so the worker files no
        // movement time and the Out and Off columns stay empty, which is the truth
        return true;
    }

    QRegularExpression re(R"(^/1([3-8])\s+(.+?)/\s*([A-Z0-9]{4})\s+([A-Z0-9]{4})\s+(\d{1,2})\s+(\d{6}))");
    QRegularExpressionMatch match = re.match(text);
    if (!match.hasMatch())
    {
        qDebug() << "AcarsEventReport: Failed to match " << text;
        return false;
    }
    m_event = 10 + match.captured(1).toInt();
    m_eventDescription = match.captured(2).trimmed();
    m_departureAirport = match.captured(3);
    m_arrivalAirport = match.captured(4);
    m_day = match.captured(5).toInt();
    QString t = match.captured(6);
    m_eventTime = QTime(t.mid(0, 2).toInt(), t.mid(2, 2).toInt(), t.mid(4, 2).toInt());

    QRegularExpression fobRe(R"(/FOB\s+(\d+))");
    QRegularExpressionMatch fobMatch = fobRe.match(text);
    if (fobMatch.hasMatch()) {
        m_fuelOnBoard = fobMatch.captured(1).toInt();
    }

    // Position in degrees/minutes/seconds, e.g. N373703,W1222251
    QRegularExpression locDmsRe(R"(/LOC\s+([NS])(\d\d)(\d\d)(\d\d),\s*([EW])(\d{2,3})(\d\d)(\d\d))");
    QRegularExpressionMatch locMatch = locDmsRe.match(text);
    if (locMatch.hasMatch())
    {
        m_latitude = locMatch.captured(2).toFloat() + locMatch.captured(3).toFloat() / 60.0f + locMatch.captured(4).toFloat() / 3600.0f;
        if (locMatch.captured(1) == "S") {
            m_latitude = -m_latitude;
        }
        m_longitude = locMatch.captured(6).toFloat() + locMatch.captured(7).toFloat() / 60.0f + locMatch.captured(8).toFloat() / 3600.0f;
        if (locMatch.captured(5) == "W") {
            m_longitude = -m_longitude;
        }
        m_positionValid = true;
    }
    else
    {
        // Or decimal, e.g. +38.9603,-077.4595
        QRegularExpression locDecRe(R"(/LOC\s+([+-]\d{1,2}\.\d+),\s*([+-]\d{1,3}\.\d+))");
        locMatch = locDecRe.match(text);
        if (locMatch.hasMatch())
        {
            m_latitude = locMatch.captured(1).toFloat();
            m_longitude = locMatch.captured(2).toFloat();
            m_positionValid = true;
        }
    }
    return true;
}

QString AcarsEventReport::toString()
{
    if (m_eventDescription.isEmpty()) {
        return "";
    }
    // The destination and ETA form, which is not an event and has no event name to look
    // up below - see decode()
    if (m_eta.isValid())
    {
        QStringList lines;
        lines.append(QString("<b>Destination:</b> %1").arg(icaoAndAirportName(m_arrivalAirport)));
        if (m_eventTime.isValid()) {
            lines.append(QString("<b>Time:</b> %1").arg(m_eventTime.toString("hh:mm")));
        }
        lines.append(QString("<b>ETA:</b> %1").arg(m_eta.toString("hh:mm")));
        return lines.join("<br>");
    }
    const QHash<int, QString> events = {
        {13, "Out of gate"},
        {14, "Takeoff"},
        {15, "Landing"},
        {16, "In gate"},
        {17, "Post report"},
        {18, "Post times report"}
    };
    QStringList lines;
    lines.append(QString("<b>Event:</b> %1 (%2)").arg(events[m_event]).arg(m_eventDescription));
    lines.append(QString("<b>Departure:</b> %1").arg(icaoAndAirportName(m_departureAirport)));
    lines.append(QString("<b>Arrival:</b> %1").arg(icaoAndAirportName(m_arrivalAirport)));
    if (m_eventTime.isValid()) {
        lines.append(QString("<b>Time:</b> Day %1 %2").arg(m_day).arg(m_eventTime.toString("hh:mm:ss")));
    }
    if (m_fuelOnBoard >= 0) {
        lines.append(QString("<b>Fuel on board:</b> %1 t").arg(m_fuelOnBoard / 10.0, 0, 'f', 1));
    }
    if (m_positionValid) {
        lines.append(QString("<b>Position:</b> %1").arg(positionString(m_latitude, m_longitude)));
    }
    return lines.join("<br>");
}

bool AcarsEventReport::getPosition(float& latitude, float& longitude, int& altitudeFt) const
{
    if (!m_positionValid) {
        return false;
    }
    latitude = m_latitude;
    longitude = m_longitude;
    altitudeFt = -1;
    return true;
}

AcarsOpsReport::AcarsOpsReport(const QByteArray &message) :
    AcarsMessage(message)
{
}

// Coordinates in label 80 reports appear as N29395 (degrees x1000) or N3539.2
// (degrees then decimal minutes)
static bool parseOpsCoordinate(const QString& s, float& value)
{
    QRegularExpression plainRe(R"(^([NSEW])(\d+)$)");
    QRegularExpressionMatch match = plainRe.match(s);
    if (match.hasMatch())
    {
        int degDigits = match.captured(2).size() - 3;
        value = match.captured(2).left(degDigits).toFloat()
                + match.captured(2).mid(degDigits).toFloat() / 1000.0f;
        if ((match.captured(1) == "S") || (match.captured(1) == "W")) {
            value = -value;
        }
        return true;
    }
    QRegularExpression minRe(R"(^([NSEW])(\d+)(\d\d\.\d)$)");
    match = minRe.match(s);
    if (match.hasMatch())
    {
        value = match.captured(2).toFloat() + match.captured(3).toFloat() / 60.0f;
        if ((match.captured(1) == "S") || (match.captured(1) == "W")) {
            value = -value;
        }
        return true;
    }
    return false;
}

// Airline operational reports, e.g:
// 3N01 POSRPT 5891/04 KIAH/MMGL .XA-VOI
// /POS N29395W095133/ALT +15608/MCH 558/FOB 0100/ETA 0410
// with OPNORM and INRANG variants, and optional /FL /TAS /SAT /HDG /NWYP /SPD /UTC
// keyed fields. Formats per airframes.io's decoder test vectors.
bool AcarsOpsReport::decode(const QString &text)
{
    QRegularExpression hdrRe(R"(^3\w0\d\s+(POSRPT|OPNORM|INRANG)\s+(\w+)/(\d{1,2})\s+([A-Z0-9]{4})/([A-Z0-9]{4})\s+\.([A-Z0-9-]+))");
    QRegularExpressionMatch match = hdrRe.match(text);
    if (!match.hasMatch())
    {
        qDebug() << "AcarsOpsReport: Failed to match " << text;
        return false;
    }
    m_reportType = match.captured(1);
    m_reportFlight = match.captured(2);
    m_day = match.captured(3).toInt();
    m_departureAirport = match.captured(4);
    m_arrivalAirport = match.captured(5);
    m_tail = match.captured(6);

    // Keyed fields anywhere in the remainder
    auto keyed = [&text](const char *pattern) -> QString
    {
        QRegularExpression re{QString(pattern)};
        QRegularExpressionMatch m = re.match(text);
        return m.hasMatch() ? m.captured(1) : QString();
    };

    QString pos = keyed(R"(/POS\s+([NS][\d.]+)\s?([EW][\d.]+))");
    if (!pos.isEmpty())
    {
        QRegularExpression posRe(R"(/POS\s+([NS][\d.]+)\s?([EW][\d.]+))");
        QRegularExpressionMatch posMatch = posRe.match(text);
        float lat, lon;
        if (parseOpsCoordinate(posMatch.captured(1), lat) && parseOpsCoordinate(posMatch.captured(2), lon))
        {
            m_latitude = lat;
            m_longitude = lon;
            m_positionValid = true;
        }
    }
    QString alt = keyed(R"(/ALT\s+\+?(\d+))");
    if (!alt.isEmpty()) {
        m_altitudeFt = alt.toInt();
    }
    QString fl = keyed(R"(/FL\s+(\d+))");
    if (!fl.isEmpty()) {
        m_altitudeFt = fl.toInt() * 100;
    }
    m_nextWaypoint = keyed(R"(/NWYP\s+(\w+))");
    QString hdg = keyed(R"(/HDG\s+(\d+))");
    if (!hdg.isEmpty()) {
        m_headingDeg = hdg.toInt();
    }
    QString mch = keyed(R"(/MCH\s+(\d+))");
    if (!mch.isEmpty()) {
        m_machX1000 = mch.toInt();
    }
    QString spd = keyed(R"(/(?:TAS|SPD)\s+(\d+))");
    if (!spd.isEmpty()) {
        m_speedKts = spd.toInt();
    }
    QString sat = keyed(R"(/SAT\s+([+-]?\d+))");
    if (!sat.isEmpty())
    {
        m_temperatureC = sat.toInt();
        m_temperatureValid = true;
    }
    m_fuelOnBoard = keyed(R"(/FOB\s+([N\d.]+))");
    QString utc = keyed(R"(/UTC\s+(\d{6}))");
    if (!utc.isEmpty()) {
        m_time = QTime(utc.mid(0, 2).toInt(), utc.mid(2, 2).toInt(), utc.mid(4, 2).toInt());
    }
    QString eta = keyed(R"(/ETA\s+(\d\d):?(\d\d))");
    if (!eta.isEmpty())
    {
        QRegularExpression etaRe(R"(/ETA\s+(\d\d):?(\d\d))");
        QRegularExpressionMatch etaMatch = etaRe.match(text);
        m_eta = QTime(etaMatch.captured(1).toInt(), etaMatch.captured(2).toInt());
    }
    return true;
}

QString AcarsOpsReport::toString()
{
    if (m_reportType.isEmpty()) {
        return "";
    }
    const QHash<QString, QString> reportTypes = {
        {"POSRPT", "Position report"},
        {"OPNORM", "Operations normal"},
        {"INRANG", "In range"}
    };
    QStringList lines;
    lines.append(QString("<b>Report:</b> %1").arg(reportTypes.value(m_reportType, m_reportType)));
    lines.append(QString("<b>Flight:</b> %1").arg(m_reportFlight));
    lines.append(QString("<b>Aircraft:</b> %1").arg(m_tail));
    lines.append(QString("<b>Departure:</b> %1").arg(icaoAndAirportName(m_departureAirport)));
    lines.append(QString("<b>Arrival:</b> %1").arg(icaoAndAirportName(m_arrivalAirport)));
    if (m_positionValid) {
        lines.append(QString("<b>Position:</b> %1").arg(positionString(m_latitude, m_longitude)));
    }
    if (m_altitudeFt >= 0) {
        lines.append(QString("<b>Altitude:</b> %1 ft").arg(m_altitudeFt));
    }
    if (!m_nextWaypoint.isEmpty()) {
        lines.append(QString("<b>Next waypoint:</b> %1").arg(m_nextWaypoint));
    }
    if (m_headingDeg >= 0) {
        lines.append(QString("<b>Heading:</b> %1%2").arg(m_headingDeg).arg(QChar(0xB0)));
    }
    if (m_machX1000 >= 0) {
        lines.append(QString("<b>Mach:</b> %1").arg(m_machX1000 / 1000.0, 0, 'f', 3));
    }
    if (m_speedKts >= 0) {
        lines.append(QString("<b>Speed:</b> %1 kts").arg(m_speedKts));
    }
    if (m_temperatureValid) {
        lines.append(QString("<b>Temperature:</b> %1%2C").arg(m_temperatureC).arg(QChar(0xB0)));
    }
    if (!m_fuelOnBoard.isEmpty()) {
        lines.append(QString("<b>Fuel on board:</b> %1").arg(m_fuelOnBoard));
    }
    if (m_time.isValid()) {
        lines.append(QString("<b>Time:</b> Day %1 %2").arg(m_day).arg(m_time.toString("hh:mm:ss")));
    }
    if (m_eta.isValid()) {
        lines.append(QString("<b>ETA:</b> %1").arg(m_eta.toString("hh:mm")));
    }
    return lines.join("<br>");
}

bool AcarsOpsReport::getPosition(float& latitude, float& longitude, int& altitudeFt) const
{
    if (!m_positionValid) {
        return false;
    }
    latitude = m_latitude;
    longitude = m_longitude;
    altitudeFt = m_altitudeFt;
    return true;
}

bool ARINC622::decode(const QString &text, bool uplink)
{
    QRegularExpression re(R"(\/(?:\w\w )?(\w*)\.(\w\w\d)([\.\w\-]{7})(.*)(\w\w\w\w))");
    QRegularExpressionMatch match = re.match(text.simplified());
    if (match.hasMatch())
    {
        m_atcCenter = match.captured(1);
        m_imi = match.captured(2);
        m_aircraft = match.captured(3);
        while (m_aircraft.startsWith(".")) {
            m_aircraft = m_aircraft.mid(1); // Remove "." prefix
        }
        // m_aircraft should be checked against ACARS registration field to ensure they match
        m_app = match.captured(4);
        if ((m_imi == "AT1") || (m_imi == "CR1"))
        {
            DO219Decoder decoder;
            m_do219 = decoder.decode(m_app, uplink);
        }

        m_crc = match.captured(5).toInt(nullptr, 16);
        m_valid = true;
        return true;
    }
    else
    {
        return false;
    }
}

QString ARINC622::toString()
{
    if (m_valid)
    {
        QString s = QString("<b>ATC Center:</b> %1<br><b>IMI:</b> %2<br><b>Aircraft:</b> %3<br><b>CRC:</b> %4")
                    .arg(m_atcCenter)
                    .arg(m_imis[m_imi])
                    .arg(m_aircraft)
                    .arg(QString::number(m_crc, 16));

        if (!m_do219.isEmpty()) {
            s.append(QString("<br><b>CPDLC decode:</b> %1").arg(m_do219));
        } else  if (!m_app.isEmpty()) {
            s.append(QString("<br><b>CPDLC data:</b> %1").arg(m_app));
        }
        return s;
    }
    else
    {
        return "";
    }
}

AcarsATCCommunications::AcarsATCCommunications(const QByteArray &message) :
    AcarsMessage(message)
{
}

bool AcarsATCCommunications::decode(const QString &text)
{
    return m_622.decode(text, m_uplink);
}

QString AcarsATCCommunications::toString()
{
    return m_622.toString();
}

AcarsMessageFromSubsystem::AcarsMessageFromSubsystem(const QByteArray &message) :
    AcarsMessage(message)
{
}

// The FMS's own predicted route, out of the /FH block a position report carries in its
// additional data.
//
// Worth having because the route field of the same message is often nearly empty - one
// real position report gave "ILSZ 10 VILNA", an approach and a single fix, while the FH
// block behind it listed 32 fixes all the way from the aircraft to the runway, each with
// the time, altitude, speed, wind and temperature the FMS expects there.
//
// The format is not documented anywhere I can find, so it was worked out from the data
// and every field checked against something independent. Records are separated by "."
// and each is exactly 11 comma separated fields:
//
//     SIT01,043946,249K,3800,162,M52,254071,456,N,174,22
//     name  eta    cas  alt  fuel oat wind  gs  ?  trk dist
//
//   eta    the message's own "next waypoint ETA" matched the first record's exactly
//   alt    tens of feet - 3800 is FL380, which is the altitude the report gives, and
//          the last record reads 14 for Alicante, whose elevation is 141 ft
//   oat    M/P and degrees, M52 against the report's own -53 C at the fix before it
//   wind   three digits of direction and three of speed, moving smoothly along the route
//   gs     confirmed on every one of 27 consecutive pairs: distance divided by the gap
//          between the two ETAs comes out within a few percent of it
//   dist   nautical miles from the previous fix, or from the aircraft for the first
//
// Only the name, altitude and ETA are taken. The fixes are NOT positioned from track and
// distance, tempting as that is with both in hand: dead reckoned against the fixes the
// databases can place, it came out 13 nm adrift at the first and 73 nm by the last, so
// it would draw a route visibly beside the real one. The names are placed the usual way
// instead, and the ones no database holds - SIT01, IAT18, RW10, the FMS's own computed
// fixes - are simply dropped, as any unplaceable fix is.
// Fields in one record of a predicted route block, which is what identifies the block
static const int ACARS_FH_FIELDS = 11;

// The rest of the additional data a position report carries: slash separated blocks,
// most of them tagged with two letters. Worked out from 45 reports off air and each
// field checked against something the message says independently, or against the other
// blocks, rather than assumed.
//
//   239058,227 / DTLCPH,,67,183600,73 / PR1561,238,370,227,,44,7,237056,M49,60,P58,P0
//
// The leading block, which carries no tag, is the wind at the aircraft and the fuel
// remaining. The wind repeats inside the PR block, within a degree or two.
//
// DT is the destination:
//
//   DTLCPH , 19L  , 67   , 183600 , 73
//      icao  rwy    fuel   eta      ?
//
//   icao   agreed with the report's own arrival airport on all 38 that carried one
//   fuel   never more than the fuel remaining, on all 37 that could be compared, and on
//          a report that also carried a predicted route it equalled the last fix's fuel
//   eta    a valid hh:mm:ss on all 38, and equal to the predicted route's arrival at the
//          runway on the report that had both
//
// PR is the flight as planned. Its third field is the CRUISE level, not the current
// altitude: it was at or above the reported altitude on all 41, and one report climbing
// out of Heathrow at 3800 ft gave 390. Field 9 is a temperature, within about 4 C of the
// reported one - close but not the same figure, so it is forecast rather than measured,
// and it is not used. The first field (PR1561) is probably a weight, but nothing here
// confirms it, so it is left alone.
void AcarsMessageFromSubsystem::extractAdditionalBlocks(const QString& additional)
{
    if (additional.isEmpty()) {
        return;
    }

    const QStringList blocks = additional.split('/');

    for (int i = 0; i < blocks.size(); i++)
    {
        const QString& block = blocks[i];
        const QStringList fields = block.split(',');

        // The wind and the fuel, in the block before any tag
        if ((i == 0) && (fields.size() >= 2))
        {
            static const QRegularExpression windRe(R"(^([0-9]{3})([0-9]{2,3})$)");
            const QRegularExpressionMatch wind = windRe.match(fields[0].trimmed());
            if (wind.hasMatch())
            {
                m_windValid = true;
                m_windDirection = wind.captured(1).toInt();
                m_windSpeed = wind.captured(2).toInt();
            }
            bool ok = false;
            const int fuel = fields[1].trimmed().toInt(&ok);
            if (ok)
            {
                m_fuelValid = true;
                m_fuel = fuel;
            }
            continue;
        }

        if (block.startsWith("FN"))
        {
            // Runs to the next block or, when it is the last, to the four character
            // checksum the FMC puts on the end - "/FNUAL972C744" is UAL972, not
            // UAL972C744. Taken as three letters and up to four alphanumerics, which is
            // what a callsign is, so a checksum run together with it is not swallowed.
            static const QRegularExpression callsignRe(R"RX(^FN([A-Z]{3}[A-Z0-9]{1,4}))RX");
            const QRegularExpressionMatch callsign = callsignRe.match(block);
            if (callsign.hasMatch()) {
                m_icaoCallsign = callsign.captured(1);
            }
            continue;
        }

        if (block.startsWith("DT") && (fields.size() >= 4))
        {
            m_destination = fields[0].mid(2).trimmed();
            m_destinationRunway = stripRunwayFiller(fields[1].trimmed());
            bool ok = false;
            const int fuel = fields[2].trimmed().toInt(&ok);
            if (ok)
            {
                m_destinationFuelValid = true;
                m_destinationFuel = fuel;
            }
            const QString eta = fields[3].trimmed();
            if (eta.size() == 6)
            {
                const QTime time(eta.mid(0, 2).toInt(), eta.mid(2, 2).toInt(),
                                 eta.mid(4, 2).toInt());
                if (time.isValid())
                {
                    m_destinationEtaValid = true;
                    m_destinationEta = time;
                }
            }
        }
        else if (block.startsWith("PR") && (fields.size() >= 3))
        {
            bool ok = false;
            const int level = fields[2].trimmed().toInt(&ok);
            // A flight level, so hundreds of feet and not a silly one
            if (ok && (level > 0) && (level <= 600))
            {
                m_cruiseLevelValid = true;
                m_cruiseLevelFt = level * 100;
            }
        }
    }

    // A route from an insert names its fixes and nothing else, so the line is drawn flat
    // on the ground. The cruise level is the one altitude the message does give, and it
    // is right for the fixes in the middle of the route - but not for the ends, which
    // are the airports the flight runs between, and not for a fix a predicted route has
    // already given a real altitude.
    if (m_cruiseLevelValid)
    {
        for (int i = 0; i < m_fpnWaypoints.size(); i++)
        {
            FpnWaypoint& waypoint = m_fpnWaypoints[i];
            // By name, not by position: an airport is four characters, and skipping the
            // first and last entries would drop two perfectly good fixes from a list that
            // does not have to start or end with one
            if (!waypoint.m_hasAltitude && (waypoint.m_name.size() != 4))
            {
                waypoint.m_hasAltitude = true;
                waypoint.m_altitudeFt = m_cruiseLevelFt;
            }
        }
    }
}

bool AcarsMessageFromSubsystem::extractPredictedRoute(const QString& additional)
{
    const int at = additional.indexOf("/FH");
    if (at < 0) {
        return false;
    }
    QString block = additional.mid(at + 3);
    const int end = block.indexOf('/');
    if (end >= 0) {
        block = block.left(end);
    }

    // Digits only. Long messages come out of reassembly with the odd character inserted
    // into a field - one had a stray B every 207 characters, in the middle of numbers
    // that were otherwise correct - and a whole fix should not be lost to one of them.
    auto number = [](const QString& field, bool *ok)
    {
        QString digits;
        for (const QChar& c : field)
        {
            if (c.isDigit()) {
                digits.append(c);
            }
        }
        return digits.isEmpty() ? (*ok = false, 0) : (*ok = true, digits.toInt());
    };

    QList<FpnWaypoint> waypoints;
    QStringList names;

    for (const QString& record : block.split('.', Qt::SkipEmptyParts))
    {
        const QStringList fields = record.split(',');
        // Exactly 11, which is what makes this block recognisable rather than guessed at
        if (fields.size() != ACARS_FH_FIELDS) {
            continue;
        }
        const QString name = fields[0].trimmed();
        if (name.isEmpty()) {
            continue;
        }

        FpnWaypoint waypoint;
        waypoint.m_name = name;

        bool ok = false;
        const int altitude = number(fields[3], &ok);
        if (ok)
        {
            waypoint.m_hasAltitude = true;
            waypoint.m_altitudeFt = altitude * 10;
        }
        const QString eta = fields[1].trimmed();
        if (eta.size() == 6)
        {
            const QTime time(eta.mid(0, 2).toInt(), eta.mid(2, 2).toInt(), eta.mid(4, 2).toInt());
            if (time.isValid())
            {
                waypoint.m_hasEta = true;
                waypoint.m_eta = time;
            }
        }

        waypoints.append(waypoint);
        names.append(name);
    }

    if (waypoints.size() < 2) {
        return false;
    }

    // Only where it says more than what is already held. The route field of a position
    // report is whatever the FMS put in it, and on some aircraft that IS the full path -
    // this must not throw that away for a predicted one.
    const int held = m_fpnRoute.split(QRegularExpression("[ .]+"), Qt::SkipEmptyParts).size();
    if ((int) names.size() <= held) {
        return false;
    }

    m_fpnRoute = names.join(" ");
    m_fpnWaypoints = waypoints;
    return true;
}

bool AcarsMessageFromSubsystem::decode(const QString &text)
{
    if (m_622.decode(text, m_uplink)) {
        return true;
    }

    // Not an ARINC 622 ATS message: try the FMS report formats, which start with a
    // #<sublabel> prefix such as #M1B (FMC), e.g:
    // #M1BPOSN43312W123174,EASON,215754,370,EBINY,220601,ELENN,M48,02216,185/TS215754,092122
    // #M1BFPN/RP:DA:KSFO:AA:KEWR:R:...
    // Field meanings per airframes.io's acars-message-documentation research
    QString t = text;
    QRegularExpression subRe(R"(^#(\w{2}[A-Z]?))");
    QRegularExpressionMatch subMatch = subRe.match(t);
    if (subMatch.hasMatch())
    {
        m_fmsSublabel = subMatch.captured(1);
        t = t.mid(subMatch.capturedLength());
    }
    else if (!t.startsWith("POS") && !t.startsWith("FPN/")
             && (t.mid(1).startsWith("POS") || t.mid(1).startsWith("FPN/")))
    {
        // The header parse strips a #xx sublabel from the text, which can leave just
        // the supplementary character (the B of #M1B) before the report type - this
        // is the form the multipart assembler's combined text takes
        m_fmsSublabel = m_subLabel + t.left(1);
        t = t.mid(1);
    }
    else if (!m_subLabel.isEmpty())
    {
        // The prefix was consumed by the block header parse, supplementary character and
        // all, so the text starts straight at the report and the sublabel comes from
        // there instead of from in front of it
        m_fmsSublabel = m_subLabel + m_subLabelSupplement;
    }

    // A route insert - the flight plan the FMS is flying - as ARINC 702A writes it:
    // colon separated fields in an /RP: block, appended to a position report or sent on
    // its own as an FPN message. The /RA: alternate block has the same shape.
    //
    //   /RP:DA:EGLL:AA:ENGM:R:27L(01R):D:BPK7G:A:RIPA3L:AP:ILSY01R.VABLPU..BPK,
    //   N51450W000064.Q295.PAAVO..PAAVO,N51518E000513.M604.GIVPO..LARGA,N54518E004092
    //
    // DA is the departure airport, AA the arrival, R the departure runway with the
    // arrival's in brackets, D the SID, A the STAR and AP the lateral path. In the path
    // "." separates a fix from the airway to the next one and ".." is a direct leg, and
    // a fix may be followed by its position. R used to be read as the route, which is
    // why a plan only ever plotted as its two airports: it is the runway.
    auto extractFlightPlan = [this](const QString& s)
    {
        // Read within the /RP: block where there is one, so the /RA: alternate block's
        // own DA and AA are not taken for this flight's
        auto block = [](const QString& text, const QString& tag) -> QString
        {
            const int start = text.indexOf(tag + ":");
            if (start < 0) {
                return QString();
            }
            const int end = text.indexOf('/', start);
            return (end >= 0) ? text.mid(start, end - start) : text.mid(start);
        };
        auto field = [](const QString& text, const char *tag) -> QString
        {
            if (text.isEmpty()) {
                return QString();
            }
            const QRegularExpression re(QString(":%1:([^:/]*)").arg(tag));
            const QRegularExpressionMatch match = re.match(text);
            return match.hasMatch() ? match.captured(1).trimmed() : QString();
        };

        const QString rpBlock = block(s, "RP");
        const QString rp = rpBlock.isEmpty() ? s : rpBlock;

        // An airport is four characters, and a field runs to the next : or / - so
        // anything an airline writes after the code comes back stuck to it. Two forms
        // turn up: the route written straight after the arrival, :AA:HEGN..DVR.UL9.., and
        // the landing runway in brackets, :AA:ESSA(19L). Both left an "airport" that no
        // database could match and that nothing would treat as an airport, so the
        // arrival went missing from the tables and the Map, and the route with it.
        //
        // The trailing character has to be one an ICAO code cannot contain, or a longer
        // identifier would be cut down to a valid looking code that is not the one meant.
        auto airport = [](const QString& value, QString *bracketed = nullptr)
        {
            // Four characters, then either something that is not part of a code or the
            // end of the field. The end matters because the checksum can be welded on
            // with nothing in between - an alternate of KMKE86AC is KMKE with 86AC after
            // it - and an ICAO code is exactly four, so anything beyond that is not part
            // of it.
            static const QRegularExpression airportRe(R"(^([A-Z0-9]{4})(?:[^A-Z0-9]|$))");
            QRegularExpressionMatch match = airportRe.match(value);
            if (!match.hasMatch())
            {
                static const QRegularExpression codeAndTailRe(R"(^([A-Z0-9]{4})[A-Z0-9]+$)");
                match = codeAndTailRe.match(value);
            }
            if (!match.hasMatch()) {
                return value;
            }
            // The runway is worth keeping where it is the only place it is given
            static const QRegularExpression runwayRe(R"(^[A-Z0-9]{4}\(([0-9]{2}[LRC]?)\))");
            const QRegularExpressionMatch runway = runwayRe.match(value);
            if (bracketed && runway.hasMatch()) {
                *bracketed = runway.captured(1);
            }
            return match.captured(1);
        };

        QString arrivalRunway;
        m_fpnDeparture = airport(field(rp, "DA"));
        m_fpnArrival = airport(field(rp, "AA"), &arrivalRunway);
        m_fpnRunway = field(rp, "R");
        if (m_fpnRunway.isEmpty()) {
            m_fpnRunway = arrivalRunway;
        }
        m_fpnRunway = stripRunwayFiller(m_fpnRunway);
        m_fpnSid = field(rp, "D");
        m_fpnStar = field(rp, "A");
        // Through the same helper as the other two airports. It was taken raw, which
        // was invisible until a message put its checksum straight onto the end of it -
        // the alternate block is the last thing in the message, so it is the field that
        // collects one - and the alternate read KMKE86AC.
        m_fpnAlternate = airport(field(block(s, "RA"), "AA"));

        QString path = field(rp, "AP");
        if (path.isEmpty()) {
            path = field(rp, "F");
        }
        if (path.isEmpty())
        {
            // Some aircraft hang the path straight off the arrival airport with no field
            // of its own in front of it:
            //
            //   FPN/RP:DA:EGPH:AA:EHAM..TLA,N55299W003212.Y96..OTBUN..ERKIT.N110..DOLAS
            //
            // The arrival is read off the front of that field either way - it stops at
            // the first character that is not part of an ICAO code - so what is left after
            // it was simply being thrown away, and a route written this way drew nothing
            // at all between its two airports.
            //
            // Only when it is a path rather than a decoration: it has to start where the
            // airport code ends and contain a separator, which "EHAM" alone and
            // "ESSA(19L)" both fail.
            const QString arrivalField = field(rp, "AA");
            if (arrivalField.startsWith(m_fpnArrival) && !m_fpnArrival.isEmpty())
            {
                const QString rest = arrivalField.mid(m_fpnArrival.size());
                if (rest.startsWith('.')) {
                    path = rest;
                }
            }
        }
        // N51450 is 51 degrees 45.0 minutes north and W000064 is 0 degrees 6.4 minutes
        // west: three digits of minutes of which the last is tenths, after two digits of
        // degrees for latitude and three for longitude
        // Not anchored at the end. The coordinate is thirteen characters and always the
        // same shape, and the message writes its own checksum onto whatever field comes
        // last - so the final fix of a route arrives as N37067W0814527814, which is
        // N37067W081452 with 7814 stuck to it. Anchored, that matched nothing and the
        // last fix of the route lost its position for the sake of four characters.
        static const QRegularExpression coordRe(R"(^([NS])(\d{2})(\d{3})([EW])(\d{3})(\d{3}))");
        const QStringList tokens = path.split('.', Qt::SkipEmptyParts);

        // Everything before the first fix that carries a position is the approach and
        // its transition: the path proper is the part that is placed. Where nothing is
        // placed there is nothing to separate them by, and it is all route.
        int first = 0;
        while ((first < tokens.size())
               && !coordRe.match(tokens[first].section(',', 1, 1).trimmed()).hasMatch()
               && !coordRe.match(tokens[first].section(',', 0, 0).trimmed()).hasMatch()) {
            first++;
        }
        if (first >= tokens.size()) {
            first = 0;
        } else if (first > 0) {
            m_fpnApproach = QStringList(tokens.mid(0, first)).join(" ");
        }

        for (int i = first; i < tokens.size(); i++)
        {
            const QString name = tokens[i].section(',', 0, 0).trimmed();
            if (name.isEmpty()) {
                continue;
            }
            // Usually NAME,POSITION - but a fix out over an ocean has no name to give, so
            // the position IS the fix and stands on its own. An oceanic route is mostly
            // made of them:
            //
            //   BERGI.M90..LONAM..NALAN..N62000W010000..N63300W020000..N65000W030000..
            //
            // Read as a name, each of those resolves against nothing and is dropped, which
            // took the whole ocean crossing out of the drawn line and left it jumping from
            // the last European fix to the first Canadian one.
            const bool bare = coordRe.match(name).hasMatch();
            const QRegularExpressionMatch coord = bare
                ? coordRe.match(name)
                : coordRe.match(tokens[i].section(',', 1, 1).trimmed());

            FpnWaypoint waypoint;
            waypoint.m_name = name;
            if (coord.hasMatch())
            {
                waypoint.m_hasPosition = true;
                waypoint.m_latitude = (coord.captured(2).toFloat()
                    + coord.captured(3).toFloat() / 600.0f) * (coord.captured(1) == "S" ? -1.0f : 1.0f);
                waypoint.m_longitude = (coord.captured(5).toFloat()
                    + coord.captured(6).toFloat() / 600.0f) * (coord.captured(4) == "W" ? -1.0f : 1.0f);
            }

            // A fix is usually given twice - once as the airway's far end and again
            // with its position - so a name that has just repeated replaces rather than
            // extends, keeping whichever of the two placed it
            if (!m_fpnWaypoints.isEmpty() && (m_fpnWaypoints.last().m_name == name))
            {
                if (waypoint.m_hasPosition) {
                    m_fpnWaypoints.last() = waypoint;
                }
                continue;
            }
            m_fpnWaypoints.append(waypoint);
        }

        QStringList names;
        for (const FpnWaypoint& waypoint : m_fpnWaypoints) {
            names.append(waypoint.m_name);
        }
        m_fpnRoute = names.join(" ");

        // Older or airline specific forms write the route as free text after the
        // arrival airport instead of as a path
        if (m_fpnRoute.isEmpty() && !m_fpnArrival.isEmpty())
        {
            static const QRegularExpression tailRe(R"(:AA:\w{4}\.+(.+)$)");
            const QRegularExpressionMatch tailMatch = tailRe.match(s);
            if (tailMatch.hasMatch()) {
                m_fpnRoute = tailMatch.captured(1).trimmed();
            }
        }

        // A plan is filed in more than one insert: one carries the departure end and the
        // en route path, another the arrival end - the STAR and the approach - and that
        // one writes the approach where the path would be. The rule above cannot tell
        // them apart, because it separates the approach from the path by the first fix
        // that carries a position, and an insert like this places nothing.
        //
        // An approach names its aid and its runway, which a route never does, so the
        // whole "path" being one of those is what identifies it. Called the route
        // instead, it put "ILS 19L" in the Route column and left the Aircraft feature
        // choosing between a path it could draw and one it could not.
        static const QRegularExpression approachRe(
            R"(^(ILS|LOC|LDA|SDF|RNAV|RNP|GPS|GNSS|VOR|VORDME|NDB|TACAN|VISUAL)[ .]?([0-9]{2}[LRC]?)?$)");
        if (m_fpnApproach.isEmpty() && !m_fpnRoute.isEmpty()
            && approachRe.match(m_fpnRoute).hasMatch())
        {
            m_fpnApproach = m_fpnRoute;
            m_fpnRoute.clear();
            m_fpnWaypoints.clear();
        }
    };

    // Position report: position, last waypoint and time over it, altitude in flight
    // levels, next waypoint and its ETA, the waypoint after, and the static air
    // temperature. Latitude and longitude are degrees x1000.
    QRegularExpression posRe(R"(^POS([NS])(\d{4,5})([EW])(\d{5,6}),([^,]*),(\d{6}),(\d+),([^,]*),(\d{6}),([^,]*),([MP])\s?(\d+)(.*)$)");
    QRegularExpressionMatch posMatch = posRe.match(t);
    if (posMatch.hasMatch())
    {
        m_latitude = posMatch.captured(2).toFloat() / 1000.0f * (posMatch.captured(1) == "S" ? -1.0f : 1.0f);
        m_longitude = posMatch.captured(4).toFloat() / 1000.0f * (posMatch.captured(3) == "W" ? -1.0f : 1.0f);
        m_positionValid = true;
        m_lastWaypoint = posMatch.captured(5);
        QString ts = posMatch.captured(6);
        m_timeOver = QTime(ts.mid(0, 2).toInt(), ts.mid(2, 2).toInt(), ts.mid(4, 2).toInt());
        m_altitudeFt = posMatch.captured(7).toInt() * 100;
        m_nextWaypoint = posMatch.captured(8);
        ts = posMatch.captured(9);
        m_etaNext = QTime(ts.mid(0, 2).toInt(), ts.mid(2, 2).toInt(), ts.mid(4, 2).toInt());
        m_nextPlus1Waypoint = posMatch.captured(10);
        m_temperatureC = posMatch.captured(12).toInt();
        if (posMatch.captured(11) == "M") {
            m_temperatureC = -m_temperatureC;
        }
        m_temperatureValid = true;
        QString additional = posMatch.captured(13);
        if (additional.startsWith(",")) {
            additional = additional.mid(1);
        }
        m_posAdditionalData = additional.trimmed();
        // A position report can carry a route insert after the report fields; it is
        // decoded into the flight plan fields above, so the several hundred characters
        // it occupies are dropped from the raw remainder rather than shown twice. Cut
        // out just those blocks: what follows them is a different subsystem's data.
        extractFlightPlan(m_posAdditionalData);
        // And the FMS's predicted route, which is a different block and usually says far
        // more about where the aircraft is going than the route field does
        extractPredictedRoute(m_posAdditionalData);
        // Last, because it fills in the altitude of whatever waypoints the blocks above
        // produced
        extractAdditionalBlocks(m_posAdditionalData);
        static const QRegularExpression insertRe(R"(/R[PIA]:[^/]*)");
        m_posAdditionalData.remove(insertRe);
        m_posAdditionalData = m_posAdditionalData.trimmed();
        return true;
    }

    // Flight plan
    if (t.startsWith("FPN/"))
    {
        extractFlightPlan(t);
        return !m_fpnDeparture.isEmpty() || !m_fpnArrival.isEmpty() || !m_fpnRoute.isEmpty()
            || !m_fpnRunway.isEmpty() || !m_fpnSid.isEmpty() || !m_fpnStar.isEmpty();
    }

    return false;
}

QString AcarsMessageFromSubsystem::toString()
{
    if (m_622.m_valid) {
        return m_622.toString();
    }
    QStringList lines;
    bool hasFpn = !m_fpnDeparture.isEmpty() || !m_fpnArrival.isEmpty() || !m_fpnRoute.isEmpty()
                || !m_fpnRunway.isEmpty() || !m_fpnSid.isEmpty() || !m_fpnStar.isEmpty()
                || !m_fpnApproach.isEmpty() || !m_fpnAlternate.isEmpty();
    if (m_positionValid)
    {
        lines.append(QString("<b>Position:</b> %1").arg(positionString(m_latitude, m_longitude)));
        if (m_altitudeFt >= 0) {
            lines.append(QString("<b>Altitude:</b> %1 ft").arg(m_altitudeFt));
        }
        if (!m_lastWaypoint.isEmpty()) {
            lines.append(QString("<b>Last waypoint:</b> %1 at %2").arg(m_lastWaypoint).arg(m_timeOver.toString("hh:mm:ss")));
        }
        if (!m_nextWaypoint.isEmpty()) {
            lines.append(QString("<b>Next waypoint:</b> %1 ETA %2").arg(m_nextWaypoint).arg(m_etaNext.toString("hh:mm:ss")));
        }
        if (!m_nextPlus1Waypoint.isEmpty()) {
            lines.append(QString("<b>Then:</b> %1").arg(m_nextPlus1Waypoint));
        }
        if (m_temperatureValid) {
            lines.append(QString("<b>Temperature:</b> %1%2C").arg(m_temperatureC).arg(QChar(0xB0)));
        }
    }
    if (hasFpn)
    {
        // In the order the flight is flown, rather than the order the fields happen
        // to appear in the message
        if (!m_fpnDeparture.isEmpty()) {
            lines.append(QString("<b>Departure:</b> %1").arg(icaoAndAirportName(m_fpnDeparture)));
        }
        if (!m_fpnRunway.isEmpty()) {
            lines.append(QString("<b>Runway:</b> %1").arg(m_fpnRunway));
        }
        if (!m_fpnSid.isEmpty()) {
            lines.append(QString("<b>SID:</b> %1").arg(m_fpnSid));
        }
        if (!m_fpnRoute.isEmpty()) {
            lines.append(QString("<b>Route:</b> %1").arg(m_fpnRoute));
        }
        if (!m_fpnStar.isEmpty()) {
            lines.append(QString("<b>STAR:</b> %1").arg(m_fpnStar));
        }
        if (!m_fpnApproach.isEmpty()) {
            lines.append(QString("<b>Approach:</b> %1").arg(m_fpnApproach));
        }
        if (!m_fpnArrival.isEmpty()) {
            lines.append(QString("<b>Arrival:</b> %1").arg(icaoAndAirportName(m_fpnArrival)));
        }
        if (!m_fpnAlternate.isEmpty()) {
            lines.append(QString("<b>Alternate:</b> %1").arg(icaoAndAirportName(m_fpnAlternate)));
        }
    }
    if (m_windValid) {
        // The degree sign itself, not an HTML entity: the document is rendered as plain
        // text, so an entity would be shown literally
        lines.append(QString("<b>Wind:</b> %1%2 / %3 kn")
            .arg(m_windDirection, 3, 10, QChar('0')).arg(QChar(0xB0)).arg(m_windSpeed));
    }
    if (m_fuelValid) {
        lines.append(QString("<b>Fuel:</b> %1").arg(m_fuel));
    }
    if (m_cruiseLevelValid) {
        lines.append(QString("<b>Cruise level:</b> FL%1").arg(m_cruiseLevelFt / 100));
    }
    if (m_destinationEtaValid) {
        lines.append(QString("<b>Destination ETA:</b> %1")
            .arg(m_destinationEta.toString("hh:mm:ss")));
    }
    if (!m_destinationRunway.isEmpty()) {
        lines.append(QString("<b>Destination runway:</b> %1").arg(m_destinationRunway));
    }
    if (m_destinationFuelValid) {
        lines.append(QString("<b>Fuel on arrival:</b> %1").arg(m_destinationFuel));
    }
    if (m_positionValid && !m_posAdditionalData.isEmpty()) {
        lines.append(QString("<b>Additional data:</b> %1").arg(m_posAdditionalData));
    }
    if (lines.isEmpty()) {
        return "";
    }
    if (!m_fmsSublabel.isEmpty()) {
        lines.prepend(QString("<b>Subsystem:</b> %1").arg(m_fmsSublabel));
    }
    return lines.join("<br>");
}

bool AcarsMessageFromSubsystem::getPosition(float& latitude, float& longitude, int& altitudeFt) const
{
    if (!m_positionValid) {
        return false;
    }
    latitude = m_latitude;
    longitude = m_longitude;
    altitudeFt = m_altitudeFt;
    return true;
}

AcarsOutReport::AcarsOutReport(const QByteArray &message) :
    AcarsMessage(message)
{
}

bool AcarsOutReport::decode(const QString &text)
{
    m_departure = text.mid(0, 4);
    m_destination = text.mid(4, 4);
    m_outTimeHours = text.mid(8, 2).toInt();
    m_outTimeMinutes = text.mid(10, 2).toInt();
    m_fuel = text.mid(12, 4).toInt();
    m_boardedFuel = text.mid(16, 4); // Can be -----
    m_freeText = text.mid(20, text.length() - 1).trimmed();
    return true;
}

QString AcarsOutReport::toString()
{
    QString s = QString("<b>Departure:</b> %1<br><b><b>Destination:</b> %2<br><b><b>Out time:</b> %3:%4<br><b><b>Fuel:</b> %5<br><b><b>Boarded fuel:</b> %6")
                    .arg(icaoAndAirportName(m_departure))
                    .arg(icaoAndAirportName(m_destination))
                    .arg(m_outTimeHours, 2, 10, QChar('0'))
                    .arg(m_outTimeMinutes, 2, 10, QChar('0'))
                    .arg(m_fuel)
                    .arg(m_boardedFuel);
    if (!m_freeText.isEmpty()) {
        s.append(QString("<br><b>Free text:</b> %1").arg(m_freeText));
    }
    return s;
}

AcarsOffReport::AcarsOffReport(const QByteArray &message) :
    AcarsMessage(message)
{
}

bool AcarsOffReport::decode(const QString &text)
{
    m_departure = text.mid(0, 4);
    m_destination = text.mid(4, 4);
    m_offTimeHours = text.mid(8, 2).toInt();
    m_offTimeMinutes = text.mid(10, 2).toInt();
    m_freeText = text.mid(12, text.length() - 1).trimmed();
    return true;
}

QString AcarsOffReport::toString()
{
    QString s = QString("<b>Departure:</b> %1<br><b>Destination:</b> %2<br><b>Off time:</b> %3:%4")
                    .arg(icaoAndAirportName(m_departure))
                    .arg(icaoAndAirportName(m_destination))
                    .arg(m_offTimeHours, 2, 10, QChar('0'))
                    .arg(m_offTimeMinutes, 2, 10, QChar('0'));
    if (!m_freeText.isEmpty()) {
        s.append(QString("<br><b>Free text:</b> %1").arg(m_freeText));
    }
    return s;
}

AcarsOnReport::AcarsOnReport(const QByteArray &message) :
    AcarsMessage(message)
{
}

bool AcarsOnReport::decode(const QString &text)
{
    m_departure = text.mid(0, 4);
    m_destination = text.mid(4, 4);
    m_onTimeHours = text.mid(8, 2).toInt();
    m_onTimeMinutes = text.mid(10, 2).toInt();
    m_freeText = text.mid(12, text.length() - 1).trimmed();
    return true;
}

QString AcarsOnReport::toString()
{
    QString s = QString("<b>Departure:</b> %1<br><b>Destination:</b> %2<br><b>On time:</b> %3:%4")
                    .arg(icaoAndAirportName(m_departure))
                    .arg(icaoAndAirportName(m_destination))
                    .arg(m_onTimeHours, 2, 10, QChar('0'))
                    .arg(m_onTimeMinutes, 2, 10, QChar('0'));
    if (!m_freeText.isEmpty()) {
        s.append(QString("<br><b>Free text:</b> %1").arg(m_freeText));
    }
    return s;
}

AcarsInReport::AcarsInReport(const QByteArray &message) :
    AcarsMessage(message)
{
}

bool AcarsInReport::decode(const QString &text)
{
    m_departure = text.mid(0, 4);
    m_destination = text.mid(4, 4);
    m_inTimeHours = text.mid(8, 2).toInt();
    m_inTimeMinutes = text.mid(10, 2).toInt();
    m_fuel = text.mid(12, 4).toInt();
    m_captainFirstOfficerId = (text.length() > 16) ? text[16] : QChar();
    m_landingCategory = (text.length() > 17) ? text[17] : QChar();
    m_freeText = text.mid(18, text.length() - 1).trimmed();
    return true;
}

QString AcarsInReport::toString()
{
    QString s = QString("<b>Departure:</b> %1<br><b>Destination:</b> %2<br><b>In time:</b> %3:%4<br><b>Fuel:</b> %5<br>Id: %6<br><b>Landing category:</b> %7")
                    .arg(icaoAndAirportName(m_departure))
                    .arg(icaoAndAirportName(m_destination))
                    .arg(m_inTimeHours, 2, 10, QChar('0'))
                    .arg(m_inTimeMinutes, 2, 10, QChar('0'))
                    .arg(m_fuel)
                    .arg(m_captainFirstOfficerId)
                    .arg(m_landingCategory);
    if (!m_freeText.isEmpty()) {
        s.append(QString("<br><b>Free text:</b> %1").arg(m_freeText));
    }
    return s;
}

AcarsOutReturnInReport::AcarsOutReturnInReport(const QByteArray &message) :
    AcarsMessage(message)
{
}

bool AcarsOutReturnInReport::decode(const QString &text)
{
    m_departure = text.mid(0, 4);
    m_destination = text.mid(4, 4);
    m_outTimeHours = text.mid(8, 2).toInt();
    m_outTimeMinutes = text.mid(10, 2).toInt();
    m_returnInTimeHours = text.mid(12, 2).toInt();
    m_returnInTimeMinutes = text.mid(14, 2).toInt();
    m_fuel = text.mid(16, 4).toInt();
    m_freeText = text.mid(20, text.length() - 1).trimmed();
    return true;
}

QString AcarsOutReturnInReport::toString()
{
    QString s = QString("<b>Departure:</b> %1<br><b>Destination:</b> %2<br><b>Out time:</b> %3:%4<br><b>Return in time:</b> %5:%6<br><b>Fuel:</b> %7")
                    .arg(icaoAndAirportName(m_departure))
                    .arg(icaoAndAirportName(m_destination))
                    .arg(m_outTimeHours, 2, 10, QChar('0'))
                    .arg(m_outTimeMinutes, 2, 10, QChar('0'))
                    .arg(m_returnInTimeHours, 2, 10, QChar('0'))
                    .arg(m_returnInTimeMinutes, 2, 10, QChar('0'))
                    .arg(m_fuel);
    if (!m_freeText.isEmpty()) {
        s.append(QString("<br><b>Free text:</b> %1").arg(m_freeText));
    }
    return s;
}

// ARINC 620

AcarsShortOooiReport::AcarsShortOooiReport(const QByteArray &message) :
    AcarsMessage(message)
{
}

bool AcarsShortOooiReport::decode(const QString &text)
{
    // Which event this is comes from the label, not from the text - the text says only
    // where and when. QG reports two events at once, the departure and the return, and
    // is recorded as the return because that is the one that ends the movement.
    static const QHash<QString, int> events = {
        {"QA", AircraftReport::OooiOut},        // Out, with fuel
        {"QB", AircraftReport::OooiOff},
        {"QD", AircraftReport::OooiIn},         // In, with fuel
        {"QE", AircraftReport::OooiOut},        // Out, with fuel and destination
        {"QF", AircraftReport::OooiOff},        // Off, with destination
        {"QG", AircraftReport::OooiIn},         // Out and back in again
    };
    m_event = events.value(m_label, AircraftReport::OooiNone);

    const QString flat = text.split(QRegularExpression("[\r\n]")).first();

    // Station and time. The station is three characters and may be dashes; the time is
    // four digits run straight on to it.
    static const QRegularExpression headRe(R"(^([A-Z]{3}|-{3})(\d{2})(\d{2}))");
    const QRegularExpressionMatch head = headRe.match(flat);
    if (!head.hasMatch()) {
        return false;
    }
    const int hours = head.captured(2).toInt();
    const int minutes = head.captured(3).toInt();
    if ((hours > 23) || (minutes > 59)) {
        return false;       // Four digits that are not a time, so not this format
    }
    if (!head.captured(1).startsWith('-')) {
        m_station = head.captured(1);
    }
    m_time = QTime(hours, minutes);

    QString rest = flat.mid(head.capturedLength(0));

    // A second time immediately after the first, which QG and Q1 use for the return and
    // the arrival. Only where it really is a time - the other labels put fuel here.
    if ((m_label == "QG") || (m_label == "Q1"))
    {
        static const QRegularExpression secondRe(R"(^(\d{2})(\d{2}))");
        const QRegularExpressionMatch second = secondRe.match(rest);
        if (second.hasMatch()
            && (second.captured(1).toInt() <= 23) && (second.captured(2).toInt() <= 59))
        {
            m_secondTime = QTime(second.captured(1).toInt(), second.captured(2).toInt());
            rest = rest.mid(second.capturedLength(0));
        }
    }

    // The trailing blocks, taken off before anything else is read so that a code inside
    // one of them cannot be mistaken for the destination
    // The block code is two or three characters and is not always letters - /FB and
    // /FUL sit alongside /A1 and /A2 in the same message
    static const QRegularExpression extraRe(R"(/[A-Z][A-Z0-9]{1,2}\s*[0-9A-Z]*)");
    QRegularExpressionMatchIterator extras = extraRe.globalMatch(rest);
    int firstExtra = rest.size();
    while (extras.hasNext())
    {
        const QRegularExpressionMatch e = extras.next();
        m_extras.append(e.captured(0).trimmed());
        firstExtra = std::min(firstExtra, (int) e.capturedStart(0));
    }
    rest = rest.left(firstExtra);

    // The destination closes the fixed part, whether it comes straight after the time
    // (SAT1458SFO) or after the fuel (SYD0642       62MEL)
    static const QRegularExpression destRe(R"(([A-Z]{3})\s*$)");
    const QRegularExpressionMatch dest = destRe.match(rest);
    if (dest.hasMatch())
    {
        m_destination = dest.captured(1);
        rest = rest.left(dest.capturedStart(1));
    }

    // The numeric run between the time and the destination is the fuel on the three
    // labels registered as carrying it, and something else - a counter, a distance -
    // on the rest. Only the three are read: naming a number "fuel" because it sits
    // where fuel sits on another label is a guess, and a wrong one shows as a fact.
    const QString remainder = rest.trimmed();
    static const QRegularExpression fuelRe(R"(^\d+$)");
    const bool carriesFuel = (m_label == "QA") || (m_label == "QD") || (m_label == "QE");
    if (carriesFuel && fuelRe.match(remainder).hasMatch()) {
        m_fuel = remainder;
    }
    return true;
}

QString AcarsShortOooiReport::toString()
{
    if (!m_time.isValid()) {
        return "";
    }
    static const QHash<QString, QString> names = {
        {"QA", "Out of the gate, with fuel"},
        {"QB", "Takeoff"},
        {"QD", "On the gate, with fuel"},
        {"QE", "Out of the gate"},
        {"QF", "Takeoff"},
        {"QG", "Out of the gate and returned"},
        {"Q1", "Estimated departure and arrival"},
    };

    QStringList lines;
    lines.append(QString("<b>Report:</b> %1").arg(names.value(m_label, "Movement report")));
    if (!m_station.isEmpty()) {
        lines.append(QString("<b>Station:</b> %1").arg(m_station));
    }
    if (!m_destination.isEmpty()) {
        lines.append(QString("<b>Destination:</b> %1").arg(m_destination));
    }
    lines.append(QString("<b>Time:</b> %1").arg(m_time.toString("hh:mm")));
    if (m_secondTime.isValid())
    {
        lines.append(QString("<b>%1:</b> %2")
            .arg(m_label == "QG" ? "Returned in" : "Arrival")
            .arg(m_secondTime.toString("hh:mm")));
    }
    if (!m_fuel.isEmpty()) {
        lines.append(QString("<b>Fuel:</b> %1").arg(m_fuel));
    }
    for (const QString& extra : m_extras) {
        lines.append(QString("<b>Also:</b> %1").arg(extra));
    }
    return lines.join("<br>");
}

AcarsSquitter::AcarsSquitter(const QByteArray &message) :
    AcarsMessage(message)
{
}

AcarsMeteorologicalReport::AcarsMeteorologicalReport(const QByteArray &message) :
    AcarsMessage(message)
{
}

// Position in degrees and decimal minutes, time, flight level in tenths, temperature in
// tenths of a degree with M for minus, then the wind. Each observation ends in "G".
//
// Read out in order the observations describe a flight: one LEBL to EGKK message has the
// aircraft at 44.5 N climbing to 50.1 N over three quarters of an hour, descending from
// FL360 to FL267 at the end while the temperature rises from -51 to -40.5 and the wind
// falls from 109 knots to 47. That internal consistency is what says the field boundaries
// are in the right places; nothing else here would survive being read wrongly.
//
// 150 of 397 label H2 messages are this format. The rest are other things entirely -
// some of them binary - and are left alone.
bool AcarsMeteorologicalReport::decode(const QString &text)
{
    static const QRegularExpression headerRe(R"RX(^\S{5}([A-Z]{4})([A-Z]{4}))RX");
    const QRegularExpressionMatch header = headerRe.match(text);
    if (header.hasMatch())
    {
        m_departureAirport = header.captured(1);
        m_arrivalAirport = header.captured(2);
    }

    static const QRegularExpression observationRe(
        R"RX(([NS])(\d{2})(\d{3})([EW])(\d{3})(\d{3})(\d{2})(\d{2})(\d{4})([MP])(\d{3})(\d{3})(\d{3})G)RX");
    QRegularExpressionMatchIterator it = observationRe.globalMatch(text);
    while (it.hasNext())
    {
        const QRegularExpressionMatch match = it.next();
        Observation observation;
        // Minutes are tenths, so there are 600 of them to the degree
        observation.m_latitude = (match.captured(2).toFloat() + match.captured(3).toFloat() / 600.0f)
            * (match.captured(1) == "S" ? -1.0f : 1.0f);
        observation.m_longitude = (match.captured(5).toFloat() + match.captured(6).toFloat() / 600.0f)
            * (match.captured(4) == "W" ? -1.0f : 1.0f);
        observation.m_time = QTime(match.captured(7).toInt(), match.captured(8).toInt());
        observation.m_altitudeFt = (int) (match.captured(9).toFloat() * 10.0f);
        observation.m_temperatureC = match.captured(11).toFloat() / 10.0f
            * (match.captured(10) == "M" ? -1.0f : 1.0f);
        observation.m_windDirectionDeg = match.captured(12).toInt();
        observation.m_windSpeedKts = match.captured(13).toInt();
        m_observations.append(observation);
    }

    return !m_observations.isEmpty();
}

QString AcarsMeteorologicalReport::toString()
{
    if (m_observations.isEmpty()) {
        return "";
    }

    QStringList lines;
    if (!m_departureAirport.isEmpty() && !m_arrivalAirport.isEmpty())
    {
        lines.append(QString("<b>Route:</b> %1 to %2")
            .arg(icaoAndAirportName(m_departureAirport))
            .arg(icaoAndAirportName(m_arrivalAirport)));
    }
    for (const Observation& observation : m_observations)
    {
        lines.append(QString("<b>%1</b> %2%3 %4%5 at %6 ft, %7&#176;C, wind %8&#176; / %9 kn")
            .arg(observation.m_time.toString("hh:mm"))
            .arg(qAbs(observation.m_latitude), 0, 'f', 3)
            .arg(observation.m_latitude < 0.0f ? "S" : "N")
            .arg(qAbs(observation.m_longitude), 0, 'f', 3)
            .arg(observation.m_longitude < 0.0f ? "W" : "E")
            .arg(observation.m_altitudeFt)
            .arg(observation.m_temperatureC, 0, 'f', 1)
            .arg(observation.m_windDirectionDeg, 3, 10, QChar('0'))
            .arg(observation.m_windSpeedKts));
    }
    return lines.join("<br>");
}

AcarsMediaAdvisory::AcarsMediaAdvisory(const QByteArray &message) :
    AcarsMessage(message)
{
}

QString AcarsMediaAdvisory::mediaName(QChar code)
{
    switch (code.toLatin1())
    {
    case 'V': return "VHF ACARS";
    case '2': return "VDL Mode 2";
    case 'S': return "SATCOM";
    case 'X': return "SATCOM (MTSAT)";
    case 'H': return "HF";
    case 'G': return "SATCOM (GlobalStar)";
    case 'C': return "SATCOM (ICO)";
    case 'I': return "SATCOM (Iridium)";
    default: return QString(code);
    }
}

bool AcarsMediaAdvisory::decode(const QString &text)
{
    // Version, whether the link was established or lost, which link, the time it
    // happened, then every link still available and optional free text. Checked against
    // 200 consecutive label SA messages off air, all 200 of which match - which is what
    // makes it worth decoding strictly rather than showing the raw string.
    static const QRegularExpression re(
        R"(^(\d)([EL])([A-Z0-9])(\d{2})(\d{2})(\d{2})([A-Z0-9]*)(?:/(.*))?$)",
        QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch match = re.match(text.trimmed());
    if (!match.hasMatch()) {
        return false;
    }

    m_version = match.captured(1);
    m_established = (match.captured(2) == "E");
    m_media = mediaName(match.captured(3).at(0));
    m_time = QTime(match.captured(4).toInt(), match.captured(5).toInt(), match.captured(6).toInt());
    if (!m_time.isValid()) {
        return false;
    }
    for (const QChar &available : match.captured(7)) {
        m_availableMedia.append(mediaName(available));
    }
    m_freeText = match.captured(8).trimmed();

    return true;
}

QString AcarsMediaAdvisory::toString()
{
    if (!m_time.isValid()) {
        return "";
    }

    QStringList lines;
    lines.append(QString("<b>Link %1:</b> %2")
        .arg(m_established ? "established" : "lost")
        .arg(m_media));
    lines.append(QString("<b>Time:</b> %1").arg(m_time.toString("hh:mm:ss")));
    if (!m_availableMedia.isEmpty()) {
        lines.append(QString("<b>Available:</b> %1").arg(m_availableMedia.join(", ")));
    }
    if (!m_freeText.isEmpty()) {
        lines.append(QString("<b>Free text:</b> %1").arg(m_freeText));
    }

    return lines.join("<br>");
}

bool AcarsSquitter::decode(const QString &text)
{
    if (m_uplink)
    {
        m_version = text.mid(0, 2);
        m_serviceProvider = text.mid(2, 2);
        if (m_version == "00")
        {
            m_freeText = text.mid(4);
        }
        else
        {
            m_iataStationId = text.mid(4, 3);
            m_icaoStationId = text.mid(7, 4);
            m_stationNumber = text.mid(11, 1);
            if (m_version == "01")
            {
                m_freeText = text.mid(12);
            }
            else
            {
                m_latitude = text.mid(12, 5);
                m_longitude = text.mid(17, 6);
                int idx = 23;
                while ((idx < text.length()) && (text[idx] != '/'))
                {
                    if (((text[idx] == 'V') || (text[idx] == 'A') || (text[idx] == 'B')) && (idx + 7 <= text.length()))
                    {
                        m_alternateService.append(text[idx]);
                        m_alternateFrequency.append(QString("%1.%2kHz")
                                                            .arg(text.mid(idx+1,3))
                                                            .arg(text.mid(idx+4,3)));
                        idx += 7;
                    }
                    else
                    {
                        break;
                    }
                }
                idx++;
                m_freeText = text.mid(idx);
            }
        }
    }
    return true;
}

QString AcarsSquitter::toString()
{
    const QHash<QString, QString> service = {
        {"A", "VDL Mode 2 ATN Only"},
        {"B", "VDL Mode 2 AOA and ATN"},
        {"V", "VDL Mode 2 AOA Only"},
    };
    const QHash<QString, QString> serviceProviders = {
        {"XA", "Rockwell Collins IMS"},
        {"XS", "SITA VHF"},
        {"AS", "Honeywell"},
    };
    QString serviceProvider = m_serviceProvider;
    if (serviceProviders.contains(m_serviceProvider)) {
        serviceProvider = QString("%1 (%2)").arg(m_serviceProvider).arg(serviceProviders.value(m_serviceProvider));
    }
    QString s = QString("<b>Version:</b> %1<br><b>Service provider:</b> %2")
                    .arg(m_version)
                    .arg(serviceProvider);
    if (m_version != "00")
    {
        s.append(QString("<br><b>IATA station Id:</b> %1<br><b>ICAO station Id:</b> %2<br><b>Station number:</b> %3")
                        .arg(m_iataStationId)
                        .arg(icaoAndAirportName(m_icaoStationId))
                        .arg(m_stationNumber));
        if (m_version != "01")
        {
            s.append(QString("<br><b>Latitude:</b> %1<br><b>Longitude:</b> %2")
                            .arg(m_latitude)
                            .arg(m_longitude));
            for (int i = 0; i < m_alternateService.size(); i++)
            {
                s.append(QString("<br><b>Alternate service:</b> %1 - %2")
                            .arg(service[m_alternateService[i]])
                            .arg(m_alternateFrequency[i]));
            }
        }
    }
    if (!m_freeText.isEmpty()) {
        s.append(QString("<br><b>Free text:</b> %1").arg(m_freeText));
    }
    return s;
}
