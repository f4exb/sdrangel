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

#include <cmath>
#include <algorithm>
#include <limits>

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QRegularExpression>

#include "dsp/dv/dvangel61.h"

#include "dsemodtxsession.h"
#include "dsemodpacketbuilder.h"

bool DSEModTxSession::keys(const QJsonObject& o, const QStringList& allowed)
{
    for (auto i = o.begin(); i != o.end(); ++i)
    {
        if (!allowed.contains(i.key())) {
            return false;
        }
    }

    return true;
}

bool DSEModTxSession::integer(const QJsonValue& v, int low, int high)
{
    return v.isDouble() && std::isfinite(v.toDouble()) && (v.toDouble() >= low)
        && (v.toDouble() <= high) && (std::floor(v.toDouble()) == v.toDouble());
}

bool DSEModTxSession::decodeBase64(const QJsonValue& v, QByteArray& bytes)
{
    if (!v.isString() || (v.toString().size() > (MaxChunkBytes + 2) / 3 * 4)) {
        return false;
    }

    const QByteArray text = v.toString().toLatin1();
    bytes = QByteArray::fromBase64(text);
    // Canonical padded base64 only; Qt's default decoder otherwise ignores junk.
    return (QString::fromLatin1(text) == v.toString()) && (bytes.toBase64() == text);
}

bool DSEModTxSession::parseFrame(const QJsonObject& o, int mode, Frame& f, QString& error)
{
    if (!keys(o, {"format", "frameType", "slot", "sequence", "data"})
        || !o["format"].isString() || !decodeBase64(o["data"], f.data))
    {
        error = "Invalid frame fields or base64";
        return false;
    }

    f.format = o["format"].toString();
    f.type = o["frameType"].toString();

    if (o.contains("sequence") && !integer(o["sequence"], 0, 255))
    {
        error = "Invalid frame sequence";
        return false;
    }

    f.sequence = o["sequence"].toInt();

    if (f.format == "dsrp")
    {
        const QByteArray packet = f.data;

        if ((packet.size() < 8) || (packet.left(4) != "DSRP"))
        {
            error = "Invalid DSRP packet";
            return false;
        }

        const int type = (quint8) packet[4];

        if ((type == 0x20) && (packet.size() == 49))
        {
            f.format = "dstar-header";
            f.data = packet.mid(8);
        }
        else if ((type == 0x21) && (packet.size() == 21) && (((quint8) packet[7] & 0x3f) <= 20))
        {
            f.format = "dstar-voice";
            f.data = packet.mid(9);
            f.sequence = (quint8) packet[7] & 0x3f;
            f.type = ((quint8) packet[7] & 0x40) ? "end" : "voice";
        }
        else
        {
            error = "Unsupported DSRP traffic packet";
            return false;
        }
    }
    else if (f.format == "ysfd")
    {
        if ((f.data.size() != 155) || (f.data.left(4) != "YSFD"))
        {
            error = "Invalid YSFD packet";
            return false;
        }

        f.sequence = (quint8) f.data[34] >> 1;
        f.type = ((quint8) f.data[34] & 1) ? "end" : "frame";
        f.data = f.data.mid(35);
        f.format = "ysf-frame";
    }
    else if (f.format == "dmrd")
    {
        if ((f.data.size() != 55) || (f.data.left(4) != "DMRD"))
        {
            error = "Invalid DMRD packet (55 bytes required)";
            return false;
        }

        const int flags = (quint8) f.data[15];
        f.slot = (flags & 0x80) ? 2 : 1;
        f.sequence = flags & 15;
        f.type = (flags & 0x20) ? QString("data-%1").arg(flags & 15)
            : ((flags & 0x10) ? "voice-sync" : "voice");
        f.data = f.data.mid(20, 33);
        f.format = "dmr-burst";
    }

    if (f.format.startsWith("dstar-") && (mode == DSEModSettings::DSEModModeDStar))
    {
        if (((f.format == "dstar-header") && (f.data.size() == 41))
            || ((f.format == "dstar-voice") && (f.data.size() == 12) && (f.sequence <= 20)
                && (f.type.isEmpty() || (f.type == "voice") || (f.type == "end"))))
        {
            return true;
        }
    }
    else if ((f.format == "angel61-frame") && (mode == AnyAngel61))
    {
        if (f.data.size() >= 3) {
            return true;
        }
    }
    else if ((f.format == "angel61-message") && (mode == AnyAngel61))
    {
        // A whole message of the call (an HF data group's, DVAngel61::packRelayMessage)
        int contentType, flags;
        std::vector<uint8_t> message;

        if (DVAngel61::unpackRelayMessage((const uint8_t*) f.data.constData(), f.data.size(), contentType, flags, message)) {
            return true;
        }
    }
    else if ((f.format == "ysf-frame") && (mode == DSEModSettings::DSEModModeYSF) && (f.data.size() == 120))
    {
        return true;
    }
    else if ((f.format == "dmr-burst") && (mode == DSEModSettings::DSEModModeDMR) && (f.data.size() == 33))
    {
        if (!f.slot)
        {
            if (!integer(o["slot"], 1, 2))
            {
                error = "DMR slot must be 1 or 2";
                return false;
            }

            f.slot = o["slot"].toInt();
        }

        if ((f.type == "voice-sync") || ((f.type == "voice") && (f.sequence <= 5))
            || QRegularExpression("^data-([0-9]|10|11)$").match(f.type).hasMatch())
        {
            return true;
        }
    }

    error = "Unsupported frame format/type/length for the selected protocol";
    return false;
}

bool DSEModTxSession::isOpenRetry(const QJsonObject& o) const
{
    return !m_openReceipt.isEmpty() && (o["sessionId"].toString() == m_id)
        && (QCryptographicHash::hash(QJsonDocument(o).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256) == m_openReceipt);
}

int DSEModTxSession::submit(const QJsonObject& o, const DSEModSettings& settings, QString& error)
{
    auto fail = [&](int code, const QString& why) {
        error = why;
        return code;
    };

    if (!keys(o, {"operation", "sessionId", "input", "protocol", "sequence", "pcm", "frames", "source", "destination", "via"})) {
        return fail(400, "Unknown transmit field");
    }

    const QString op = o["operation"].toString();
    const QString id = o["sessionId"].toString();

    if (!QRegularExpression("^[A-Za-z0-9_.:-]{1,64}$").match(id).hasMatch()) {
        return fail(400, "sessionId must contain 1-64 letters, digits, _, ., : or -");
    }

    if (!QStringList{"send", "start", "append", "finish", "abort"}.contains(op)) {
        return fail(400, "Unknown transmit operation");
    }

    const QByteArray receipt = QCryptographicHash::hash(QJsonDocument(o).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256);
    const bool opening = (op == "start") || (op == "send");

    if (opening && o.contains("sequence")) {
        return fail(400, "sequence is only valid on append");
    }

    if (opening && (id == m_id) && (receipt == m_openReceipt)) {
        return 202;
    }

    if (opening && m_active) {
        return fail(409, QString("Transmitter busy with session %1 (%2)").arg(m_id, m_state));
    }

    if (opening && (id == m_id)) {
        return fail(409, QString("Session %1 was already used").arg(id));
    }

    for (const auto& h : m_history)
    {
        if (opening && (h.toObject()["sessionId"].toString() == id)) {
            return fail(409, "sessionId already used");
        }
    }

    if (!opening && (id != m_id))
    {
        return fail(409, QString("No matching active session %1 (current session: %2, state: %3)")
            .arg(id, m_id.isEmpty() ? QString("none") : m_id, m_state));
    }

    // A gateway may discover that a call ended after DSEMod has already
    // drained or failed it. Aborting that same terminal session is harmless.
    if (!opening && !m_active && (op == "abort")) {
        return 202;
    }

    if (!opening && !m_active && (receipt != m_closeReceipt))
    {
        return fail(409, QString("Session %1 is %2%3").arg(id, m_state,
            m_error.isEmpty() ? QString() : QString(": %1").arg(m_error)));
    }

    if (((op == "finish") || (op == "abort")) && (receipt == m_closeReceipt)) {
        return 202;
    }

    if ((op == "finish") || (op == "abort"))
    {
        if (!keys(o, {"operation", "sessionId"})) {
            return fail(400, "finish/abort accept only operation and sessionId");
        }

        m_closed = true;
        m_abort = (op == "abort");
        m_closeReceipt = receipt;
        return 202;
    }

    int sequence = 0;

    if (!opening)
    {
        // (any int: at one 20 ms frame a chunk, 1,000,000 ended a session at 5.5 hours, and the frame
        // session's limit is 24)
        if (!integer(o["sequence"], 0, std::numeric_limits<int>::max() - 1)) {
            return fail(400, "append requires an integer sequence");
        }

        sequence = o["sequence"].toInt();

        if (m_receipts.contains(sequence)) {
            return (m_receipts[sequence] == receipt) ? 202 : fail(409, "Chunk sequence reused with different content");
        }

        if (m_closed || (sequence != m_next)) {
            return fail(409, QString("Input closed or out of order; expected sequence %1").arg(m_next));
        }

        if (!keys(o, {"operation", "sessionId", "sequence", "pcm", "frames"})) {
            return fail(400, "Session format cannot change on append");
        }
    }

    const QString input = opening ? o["input"].toString() : m_input;

    if (opening && (input == "pcm") && o.contains("protocol")) {
        return fail(400, "PCM uses the currently selected protocol");
    }

    if ((input != "pcm") && (input != "frames") && (input != "angel61")) {
        return fail(400, "input must be pcm, frames or angel61");
    }

    const int mode = opening ? (int) settings.m_mode : m_mode;

    // (Angel61's frames come through the "angel61" input: the modulator makes its own frames of them)
    if (opening && (input == "frames") && (mode != DSEModSettings::DSEModModeDStar)
        && (mode != DSEModSettings::DSEModModeDMR) && (mode != DSEModSettings::DSEModModeYSF))
    {
        return fail(422, "External frames are unsupported for this mode");
    }

    if (opening && (input == "frames")
        && (!integer(o["protocol"], 0, DSEModSettings::DSEModModeAngel61HFWB) || (o["protocol"].toInt() != mode)))
    {
        return fail(422, "Frame protocol must match the selected DSEMod mode");
    }

    int sourceProtocol = opening ? mode : m_sourceProtocol;
    QString source = opening ? QString() : m_source;
    QString destination = opening ? QString() : m_destination;
    QString via = opening ? QString() : m_via;

    // A network Angel61 call (the talkgroup gateway): frames of any profile. The call's profile is
    // its frames'; this modulator repacks them into its own (ANGEL61.md 3.1.1)
    if (input == "angel61")
    {
        if (opening)
        {
            if (!DSEModSettings::isAngel61(DSEModSettings::DSEModMode(mode))) {
                return fail(422, "Angel61 network input requires an Angel61 mode");
            }

            if (o.contains("protocol")) {
                return fail(400, "Angel61 network input takes its profile from its frames");
            }

            if (!o["source"].isString() || !o["destination"].isString()) {
                return fail(400, "Angel61 network input start requires source and destination");
            }

            source = o["source"].toString().trimmed().toUpper();
            destination = o["destination"].toString().trimmed().toUpper();
            // The routing of the preamble this modulator builds: a callsign, CQ or a talkgroup
            // (ANGEL61.md 2.6). Which destinations a network carries is the gateway's rule
            const QRegularExpression identity("^[A-Z0-9/.-]{1,20}$");

            if (!identity.match(source).hasMatch()
                || (DVAngel61::encodeCallsign(destination.toStdString()) == DVAngel61::CallsignNone))
            {
                return fail(422, "Angel61 network input requires a valid source and destination");
            }

            destination = QString::fromStdString(DVAngel61::decodeCallsign(DVAngel61::encodeCallsign(destination.toStdString())));

            // A gateway relaying a private call names itself as the path (ANGEL61.md 2.6)
            if (o.contains("via"))
            {
                via = o["via"].toString().trimmed().toUpper();
                const uint64_t code = DVAngel61::encodeCallsign(via.toStdString());

                if (!identity.match(via).hasMatch() || (code == DVAngel61::CallsignNone) || DVAngel61::isGroup(code)) {
                    return fail(422, "Angel61 network input via must be a callsign");
                }
            }

            sourceProtocol = 0;
        }
    }
    else if (opening && (o.contains("source") || o.contains("destination") || o.contains("via")))
    {
        return fail(400, "source and destination are only valid for the Angel61 network input");
    }

    QByteArray pcm;
    std::deque<Frame> frames;
    auto validator = (opening || !m_validator) ? std::make_shared<DSEModPacketBuilder>()
        : std::make_shared<DSEModPacketBuilder>(*m_validator);

    if (opening) {
        validator->reset(settings);
    }

    int rate = opening ? 0 : m_rate;

    if (input == "pcm")
    {
        if (o.contains("frames") || !o["pcm"].isObject()) {
            return fail(400, "PCM input requires a pcm object only");
        }

        const auto p = o["pcm"].toObject();

        if (!keys(p, {"encoding", "sampleRate", "channels", "data"})) {
            return fail(400, "Unknown PCM field");
        }

        if (opening)
        {
            if ((p["encoding"] != "s16le") || !integer(p["channels"], 1, 1)
                || !integer(p["sampleRate"], 8000, 48000)
                || !QList<int>{8000, 16000, 48000}.contains(p["sampleRate"].toInt()))
            {
                return fail(422, "PCM requires s16le, mono, and 8000, 16000 or 48000 Hz");
            }

            rate = p["sampleRate"].toInt();
        }
        else if (!keys(p, {"data"}))
        {
            return fail(400, "append PCM contains data only");
        }

        if (p["data"].isString() && (p["data"].toString().size() > (rate * 2 + 2) / 3 * 4)) {
            return fail(413, "PCM chunk exceeds one second");
        }

        if (!decodeBase64(p["data"], pcm) || (pcm.size() & 1)) {
            return fail(400, "Invalid PCM base64 or incomplete sample");
        }

        if (pcm.size() > rate * 2) {
            return fail(413, "PCM chunk exceeds one second");
        }

        if (!opening && (m_pcm.size() + pcm.size() > rate * 10)) {
            return fail(429, "PCM queue full; retry later");
        }

        if ((op != "start") && pcm.isEmpty()) {
            return fail(400, "Empty PCM payload");
        }
    }
    else
    {
        if (o.contains("pcm") || !o["frames"].isArray()) {
            return fail(400, "Frame input requires a frames array only");
        }

        const auto array = o["frames"].toArray();

        if (array.size() > 50) {
            return fail(413, "At most 50 frames per request");
        }

        if (!opening && ((int) m_frames.size() + array.size() > MaxFrames)) {
            return fail(429, "Frame queue full; retry later");
        }

        for (const auto& value : array)
        {
            Frame frame;
            const int frameMode = (input == "angel61") ? AnyAngel61 : mode;

            if (!value.isObject() || !parseFrame(value.toObject(), frameMode, frame, error)) {
                return 422;
            }

            if ((frame.format == "dmr-burst") && (frame.slot != settings.m_dmrSlot)) {
                return fail(422, "DMR frame slot must match the selected transmit slot");
            }

            std::vector<int8_t> scratch;

            if (input == "angel61")
            {
                if (frame.format == "angel61-message")
                {
                    frames.push_back(frame);    // no profile of its own
                    continue;
                }

                // Any profile's records, one call one profile
                const int frameProtocol = angel61Mode(frame, error);

                if (!frameProtocol) {
                    return 422;
                }

                if (sourceProtocol && (frameProtocol != sourceProtocol)) {
                    return fail(422, "Angel61 call changed profile");
                }

                sourceProtocol = frameProtocol;
                frames.push_back(frame);
                continue;
            }

            if (!DSEModPacketBuilder::validate(frame, error)) {
                return 422;
            }

            if (!validator->append(frame, scratch, error)) {
                return 422;
            }

            frames.push_back(frame);
        }

        if ((op != "start") && frames.empty()) {
            return fail(400, "Empty frame batch");
        }
    }

    if (opening)
    {
        reset();
        m_id = id;
        m_input = input;
        m_mode = mode;
        m_sourceProtocol = sourceProtocol;
        m_source = source;
        m_destination = destination;
        m_via = via;
        m_rate = rate;
        m_maxDurationMs = ((input == "frames") || (input == "angel61"))
            ? std::max(MinFrameMaxDurationMs, std::min(MaxFrameMaxDurationMs, settings.m_frameSessionMaxDurationMs))
            : PCMMaxDurationMs;
        m_state = "buffering";
        m_active = true;
        m_openReceipt = receipt;
        m_lifetime.start();
    }

    m_pcm.append(pcm);
    m_sourceProtocol = sourceProtocol;
    m_validator = validator;
    m_frames.insert(m_frames.end(), frames.begin(), frames.end());
    m_accepted += (input == "pcm") ? pcm.size() / 2 : (int) frames.size();
    m_arrival.start();

    if (!opening)
    {
        m_receipts[sequence] = receipt;
        ++m_next;

        while (m_receipts.size() > 256) {
            m_receipts.erase(m_receipts.begin());
        }
    }

    m_closed = (op == "send");
    return 202;
}

int DSEModTxSession::angel61Mode(const Frame& frame, QString& error)
{
    DVAngel61::Header header;
    std::vector<uint8_t> block;

    if (!DVAngel61::unpackRecord((const uint8_t*) frame.data.constData(), frame.data.size(), header, block)
        || (frame.sequence != header.m_counter) || ((frame.type != "frame") && (frame.type != "end"))
        || ((frame.type == "end") != header.m_lastFrame))
    {
        error = "Invalid Angel61 frame: type, length, counter, flags or CRC";
        return 0;
    }

    switch (DVAngel61::profileOf(header.m_type))
    {
    case DVAngel61::ProfileWB:
        return DSEModSettings::DSEModModeAngel61WB;
    case DVAngel61::ProfileHFNB:
        return DSEModSettings::DSEModModeAngel61HFNB;
    case DVAngel61::ProfileHFWB:
        return DSEModSettings::DSEModModeAngel61HFWB;
    default:
        return DSEModSettings::DSEModModeAngel61;
    }
}

void DSEModTxSession::reset()
{
    m_pcm.clear();
    m_frames.clear();
    m_receipts.clear();
    m_closeReceipt.clear();
    m_openReceipt.clear();
    m_accepted = 0;
    m_consumed = 0;
    m_next = 0;
    m_active = false;
    m_closed = false;
    m_abort = false;
    m_error.clear();
    m_sourceProtocol = 0;
    m_source.clear();
    m_destination.clear();
    m_via.clear();
    m_maxDurationMs = PCMMaxDurationMs;
}

QByteArray DSEModTxSession::takePCM(int bytes)
{
    QByteArray data = m_pcm.left(bytes);
    m_pcm.remove(0, data.size());
    return data;
}

DSEModTxSession::Frame DSEModTxSession::takeFrame()
{
    Frame frame = m_frames.front();
    m_frames.pop_front();
    return frame;
}

bool DSEModTxSession::timedOut() const
{
    return !m_closed && m_arrival.isValid() && (m_arrival.elapsed() >= IdleTimeoutMs);
}

void DSEModTxSession::complete(const QString& state, const QString& error)
{
    m_state = state;
    m_error = error;
    m_active = false;
    m_pcm.clear();
    m_frames.clear();
    auto result = report();
    result.remove("history");
    m_history.append(result);

    while (m_history.size() > 16) {
        m_history.removeFirst();
    }
}

QJsonObject DSEModTxSession::report() const
{
    return {
        {"sessionId", m_id},
        {"input", m_input},
        {"state", m_state},
        {"protocol", m_mode},
        {"sourceProtocol", m_sourceProtocol},
        {"source", m_source},
        {"destination", m_destination},
        {"via", m_via},
        {"nextSequence", m_next},
        {"accepted", (double) m_accepted},
        {"consumed", (double) m_consumed},
        {"maxDurationMs", m_maxDurationMs},
        {"queuedSamples", m_pcm.size() / 2},
        {"queuedFrames", (int) m_frames.size()},
        {"availableSamples", m_rate ? (m_rate * 10 - m_pcm.size()) / 2 : 0},
        {"availableFrames", MaxFrames - (int) m_frames.size()},
        {"lastError", m_error},
        {"history", m_history}
    };
}

QJsonObject DSEModTxSession::capabilities(const DSEModSettings& settings)
{
    const int frameMaxDurationMs = std::max(MinFrameMaxDurationMs,
        std::min(MaxFrameMaxDurationMs, settings.m_frameSessionMaxDurationMs));

    return {
        {"pcmEncoding", "s16le"},
        {"pcmChannels", 1},
        {"pcmSampleRates", QJsonArray{8000, 16000, 48000}},
        {"maxPCMChunkMs", 1000},
        {"maxPCMQueueMs", 5000},
        {"maxFrameBatch", 50},
        {"maxQueuedFrames", MaxFrames},
        {"idleTimeoutMs", IdleTimeoutMs},
        {"pcmMaxDurationMs", PCMMaxDurationMs},
        {"frameMaxDurationMs", frameMaxDurationMs},
        {"minFrameMaxDurationMs", MinFrameMaxDurationMs},
        {"maxFrameMaxDurationMs", MaxFrameMaxDurationMs},
        {"frameFormats", QJsonArray{"dstar-header", "dstar-voice", "dmr-burst", "ysf-frame", "angel61-frame", "dsrp", "dmrd", "ysfd"}},
        {"angel61NetworkInput", true},
        {"messageServices", QJsonArray{"dstar-text", "angel61-text", "angel61-data"}}
    };
}
