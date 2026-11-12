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

#include <algorithm>
#include <cmath>

#include <QDateTime>
#include <QDebug>
#include <QTimer>

#include "dsemodangel61.h"
#include "dsemodvocodervocos.h"
#include "dsemodprotocol.h"
#include "dsemodysf.h"
#include "dsemodshaper.h"
#include "dsemodvocodernull.h"
#include "dsemodvocoderambefeature.h"
#include "dsemodvocodermbelib.h"
#include "dsemodvocodercodec2.h"
#include "dsemodvocoderopus.h"
#include "dsemodprocessor.h"
#include "dsemodsource.h"
#include "maincore.h"

MESSAGE_CLASS_DEFINITION(DSEModProcessor::MsgConfigure, Message)
MESSAGE_CLASS_DEFINITION(DSEModProcessor::MsgTransmit, Message)
MESSAGE_CLASS_DEFINITION(DSEModProcessor::MsgPushAudioFrame, Message)
MESSAGE_CLASS_DEFINITION(DSEModProcessor::MsgSendDTMF, Message)
MESSAGE_CLASS_DEFINITION(DSEModProcessor::MsgGenerate, Message)
MESSAGE_CLASS_DEFINITION(DSEModProcessor::MsgSetAMBEFeature, Message)
MESSAGE_CLASS_DEFINITION(DSEModProcessor::MsgSendMessage, Message)
MESSAGE_CLASS_DEFINITION(DSEModProcessor::MsgLinkState, Message)
MESSAGE_CLASS_DEFINITION(DSEModProcessor::MsgSendAPRS, Message)
MESSAGE_CLASS_DEFINITION(DSEModProcessor::MsgSendDatagram, Message)
MESSAGE_CLASS_DEFINITION(DSEModProcessor::MsgTunnelState, Message)
MESSAGE_CLASS_DEFINITION(DSEModProcessor::MsgTunnelControl, Message)
MESSAGE_CLASS_DEFINITION(DSEModProcessor::MsgSendFile, Message)
MESSAGE_CLASS_DEFINITION(DSEModProcessor::MsgFileControl, Message)
MESSAGE_CLASS_DEFINITION(DSEModProcessor::MsgFileNoAnswer, Message)

DSEModProcessor::DSEModProcessor() :
    m_fifo(m_fifoSize),
    m_vocoder(nullptr),
    m_protocol(nullptr),
    m_shaper(nullptr),
    m_fifoChannels(1),
    m_reportQueue(nullptr),
    m_ambeFeature(nullptr),
    m_linkRequestedTier(0),
    m_linkSendTier(0),
    m_linkTimestampMs(0),
    m_transmitting(false),
    m_stopping(false),
    m_vocoderReady(false),
    m_encodedFrameCount(0),
    m_underrunCount(0),
    m_overrunCount(0),
    m_trimCount(0),
    m_txFrameCount(0),
    m_peakDeviation(1200),
    m_depthMin(0),
    m_depthCount(0)
{
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    createProtocol();
    createVocoder();
    auto timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &DSEModProcessor::serviceAPI);
    timer->start(10);
}

DSEModProcessor::~DSEModProcessor()
{
    if (m_vocoder)
    {
        m_vocoder->close();
        delete m_vocoder;
    }

    m_angel61Data = nullptr;
    delete m_protocol;
    delete m_shaper;
}

void DSEModProcessor::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (MsgConfigure::match(*message))
        {
            MsgConfigure& cfg = (MsgConfigure&) *message;
            applySettings(cfg.getSettingsKeys(), cfg.getSettings(), cfg.getForce());
        }
        else if (MsgTransmit::match(*message))
        {
            MsgTransmit& msg = (MsgTransmit&) *message;

            if (m_api.active()) {
                if (!msg.getOn()) { finishAPI("aborted", "Stopped by local control"); }
            } else if (msg.getOn()) {
                startTransmit();
            } else {
                stopTransmit(false);
            }
        }
        else if (MsgPushAudioFrame::match(*message))
        {
            MsgPushAudioFrame& msg = (MsgPushAudioFrame&) *message;

            if (!m_api.active() && m_transmitting && m_protocol && m_protocol->isActive() && !m_protocol->isDataOnly())
            {
                const DSEModVocoder::Rate rate = m_protocol->getVocoderRate();
                bool vocoderReady = m_vocoder && m_vocoder->isReady();
                // Frames the encoder does not make go out one per encoded frame's time: every 20 ms,
                // every fourth 20 ms for an Opus packet (80 ms), one then two for V1's 13.33 ms
                const int64_t count = ++m_audioFrameCount;
                const int64_t perFrame = DSEModVocoder::getFrameSamples48k(rate);
                const int framesDue = (int) ((count * m_samplesPerAudioFrame) / perFrame - ((count - 1) * m_samplesPerAudioFrame) / perFrame);

                if (msg.hasTone() && vocoderReady && m_vocoder->encodeTone(msg.getSequence(), msg.getToneF1(), msg.getToneF2(), msg.getToneAmplitude())) {
                    // sent as a tone frame
                } else if ((m_settings.m_modAFInput == DSEModSettings::DSEModInputSilence)
                    || (m_settings.m_modAFInput == DSEModSettings::DSEModInputNone)) {
                    // No input selected is silence, and needs no vocoder to send it
                    for (int i = 0; i < framesDue; i++) {
                        frameSink(msg.getSequence(), DSEModVocoder::getSilenceFrame(rate));
                    }
                } else if (vocoderReady) {
                    const int inRate = msg.getSampleRate() > 0 ? msg.getSampleRate() : DSEModVocoder::getPcmSampleRate(rate);
                    m_vocoder->encode(msg.getSequence(), convertPCM(msg.getPCM().data(), inRate, DSEModVocoder::getPcmSampleRate(rate)));
                } else {
                    for (int i = 0; i < framesDue; i++) {
                        frameSink(msg.getSequence(), DSEModVocoder::getSilenceFrame(rate));
                    }
                }
            }

            generate();
        }
        else if (MsgGenerate::match(*message))
        {
            generate();
        }
        else if (MsgSendMessage::match(*message))
        {
            MsgSendMessage& msg = (MsgSendMessage&) *message;

            if (m_protocol && !m_protocol->queueMessage(msg.getText().toStdString())) {
                qDebug("DSEModProcessor::handleInputMessages: MsgSendMessage: not accepted by %s", m_protocol->getName());
            } else if (msg.getAutoTransmit() && m_angel61Data) {
                m_angel61Data->setDataOnlyNext(true);   // keyed up for it alone: it ends when it is out
            }
        }
        else if (MsgSendDTMF::match(*message))
        {
            // Angel61 DTMF (3.8): data in the DTMF slot, never through the voice codec
            MsgSendDTMF& msg = (MsgSendDTMF&) *message;
            DSEModAngel61Data *angel61 = m_angel61Data;

            if (!angel61 || !angel61->queueDTMF(msg.getKeys(), msg.getToneMs(), msg.getGapMs(), msg.getNewSequence(), msg.getHold(), msg.getOnly())) {
                qDebug("DSEModProcessor::handleInputMessages: MsgSendDTMF: %s not queued", msg.getKeys().c_str());
            }
        }
        else if (MsgSendAPRS::match(*message))
        {
            MsgSendAPRS& msg = (MsgSendAPRS&) *message;
            DSEModAngel61Data *angel61 = m_angel61Data;

            if (angel61) {
                angel61->setAPRS(msg.getInfo().toStdString());
            }
        }
        else if (MsgSendDatagram::match(*message))
        {
            // Angel61 IP tunnelling (3.5): one datagram, queued as a content type 6 message
            MsgSendDatagram& msg = (MsgSendDatagram&) *message;
            DSEModAngel61Data *angel61 = m_angel61Data;

            if (angel61)
            {
                const QByteArray& data = msg.getDatagram();
                std::vector<uint8_t> datagram(data.begin(), data.end());

                // (an empty one keys up for what the tunnel has queued: its control, its advertise)
                if ((datagram.empty() || angel61->queueDatagram(datagram)) && msg.getAutoTransmit()) {
                    angel61->setDataOnlyNext(false);   // keyed up for the tunnel alone: it ends when they are out
                }
            }
        }
        else if (MsgTunnelState::match(*message))
        {
            // What this station will route: the advertise it sends at the head of a transmission
            MsgTunnelState& msg = (MsgTunnelState&) *message;
            DSEModAngel61Data *angel61 = m_angel61Data;

            if (angel61)
            {
                const QByteArray& bytes = msg.getAdvertise();
                DVAngel61IP::Control advertise;
                DVAngel61IP::Control::decode(std::vector<uint8_t>(bytes.begin(), bytes.end()), advertise);
                angel61->setTunnel(msg.getEnabled(), advertise);
            }
        }
        else if (MsgTunnelControl::match(*message))
        {
            // Header compression (3.6): what this station's decompressor wants the peer's
            // compressor to hear goes on the air; what the peer said goes to this compressor
            MsgTunnelControl& msg = (MsgTunnelControl&) *message;
            DSEModAngel61Data *angel61 = m_angel61Data;
            const QByteArray& bytes = msg.getControl();
            DVAngel61IP::Control control;

            if (angel61 && DVAngel61IP::Control::decode(std::vector<uint8_t>(bytes.begin(), bytes.end()), control))
            {
                if (msg.getSend()) {
                    angel61->queueTunnelControl(control);
                } else {
                    angel61->handlePeerTunnelControl(control);
                }
            }
        }
        else if (MsgSendFile::match(*message))
        {
            // Angel61 file transfer (3.4): the offer and the blocks ride in data frames
            MsgSendFile& msg = (MsgSendFile&) *message;
            DSEModAngel61Data *angel61 = m_angel61Data;
            const QByteArray& data = msg.getData();

            if (angel61 && !data.isEmpty())
            {
                std::vector<uint8_t> bytes(data.begin(), data.end());

                if (angel61->startFile(msg.getName().toStdString(), bytes)) {
                    reportFileTransfer(*angel61);
                } else {
                    qDebug("DSEModProcessor::handleInputMessages: MsgSendFile: %s not accepted", qPrintable(msg.getName()));
                }
            }
            else
            {
                qDebug("DSEModProcessor::handleInputMessages: MsgSendFile: not an Angel61 transmission");
            }
        }
        else if (MsgFileControl::match(*message))
        {
            MsgFileControl& msg = (MsgFileControl&) *message;
            DSEModAngel61Data *angel61 = m_angel61Data;
            DVAngel61FileControl control;
            std::vector<uint8_t> bytes(msg.getControl().begin(), msg.getControl().end());

            if (angel61 && DVAngel61FileControl::decode(bytes, control))
            {
                if (msg.getSend())
                {
                    // Owed to the station sending this one a file: on the air, in its own
                    // transmission when the channel keyed up for it
                    if (angel61->queueFileControl(control, msg.getDestination().toStdString()) && msg.getAutoTransmit()) {
                        angel61->setDataOnlyNext(false);
                    }
                }
                else
                {
                    angel61->handleFileControl(control);
                    reportFileTransfer(*angel61);
                }
            }
        }
        else if (MsgFileNoAnswer::match(*message))
        {
            // Its report says whether a round is due: the channel keys up for it
            if (DSEModAngel61Data *angel61 = m_angel61Data)
            {
                angel61->fileNoAnswer();
                reportFileTransfer(*angel61);
            }
        }
        else if (MsgLinkState::match(*message))
        {
            MsgLinkState& msg = (MsgLinkState&) *message;
            m_linkRequestedTier = msg.getRequestedTier();
            m_linkSendTier = msg.getSendTier();
            m_linkTimestampMs = msg.getTimestampMs();
        }
        else if (MsgSetAMBEFeature::match(*message))
        {
            MsgSetAMBEFeature& msg = (MsgSetAMBEFeature&) *message;
            m_ambeFeature = msg.getFeature();
            m_ambePort = msg.getPort();
            qDebug("DSEModProcessor::handleInputMessages: MsgSetAMBEFeature: %p", (void *) m_ambeFeature);

            if (m_vocoder && (m_settings.m_vocoder == DSEModSettings::DSEModVocoderAMBEFeature))
            {
                DSEModVocoderAMBEFeature *vocoder = static_cast<DSEModVocoderAMBEFeature*>(m_vocoder);
                vocoder->setFeature(m_settings.m_connectAMBE ? m_ambeFeature : nullptr, m_ambePort);
            }

            updateVocoderReady();
        }
        else if (m_vocoder && m_vocoder->handleMessage(*message))
        {
            generate();
        }

        delete message;
    }
}

void DSEModProcessor::reportFileTransfer(const DSEModAngel61Data& protocol)
{
    if (!m_reportQueue) {
        return;
    }

    const DVAngel61FileSender& sender = protocol.getFileSender();
    m_reportQueue->push(DSEModSource::MsgReportFileTransfer::create(
        (int) sender.getState(), sender.getBlocksSent(), sender.getBlocksPending(),
        sender.getOffer().blocks(), sender.getRounds(), protocol.fileWantsTransmission(),
        sender.getOffer().m_blockSize));
}

void DSEModProcessor::applySettings(const QStringList& settingsKeys, const DSEModSettings& settings, bool force)
{
    if (m_api.active()) { return; } // session settings are latched
    // (YSF's voice mode is its protocol's: V/D type 2 or 1 and voice full rate are built differently,
    // and full rate needs the IMBE encoder)
    bool modeChanged = (settingsKeys.contains("mode") && (settings.m_mode != m_settings.m_mode))
        || (settingsKeys.contains("ysfMode") && (settings.m_ysfMode != m_settings.m_ysfMode) && (settings.m_mode == DSEModSettings::DSEModModeYSF))
        || force;
    bool vocoderChanged = (settingsKeys.contains("vocoder") && (settings.m_vocoder != m_settings.m_vocoder))
        || (settingsKeys.contains("codecModelDirectory") && (settings.m_codecModelDirectory != m_settings.m_codecModelDirectory)) || force;
    bool ambeChanged = (settingsKeys.contains("connectAMBE") && (settings.m_connectAMBE != m_settings.m_connectAMBE))
        || (settingsKeys.contains("vocoderGain") && (settings.m_vocoderGain != m_settings.m_vocoderGain))
        || (settingsKeys.contains("toneDetect") && (settings.m_toneDetect != m_settings.m_toneDetect))
        || (settingsKeys.contains("noiseSuppress") && (settings.m_noiseSuppress != m_settings.m_noiseSuppress));

    bool wasTransmitting = m_transmitting;

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    // Angel61's tier and addresses are the transmission's (ANGEL61.md 2.8: voice of one tier; the
    // preamble says who is calling whom): a rate, Src, Dst or Via that changes them while
    // transmitting ends the transmission with its last frame now, the speech queued for it dropped,
    // and starts another with them once that frame is out (generate). Not cut off as a mode change
    // is: a receiver would stay locked on the old transmission's frames, miss the new one's preamble
    // and count the misses as lost blocks. A relay's tier and addresses are the call's: they stay.
    // A data transmission keeps them too: it is short, and was keyed up for its data alone (a
    // message, a file, the tunnel, an answer), which another started in its place might not be
    const DSEModAngel61 *angel61 = dynamic_cast<const DSEModAngel61*>(m_protocol);
    const bool transmissionKey = settingsKeys.contains("angel61Rate") || settingsKeys.contains("angel61Source")
        || settingsKeys.contains("angel61Destination") || settingsKeys.contains("angel61Path");

    if (!modeChanged && wasTransmitting && !m_stopping && !m_restartPending && angel61 && !angel61->isRelaying() && !angel61->isDataOnly()
        && transmissionKey && angel61->transmissionChanges(m_settings))
    {
        m_restartPending = true;
        m_protocol->stop();
        generate();
    }

    if (modeChanged)
    {
        if (wasTransmitting) {
            stopTransmit(true);
        }

        int previousChannels = m_fifoChannels;
        createProtocol();

        // A change of the FIFO's format (FM samples or I,Q pairs): what the old protocol left in
        // it (its flush included) cannot be read as the new one's
        if (m_fifoChannels != previousChannels) {
            m_fifo.clear();
        }

        // The vocoder follows the protocol's family: a change between an MBE protocol and Angel61
        // (V1 or Opus) needs the other encoder, whatever the vocoder setting says
        vocoderChanged |= !vocoderMatchesRate();
    }

    if (vocoderChanged)
    {
        createVocoder();
    }
    else if (ambeChanged && m_vocoder && (m_settings.m_vocoder == DSEModSettings::DSEModVocoderAMBEFeature))
    {
        DSEModVocoderAMBEFeature *vocoder = static_cast<DSEModVocoderAMBEFeature*>(m_vocoder);
        vocoder->setFeature(m_settings.m_connectAMBE ? m_ambeFeature : nullptr, m_ambePort);
        vocoder->setGain(m_settings.m_vocoderGain);
    }
    else if (ambeChanged && m_vocoder && (m_settings.m_vocoder == DSEModSettings::DSEModVocoderMbelib) && DSEModVocoderMbelib::isAvailable())
    {
        DSEModVocoderMbelib *vocoder = static_cast<DSEModVocoderMbelib*>(m_vocoder);
        vocoder->setGain(m_settings.m_vocoderGain);
        vocoder->setToneDetect(m_settings.m_toneDetect);
        vocoder->setNoiseSuppress(m_settings.m_noiseSuppress);
    }

    updateVocoderReady();

    if (modeChanged && wasTransmitting && m_settings.m_transmit) {
        startTransmit();
    }
}

void DSEModProcessor::createProtocol()
{
    m_encoderSampleRate = 0;    // the source goes back to the mode's own input rate
    m_angel61Data = nullptr;
    delete m_protocol;
    delete m_shaper;
    m_protocol = nullptr;
    m_shaper = nullptr;

    if (m_settings.m_mode == DSEModSettings::DSEModModeYSF)
    {
        m_protocol = new DSEModYSF(m_settings.m_ysfMode == DSEModSettings::DSEModYSFModeVD1 ? DSEModYSF::DTVoiceData1
            : m_settings.m_ysfMode == DSEModSettings::DSEModYSFModeVW ? DSEModYSF::DTVoiceFullRate : DSEModYSF::DTVoiceData2);
    }
    else
    {
        m_protocol = DSEModProtocol::create(m_settings.m_mode);
    }
    m_peakDeviation = m_protocol->getPeakDeviation();

    if (DSEModAngel61 *angel61 = dynamic_cast<DSEModAngel61*>(m_protocol)) {
        m_angel61Data = angel61;
    }

    int samplesPerSymbol = DSEModShaper::m_sampleRate / m_protocol->getSymbolRate();

    switch (m_protocol->getShaping())
    {
    case DSEModProtocol::ShapingRRC4FSK:
        m_shaper = new DSEModRRC4FSKShaper(samplesPerSymbol, m_protocol->getShapingAlpha(), m_protocol->getShapingSpan());
        break;
    case DSEModProtocol::ShapingLinear:
        m_shaper = nullptr; // m_linearShaper
        break;
    case DSEModProtocol::ShapingOFDM:
        m_shaper = nullptr; // m_ofdm
        break;
    case DSEModProtocol::ShapingGMSK:
    default:
        m_shaper = new DSEModGMSKShaper(samplesPerSymbol);
        break;
    }

    m_fifoChannels = m_protocol->isIQ() ? 2 : 1;
    m_ofdm.reset(m_protocol->isOFDM() ? new DVAngel61OFDM::Modulator((DVAngel61::Profile) m_protocol->getOFDMProfile()) : nullptr);

    m_protocol->setFrameSource([this](uint8_t *frame) { return frameSource(frame); });
    qDebug("DSEModProcessor::createProtocol: %s", m_protocol->getName());
}

void DSEModProcessor::createVocoder()
{
    if (m_vocoder)
    {
        m_vocoder->close();
        delete m_vocoder;
        m_vocoder = nullptr;
    }

    if (m_protocol && (m_protocol->getVocoderRate() == DSEModVocoder::RateNone))
    {
        // A protocol with no voice: nothing to encode
        m_vocoder = new DSEModVocoderNull();
    }
    else if (m_protocol && DSEModVocoder::isVocos(m_protocol->getVocoderRate()))
    {
        // Angel61 revision 3: V1 (vocoscodec, ANGEL61.md 3.1.1), whatever the vocoder setting says
        DSEModVocoderVocos *vocoder = new DSEModVocoderVocos();
        vocoder->setModelDirectory(m_settings.m_codecModelDirectory.toStdString());
        vocoder->open(m_protocol->getVocoderRate());    // now rather than at the first key-up
        m_vocoder = vocoder;
    }
    else if (m_protocol && (m_protocol->getVocoderRate() == DSEModVocoder::RateCodec23200))
    {
        // M17: Codec2, whatever the vocoder setting says (it is for the MBE protocols)
        m_vocoder = new DSEModVocoderCodec2();
    }
    else if (m_protocol && DSEModVocoder::isOpus(m_protocol->getVocoderRate()))
    {
        // Angel61 WB audio: Opus (codec family O1, ANGEL61.md 3.1.2)
        m_vocoder = new DSEModVocoderOpus();
    }
    else switch (m_settings.m_vocoder)
    {
    case DSEModSettings::DSEModVocoderAMBEFeature:
    {
        DSEModVocoderAMBEFeature *vocoder = new DSEModVocoderAMBEFeature(&m_inputMessageQueue);
        vocoder->setFeature(m_settings.m_connectAMBE ? m_ambeFeature : nullptr, m_ambePort);
        vocoder->setGain(m_settings.m_vocoderGain);
        m_vocoder = vocoder;
        break;
    }
    case DSEModSettings::DSEModVocoderMbelib:
    default:
        if (DSEModVocoderMbelib::isAvailable())
        {
            DSEModVocoderMbelib *vocoder = new DSEModVocoderMbelib();
            vocoder->setGain(m_settings.m_vocoderGain);
            vocoder->setToneDetect(m_settings.m_toneDetect);
            vocoder->setNoiseSuppress(m_settings.m_noiseSuppress);
            m_vocoder = vocoder;
        }
        else
        {
            qWarning("DSEModProcessor::createVocoder: mbevocoder encoder not built in: using null vocoder");
            m_vocoder = new DSEModVocoderNull();
        }
        break;
    }

    m_vocoder->setFrameSink([this](uint32_t sequence, const uint8_t *frame) { frameSink(sequence, frame); });

    if (m_transmitting && m_protocol) {
        m_vocoder->open(m_protocol->getVocoderRate());
    }

    qDebug("DSEModProcessor::createVocoder: %s", m_vocoder->getName());
}

void DSEModProcessor::updateVocoderReady()
{
    // While transmitting: the encoder is open and working. Idle: it could be opened.
    m_vocoderReady = m_vocoder && (m_transmitting ? m_vocoder->isReady() : m_vocoder->canOpen());
}

//! The source's 20 ms of PCM at the rate it was fed to the encoder's rate (Angel61 WB: 48 kHz to
//! V1's 24 when its tier is not Opus); the same frame when the rates match
const int16_t *DSEModProcessor::convertPCM(const int16_t *pcm, int inRate, int outRate)
{
    if ((inRate == outRate) || (inRate <= 0) || (outRate <= 0)) {
        return pcm;
    }

    const int inSamples = inRate / 50;
    const int outSamples = outRate / 50;

    if (inRate < outRate)
    {
        // Not a case the source makes (it feeds the highest rate): linear interpolation keeps the time
        for (int i = 0; i < outSamples; i++)
        {
            float x = (float) i * inSamples / outSamples;
            int i0 = (int) x;
            int i1 = std::min(i0 + 1, inSamples - 1);
            m_pcmConverted[i] = (int16_t) std::lround(pcm[i0] + (x - i0) * (pcm[i1] - pcm[i0]));
        }

        return m_pcmConverted.data();
    }

    const int factor = inRate / outRate;

    if ((inRate != m_pcmInRate) || (outRate != m_pcmOutRate))
    {
        // 64 taps per output sample, cut off at 0.48 of the output rate, Kaiser window (beta 7):
        // for 48 to 16 kHz flat to 7 kHz, -1.6 dB at 7.5 kHz, 77 dB down from 8.5 kHz (and so for
        // 24 kHz at 1.5 times those)
        const int taps = 64 * factor + 1;
        const double pi = 3.14159265358979323846;
        const double cutoff = 0.48 / factor;
        const double beta = 7.0;
        auto besselI0 = [](double x) {
            double sum = 1.0, term = 1.0;
            for (int k = 1; k < 30; k++) {
                term *= (x / (2.0 * k)) * (x / (2.0 * k));
                sum += term;
            }
            return sum;
        };
        double sum = 0.0;
        m_pcmTaps.assign(taps, 0.0f);
        m_pcmHistory.assign(taps, 0.0f);

        for (int i = 0; i < taps; i++)
        {
            const int n = i - taps / 2;
            const double h = n ? std::sin(2.0 * pi * cutoff * n) / (pi * n) : 2.0 * cutoff;
            const double r = 2.0 * i / (taps - 1) - 1.0;
            m_pcmTaps[i] = (float) (h * besselI0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / besselI0(beta));
            sum += m_pcmTaps[i];
        }

        for (float& h : m_pcmTaps) {
            h = (float) (h / sum);
        }

        m_pcmHistoryPos = 0;
        m_pcmInRate = inRate;
        m_pcmOutRate = outRate;
    }

    const int taps = (int) m_pcmTaps.size();

    for (int o = 0; o < outSamples; o++)
    {
        for (int k = 0; k < factor; k++)
        {
            m_pcmHistory[m_pcmHistoryPos] = pcm[o * factor + k];
            m_pcmHistoryPos = (m_pcmHistoryPos + 1) % taps;
        }

        float out = 0.0f;

        for (int t = 0; t < taps; t++) {
            out += m_pcmTaps[t] * m_pcmHistory[(m_pcmHistoryPos + t) % taps];
        }

        m_pcmConverted[o] = (int16_t) std::max(-32768.0f, std::min(32767.0f, std::round(out)));
    }

    return m_pcmConverted.data();
}

//! The encoder makes the protocol's rate: V1 for V1, Opus for Opus, Codec2 for M17 and the
//! setting's MBE encoder otherwise
bool DSEModProcessor::vocoderMatchesRate() const
{
    return !m_protocol || !m_vocoder || m_vocoder->canEncode(m_protocol->getVocoderRate());
}

void DSEModProcessor::startTransmit()
{
    if (!m_protocol || !m_vocoder) {
        return;
    }

    if (m_transmitting)
    {
        qDebug("DSEModProcessor::startTransmit: already transmitting");
        return;
    }

    qDebug("DSEModProcessor::startTransmit: %s", m_protocol->getName());

    if (m_shaper) {
        m_shaper->reset();
    }

    m_linearShaper.reset();

    if (m_ofdm) {
        m_ofdm->reset();
    }

    updatePosition(); // the station's position for the protocols with a position slot (Angel61, D-STAR D-PRS)
    m_positionTimer.start();
    m_fileReportTimer.start();

    m_encodedFrames.clear();
    m_audioFrameCount = 0;
    m_pcmInRate = m_pcmOutRate = 0;  // the decimator starts afresh
    m_depthMin = 0;
    m_depthCount = 0;
    m_underrunCount = 0;
    m_overrunCount = 0;
    m_trimCount = 0;
    m_encodedFrameCount = 0;
    m_txFrameCount = 0;
    // The receiver side's report on the destination, unless it is over a minute old
    bool linkValid = (m_linkTimestampMs > 0) && (QDateTime::currentMSecsSinceEpoch() - m_linkTimestampMs < 60000);
    m_protocol->setLinkState(linkValid ? m_linkRequestedTier : 0, linkValid ? m_linkSendTier : 0);
    m_protocol->start(m_settings); // first: a protocol's vocoder rate can depend on the settings (Angel61)

    if (m_protocol->isLinear())
    {
        // The pulse of the protocol's profile, and the output level of the tier start() chose:
        // 16-QAM needs 3 dB more headroom for its peaks than QPSK (ANGEL61.md 2.1). After start(),
        // since an "auto" rate is only known then
        m_linearShaper.setSamplesPerSymbol(m_protocol->getLinearSamplesPerSymbol());
        m_linearShaper.setLevel(m_protocol->getLinearLevel());
    }

    // Angel61 WB changes codec family with its tier (V1 at 0 to 2, Opus at 3): the encoder
    // follows the rate this transmission chose
    if (!vocoderMatchesRate()) {
        createVocoder();
    }

    m_vocoder->open(m_protocol->getVocoderRate());
    m_encoderSampleRate = DSEModVocoder::getPcmSampleRate(m_protocol->getVocoderRate());
    updateVocoderReady();

    // Prime the encoded frame queue with silence so that vocoder latency and the FIFO read-ahead
    // do not cause underruns once real frames start arriving: the first generate() pass fills the
    // FIFO to the high water mark and a protocol block consumes up to getFramesPerBlock() frames
    // at once (P25 LDU: 9). The delay control in frameSink() trims any excess afterwards.
    const int samplesPerVoiceFrame = DSEModVocoder::getFrameSamples48k(m_protocol->getVocoderRate());   // 20 ms, 60 for an Opus packet, 13.33 for V1
    // A protocol whose preamble takes no voice needs less (getPrimeCredit); one whose preamble outlasts
    // the FIFO's lead (Angel61 revision 3 HF, 480 ms) would have the audio of its start queue up past the
    // slack, and the delay control would take it out of the first seconds of speech a frame at a time:
    // those first frames, the key-up's, are dropped as they arrive instead
    m_depthWindow = m_depthWindowSamples / samplesPerVoiceFrame;
    m_depthSlack = (m_depthSlackSamples + samplesPerVoiceFrame - 1) / samplesPerVoiceFrame;
    m_depthTrim = (m_depthSlackSamples + m_depthJitterSamples + samplesPerVoiceFrame - 1) / samplesPerVoiceFrame;
    int primeFrames = (m_fifoHighWater / samplesPerVoiceFrame) + m_protocol->getFramesPerBlock()
        + m_vocoder->getLatencyFrames() + m_depthSlack - m_protocol->getPrimeCredit();
    m_dropFirstFrames = std::max(0, -primeFrames);
    primeFrames = std::max(0, primeFrames);
    const uint8_t *silence = DSEModVocoder::getSilenceFrame(m_protocol->getVocoderRate());

    for (int i = 0; i < primeFrames; i++)
    {
        std::array<uint8_t, DSEModVocoder::m_maxFrameBytes> frame{};   // (V1's codebooks byte 0: all 16)
        std::copy(silence, silence + DSEModVocoder::getFrameBytes(m_protocol->getVocoderRate()), frame.begin());
        m_encodedFrames.push_back(frame);
    }

    m_stopping = false;
    m_restartPending = false;
    m_transmitting = true;
    generate();
}

void DSEModProcessor::stopTransmit(bool abort)
{
    if (!m_protocol || !m_transmitting) {
        return;
    }

    qDebug("DSEModProcessor::stopTransmit: abort: %d", abort);
    m_restartPending = false;   // released or cut off: no rate change's next transmission

    if (abort)
    {
        m_protocol->abort();
        m_samples.clear();

        if (m_shaper) {
            m_shaper->flush(m_samples);
        } else if (m_ofdm) {
            m_ofdmSamples.clear();
            m_ofdm->flush(m_ofdmSamples);
            for (const std::complex<float>& v : m_ofdmSamples)
            {
                m_samples.push_back(std::max(-1.0f, std::min(1.0f, v.real() * m_ofdmGain)));
                m_samples.push_back(std::max(-1.0f, std::min(1.0f, v.imag() * m_ofdmGain)));
            }
        } else {
            m_linearShaper.flush(m_samples);
        }

        writeSamples();
        m_transmitting = false;
        m_stopping = false;

        if (m_vocoder) {
            m_vocoder->endTransmission();
        }

        updateVocoderReady();
    }
    else
    {
        // The protocol runs ahead of the audio by the baseband lead: end the transmission once the
        // vocoder frames already received have been sent (frameSource), not before
        m_stopping = true;

        if (m_encodedFrames.empty()) {
            m_protocol->stop();
        }

        generate();
    }
}

void DSEModProcessor::updatePosition()
{
    if (!m_protocol || !MainCore::instance()) {
        return;
    }

    const MainSettings& mainSettings = MainCore::instance()->getSettings();
    double latitude = mainSettings.getLatitude();
    double longitude = mainSettings.getLongitude();
    // The position is a GNSS fix when something is updating it, and set by hand otherwise
    // (ANGEL61.md 3.2's source field). A hand set position does not move, so it has no velocity
    bool gnss = mainSettings.getAutoUpdatePosition();
    int speed = -1;
    int course = -1;

    if (gnss)
    {
        qint64 now = QDateTime::currentMSecsSinceEpoch();

        if (m_lastFixAt > 0)
        {
            int fixSpeed = -1;
            int fixCourse = -1;
            DSEModProtocol::velocity(m_lastFixLatitude, m_lastFixLongitude, latitude, longitude,
                (double) (now - m_lastFixAt) / 1000.0, fixSpeed, fixCourse);

            // Smoothed, because one bad fix otherwise throws a course right round; the course
            // follows the last fix that had one, so a moment of standstill does not lose it
            if (fixSpeed >= 0) {
                m_fixSpeed = m_fixSpeed < 0 ? (double) fixSpeed : 0.5 * m_fixSpeed + 0.5 * fixSpeed;
            }

            if (fixCourse >= 0) {
                m_fixCourse = fixCourse;
            }

            if (m_fixSpeed >= 0.0)
            {
                speed = (int) (m_fixSpeed + 0.5);
                course = (speed > 0) ? m_fixCourse : -1;
            }
        }

        m_lastFixLatitude = latitude;
        m_lastFixLongitude = longitude;
        m_lastFixAt = now;
    }
    else
    {
        m_fixSpeed = -1.0;
        m_fixCourse = -1;
        m_lastFixAt = 0;
    }

    m_protocol->setPosition(true, latitude, longitude, mainSettings.getAltitude(),
        speed, course, gnss ? 0 : 1);
}

void DSEModProcessor::generate()
{
    if (m_api.active()) { serviceAPI(); return; }
    if (!m_protocol || !m_transmitting) {
        return;
    }

    if (m_positionTimer.elapsed() > 5000) // a moving station: follow the GPS feature during the transmission
    {
        updatePosition();
        m_positionTimer.restart();
    }

    // A file transfer while it is running: the GUI used to hear about the first block and the
    // last and nothing in between, so its progress sat still and the rate it worked out from
    // two reports a minute apart was nonsense
    if (m_fileReportTimer.elapsed() > 500)
    {
        if (const DSEModAngel61Data *angel61 = m_angel61Data)
        {
            if (angel61->getFileSender().getState() != DVAngel61FileSender::Idle) {
                reportFileTransfer(*angel61);
            }
        }

        m_fileReportTimer.restart();
    }

    while (m_protocol->isActive() && (m_fifo.getFill() < m_fifoHighWater * m_fifoChannels))
    {
        m_samples.clear();

        if (m_protocol->isOFDM())
        {
            if (!ofdmFrame()) {
                break;
            }

            m_txFrameCount++;
        }
        else if (m_protocol->isLinear())
        {
            if (!m_protocol->nextSymbols(m_symbols)) {
                break;
            }

            m_linearShaper.shape(m_symbols.data(), (int) m_symbols.size(), m_samples);
            m_txFrameCount++;

            if (!m_protocol->isActive()) {
                m_linearShaper.flush(m_samples); // end of transmission: the whole filter tail
            }
        }
        else
        {
            if (!m_protocol->nextFrame(m_levels)) {
                break;
            }

            invertLevels(m_levels);
            m_shaper->shapeWithGaps(m_levels.data(), (int) m_levels.size(), m_samples);
            m_txFrameCount++;

            if (!m_protocol->isActive()) {
                m_shaper->flush(m_samples); // end of transmission: push out the filter tail
            }
        }

        writeSamples();
    }

    if (!m_protocol->isActive())
    {
        qDebug("DSEModProcessor::generate: end of transmission: frames: %u underruns: %u overruns: %u trims: %u",
            m_txFrameCount.load(), m_underrunCount.load(), m_overrunCount.load(), m_trimCount.load());
        m_transmitting = false;
        m_stopping = false;     // the release is done: not the next transmission's (an API one's)
        m_encodedFrames.clear();

        // A rate change's last frame is out: the next transmission, at the new tier, follows at
        // once, while the key is still held
        if (m_restartPending)
        {
            m_restartPending = false;

            if (m_settings.m_transmit)
            {
                startTransmit();
                return;
            }
        }

        if (m_vocoder) {
            m_vocoder->endTransmission();
        }

        updateVocoderReady();

        if (m_reportQueue) {
            m_reportQueue->push(DSEModSource::MsgReportTransmissionEnded::create());
        }

        // A file transfer that is waiting for the answer, or that is over: the channel decides
        // whether to key up again for another round
        if (const DSEModAngel61Data *angel61 = m_angel61Data)
        {
            if (angel61->getFileSender().getState() != DVAngel61FileSender::Idle) {
                reportFileTransfer(*angel61);
            }
        }
    }
}

bool DSEModProcessor::ofdmFrame()
{
    int constellation = 0;

    if (!m_ofdm || !m_protocol->nextCells(m_ofdmCells, constellation)) {
        return false;
    }

    // The reference transmitter (2.1): peaks at -1 dBFS for the frame's constellation, so that the RMS
    // is about -5.1 dBFS for QPSK frames and -6.4 dBFS for 16-QAM (2.11). The rare higher peak is
    // clipped here rather than in writeSamples, whose carrier-off marker lies below -1
    m_ofdmGain = DVAngel61OFDM::outputGain((DVAngel61::Constellation) constellation);
    m_ofdmSamples.clear();
    m_ofdm->modulateFrame(m_ofdmCells.data(), (DVAngel61::Constellation) constellation, m_ofdmSamples);

    if (!m_protocol->isActive()) {
        m_ofdm->flush(m_ofdmSamples);   // end of transmission: the last symbol's taper
    }

    m_samples.reserve(m_samples.size() + 2 * m_ofdmSamples.size());

    for (const std::complex<float>& v : m_ofdmSamples)
    {
        m_samples.push_back(std::max(-1.0f, std::min(1.0f, v.real() * m_ofdmGain)));
        m_samples.push_back(std::max(-1.0f, std::min(1.0f, v.imag() * m_ofdmGain)));
    }

    return true;
}

void DSEModProcessor::writeSamples()
{
    m_int16Samples.resize(m_samples.size());
    const bool fm = m_fifoChannels == 1;    // the frequency in peak deviations, else linear I,Q

    for (size_t i = 0; i < m_samples.size(); i++)
    {
        if (m_samples[i] <= DSEModShaper::m_sampleOff) // carrier off marker: never produced by the clamp below
        {
            m_int16Samples[i] = m_fifoOff;
            continue;
        }

        if (fm)
        {
            m_int16Samples[i] = DSEModShaper::fmToFifo(m_samples[i]);
            continue;
        }

        float v = std::max(-1.0f, std::min(1.0f, m_samples[i]));
        m_int16Samples[i] = (int16_t) (v * 32767.0f);
    }

    // Whole I,Q pairs only: a pair split by a full FIFO would misalign every pair after it
    uint32_t count = (uint32_t) m_int16Samples.size();
    uint32_t room = (uint32_t) std::max(0, (int) m_fifo.getSize() - 1 - m_fifo.getFill());
    count = std::min(count, room) & ~(uint32_t) (m_fifoChannels - 1);
    uint32_t written = m_fifo.write(m_int16Samples.data(), count);

    if (written != m_int16Samples.size()) {
        qDebug("DSEModProcessor::writeSamples: FIFO overflow: %u/%u written", written, (unsigned int) m_int16Samples.size());
    }
}

bool DSEModProcessor::frameSource(uint8_t *frame)
{
    if (m_api.active() && m_apiMessage) {
        const auto rate = m_protocol->getVocoderRate();
        std::fill(frame, frame + DSEModVocoder::m_maxFrameBytes, 0);   // (V1's codebooks byte: all 16)
        std::copy(DSEModVocoder::getSilenceFrame(rate), DSEModVocoder::getSilenceFrame(rate) + DSEModVocoder::getFrameBytes(rate), frame);
        return true;
    }
    if (m_encodedFrames.empty())
    {
        if (!m_stopping && (!m_api.active() || !m_apiInputEnded || !m_apiPending.empty())) {
            m_underrunCount++;
        }

        return false;
    }

    std::copy(m_encodedFrames.front().begin(), m_encodedFrames.front().end(), frame);
    m_encodedFrames.pop_front();

    if (m_stopping && m_encodedFrames.empty()) { // last frame of a released transmission
        m_protocol->stop();
    }

    return true;
}

void DSEModProcessor::frameSink(uint32_t sequence, const uint8_t *frame)
{
    if (m_api.active()) {
        if (std::find(m_apiPending.begin(), m_apiPending.end(), sequence) != m_apiPending.end()) {
            std::array<uint8_t, DSEModVocoder::m_maxFrameBytes> result{};
            std::copy(frame, frame + DSEModVocoder::getFrameBytes(m_protocol->getVocoderRate()), result.begin());
            m_apiResults[sequence].push_back(result);
        }
        return;
    }
    // API encodes have their own sequence space; a late reply cannot enter a
    // subsequent local transmission after an abort.
    if (sequence & 0x80000000U) { return; }

    if (!m_transmitting) {
        return;
    }

    if (m_dropFirstFrames > 0)
    {
        m_dropFirstFrames--;    // the key-up's audio a preamble longer than the FIFO's lead has no room for
        return;
    }

    // Delay control. The queue is primed so that in steady state its depth at frame arrival
    // oscillates between about one frame (after a generate() burst) and the FIFO lead. An
    // underrun (silence substituted, then the late frame arriving) or an audio clock faster
    // than the SDR clock raises that whole range: whenever the depth has stayed above the
    // slack and the jitter allowance (100 ms) for a full window, the oldest frame is dropped, which takes the delay back down
    // by that frame's time. An audio clock slower than the SDR clock lets the queue run out and silence
    // is substituted at consumption, so no action is needed on that side.
    size_t depth = m_encodedFrames.size();
    m_depthMin = (m_depthCount == 0) ? depth : std::min(m_depthMin, depth);

    if (++m_depthCount >= m_depthWindow)
    {
        if ((m_depthMin > (size_t) m_depthTrim) && !m_encodedFrames.empty())
        {
            m_encodedFrames.pop_front();
            m_trimCount++;
        }

        m_depthCount = 0;
    }

    std::array<uint8_t, DSEModVocoder::m_maxFrameBytes> f{};     // a V1 packet's codebooks byte 0: all 16
    std::copy(frame, frame + DSEModVocoder::getFrameBytes(m_protocol ? m_protocol->getVocoderRate() : DSEModVocoder::Rate3600x2450), f.begin());
    m_encodedFrames.push_back(f);
    m_encodedFrameCount++;

    while (m_encodedFrames.size() > (size_t) m_maxQueuedFrames) // hard bound
    {
        m_encodedFrames.pop_front();
        m_overrunCount++;
    }
}
