///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2020, 2022-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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
#include <cstdint>
#include <cmath>

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QGeoCoordinate>
#include <QUuid>
#include <QSet>

#include "dsp/basebandsamplesink.h"
#include "dsp/datafifo.h"
#include "dsp/dspcommands.h"
#include "dsp/dv/dvframe.h"
#include "dsp/dv/dvbits.h"
#include "feature/feature.h"
#include "util/aprs.h"
#include "util/maidenhead.h"
#include "util/messagequeue.h"
#include "maincore.h"

#include "SWGMapItem.h"

#include "dsdprotocoldmr.h"
#include "dsdprotocoldstar.h"
#include "dsdprotocolysf.h"
#include "dsdprotocolnxdn.h"
#include "dsdprotocolm17.h"
#include "dsdprotocolp25.h"
#include "dsdprotocoldpmr.h"
#include "dsddemodsink.h"

namespace {

//! For a quoted field of the event log
QString escapeEventText(const std::string& text)
{
    QString escaped = QString::fromUtf8(text.c_str());
    escaped.replace('\\', "\\\\");
    escaped.replace('"', "\\\"");
    return escaped;
}

QString toHex(const std::vector<uint8_t>& bytes)
{
    return QString::fromLatin1(QByteArray::fromRawData((const char *) bytes.data(), (int) bytes.size()).toHex());
}

}

DSDDemodSink::DSDDemodSink() :
    m_channelSampleRate(48000),
    m_channelFrequencyOffset(0),
    m_channel(nullptr),
    m_audioSampleRate(48000),
    m_upsampling(6),
    m_interpolatorDistance(0.0f),
    m_interpolatorDistanceRemain(0.0f),
    m_sampleCount(0),
    m_myLatitude(0.0f),
    m_myLongitude(0.0f),
    m_hasMyPosition(false),
    m_noVocoderText(),
    m_audioFifo1(48000),
    m_audioFifo2(48000),
    m_scopeXY(nullptr),
    m_scopeLocked(false),
    m_berEstimate(0.0f),
    m_dmrMetadataDecoder(nullptr),
    m_dstarMetadataDecoder(nullptr),
    m_dstarMapPositionValid(false),
    m_lastDStarMapLatitude(0.0),
    m_lastDStarMapLongitude(0.0),
    m_angel61IPRxPackets(0),
    m_angel61IPRxBytes(0),
    m_eventSequence(0),
    m_messageSequence(0),
    m_lastDPRSRevision(0)
{
    m_demodBuffer.resize(1<<12);
    m_demodBufferFill = 0;
    m_iqBlock.reserve(480);

    for (Stream& s : m_streams) {
        s.m_audioBuffer.reserve(160 * 6);
    }

    for (int slot = 0; slot < 2; slot++)
    {
        m_dmrAliasRevision[slot] = 0;
        m_dmrGPSRevision[slot] = 0;
        m_dmrGPSMapRevision[slot] = 0;
        m_dmrTextRevision[slot] = 0;
        m_lastDMRGPSEventSample[slot] = -48000;
    }

	m_magsq = 0.0f;
    m_magsqSum = 0.0f;
    m_magsqPeak = 0.0f;
    m_magsqCount = 0;

    m_demodulator.setDiscriminatorOutput(false); // the data pipes carry the audio, not the discriminator
	applySettings(QStringList(), m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

DSDDemodSink::~DSDDemodSink()
{
    setAmbePort(nullptr); // the feature's workers hold the audio FIFOs, which go with this sink
}

void DSDDemodSink::setAmbePort(std::shared_ptr<MbeEncoderPort> port)
{
    std::shared_ptr<MbeEncoderPort> previous;

    {
        QMutexLocker locker(&m_ambePortMutex);
        previous = m_ambePort;
        m_ambePort = port;
    }

    if (previous && (previous != port))
    {
        DSPReleaseMbeClient release1(&m_audioFifo1);
        DSPReleaseMbeClient release2(&m_audioFifo2);
        previous->handleMessage(release1);
        previous->handleMessage(release2);
    }
}

DSDDemodulator::Protocol DSDDemodSink::toProtocol(int protocol)
{
    switch (protocol)
    {
    case DSDDemodSettings::ProtocolDMR: return DSDDemodulator::ProtocolDMR;
    case DSDDemodSettings::ProtocolDStar: return DSDDemodulator::ProtocolDStar;
    case DSDDemodSettings::ProtocolYSF: return DSDDemodulator::ProtocolYSF;
    case DSDDemodSettings::ProtocolNXDN48: return DSDDemodulator::ProtocolNXDN48;
    case DSDDemodSettings::ProtocolNXDN96: return DSDDemodulator::ProtocolNXDN96;
    case DSDDemodSettings::ProtocolDPMR: return DSDDemodulator::ProtocolDPMR;
    case DSDDemodSettings::ProtocolP25: return DSDDemodulator::ProtocolP25;
    case DSDDemodSettings::ProtocolP25P2: return DSDDemodulator::ProtocolP25P2;
    case DSDDemodSettings::ProtocolM17: return DSDDemodulator::ProtocolM17;
    case DSDDemodSettings::ProtocolAngel61: return DSDDemodulator::ProtocolAngel61;
    case DSDDemodSettings::ProtocolAngel61WB: return DSDDemodulator::ProtocolAngel61WB;
    case DSDDemodSettings::ProtocolAngel61HFNB: return DSDDemodulator::ProtocolAngel61HFNB;
    case DSDDemodSettings::ProtocolAngel61HFWB: return DSDDemodulator::ProtocolAngel61HFWB;
    default: return DSDDemodulator::ProtocolAuto;
    }
}

void DSDDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
	Complex ci;

	for (SampleVector::const_iterator it = begin; it != end; ++it)
	{
		Complex c(it->real(), it->imag());
		c *= m_nco.nextIQ();

        if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
        {
            Real re = ci.real() / SDR_RX_SCALED;
            Real im = ci.imag() / SDR_RX_SCALED;
            Real magsq = re*re + im*im;
            m_movingAverage(magsq);
            m_magsqSum += magsq;

            if (magsq > m_magsqPeak) {
                m_magsqPeak = magsq;
            }

            m_magsqCount++;
            m_iqBlock.push_back(std::complex<float>(re, im));

            if (m_iqBlock.size() >= 480) {
                processBlock();
            }

            m_interpolatorDistanceRemain += m_interpolatorDistance;
        }
	}
}

void DSDDemodSink::processBlock()
{
    // No power squelch: the sync search is the gate, and the demodulator needs the quiet periods
    // for its noise floor
    m_demodulator.process(m_iqBlock.data(), (int) m_iqBlock.size());
    m_sampleCount += (long long) m_iqBlock.size();

    for (const DSDVoiceFrame& frame : m_demodulator.takeFrames()) {
        processFrame(frame);
    }

    playDTMFAlone();
    endHeardDTMF();

    for (const DSDProtocolEvent& event : m_demodulator.takeProtocolEvents()) {
        publishDVEvent(event);
    }

    m_iqBlock.clear();

    // Scope: consecutive discriminator samples while searching, or symbol-timed matched-filter
    // samples while locked, in units of the nominal outer level (DMR's while searching); the
    // equalised constellation of an Angel61 transmission
    {
        QMutexLocker scopeLocker(&m_scopeMutex);

        if (m_scopeXY)
        {
            bool scopeLocked = m_demodulator.isLocked();

            if (scopeLocked != m_scopeLocked)
            {
                m_scopeXY->start();
                m_scopeLocked = scopeLocked;
            }

            m_scopeSampleBuffer.clear();
            float norm = SDR_RX_SCALEF * 0.8f / m_demodulator.getScopeFullScale();

            for (const auto& p : m_demodulator.takeScopeSamples())
            {
                FixReal x = (FixReal) std::max(-SDR_RX_SCALEF, std::min(SDR_RX_SCALEF - 1.0f, p.first * norm));
                FixReal y = (FixReal) std::max(-SDR_RX_SCALEF, std::min(SDR_RX_SCALEF - 1.0f, p.second * norm));
                m_scopeSampleBuffer.push_back(Sample(x, y));
            }

            if (!m_scopeSampleBuffer.empty()) {
                m_scopeXY->feed(m_scopeSampleBuffer.begin(), m_scopeSampleBuffer.end(), true);
            }
        }
        else
        {
            m_demodulator.takeScopeSamples();
        }
    }

    updateReport();
}

void DSDDemodSink::publishDVEvent(const DSDProtocolEvent& input)
{
    DVFrameEvent event;
    event.callId = input.m_callId;
    event.sequence = input.m_sequence;
    event.sampleIndex = m_sampleCount;
    event.protocol = QString::fromStdString(input.m_protocol).toLower();
    event.protocol.remove('-');
    event.reason = QString::fromStdString(input.m_reason);
    event.kind = input.m_kind == DSDProtocolEvent::Start ? DVFrameEvent::Start
        : input.m_kind == DSDProtocolEvent::End ? DVFrameEvent::End
        : input.m_kind == DSDProtocolEvent::Lost ? DVFrameEvent::Lost
        : DVFrameEvent::Frame;

    if (const auto *dstar = dynamic_cast<const DSDProtocolDStar *>(m_demodulator.getDecoder()))
    {
        event.source = QString::fromStdString(dstar->getMyCall()).trimmed();
        event.destination = QString::fromStdString(dstar->getUrCall()).trimmed();
    }
    else if (const auto *ysf = dynamic_cast<const DSDProtocolYSF *>(m_demodulator.getDecoder()))
    {
        event.source = QString::fromStdString(ysf->getSource()).trimmed();
        event.destination = QString::fromStdString(ysf->getDestination()).trimmed();
    }
    else if (const auto *dmr = dynamic_cast<const DSDProtocolDMR *>(m_demodulator.getDecoder()))
    {
        // The frame's slot is 1 or 2 (0: an event of the whole call, taken as slot 1); the
        // decoder's link control is 0 or 1
        const int slot = input.m_frame.m_slot > 0 ? input.m_frame.m_slot : 1;
        const DSDProtocolDMR::LinkControl lc = dmr->getLinkControl(slot - 1);

        if (lc.m_valid)
        {
            event.source = QString::number(lc.m_source);
            event.destination = QString::number(lc.m_destination);
            event.group = lc.m_group;
        }
    }

    if (input.m_kind == DSDProtocolEvent::Frame)
    {
        event.frame.format = QString::fromStdString(input.m_frame.m_format);
        event.frame.type = QString::fromStdString(input.m_frame.m_type);
        event.frame.slot = input.m_frame.m_slot;
        event.frame.sequence = input.m_frame.m_sequence;
        event.frame.data = QByteArray(
            reinterpret_cast<const char *>(input.m_frame.m_data.data()),
            (int) input.m_frame.m_data.size());
    }
    publishDVFrameEvent(event);
}

void DSDDemodSink::publishDVFrameEvent(const DVFrameEvent& event)
{
    if (!m_channel) {
        return;
    }

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_channel, "dvframes", pipes);
    QSet<MessageQueue*> currentQueues;

    for (const ObjectPipe *pipe : pipes)
    {
        if (MessageQueue *queue = qobject_cast<MessageQueue*>(pipe->m_element)) {
            currentQueues.insert(queue);
        }
    }

    for (auto it = m_dvSuppressedCalls.begin(); it != m_dvSuppressedCalls.end(); )
    {
        if (!currentQueues.contains(it.key())) {
            it = m_dvSuppressedCalls.erase(it);
        } else {
            ++it;
        }
    }

    static constexpr int MaxQueueDepth = 64;
    static constexpr int ResumeQueueDepth = 16;

    for (MessageQueue *queue : currentQueues)
    {
        auto suppressed = m_dvSuppressedCalls.find(queue);

        if (suppressed != m_dvSuppressedCalls.end())
        {
            if ((event.kind != DVFrameEvent::Start) || (queue->size() > ResumeQueueDepth)) {
                continue;
            }

            m_dvSuppressedCalls.erase(suppressed);
        }

        if (queue->size() >= MaxQueueDepth)
        {
            DVFrameEvent lost = event;
            lost.kind = DVFrameEvent::Lost;
            lost.reason = "dvframes consumer overflow";
            queue->push(MsgDVFrame::create(lost)); // The reserved overflow-notification slot.
            m_dvSuppressedCalls.insert(queue, event.callId);
            continue;
        }

        queue->push(MsgDVFrame::create(event));
    }
}

void DSDDemodSink::processFrame(const DSDVoiceFrame& frame)
{
    int index = frame.m_slot & 1;
    Stream& stream = m_streams[index];

    // Half a second without voice on the stream is a new transmission (a new lock, a new call
    // on the slot): the vocoder restarts rather than continuing from the previous call's frames
    if ((stream.m_lastVoiceSample < 0) || (m_sampleCount - stream.m_lastVoiceSample > 24000)) {
        stream.restart();
    }

    stream.m_lastVoiceSample = m_sampleCount;

    // The raw BER shown (B.4): the channel's errors the frame's codes corrected, counted whatever
    // vocoder plays it - the protocol's own count (M17, YSF V/D type 2), else the MBE frame's codes
    // decoded hard. Angel61's come from its session. An erased frame has none to count
    if (!frame.m_erasure)
    {
        int errors = 0, bits = 0;

        if (frame.m_channelBits > 0)
        {
            errors = frame.m_channelErrors;
            bits = frame.m_channelBits;
        }
        else if (!DSDVocoderMbelib::channelErrors(frame, errors, bits))
        {
            errors = 0;
            bits = 0;
        }

        stream.m_correctedBits += errors;
        stream.m_protectedBits += bits;
        stream.m_correctedBitsTotal += errors;
        stream.m_protectedBitsTotal += bits;
        stream.m_voiceFramesTotal++;
    }
    else
    {
        stream.m_voiceFramesErasedTotal++;
    }

    // DTMF (the FM protocols): the keys of AMBE's tone frames, as text whether the voice is played or not
    if (!DSDVoiceFrame::isOpus(frame.m_rate) && !DSDVoiceFrame::isVocos(frame.m_rate) && !frame.m_erasure) {
        toneFrameKey(stream, DSDVocoderMbelib::dtmfKey(frame));
    }

    bool enabled = index == 0 ? m_settings.m_slot1On : m_settings.m_slot2On;
    bool colorCodeMatches = true;   // The CC control: DMR's and dPMR's colour code, NXDN's RAN

    if (m_settings.m_dmrColorCode >= 0)
    {
        const auto *decoder = m_demodulator.getDecoder();

        if (const auto *dmr = dynamic_cast<const DSDProtocolDMR*>(decoder)) {
            colorCodeMatches = dmr->getColorCode() == m_settings.m_dmrColorCode;
        } else if (const auto *dpmr = dynamic_cast<const DSDProtocolDPMR*>(decoder)) {
            colorCodeMatches = dpmr->getColourCode() == m_settings.m_dmrColorCode;
        } else if (const auto *nxdn = dynamic_cast<const DSDProtocolNXDN*>(decoder)) {
            colorCodeMatches = nxdn->getRAN() == m_settings.m_dmrColorCode;
        }
    }

    bool groupPlayed = !(DSDVoiceFrame::isOpus(frame.m_rate) || DSDVoiceFrame::isVocos(frame.m_rate))
        || angel61CallPlayed();

    if (!enabled || m_settings.m_audioMute || !colorCodeMatches || !groupPlayed) {
        return;
    }

    unsigned char channels = m_settings.m_tdmaStereo ? (index == 0 ? 1 : 2) : 3;
    AudioFifo *fifo = index == 0 ? &m_audioFifo1 : &m_audioFifo2;

    if (frame.m_rate == DSDVoiceFrame::RateCodec23200)
    {
        // M17: Codec2 3200, 160 samples at 8 kHz, linked in rather than loaded
        float audio[DSDVocoderCodec2::m_samplesPerFrame];

        if (!stream.m_codec2.decode(frame, audio))
        {
            m_noVocoderText = "voice unavailable: Codec2 could not open";
            return;
        }

        m_noVocoderText.clear();
        detectDTMF(stream, audio, DSDVocoderCodec2::m_samplesPerFrame);
        outputAudio(stream, index, audio, DSDVocoderCodec2::m_samplesPerFrame, DSDVocoderCodec2::m_sampleRate);
        return;
    }

    if (DSDVoiceFrame::isOpus(frame.m_rate))
    {
        // Angel61 WB audio: Opus, part of sdrbase (nothing to load), 3840 samples at 48 kHz
        float audio[DSDVocoderOpus::m_maxSamplesPerFrame];

        if (!stream.m_opus.decode(frame, audio))
        {
            m_noVocoderText = "voice unavailable: the Opus decoder failed";
            return;
        }

        m_noVocoderText.clear();
        overlayDTMF(stream, audio, DSDVoiceFrame::getAudioSamples(frame.m_rate), DSDVocoderOpus::m_sampleRate);
        outputAudio(stream, index, audio, DSDVoiceFrame::getAudioSamples(frame.m_rate), DSDVocoderOpus::m_sampleRate);
        return;
    }

    if (DSDVoiceFrame::isVocos(frame.m_rate))
    {
        // Angel61: V1, a radio frame's packets in one call, 24 kHz (ANGEL61.md 3.1.1)
        if (!DSDVocoderVocos::isAvailable())
        {
            m_noVocoderText = "voice unavailable: " + QString::fromStdString(VocosCodec::getError());
            return;
        }

        std::vector<float> audio(DSDVocoderVocos::m_maxSamples);
        int samples = 0;

        if (!stream.m_vocos.decode(frame, audio.data(), samples))
        {
            m_noVocoderText = stream.m_vocos.getError().empty() ? QString("voice unavailable: V1 models not found")
                : QString::fromStdString(stream.m_vocos.getError());
            return;
        }

        m_noVocoderText.clear();

        if (samples > 0)
        {
            overlayDTMF(stream, audio.data(), samples, DSDVocoderVocos::m_sampleRate);
            outputAudio(stream, index, audio.data(), samples, DSDVocoderVocos::m_sampleRate);
        }

        return;
    }

    if (const std::shared_ptr<MbeEncoderPort> port = ambePort())
    {
        // Hard bits to the AMBE feature in the DVSI order (D-STAR bytes LSB first, the others MSB first)
        int rateIndex;

        switch (frame.m_rate)
        {
        case DSDVoiceFrame::RateAMBE3600x2400: rateIndex = 1; break; // SerialDV::DVRate3600x2400
        case DSDVoiceFrame::RateAMBE3600x2450: rateIndex = 2; break;
        case DSDVoiceFrame::RateIMBE7200x4400: rateIndex = 3; break;
        case DSDVoiceFrame::RateAMBE7200x4400: rateIndex = 3; break; // the DVSI chip's 7200x4400 rate covers both
        case DSDVoiceFrame::RateAMBE2450: rateIndex = 6; break;
        default: return;
        }

        DSPPushMbeFrame msg(frame.m_bytes, rateIndex, (int) std::lround(m_settings.m_volume * 10.0f), channels, m_settings.m_highPassFilter, m_upsampling, fifo);
        if (port->handleMessage(msg)) {
            return;
        }
        // (the feature is gone: the software vocoder, as when there is none)
    }

    if (!DSDVocoderMbelib::isAvailable())
    {
        m_noVocoderText = "no vocoder: mbevocoder not compiled in, use the AMBE feature";
        return;
    }

    float audio[DSDVocoderMbelib::m_samplesPerFrame];
    DSDVocoderMbelib::Result result;

    if (!stream.m_vocoder.decode(frame, audio, result))
    {
        m_noVocoderText = (frame.m_rate == DSDVoiceFrame::RateAMBE7200x4400)
            ? "no software vocoder for the full rate AMBE (EFR): use the AMBE feature"
            : "no software vocoder for this rate: use the AMBE feature";
        return;
    }

    m_noVocoderText.clear();
    detectDTMF(stream, audio, DSDVocoderMbelib::m_samplesPerFrame);
    outputAudio(stream, index, audio, DSDVocoderMbelib::m_samplesPerFrame, 8000);
}

void DSDDemodSink::toneFrameKey(Stream& stream, char key)
{
    if (key && (key == stream.m_toneKey))
    {
        stream.m_toneKeySample = m_sampleCount;

        if (++stream.m_toneRun == 2) {
            heardDTMF(stream, key, true);
        }
    }
    else
    {
        stream.m_toneKey = key;
        stream.m_toneRun = key ? 1 : 0;

        if (key) {
            stream.m_toneKeySample = m_sampleCount;
        }
    }
}

void DSDDemodSink::detectDTMF(Stream& stream, const float *audio, int n)
{
    // Keys a far end coded as voice (IMBE, Codec2, an AMBE vocoder without tone frames), ITU-T Q.24's limits
    std::vector<float> x(n);

    for (int i = 0; i < n; i++) {
        x[i] = audio[i] / 32768.0f;
    }

    std::vector<DVDTMF::Detector::Key> keys;
    stream.m_dtmfDetector.process(x.data(), n, keys);

    for (const DVDTMF::Detector::Key& key : keys) {
        heardDTMF(stream, key.m_key, false);
    }
}

void DSDDemodSink::heardDTMF(Stream& stream, char key, bool fromToneFrames)
{
    // The tones of tone frames reach the detector too: its key within 500 ms of them is theirs
    if (!fromToneFrames && (stream.m_toneKeySample >= 0) && (m_sampleCount - stream.m_toneKeySample < 24000)) {
        return;
    }

    if (stream.m_dtmfHeard.empty()) {
        stream.m_dtmfProtocol = QString::fromStdString(m_demodulator.getStatus().m_protocol);
    }

    if (stream.m_dtmfHeard.size() < 64) {
        stream.m_dtmfHeard += key;
    }

    stream.m_dtmfHeardSample = m_sampleCount;
}

void DSDDemodSink::endHeardDTMF()
{
    for (int index = 0; index < 2; index++)
    {
        Stream& stream = m_streams[index];

        if (stream.m_dtmfHeard.empty() || (m_sampleCount - stream.m_dtmfHeardSample < 2 * DSDFrontEnd::m_sampleRate)) {
            continue;
        }

        const QString keys = QString::fromStdString(stream.m_dtmfHeard);
        const QString protocol = stream.m_dtmfProtocol.isEmpty() ? QString("DSD") : stream.m_dtmfProtocol;
        addEvent(QString("DSD EVT:DTMF PROTOCOL:%1 SLOT:%2 KEYS:%3").arg(protocol).arg(index + 1).arg(keys));
        addTextMessage(protocol, "--", "--", "DTMF " + keys, true);
        m_lastDTMF = keys;
        stream.m_dtmfHeard.clear();
    }
}

int DSDDemodSink::overlayDTMF(Stream& stream, float *audio, int n, int rate)
{
    int done = 0;

    while (done < n)
    {
        if (!stream.m_dtmf.active())
        {
            if (stream.m_dtmfPending.empty()) {
                break;
            }

            // The next key at the rate of the audio it goes into (V1's 24 kHz, Opus's 48)
            if (stream.m_dtmf.pending() == 0) {
                stream.m_dtmf.setSampleRate(rate);
            }

            const DSDAngel61Session::DTMFKey key = stream.m_dtmfPending.front();
            stream.m_dtmfPending.pop_front();
            stream.m_dtmf.queue(key.m_key, key.m_toneMs, key.m_gapMs);
        }

        const int start = done;
        done += stream.m_dtmf.generate(audio + done, n - done);

        for (int i = start; i < done; i++) {
            audio[i] *= 32767.0f;
        }
    }

    stream.m_dtmfClock = m_sampleCount;
    return done;
}

void DSDDemodSink::playDTMFAlone()
{
    Stream& stream = m_streams[0];
    // Voice in the last 600 ms (more than an HF frame apart): the tones go into it (processFrame)
    const bool voice = (stream.m_lastVoiceSample >= 0) && (m_sampleCount - stream.m_lastVoiceSample < 28800);

    if (voice || (stream.m_dtmfPending.empty() && !stream.m_dtmf.active()))
    {
        stream.m_dtmfClock = m_sampleCount;
        return;
    }

    // Played as the call's voice would be, or not at all
    if (!m_settings.m_slot1On || m_settings.m_audioMute || !angel61CallPlayed())
    {
        stream.m_dtmfPending.clear();
        stream.m_dtmf.reset();
        stream.m_dtmfClock = m_sampleCount;
        return;
    }

    if (stream.m_dtmfClock < 0) {
        stream.m_dtmfClock = m_sampleCount;
    }

    const int rate = DSDVocoderVocos::m_sampleRate;
    const int n = (int) ((m_sampleCount - stream.m_dtmfClock) * rate / DSDFrontEnd::m_sampleRate);

    if (n <= 0) {
        return;
    }

    const int64_t clock = stream.m_dtmfClock + (int64_t) n * DSDFrontEnd::m_sampleRate / rate;
    std::vector<float> audio(n, 0.0f);
    const int done = overlayDTMF(stream, audio.data(), n, rate);
    stream.m_dtmfClock = clock;

    if (done > 0) {
        outputAudio(stream, 0, audio.data(), done, rate);
    }
}

void DSDDemodSink::createResampler(Stream& stream, int vocoderRate)
{
    // Polyphase resampling from the vocoder rate to the audio device rate, whatever the ratio (a 44.1
    // or 96 kHz device gets exactly its rate); the anti-imaging low pass is the resampler's
    stream.m_vocoderRate = vocoderRate;
    // Opus (48 kHz) keeps what its bandwidth carries, up to what the device rate can hold
    double cutoff = vocoderRate == 8000 ? 3400.0 : vocoderRate == 16000 ? 7000.0
        : std::min(20000.0, 0.45 * std::min(vocoderRate, std::max(1, m_audioSampleRate)));
    stream.m_resampler.create(16, (double) vocoderRate, cutoff);
    stream.m_resampleDistance = (Real) vocoderRate / (Real) std::max(1, m_audioSampleRate);
    stream.m_resampleRemain = 0.0f;
}

void DSDDemodSink::outputAudio(Stream& stream, int streamIndex, const float *audio, int nbSamples, int vocoderRate)
{
    unsigned char channels = m_settings.m_tdmaStereo ? (streamIndex == 0 ? 1 : 2) : 3;
    float gain = m_settings.m_volume;
    // The high pass is designed for 8 kHz audio: it is not applied to V1's 24 kHz or Opus's 48 kHz
    stream.m_filter.useHP(m_settings.m_highPassFilter && (vocoderRate == 8000));
    stream.m_audioBuffer.clear();

    if (stream.m_vocoderRate != vocoderRate) {
        createResampler(stream, vocoderRate);
    }

    auto push = [&](float v) {
        qint16 s = (qint16) std::max(-32767.0f, std::min(32767.0f, v));
        AudioSample sample;
        sample.l = (channels & 1) ? s : 0;
        sample.r = (channels & 2) ? s : 0;
        stream.m_audioBuffer.push_back(sample);
    };

    for (int i = 0; i < nbSamples; i++)
    {
        float cur = audio[i] * gain;

        if (stream.m_filter.usesHP()) {
            cur = stream.m_filter.runHP(cur);
        }

        Complex in(cur, 0.0f), out;

        if (stream.m_resampleDistance < 1.0f)
        {
            while (!stream.m_resampler.interpolate(&stream.m_resampleRemain, in, &out))
            {
                push(out.real());
                stream.m_resampleRemain += stream.m_resampleDistance;
            }
        }
        else if (stream.m_resampler.decimate(&stream.m_resampleRemain, in, &out))
        {
            push(out.real());
            stream.m_resampleRemain += stream.m_resampleDistance;
        }
    }

    AudioFifo *fifo = streamIndex == 0 ? &m_audioFifo1 : &m_audioFifo2;
    fifo->write((const quint8*) stream.m_audioBuffer.data(), (uint) stream.m_audioBuffer.size());

    // The same audio, mono, to the "demod" data pipes (the Demod Analyzer, the Morse Decoder...) at
    // the audio rate, as the analogue voice demodulators send theirs
    for (const AudioSample& sample : stream.m_audioBuffer)
    {
        m_demodBuffer[m_demodBufferFill] = (channels & 1) ? sample.l : sample.r;

        if (++m_demodBufferFill >= m_demodBuffer.size())
        {
            QList<ObjectPipe*> dataPipes;
            MainCore::instance()->getDataPipes().getDataPipes(m_channel, "demod", dataPipes);

            for (ObjectPipe *pipe : dataPipes)
            {
                DataFifo *dataFifo = qobject_cast<DataFifo*>(pipe->m_element);

                if (dataFifo) {
                    dataFifo->write((quint8*) &m_demodBuffer[0], m_demodBuffer.size() * sizeof(qint16), DataFifo::DataTypeI16);
                }
            }

            m_demodBufferFill = 0;
        }
    }
}

void DSDDemodSink::applyAudioSampleRate(int sampleRate)
{
    if (sampleRate < 0)
    {
        qWarning("DSDDemodSink::applyAudioSampleRate: invalid sample rate: %d", sampleRate);
        return;
    }

    // The software path resamples 8 kHz to the device rate exactly; the AMBE feature upsamples by
    // a whole number itself, so it gets the nearest one
    m_upsampling = std::max(1, std::min(6, (int) std::lround(sampleRate / 8000.0)));
    qDebug("DSDDemodSink::applyAudioSampleRate: audio rate: %d (AMBE feature upsampling by %d)", sampleRate, m_upsampling);

    if ((sampleRate % 8000 != 0) && (m_settings.m_connectAMBE || ambePort())) {
        qDebug("DSDDemodSink::applyAudioSampleRate: the AMBE feature sounds best at audio sample rates that are multiples of 8 kHz");
    }

    m_audioSampleRate = sampleRate;

    for (Stream& stream : m_streams) {
        createResampler(stream, stream.m_vocoderRate);
    }

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_channel, "reportdemod", pipes);

    for (const auto& pipe : pipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(m_channel, sampleRate);
        messageQueue->push(msg);
    }
}

void DSDDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "DSDDemodSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " inputFrequencyOffset: " << channelFrequencyOffset;

    if ((channelFrequencyOffset != m_channelFrequencyOffset) ||
        (channelSampleRate != m_channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((channelSampleRate != m_channelSampleRate) || force)
    {
        m_interpolator.create(16, channelSampleRate, DSDDemodulator::inputCutoff(toProtocol(m_settings.m_protocol), m_settings.m_rfBandwidth));
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance =  (Real) channelSampleRate / (Real) 48000;
        m_demodulator.reset();
        m_angel61Gateway.reset([this](const DVFrameEvent& event) { publishDVFrameEvent(event); });
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void DSDDemodSink::applySettings(const QStringList& settingsKeys, const DSDDemodSettings& settings, bool force)
{
    qDebug() << "DSDDemodSink::applySettings: " << settings.getDebugString(settingsKeys, force);

    // The decimation's cutoff follows the protocol as well as the RF bandwidth: Angel61 needs its
    // signal passed flat (DSDDemodulator::inputCutoff)
    if ((settingsKeys.contains("rfBandwidth") && (settings.m_rfBandwidth != m_settings.m_rfBandwidth))
        || (settingsKeys.contains("protocol") && (settings.m_protocol != m_settings.m_protocol)) || force)
    {
        m_interpolator.create(16, m_channelSampleRate, DSDDemodulator::inputCutoff(toProtocol(settings.m_protocol), settings.m_rfBandwidth));
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance =  (Real) m_channelSampleRate / (Real) 48000;
        m_demodulator.setRfBandwidth(settings.m_rfBandwidth);
    }

    if ((settingsKeys.contains("protocol") && (settings.m_protocol != m_settings.m_protocol)) || force)
    {
        m_demodulator.setProtocol(toProtocol(settings.m_protocol));
        m_angel61Gateway.reset([this](const DVFrameEvent& event) { publishDVFrameEvent(event); });
    }

    // Angel61 revision 3's V1 models (ANGEL61.md 8): the next voice frame loads them
    if (settingsKeys.contains("codecModelDirectory") || force)
    {
        for (Stream& stream : m_streams) {
            stream.m_vocos.setModelDirectory(settings.m_codecModelDirectory.toStdString());
        }
    }

    if (settingsKeys.contains("p25p2Wacn") || settingsKeys.contains("p25p2Sysid") || settingsKeys.contains("p25p2Nac") || force) {
        m_demodulator.setP25P2System((uint32_t) settings.m_p25p2Wacn, (uint16_t) settings.m_p25p2Sysid, (uint16_t) settings.m_p25p2Nac);
    }

    if (settingsKeys.contains("coherent") || force) {
        m_demodulator.setCoherent(settings.m_coherent);
    }

    if (settingsKeys.contains("angel61Groups") || force) {
        m_angel61Groups = DVAngel61::parseTalkgroupList(settings.m_angel61Groups.toStdString());
    }

    if (force) {
        const bool fileDirectoryChanged = (settingsKeys.contains("angel61FileDirectory")
        && (settings.m_angel61FileDirectory != m_settings.m_angel61FileDirectory)) || force;
    m_settings = settings;

    // The unfinished file transfers kept there by an earlier session (3.4)
    if (fileDirectoryChanged) {
        loadAngel61Partials();
    }
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

bool DSDDemodSink::angel61CallPlayed() const
{
    // The group list (ANGEL61.md 2.6): a call to a talkgroup not listed is shown, not played.
    // CQ, a callsign, and everything when nothing is listed, are played
    const DSDAngel61Session *session = m_demodulator.getAngel61Session();
    return !session || !session->hasRouting() || DVAngel61::talkgroupPlayed(session->getRouting().m_destination, m_angel61Groups);
}

void DSDDemodSink::configureMyPosition(float myLatitude, float myLongitude)
{
    QMutexLocker locker(&m_positionMutex);   // from the channel's thread
    m_myLatitude = myLatitude;
    m_myLongitude = myLongitude;
    m_hasMyPosition = true;
}

DSDDemodSink::Report DSDDemodSink::getReport() const
{
    QMutexLocker locker(&m_reportMutex);
    return m_report;
}

QVector<QJsonObject> DSDDemodSink::takeContactReports()
{
    QMutexLocker locker(&m_contactMutex);
    QVector<QJsonObject> reports;
    reports.swap(m_contactReports);
    return reports;
}

void DSDDemodSink::updateReport()
{
    // Angel61: every session's traffic, and the report of the one that holds the channel (the
    // selected profile's, or in Auto a transmission's); the FM protocols' otherwise
    DSDAngel61Session *holding = m_demodulator.getAngel61Holding();
    updateAngel61FileDecision();

    for (DSDAngel61Session *session : m_demodulator.getAngel61Sessions()) {
        updateAngel61(*session, session == holding);
    }

    persistAngel61Partials();

    // The channel's state, for the DSE Modulators' channel access (3.5): a transmission heard - an
    // Angel61 one, or an FM protocol's lock - and, when it ends, whose it was and to whom
    bool busy = m_demodulator.getStatus().m_locked;

    for (DSDAngel61Session *session : m_demodulator.getAngel61Sessions()) {
        busy = busy || session->isActive();
    }

    if (busy != m_angel61ChannelBusy)
    {
        m_angel61ChannelBusy = busy;
        sendAngel61ChannelState(busy);
        m_angel61EndedSource = 0;
        m_angel61EndedDestination = 0;
    }

    if (holding) {
        return;
    }

    const DSDDemodulator::Status& status = m_demodulator.getStatus();
    Report report;
    report.m_locked = status.m_locked;
    report.m_protocol = QString::fromStdString(status.m_protocol);
    report.m_syncType = QString::fromStdString(status.m_lastPattern);
    report.m_inverted = status.m_inverted;
    report.m_deviationHz = status.m_outerLevelHz;
    report.m_frequencyErrorHz = status.m_correctionHz + status.m_offsetHz;
    report.m_clockPpm = status.m_clockPpm;
    report.m_snrDB = status.m_snrDB;
    report.m_scopeSampleRate = m_demodulator.getScopeSampleRate();
    report.m_syncCount = status.m_syncCount;
    report.m_slot1Voice = (m_streams[0].m_lastVoiceSample >= 0) && (m_sampleCount - m_streams[0].m_lastVoiceSample < 24000);
    report.m_slot2Voice = (m_streams[1].m_lastVoiceSample >= 0) && (m_sampleCount - m_streams[1].m_lastVoiceSample < 24000);
    updateDMRCallEvents(report.m_slot1Voice, report.m_slot2Voice);
    updateDStarMap();
    updateDataText();
    report.m_dtmf = m_lastDTMF;
    report.m_angel61FilePending = m_angel61FilePending;
    report.m_lastMessage = m_lastMessage;
    report.m_lastMessageTime = m_lastMessageTime;
    report.m_angel61IPRxPackets = m_angel61IPRxPackets;
    report.m_angel61IPRxBytes = m_angel61IPRxBytes;
    report.m_angel61HeardCallsigns = m_angel61HeardCallsigns;
    report.m_events = m_events;
    report.m_messages = m_messages;

    updateBerEstimate(status.m_locked, report.m_slot1Voice || report.m_slot2Voice);

    if (!status.m_locked)
    {
        m_noVocoderText.clear();
        m_statusText.clear();
    }
    else if (statusTextDue())
    {
        m_statusText = formatStatusText();
    }

    report.m_berEstimate = m_berEstimate;
    reportVoiceCounters(report);
    report.m_statusText = m_statusText;

    if (status.m_locked && !m_noVocoderText.isEmpty() && (report.m_slot1Voice || report.m_slot2Voice)) {
        report.m_statusText += " | " + m_noVocoderText;
    }

    QMutexLocker locker(&m_reportMutex);
    m_report = report;
}

void DSDDemodSink::updateBerEstimate(bool locked, bool voice)
{
    // Raw BER from the frames' codes (processFrame), smoothed over the reports
    int64_t corrected = 0, protectedBits = 0;

    for (Stream& s : m_streams)
    {
        corrected += s.m_correctedBits;
        protectedBits += s.m_protectedBits;
        s.m_correctedBits = 0;
        s.m_protectedBits = 0;
    }

    if (protectedBits > 0) {
        m_berEstimate += 0.2f * ((float) corrected / protectedBits - m_berEstimate);
    } else if (!locked || !voice) {
        m_berEstimate = 0.0f; // no voice to measure on (unlocked, a control channel, a call that ended)
    }
}

bool DSDDemodSink::statusTextDue()
{
    // The report is made every block (10 ms) and read far less often: the text is made at 10 Hz
    if (!m_statusText.isEmpty() && (m_sampleCount - m_statusTextSample < 4800)) {
        return false;
    }

    m_statusTextSample = m_sampleCount;
    return true;
}

void DSDDemodSink::updateDMRCallEvents(bool slot1Voice, bool slot2Voice)
{
    const DSDProtocolDMR *dmr = dynamic_cast<const DSDProtocolDMR*>(m_demodulator.getDecoder());
    const bool voice[2] = {slot1Voice, slot2Voice};

    if (dmr != m_dmrMetadataDecoder)
    {
        m_dmrMetadataDecoder = dmr;

        for (int slot = 0; slot < 2; slot++)
        {
            m_dmrAliasRevision[slot] = 0;
            m_dmrGPSRevision[slot] = 0;
            m_dmrGPSMapRevision[slot] = 0;
            m_dmrTextRevision[slot] = 0;
            m_lastDMRGPSEventSample[slot] = m_sampleCount - 48000;
        }
    }

    for (int slot = 0; slot < 2; slot++)
    {
        DMRCallState& call = m_dmrCalls[slot];
        DSDProtocolDMR::LinkControl linkControl;

        if (dmr) {
            linkControl = dmr->getLinkControl(slot);
        }

        if (voice[slot] && dmr)
        {
            int colorCode = dmr->getColorCode();

            if (!call.m_active)
            {
                call.m_active = true;
                call.m_startSample = m_sampleCount;
                call.m_colorCode = colorCode;
                call.m_linkControl = linkControl;
                addDMRCallEvent(slot, "START", call);
            }
            else
            {
                if (colorCode >= 0) {
                    call.m_colorCode = colorCode;
                }

                if (linkControl.m_valid)
                {
                    bool addressChanged = call.m_linkControl.m_valid
                        && ((linkControl.m_source != call.m_linkControl.m_source)
                            || (linkControl.m_destination != call.m_linkControl.m_destination)
                            || (linkControl.m_group != call.m_linkControl.m_group));
                    bool detailsChanged = !call.m_linkControl.m_valid
                        || (linkControl.m_emergency != call.m_linkControl.m_emergency)
                        || (linkControl.m_privacy != call.m_linkControl.m_privacy)
                        || (linkControl.m_priority != call.m_linkControl.m_priority);

                    if (addressChanged)
                    {
                        addDMRCallEvent(slot, "END", call, (m_sampleCount - call.m_startSample) / 48000.0f);
                        call.m_startSample = m_sampleCount;
                        call.m_linkControl = linkControl;
                        addDMRCallEvent(slot, "START", call);
                    }
                    else if (detailsChanged)
                    {
                        call.m_linkControl = linkControl;
                        addDMRCallEvent(slot, "UPDATE", call);
                    }
                    else
                    {
                        call.m_linkControl = linkControl;
                    }
                }
            }
        }
        else if (call.m_active)
        {
            int64_t lastVoiceSample = std::max(call.m_startSample, m_streams[slot].m_lastVoiceSample);
            addDMRCallEvent(slot, "END", call, (lastVoiceSample - call.m_startSample) / 48000.0f + 0.02f);
            call = DMRCallState();
        }

        if (dmr)
        {
            DSDProtocolDMR::TalkerAlias alias = dmr->getTalkerAlias(slot);

            if (alias.m_valid && (alias.m_revision > m_dmrAliasRevision[slot]))
            {
                static const char *const formats[] = {"7BIT", "ISO-8859-1", "UTF-8", "UTF-16BE"};
                const QString escaped = escapeEventText(alias.m_text);
                QString source = linkControl.m_valid ? QString::number(linkControl.m_source) : QString("--");
                addEvent(QString("DMR S%1 CC:%2 EVT:ALIAS SRC:%3 FORMAT:%4 TEXT:\"%5\"")
                    .arg(slot + 1)
                    .arg(dmr->getColorCode() < 0 ? QString("--") : QString::number(dmr->getColorCode()))
                    .arg(source)
                    .arg(formats[alias.m_format & 3])
                    .arg(escaped));
                m_dmrAliasRevision[slot] = alias.m_revision;
            }

            DSDProtocolDMR::GPSInfo gps = dmr->getGPSInfo(slot);

            if (gps.m_valid && (gps.m_revision > m_dmrGPSRevision[slot])
                && (m_sampleCount - m_lastDMRGPSEventSample[slot] >= 48000))
            {
                static const char *const errors[] = {"<2m", "<20m", "<200m", "<2km", "<20km", "<=200km", ">200km", "UNKNOWN"};
                QString source = linkControl.m_valid ? QString::number(linkControl.m_source) : QString("--");
                addEvent(QString("DMR S%1 CC:%2 EVT:GPS SRC:%3 LAT:%4 LON:%5 ERR:%6")
                    .arg(slot + 1)
                    .arg(dmr->getColorCode() < 0 ? QString("--") : QString::number(dmr->getColorCode()))
                    .arg(source)
                    .arg(gps.m_latitude, 0, 'f', 6)
                    .arg(gps.m_longitude, 0, 'f', 6)
                    .arg(errors[gps.m_positionError & 7]));
                m_dmrGPSRevision[slot] = gps.m_revision;
                m_lastDMRGPSEventSample[slot] = m_sampleCount;
            }

            if (gps.m_valid && (gps.m_revision > m_dmrGPSMapRevision[slot]))
            {
                static const char *const errors[] = {"<2m", "<20m", "<200m", "<2km", "<20km", "<=200km", ">200km", "UNKNOWN"};
                const QString name = linkControl.m_valid
                    ? QString("DMR %1").arg(linkControl.m_source)
                    : QString("DMR S%1").arg(slot + 1);
                const QString text = QString("%1\nSlot: %2\nColour code: %3\nAccuracy: %4\nLast heard: %5")
                    .arg(name)
                    .arg(slot + 1)
                    .arg(dmr->getColorCode() < 0 ? QString("Unknown") : QString::number(dmr->getColorCode()))
                    .arg(errors[gps.m_positionError & 7])
                    .arg(QDateTime::currentDateTime().toString("dd MMM yyyy HH:mm:ss"));

                if (sendPositionToMap(name, name, text, gps.m_latitude, gps.m_longitude)) {
                    m_dmrGPSMapRevision[slot] = gps.m_revision;
                }
            }

            DSDProtocolDMR::TextMessage message = dmr->getTextMessage(slot);

            if (message.m_valid && (message.m_revision > m_dmrTextRevision[slot]))
            {
                QString format = message.m_format == 17 ? QString("TMS/IP") : message.m_format == 16 ? QString("UTF-16BE/IP")
                    : message.m_format == 7 ? QString("UTF-16BE")
                    : message.m_format == 4 ? QString("ISO-8859-1") : QString("7BIT");
                const QString escaped = escapeEventText(message.m_text);
                addEvent(QString("DMR S%1 CC:%2 EVT:TEXT SRC:%3 DST:%4 TYPE:%5 FORMAT:%6 TEXT:\"%7\"")
                    .arg(slot + 1)
                    .arg(dmr->getColorCode() < 0 ? QString("--") : QString::number(dmr->getColorCode()))
                    .arg(message.m_source)
                    .arg(message.m_destination)
                    .arg(message.m_group ? QString("GROUP") : QString("UNIT"))
                    .arg(format)
                    .arg(escaped));
                addTextMessage("DMR", QString::number(message.m_source), (message.m_group ? "TG " : "") + QString::number(message.m_destination), QString::fromUtf8(message.m_text.c_str()));
                m_dmrTextRevision[slot] = message.m_revision;
            }
        }
    }
}

void DSDDemodSink::updateDStarMap()
{
    const DSDProtocolDStar *dstar = dynamic_cast<const DSDProtocolDStar*>(m_demodulator.getDecoder());

    if (dstar != m_dstarMetadataDecoder)
    {
        m_dstarMetadataDecoder = dstar;
        m_dstarMapPositionValid = false;
        m_lastDStarMapName.clear();
        m_lastDStarText.clear();
        m_lastDPRSRevision = 0;
    }

    if (!dstar) {
        return;
    }

    // D-PRS: the APRS packet of the slow data, as it was received, to the APRS feature
    if (dstar->getDPRSRevision() != m_lastDPRSRevision)
    {
        m_lastDPRSRevision = dstar->getDPRSRevision();
        forwardAPRS(QString::fromUtf8(dstar->getDPRS().c_str()));
    }

    // The 20 character slow data text, once per transmission (it repeats every few seconds)
    QString slowDataText = QString::fromUtf8(dstar->getText().c_str()).trimmed();

    if (!slowDataText.isEmpty() && (slowDataText != m_lastDStarText))
    {
        m_lastDStarText = slowDataText;
        addTextMessage("D-STAR", QString::fromStdString(dstar->getMyCall()).trimmed(), QString::fromStdString(dstar->getUrCall()).trimmed(), slowDataText);
    }

    double latitude;
    double longitude;

    if (!dstar->getPosition(latitude, longitude)) {
        return;
    }

    QString callsign = dstar->isHeaderValid()
        ? QString::fromStdString(dstar->getMyCall()).trimmed() : QString();
    QString name = callsign.isEmpty() ? QString("D-STAR") : QString("D-STAR %1").arg(callsign);

    if (m_dstarMapPositionValid && (latitude == m_lastDStarMapLatitude)
        && (longitude == m_lastDStarMapLongitude) && (name == m_lastDStarMapName))
    {
        return;
    }

    QString label = callsign.isEmpty() ? QString("D-STAR") : callsign;
    QString text = QString("%1\nLatitude: %2\nLongitude: %3\nLast heard: %4")
        .arg(name)
        .arg(latitude, 0, 'f', 6)
        .arg(longitude, 0, 'f', 6)
        .arg(QDateTime::currentDateTime().toString("dd MMM yyyy HH:mm:ss"));
    if (sendPositionToMap(name, label, text, latitude, longitude))
    {
        m_dstarMapPositionValid = true;
        m_lastDStarMapLatitude = latitude;
        m_lastDStarMapLongitude = longitude;
        m_lastDStarMapName = name;
    }
}

void DSDDemodSink::forwardAPRS(const QString& tnc2)
{
    // The packet is repeated every few seconds while the transmission lasts: the same text again
    // inside 30 seconds is a duplicate, as it is for an APRS-IS gateway
    QDateTime now = QDateTime::currentDateTime();

    if (tnc2.isEmpty() || !m_channel) {
        return;
    }

    if ((tnc2 == m_lastAPRSPacket) && m_lastAPRSTime.isValid() && (m_lastAPRSTime.secsTo(now) < 30)) {
        return;
    }

    m_lastAPRSPacket = tnc2;
    m_lastAPRSTime = now;
    QByteArray packet = APRSPacket::toByteArray(tnc2); // TNC2 text to an AX.25 frame, as the consumers expect

    if (packet.isEmpty()) {
        return;
    }

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_channel, "packets", pipes);

    for (const auto& pipe : pipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);

        if (messageQueue) {
            messageQueue->push(MainCore::MsgPacket::create(m_channel, packet, now));
        }
    }

    addEvent(QString("DSTAR EVT:APRS %1").arg(tnc2));
    qDebug("DSDDemodSink::forwardAPRS: %s to %d consumer(s)", qPrintable(tnc2), (int) pipes.size());
}

template <class Session>
void DSDDemodSink::updateAngel61File(Session& session, bool ended)
{
    // A file transfer (3.4): the offer, the progress, the file itself when it is complete, and
    // the answer this station owes the transmitter when the transmission ends
    const DVAngel61FileReceiver& receiver = session.getFileReceiver();
    // In 32 bits the kilobytes overflow before they are compared with anything: a setting of
    // 4 194 304 kB or more wrapped to a small limit, or to none at all
    const uint64_t maxBytes = (uint64_t) std::max(0, m_settings.m_angel61FileMaxKB) * 1024ULL;
    session.setMaxFileSize(m_settings.m_angel61FileMaxKB > 0
        ? (uint32_t) std::min<uint64_t>(maxBytes, 0xFFFFFFFFULL) : 0);
    session.setFileConfirm(m_settings.m_angel61FileConfirm);
    session.setFilePartials(&m_angel61Partials);

    if (receiver.hasOffer() && (receiver.getOffer().m_crc != m_angel61FileCrc))
    {
        m_angel61FileCrc = receiver.getOffer().m_crc;
        addEvent(QString("ANGEL61 EVT:FILE OFFER NAME:\"%1\" SIZE:%2 BLOCKS:%3%4%5")
            .arg(QString::fromStdString(DVAngel61FileReceiver::sanitiseName(receiver.getOffer().m_name)))
            .arg(receiver.getOffer().m_fileSize)
            .arg(receiver.getBlocksExpected())
            .arg(receiver.getBlocksResumed() > 0 ? QString(" RESUMED:%1").arg(receiver.getBlocksResumed()) : QString())
            .arg(receiver.isAwaitingConfirmation() ? " AWAITING CONFIRMATION" : ""));
    }

    uint64_t source = 0;
    std::string name;
    std::vector<uint8_t> data;

    if (session.takeCompletedFile(source, name, data))
    {
        QString path = writeAngel61File(QString::fromStdString(name), data);
        addEvent(QString("ANGEL61 EVT:FILE %1 SRC:%2 NAME:\"%3\" SIZE:%4")
            .arg(path.isEmpty() ? "NOT SAVED" : "SAVED")
            .arg(QString::fromStdString(DVAngel61::decodeCallsign(source)))
            .arg(path.isEmpty() ? QString::fromStdString(name) : path)
            .arg((int) data.size()));
    }

    if (receiver.getState() == DVAngel61FileReceiver::Rejected) {
        m_angel61FileRejected = true;
    }

    // The answer goes out through a DSE Modulator of this instance, in its own transmission; an
    // unfinished file is kept for a resume (3.4)
    if (ended)
    {
        session.keepFilePartial();
        DVAngel61FileControl control;

        if (session.takeFileControl(control)) {
            sendAngel61FileControl(control, session.hasRouting() ? session.getRouting().m_source : 0, true);
        }
    }

    for (const DVAngel61FileControl& control : session.takeHeardFileControls()) {
        sendAngel61FileControl(control, 0, false); // heard from another station: for our own transfer
    }
}

QString DSDDemodSink::writeAngel61File(const QString& name, const std::vector<uint8_t>& data)
{
    QString directory = m_settings.m_angel61FileDirectory;

    if (directory.trimmed().isEmpty()) {
        return QString(); // no directory configured: the file is decoded but not stored
    }

    QDir dir(directory);

    if (!dir.exists() && !dir.mkpath(".")) {
        qWarning("DSDDemodSink::writeAngel61File: cannot make %s", qPrintable(directory));
        return QString();
    }

    // The name came off the air, so it may well be one that is already here. Nothing received
    // is allowed to replace a file that exists: a suffix is added until the name is free
    QFileInfo info(dir.filePath(name));
    QString path = info.filePath();

    for (int i = 1; QFileInfo::exists(path); i++)
    {
        if (i > 1000)
        {
            qWarning("DSDDemodSink::writeAngel61File: no free name for %s", qPrintable(name));
            return QString();
        }

        QString suffix = info.completeSuffix();
        path = dir.filePath(info.baseName() + QString(" (%1)").arg(i) + (suffix.isEmpty() ? "" : "." + suffix));
    }

    // QSaveFile so that a failure part way leaves nothing behind rather than a truncated file,
    // and the write is checked: a full disk used to be reported as a saved file
    QSaveFile file(path);

    if (!file.open(QIODevice::WriteOnly))
    {
        qWarning("DSDDemodSink::writeAngel61File: cannot write %s", qPrintable(path));
        return QString();
    }

    qint64 written = file.write((const char *) data.data(), (qint64) data.size());

    if ((written != (qint64) data.size()) || !file.commit())
    {
        qWarning("DSDDemodSink::writeAngel61File: %s: wrote %lld of %d bytes: %s",
            qPrintable(path), (long long) written, (int) data.size(), qPrintable(file.errorString()));
        file.cancelWriting();
        return QString();
    }

    qDebug("DSDDemodSink::writeAngel61File: %d bytes to %s", (int) data.size(), qPrintable(path));
    return path;
}

template <class Session>
void DSDDemodSink::updateAngel61Tunnel(Session& session)
{
    // IP tunnelling (3.5): a datagram off the air goes to a DSE Modulator of this instance, which
    // is the side that owns the tunnel, on the pipe that already carries the rate requests and
    // the file transfer answers. It has been checked in the session; the modulator checks it
    // again against its own peer and prefixes before the host's stack ever sees it
    for (const auto& datagram : session.takeDatagrams())
    {
        m_angel61IPRxPackets++;
        m_angel61IPRxBytes += quint64(datagram.second.size());
        const QString hex = toHex(datagram.second);

        sendAngel61Packet(QString("ANGEL61IP %1 %2")
            .arg(QString::fromStdString(DVAngel61::decodeCallsign(datagram.first))).arg(hex));
        addEvent(QString("ANGEL61 EVT:IP SRC:%1 BYTES:%2")
            .arg(QString::fromStdString(DVAngel61::decodeCallsign(datagram.first)))
            .arg((int) datagram.second.size()));
    }

    for (const auto& control : session.takeTunnelControls())
    {
        const QString hex = toHex(control.second.encode());

        sendAngel61Packet(QString("ANGEL61IPC %1 %2")
            .arg(QString::fromStdString(DVAngel61::decodeCallsign(control.first))).arg(hex));
        addEvent(QString("ANGEL61 EVT:IPCONTROL SRC:%1 %2")
            .arg(QString::fromStdString(DVAngel61::decodeCallsign(control.first)))
            .arg(QString::fromStdString(control.second.toString())));
    }

    // Header compression (3.6): what the decompressor for a station wants that station's
    // compressor to hear. The modulator sends it when the station is its tunnel's peer
    for (const auto& control : session.takeCompressionControls())
    {
        const QString hex = toHex(control.second.encode());

        sendAngel61Packet(QString("ANGEL61IPQ %1 %2")
            .arg(QString::fromStdString(DVAngel61::decodeCallsign(control.first))).arg(hex));
        addEvent(QString("ANGEL61 EVT:IPCONTEXT SRC:%1 %2")
            .arg(QString::fromStdString(DVAngel61::decodeCallsign(control.first)))
            .arg(QString::fromStdString(control.second.toString())));
    }
}

void DSDDemodSink::sendAngel61Packet(const QString& text)
{
    if (!m_channel) {
        return;
    }

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_channel, "angel61", pipes);
    QByteArray packet = text.toUtf8();

    for (const auto& pipe : pipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);

        if (messageQueue) {
            messageQueue->push(MainCore::MsgPacket::create(m_channel, packet));
        }
    }
}

void DSDDemodSink::sendAngel61ChannelState(bool busy)
{
    if (!m_channel) {
        return;
    }

    // "ANGEL61CH 1 -- --" when a transmission is heard, "ANGEL61CH 0 source destination" when it ends
    // ("--" for a transmission whose routing was not heard: an FM protocol's, or an Angel61 one's lost)
    const QString source = m_angel61EndedSource ? QString::fromStdString(DVAngel61::decodeCallsign(m_angel61EndedSource)) : QString("--");
    const QString destination = m_angel61EndedDestination ? QString::fromStdString(DVAngel61::decodeCallsign(m_angel61EndedDestination)) : QString("--");
    const QByteArray packet = (busy ? QString("ANGEL61CH 1 -- --") : QString("ANGEL61CH 0 %1 %2").arg(source.trimmed()).arg(destination.trimmed())).toUtf8();
    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_channel, "angel61", pipes);

    for (const auto& pipe : pipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);

        if (messageQueue) {
            messageQueue->push(MainCore::MsgPacket::create(m_channel, packet));
        }
    }
}

void DSDDemodSink::updateAngel61FileDecision()
{
    // The operator's decision on a file awaiting it (3.4), taken by every session holding one: a
    // session between transmissions sends its answer at once, one in a transmission at its end
    const int decision = m_angel61FileDecision.exchange(0);
    m_angel61FilePending.clear();

    for (DSDAngel61Session *session : m_demodulator.getAngel61Sessions())
    {
        const DVAngel61FileReceiver& receiver = session->getFileReceiver();

        if (decision && receiver.isAwaitingConfirmation())
        {
            const uint64_t source = receiver.getSource();
            addEvent(QString("ANGEL61 EVT:FILE %1 NAME:\"%2\"").arg(decision == 1 ? "ACCEPTED" : "REFUSED")
                .arg(QString::fromStdString(DVAngel61FileReceiver::sanitiseName(receiver.getOffer().m_name))));
            session->decideFile(decision == 1);
            DVAngel61FileControl control;

            if (!session->isActive() && session->takeFileControl(control)) {
                sendAngel61FileControl(control, source, true);
            }
        }

        if (receiver.isAwaitingConfirmation() && m_angel61FilePending.isEmpty())
        {
            m_angel61FilePending = QString("%1 (%2 bytes) from %3")
                .arg(QString::fromStdString(DVAngel61FileReceiver::sanitiseName(receiver.getOffer().m_name)))
                .arg(receiver.getOffer().m_fileSize)
                .arg(QString::fromStdString(DVAngel61::decodeCallsign(receiver.getSource())));
        }
    }
}

QString DSDDemodSink::angel61PartialDirectory() const
{
    return m_settings.m_angel61FileDirectory.isEmpty() ? QString()
        : QDir(m_settings.m_angel61FileDirectory).filePath(".angel61-partial");
}

void DSDDemodSink::loadAngel61Partials()
{
    // The unfinished transfers of an earlier session (3.4), a day at most: those of another age or
    // that are not ours are left alone, the expired ones removed
    const QString directory = angel61PartialDirectory();

    if (directory.isEmpty()) {
        return;
    }

    const int64_t now = QDateTime::currentSecsSinceEpoch();
    const qint64 largest = (qint64) DVAngel61FileOffer::MaxBlocks * DVAngel61FileOffer::MaxBlockSize + 4096;
    int loaded = 0;

    for (const QFileInfo& info : QDir(directory).entryInfoList(QStringList("*.a61p"), QDir::Files))
    {
        QFile file(info.filePath());

        if ((info.size() > largest) || !file.open(QIODevice::ReadOnly)) {
            continue;
        }

        const QByteArray bytes = file.readAll();
        file.close();
        DVAngel61FilePartial partial;

        if (!DVAngel61FilePartial::decode(std::vector<uint8_t>(bytes.begin(), bytes.end()), partial)
            || (info.completeBaseName() != QString::fromStdString(partial.key()))) {
            continue;
        }

        if (now - partial.m_savedSec > DVAngel61FilePartials::MaxAgeSec)
        {
            QFile::remove(info.filePath());
            continue;
        }

        m_angel61Partials.load(partial);
        loaded++;
    }

    qDebug("DSDDemodSink::loadAngel61Partials: %d unfinished transfer(s) from %s", loaded, qPrintable(directory));
}

void DSDDemodSink::persistAngel61Partials()
{
    m_angel61Partials.expire(QDateTime::currentSecsSinceEpoch());
    const std::vector<std::string> changed = m_angel61Partials.takeChanged();
    const std::vector<std::string> removed = m_angel61Partials.takeRemoved();
    const QString directory = angel61PartialDirectory();

    // (no directory: kept for this session alone)
    if (directory.isEmpty() || (changed.empty() && removed.empty())) {
        return;
    }

    QDir dir(directory);

    if (!dir.exists() && !QDir().mkpath(directory))
    {
        qWarning("DSDDemodSink::persistAngel61Partials: cannot create %s", qPrintable(directory));
        return;
    }

    for (const std::string& key : removed) {
        QFile::remove(dir.filePath(QString::fromStdString(key) + ".a61p"));
    }

    for (const std::string& key : changed)
    {
        const DVAngel61FilePartial *partial = m_angel61Partials.get(key);

        if (!partial) {
            continue;
        }

        const std::vector<uint8_t> bytes = partial->encode();
        QSaveFile file(dir.filePath(QString::fromStdString(key) + ".a61p"));

        if (file.open(QIODevice::WriteOnly) && (file.write((const char *) bytes.data(), (qint64) bytes.size()) == (qint64) bytes.size())) {
            file.commit();
        } else {
            qWarning("DSDDemodSink::persistAngel61Partials: cannot write %s", qPrintable(file.fileName()));
        }
    }
}

void DSDDemodSink::sendAngel61FileControl(const DVAngel61FileControl& control, uint64_t destination, bool owed)
{
    if (!m_channel) {
        return;
    }

    // On the same pipe the rate request records use: "ANGEL61FILEQ destination hex", the answer this
    // station owes the station sending it a file (a DSE Modulator transmits it); "ANGEL61FILE -- hex",
    // one heard from a receiving station (a DSE Modulator acts on it if the transfer is its own)
    std::vector<uint8_t> bytes = control.encode();
    const QString hex = toHex(bytes);

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_channel, "angel61", pipes);
    QByteArray packet = QString("%1 %2 %3")
        .arg(owed ? "ANGEL61FILEQ" : "ANGEL61FILE")
        .arg(destination ? QString::fromStdString(DVAngel61::decodeCallsign(destination)) : QString("--"))
        .arg(hex).toUtf8();

    for (const auto& pipe : pipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);

        if (messageQueue) {
            messageQueue->push(MainCore::MsgPacket::create(m_channel, packet));
        }
    }

    addEvent(QString("ANGEL61 EVT:FILE CONTROL %1 KIND:%2 ID:%3").arg(owed ? "OWED" : "HEARD").arg(control.m_kind).arg(control.m_transferId));
}

void DSDDemodSink::updateAngel61(DSDAngel61Session& hf, bool reportIt)
{
    // The profile's name: in the events, the rate records and on the network (the gateway's listing,
    // contacts)
    const DVAngel61::Profile sessionProfile = hf.getProfile();
    const QString profile = sessionProfile == DVAngel61::ProfileHFWB ? "HF-WB" : sessionProfile == DVAngel61::ProfileHFNB ? "HF-NB"
        : sessionProfile == DVAngel61::ProfileWB ? "WB" : "NB";
    const QString& networkProfile = profile;

    if (hf.isActive() && hf.hasRouting()) {
        addAngel61HeardCallsign(hf.getRouting().m_source);
    }

    // The gateway (plan C7)
    m_angel61Gateway.update(hf, [this](const DVFrameEvent& event) { publishDVFrameEvent(event); });

    // The transmissions that ended, as they were: the session may already hold the next one
    hf.takeEnds();
    const std::vector<DSDAngel61Session::Ended> endedTransmissions = hf.takeEnded();
    const int ends = (int) endedTransmissions.size();

    for (const DSDAngel61Session::Ended& ended : endedTransmissions)
    {
        m_angel61EndedSource = ended.m_hasRouting ? ended.m_routing.m_source : 0;
        m_angel61EndedDestination = ended.m_hasRouting ? ended.m_routing.m_destination : 0;
        const QString endSource = ended.m_hasRouting ? QString::fromStdString(DVAngel61::decodeCallsign(ended.m_routing.m_source)) : QString("--");
        const QString endDestination = ended.m_hasRouting ? QString::fromStdString(DVAngel61::decodeCallsign(ended.m_routing.m_destination)) : QString("--");
        addEvent(QString("ANGEL61 EVT:END SRC:%1 DST:%2 FRAMES:%3 LOST:%4 SNR:%5 VOICE:%6 PROFILE:%7")
            .arg(endSource)
            .arg(endDestination)
            .arg(ended.m_framesExpected)
            .arg(ended.m_framesLost)
            .arg(ended.m_meanSnrDB, 0, 'f', 1)
            .arg(ended.m_voiceFrames)
            .arg(profile));

        // The contact, for the gateway's server: a station heard with voice or text
        const QString source = ended.m_hasRouting ? endSource.trimmed().toUpper() : QString();
        const bool text = ended.m_textMessages > 0;
        if (m_settings.m_contactReportEnabled && ended.m_hasRouting && ((ended.m_voiceFrames > 0) || text)
            && !source.isEmpty() && (source != "--") && (source != "CQ")
            && !DVAngel61::isGroup(ended.m_routing.m_source))
        {
            // (its times from the transmission's own samples: the report is made at its end)
            const qint64 endedMs = QDateTime::currentMSecsSinceEpoch();
            const qint64 startedMs = endedMs - (ended.m_endSample - ended.m_beginSample) / (DSDAngel61Session::SampleRate / 1000);
            QJsonObject contact{{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
                {"observedUtcMs", double(startedMs)},
                {"endedUtcMs", double(endedMs)},
                {"txCallsign", source},
                {"destination", endDestination},
                {"frequencyHz", double(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset)},
                {"media", ended.m_voiceFrames > 0 ? (text ? "audio+text" : "audio") : "text"},
                {"profile", networkProfile}, {"framesDecoded", ended.m_framesExpected - ended.m_framesLost},
                {"framesLost", ended.m_framesLost}, {"relayStatus", "unknown"}};
            if (ended.m_framesExpected > 0) contact["snrDb"] = DVAngel61::isHF(sessionProfile) ? ended.m_meanSnrDB : ended.m_minSnrDB;
            if (ended.m_hasPosition) {
                contact["txLat"] = ended.m_position.m_latitude;
                contact["txLon"] = ended.m_position.m_longitude;
            }
            QMutexLocker locker(&m_contactMutex);
            if (m_contactReports.size() >= 64) m_contactReports.remove(0);
            m_contactReports.append(contact);
        }
    }

    // (after the ends: one station right after another ends, then begins)
    for (int i = hf.takeBegins(); i > 0; i--) {
        addEvent("ANGEL61 EVT:START PROFILE:" + profile);
    }

    for (const std::string& packet : hf.takeAPRSPackets()) {
        forwardAPRS(QString::fromStdString(packet));
    }

    // Files and IP tunnelling (3.4 to 3.6)
    updateAngel61File(hf, ends > 0);
    updateAngel61Tunnel(hf);

    // DTMF (3.8): the keys of the transmission that holds the channel, made into tones
    for (const DSDAngel61Session::DTMFKey& key : hf.takeDTMFKeys())
    {
        if (reportIt && (m_streams[0].m_dtmfPending.size() < 64)) {
            m_streams[0].m_dtmfPending.push_back(key);
        }
    }

    for (const DSDAngel61Session::Message& message : hf.takeMessages())
    {
        if (message.m_dtmf)
        {
            const QString src = QString::fromStdString(DVAngel61::decodeCallsign(message.m_source));
            const QString dst = QString::fromStdString(DVAngel61::decodeCallsign(message.m_destination));
            addEvent(QString("ANGEL61 EVT:DTMF SRC:%1 DST:%2 KEYS:%3").arg(src).arg(dst).arg(QString::fromStdString(message.m_text)));
            addTextMessage("Angel61 " + profile, src, dst, "DTMF " + QString::fromStdString(message.m_text), true);
            m_lastDTMF = QString::fromStdString(message.m_text);
            continue;
        }

        const QString escaped = escapeEventText(message.m_text);
        addEvent(QString("ANGEL61 EVT:TEXT SRC:%1 DST:%2 VIA:%3 TEXT:\"%4\"")
            .arg(QString::fromStdString(DVAngel61::decodeCallsign(message.m_source)))
            .arg(QString::fromStdString(DVAngel61::decodeCallsign(message.m_destination)))
            .arg(message.m_fromSlots ? "SLOTS" : (message.m_contentType == DVAngel61::ContentText ? "DATA" : "BINARY"))
            .arg(escaped));
        addTextMessage("Angel61 " + profile, QString::fromStdString(DVAngel61::decodeCallsign(message.m_source)),
            QString::fromStdString(DVAngel61::decodeCallsign(message.m_destination)),
            message.m_contentType == DVAngel61::ContentBinary ? QString("[%1 bytes of binary data]").arg(message.m_text.size()) : QString::fromUtf8(message.m_text.c_str()));
    }

    for (const DSDAngel61Session::PositionReport& report : hf.takePositions())
    {
        if (report.m_source == DVAngel61::CallsignNone) {
            continue;
        }

        const QString source = QString::fromStdString(DVAngel61::decodeCallsign(report.m_source));
        const DVAngel61::Position& p = report.m_position;
        addEvent(QString("ANGEL61 EVT:POSITION SRC:%1 LAT:%2 LON:%3").arg(source).arg(p.m_latitude, 0, 'f', 5).arg(p.m_longitude, 0, 'f', 5));
        const QString text = QString("%1\nAngel61 %2\nAltitude: %3\nLast heard: %4")
            .arg(source)
            .arg(profile)
            .arg(p.m_altitude == DVAngel61::Position::AltitudeUnknown ? QString("Unknown") : QString("%1 m").arg(p.m_altitude))
            .arg(QDateTime::currentDateTime().toString("dd MMM yyyy HH:mm:ss"));
        sendPositionToMap(QString("Angel61 %1").arg(source), source, text, p.m_latitude, p.m_longitude);
    }

    // The rate request records (2.10), as the VHF receivers', with the HF profile
    std::vector<DSDAngel61Session::Correspondent> updates = hf.takeCorrespondentUpdates();

    if (!updates.empty() && m_channel)
    {
        QList<ObjectPipe*> pipes;
        MainCore::instance()->getMessagePipes().getMessagePipes(m_channel, "angel61", pipes);

        for (const DSDAngel61Session::Correspondent& c : updates)
        {
            const QString src = QString::fromStdString(DVAngel61::decodeCallsign(c.m_callsign));
            const QString dst = QString::fromStdString(DVAngel61::decodeCallsign(c.m_destination));
            addEvent(QString("ANGEL61 EVT:RATE SRC:%1 DST:%2 REQ:%3 SEND:%4 PROFILE:%5 BLER:%6 SNR:%7")
                .arg(src).arg(dst).arg(c.m_requestedTier).arg(c.m_sendTier).arg(profile)
                .arg(100.0f * c.m_blockErrorRate, 0, 'f', 1).arg(c.m_meanSnrDB, 0, 'f', 1));

            for (const auto& pipe : pipes)
            {
                MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);

                if (messageQueue)
                {
                    QByteArray packet = QString("ANGEL61 %1 %2 %3 %4 %5").arg(src).arg(dst).arg(c.m_requestedTier).arg(c.m_sendTier).arg(profile).toUtf8();
                    messageQueue->push(MainCore::MsgPacket::create(m_channel, packet));
                }
            }
        }
    }

    if (!reportIt) {
        return;
    }

    Report report;
    report.m_locked = hf.isLocked();
    report.m_protocol = hf.isActive() ? "Angel61 " + profile : QString();
    report.m_syncType = "Angel61 " + profile;
    report.m_frequencyErrorHz = hf.getFrequencyHz();
    report.m_snrDB = hf.getMeanSnrDB();
    // The constellation's points a second, its payload cells' (left at the default 48000, the
    // GUI sized a redraw at 14400 points of a 300 ms trace: four seconds of NB's)
    report.m_scopeSampleRate = m_demodulator.getScopeSampleRate();
    report.m_slot1Voice = hf.isActive() && (hf.getVoiceFrames() > 0);
    report.m_angel61IPRxPackets = m_angel61IPRxPackets;
    report.m_angel61IPRxBytes = m_angel61IPRxBytes;
    report.m_angel61HeardCallsigns = m_angel61HeardCallsigns;
    report.m_events = m_events;
    report.m_messages = m_messages;
    report.m_dtmf = m_lastDTMF;
    report.m_angel61FilePending = m_angel61FilePending;
    report.m_lastMessage = m_lastMessage;
    report.m_lastMessageTime = m_lastMessageTime;
    updateBerEstimate(report.m_locked, report.m_slot1Voice);
    report.m_berEstimate = m_berEstimate;
    reportVoiceCounters(report);

    if (!hf.isActive() && !hf.hasRouting())
    {
        m_statusText.clear();
    }
    else if (statusTextDue())
    {
        m_statusText = QString::fromStdString(hf.getStatusText());
        float myLatitude, myLongitude;

        if (hf.hasPosition() && getMyPosition(myLatitude, myLongitude))
        {
            const DVAngel61::Position& position = hf.getPosition();
            QGeoCoordinate mine(myLatitude, myLongitude);
            QGeoCoordinate theirs(position.m_latitude, position.m_longitude);
            m_statusText += QString(" | %1:%2/%3km").arg(Maidenhead::toMaidenhead((float) position.m_latitude, (float) position.m_longitude))
                .arg((int) std::lround(mine.azimuthTo(theirs))).arg(mine.distanceTo(theirs) / 1000.0, 0, 'f', 1);
        }
    }

    report.m_statusText = m_statusText;

    QMutexLocker locker(&m_reportMutex);
    m_report = report;
}

void DSDDemodSink::addAngel61HeardCallsign(uint64_t encodedCallsign)
{
    if ((encodedCallsign == DVAngel61::CallsignNone) || DVAngel61::isGroup(encodedCallsign)) {
        return;
    }

    const QString callsign = QString::fromStdString(DVAngel61::decodeCallsign(encodedCallsign)).trimmed().toUpper();

    if (callsign.isEmpty() || (callsign == "--") || (callsign == "CQ")) {
        return;
    }

    m_angel61HeardCallsigns.removeAll(callsign);
    m_angel61HeardCallsigns.prepend(callsign);

    static constexpr int MaxHeardCallsigns = 32;
    while (m_angel61HeardCallsigns.size() > MaxHeardCallsigns) {
        m_angel61HeardCallsigns.removeLast();
    }
}

void DSDDemodSink::updateDataText()
{
    // NXDN short/data calls and P25 packet data text messages: emitted once per revision, like DMR's
    // The decoder of the lock, or of the last one: dPMR and M17 finish a message as the lock ends.
    // A decoder keeps its last message from one lock to the next, so what was seen of it is kept too
    if (const DSDProtocol *locked = m_demodulator.getDecoder()) {
        m_dataTextDecoder = locked;
    }

    const DSDProtocol *decoder = m_dataTextDecoder;

    if (!decoder) {
        return;
    }

    DataRevisions& revisions = m_dataRevisions[decoder];

    if (const DSDProtocolNXDN *nxdn = dynamic_cast<const DSDProtocolNXDN*>(decoder))
    {
        const DSDProtocolNXDN::TextMessage& t = nxdn->getTextMessage();

        if (t.m_valid && (t.m_revision > revisions.m_text))
        {
            revisions.m_text = t.m_revision;
            const QString escaped = escapeEventText(t.m_text);
            addEvent(QString("NXDN EVT:%1 SRC:%2 DST:%3 TEXT:\"%4\"")
                .arg(t.m_isText ? "TEXT" : "DATA").arg(t.m_source).arg(t.m_destination).arg(escaped));
            addTextMessage("NXDN", QString::number(t.m_source), (t.m_group ? "TG " : "") + QString::number(t.m_destination), QString::fromUtf8(t.m_text.c_str()));
        }
    }
    else if (const DSDProtocolYSF *ysf = dynamic_cast<const DSDProtocolYSF*>(decoder))
    {
        // Data full rate messages (Wires-X, text messages, pictures), and the GPS position of the
        // V/D type 2 frames
        const DSDProtocolYSF::DataMessage& d = ysf->getDataMessage();

        if (d.m_valid && (d.m_revision > revisions.m_text))
        {
            revisions.m_text = d.m_revision;
            const QString escaped = escapeEventText(d.m_text);
            addEvent(QString("YSF EVT:%1 SRC:%2 DST:%3 KIND:\"%4\" TEXT:\"%5\"%6")
                .arg(d.m_isText ? "TEXT" : "DATA")
                .arg(QString::fromStdString(d.m_source)).arg(QString::fromStdString(d.m_destination))
                .arg(QString::fromStdString(d.m_kind)).arg(escaped)
                .arg(d.m_complete ? QString() : QString(" (cut short)")));

            if (d.m_isText) {
                addTextMessage("YSF", QString::fromStdString(d.m_source), QString::fromStdString(d.m_destination), QString::fromUtf8(d.m_text.c_str()));
            }
        }

        const DSDProtocolYSF::Position& p = ysf->getPosition();

        if (p.m_valid && (p.m_revision > revisions.m_position))
        {
            revisions.m_position = p.m_revision;
            const QString source = QString::fromStdString(p.m_source);
            const char *radio = DSDProtocolYSF::radioName(p.m_radio);
            addEvent(QString("YSF EVT:GPS SRC:%1 LAT:%2 LON:%3%4")
                .arg(source).arg(p.m_latitude, 0, 'f', 5).arg(p.m_longitude, 0, 'f', 5)
                .arg(radio ? QString(" RADIO:%1").arg(radio) : QString()));
            const QString name = QString("YSF %1").arg(source);
            const QString text = QString("%1\n%2Speed: %3 kn\nCourse: %4\nLast heard: %5")
                .arg(name)
                .arg(radio ? QString("Radio: %1\n").arg(radio) : QString())
                .arg(p.m_speedKnots < 0 ? QString("-") : QString::number(p.m_speedKnots))
                .arg(p.m_course < 0 ? QString("-") : QString::number(p.m_course))
                .arg(QDateTime::currentDateTime().toString("dd MMM yyyy HH:mm:ss"));
            sendPositionToMap(name, source, text, p.m_latitude, p.m_longitude);
        }
    }
    else if (const DSDProtocolDPMR *dpmr = dynamic_cast<const DSDProtocolDPMR*>(decoder))
    {
        // dPMR slow data messages, sent during a call
        const DSDProtocolDPMR::TextMessage& t = dpmr->getTextMessage();

        if (t.m_valid && (t.m_revision > revisions.m_text))
        {
            revisions.m_text = t.m_revision;
            const QString escaped = escapeEventText(t.m_text);
            addEvent(QString("dPMR EVT:%1 SRC:%2 DST:%3 TEXT:\"%4\"%5")
                .arg(t.m_isText ? "TEXT" : "DATA").arg(t.m_source).arg(t.m_destination).arg(escaped)
                .arg(t.m_complete ? QString() : QString(" (cut short)")));
            addTextMessage("dPMR", QString::number(t.m_source), QString::number(t.m_destination), QString::fromUtf8(t.m_text.c_str()));
        }
    }
    else if (const DSDProtocolM17 *m17 = dynamic_cast<const DSDProtocolM17*>(decoder))
    {
        // M17 packet transmissions: an SMS message, and APRS packets to the APRS feature
        const DSDProtocolM17::TextMessage& t = m17->getTextMessage();

        if (t.m_valid && (t.m_revision > revisions.m_text))
        {
            revisions.m_text = t.m_revision;
            const QString escaped = escapeEventText(t.m_text);
            addEvent(QString("M17 EVT:TEXT SRC:%1 DST:%2 TEXT:\"%3\"")
                .arg(QString::fromStdString(t.m_source)).arg(QString::fromStdString(t.m_destination)).arg(escaped));
            addTextMessage("M17", QString::fromStdString(t.m_source), QString::fromStdString(t.m_destination), QString::fromUtf8(t.m_text.c_str()));
        }

        std::string packet;

        while (const_cast<DSDProtocolM17*>(m17)->takeAPRSPacket(packet)) {
            forwardAPRS(QString::fromStdString(packet));
        }
    }
    else if (const DSDProtocolP25 *p25 = dynamic_cast<const DSDProtocolP25*>(decoder))
    {
        const DSDProtocolP25::TextMessage& t = p25->getTextMessage();

        if (t.m_valid && (t.m_revision > revisions.m_text))
        {
            revisions.m_text = t.m_revision;
            const QString escaped = escapeEventText(t.m_text);
            addEvent(QString("P25 EVT:TEXT SRC:%1 DST:%2 FORMAT:%3 TEXT:\"%4\"")
                .arg(t.m_source).arg(t.m_destination).arg(QString::fromStdString(t.m_kind)).arg(escaped));
            addTextMessage("P25", QString::number(t.m_source), QString::number(t.m_destination), QString::fromUtf8(t.m_text.c_str()));
        }
    }
}

bool DSDDemodSink::sendPositionToMap(const QString& name, const QString& label, const QString& text,
    double latitude, double longitude)
{
    if (!m_channel) {
        return false;
    }

    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_channel, "mapitems", mapPipes);
    bool sent = false;

    for (const auto& pipe : mapPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);

        if (!messageQueue) {
            continue;
        }

        SWGSDRangel::SWGMapItem *mapItem = new SWGSDRangel::SWGMapItem();
        mapItem->setName(new QString(name));
        mapItem->setLatitude((float) latitude);
        mapItem->setLongitude((float) longitude);
        mapItem->setAltitude(0.0f);
        mapItem->setAltitudeReference(1); // CLAMP_TO_GROUND
        mapItem->setFixedPosition(false);
        mapItem->setPositionDateTime(new QString(QDateTime::currentDateTime().toString(Qt::ISODateWithMs)));
        mapItem->setImageRotation(0);
        mapItem->setText(new QString(text));
        mapItem->setImage(new QString("antenna.png"));
        mapItem->setModel(new QString("antenna.glb"));
        mapItem->setModelAltitudeOffset(0.0f);
        mapItem->setLabel(new QString(label));
        mapItem->setLabelAltitudeOffset(4.5f);
        mapItem->setOrientation(0);
        mapItem->setHeading(0.0f);
        messageQueue->push(MainCore::MsgMapItem::create(m_channel, mapItem));
        sent = true;
    }

    return sent;
}

void DSDDemodSink::addDMRCallEvent(int slot, const QString& action, const DMRCallState& call, float durationSeconds)
{
    const DSDProtocolDMR::LinkControl& lc = call.m_linkControl;
    QString text = QString("DMR S%1 CC:%2 EVT:%3 SRC:%4 DST:%5 TYPE:%6 EMG:%7 PRIV:%8 PRI:%9")
        .arg(slot + 1)
        .arg(call.m_colorCode < 0 ? QString("--") : QString::number(call.m_colorCode))
        .arg(action)
        .arg(lc.m_valid ? QString::number(lc.m_source) : QString("--"))
        .arg(lc.m_valid ? QString::number(lc.m_destination) : QString("--"))
        .arg(lc.m_valid ? (lc.m_group ? QString("GROUP") : QString("UNIT")) : QString("--"))
        .arg(lc.m_valid && lc.m_emergency ? QString("Y") : QString("N"))
        .arg(lc.m_valid && lc.m_privacy ? QString("Y") : QString("N"))
        .arg(lc.m_valid ? QString::number(lc.m_priority) : QString("--"));

    if (durationSeconds >= 0.0f) {
        text += QString(" DUR:%1s").arg(durationSeconds, 0, 'f', 1);
    }

    addEvent(text);
}

// The voice counters since the channel started (the sweeps' frames lost and raw BER over an interval)
void DSDDemodSink::reportVoiceCounters(Report& report) const
{
    for (const Stream& s : m_streams)
    {
        report.m_voiceFrames += s.m_voiceFramesTotal;
        report.m_voiceFramesErased += s.m_voiceFramesErasedTotal;
        report.m_correctedBits += s.m_correctedBitsTotal;
        report.m_protectedBits += s.m_protectedBitsTotal;
    }
}

void DSDDemodSink::addTextMessage(const QString& protocol, const QString& source, const QString& destination, const QString& text, bool dtmf)
{
    m_messages.push_back(TextMessage{++m_messageSequence, protocol, source, destination, text});

    // The report's last message (DSDDemodReport.lastMessage), for Web API clients; DTMF has its own field
    if (!dtmf)
    {
        m_lastMessage = m_messages.back();
        m_lastMessageTime = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    }

    while (m_messages.size() > 32) {
        m_messages.removeFirst();
    }
}

void DSDDemodSink::addEvent(const QString& text)
{
    m_events.push_back(Event{++m_eventSequence, text});

    while (m_events.size() > 64) {
        m_events.removeFirst();
    }
}

QString DSDDemodSink::formatStatusText() const
{
    const DSDProtocol *decoder = m_demodulator.getDecoder();

    if (!decoder) {
        return QString();
    }

    if (dynamic_cast<const DSDProtocolDMR*>(decoder)) {
        return formatDMR();
    }

    if (dynamic_cast<const DSDProtocolDStar*>(decoder)) {
        return formatDStar();
    }

    return QString::fromStdString(decoder->getStatusText());
}

QString DSDDemodSink::formatDMR() const
{
    const DSDProtocolDMR *dmr = static_cast<const DSDProtocolDMR*>(m_demodulator.getDecoder());
    const char *station = "NA";

    switch (dmr->getStationType())
    {
    case DSDProtocolDMR::StationBase: station = "BS"; break;
    case DSDProtocolDMR::StationMobile: station = "MS"; break;
    case DSDProtocolDMR::StationDirect: station = "DM"; break;
    default: break;
    }

    QString text = QString("Sta: %1 CC: %2").arg(station).arg(dmr->getColorCode() < 0 ? QString("--") : QString::number(dmr->getColorCode()));

    for (int slot = 0; slot < 2; slot++)
    {
        DSDProtocolDMR::LinkControl linkControl = dmr->getLinkControl(slot);
        QString lc = linkControl.m_valid
            ? QString("%1 %2>%3").arg(linkControl.m_group ? "G" : "U").arg(linkControl.m_source).arg(linkControl.m_destination)
            : QString();
        text += QString(" S%1: %2 %3").arg(slot + 1).arg(dmr->getSlotTypeText(slot)).arg(lc);
    }

    return text;
}

QString DSDDemodSink::formatDStar() const
{
    const DSDProtocolDStar *dstar = static_cast<const DSDProtocolDStar*>(m_demodulator.getDecoder());
    QString text;

    if (dstar->isHeaderValid())
    {
        text = QString("%1/%2>%3|%4>%5")
            .arg(QString::fromStdString(dstar->getMyCall()).trimmed())
            .arg(QString::fromStdString(dstar->getMySuffix()).trimmed())
            .arg(QString::fromStdString(dstar->getUrCall()).trimmed())
            .arg(QString::fromStdString(dstar->getRpt1()).trimmed())
            .arg(QString::fromStdString(dstar->getRpt2()).trimmed());
    }
    else
    {
        text = "no header";
    }

    if (!dstar->getText().empty()) {
        text += "|" + QString::fromStdString(dstar->getText());
    }

    double latitude, longitude;

    if (dstar->getPosition(latitude, longitude))
    {
        text += "|" + Maidenhead::toMaidenhead((float) latitude, (float) longitude);

        float myLatitude, myLongitude;

        if (getMyPosition(myLatitude, myLongitude))
        {
            QGeoCoordinate mine(myLatitude, myLongitude);
            QGeoCoordinate theirs(latitude, longitude);
            text += QString(":%1/%2km").arg((int) std::lround(mine.azimuthTo(theirs))).arg(mine.distanceTo(theirs) / 1000.0, 0, 'f', 1);
        }
    }

    return text;
}
