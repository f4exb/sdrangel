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

#include <QFile>
#include <QJsonArray>
#include <QUrl>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>
#include <QUdpSocket>
#include <QUuid>

#include <algorithm>

#include "util/csv.h"
#include "radiopagingpage.h"
#include "radiopagingpagerformat.h"
#include "radiopagingrecode.h"
#include "messagecenterengine.h"
#include "messagecenterarchive.h"

namespace {

QJsonArray intsToJson(const QList<int>& values)
{
    QJsonArray array;

    for (int v : values) {
        array.append(v);
    }

    return array;
}

QList<int> intsFromJson(const QJsonValue& value)
{
    QList<int> out;

    for (const QJsonValue& v : value.toArray()) {
        out.append(v.toInt());
    }

    return out;
}

} // namespace

// ---------------------------------------------------------------------------
// Filter
// ---------------------------------------------------------------------------

bool MessageCenterFilter::matches(const MessageRecord& record) const
{
    if (!m_protocols.isEmpty() && !m_protocols.contains(record.m_protocol, Qt::CaseInsensitive)) {
        return false;
    }
    // The contact of the address book the message is from or for (the engine names it when the record comes in)
    if (!m_contacts.isEmpty() && !m_contacts.contains(record.m_protocolData.value("contact").toString())) {
        return false;
    }
    // The channel of a message; a page also by the transmitter it is for
    if (!m_channels.isEmpty() && !m_channels.contains(record.m_channel)
        && !m_channels.contains(record.m_protocolData.value("transmitter").toString())) {
        return false;
    }
    if ((m_direction >= 0) && ((int) record.m_direction != m_direction)) {
        return false;
    }
    if ((m_kind >= 0) && ((int) record.m_kind != m_kind)) {
        return false;
    }
    if ((m_polarity >= 0) && ((int) record.m_polarity != m_polarity)) {
        return false;
    }
    if (!m_speeds.isEmpty() && !m_speeds.contains(record.m_speed)) {
        return false;
    }
    if (!m_msgType.isEmpty() && (record.m_msgType != m_msgType)) {
        return false;
    }
    // The address, or the address of a member of the group of a group message
    if (!m_address.isEmpty())
    {
        const QRegularExpression& expression = compile(m_compiledAddress, m_address, true);
        const QStringList addresses = record.addresses();

        if (!expression.isValid() || std::none_of(addresses.begin(), addresses.end(),
            [&expression](const QString& a) { return expression.match(a).hasMatch(); })) {
            return false;
        }
    }
    if (!m_text.isEmpty())
    {
        const QRegularExpression& expression = compile(m_compiledText, m_text, false,
            QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);

        if (!expression.isValid() || !expression.match(record.m_text).hasMatch()) {
            return false;
        }
    }
    if (m_hideFragments && record.m_protocolData.value("is_fragment").toBool()) {
        return false;
    }
    if (!m_quality.isEmpty())
    {
        const MessageRecord::Quality q = quality(record);
        const bool met = (m_quality == "notDamaged") ? (q != MessageRecord::QualityDamaged)
            : (MessageRecord::qualityToString(q) == m_quality);

        if (!met) {
            return false;
        }
    }
    // Any field of the record or of its protocol data; a list (the members of a group message): one of its values
    // Each condition reads its field of the record alone (no JSON of the whole record: a filter over a large log)
    if (!std::all_of(m_conditions.begin(), m_conditions.end(),
        [&record](const MessageCenterContact::Condition& condition) { return condition.matches(record); })) {
        return false;
    }
    if ((m_frequencyMin >= 0) && (record.m_frequency < m_frequencyMin)) {
        return false;
    }
    if ((m_frequencyMax >= 0) && ((record.m_frequency < 0) || (record.m_frequency > m_frequencyMax))) {
        return false;
    }
    if ((m_timeFrom >= 0) && (record.m_timestamp < m_timeFrom)) {
        return false;
    }
    if ((m_timeTo >= 0) && (record.m_timestamp > m_timeTo)) {
        return false;
    }
    if (m_hideDuplicates && (record.m_duplicate != MessageRecord::DuplicateNone)) {
        return false;
    }
    if (m_voiceOnly && record.m_protocolData.value("voice_file").toString().isEmpty()) {
        return false;
    }

    return true;
}

bool MessageCenterFilter::matches(const MessageRecord& record, const QString& source) const
{
    if (!m_sources.isEmpty() && !m_sources.contains(source)) {
        return false;
    }

    return matches(record);
}

// Anchored: the whole text must match, as the filters of the other plugins
const QRegularExpression& MessageCenterFilter::compile(Compiled& compiled, const QString& pattern, bool anchored,
    QRegularExpression::PatternOptions options)
{
    if (!compiled.m_done || (compiled.m_pattern != pattern))
    {
        compiled.m_expression = QRegularExpression(anchored ? QRegularExpression::anchoredPattern(pattern) : pattern, options);
        compiled.m_expression.optimize();
        compiled.m_pattern = pattern;
        compiled.m_done = true;
    }

    return compiled.m_expression;
}

QString MessageCenterFilter::errors() const
{
    QStringList errors;
    const QRegularExpression::PatternOptions textOptions = QRegularExpression::DotMatchesEverythingOption
        | QRegularExpression::CaseInsensitiveOption;

    if (!m_address.isEmpty() && !compile(m_compiledAddress, m_address, true).isValid()) {
        errors.append(QString("address: %1").arg(compile(m_compiledAddress, m_address, true).errorString()));
    }
    if (!m_text.isEmpty() && !compile(m_compiledText, m_text, false, textOptions).isValid()) {
        errors.append(QString("text: %1").arg(compile(m_compiledText, m_text, false, textOptions).errorString()));
    }
    for (const MessageCenterContact::Condition& condition : m_conditions)
    {
        const QString error = condition.check();

        if (!error.isEmpty()) {
            errors.append(error);
        }
    }

    return errors.join("; ");
}

MessageRecord::Quality MessageCenterFilter::quality(const MessageRecord& record)
{
    // A record of before quality (the history, an imported file): as the paging receiver judges it now
    return RadioPagingPage::quality(record);
}

QStringList MessageCenterFilter::captures(const MessageRecord& record) const
{
    if (m_text.isEmpty()) {
        return QStringList();
    }

    const QRegularExpression& expression = compile(m_compiledText, m_text, false,
        QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = expression.isValid() ? expression.match(record.m_text) : QRegularExpressionMatch();
    return match.hasMatch() ? match.capturedTexts() : QStringList();
}

QString messageCenterSubstitute(const QString& text, const MessageRecord& record, const QStringList& captures,
    const QMap<QString, QString>& named)
{
    QString out;
    int pos = 0;

    while (pos < text.size())
    {
        int start = text.indexOf("${", pos);
        int end = (start < 0) ? -1 : text.indexOf('}', start + 2);

        if (end < 0)
        {
            out += text.mid(pos);
            break;
        }

        out += text.mid(pos, start - pos);
        QString name = text.mid(start + 2, end - start - 2);
        // An encoding after the name: ${text:base64}, ${text:json} (a JSON string, with its quotes), ${text:url}
        QString encoding;
        const int colon = name.lastIndexOf(':');

        if ((colon > 0) && QStringList({"base64", "json", "url"}).contains(name.mid(colon + 1)))
        {
            encoding = name.mid(colon + 1);
            name = name.left(colon);
        }

        bool isIndex;
        int index = name.toInt(&isIndex);
        bool known = true;
        QString value;

        if (isIndex && (index >= 0)) {
            value = (index < captures.size()) ? captures[index] : QString();
        } else if (name == "address") {
            value = record.m_address;
        } else if (name == "message") {
            value = record.m_text;
        } else if (name == "protocol") {
            value = record.m_protocol;
        } else if (name == "type") {
            value = record.m_msgType;
        } else if (name == "channel") {
            value = record.m_channel;
        } else if (name == "mode") {
            value = record.m_mode;
        } else if (name == "id") {
            value = record.m_id;
        } else if (name == "frequency") {
            value = QString::number(record.m_frequency);
        } else if (name == "time") {
            value = QDateTime::fromMSecsSinceEpoch(record.m_timestamp, Qt::UTC).toString(Qt::ISODate);
        } else if (named.contains(name)) {
            value = named.value(name);
        } else {
            // Any other field of the record or of its protocol data (a list: its values separated by commas)
            const QJsonValue field = MessageCenterContact::recordValue(record, name);

            if (field.isUndefined() || field.isObject()) {
                known = false;
            } else if (field.isArray()) {
                value = field.toVariant().toStringList().join(",");
            } else if (!field.isNull()) {
                value = field.toVariant().toString();
            }
        }

        if (known && (encoding == "base64")) {
            value = QString::fromLatin1(value.toUtf8().toBase64());
        } else if (known && (encoding == "json")) {
            value = QString::fromUtf8(QJsonDocument(QJsonArray({value})).toJson(QJsonDocument::Compact)).mid(1).chopped(1);
        } else if (known && (encoding == "url")) {
            value = QString::fromLatin1(QUrl::toPercentEncoding(value));
        }

        out += known ? value : text.mid(start, end - start + 1);
        pos = end + 1;
    }

    return out;
}

bool MessageCenterFilter::isEmpty() const
{
    return toJson().isEmpty();
}

QJsonObject MessageCenterFilter::toJson() const
{
    QJsonObject json;

    if (!m_protocols.isEmpty()) {
        json["protocols"] = QJsonArray::fromStringList(m_protocols);
    }
    if (!m_channels.isEmpty()) {
        json["channels"] = QJsonArray::fromStringList(m_channels);
    }
    if (!m_contacts.isEmpty()) {
        json["contacts"] = QJsonArray::fromStringList(m_contacts);
    }
    if (!m_sources.isEmpty()) {
        json["sources"] = QJsonArray::fromStringList(m_sources);
    }
    if (m_direction >= 0) {
        json["direction"] = m_direction;
    }
    if (m_kind >= 0) {
        json["kind"] = m_kind;
    }
    if (m_polarity >= 0) {
        json["polarity"] = m_polarity;
    }
    if (!m_speeds.isEmpty()) {
        json["speeds"] = intsToJson(m_speeds);
    }
    if (!m_address.isEmpty()) {
        json["address"] = m_address;
    }
    if (!m_text.isEmpty()) {
        json["text"] = m_text;
    }
    if (!m_msgType.isEmpty()) {
        json["msgType"] = m_msgType;
    }
    if (!m_conditions.isEmpty())
    {
        QJsonArray conditions;

        for (const MessageCenterContact::Condition& condition : m_conditions) {
            conditions.append(condition.toJson());
        }

        json["conditions"] = conditions;
    }
    if (!m_quality.isEmpty()) {
        json["quality"] = m_quality;
    }
    if (m_hideFragments) {
        json["hideFragments"] = true;
    }
    if (m_frequencyMin >= 0) {
        json["frequencyMin"] = (double) m_frequencyMin;
    }
    if (m_frequencyMax >= 0) {
        json["frequencyMax"] = (double) m_frequencyMax;
    }
    if (m_timeFrom >= 0) {
        json["timeFrom"] = (double) m_timeFrom;
    }
    if (m_timeTo >= 0) {
        json["timeTo"] = (double) m_timeTo;
    }
    if (m_hideDuplicates) {
        json["hideDuplicates"] = true;
    }
    if (m_voiceOnly) {
        json["voiceOnly"] = true;
    }

    return json;
}

MessageCenterFilter MessageCenterFilter::fromJson(const QJsonObject& json)
{
    MessageCenterFilter f;

    for (const QJsonValue& v : json["protocols"].toArray()) {
        f.m_protocols.append(v.toString());
    }
    // aliases: the name in the routes saved before it was channels
    for (const QJsonValue& v : json[json.contains("channels") ? "channels" : "aliases"].toArray()) {
        f.m_channels.append(v.toString());
    }
    for (const QJsonValue& v : json["contacts"].toArray()) {
        f.m_contacts.append(v.toString());
    }
    for (const QJsonValue& v : json["sources"].toArray()) {
        f.m_sources.append(v.toString());
    }

    f.m_direction = json.contains("direction") ? json["direction"].toInt(-1) : -1;
    f.m_kind = json.contains("kind") ? json["kind"].toInt(-1) : -1;
    f.m_polarity = json.contains("polarity") ? json["polarity"].toInt(-1) : -1;
    f.m_speeds = intsFromJson(json["speeds"]);
    f.m_address = json["address"].toString();
    f.m_text = json["text"].toString();
    f.m_msgType = json["msgType"].toString();
    for (const QJsonValue& v : json["conditions"].toArray()) {
        f.m_conditions.append(MessageCenterContact::Condition::fromJson(v.toObject()));
    }

    // The one field of before: its whole value matched the expression
    if (!json["field"].toString().trimmed().isEmpty())
    {
        MessageCenterContact::Condition condition;
        condition.m_field = json["field"].toString().trimmed();
        condition.m_operator = MessageCenterContact::Condition::Matches;
        condition.m_value = json["value"].toVariant().toString();
        f.m_conditions.append(condition);
    }

    f.m_quality = json["quality"].toString();
    f.m_hideFragments = json["hideFragments"].toBool(false);
    f.m_frequencyMin = json.contains("frequencyMin") ? (qint64) json["frequencyMin"].toDouble(-1) : -1;
    f.m_frequencyMax = json.contains("frequencyMax") ? (qint64) json["frequencyMax"].toDouble(-1) : -1;
    f.m_timeFrom = json.contains("timeFrom") ? (qint64) json["timeFrom"].toDouble(-1) : -1;
    f.m_timeTo = json.contains("timeTo") ? (qint64) json["timeTo"].toDouble(-1) : -1;
    f.m_hideDuplicates = json["hideDuplicates"].toBool(false);
    f.m_voiceOnly = json["voiceOnly"].toBool(false);
    return f;
}

// ---------------------------------------------------------------------------
// Contacts
// ---------------------------------------------------------------------------

QString MessageCenterContact::Condition::operatorKey(Operator op)
{
    static const char *keys[] = {"is", "isNot", "matches", "lt", "le", "gt", "ge", "present", "absent"};
    return ((op >= 0) && (op < OperatorCount)) ? QString(keys[op]) : QString("is");
}

QString MessageCenterContact::Condition::operatorText(Operator op)
{
    static const char *texts[] = {"is", "is not", "matches", "<", "<=", ">", ">=", "is present", "is absent"};
    return ((op >= 0) && (op < OperatorCount)) ? QString(texts[op]) : QString("is");
}

QString MessageCenterContact::Condition::text() const
{
    return takesValue(m_operator) ? QString("%1 %2 %3").arg(m_field, operatorText(m_operator), m_value)
        : QString("%1 %2").arg(m_field, operatorText(m_operator));
}

QJsonObject MessageCenterContact::Condition::toJson() const
{
    QJsonObject json;
    json["field"] = m_field;

    if (takesValue(m_operator)) {
        json[operatorKey(m_operator)] = m_value;
    } else {
        json[operatorKey(m_operator)] = true;
    }

    return json;
}

MessageCenterContact::Condition MessageCenterContact::Condition::fromJson(const QJsonObject& json)
{
    Condition c;
    c.m_field = json["field"].toString().trimmed();

    // The operator is the key present (is when none)
    for (int op = 0; op < OperatorCount; op++)
    {
        if (json.contains(operatorKey((Operator) op)))
        {
            c.m_operator = (Operator) op;
            break;
        }
    }

    if (takesValue(c.m_operator)) {
        c.m_value = json[operatorKey(c.m_operator)].toVariant().toString();
    }

    return c;
}

QString MessageCenterContact::Condition::check() const
{
    if (m_field.isEmpty()) {
        return "a condition has no field";
    }
    if (m_operator == Matches)
    {
        QRegularExpression expression(QRegularExpression::anchoredPattern(m_value));

        if (!expression.isValid()) {
            return QString("%1 matches %2: %3").arg(m_field, m_value, expression.errorString());
        }
    }
    if (numeric(m_operator) && !QRegularExpression("^\\s*[+-]?[0-9]+(\\.[0-9]+)?\\s*$").match(m_value).hasMatch()) {
        return QString("%1 %2 %3: not a number").arg(m_field, operatorText(m_operator), m_value);
    }

    return QString();
}

bool MessageCenterContact::Condition::matches(const QJsonObject& fields) const
{
    return matchesField(fields.value(m_field));
}

bool MessageCenterContact::Condition::matches(const MessageRecord& record) const
{
    return matchesField(recordValue(record, m_field));
}

QJsonValue MessageCenterContact::recordValue(const MessageRecord& record, const QString& key)
{
    const QJsonValue value = record.jsonValue(key);

    if (!value.isUndefined()) {
        return value;
    }

    const auto it = record.m_protocolData.constFind(key);
    return (it != record.m_protocolData.constEnd()) ? QJsonValue::fromVariant(it.value()) : QJsonValue(QJsonValue::Undefined);
}

bool MessageCenterContact::Condition::matchesField(const QJsonValue& value) const
{
    const bool absent = value.isUndefined() || value.isNull() || (value.isString() && value.toString().isEmpty())
        || (value.isArray() && value.toArray().isEmpty());

    if (m_operator == Absent) {
        return absent;
    }
    if (m_operator == Present) {
        return !absent;
    }
    if (absent) {
        return m_operator == IsNot;     // Not the value: there is none
    }

    // A list: one of its values; is not: none of them is the value
    if (value.isArray())
    {
        const QJsonArray array = value.toArray();

        if (m_operator == IsNot)
        {
            Condition is = *this;
            is.m_operator = Is;
            return std::none_of(array.begin(), array.end(), [&is](const QJsonValue& v) { return is.matchesValue(v); });
        }

        return std::any_of(array.begin(), array.end(), [this](const QJsonValue& v) { return matchesValue(v); });
    }

    return matchesValue(value);
}

bool MessageCenterContact::Condition::matchesValue(const QJsonValue& value) const
{
    const QString valueText = value.isString() ? value.toString() : value.toVariant().toString();

    if (m_operator == Matches)
    {
        if (m_compiled.pattern().isEmpty() && !m_value.isEmpty()) {
            m_compiled = QRegularExpression(QRegularExpression::anchoredPattern(m_value));
        }

        return m_compiled.isValid() && m_compiled.match(valueText).hasMatch();
    }

    // Numbers as numbers: the capcode 0001234567 is 1234567, a bit error rate 0.01 is less than 0.1. Plain decimals
    // only: the capcode 3E005237593 is not 3 times 10 to the 5237593
    static const QRegularExpression decimal("^\\s*[+-]?[0-9]+(\\.[0-9]+)?\\s*$");
    const bool numbers = decimal.match(valueText).hasMatch() && decimal.match(m_value).hasMatch();
    const double a = numbers ? valueText.trimmed().toDouble() : 0.0;
    const double b = numbers ? m_value.trimmed().toDouble() : 0.0;

    switch (m_operator)
    {
    case Is:
        return numbers ? (a == b) : (valueText == m_value);
    case IsNot:
        return numbers ? (a != b) : (valueText != m_value);
    case Less:
        return numbers && (a < b);
    case LessOrEqual:
        return numbers && (a <= b);
    case Greater:
        return numbers && (a > b);
    case GreaterOrEqual:
        return numbers && (a >= b);
    default:
        return false;
    }
}

bool MessageCenterContact::matches(const QJsonObject& fields) const
{
    for (const Rule& rule : m_rules)
    {
        bool all = !rule.isEmpty();

        for (const Condition& condition : rule)
        {
            if (!condition.matches(fields))
            {
                all = false;
                break;
            }
        }

        if (all) {
            return true;
        }
    }

    return false;
}

QString MessageCenterContact::SendTo::check() const
{
    for (const Condition& condition : m_when)
    {
        const QString error = condition.check();

        if (!error.isEmpty()) {
            return error;
        }
    }

    if (m_page.value("transmitter").toString().trimmed().isEmpty()) {
        return "a send to has no transmitter";
    }

    // An address of a paging protocol: as a page has it (the same check as the WebAPI and the composer)
    const QString protocol = m_page.value("protocol").toString().toLower();

    if (RadioPagingPage::protocols().contains(protocol))
    {
        RadioPagingPage page;
        page.m_protocol = protocol;
        page.m_address = m_page.value("address").toVariant().toString().trimmed();
        page.m_function = m_page.value("function").toInt(0);
        const QString error = page.checkAddress();

        if (!error.isEmpty()) {
            return QString("send to %1: %2").arg(m_page.value("transmitter").toString(), error);
        }
    }
    else if (m_page.value("address").toVariant().toString().trimmed().isEmpty())
    {
        return "a send to has no address";
    }

    return QString();
}

QJsonObject MessageCenterContact::SendTo::toJson() const
{
    QJsonObject json;
    QJsonArray when;

    for (const Condition& c : m_when) {
        when.append(c.toJson());
    }

    if (!when.isEmpty()) {
        json["when"] = when;
    }

    json["page"] = m_page;
    return json;
}

MessageCenterContact::SendTo MessageCenterContact::SendTo::fromJson(const QJsonObject& json)
{
    SendTo s;

    for (const QJsonValue& v : json["when"].toArray()) {
        s.m_when.append(Condition::fromJson(v.toObject()));
    }

    s.m_page = json["page"].toObject();
    return s;
}

bool MessageCenterContact::pageFor(const QJsonObject& message, QJsonObject& page, QString *error) const
{
    for (const SendTo& sendTo : m_sendTo)
    {
        bool all = true;

        for (const Condition& condition : sendTo.m_when)
        {
            if (!condition.matches(message))
            {
                all = false;
                break;
            }
        }

        if (!all) {
            continue;
        }

        // The page as it was, with the fields of the send to (its transmitter and address)
        page = message;
        page.remove("contact");

        for (auto it = sendTo.m_page.begin(); it != sendTo.m_page.end(); ++it) {
            page.insert(it.key(), it.value());
        }

        return true;
    }

    if (error) {
        *error = m_sendTo.isEmpty() ? QString("%1 has no send to").arg(m_name) : QString("No send to of %1 for this page").arg(m_name);
    }

    return false;
}

QJsonObject MessageCenterContact::recordFields(const MessageRecord& record)
{
    QJsonObject fields = record.toJson();
    const QJsonObject data = fields.value("protocolData").toObject();

    for (auto it = data.begin(); it != data.end(); ++it)
    {
        if (!fields.contains(it.key())) {
            fields.insert(it.key(), it.value());
        }
    }

    return fields;
}

QString MessageCenterContact::ruleText(const Rule& rule)
{
    QStringList parts;

    for (const Condition& c : rule) {
        parts.append(c.text());
    }

    return parts.join(" and ");
}

QJsonObject MessageCenterContact::toJson() const
{
    QJsonObject json;
    QJsonArray rules;

    for (const Rule& rule : m_rules)
    {
        QJsonArray conditions;

        for (const Condition& c : rule) {
            conditions.append(c.toJson());
        }

        rules.append(conditions);
    }

    json["name"] = m_name;
    json["rules"] = rules;

    if (!m_sendTo.isEmpty())
    {
        QJsonArray sendTo;

        for (const SendTo& s : m_sendTo) {
            sendTo.append(s.toJson());
        }

        json["sendTo"] = sendTo;
    }

    if (!m_icon.isEmpty()) {
        json["icon"] = m_icon;
    }
    if (m_color != 0) {
        json["color"] = (double) m_color;
    }
    if (!m_note.isEmpty()) {
        json["note"] = m_note;
    }

    return json;
}

MessageCenterContact MessageCenterContact::fromJson(const QJsonObject& json)
{
    MessageCenterContact c;
    c.m_name = json["name"].toString().trimmed();

    for (const QJsonValue& r : json["rules"].toArray())
    {
        Rule rule;

        for (const QJsonValue& v : r.toArray()) {
            rule.append(Condition::fromJson(v.toObject()));
        }

        if (!rule.isEmpty()) {
            c.m_rules.append(rule);
        }
    }

    for (const QJsonValue& v : json["sendTo"].toArray()) {
        c.m_sendTo.append(SendTo::fromJson(v.toObject()));
    }

    c.m_icon = json["icon"].toString();
    c.m_color = (quint32) json["color"].toDouble(0);
    c.m_note = json["note"].toString();
    return c;
}

QByteArray MessageCenterContact::listToJson(const QList<MessageCenterContact>& contacts)
{
    QJsonArray array;

    for (const MessageCenterContact& c : contacts) {
        array.append(c.toJson());
    }

    return QJsonDocument(array).toJson(QJsonDocument::Compact);
}

QList<MessageCenterContact> MessageCenterContact::listFromJson(const QByteArray& json, QString *error)
{
    QList<MessageCenterContact> contacts;
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);

    if (error) {
        error->clear();
    }

    if (json.trimmed().isEmpty()) {
        return contacts;
    }
    if (!doc.isArray())
    {
        if (error) {
            *error = QString("Contacts: not a JSON array (%1)").arg(parseError.errorString());
        }
        return QList<MessageCenterContact>();
    }

    QStringList names;

    for (const QJsonValue& v : doc.array())
    {
        MessageCenterContact c = fromJson(v.toObject());
        QString why;

        if (c.m_name.isEmpty()) {
            why = "a contact has no name";
        } else if (names.contains(c.m_name)) {
            why = QString("two contacts are named %1").arg(c.m_name);
        }

        for (const Rule& rule : c.m_rules)
        {
            for (const Condition& condition : rule)
            {
                const QString conditionError = condition.check();

                if (why.isEmpty() && !conditionError.isEmpty()) {
                    why = QString("%1: %2").arg(c.m_name, conditionError);
                }
            }
        }
        for (const SendTo& sendTo : c.m_sendTo)
        {
            const QString sendError = sendTo.check();

            if (why.isEmpty() && !sendError.isEmpty()) {
                why = QString("%1: %2").arg(c.m_name, sendError);
            }
        }

        if (!why.isEmpty())
        {
            if (error) {
                *error = QString("Contacts: %1").arg(why);
            }
            return QList<MessageCenterContact>();
        }

        names.append(c.m_name);
        contacts.append(c);
    }

    return contacts;
}

const QList<QPair<QString, QStringList>>& MessageCenterContact::iconCategories()
{
    static const QList<QPair<QString, QStringList>> categories = {
        {"Emergency", QString("fire_engine ambulance police_car oncoming_police_car police_car_light fire fire_extinguisher hospital "
            "medical_symbol syringe pill stethoscope adhesive_bandage drop_of_blood rescue_workers_helmet sos_button warning no_entry "
            "stop_sign construction high_voltage collision prohibited radioactive biohazard red_exclamation_mark red_question_mark").split(' ')},
        {"Radio", QString("pager radio satellite_antenna antenna_bars satellite telephone telephone_receiver mobile_phone "
            "mobile_phone_with_arrow mobile_phone_off vibration_mode fax_machine studio_microphone microphone headphone loudspeaker "
            "megaphone speaker_high_volume speaker_medium_volume speaker_low_volume muted_speaker bell bell_with_slash postal_horn "
            "control_knobs level_slider television video_camera movie_camera camera film_projector videocassette laptop "
            "desktop_computer keyboard computer_mouse printer joystick floppy_disk computer_disk optical_disk battery low_battery "
            "electric_plug light_bulb flashlight magnet telescope microscope robot flying_saucer rocket globe_with_meridians link "
            "bar_chart chart_increasing chart_decreasing abacus input_numbers input_symbols input_latin_letters").split(' ')},
        {"Messages", QString("envelope incoming_envelope envelope_with_arrow e-mail inbox_tray outbox_tray postbox "
            "open_mailbox_with_raised_flag closed_mailbox_with_raised_flag memo spiral_notepad clipboard calendar pushpin "
            "round_pushpin paperclip").split(' ')},
        {"Weather", QString("cloud_with_lightning cloud_with_lightning_and_rain tornado water_wave snowflake thermometer droplet sun "
            "cloud_with_rain fog volcano mountain evergreen_tree deciduous_tree seedling dog cat horse cow bird fish honeybee").split(' ')},
        {"Places", QString("house house_with_garden office_building factory school bank hotel post_office classical_building church "
            "stadium tent hut castle department_store convenience_store station").split(' ')},
        {"Transport", QString("automobile taxi bus delivery_truck articulated_lorry tractor racing_car motorcycle bicycle locomotive "
            "railway_car tram ship ferry motor_boat sailboat anchor airplane small_airplane helicopter fuel_pump").split(' ')},
        {"People", QString("bust_in_silhouette busts_in_silhouette police_officer firefighter health_worker construction_worker guard "
            "mechanic farmer cook teacher technologist scientist pilot detective person baby older_person").split(' ')},
        {"Things", QString("key locked unlocked shield hammer_and_wrench wrench gear toolbox package alarm_clock stopwatch watch "
            "mantelpiece_clock timer_clock hourglass_done hourglass_not_done money_bag credit_card shopping_cart "
            "fork_and_knife_with_plate hot_beverage briefcase magnifying_glass_tilted_left globe_showing_europe-africa "
            "globe_showing_americas world_map compass").split(' ')},
        {"Signs", QString("information check_mark_button cross_mark recycling_symbol star glowing_star sparkles id_button new_button "
            "free_button ok_button cool_button play_button pause_button stop_button record_button fast-forward_button "
            "fast_reverse_button repeat_button shuffle_tracks_button radio_button musical_note musical_notes triangular_flag "
            "chequered_flag keycap_0 keycap_1 keycap_2 keycap_3 keycap_4 keycap_5 keycap_6 keycap_7 keycap_8 keycap_9").split(' ')},
        {"Colours", QString("red_heart orange_heart yellow_heart green_heart blue_heart purple_heart black_heart white_heart "
            "red_circle orange_circle yellow_circle green_circle blue_circle purple_circle brown_circle black_circle white_circle "
            "red_square orange_square yellow_square green_square blue_square purple_square brown_square black_large_square "
            "white_large_square red_triangle_pointed_down").split(' ')}
    };
    return categories;
}

QString MessageCenterContact::iconPath(const QString& icon)
{
    if (icon.startsWith(':') && !icon.startsWith(":/")) {
        return QString(":/messagecenter/icons/%1.svg").arg(icon.mid(1));
    }

    return icon;
}

// ---------------------------------------------------------------------------
// File sink and source
// ---------------------------------------------------------------------------

MessageCenterFileSink::MessageCenterFileSink(const QString& name, const QString& path, Format format) :
    m_name(name),
    m_path(path),
    m_format(format)
{
}

bool MessageCenterFileSink::accept(const MessageRecord& record, QString& error)
{
    QFile file(m_path);

    if (!file.open(QIODevice::Append | QIODevice::Text))
    {
        error = QString("Cannot write %1").arg(m_path);
        return false;
    }

    QByteArray line;

    if (m_format == JsonLines) {
        line = record.toJsonLine();
    }
    else if (m_format == PagerCsv)
    {
        QString row = RadioPagingPagerFormat::csvRow(record);

        // Not a message: nothing in that log
        if (row.isEmpty()) {
            return true;
        }
        if (file.size() == 0) {
            line += RadioPagingPagerFormat::csvHeader().join(',').toUtf8() + "\n";
        }

        line += row.toUtf8() + "\n";
    }
    else
    {
        if (file.size() == 0) {
            line += MessageRecord::csvHeader().join(',').toUtf8() + "\n";
        }

        line += record.toCSV().toUtf8() + "\n";
    }

    if (file.write(line) != line.size())
    {
        error = QString("Cannot write %1").arg(m_path);
        return false;
    }

    return true;
}

MessageCenterFileSource::MessageCenterFileSource(const QString& name, const QString& path) :
    m_name(name),
    m_path(path)
{
}

bool MessageCenterFileSource::poll(std::vector<MessageRecord>& records, QString& error)
{
    QFile file(m_path);

    if (!file.open(QIODevice::ReadOnly))
    {
        error = QString("Cannot read %1").arg(m_path);
        return false;
    }

    if (file.size() < m_offset) {
        m_offset = 0;           // The file was replaced
    }

    file.seek(m_offset);
    int lines = 0;
    m_more = false;

    // A big file is read over several polls, and a line has a limit
    while (!file.atEnd() && (lines++ < m_maxLinesPerPoll))
    {
        qint64 start = file.pos();
        QByteArray line = file.readLine(m_maxLineLength);

        if (!line.endsWith('\n') && (line.size() >= m_maxLineLength - 1))
        {
            // Not a record: skip to the end of the line
            file.readLine();
            m_offset = file.pos();
            m_skipped++;
            continue;
        }

        // A line being written is read when it is complete
        if (!line.endsWith('\n'))
        {
            file.seek(start);
            break;
        }

        m_offset = file.pos();
        line = line.trimmed();

        if (line.isEmpty()) {
            continue;
        }

        QJsonParseError parseError;
        QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
        MessageRecord record;

        if ((parseError.error != QJsonParseError::NoError) || !document.isObject()
            || !MessageRecord::fromJson(document.object(), record))
        {
            m_skipped++;
            continue;
        }

        records.push_back(record);
    }

    // Stopped by the number of lines: the rest is for the next poll
    m_more = (lines > m_maxLinesPerPoll) && !file.atEnd();
    return true;
}

// ---------------------------------------------------------------------------
// CSV source
// ---------------------------------------------------------------------------

MessageCenterCsvSource::MessageCenterCsvSource(const QString& name, const QString& path, bool newRowsOnly) :
    m_name(name),
    m_path(path),
    m_newRowsOnly(newRowsOnly)
{
}

bool MessageCenterCsvSource::poll(std::vector<MessageRecord>& records, QString& error)
{
    QFile file(m_path);

    if (!file.open(QIODevice::ReadOnly))
    {
        error = QString("Cannot read %1").arg(m_path);
        return false;
    }

    if (file.size() < m_offset)
    {
        // The file was replaced
        m_offset = 0;
        m_headerRead = false;
        m_notReported = true;
    }

    file.seek(m_offset);
    int lines = 0;
    m_more = false;

    while (!file.atEnd() && (lines++ < m_maxLinesPerPoll))
    {
        qint64 start = file.pos();
        QByteArray line = file.readLine(m_maxLineLength);

        if (!line.endsWith('\n') && (line.size() >= m_maxLineLength - 1))
        {
            // Not a row: skip to the end of the line
            file.readLine();
            m_offset = file.pos();
            m_skipped++;
            continue;
        }

        // A line being written is read when it is complete
        if (!line.endsWith('\n'))
        {
            file.seek(start);
            break;
        }

        m_offset = file.pos();
        QString text = QString::fromUtf8(line);

        if (text.trimmed().isEmpty()) {
            continue;
        }

        QTextStream in(&text, QIODevice::ReadOnly);
        QStringList row;

        if (!CSV::readRow(in, &row)) {
            m_skipped++;
            continue;
        }

        if (!m_headerRead)
        {
            m_columns.clear();

            for (int i = 0; i < row.size(); i++) {
                m_columns.insert(row[i].trimmed(), i);
            }

            m_headerRead = true;
            bool recordLog = m_columns.contains("id") && m_columns.contains("timestamp") && m_columns.contains("protocol");
            bool pagerLog = true;

            for (const QString& column : RadioPagingPagerFormat::csvColumns()) {
                pagerLog = pagerLog && m_columns.contains(column);
            }

            m_pagerFormat = !recordLog && pagerLog;

            if (!recordLog && !pagerLog)
            {
                // Not a log: nothing is read from this file until it is replaced
                m_columns.clear();
                m_offset = file.size();

                if (m_notReported)
                {
                    m_notReported = false;
                    error = QString("%1 is not a log of messages (the first row must name the columns)").arg(m_path);
                    return false;
                }

                return true;
            }

            // The rows written before: skipped, the next polls read the new ones
            if (m_newRowsOnly && !m_skipped0)
            {
                m_skipped0 = true;
                m_offset = file.size();
                return true;
            }

            continue;
        }

        if (m_columns.isEmpty()) {
            continue;
        }

        MessageRecord record;
        bool ok = m_pagerFormat ? RadioPagingPagerFormat::fromCsvRow(m_columns, row, record)
            : MessageRecord::fromCSV(m_columns, row, record);

        if (ok) {
            records.push_back(record);
        } else {
            m_skipped++;
        }
    }

    m_more = (lines > m_maxLinesPerPoll) && !file.atEnd();
    return true;
}

// ---------------------------------------------------------------------------
// UDP source and sink
// ---------------------------------------------------------------------------

MessageCenterUdpSource::MessageCenterUdpSource(const QString& name, const QString& address, quint16 port) :
    m_name(name),
    m_socket(new QUdpSocket())
{
    QHostAddress host(address.isEmpty() ? QString("127.0.0.1") : address);

    if (host.isNull()) {
        m_bindError = QString("%1 is not an IP address").arg(address);
    } else if ((port == 0) || !m_socket->bind(host, port)) {
        m_bindError = QString("Cannot listen on %1:%2: %3").arg(host.toString()).arg(port).arg(m_socket->errorString());
    }
}

MessageCenterUdpSource::~MessageCenterUdpSource()
{
}

bool MessageCenterUdpSource::poll(std::vector<MessageRecord>& records, QString& error)
{
    if (!m_bindError.isEmpty())
    {
        // Said once: the source stays there, nothing is received
        if (m_errorReported) {
            return true;
        }

        m_errorReported = true;
        error = m_bindError;
        return false;
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    for (int n = 0; (n < m_maxDatagramsPerPoll) && m_socket->hasPendingDatagrams(); n++)
    {
        qint64 size = m_socket->pendingDatagramSize();

        if ((size < 0) || (size > m_maxDatagramSize))
        {
            // Too big to be a message
            QByteArray discard(1, 0);
            m_socket->readDatagram(discard.data(), 1);
            m_skipped++;
            continue;
        }

        QByteArray datagram(size, 0);
        m_socket->readDatagram(datagram.data(), size);
        std::vector<MessageRecord> found;

        if (datagram.trimmed().startsWith('{'))
        {
            for (const QByteArray& line : datagram.split('\n'))
            {
                QJsonParseError parseError;
                QJsonDocument document = QJsonDocument::fromJson(line.trimmed(), &parseError);
                MessageRecord record;

                if ((parseError.error == QJsonParseError::NoError) && document.isObject() && MessageRecord::fromJson(document.object(), record)) {
                    found.push_back(record);
                } else if (!line.trimmed().isEmpty()) {
                    m_skipped++;
                }
            }
        }
        else
        {
            MessageRecord record;

            if (RadioPagingPagerFormat::fromDatagram(datagram, now, record)) {
                found.push_back(record);
            } else {
                m_skipped++;
            }
        }

        for (MessageRecord& record : found)
        {
            // Received from the network: a message received, never a page for a transmitter
            record.m_direction = MessageRecord::DirectionRX;
            record.m_protocolData.remove("transmitter");

            if (record.m_kind == MessageRecord::KindEvent) {
                continue;
            }

            records.push_back(record);
        }
    }

    return true;
}

MessageCenterUdpSink::MessageCenterUdpSink(const QString& name, const QString& address, quint16 port, Format format) :
    m_name(name),
    m_addressText(address),
    m_port(port),
    m_format(format),
    m_socket(new QUdpSocket()),
    m_address(address)
{
    m_addressValid = !m_address.isNull() && (port != 0);
}

MessageCenterUdpSink::~MessageCenterUdpSink()
{
}

bool MessageCenterUdpSink::accept(const MessageRecord& record, QString& error)
{
    if (!m_addressValid)
    {
        error = QString("%1: %2:%3 is not an address to send to").arg(m_name).arg(m_addressText).arg(m_port);
        return false;
    }

    QByteArray datagram = (m_format == Json) ? record.toJsonLine() : RadioPagingPagerFormat::datagram(record);

    // The end of a voice message, an event: nothing to send in the format
    if (datagram.isEmpty()) {
        return true;
    }
    if (datagram.size() > m_maxDatagramSize)
    {
        error = QString("%1: the message is too long for a datagram").arg(m_name);
        return false;
    }
    if (m_socket->writeDatagram(datagram, m_address, m_port) != datagram.size())
    {
        error = QString("%1: %2").arg(m_name).arg(m_socket->errorString());
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

MessageCenterCommandSink::MessageCenterCommandSink(const QString& name, const QString& command, bool env, bool standardInput,
    Launcher launcher) :
    m_name(name),
    m_command(command),
    m_env(env),
    m_stdin(standardInput),
    m_launcher(launcher)
{
    if (!m_launcher)
    {
        m_launcher = [](const Run& run) {
            // Its arguments only: detached. With an environment or a standard input: a process of its own, deleted
            // when it ends (its standard input written and closed at its start)
            if (run.m_environment.isEmpty() && run.m_stdin.isEmpty()) {
                return QProcess::startDetached(run.m_program, run.m_arguments);
            }

            QProcess *process = new QProcess();
            QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();

            for (const QString& variable : run.m_environment) {
                environment.insert(variable.section('=', 0, 0), variable.section('=', 1));
            }

            process->setProcessEnvironment(environment);
            process->setStandardOutputFile(QProcess::nullDevice());
            process->setStandardErrorFile(QProcess::nullDevice());
            QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), process, &QObject::deleteLater);
            QObject::connect(process, &QProcess::errorOccurred, process, [process](QProcess::ProcessError error) {
                if (error == QProcess::FailedToStart) {
                    process->deleteLater();
                }
            });
            process->start(run.m_program, run.m_arguments);

            if (!process->waitForStarted(1000)) {
                return false;
            }

            if (!run.m_stdin.isEmpty()) {
                process->write(run.m_stdin);
            }

            process->closeWriteChannel();
            return true;
        };
    }
}

QStringList MessageCenterCommandSink::environment(const MessageRecord& record)
{
    const QVariantMap& d = record.m_protocolData;
    return {
        "MC_ID=" + record.m_id,
        "MC_TIME=" + QDateTime::fromMSecsSinceEpoch(record.m_timestamp, Qt::UTC).toString(Qt::ISODate),
        "MC_PROTOCOL=" + record.m_protocol,
        "MC_ADDRESS=" + record.m_address,
        "MC_TYPE=" + record.m_msgType,
        "MC_TEXT=" + record.m_text,
        "MC_CHANNEL=" + record.m_channel,
        "MC_FREQUENCY=" + ((record.m_frequency < 0) ? QString() : QString::number(record.m_frequency)),     // Empty if unknown
        "MC_POLARITY=" + MessageRecord::polarityToString(record.m_polarity),
        "MC_QUALITY=" + MessageRecord::qualityToString(RadioPagingPage::quality(record)),
        "MC_CONTACT=" + d.value("contact").toString(),
        "MC_RECORD=" + QString::fromUtf8(QJsonDocument(record.toJson()).toJson(QJsonDocument::Compact))
    };
}

bool MessageCenterCommandSink::accept(const MessageRecord& record, const QStringList& captures, QString& error)
{
    // The messages only: not an update that completes one, nor a diagnostic
    if ((record.m_kind != MessageRecord::KindMessage) || RadioPagingPagerFormat::isCompletion(record)) {
        return true;
    }

    QStringList arguments = QProcess::splitCommand(m_command);

    if (arguments.isEmpty())
    {
        error = QString("%1: no command").arg(m_name);
        return false;
    }

    // The program is the one set; the text of the message is only ever an argument (no shell)
    Run run;
    run.m_program = arguments.takeFirst();

    for (QString& argument : arguments) {
        argument = messageCenterSubstitute(argument, record, captures);
    }

    run.m_arguments = arguments;

    if (m_env) {
        run.m_environment = environment(record);
    }
    if (m_stdin) {
        run.m_stdin = record.toJsonLine();
    }

    if (!m_launcher(run))
    {
        error = QString("%1: cannot run %2").arg(m_name).arg(run.m_program);
        return false;
    }

    return true;
}

MessageCenterActionSink::MessageCenterActionSink(const QString& name, const QString& type, const QString& text, quint32 color, Action action) :
    m_name(name),
    m_type(type),
    m_text(text),
    m_color(color),
    m_action(action)
{
}

bool MessageCenterActionSink::accept(const MessageRecord& record, const QStringList& captures, QString& error)
{
    (void) error;

    if ((record.m_kind != MessageRecord::KindMessage) || RadioPagingPagerFormat::isCompletion(record)) {
        return true;
    }

    if (m_action) {
        m_action(m_type, record, messageCenterSubstitute(m_text, record, captures), m_color);
    }

    return true;
}

// ---------------------------------------------------------------------------
// Endpoint
// ---------------------------------------------------------------------------

// Page
// ---------------------------------------------------------------------------

QString MessageCenterPage::stateName(State state)
{
    static const char *names[] = {"pending", "queued", "claimed", "transmitted", "sent", "dead"};
    return names[(int) state];
}

// ---------------------------------------------------------------------------
// Engine
// ---------------------------------------------------------------------------

// Taken by reference (std::max, std::min): defined here
const int MessageCenterEngine::m_minLogSize;
const int MessageCenterEngine::m_maxLogSize;

MessageCenterEngine::MessageCenterEngine() :
    m_logSize(m_defaultLogSize),
    m_routes(defaultRoutes())
{
}

MessageCenterEngine::~MessageCenterEngine()
{
}

void MessageCenterEngine::addSink(std::unique_ptr<MessageCenterSink> sink)
{
    removeSink(sink->name());
    m_sinks.push_back(std::move(sink));
}

bool MessageCenterEngine::removeSink(const QString& name)
{
    for (auto it = m_sinks.begin(); it != m_sinks.end(); ++it)
    {
        if ((*it)->name() == name)
        {
            m_sinks.erase(it);
            return true;
        }
    }

    return false;
}

QStringList MessageCenterEngine::sinkNames() const
{
    QStringList names({"log", "queue"});

    for (const auto& sink : m_sinks) {
        names.append(sink->name());
    }

    return names;
}

void MessageCenterEngine::addSource(std::unique_ptr<MessageCenterSource> source)
{
    removeSource(source->name());
    m_sources.push_back(std::move(source));
}

bool MessageCenterEngine::removeSource(const QString& name)
{
    for (auto it = m_sources.begin(); it != m_sources.end(); ++it)
    {
        if ((*it)->name() == name)
        {
            m_sources.erase(it);
            return true;
        }
    }

    return false;
}

QStringList MessageCenterEngine::sourceNames() const
{
    QStringList names;

    for (const auto& source : m_sources) {
        names.append(source->name());
    }

    return names;
}

int MessageCenterEngine::pollSources(QStringList& errors)
{
    int taken = 0;
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    for (const auto& source : m_sources)
    {
        std::vector<MessageRecord> records;
        QString error;

        if (!source->poll(records, error)) {
            errors.append(QString("%1: %2").arg(source->name()).arg(error));
        }

        for (const MessageRecord& record : records) {
            ingest(record, source->name(), now);
        }

        taken += (int) records.size();
    }

    return taken;
}

void MessageCenterEngine::setLogSize(int size)
{
    m_logSize = std::max(1, size);

    if ((int) m_log.size() > m_logSize)
    {
        while ((int) m_log.size() > m_logSize) {
            m_log.pop_front();
        }

        noteReset();
    }
}

// The same id is the same message: the later record (a voice message that ended, a page that was
// sent) completes the earlier one
void MessageCenterEngine::setContacts(const QList<MessageCenterContact>& contacts)
{
    m_contacts = contacts;
}

// The first contact a rule of which the record matches (the fields of the record made once for all)
const MessageCenterContact *MessageCenterEngine::contactOf(const MessageRecord& record) const
{
    if (m_contacts.isEmpty()) {
        return nullptr;
    }

    const QJsonObject fields = MessageCenterContact::recordFields(record);

    for (const MessageCenterContact& contact : m_contacts)
    {
        if (contact.matches(fields)) {
            return &contact;
        }
    }

    return nullptr;
}

void MessageCenterEngine::nameContact(MessageRecord& record) const
{
    const MessageCenterContact *contact = contactOf(record);

    record.m_protocolData.remove("contact");
    record.m_protocolData.remove("contact_icon");
    record.m_protocolData.remove("contact_color");

    if (contact)
    {
        record.m_protocolData.insert("contact", contact->m_name);

        if (!contact->m_icon.isEmpty()) {
            record.m_protocolData.insert("contact_icon", contact->m_icon);
        }
        if (contact->m_color != 0) {
            record.m_protocolData.insert("contact_color", (qulonglong) contact->m_color);
        }
    }
}

void MessageCenterEngine::noteChange(const QString& id)
{
    m_logged++;
    m_changes.emplace_back(m_logged, id);

    while ((int) m_changes.size() > m_maxChanges) {
        m_changes.pop_front();
    }
}

void MessageCenterEngine::noteReset()
{
    m_logged++;
    m_resetVersion = m_logged;
    m_changes.clear();
}

bool MessageCenterEngine::changedSince(int version, QStringList& ids) const
{
    ids.clear();

    // All the changes after version are kept: the oldest one kept is version + 1 or older
    if ((version < m_resetVersion) || (version > m_logged)
        || (!m_changes.empty() && (m_changes.front().first > version + 1)) || (m_changes.empty() && (version != m_logged))) {
        return false;
    }

    for (const auto& change : m_changes)
    {
        if (change.first > version) {
            ids.append(change.second);
        }
    }

    return true;
}

void MessageCenterEngine::store(const MessageRecord& record)
{
    noteChange(record.m_id);

    if (!record.m_id.isEmpty())
    {
        for (auto it = m_log.rbegin(); it != m_log.rend(); ++it)
        {
            if (it->m_id != record.m_id) {
                continue;
            }

            QVariantMap data = it->m_protocolData;

            for (auto d = record.m_protocolData.begin(); d != record.m_protocolData.end(); ++d) {
                data.insert(d.key(), d.value());
            }

            // What the later record sets completes the earlier one; what it leaves out stays
            MessageRecord merged = *it;
            merged.m_protocolData = data;

            if (!record.m_text.isEmpty() && (it->m_partial || !record.m_partial)) {
                merged.m_text = record.m_text;
                merged.m_charStatus = record.m_charStatus;
            }
            if (!record.m_msgType.isEmpty()) {
                merged.m_msgType = record.m_msgType;
            }
            if (!record.m_address.isEmpty()) {
                merged.m_address = record.m_address;
            }
            if (!record.m_mode.isEmpty()) {
                merged.m_mode = record.m_mode;
            }
            if (!record.m_channel.isEmpty()) {
                merged.m_channel = record.m_channel;
            }
            if (!record.m_protocol.isEmpty()) {
                merged.m_protocol = record.m_protocol;
            }
            if (record.m_rawBits >= 0) {
                merged.m_rawData = record.m_rawData;
                merged.m_rawBits = record.m_rawBits;
            }
            if (record.m_speed > 0) {
                merged.m_speed = record.m_speed;
            }
            if (record.m_frequency >= 0) {
                merged.m_frequency = record.m_frequency;
            }
            if (record.m_polarity != MessageRecord::PolarityUnknown) {
                merged.m_polarity = record.m_polarity;
            }

            // Only an update of a message still partial leaves it partial
            merged.m_partial = it->m_partial && record.m_partial;
            *it = merged;
            appendHistory(merged);
            return;
        }
    }

    m_log.push_back(record);
    appendHistory(record);

    while ((int) m_log.size() > m_logSize) {
        m_log.pop_front();
    }
}

bool MessageCenterEngine::replace(const MessageRecord& record)
{
    for (auto it = m_log.rbegin(); it != m_log.rend(); ++it)
    {
        if (it->m_id == record.m_id)
        {
            *it = record;
            noteChange(record.m_id);
            appendHistory(record);
            return true;
        }
    }

    return false;
}

void MessageCenterEngine::clearLog()
{
    m_log.clear();
    noteReset();    // The views show the empty log
}

bool MessageCenterEngine::setHistory(const QString& folder, int days, qint64 maxBytes, QString& error)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    MessageCenterArchive::Limits limits;
    limits.m_days = std::max(1, days);
    limits.m_bytes = std::max((qint64) 1, maxBytes);

    if (folder.isEmpty())
    {
        m_archive.reset();
        return true;
    }
    if (m_archive && (m_archive->folder() == folder))
    {
        m_archive->setLimits(limits, now);
        return true;
    }

    std::shared_ptr<MessageCenterArchive> archive(new MessageCenterArchive());

    if (!archive->open(folder, limits, now, error)) {
        return false;
    }

    m_archive = archive;

    // An empty log: the newest records of the history. Else (the history turned on later) the log goes into it
    if (m_log.empty())
    {
        for (const MessageRecord& record : m_archive->newest(m_logSize)) {
            m_log.push_back(record);
        }

        noteReset();    // The views show the log loaded
    }
    else
    {
        for (const MessageRecord& record : m_log) {
            m_archive->append(record, now);
        }
    }

    return true;
}

QString MessageCenterEngine::historyFolder() const
{
    return m_archive ? m_archive->folder() : QString();
}

qint64 MessageCenterEngine::historyBytes() const
{
    return m_archive ? m_archive->bytes() : 0;
}

bool MessageCenterEngine::clearHistory(QString& error)
{
    return m_archive ? m_archive->clear(error) : true;
}

std::shared_ptr<const MessageCenterArchive> MessageCenterEngine::archive() const
{
    return m_archive;
}

MessageCenterArchiveResult MessageCenterEngine::queryLog(qint64 from, qint64 to, const MessageCenterFilter& filter, int offset, int limit) const
{
    MessageCenterArchiveResult result;
    std::vector<const MessageRecord*> selected;

    for (const MessageRecord& record : m_log)
    {
        if (((from >= 0) && (record.m_timestamp < from)) || ((to >= 0) && (record.m_timestamp > to)) || !filter.matches(record)) {
            continue;
        }

        selected.push_back(&record);
    }

    std::stable_sort(selected.begin(), selected.end(),
        [](const MessageRecord *a, const MessageRecord *b) { return a->m_timestamp > b->m_timestamp; });
    result.m_total = (int) selected.size();

    for (int i = std::max(0, offset); (i < (int) selected.size()) && ((int) result.m_records.size() < limit); i++) {
        result.m_records.push_back(*selected[i]);
    }

    return result;
}

void MessageCenterEngine::appendHistory(const MessageRecord& record)
{
    QString error;

    if (m_archive && !m_archive->append(record, QDateTime::currentMSecsSinceEpoch(), &error)) {
        qWarning("MessageCenterEngine::appendHistory: %s", qPrintable(error));
    }
}

void MessageCenterEngine::deliver(const MessageRecord& record, const QString& sinkName, const QStringList& captures, qint64 now)
{
    if (sinkName == "log")
    {
        store(record);
        m_sinkStats[sinkName].m_accepted++;
        return;
    }
    if (sinkName == "queue")
    {
        // Only a message for a transmitter becomes a page
        if ((record.m_direction == MessageRecord::DirectionTX) && !record.m_protocolData.value("transmitter").toString().isEmpty())
        {
            addPage(record, now);
            m_sinkStats[sinkName].m_accepted++;
        }

        return;
    }

    // The limit of the sink: over it the message waits (behind those waiting already) or is dropped
    auto it = m_rates.find(sinkName);

    if ((it != m_rates.end()) && (it->m_limit.m_count > 0))
    {
        RateState& rate = it.value();

        while (!rate.m_sent.empty() && (rate.m_sent.front() <= now - rate.m_limit.m_periodMs)) {
            rate.m_sent.pop_front();
        }

        if (!rate.m_held.empty() || ((int) rate.m_sent.size() >= rate.m_limit.m_count))
        {
            SinkStat& stat = m_sinkStats[sinkName];

            // Said once each time the limit is reached
            if (!rate.m_reported && (m_sinkErrors.size() < m_maxSinkErrors))
            {
                m_sinkErrors.append(QString("%1: over its limit of %2 in %3 s, the messages %4").arg(sinkName)
                    .arg(rate.m_limit.m_count).arg(rate.m_limit.m_periodMs / 1000).arg(rate.m_limit.m_hold ? "wait" : "are dropped"));
            }

            rate.m_reported = true;

            if (rate.m_limit.m_hold)
            {
                rate.m_held.emplace_back(record, captures);

                if ((int) rate.m_held.size() > m_maxHeld)
                {
                    rate.m_held.pop_front();    // The oldest one waiting goes
                    stat.m_dropped++;
                    stat.m_failed++;
                }
            }
            else
            {
                stat.m_dropped++;
                stat.m_failed++;
            }

            stat.m_waiting = (int) rate.m_held.size();
            return;
        }

        rate.m_sent.push_back(now);
        rate.m_reported = false;
    }

    send(record, sinkName, captures);
}

void MessageCenterEngine::send(const MessageRecord& record, const QString& sinkName, const QStringList& captures)
{
    for (const auto& sink : m_sinks)
    {
        if (sink->name() == sinkName)
        {
            QString error;

            if (sink->accept(record, captures, error))
            {
                m_sinkStats[sinkName].m_accepted++;
            }
            else
            {
                m_sinkStats[sinkName].m_failed++;

                if (m_sinkErrors.size() < m_maxSinkErrors) {
                    m_sinkErrors.append(error);
                }
            }

            return;
        }
    }
}

void MessageCenterEngine::setRateLimit(const QString& sink, const RateLimit& limit)
{
    if (limit.m_count <= 0)
    {
        // No limit: those waiting go now
        if (m_rates.contains(sink))
        {
            for (const auto& held : m_rates[sink].m_held) {
                send(held.first, sink, held.second);
            }

            m_rates.remove(sink);
            m_sinkStats[sink].m_waiting = 0;
        }

        return;
    }

    m_rates[sink].m_limit = limit;
}

void MessageCenterEngine::releaseHeld(qint64 now)
{
    for (auto it = m_rates.begin(); it != m_rates.end(); ++it)
    {
        RateState& rate = it.value();

        while (!rate.m_sent.empty() && (rate.m_sent.front() <= now - rate.m_limit.m_periodMs)) {
            rate.m_sent.pop_front();
        }

        while (!rate.m_held.empty() && ((int) rate.m_sent.size() < rate.m_limit.m_count))
        {
            const std::pair<MessageRecord, QStringList> held = rate.m_held.front();
            rate.m_held.pop_front();
            rate.m_sent.push_back(now);
            send(held.first, it.key(), held.second);
        }

        if (rate.m_held.empty()) {
            rate.m_reported = false;
        }

        m_sinkStats[it.key()].m_waiting = (int) rate.m_held.size();
    }
}

bool MessageCenterEngine::pageToContact(const MessageRecord& record, const QString& name, MessageRecord& pageRecord,
    QString& error) const
{
    const auto contact = std::find_if(m_contacts.begin(), m_contacts.end(),
        [&name](const MessageCenterContact& c) { return c.m_name == name; });

    if (contact == m_contacts.end())
    {
        error = QString("no contact %1").arg(name);
        return false;
    }

    const QVariantMap& d = record.m_protocolData;
    RadioPagingPage paging;
    paging.m_type = record.m_msgType.isEmpty() ? QString("alphanumeric") : record.m_msgType;
    paging.m_text = record.m_text;

    if (paging.m_type == "voice") {
        paging.m_voiceFile = d.value("voice_file").toString();
    }

    // The fields the conditions of its send to look at, as for a page sent to it (Send, the API)
    QJsonObject message;
    message["contact"] = name;
    message["protocol"] = record.m_protocol;
    message["type"] = paging.m_type;
    message["text"] = paging.m_text;
    QJsonObject sent;

    if (!contact->pageFor(message, sent, &error)) {
        return false;
    }

    paging.m_transmitter = sent.value("transmitter").toString();
    paging.m_protocol = sent.value("protocol").toString(record.m_protocol).toLower();
    paging.m_address = sent.value("address").toVariant().toString();
    paging.m_function = sent.value("function").toInt(paging.m_function);
    paging.m_speed = sent.value("speed").toInt(paging.m_speed);

    if (sent.contains("polarity"))
    {
        const QString polarity = sent.value("polarity").toString().toLower();
        paging.m_polarity = (polarity == "normal") ? 0 : (polarity == "inverted") ? 1 : -1;
    }

    if (paging.m_transmitter.isEmpty())
    {
        error = QString("the send to of %1 has no transmitter").arg(name);
        return false;
    }

    error = paging.check();

    if (!error.isEmpty()) {
        return false;
    }

    pageRecord = paging.toRecord();
    pageRecord.m_protocolData.insert("relay_of", record.m_id);     // The message it was made from
    return true;
}

void MessageCenterEngine::importRecord(const MessageRecord& record)
{
    store(record);
}

QString MessageCenterEngine::endpointKey(MessageEndpoint::Role role, const QString& alias)
{
    return QString(role == MessageEndpoint::Transmitter ? "tx:" : "rx:") + alias;
}

void MessageCenterEngine::ingest(const MessageRecord& record, const QString& source, qint64 now)
{
    const QVariantMap& d = record.m_protocolData;

    // The announcements of the channels and the states of the pages come from the channels, not
    // from a file that is read (a replayed file must not move the live pages)
    if (record.m_kind == MessageRecord::KindEvent)
    {
        if (sourceNames().contains(source)) {
            return;
        }

        MessageEndpoint endpoint;
        QString event;

        if (MessageEndpoint::fromRecord(record, endpoint, event))
        {
            // A receiver and a transmitter may have the same alias (the frequency): they are two endpoints
            QString key = endpointKey(endpoint.m_role, endpoint.m_channel);

            // A channel whose alias changed (the alias is the frequency by default): its entry of the old alias goes
            if ((record.m_deviceSetIndex >= 0) && (record.m_channelIndex >= 0))
            {
                const QString channel = QString("%1:%2").arg(record.m_deviceSetIndex).arg(record.m_channelIndex);

                for (auto it = m_endpointChannels.begin(); it != m_endpointChannels.end();)
                {
                    if ((it.value() == channel) && (it.key() != key) && m_endpoints.contains(it.key())
                        && (m_endpoints[it.key()].m_role == endpoint.m_role))
                    {
                        if (endpoint.m_role == MessageEndpoint::Transmitter) {
                            endpointGone(m_endpoints[it.key()].m_channel, now);
                        }

                        m_endpoints.remove(it.key());
                        it = m_endpointChannels.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }

                m_endpointChannels[key] = channel;
            }

            // Every announcement (status too) carries the whole description: the protocols, speeds and
            // capabilities follow the settings of the channel
            m_endpoints[key] = endpoint;

            if ((m_endpoints[key].m_status == MessageEndpoint::Offline) && (endpoint.m_role == MessageEndpoint::Transmitter)) {
                endpointGone(endpoint.m_channel, now);
            }

            return;
        }
        if (d.value("event").toString() == "page")
        {
            applyEvent(record.m_id, d.value("state").toString(), d.value("reason").toString(), now);
            return;
        }
    }

    // The messages: the rules in their order (applyRules); then its deliveries, with the message as the changes left
    // it. The log is a delivery as the others: the default rule logs every message. The message has the name of its
    // contact first: the rules select by it, the sinks have it
    MessageRecord named = record;
    nameContact(named);
    std::vector<Delivery> deliveries;
    const MessageRecord changed = applyRules(named, source, deliveries);
    QSet<QString> delivered;

    for (const Delivery& delivery : deliveries)
    {
        // A sink takes a message once, whatever the number of rules that send it there (a contact is paged once)
        const QString key = delivery.m_sink + '\n' + delivery.m_contact;

        if (delivered.contains(key)) {
            continue;
        }

        delivered.insert(key);

        if (delivery.m_contact.isEmpty())
        {
            deliver(changed, delivery.m_sink, delivery.m_captures, now);
            continue;
        }

        // A page to a contact: a new page of its own, queued (not walked through the rules again)
        MessageRecord made;
        QString error;

        if (pageToContact(changed, delivery.m_contact, made, error) && !addPage(made, now).isEmpty())
        {
            m_sinkStats["queue"].m_accepted++;
        }
        else
        {
            m_sinkStats["queue"].m_failed++;

            if (m_sinkErrors.size() < m_maxSinkErrors) {
                m_sinkErrors.append(QString("Page to %1: %2").arg(delivery.m_contact, error.isEmpty() ? QString("not queued") : error));
            }
        }
    }

    if (!deliveries.empty()) {
        m_routed++;
    }
}

// ---------------------------------------------------------------------------
// Rules
// ---------------------------------------------------------------------------

namespace {

const char * const actionNames[MessageCenterEngine::Route::ActionCount] = {
    "log", "send", "page", "mark", "decode", "transform", "replace", "drop"
};

} // namespace

QString MessageCenterEngine::Route::actionName(Action action)
{
    return ((action >= 0) && (action < ActionCount)) ? QString(actionNames[action]) : QString("log");
}

MessageCenterEngine::Route::Action MessageCenterEngine::Route::actionOf(const QString& name)
{
    for (int a = 0; a < ActionCount; a++)
    {
        if (name == actionNames[a]) {
            return (Action) a;
        }
    }

    return ActionLog;
}

QString MessageCenterEngine::Route::pageToName(PageTo to)
{
    return (to == PageContact) ? QString("contact") : (to == PageMessageContact) ? QString("messageContact") : QString("transmitter");
}

MessageCenterEngine::Route::PageTo MessageCenterEngine::Route::pageToOf(const QString& name)
{
    return (name == "contact") ? PageContact : (name == "messageContact") ? PageMessageContact : PageTransmitter;
}

QString MessageCenterEngine::Route::check() const
{
    const QString error = m_filter.errors();

    if (!error.isEmpty()) {
        return QString("Rule %1: %2").arg(m_name, error);
    }
    if ((m_action == ActionSend) && m_target.trimmed().isEmpty()) {
        return QString("Rule %1: send to where").arg(m_name);
    }
    if ((m_action == ActionPage) && (m_pageTo == PageContact) && m_target.trimmed().isEmpty()) {
        return QString("Rule %1: page which contact").arg(m_name);
    }
    if ((m_action == ActionMark) && m_mark.value("icon").toString().isEmpty() && (m_mark.value("color").toUInt() == 0)
        && (m_mark.value("background").toUInt() == 0)) {
        return QString("Rule %1: a mark needs an icon, a colour of the text or a colour of the row").arg(m_name);
    }
    if ((m_action == ActionDecode) && !decodes()) {
        return QString("Rule %1: decode as what (a codepage or a type)").arg(m_name);
    }
    if ((m_action == ActionTransform) || (m_action == ActionReplace))
    {
        if (m_pattern.isEmpty()) {
            return QString("Rule %1: %2 needs an expression").arg(m_name, actionName(m_action));
        }

        const QRegularExpression expression(m_pattern);

        if (!expression.isValid()) {
            return QString("Rule %1: %2: %3").arg(m_name, m_pattern, expression.errorString());
        }
    }

    return QString();
}

bool MessageCenterEngine::Route::apply(MessageRecord& record) const
{
    switch (m_action)
    {
    case ActionMark:
    {
        // Several rules mark a message: their marks side by side, each once
        QVariantList marks = record.m_protocolData.value("marks").toList();
        QVariantMap mark = m_mark;
        mark.insert("rule", m_name);

        if (!marks.contains(mark))
        {
            marks.append(mark);
            record.m_protocolData.insert("marks", marks);
        }

        return true;
    }
    case ActionDecode:
    {
        MessageRecord decoded;
        QString error;

        if (decodes() && RadioPagingRecode::redecode(record, m_decodeCharset, m_decodeType, decoded, error)) {
            record = decoded;
        }

        return true;
    }
    case ActionTransform:
    case ActionReplace:
    {
        const QRegularExpression expression(m_pattern, m_caseInsensitive ? QRegularExpression::CaseInsensitiveOption
            : QRegularExpression::NoPatternOption);

        if (!expression.isValid()) {
            return true;
        }

        // The groups of a match: by number and by name; \1 in the template is ${1}
        static const QRegularExpression backReference("\\\\(\\d+)");
        QString tmpl = m_template;
        tmpl.replace(backReference, "${\\1}");
        auto fill = [&record, &tmpl, &expression](const QRegularExpressionMatch& match) {
            QMap<QString, QString> named;

            for (const QString& name : expression.namedCaptureGroups())
            {
                if (!name.isEmpty()) {
                    named.insert(name, match.captured(name));
                }
            }

            return messageCenterSubstitute(tmpl, record, match.capturedTexts(), named);
        };

        const QString before = record.m_text;
        QString after;

        if (m_action == ActionTransform)
        {
            // The whole text becomes the template, when the expression is found in it
            const QRegularExpressionMatch match = expression.match(before);

            if (!match.hasMatch()) {
                return true;
            }

            after = fill(match);
        }
        else
        {
            // Each match (the first one only) replaced, the rest of the text as it is
            QRegularExpressionMatchIterator it = expression.globalMatch(before);
            int pos = 0;

            while (it.hasNext())
            {
                const QRegularExpressionMatch match = it.next();

                if (match.capturedLength() == 0) {
                    continue;   // An empty match changes nothing (and would repeat)
                }

                after += before.mid(pos, match.capturedStart() - pos) + fill(match);
                pos = match.capturedEnd();

                if (!m_all) {
                    break;
                }
            }

            after += before.mid(pos);
        }

        if (after != before)
        {
            if (!record.m_protocolData.contains("original_text")) {
                record.m_protocolData.insert("original_text", before);
            }

            record.m_text = after;
            record.m_charStatus.clear();      // The status of the characters was of the text received
            record.m_protocolData.remove("char_status");
        }

        return true;
    }
    default:
        return false;
    }
}

MessageRecord MessageCenterEngine::applyRules(const MessageRecord& record, const QString& source,
    std::vector<Delivery>& deliveries) const
{
    MessageRecord working = record;
    working.m_protocolData.remove("marks");     // Made again by the rules (a message decoded again, imported)
    deliveries.clear();

    for (const Route& route : m_routes)
    {
        if (!route.m_enabled || !route.m_filter.matches(working, source)) {
            continue;
        }

        switch (route.m_action)
        {
        case Route::ActionLog:
            deliveries.push_back(Delivery{QString("log"), route.m_filter.captures(working), QString()});
            break;
        case Route::ActionPage:
        {
            // To a contact: the one of the rule, or the one of the message (none: nothing to page)
            const QString contact = (route.m_pageTo == Route::PageContact) ? route.m_target
                : (route.m_pageTo == Route::PageMessageContact) ? working.m_protocolData.value("contact").toString() : QString();

            if ((route.m_pageTo == Route::PageTransmitter) || !contact.isEmpty()) {
                deliveries.push_back(Delivery{QString("queue"), route.m_filter.captures(working), contact});
            }

            break;
        }
        case Route::ActionSend:
            deliveries.push_back(Delivery{route.m_target, route.m_filter.captures(working), QString()});
            break;
        case Route::ActionDrop:
            return working;
        default:
            route.apply(working);
            break;
        }
    }

    return working;
}

std::vector<MessageCenterEngine::Route> MessageCenterEngine::defaultRoutes()
{
    Route all;
    all.m_name = "All messages to the log";
    all.m_action = Route::ActionLog;
    return {all};
}

std::vector<MessageRecord> MessageCenterEngine::select(const MessageCenterFilter& filter) const
{
    std::vector<MessageRecord> out;

    for (const MessageRecord& record : m_log)
    {
        if (filter.matches(record)) {
            out.push_back(record);
        }
    }

    return out;
}

const MessageRecord *MessageCenterEngine::find(const QString& id) const
{
    for (auto it = m_log.rbegin(); it != m_log.rend(); ++it)
    {
        if (it->m_id == id) {
            return &*it;
        }
    }

    return nullptr;
}

// The record of a page in the log, until the transmitter's record (with its channel) replaces it, has the
// state of the page and its reason in protocolData page_state and page_reason
bool MessageCenterEngine::markLogged(const QString& id, const QString& state, const QString& reason)
{
    for (auto it = m_log.rbegin(); it != m_log.rend(); ++it)
    {
        if (it->m_id != id) {
            continue;
        }
        if ((it->m_direction != MessageRecord::DirectionTX) || !it->m_channel.isEmpty()
            || ((it->m_protocolData.value("page_state").toString() == state) && (it->m_protocolData.value("page_reason").toString() == reason))) {
            return false;
        }

        it->m_protocolData.insert("page_state", state);

        if (reason.isEmpty()) {
            it->m_protocolData.remove("page_reason");
        } else {
            it->m_protocolData.insert("page_reason", reason);
        }

        noteChange(id);     // The views show the new state
        return true;
    }

    return false;
}

int MessageCenterEngine::syncLoggedPages()
{
    int changed = 0;

    for (const MessageCenterPage& page : m_pages)
    {
        if (markLogged(page.m_record.m_id, MessageCenterPage::stateName(page.m_state), page.m_reason)) {
            changed++;
        }
    }

    return changed;
}

QStringList MessageCenterEngine::eligible(const MessageRecord& page) const
{
    QStringList aliases;

    for (auto it = m_endpoints.begin(); it != m_endpoints.end(); ++it)
    {
        if ((it->m_status != MessageEndpoint::Offline) && it->canTransmit(page)) {
            aliases.append(it->m_channel);
        }
    }

    return aliases;
}

QString MessageCenterEngine::addPage(MessageRecord record, qint64 now)
{
    if (record.m_id.isEmpty()) {
        record.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    // A page has its transmitter and an id of its own
    if (record.m_protocolData.value("transmitter").toString().isEmpty() || findPage(record.m_id)) {
        return QString();
    }

    record.m_direction = MessageRecord::DirectionTX;
    record.m_kind = MessageRecord::KindMessage;

    if (record.m_timestamp <= 0) {
        record.m_timestamp = now;
    }

    nameContact(record);
    MessageCenterPage page;
    page.m_record = record;
    page.m_changed = now;
    m_pages.push_back(page);
    store(record);
    return record.m_id;
}

MessageCenterPage *MessageCenterEngine::findPage(const QString& id)
{
    for (MessageCenterPage& page : m_pages)
    {
        if (page.m_record.m_id == id) {
            return &page;
        }
    }

    return nullptr;
}

const MessageCenterPage *MessageCenterEngine::page(const QString& id) const
{
    for (const MessageCenterPage& page : m_pages)
    {
        if (page.m_record.m_id == id) {
            return &page;
        }
    }

    return nullptr;
}

std::vector<MessageCenterPage> MessageCenterEngine::pages(int state) const
{
    std::vector<MessageCenterPage> out;

    for (const MessageCenterPage& page : m_pages)
    {
        if ((state < 0) || ((int) page.m_state == state)) {
            out.push_back(page);
        }
    }

    return out;
}

// Pending -> queued when the transmitter named on the page is there and can send it; queued ->
// dispatched when it is ready. A page that has used its attempts is dead
std::vector<MessageCenterPage> MessageCenterEngine::dispatch(qint64 now)
{
    std::vector<MessageCenterPage> out;
    QMap<QString, int> outstanding;

    for (const MessageCenterPage& page : m_pages)
    {
        if (page.m_dispatched) {
            outstanding[page.transmitter()]++;
        }
    }

    for (MessageCenterPage& page : m_pages)
    {
        const QString alias = page.transmitter();
        auto endpoint = m_endpoints.constFind(endpointKey(MessageEndpoint::Transmitter, alias));

        if (page.m_state == MessageCenterPage::Pending)
        {
            if ((endpoint != m_endpoints.constEnd()) && (endpoint->m_status != MessageEndpoint::Offline)
                && endpoint->canTransmit(page.m_record))
            {
                page.m_state = MessageCenterPage::Queued;
                page.m_changed = now;

                if (page.m_reason.startsWith("waiting:")) {
                    page.m_reason.clear();      // A rejection is kept for the next attempt
                }
            }
            else
            {
                // What the page waits for, so that the GUI and the API say why it is not sent
                QString waiting;

                if (endpoint == m_endpoints.constEnd()) {
                    waiting = QString("waiting: no transmitter %1").arg(alias);
                } else if (endpoint->m_status == MessageEndpoint::Offline) {
                    waiting = QString("waiting: %1 is offline").arg(alias);
                } else if (!endpoint->m_protocols.isEmpty() && !endpoint->m_protocols.contains(page.m_record.m_protocol, Qt::CaseInsensitive)) {
                    waiting = QString("waiting: %1 sends %2 now, not %3").arg(alias, endpoint->m_protocols.join(", "), page.m_record.m_protocol);
                } else {
                    waiting = QString("waiting: %1 does not send this speed or polarity now").arg(alias);
                }

                page.m_reason = waiting;
            }
        }

        if ((page.m_state != MessageCenterPage::Queued) || page.m_dispatched) {
            continue;
        }

        if (page.m_attempts >= m_maxAttempts)
        {
            page.m_state = MessageCenterPage::Dead;
            page.m_reason = "attempts";
            page.m_changed = now;
            continue;
        }

        if ((endpoint == m_endpoints.constEnd()) || !endpoint->ready()
            || ((endpoint->m_capacity > 0) && (endpoint->m_queueDepth + outstanding[alias] >= endpoint->m_capacity))) {
            continue;
        }

        page.m_dispatched = true;
        page.m_dispatchedAt = now;
        page.m_changed = now;
        outstanding[alias]++;
        out.push_back(page);
    }

    return out;
}

bool MessageCenterEngine::applyEvent(const QString& id, const QString& state, const QString& reason, qint64 now)
{
    MessageCenterPage *page = findPage(id);

    if (!page) {
        return false;
    }

    auto failed = [&](const QString& why)
    {
        // Out of attempts: dead. Else it goes back to the queue to be sent again
        page->m_dispatched = false;
        page->m_claimedBy.clear();
        page->m_changed = now;

        if (page->m_attempts >= m_maxAttempts)
        {
            page->m_state = MessageCenterPage::Dead;
            page->m_reason = QString("attempts (%1)").arg(why);
        }
        else
        {
            page->m_state = MessageCenterPage::Queued;
            page->m_retransmits++;
            page->m_reason = why;
        }
    };

    // A late or repeated report does not move a page that is somewhere else already
    const MessageCenterPage::State current = page->m_state;
    const bool claimed = (current == MessageCenterPage::Claimed);
    const bool transmitted = (current == MessageCenterPage::Transmitted);

    if (state == "claimed")
    {
        if ((current != MessageCenterPage::Queued) && (current != MessageCenterPage::Pending)) {
            return false;
        }
        if (page->m_attempts >= m_maxAttempts)
        {
            page->m_state = MessageCenterPage::Dead;
            page->m_reason = "attempts";
            page->m_dispatched = false;
            page->m_changed = now;
            return false;
        }

        page->m_state = MessageCenterPage::Claimed;
        page->m_attempts++;
        page->m_dispatched = false;
        page->m_claimedBy = page->transmitter();
        page->m_reason.clear();
    }
    else if (state == "transmitted")
    {
        if (!claimed) {
            return false;
        }

        page->m_state = MessageCenterPage::Transmitted;
    }
    else if (state == "sent")
    {
        if (!claimed && !transmitted) {
            return false;
        }

        page->m_state = MessageCenterPage::Sent;
        page->m_dispatched = false;
        page->m_reason.clear();
    }
    else if (state == "rejected")
    {
        if ((current == MessageCenterPage::Sent) || (current == MessageCenterPage::Dead) || transmitted) {
            return false;
        }

        page->m_state = MessageCenterPage::Dead;
        page->m_reason = reason.isEmpty() ? QString("rejected") : QString("rejected: %1").arg(reason);
        page->m_dispatched = false;
    }
    else if ((state == "aborted") || (state == "underrun") || (state == "interrupted"))
    {
        if (!claimed && !transmitted) {
            return false;
        }

        failed(state);
    }
    else if (state == "released")
    {
        // Nothing went on air: it waits for the transmitter
        if (!claimed && (current != MessageCenterPage::Queued)) {
            return false;
        }

        page->m_state = MessageCenterPage::Pending;
        page->m_dispatched = false;
        page->m_claimedBy.clear();
    }
    else
    {
        return false;
    }

    page->m_changed = now;
    return true;
}

// A transmitter gone: what it had claimed and not transmitted waits for it; what it was
// transmitting is interrupted
int MessageCenterEngine::removeEndpoints(const QStringList& keys, bool offline, qint64 now)
{
    int removed = 0;

    for (auto it = m_endpoints.begin(); it != m_endpoints.end();)
    {
        if ((offline && (it->m_status == MessageEndpoint::Offline)) || keys.contains(it.key()))
        {
            if (it->m_role == MessageEndpoint::Transmitter) {
                endpointGone(it->m_channel, now);
            }

            m_endpointChannels.remove(it.key());
            it = m_endpoints.erase(it);
            removed++;
        }
        else
        {
            ++it;
        }
    }

    return removed;
}

void MessageCenterEngine::endpointGone(const QString& alias, qint64 now)
{
    for (MessageCenterPage& page : m_pages)
    {
        if (page.transmitter() != alias) {
            continue;
        }

        if ((page.m_state == MessageCenterPage::Claimed) || (page.m_state == MessageCenterPage::Queued))
        {
            page.m_state = MessageCenterPage::Pending;
            page.m_dispatched = false;
            page.m_claimedBy.clear();
            page.m_changed = now;
        }
        else if (page.m_state == MessageCenterPage::Transmitted) {
            applyEvent(page.m_record.m_id, "interrupted", QString(), now);
        }
    }
}

void MessageCenterEngine::expireClaims(qint64 now, qint64 timeoutMs)
{
    for (MessageCenterPage& page : m_pages)
    {
        if (!page.m_dispatched || (now - page.m_dispatchedAt < timeoutMs)) {
            continue;
        }

        page.m_dispatched = false;
        page.m_attempts++;
        page.m_retransmits++;
        page.m_changed = now;

        if (page.m_attempts >= m_maxAttempts)
        {
            page.m_state = MessageCenterPage::Dead;
            page.m_reason = "attempts (claim timed out)";
        }
        else {
            page.m_reason = "claim timed out";
        }
    }
}

// A channel that has not announced itself for this long is gone: its transmitter's pages are
// released or interrupted, it is offline until it announces again
void MessageCenterEngine::sweep(qint64 now, qint64 silentMs)
{
    for (auto it = m_endpoints.begin(); it != m_endpoints.end(); ++it)
    {
        if ((it->m_status == MessageEndpoint::Offline) || (now - it->m_lastSeen <= silentMs)) {
            continue;
        }

        it->m_status = MessageEndpoint::Offline;

        if (it->m_role == MessageEndpoint::Transmitter) {
            endpointGone(it->m_channel, now);
        }
    }
}

QStringList MessageCenterEngine::takeSinkErrors()
{
    QStringList errors = m_sinkErrors;
    m_sinkErrors.clear();
    return errors;
}

bool MessageCenterEngine::requeue(const QString& id, qint64 now)
{
    MessageCenterPage *page = findPage(id);

    if (!page || (page->m_state != MessageCenterPage::Dead)) {
        return false;
    }

    page->m_state = MessageCenterPage::Pending;
    page->m_attempts = 0;
    page->m_retransmits = 0;
    page->m_dispatched = false;
    page->m_claimedBy.clear();
    page->m_reason.clear();
    page->m_changed = now;
    return true;
}

// A page nobody has claimed can be withdrawn
bool MessageCenterEngine::cancel(const QString& id)
{
    for (auto it = m_pages.begin(); it != m_pages.end(); ++it)
    {
        if (it->m_record.m_id != id) {
            continue;
        }
        if (!it->m_dispatched && ((it->m_state == MessageCenterPage::Pending) || (it->m_state == MessageCenterPage::Queued)
            || (it->m_state == MessageCenterPage::Dead)))
        {
            markLogged(id, "cancelled", QString());
            m_pages.erase(it);
            return true;
        }

        return false;
    }

    return false;
}

int MessageCenterEngine::removeFinished()
{
    size_t before = m_pages.size();
    m_pages.erase(std::remove_if(m_pages.begin(), m_pages.end(),
        [](const MessageCenterPage& p) { return p.m_state == MessageCenterPage::Sent; }), m_pages.end());
    return (int) (before - m_pages.size());
}
