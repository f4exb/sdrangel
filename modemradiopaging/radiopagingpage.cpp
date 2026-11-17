///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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

#include <algorithm>

#include <QRegularExpression>

#include "SWGRadioPagingModTx.h"
#include "SWGRadioPagingCharset.h"

#include "radiopagingcharset.h"
#include "radiopagingflex.h"
#include "radiopagingflexscheduler.h"
#include "radiopaginggsc.h"
#include "radiopagingpocsag.h"
#include "radiopagingpage.h"

const QStringList& RadioPagingPage::protocols()
{
    static const QStringList list = {"pocsag", "flex", "gsc"};
    return list;
}

const QStringList& RadioPagingPage::types()
{
    static const QStringList list = {"alphanumeric", "numeric", "special_numeric", "numbered_numeric", "numbered_special_numeric", "tone",
        "voice", "short_message", "binary", "secure"};
    return list;
}

QStringList RadioPagingPage::types(const QString& protocol)
{
    if (protocol == "flex") {
        return {"alphanumeric", "numeric", "special_numeric", "numbered_numeric", "numbered_special_numeric", "tone", "short_message",
            "binary", "secure"};
    } else if (protocol == "gsc") {
        return {"alphanumeric", "numeric", "tone", "voice"};
    } else {
        return {"alphanumeric", "numeric", "tone"};
    }
}

QList<qulonglong> RadioPagingPage::groupMembers(bool *ok) const
{
    QList<qulonglong> members;
    bool all = true;

    for (const QString& token : m_groupMembers.split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts))
    {
        RadioPagingFlex::CapcodeText capcode;
        std::string error;
        bool parsed = RadioPagingFlex::parseCapcode(token.toStdString(), capcode, error);

        all = all && parsed;
        members.append(capcode.m_capcode);
    }

    if (ok) {
        *ok = all;
    }

    return members;
}

bool RadioPagingPage::flexCapcode(qulonglong& capcode, QString *error) const
{
    RadioPagingFlex::CapcodeText text;
    std::string reason;
    bool ok = RadioPagingFlex::parseCapcode(m_address.toStdString(), text, reason);

    capcode = ok ? text.m_capcode : 0;

    if (!ok && error) {
        *error = QString::fromStdString(reason);
    }

    return ok;
}

QString RadioPagingPage::checkAddress() const
{
    if (!protocols().contains(m_protocol)) {
        return "Protocol must be pocsag, flex or gsc";
    }
    // A JSON number is not taken for the address string
    if (m_address.isEmpty()) {
        return "The address is missing: a string of digits, e.g. \"1234567\"";
    }

    if (m_protocol == "flex")
    {
        // FLEX: digits or a capcode form with letters
        qulonglong capcode;
        QString error;

        if (!flexCapcode(capcode, &error)) {
            return QString("FLEX capcode: %1").arg(error);
        }
        if (capcode == 0) {
            return "FLEX capcode must be 1 to 1933312 or 2101249 to 4297068542";
        }
    }
    else
    {
        bool digits = true;

        for (QChar c : m_address) {
            digits = digits && c.isDigit();
        }

        if (!digits) {
            return "The address must be a number";
        }
        if ((m_protocol == "pocsag") && (m_address.toULongLong() > RadioPagingPocsag::m_maxAddress)) {
            return "POCSAG address must be 0 to 2097151";
        }
        if (m_protocol == "gsc")
        {
            std::string address, error;
            int number;

            if (!RadioPagingGsc::parseCapCode(m_address.toStdString(), address, number, &error)) {
                return QString("GSC: %1").arg(QString::fromStdString(error));
            }
        }
    }
    if ((m_function < 0) || (m_function > 3)) {
        return (m_protocol == "gsc") ? "GSC function must be 0 to 3 (A to D), for the address number 1 to 4"
            : "Function must be 0 to 3";
    }

    return QString();
}

QStringList RadioPagingPage::functions(const QString& protocol)
{
    QStringList names;

    // POCSAG: the function A to D; GSC: the address number 1 to 4 (function A to D); FLEX has none
    if ((protocol == "pocsag") || (protocol == "gsc"))
    {
        for (int i = 0; i < 4; i++) {
            names.append((protocol == "gsc") ? QString("%1 (%2)").arg(i + 1).arg(QChar(RadioPagingGsc::functionLetter(i + 1))) : QString(QChar('A' + i)));
        }
    }

    return names;
}

// A FLEX check that damages the message. The checksum K is of the words of a fragment (a reassembled message has the
// worst of its fragments): ok or not (incomplete in records of before: not ok). The signature S is of the characters
// of the whole message: failed, or incomplete (characters lost); incomplete in a fragment or a part, whose rest is
// still to come, is no damage
bool RadioPagingPage::checkDamages(const MessageRecord& record, const char *check)
{
    const QString result = record.m_protocolData.value(check).toString();

    if (result == "failed") {
        return true;
    }
    if (result != "incomplete") {
        return false;
    }

    const bool unfinished = record.m_partial || record.m_protocolData.value("is_fragment").toBool();
    return (QString(check) == "checksum") || !unfinished;
}

MessageRecord::Quality RadioPagingPage::receivedQuality(const MessageRecord& record)
{
    const QVariantMap& d = record.m_protocolData;
    bool checked = false;

    for (const char *key : {"checksum", "signature"})
    {
        if (checkDamages(record, key)) {
            return MessageRecord::QualityDamaged;
        }

        checked = checked || (d.value(key).toString() == "ok");
    }

    // A message of fragments one of which is missing is incomplete; so is a message cut (POCSAG, GSC: the signal ended
    // before the message did, its rest may be missing)
    if ((d.value("checksum_errors").toInt() > 0) || (d.value("fragmented").toBool() && !d.value("complete", true).toBool())
        || (d.value("end").toString() == "cut")) {
        return MessageRecord::QualityDamaged;
    }

    checked = checked || (d.value("checksum_passed").toInt() > 0);
    const MessageRecord::Quality q = MessageRecord::qualityFromErrors(record.m_errorsCorrected, record.m_errorsUncorrectable,
        record.m_charStatus);
    return ((q == MessageRecord::QualityNone) && checked) ? MessageRecord::QualityClean : q;
}

QVariantList RadioPagingPage::receivedFields()
{
    struct Field
    {
        const char *protocol;
        const char *key;
        const char *name;
        const char *type;
        QStringList values;
    };
    const QStringList yesNo = {"true", "false"};
    const std::vector<Field> fields = {
        {"pocsag", "ric", "RIC (address without its function)", "number", {}},
        {"pocsag", "function", "Function (0-3: A-D)", "list", {"0", "1", "2", "3"}},
        {"pocsag", "batch", "Batch of the address (from 0)", "number", {}},
        {"pocsag", "end", "How it ended", "list", {"idle", "address", "batch", "cut"}},
        {"flex", "capcode", "Capcode", "text", {}},
        {"flex", "phase", "Phase", "list", {"A", "B", "C", "D"}},
        {"flex", "cycle", "Cycle (0-14)", "number", {}},
        {"flex", "frame", "Frame (0-127)", "number", {}},
        {"flex", "checksum", "Checksum K (of the words of a fragment)", "list", {"ok", "failed"}},
        {"flex", "signature", "Signature S (of the characters of the message)", "list", {"ok", "failed", "incomplete"}},
        {"flex", "message_number", "Message number", "number", {}},
        {"flex", "fragmented", "Sent in fragments", "list", yesNo},
        {"flex", "complete", "All its fragments received", "list", yesNo},
        {"flex", "is_fragment", "A fragment (not the whole message)", "list", yesNo},
        {"flex", "fragment_count", "Fragments", "number", {}},
        {"flex", "priority", "Priority address", "list", yesNo},
        {"flex", "maildrop", "Mail drop", "list", yesNo},
        {"flex", "long_address", "Long address", "list", yesNo},
        {"flex", "blocking", "Blocking length (bits of a binary character)", "number", {}},
        {"flex", "group_slot", "Group slot", "number", {}},
        {"gsc", "function", "Function (0-3: A-D)", "list", {"0", "1", "2", "3"}},
        {"gsc", "address_number", "Address number (1-4)", "list", {"1", "2", "3", "4"}},
        {"gsc", "preamble", "Preamble (0-9)", "number", {}},
        {"gsc", "slot", "Slot of the address", "number", {}},
        {"gsc", "non_battery_saver", "Non battery saver", "list", yesNo},
        {"gsc", "checksum_errors", "Blocks with a wrong checksum", "number", {}},
        {"gsc", "end", "How it ended", "list", {"complete", "cut"}},
        {"gsc", "voice_seconds", "Voice length (s)", "number", {}}
    };
    QVariantList out;

    for (const Field& f : fields)
    {
        QVariantMap m;
        m.insert("protocol", f.protocol);
        m.insert("key", f.key);
        m.insert("name", f.name);
        m.insert("type", f.type);

        if (!f.values.isEmpty()) {
            m.insert("values", f.values);
        }

        out.append(m);
    }

    return out;
}

MessageRecord::Quality RadioPagingPage::quality(const MessageRecord& record)
{
    if (record.m_quality != MessageRecord::QualityUnknown) {
        return record.m_quality;
    }
    if (record.m_direction == MessageRecord::DirectionTX) {
        return MessageRecord::QualityClean;     // Nothing was received: nothing went wrong
    }

    return receivedQuality(record);
}

QString RadioPagingPage::qualityReason(const MessageRecord& record)
{
    const QVariantMap& d = record.m_protocolData;
    QStringList reasons;

    if (record.m_errorsUncorrectable > 0) {
        reasons.append(QString("%1 codeword%2 uncorrectable").arg(record.m_errorsUncorrectable).arg(record.m_errorsUncorrectable > 1 ? "s" : ""));
    }
    if (record.m_charStatus.contains('b')) {
        reasons.append("characters lost");
    }

    const QList<QPair<const char*, const char*>> checks = {{"checksum", "checksum K"}, {"signature", "signature S"}};

    if (d.value("fragmented").toBool() && !d.value("complete", true).toBool()) {
        reasons.append("incomplete: fragments missing");
    }
    if (d.value("end").toString() == "cut") {
        reasons.append("cut: the signal ended before the message did");
    }

    for (const auto& field : checks)
    {
        if (checkDamages(record, field.first))
        {
            // K is ok or not
            const QString result = (QString(field.first) == "checksum") ? QString("failed") : d.value(field.first).toString();
            reasons.append(QString("%1 %2").arg(field.second, result));
        }
    }

    if (d.value("checksum_errors").toInt() > 0) {
        reasons.append(QString("%1 blocks with a wrong checksum").arg(d.value("checksum_errors").toInt()));
    }
    if (record.m_errorsCorrected > 0) {
        reasons.append(QString("%1 bit error%2 corrected").arg(record.m_errorsCorrected).arg(record.m_errorsCorrected > 1 ? "s" : ""));
    }

    return reasons.join(", ");
}

QString RadioPagingPage::addressDescription(const QString& protocol, const QString& address, int function)
{
    // GSC the digits of the cap code (a suffix digit sets the function), FLEX the capcode form, POCSAG the frame and
    // function
    if (protocol == "gsc") {
        return QString::fromStdString(RadioPagingGsc::capCodeText(address.toStdString()));
    } else if (protocol == "flex") {
        return QString::fromStdString(RadioPagingFlex::capcodeDescription(address.toStdString()));
    } else if (protocol == "pocsag") {
        return QString::fromStdString(RadioPagingPocsag::addressDescription((uint32_t) std::min<qulonglong>(address.toULongLong(), 0xffffffffULL),
            std::max(0, function)));
    }

    return QString();
}

QString RadioPagingPage::check() const
{
    const QString type = m_type.isEmpty() ? QString("alphanumeric") : m_type;

    // A group message: the pagers, not an address
    const bool group = (m_protocol == "flex") && !m_groupMembers.trimmed().isEmpty();

    if (!group)
    {
        const QString error = checkAddress();

        if (!error.isEmpty()) {
            return error;
        }
    }
    else
    {
        bool ok = false;
        QList<qulonglong> members = groupMembers(&ok);

        if (!ok || (members.size() < 2) || (members.size() > RadioPagingFlexEncoder::m_maxGroupMembers)) {
            return QString("A group has 2 to %1 pagers: capcodes separated by commas").arg(RadioPagingFlexEncoder::m_maxGroupMembers);
        }
        for (qulonglong capcode : members)
        {
            if (!RadioPagingFlex::isValidAddress(capcode)) {
                return "FLEX capcode must be 1 to 1933312 or 2101249 to 4297068542";
            }
        }
        if ((type != "alphanumeric") && (type != "numeric") && (type != "tone") && (type != "short_message")) {
            return "A group message is alphanumeric, numeric, tone or a short message";
        }
    }
    if (!types(m_protocol).contains(type)) {
        return QString("Type must be %1 for %2").arg(types(m_protocol).join(", ")).arg(m_protocol);
    }
    if ((m_polarity < -1) || (m_polarity > 1)) {
        return "Polarity must be normal or inverted";
    }
    if ((type == "voice") && m_voiceFile.isEmpty()) {
        return "A voice page needs the WAV file";
    }
    if ((type == "short_message") && ((m_shortType < 0) || (m_shortType > 2) || (m_source < 0) || (m_source > 7))) {
        return "FLEX short message: shortType 0 (numeric), 1 (source) or 2 (numbered), source 0 to 7";
    }
    if ((type == "binary") && ((m_blocking < 1) || (m_blocking > 16))) {
        return "FLEX binary message: blocking 1 to 16";
    }
    if ((type == "secure") && ((m_secureType < 0) || (m_secureType > 2))) {
        return "FLEX secure message: secureType 0 (alphanumeric), 1 (data defined separately) or 2 (binary)";
    }
    if (m_retransmit < 0) {
        return "Retransmissions must be 0 or more";
    }
    if ((m_sendDelay < 0.0) || (m_sendDelay > m_maxScheduleSeconds) || (m_retransmitInterval > m_maxScheduleSeconds)) {
        return "Send delay and retransmission interval must be 0 to 3600 s";
    }
    if (!m_charset.empty())
    {
        QString error = checkCharset(m_protocol, m_charset);

        if (!error.isEmpty()) {
            return error;
        }
    }

    return QString();
}

// A cap code that is not one (check() refuses it) is kept as it is, padded to 6 digits
bool RadioPagingPage::gscAddress(QString& address, int& function) const
{
    std::string a;
    int number = 0;
    bool ok = RadioPagingGsc::parseCapCode(m_address.toStdString(), a, number);

    address = ok ? QString::fromStdString(a) : m_address.rightJustified(6, QChar('0'));
    function = (number > 0) ? number - 1 : (m_function & 3);
    return ok;
}

MessageRecord RadioPagingPage::toRecord() const
{
    MessageRecord r;
    r.m_id = m_id;
    r.m_direction = MessageRecord::DirectionTX;
    r.m_kind = MessageRecord::KindMessage;
    r.m_protocol = m_protocol;
    r.m_msgType = m_type.isEmpty() ? QString("alphanumeric") : m_type;
    r.m_text = ((r.m_msgType == "tone") || (r.m_msgType == "voice")) ? QString() : m_text;
    r.m_polarity = (m_polarity == 0) ? MessageRecord::PolarityNormal
        : (m_polarity == 1) ? MessageRecord::PolarityInverted : MessageRecord::PolarityUnknown;
    r.m_speed = ((m_protocol == "pocsag") && (m_speed > 0)) ? m_speed : -1;

    QVariantMap& d = r.m_protocolData;
    d.insert("transmitter", m_transmitter);

    // The address and its keys as in the records of the receivers and transmitters: POCSAG ric and function
    // 0-3, FLEX capcode, GSC address number 1-4 and function 0-3 (A-D, the address number less 1)
    if (m_protocol == "pocsag")
    {
        r.m_address = QString::fromStdString(RadioPagingPocsag::addressText((uint32_t) m_address.toULongLong(), m_function));
        d.insert("ric", m_address.toULongLong());
        d.insert("function", m_function);
    }
    else if (m_protocol == "gsc")
    {
        QString address;
        int function;
        gscAddress(address, function);
        r.m_address = QString::fromStdString(RadioPagingGsc::addressText(address.toStdString(), function + 1));
        d.insert("address_number", function + 1);
        d.insert("function", function);
    }
    else
    {
        qulonglong capcode = 0;
        flexCapcode(capcode);
        const QList<qulonglong> members = groupMembers();

        if (!m_groupMembers.trimmed().isEmpty())
        {
            // A group message: the pagers, in the keys of the other records
            QStringList texts;
            QVariantList list;

            for (qulonglong member : members)
            {
                texts << QString::fromStdString(RadioPagingFlex::addressText(member));
                list << member;
            }

            r.m_address = texts.join(",");
            d.insert("group_members", list);
            d.insert("priority", m_priority);
        }
        else
        {
            r.m_address = QString::fromStdString(RadioPagingFlex::addressText(capcode));
            d.insert("capcode", capcode);
            d.insert("long_address", RadioPagingFlex::isLongAddress(capcode));
            d.insert("addr_type", RadioPagingFlex::isLongAddress(capcode) ? "long" : "short");
            d.insert("priority", m_priority);
        }
    }

    if (r.m_msgType == "voice") {
        d.insert("voice_source", m_voiceFile);      // voice_file is the record of the voice sent or received
    }
    else if (r.m_msgType == "alphanumeric")
    {
        if (m_protocol == "flex") {
            d.insert("maildrop", m_mailDrop);
        }
    }
    else if (r.m_msgType == "short_message")
    {
        d.insert("short_type", m_shortType);
        d.insert("short_source", m_source);
    }
    else if (r.m_msgType == "binary")
    {
        d.insert("blocking", m_blocking);
        d.insert("maildrop", m_mailDrop);
        d.insert("display_direction", m_rightToLeft ? "rtl" : "ltr");
        d.insert("header_message", m_headerMessage);
        d.insert("status_info", m_statusInfo);
    }
    else if (r.m_msgType == "secure")
    {
        d.insert("secure_type", m_secureType);
    }

    if (m_retransmit != 0) {
        d.insert("retransmit", m_retransmit);
    }
    if (m_retransmitInterval >= 0.0) {
        d.insert("retransmit_interval", m_retransmitInterval);
    }
    if (m_sendDelay != 0.0) {
        d.insert("send_delay", m_sendDelay);
    }
    if (!m_charset.empty()) {
        d.insert("charset", charsetData(m_charset));
    }

    return r;
}

RadioPagingPage RadioPagingPage::fromRecord(const MessageRecord& record)
{
    const QVariantMap& d = record.m_protocolData;
    RadioPagingPage page;

    page.m_id = record.m_id;
    page.m_transmitter = d.value("transmitter").toString();
    page.m_protocol = record.m_protocol;
    page.m_type = record.m_msgType.isEmpty() ? QString("alphanumeric") : record.m_msgType;
    page.m_text = record.m_text;
    page.m_voiceFile = d.value("voice_source").toString();
    page.m_speed = (record.m_speed > 0) ? record.m_speed : 0;
    page.m_polarity = (record.m_polarity == MessageRecord::PolarityNormal) ? 0
        : (record.m_polarity == MessageRecord::PolarityInverted) ? 1 : -1;

    if (record.m_protocol == "pocsag")
    {
        page.m_address = QString::number(d.value("ric").toULongLong());
        page.m_function = d.value("function").toInt();
    }
    else if (record.m_protocol == "gsc")
    {
        // The 6 digits of the record address; the address number 1-4 as the function 0-3 (A-D)
        page.m_address = record.m_address.left(6);
        page.m_function = d.contains("address_number") ? std::max(1, std::min(4, d.value("address_number").toInt())) - 1
            : (d.value("function").toInt() & 3);
    }
    else
    {
        page.m_address = QString::number(d.value("capcode").toULongLong());
        page.m_priority = d.value("priority").toBool();

        QStringList members;

        for (const QVariant& member : d.value("group_members").toList()) {
            members << QString::number(member.toULongLong());
        }

        page.m_groupMembers = members.join(",");
    }

    page.m_shortType = d.value("short_type", 0).toInt();
    page.m_source = d.value("short_source", 0).toInt();
    page.m_blocking = d.value("blocking", 1).toInt();
    page.m_mailDrop = d.value("maildrop").toBool();
    page.m_rightToLeft = (d.value("display_direction").toString() == "rtl");
    page.m_headerMessage = d.value("header_message").toBool();
    page.m_statusInfo = d.value("status_info").toBool();
    page.m_secureType = d.value("secure_type", 0).toInt();
    page.m_retransmit = d.value("retransmit", 0).toInt();
    page.m_retransmitInterval = d.value("retransmit_interval", -1.0).toDouble();
    page.m_sendDelay = d.value("send_delay", 0.0).toDouble();
    page.m_charset = charsetKey(d.value("charset"));
    return page;
}

// The address as a string. POCSAG: address and function; FLEX: capcode and priority; GSC: the 6 digits of the
// address, the function 0-3 for the address number 1-4. A voice page has its sound file as the text
void RadioPagingPage::toTx(SWGSDRangel::SWGRadioPagingModTx& tx) const
{
    const QString type = m_type.isEmpty() ? QString("alphanumeric") : m_type;

    tx.setId(new QString(m_id));
    tx.setProtocol(new QString(m_protocol));
    tx.setType(new QString(type));
    tx.setText(new QString((type == "voice") ? m_voiceFile : m_text));
    tx.setPolarity(new QString((m_polarity == 1) ? "inverted" : (m_polarity == 0) ? "normal" : ""));

    if (m_protocol == "pocsag")
    {
        tx.setAddress(new QString(QString::number(m_address.toULongLong())));
        tx.setFunction(m_function);
        tx.setSpeed(m_speed);
    }
    else if (m_protocol == "flex")
    {
        qulonglong capcode = 0;
        flexCapcode(capcode);
        tx.setAddress(new QString(QString::number(capcode)));
        tx.setPriority(m_priority ? 1 : 0);
        tx.setGroupMembers(new QString(m_groupMembers));

        if (type == "alphanumeric")
        {
            tx.setMailDrop(m_mailDrop ? 1 : 0);
        }
        else if (type == "short_message")
        {
            tx.setShortType(m_shortType);
            tx.setSource(m_source);
        }
        else if (type == "binary")
        {
            tx.setBlocking(m_blocking);
            tx.setMailDrop(m_mailDrop ? 1 : 0);
            tx.setRightToLeft(m_rightToLeft ? 1 : 0);
            tx.setHeaderMessage(m_headerMessage ? 1 : 0);
            tx.setStatusInfo(m_statusInfo ? 1 : 0);
        }
        else if (type == "secure")
        {
            tx.setSecureType(m_secureType);
        }
    }
    else
    {
        QString address;
        int function;
        gscAddress(address, function);
        tx.setAddress(new QString(address));
        tx.setFunction(function);
    }

    tx.setRetransmit(m_retransmit);
    tx.setRetransmitInterval((float) m_retransmitInterval);
    tx.setSendDelay((float) m_sendDelay);

    if (!m_charset.empty()) {
        tx.setCharset(charsetSwg(m_charset));
    }
}

RadioPagingPage RadioPagingPage::fromTx(SWGSDRangel::SWGRadioPagingModTx& tx, const QString& defaultProtocol)
{
    auto text = [](QString *s) { return s ? *s : QString(); };
    RadioPagingPage page;
    QString polarity = text(tx.getPolarity()).toLower();

    page.m_id = text(tx.getId());
    page.m_protocol = text(tx.getProtocol()).toLower();
    page.m_type = text(tx.getType()).toLower();
    page.m_polarity = polarity.isEmpty() ? -1 : (polarity == "normal") ? 0 : (polarity == "inverted") ? 1 : -2;
    page.m_function = tx.getFunction();
    page.m_priority = tx.getPriority() != 0;
    page.m_groupMembers = text(tx.getGroupMembers());
    page.m_speed = tx.getSpeed();
    page.m_shortType = tx.getShortType();
    page.m_source = tx.getSource();
    page.m_blocking = (tx.getBlocking() == 0) ? 1 : tx.getBlocking();
    page.m_mailDrop = tx.getMailDrop() != 0;
    page.m_rightToLeft = tx.getRightToLeft() != 0;
    page.m_headerMessage = tx.getHeaderMessage() != 0;
    page.m_statusInfo = tx.getStatusInfo() != 0;
    page.m_secureType = tx.getSecureType();
    page.m_retransmit = tx.getRetransmit();
    // 0 (not given) or negative: the default of the protocol
    page.m_retransmitInterval = (tx.getRetransmitInterval() > 0.0f) ? tx.getRetransmitInterval() : -1.0f;
    page.m_sendDelay = tx.getSendDelay();
    page.m_charset = charsetKey(tx.getCharset());

    if (page.m_type == "voice") {
        page.m_voiceFile = text(tx.getText());
    } else {
        page.m_text = text(tx.getText());
    }

    // The address of the protocol of the page
    if (page.m_protocol.isEmpty()) {
        page.m_protocol = defaultProtocol;
    }

    page.m_address = text(tx.getAddress()).trimmed();

    return page;
}

QVariantMap RadioPagingPage::charsetData(const RadioPagingCharset::Key& key)
{
    QVariantMap data;
    data.insert("vendor", QString::fromStdString(key.m_vendor));
    data.insert("model", QString::fromStdString(key.m_model));
    data.insert("codepage", QString::fromStdString(key.m_codepage));
    return data;
}

RadioPagingCharset::Key RadioPagingPage::charsetKey(const QVariant& data)
{
    const QVariantMap map = data.toMap();
    RadioPagingCharset::Key key;
    key.m_vendor = map.value("vendor").toString().trimmed().toStdString();
    key.m_model = map.value("model").toString().trimmed().toStdString();
    key.m_codepage = map.value("codepage").toString().trimmed().toStdString();
    return key;
}

SWGSDRangel::SWGRadioPagingCharset *RadioPagingPage::charsetSwg(const RadioPagingCharset::Key& key)
{
    SWGSDRangel::SWGRadioPagingCharset *swg = new SWGSDRangel::SWGRadioPagingCharset();
    swg->setVendor(new QString(QString::fromStdString(key.m_vendor)));
    swg->setModel(new QString(QString::fromStdString(key.m_model)));
    swg->setCodepage(new QString(QString::fromStdString(key.m_codepage)));
    return swg;
}

RadioPagingCharset::Key RadioPagingPage::charsetKey(SWGSDRangel::SWGRadioPagingCharset *swg)
{
    auto text = [](QString *s) { return s ? s->trimmed().toStdString() : std::string(); };
    RadioPagingCharset::Key key;

    if (swg)
    {
        key.m_vendor = text(swg->getVendor());
        key.m_model = text(swg->getModel());
        key.m_codepage = text(swg->getCodepage());
    }

    return key;
}

QString RadioPagingPage::checkCharset(const QString& protocol, const RadioPagingCharset::Key& key)
{
    if (!RadioPagingCharset::find(protocol.toStdString(), key)) {
        return QString("%1 is not a codepage of %2").arg(charsetText(key)).arg(protocol);
    }

    return QString();
}

QString RadioPagingPage::charsetText(const RadioPagingCharset::Key& key)
{
    return QString("%1 / %2 / %3").arg(QString::fromStdString(key.m_vendor), QString::fromStdString(key.m_model),
        QString::fromStdString(key.m_codepage));
}
