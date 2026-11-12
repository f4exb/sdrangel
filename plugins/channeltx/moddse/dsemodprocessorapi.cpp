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

#include <numeric>
#include <algorithm>
#include <cmath>

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QDateTime>
#include <QRegularExpression>
#include <QMetaObject>
#include <QtEndian>

#include "dsp/dv/dvangel61.h"
#include "dsp/dv/dvbits.h"

#include "dsemodprocessor.h"
#include "dsemodprotocol.h"
#include "dsemodangel61.h"
#include "dsemodsource.h"

int DSEModProcessor::submitAction(const QJsonObject& action, QString& error)
{
    if ((action.size() != 1) || (!action.contains("transmit") && !action.contains("message")))
    {
        error = "Exactly one transmit or message action is required";
        return 400;
    }

    if (action.contains("message"))
    {
        if (!action["message"].isObject())
        {
            error = "message must be an object";
            return 400;
        }

        return submitMessage(action["message"].toObject(), error);
    }

    if (!action["transmit"].isObject())
    {
        error = "transmit must be an object";
        return 400;
    }

    const auto request = action["transmit"].toObject();
    const bool opening = (request["operation"] == "start") || (request["operation"] == "send");

    // (a retry of the request that opened the session is accepted again whatever came after it: its
    // transmission may be draining, or the encoder closed, by the time the answer is retried)
    if (opening && m_api.isOpenRetry(request)) {
        return 202;
    }

    if (opening && !m_api.active() && (m_transmitting || m_settings.m_transmit || m_fifo.getFill()))
    {
        error = "Local transmission is busy or draining";
        return 409;
    }

    if (opening && (request["input"] == "pcm") && (!m_vocoder || !m_vocoder->canOpen()))
    {
        error = "Selected speech encoder is unavailable";
        return 503;
    }

    bool wasActive = m_api.active();
    const int status = m_api.submit(request, m_settings, error);

    if (status != 202) {
        return status;
    }

    if (!wasActive && m_api.active())
    {
        m_apiActive = true;
        m_apiMessage = false;
        m_apiMessageRequest = {};
        m_apiStarted = false;
        m_apiInputEnded = false;
        m_apiDraining = false;
        m_apiOutputDrained = false;
        m_apiPending.clear();
        m_apiResults.clear();
        m_apiPCM.clear();
        m_apiTranscodeDecodedFrames = 0;
        m_apiAngel61 = false;
        m_apiTranscode = false;
        m_apiCallType = -1;
        m_apiInvalidRecords = 0;
        m_apiMonitorUnavailable = false;
        m_apiEncoderWait.invalidate();
        m_packets.reset(m_settings);
    }

    serviceAPI();
    return 202;
}

int DSEModProcessor::submitMessage(const QJsonObject& message, QString& error)
{
    auto fail = [&](int code, const QString& why) {
        error = why;
        return code;
    };

    if (!DSEModTxSession::keys(message, {"requestId", "type", "delivery", "sessionId", "text", "data", "service"})) {
        return fail(400, "Unknown message field");
    }

    const QString id = message["requestId"].toString();

    if (!QRegularExpression("^[A-Za-z0-9_.:-]{1,64}$").match(id).hasMatch()) {
        return fail(400, "Invalid requestId");
    }

    const auto digest = QCryptographicHash::hash(QJsonDocument(message).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256);

    if (m_apiMessageReceipts.contains(id)) {
        return (m_apiMessageReceipts[id] == digest) ? 202 : fail(409, "requestId reused with different content");
    }

    const bool text = message["type"] == "text";

    if (!text && (message["type"] != "data")) {
        return fail(400, "message type must be text or data");
    }

    QByteArray data;

    if (text)
    {
        if (!message["text"].isString() || message.contains("data")) {
            return fail(400, "Text message requires text only");
        }

        data = message["text"].toString().toUtf8();
    }
    else if (message.contains("text") || !DSEModTxSession::decodeBase64(message["data"], data))
    {
        return fail(400, "Data message requires canonical base64 data only");
    }

    if (data.isEmpty()) {
        return fail(400, "Message cannot be empty");
    }

    const bool dstar = m_settings.m_mode == DSEModSettings::DSEModModeDStar;
    const bool angel = DSEModSettings::isAngel61(m_settings.m_mode);
    const QString service = dstar ? "dstar-text" : (text ? "angel61-text" : "angel61-data");

    if ((!dstar && !angel)
        || (dstar && !text)
        || (message.contains("service") && (message["service"] != service))
        || (!text && !message.contains("service")))
    {
        return fail(422, "Unsupported message service for selected protocol");
    }

    const bool inBand = message["delivery"] == "inBand";

    if (!inBand && (message["delivery"] != "standalone")) {
        return fail(400, "delivery must be standalone or inBand");
    }

    if (inBand || dstar)
    {
        for (auto c : data)
        {
            if (((quint8) c < 32) || ((quint8) c > 126)) {
                return fail(422, "This message service requires printable ASCII");
            }
        }
    }

    const int maxBytes = dstar ? 20 : (inBand ? 180 : 5098);

    if (data.size() > maxBytes) {
        return fail(413, QString("Message exceeds %1 bytes").arg(maxBytes));
    }

    if (inBand)
    {
        if (!text) {
            return fail(422, "Binary messages require standalone delivery");
        }

        if (!m_api.active() || !m_apiStarted || m_api.closed() || (m_api.input() != "pcm")
            || (message["sessionId"] != m_api.id()))
        {
            return fail(409, "inBand requires the matching open PCM session");
        }

        if (!m_protocol->queueMessage(data.toStdString())) {
            return fail(429, "Protocol message queue full");
        }
    }
    else
    {
        if (message.contains("sessionId")) {
            return fail(400, "Standalone message uses requestId as its sessionId");
        }

        if (m_api.active() || m_transmitting || m_settings.m_transmit || m_fifo.getFill()) {
            return fail(409, "Transmitter busy");
        }

        QJsonObject request{{"operation", "start"}, {"sessionId", id}, {"input", "pcm"},
            {"pcm", QJsonObject{{"encoding", "s16le"}, {"sampleRate", 8000}, {"channels", 1}, {"data", ""}}}};
        int status = m_api.submit(request, m_settings, error);

        if (status != 202) {
            return status;
        }

        m_api.submit({{"operation", "finish"}, {"sessionId", id}}, m_settings, error);
        m_apiActive = true;
        m_apiStarted = false;
        m_apiInputEnded = false;
        m_apiDraining = false;
        m_apiOutputDrained = false;
        m_apiMessage = true;
        m_apiMessageRequest = message;
        m_apiPending.clear();
        m_apiResults.clear();
        m_apiPCM.clear();
        m_apiMessageFrames = dstar ? 42 : 0;
    }

    m_apiMessageReceipts[id] = digest;
    m_apiMessageResults.append(QJsonObject{{"requestId", id}, {"state", inBand ? "queued" : "accepted"}});

    while (m_apiMessageResults.size() > 32)
    {
        m_apiMessageReceipts.remove(m_apiMessageResults.first().toObject()["requestId"].toString());
        m_apiMessageResults.removeFirst();
    }

    serviceAPI();
    return 202;
}

QJsonObject DSEModProcessor::getAPIReport() const
{
    auto report = m_api.report();
    report["messages"] = m_apiMessageResults;
    report["pendingEncoderFrames"] = (int) m_apiPending.size();
    report["encodedQueueFrames"] = (int) m_encodedFrames.size();
    report["transcodeDecodedFrames"] = m_apiTranscodeDecodedFrames;
    report["relayInvalidFrames"] = m_apiInvalidRecords;

    if (const DSEModAngel61 *angel61 = dynamic_cast<const DSEModAngel61*>(m_protocol)) {
        report["relayHoldFrames"] = angel61->getRelayHoldFrames();   // idle frames for want of the call's packets
    }

    return report;
}

void DSEModProcessor::cancelAPI(const QString& reason)
{
    if (m_api.active()) {
        finishAPI("failed", reason);
    }
}

void DSEModProcessor::finishAPI(const QString& state, const QString& reason)
{
    m_protocol->abort();
    m_fifo.clear();
    m_encodedFrames.clear();
    m_apiPending.clear();
    m_apiResults.clear();
    m_apiPCM.clear();
    m_apiOpusDecoder.reset();
    m_apiTranscode = false;
    m_apiAngel61 = false;

    if (m_vocoder) {
        m_vocoder->close();
    }

    m_transmitting = false;
    m_stopping = false;
    m_api.complete(state, reason);

    for (int i = 0; i < m_apiMessageResults.size(); ++i)
    {
        auto item = m_apiMessageResults[i].toObject();

        if (item["requestId"] == m_api.id())
        {
            item["state"] = state;
            item["error"] = reason;
            m_apiMessageResults[i] = item;
        }
    }

    m_apiActive = false;
    m_apiDraining = false;
    m_apiStarted = false;

    if (DSEModAngel61 *angel61 = dynamic_cast<DSEModAngel61*>(m_protocol))
    {
        angel61->endSession();      // (not a new protocol: this station's tunnel and file transfer stay)
        m_encoderSampleRate = 0;    // the source goes back to the mode's own input rate
    }
    else
    {
        createProtocol();
    }

    updateVocoderReady();
}

void DSEModProcessor::startAPI()
{
    m_apiStarted = true;
    m_transmitting = true;
    m_stopping = false; // (a local transmission's release, left set, ended the session at its first empty queue)
    m_apiStarved.invalidate();
    m_encodedFrames.clear();
    m_fifo.clear();
    m_apiPCM.clear();
    m_apiTaps.clear();
    m_apiHistory.clear();
    m_apiFilterPos = 0;
    m_apiDecimationCount = 0;
    m_apiEncoderWait.invalidate();
    m_apiMbeMonitor.reset();
    m_encodedFrameCount = 0;
    m_txFrameCount = 0;
    m_underrunCount = 0;
    m_overrunCount = 0;
    m_trimCount = 0;

    if (m_shaper) {
        m_shaper->reset();
    }

    m_linearShaper.reset();
    DSEModSettings settings = m_settings;
    settings.m_modAFInput = DSEModSettings::DSEModInputSilence;
    settings.m_angel61Message.clear();
    // A network Angel61 call: its frames say its profile and rate. It goes out in this modulator's
    // profile, at the call's rate or lower, never higher: a relay only truncates (ANGEL61.md 3.1.1)
    DSEModAngel61 *angel61 = dynamic_cast<DSEModAngel61*>(m_protocol);

    if ((m_api.input() == "angel61") && angel61)
    {
        m_apiAngel61 = true;
        m_apiTranscode = false;
        m_apiCallType = firstCallVoiceType();
        const int cap = (m_apiCallType >= 0) ? DSEModAngel61::relayTierCap(angel61->getProfile(), m_apiCallType) : 3;

        if (cap < 0)
        {
            finishAPI("failed", QString("Angel61 relay: the call's %1 (%2 codebooks) is below %3's lowest tier, and a relay never raises the rate")
                .arg(DVAngel61::typeName(m_apiCallType)).arg(DVAngel61::codebooksOf(m_apiCallType))
                .arg(DVAngel61::profileName(angel61->getProfile())));
            return;
        }

        angel61->setTierCap(cap);
        angel61->setRelay();
        settings.m_angel61Source = m_api.source();
        settings.m_angel61Destination = m_api.destination();
        settings.m_angel61Path = m_api.via();
    }

    bool transcode = apiTranscoding();

    if (m_apiMessage)
    {
        if (settings.m_mode == DSEModSettings::DSEModModeDStar)
        {
            settings.m_dstarText = m_apiMessageRequest["text"].toString();
        }
        else
        {
            QByteArray data = (m_apiMessageRequest["type"] == "text") ? m_apiMessageRequest["text"].toString().toUtf8()
                : QByteArray::fromBase64(m_apiMessageRequest["data"].toString().toLatin1());

            if (m_angel61Data) {
                m_angel61Data->setDataMessage(std::vector<uint8_t>(data.begin(), data.end()), (m_apiMessageRequest["type"] == "text") ? 0 : 1);
            }
        }
    }

    const bool linkValid = (m_api.input() != "angel61") && (m_linkTimestampMs > 0)
        && (QDateTime::currentMSecsSinceEpoch() - m_linkTimestampMs < 60000);
    m_protocol->setLinkState(linkValid ? m_linkRequestedTier : 0, linkValid ? m_linkSendTier : 0);
    m_protocol->start(settings);

    // This station's position, as a local transmission has it (not a relayed call's: that is the
    // caller's, sent again from its frames)
    if (!m_apiAngel61) {
        updatePosition();
    }

    // O1 into a V1 tier (WB's Opus relayed at a lower tier, or to another profile): the only
    // transcoding (3.1.2), the packets decoded and this modulator's V1 encoder
    if (m_apiAngel61 && (m_apiCallType >= 0) && DVAngel61::isOpus(m_apiCallType) && !angel61->isOpusTier())
    {
        if (!m_vocoder || !m_vocoder->canOpen())
        {
            finishAPI("failed", "Angel61 relay: an Opus call to a V1 tier needs the V1 encoder");
            return;
        }

        if (!m_apiOpusDecoder.open(DVOpus::FrameSamples, DVOpus::PacketBytes))
        {
            finishAPI("failed", "Angel61 relay: an Opus call to a V1 tier needs the Opus decoder");
            return;
        }

        m_apiTranscode = true;
        transcode = true;
    }

    m_linearShaper.setSamplesPerSymbol(m_protocol->getLinearSamplesPerSymbol());
    m_linearShaper.setLevel(m_protocol->getLinearLevel());

    if (((m_api.input() == "pcm") || transcode) && !m_apiMessage)
    {
        // Angel61 WB's tier picks V1 or Opus: the encoder follows this transmission's rate
        if (m_vocoder && !vocoderMatchesRate()) {
            createVocoder();
        }

        if (!m_vocoder || !m_vocoder->open(m_protocol->getVocoderRate()) || !m_vocoder->isReady())
        {
            finishAPI("failed", "Speech encoder could not be opened");
            return;
        }

        if (!transcode) {
            configureAPIResampler(m_api.rate());
        } else {
            configureAPIResampler(48000);     // Opus's 48 kHz to V1's 24
        }
    }

    m_api.setState("transmitting");
    updateVocoderReady();
}

void DSEModProcessor::configureAPIResampler(int sourceRate)
{
    const int target = DSEModVocoder::getPcmSampleRate(m_protocol->getVocoderRate());

    if ((sourceRate <= 0) || (target <= 0)) {
        return;
    }

    // The ratio in lowest terms: 16 kHz to V1's 24 kHz is up 3, down 2 (whole quotients made it up 1,
    // down 1, and the speech played 1.5 times fast). The API's rates (8, 16, 48 kHz) and the vocoders'
    // (8, 24, 48) keep both at most 6
    const int common = std::gcd(target, sourceRate);
    const int up = target / common;
    const int down = sourceRate / common;

    if (!m_apiTaps.empty() && (m_apiUp == up) && (m_apiDown == down)) {
        return;
    }

    m_apiUp = up;
    m_apiDown = down;
    m_apiFilterPos = 0;
    m_apiDecimationCount = 0;
    const int taps = (sourceRate == target) ? 1 : 63;
    m_apiTaps.assign(taps, 0);
    m_apiHistory.assign(taps, 0);
    const double pi = 3.14159265358979323846;
    const double cutoff = 0.45 / std::max(m_apiUp, m_apiDown);
    double sum = 0;

    for (int i = 0; i < taps; ++i)
    {
        const int n = i - taps / 2;
        const double h = n ? std::sin(2 * pi * cutoff * n) / (pi * n) : 2 * cutoff;
        m_apiTaps[i] = (taps == 1) ? 1 : h * (0.54 - 0.46 * std::cos(2 * pi * i / (taps - 1)));
        sum += m_apiTaps[i];
    }

    for (auto& h : m_apiTaps) {
        h *= m_apiUp / sum;
    }
}

void DSEModProcessor::resampleAPI(const QByteArray& pcm)
{
    const int taps = (int) m_apiTaps.size();

    for (int i = 0; i < pcm.size(); i += 2)
    {
        const auto sample = qFromLittleEndian<qint16>((const uchar*) pcm.constData() + i);

        for (int u = 0; u < m_apiUp; ++u)
        {
            m_apiHistory[m_apiFilterPos] = (u == 0) ? sample * m_settings.m_volumeFactor : 0;

            if (++m_apiDecimationCount >= m_apiDown)
            {
                m_apiDecimationCount = 0;
                double out = 0;

                for (int k = 0; k < taps; ++k) {
                    out += m_apiTaps[k] * m_apiHistory[(m_apiFilterPos - k + taps) % taps];
                }

                m_apiPCM.push_back((int16_t) std::max(-32768.0, std::min(32767.0, std::round(out))));
            }

            m_apiFilterPos = (m_apiFilterPos + 1) % taps;
        }
    }
}

int DSEModProcessor::firstCallVoiceType() const
{
    for (const DSEModTxSession::Frame& frame : m_api.frames())
    {
        if (frame.format == "angel61-message") {
            continue;
        }

        const int type = frame.data.isEmpty() ? -1 : quint8(frame.data[0]);

        if ((type >= 0) && DVAngel61::isKnownType(type) && DVAngel61::isVoice(type)) {
            return type;
        }
    }

    return -1;
}

void DSEModProcessor::relayAngel61API()
{
    // The call's voice to the protocol, packet by packet, as its encoder's would come: a V1 packet
    // with the codebooks it carries (the protocol truncates, or steps a frame down to them), or an
    // Opus packet. Its idle and data frames carry no voice. Its slots and fields go to the protocol
    // (relayFrame), which sends their text, messages, DTMF and position again in its own
    DSEModAngel61 *angel61 = dynamic_cast<DSEModAngel61*>(m_protocol);
    const int limit = 3 * DVAngel61::packetsPerFrame(angel61 ? angel61->getProfile() : DVAngel61::ProfileHFNB);

    while (m_api.frameCount() && ((int) m_encodedFrames.size() < limit))
    {
        const DSEModTxSession::Frame frame = m_api.takeFrame();
        m_api.consumed(1);
        DVAngel61::Header header;
        std::vector<uint8_t> block;

        if (frame.format == "angel61-message")
        {
            // A whole message of the call (an HF data group's): sent again in this station's fields
            int contentType, flags;
            std::vector<uint8_t> message;

            if (angel61 && DVAngel61::unpackRelayMessage((const uint8_t*) frame.data.constData(), frame.data.size(), contentType, flags, message)) {
                angel61->relayMessage(contentType, flags, message);
            }

            continue;
        }

        if (!DVAngel61::unpackRecord((const uint8_t*) frame.data.constData(), frame.data.size(), header, block))
        {
            m_apiInvalidRecords++;
            continue;
        }

        const int type = header.m_type;

        // The call's DTMF (3.8), text, messages and position, from any frame: sent again in this
        // station's own slots and fields
        if (angel61) {
            angel61->relayFrame(header, block);
        }

        if (!DVAngel61::isVoice(type)) {
            continue;
        }

        if (DVAngel61::isOpus(type))
        {
            std::array<uint8_t, DSEModVocoder::m_maxFrameBytes> packet{};
            DVBits::bitsToBytesMSB(block.data(), DVAngel61::OpusBits, packet.data());
            m_encodedFrames.push_back(packet);
            m_encodedFrameCount++;
            continue;
        }

        const int codebooks = DVAngel61::codebooksOf(type);
        uint8_t bits[DVAngel61::CodebookBits * 16];

        for (int p = 0; p < DVAngel61::packetsPerFrame(DVAngel61::profileOf(type)); p++)
        {
            std::array<uint8_t, DSEModVocoder::m_maxFrameBytes> packet{};
            DVAngel61::getVoicePacket(block.data(), type, p, bits);
            DVBits::bitsToBytesMSB(bits, DVAngel61::CodebookBits * codebooks, packet.data());
            packet[DSEModVocoder::m_vocosCodebooksByte] = (uint8_t) ((codebooks < 16) ? codebooks : 0);
            m_encodedFrames.push_back(packet);
            m_encodedFrameCount++;
        }
    }

    if (m_api.closed() && !m_api.frameCount()) {
        m_apiInputEnded = true;
    }
}

void DSEModProcessor::decodeAngel61APIFrame(const DSEModTxSession::Frame& frame)
{
    // O1 into V1: an Opus packet decoded (80 ms at 48 kHz), any other frame 80 ms of silence, to the
    // encoder's 24 kHz. A record refused is counted and its time kept as silence
    // The call's text, messages, DTMF and position go on as relayAngel61API sends them (relayFrame,
    // relayMessage): this path dropped them, and took a message entry for a refused record and
    // 80 ms of silence
    DSEModAngel61 *angel61 = dynamic_cast<DSEModAngel61*>(m_protocol);

    if (frame.format == "angel61-message")
    {
        int contentType, flags;
        std::vector<uint8_t> message;

        if (angel61 && DVAngel61::unpackRelayMessage((const uint8_t*) frame.data.constData(), frame.data.size(), contentType, flags, message)) {
            angel61->relayMessage(contentType, flags, message);
        }

        return;
    }

    DVAngel61::Header header;
    std::vector<uint8_t> block;
    std::vector<int16_t> pcm(DVOpus::FrameSamples, 0);

    if (DVAngel61::unpackRecord((const uint8_t*) frame.data.constData(), frame.data.size(), header, block))
    {
        if (angel61) {
            angel61->relayFrame(header, block);
        }

        if (DVAngel61::isOpus(header.m_type))
        {
            uint8_t packet[DVOpus::PacketBytes];
            DVBits::bitsToBytesMSB(block.data(), DVAngel61::OpusBits, packet);

            if (!m_apiOpusDecoder.decode(packet, pcm.data())) {
                std::fill(pcm.begin(), pcm.end(), 0);
            }

            m_apiTranscodeDecodedFrames++;
        }
    }
    else
    {
        m_apiInvalidRecords++;
    }

    QByteArray bytes((int) pcm.size() * 2, '\0');

    for (size_t i = 0; i < pcm.size(); i++) {
        qToLittleEndian<qint16>(pcm[i], (uchar*) bytes.data() + 2 * i);
    }

    resampleAPI(bytes);
}

void DSEModProcessor::queueGatewayFeedback(const QByteArray& pcm, int sampleRate)
{
    if (!m_feedbackSource || pcm.isEmpty()) {
        return;
    }

    DSEModSource *source = m_feedbackSource;
    QMetaObject::invokeMethod(source, [source, pcm, sampleRate] {
        source->playGatewayFeedback(pcm, sampleRate);
    }, Qt::QueuedConnection);
}

void DSEModProcessor::encodeAPI()
{
    // One encoded frame's audio: 20 ms, or four 20 ms frames for an Opus packet (delivered with the fourth)
    const int frameSize = DSEModVocoder::getPcmFrameSize(m_protocol->getVocoderRate());
    const int framesPerPacket = std::max(1, DSEModVocoder::getFrameMs(m_protocol->getVocoderRate()) / 20);
    const int n = frameSize * framesPerPacket;

    while ((int) m_encodedFrames.size() + (int) m_apiPending.size() < 24)
    {
        if (apiTranscoding())
        {
            while ((m_apiPCM.size() < (size_t) n) && m_api.frameCount())
            {
                decodeAngel61APIFrame(m_api.takeFrame());
                m_api.consumed(1);
            }
        }
        else
        {
            // (20 ms at a time until a packet's worth: Opus wants 80 ms, and one 20 ms take a pass
            // left it waiting on the service timer for the rest)
            while ((m_apiPCM.size() < (size_t) n) && m_api.pcmBytes())
            {
                auto pcm = m_api.takePCM(std::min(m_api.pcmBytes(), m_api.rate() / 50 * 2));
                resampleAPI(pcm);
                m_api.consumed(pcm.size() / 2);
            }
        }

        if (m_apiPCM.size() < (size_t) n)
        {
            const bool queuedInput = apiTranscoding() ? (m_api.frameCount() > 0) : (m_api.pcmBytes() > 0);

            if (!m_api.closed() || queuedInput) {
                break;
            }

            if (!m_apiInputEnded)
            {
                // Flush filter delay once, then pad the last codec frame.
                if (m_apiTaps.size() > 1) {
                    resampleAPI(QByteArray((int) m_apiTaps.size() * 2, '\0'));
                }

                m_apiInputEnded = true;
            }

            if (m_apiPCM.empty()) {
                break;
            }

            if (m_apiPCM.size() < (size_t) n) {
                m_apiPCM.resize(n, 0);
            }
        }

        const uint32_t sequence = m_apiSequence++;

        if (m_apiPending.empty()) {
            m_apiEncoderWait.start();
        }

        m_apiPending.push_back(sequence);

        for (int f = 0; f < framesPerPacket; f++) {
            m_vocoder->encode(sequence, m_apiPCM.data() + f * frameSize);
        }

        m_apiPCM.erase(m_apiPCM.begin(), m_apiPCM.begin() + n);
    }

    while (!m_apiPending.empty() && m_apiResults.contains(m_apiPending.front()))
    {
        const auto sequence = m_apiPending.front();
        m_apiPending.pop_front();

        for (const auto& packet : m_apiResults.take(sequence))
        {
            m_encodedFrames.push_back(packet);
            ++m_encodedFrameCount;
        }

        m_apiEncoderWait.start();
    }
}

void DSEModProcessor::serviceAPI()
{
    if (!m_api.active() || m_apiGenerating) {
        return;
    }

    if (m_api.aborted())
    {
        finishAPI("aborted");
        return;
    }

    if (m_api.expired())
    {
        finishAPI("failed", "Transmit session maximum duration reached");
        return;
    }

    if (m_api.timedOut())
    {
        finishAPI("failed", "Transmit session timed out");
        return;
    }

    if (m_apiDraining)
    {
        if (m_apiOutputDrained) {
            finishAPI("completed");
        }

        return;
    }

    if (!m_apiStarted)
    {
        if (!m_apiMessage && !m_api.pcmBytes() && !m_api.frameCount())
        {
            if (m_api.closed()) {
                finishAPI("completed");
            }

            return;
        }

        // Angel61 arrives as 80 or 240 ms RF frames. Keep enough frames ahead of the
        // transmitter to absorb decoder and UDP scheduling jitter at key-up.
        const int primeFrames = DSEModSettings::isAngel61(m_settings.m_mode) ? 5 : 3;
        // A relay's tier is the call's or lower: it waits for the call's first voice frame (or
        // 16 frames without one) to know it
        const bool rateUnknown = (m_api.input() == "angel61") && DSEModSettings::isAngel61(m_settings.m_mode)
            && (firstCallVoiceType() < 0) && (m_api.frameCount() < 16);

        // Streaming input is primed before key-up. Finite sends can be shorter.
        if (!m_api.closed() && !m_apiMessage
            && (((m_api.input() == "pcm") && (m_api.pcmBytes() < m_api.rate() * 2 / 5))
                || (((m_api.input() == "frames") || (m_api.input() == "angel61"))
                    && ((m_api.frameCount() < primeFrames) || rateUnknown))))
        {
            return;
        }

        startAPI();

        if (!m_api.active()) {
            return;
        }
    }

    if (((m_api.input() == "pcm") || apiTranscoding()) && !m_apiMessage)
    {
        if (!m_vocoder->isReady())
        {
            finishAPI("failed", "Speech encoder became unavailable");
            return;
        }

        encodeAPI();

        if (!m_apiPending.empty() && m_apiEncoderWait.isValid() && (m_apiEncoderWait.elapsed() >= 5000))
        {
            finishAPI("failed", "Speech encoder timed out");
            return;
        }
    }
    else if (m_apiAngel61 && !m_apiMessage)
    {
        relayAngel61API();
    }

    if (((m_api.input() == "frames") || (m_api.input() == "angel61"))
        && !m_api.closed() && !m_api.frameCount() && !m_fifo.getFill())
    {
        if (!m_apiStarved.isValid()) {
            m_apiStarved.start();
        }

        // DSGateway closes a network call after 1.5 s without media (4.5 s for an
        // Angel61 HF call's). Leave room for its timer and cross-thread finish
        // delivery instead of failing a session during a recoverable network gap:
        // Angel61, as long as the session's own idle limit
        const int timeoutMs = DSEModSettings::isAngel61(m_settings.m_mode) ? DSEModTxSession::IdleTimeoutMs : 2000;

        if (m_apiStarved.elapsed() >= timeoutMs)
        {
            finishAPI("failed", "Gateway frame input underrun");
            return;
        }
    }
    else
    {
        m_apiStarved.invalidate();
    }

    generateAPI();
}

void DSEModProcessor::generateAPI()
{
    m_apiGenerating = true;

    while (m_api.active() && (m_fifo.getFill() < m_fifoHighWater * m_fifoChannels))
    {
        m_samples.clear();
        m_levels.clear();
        m_symbols.clear();
        bool ended = false;

        if (apiRelaying())
        {
            if (m_api.frameCount())
            {
                QString error;
                DSEModTxSession::Frame frame = m_api.takeFrame();

                if (m_settings.m_feedbackAudioEnable && m_feedbackSource && !m_apiMonitorUnavailable
                    && ((m_settings.m_mode == DSEModSettings::DSEModModeDMR)
                        || (m_settings.m_mode == DSEModSettings::DSEModModeDStar)))
                {
                    QByteArray pcm;
                    QString monitorError;

                    if (!m_apiMbeMonitor.decode(frame, pcm, monitorError))
                    {
                        qWarning("DSEModProcessor: gateway audio feedback unavailable: %s",
                            qPrintable(monitorError));
                        m_apiMonitorUnavailable = true;
                    }
                    else
                    {
                        queueGatewayFeedback(pcm, 8000);
                    }
                }

                if (!m_packets.append(frame, m_levels, error))
                {
                    finishAPI("failed", error);
                    break;
                }

                m_api.consumed(1);
            }
            else if (m_api.closed())
            {
                m_packets.finish(m_levels);
                ended = true;
            }
            else
            {
                break;
            }

            if (!m_levels.empty())
            {
                invertLevels(m_levels);
                m_shaper->shapeWithGaps(m_levels.data(), (int) m_levels.size(), m_samples);
                ++m_txFrameCount;
            }
        }
        else
        {
            if (!m_apiMessage)
            {
                const bool noQueuedInput = m_apiAngel61 ? !m_api.frameCount() : !m_api.pcmBytes();
                const bool exhausted = m_apiInputEnded && noQueuedInput && m_apiPCM.empty() && m_apiPending.empty();

                if (!exhausted && ((int) m_encodedFrames.size() < m_protocol->getFramesPerBlock()))
                {
                    // Keep protocol timing through short input/encoder stalls.
                    // Missing frames become silence; accepted audio stays queued.
                    if (!m_txFrameCount || (m_fifo.getFill() >= m_fifoLowWater * m_fifoChannels)) {
                        break;
                    }
                }

                if (exhausted && m_encodedFrames.empty()) {
                    m_protocol->stop();
                }
            }
            else if ((m_apiMessageFrames > 0) && (m_protocol->getVoiceFrameCount() >= (uint32_t) m_apiMessageFrames))
            {
                m_protocol->stop();
            }

            if (m_protocol->isOFDM())
            {
                if (!ofdmFrame()) {
                    ended = true;
                }
            }
            else if (m_protocol->isLinear())
            {
                if (!m_protocol->nextSymbols(m_symbols)) {
                    ended = true;
                } else {
                    m_linearShaper.shape(m_symbols.data(), (int) m_symbols.size(), m_samples);
                }
            }
            else
            {
                if (!m_protocol->nextFrame(m_levels))
                {
                    ended = true;
                }
                else
                {
                    // (as generate() does: DMR direct mode's carrier-off markers are no symbols,
                    // and shaped as levels they keyed full deviation in its gaps)
                    invertLevels(m_levels);
                    m_shaper->shapeWithGaps(m_levels.data(), (int) m_levels.size(), m_samples);
                }
            }

            ++m_txFrameCount;
            ended = ended || !m_protocol->isActive();
        }

        if (ended)
        {
            if (m_protocol->isLinear()) {
                m_linearShaper.flush(m_samples);
            } else if (m_shaper) {
                m_shaper->flush(m_samples);
            }
        }

        writeSamples();

        if (ended)
        {
            m_api.setState("draining");
            m_apiDraining = true;
            break;
        }
    }

    m_apiGenerating = false;
}
