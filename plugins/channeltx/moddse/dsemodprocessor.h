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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODPROCESSOR_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODPROCESSOR_H_

#include <array>
#include <atomic>
#include <deque>
#include <vector>

#include <QElapsedTimer>
#include <QObject>

#include "util/message.h"
#include "util/messagequeue.h"

#include "dsemodsettings.h"
#include "dsemodfifo.h"
#include "dsp/dspcommands.h"
#include "dsemodvocoder.h"
#include "dsemodshaper.h"
#include "dsemodtxsession.h"
#include "dsemodpacketbuilder.h"
#include "dsemodgatewaymonitor.h"
#include "dsp/dv/dvopus.h"

class Feature;
class DSEModProtocol;
class DSEModShaper;
class DSEModLinearShaper;
#include "dsp/dv/dvangel61ofdm.h"
#include "dsemodangel61data.h"
#include <memory>
class DSEModSource;

/**
 * Runs on its own thread: encodes audio frames with the vocoder back end,
 * builds protocol frames and shapes them into 48 kS/s baseband written to the
 * FIFO read by the DSP thread.
 */
class DSEModProcessor : public QObject
{
    Q_OBJECT
public:
    class MsgConfigure : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const DSEModSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigure* create(const QStringList& settingsKeys, const DSEModSettings& settings, bool force) {
            return new MsgConfigure(settingsKeys, settings, force);
        }

    private:
        DSEModSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigure(const QStringList& settingsKeys, const DSEModSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    class MsgTransmit : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getOn() const { return m_on; }
        static MsgTransmit* create(bool on) { return new MsgTransmit(on); }

    private:
        bool m_on;
        MsgTransmit(bool on) : Message(), m_on(on) { }
    };

    class MsgPushAudioFrame : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        //! The PCM of the frame: 20 ms at getSampleRate() (160 at 8 kHz, 320 at 16 kHz, 960 at 48 kHz)
        std::array<int16_t, DSEModVocoder::m_maxPcmFrameSize>& getPCM() { return m_pcm; }
        const std::array<int16_t, DSEModVocoder::m_maxPcmFrameSize>& getPCM() const { return m_pcm; }
        uint32_t getSequence() const { return m_sequence; }
        //! The PCM's rate: 0 when it is the encoder's
        void setSampleRate(int sampleRate) { m_sampleRate = sampleRate; }
        int getSampleRate() const { return m_sampleRate; }
        //! The frame is a tone: send it as a tone frame if the vocoder can, else encode the PCM (which carries the same tone)
        void setTone(float f1, float f2, int amplitude) { m_toneF1 = f1; m_toneF2 = f2; m_toneAmplitude = amplitude; }
        bool hasTone() const { return m_toneF1 > 0.0f; }
        float getToneF1() const { return m_toneF1; }
        float getToneF2() const { return m_toneF2; }
        int getToneAmplitude() const { return m_toneAmplitude; }
        static MsgPushAudioFrame* create(uint32_t sequence) { return new MsgPushAudioFrame(sequence); }

    private:
        std::array<int16_t, DSEModVocoder::m_maxPcmFrameSize> m_pcm;
        uint32_t m_sequence;
        int m_sampleRate = 0;
        float m_toneF1;
        float m_toneF2;
        int m_toneAmplitude;
        MsgPushAudioFrame(uint32_t sequence) : Message(), m_sequence(sequence), m_toneF1(0.0f), m_toneF2(0.0f), m_toneAmplitude(0) { m_pcm.fill(0); }
    };

    class MsgGenerate : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgGenerate* create() { return new MsgGenerate(); }

    private:
        MsgGenerate() : Message() { }
    };

    //! A text message for the protocol's data channel (Angel61)
    //! An APRS information field (Angel61 3.3), sent as a data transmission of content type 2
    class MsgSendAPRS : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getInfo() const { return m_info; }
        static MsgSendAPRS* create(const QString& info) { return new MsgSendAPRS(info); }

    private:
        QString m_info;
        MsgSendAPRS(const QString& info) : Message(), m_info(info) { }
    };

    //! A file to offer over Angel61 (3.4)
    class MsgSendFile : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getName() const { return m_name; }
        const QByteArray& getData() const { return m_data; }
        static MsgSendFile* create(const QString& name, const QByteArray& data) { return new MsgSendFile(name, data); }

    private:
        QString m_name;
        QByteArray m_data;
        MsgSendFile(const QString& name, const QByteArray& data) : Message(), m_name(name), m_data(data) { }
    };

    //! The answer of the station receiving a file
    //! One IP datagram for the tunnel of 3.5
    class MsgSendDatagram : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QByteArray& getDatagram() const { return m_datagram; }
        static MsgSendDatagram* create(const QByteArray& datagram, bool autoTransmit = false) { return new MsgSendDatagram(datagram, autoTransmit); }
        bool getAutoTransmit() const { return m_autoTransmit; }   //!< The channel keys up for it alone

    private:
        QByteArray m_datagram;
        bool m_autoTransmit;
        MsgSendDatagram(const QByteArray& datagram, bool autoTransmit) : Message(), m_datagram(datagram), m_autoTransmit(autoTransmit) { }
    };

    //! Whether this station tunnels, and the advertise it sends (3.5)
    class MsgTunnelState : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getEnabled() const { return m_enabled; }
        const QByteArray& getAdvertise() const { return m_advertise; }
        static MsgTunnelState* create(bool enabled, const QByteArray& advertise) {
            return new MsgTunnelState(enabled, advertise);
        }

    private:
        bool m_enabled;
        QByteArray m_advertise;
        MsgTunnelState(bool enabled, const QByteArray& advertise) :
            Message(), m_enabled(enabled), m_advertise(advertise) { }
    };

    //! Tunnel control: to send (the decompressor's context state and acknowledgements), or heard from the peer for the header compressor (3.6)
    class MsgTunnelControl : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QByteArray& getControl() const { return m_control; }
        bool getSend() const { return m_send; }
        static MsgTunnelControl* create(const QByteArray& control, bool send) {
            return new MsgTunnelControl(control, send);
        }

    private:
        QByteArray m_control;
        bool m_send;
        MsgTunnelControl(const QByteArray& control, bool send) :
            Message(), m_control(control), m_send(send) { }
    };

    //! A file transfer's answer (3.4): heard from the receiving station, for this one's transfer; or,
    //! `send`, owed by this station to `destination`, which is sending it a file
    class MsgFileControl : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QByteArray& getControl() const { return m_control; }
        bool getSend() const { return m_send; }
        const QString& getDestination() const { return m_destination; }
        bool getAutoTransmit() const { return m_autoTransmit; }   //!< The channel keys up for it alone
        static MsgFileControl* create(const QByteArray& control, bool send = false, const QString& destination = QString(), bool autoTransmit = false) {
            return new MsgFileControl(control, send, destination, autoTransmit);
        }

    private:
        QByteArray m_control;
        bool m_send;
        QString m_destination;
        bool m_autoTransmit;
        MsgFileControl(const QByteArray& control, bool send, const QString& destination, bool autoTransmit) :
            Message(), m_control(control), m_send(send), m_destination(destination), m_autoTransmit(autoTransmit) { }
    };

    //! A file transfer's answer did not come in time (the channel's timer): the offer again, alone
    class MsgFileNoAnswer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgFileNoAnswer* create() { return new MsgFileNoAnswer(); }

    private:
        MsgFileNoAnswer() : Message() { }
    };

    class MsgSendMessage : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getText() const { return m_text; }
        bool getAutoTransmit() const { return m_autoTransmit; }   //!< The channel keys up for it alone
        static MsgSendMessage* create(const QString& text, bool autoTransmit = false) { return new MsgSendMessage(text, autoTransmit); }

    private:
        QString m_text;
        bool m_autoTransmit;
        MsgSendMessage(const QString& text, bool autoTransmit) : Message(), m_text(text), m_autoTransmit(autoTransmit) { }
    };

    //! Angel61 DTMF (3.8): keys for the DTMF slot (DSEModAngel61Data::queueDTMF)
    class MsgSendDTMF : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const std::string& getKeys() const { return m_keys; }
        int getToneMs() const { return m_toneMs; }
        int getGapMs() const { return m_gapMs; }
        bool getNewSequence() const { return m_newSequence; }
        bool getHold() const { return m_hold; }
        bool getOnly() const { return m_only; }
        static MsgSendDTMF* create(const std::string& keys, int toneMs, int gapMs, bool newSequence, bool hold, bool only) {
            return new MsgSendDTMF(keys, toneMs, gapMs, newSequence, hold, only);
        }

    private:
        std::string m_keys;
        int m_toneMs;
        int m_gapMs;
        bool m_newSequence;
        bool m_hold;
        bool m_only;
        MsgSendDTMF(const std::string& keys, int toneMs, int gapMs, bool newSequence, bool hold, bool only) :
            Message(), m_keys(keys), m_toneMs(toneMs), m_gapMs(gapMs), m_newSequence(newSequence), m_hold(hold), m_only(only) { }
    };

    //! The receiver side's report on the destination (Angel61 2.10), applied at the next start
    class MsgLinkState : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getRequestedTier() const { return m_requestedTier; }
        int getSendTier() const { return m_sendTier; }
        qint64 getTimestampMs() const { return m_timestampMs; }
        static MsgLinkState* create(int requestedTier, int sendTier, qint64 timestampMs) { return new MsgLinkState(requestedTier, sendTier, timestampMs); }

    private:
        int m_requestedTier;
        int m_sendTier;
        qint64 m_timestampMs;   //!< When the report was heard (ms since the epoch), 0 for none: it expires after a minute
        MsgLinkState(int requestedTier, int sendTier, qint64 timestampMs) : Message(), m_requestedTier(requestedTier), m_sendTier(sendTier), m_timestampMs(timestampMs) { }
    };

    class MsgSetAMBEFeature : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        Feature *getFeature() const { return m_feature; }
        std::shared_ptr<MbeEncoderPort> getPort() const { return m_port; }
        static MsgSetAMBEFeature* create(Feature *feature, std::shared_ptr<MbeEncoderPort> port) { return new MsgSetAMBEFeature(feature, port); }

    private:
        Feature *m_feature;
        std::shared_ptr<MbeEncoderPort> m_port;
        MsgSetAMBEFeature(Feature *feature, std::shared_ptr<MbeEncoderPort> port) : Message(), m_feature(feature), m_port(port) { }
    };

    DSEModProcessor();
    ~DSEModProcessor();

    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    //! Where the processor reports (the channel): the end of a transmission the protocol ended by itself
    void setReportQueue(MessageQueue *queue) { m_reportQueue = queue; }
    void setFeedbackSource(DSEModSource *source) { m_feedbackSource = source; }
    //! How the Angel61 file transfer is going, to the channel (it keys up for the next round)
    void reportFileTransfer(const DSEModAngel61Data& protocol);
    DSEModFIFO& getBasebandFifo() { return m_fifo; }
    int getFifoLowWater() const { return m_fifoLowWater * m_fifoChannels; }
    //! Values per baseband sample in the FIFO: 1 (FM: the frequency), 2 (linear: I and Q interleaved)
    int getFifoChannels() const { return m_fifoChannels.load(); }

    // Status (readable from other threads)
    bool isTransmitting() const { return m_transmitting.load(); }
    bool isVocoderReady() const { return m_vocoderReady.load(); }
    uint32_t getEncodedFrameCount() const { return m_encodedFrameCount.load(); }
    uint32_t getUnderrunCount() const { return m_underrunCount.load(); }
    uint32_t getOverrunCount() const { return m_overrunCount.load(); }
    uint32_t getTrimCount() const { return m_trimCount.load(); }
    uint32_t getTxFrameCount() const { return m_txFrameCount.load(); }
    int getPeakDeviation() const { return m_peakDeviation.load(); }

    // Call these through invokeMethod on this object's thread. API admission is
    // synchronous; modulation and encoding remain asynchronous.
    int submitAction(const QJsonObject& action, QString& error);
    QJsonObject getAPIReport() const;
    void cancelAPI(const QString& reason);
    bool isAPIActive() const { return m_apiActive.load(); }
    //! The PCM rate of the encoder the last transmission opened (0 before one has, or since the
    //! protocol changed): the source feeds its audio at this rate, so that Angel61 WB's V1 tiers
    //! get 24 kHz audio as NB's do rather than 48 kHz resampled
    int getEncoderSampleRate() const { return m_encoderSampleRate.load(); }
    bool isAPIDraining() const { return m_apiDraining.load(); }
    void outputDrained() { m_apiOutputDrained = true; }

    static constexpr int m_samplesPerAudioFrame = 960; //!< 20 ms at 48 kS/s: the audio is fed to every vocoder in 20 ms frames
    static constexpr int m_fifoSize = 48000;      //!< 1 s
    static constexpr int m_fifoLowWater = 2400;   //!< 50 ms
    static constexpr int m_fifoHighWater = 9600;  //!< 200 ms
    static constexpr int16_t m_fifoOff = -32768;  //!< FIFO sample meaning "carrier off" (the FM path never produces it)
    static constexpr int m_maxQueuedFrames = 50;
    static constexpr int m_depthWindowSamples = 48000;  //!< Time over which the minimum queue depth is measured (1 s)
    static constexpr int m_depthSlackSamples = 1920;    //!< Minimum depth the priming aims for (40 ms)
    //! Allowance above the slack before the delay control trims (60 ms): the prime credits keep a margin
    //! for devices that read the FIFO in blocks (File Output: 50 ms), which a device reading it smoothly
    //! keeps, and trimming it took packets out of the first seconds of speech
    static constexpr int m_depthJitterSamples = 2880;

private slots:
    void handleInputMessages();

private:
    friend class DSEModAPITest;
    MessageQueue m_inputMessageQueue;
    DSEModSettings m_settings;
    DSEModFIFO m_fifo;
    DSEModVocoder *m_vocoder;
    DSEModProtocol *m_protocol;
    //! The Angel61 protocol's data services (files, tunnelling, APRS, API data); null for the others
    DSEModAngel61Data *m_angel61Data = nullptr;
    DSEModShaper *m_shaper;
    DSEModLinearShaper m_linearShaper;
    std::unique_ptr<DVAngel61OFDM::Modulator> m_ofdm;     //!< OFDM protocols (Angel61 revision 3 HF)
    std::vector<std::complex<float>> m_ofdmCells, m_ofdmSamples;
    float m_ofdmGain = 1.0f;
    //! An OFDM protocol's next frame into m_samples (interleaved I, Q, peaks at -1 dBFS); false when it has none
    bool ofdmFrame();
    std::atomic<int> m_fifoChannels;
    MessageQueue *m_reportQueue;
    DSEModSource *m_feedbackSource = nullptr;
    std::vector<std::complex<float>> m_symbols;
    Feature *m_ambeFeature;     //!< Identity only
    std::shared_ptr<MbeEncoderPort> m_ambePort;
    int m_linkRequestedTier;
    int m_linkSendTier;
    qint64 m_linkTimestampMs;
    std::deque<std::array<uint8_t, DSEModVocoder::m_maxFrameBytes>> m_encodedFrames;
    std::vector<int8_t> m_levels;
    std::vector<float> m_samples;
    std::vector<int16_t> m_int16Samples;

    std::atomic<bool> m_transmitting;
    bool m_stopping; //!< end of transmission requested, no more audio expected
    bool m_restartPending = false; //!< Angel61: a rate change ended the transmission, the next one starts when it has
    std::atomic<bool> m_vocoderReady;
    std::atomic<int> m_encoderSampleRate{0};
    std::atomic<uint32_t> m_encodedFrameCount;
    std::atomic<uint32_t> m_underrunCount;
    std::atomic<uint32_t> m_overrunCount; //!< Frames dropped because the queue hit its hard bound
    std::atomic<uint32_t> m_trimCount;    //!< Frames dropped by the delay control (excess lead)
    std::atomic<uint32_t> m_txFrameCount;
    std::atomic<int> m_peakDeviation;
    QElapsedTimer m_positionTimer;
    QElapsedTimer m_fileReportTimer;   //!< the file transfer's progress goes out twice a second
    //! The last fix, for the speed and course a position source that gives neither (3.2)
    double m_lastFixLatitude = 0.0;
    double m_lastFixLongitude = 0.0;
    qint64 m_lastFixAt = 0;
    double m_fixSpeed = -1.0;          //!< km/h, smoothed
    int m_fixCourse = -1;              //!< degrees true, of the last fix that was moving //!< Refresh of the station position given to the protocol
    size_t m_depthMin;      //!< Smallest queue depth seen at frame arrival in the current window
    int m_depthWindow = 50;     //!< m_depthWindowSamples in this transmission's encoded frames
    int m_depthSlack = 2;       //!< m_depthSlackSamples in this transmission's encoded frames, rounded up
    int m_depthTrim = 5;        //!< m_depthSlackSamples + m_depthJitterSamples in encoded frames, rounded up
    int m_dropFirstFrames = 0;  //!< Encoded frames of the key-up still to drop (a preamble longer than the FIFO's lead)
    int m_depthCount;       //!< Arrivals in the current window

    DSEModTxSession m_api;
    DSEModPacketBuilder m_packets;
    DSEModGatewayMonitor m_apiMbeMonitor;
    std::atomic<bool> m_apiActive{false}, m_apiDraining{false}, m_apiOutputDrained{false};
    bool m_apiStarted = false, m_apiInputEnded = false, m_apiGenerating = false;
    bool m_apiMessage = false;
    int m_apiMessageFrames = 0;
    uint32_t m_apiSequence = 0x80000000U;
    std::deque<uint32_t> m_apiPending;
    //! Per API encode: every packet it gave (V1 gives one or two 13.3 ms packets for a 20 ms frame,
    //! all under its sequence: one kept lost a third of the speech)
    QMap<uint32_t, std::vector<std::array<uint8_t, DSEModVocoder::m_maxFrameBytes>>> m_apiResults;
    std::vector<int16_t> m_apiPCM;
    int m_apiTranscodeDecodedFrames = 0;
    bool m_apiMonitorUnavailable = false;
    //! Frames put on the air as they are: external frames (D-STAR, DMR, YSF)
    bool apiRelaying() const { return m_api.input() == "frames"; }
    //! "angel61" input: a network call. Its V1 packets go to this modulator's protocol as its encoder's
    //! would, truncated and repacked into this profile's frames at a tier no higher than the call's
    //! (ANGEL61.md 3.1.1); O1 into a V1 tier is decoded and encoded again (m_apiTranscode, the only
    //! transcoding)
    bool m_apiAngel61 = false;
    bool m_apiTranscode = false;
    bool apiTranscoding() const { return m_apiAngel61 && m_apiTranscode; }
    int m_apiCallType = -1;          //!< the call's first voice type
    int m_apiInvalidRecords = 0;     //!< records refused (their CRC-12)
    DVOpus::Decoder m_apiOpusDecoder;
    //! The first voice type among the queued frames, -1 for none yet
    int firstCallVoiceType() const;
    void relayAngel61API();
    //! O1 into V1: a frame to 80 ms of PCM for the encoder
    void decodeAngel61APIFrame(const DSEModTxSession::Frame& frame);
    // The source's PCM to the encoder's rate (48 kHz to V1's 24 in Angel61 WB): a windowed sinc
    // low pass run at the input rate, with its state kept across frames
    std::vector<float> m_pcmTaps, m_pcmHistory;
    int m_pcmHistoryPos = 0, m_pcmInRate = 0, m_pcmOutRate = 0;
    std::array<int16_t, DSEModVocoder::m_maxPcmFrameSize> m_pcmConverted;
    int64_t m_audioFrameCount = 0;  //!< 20 ms frames of this transmission: an Opus packet is every fourth, V1's three every second
    std::vector<double> m_apiTaps, m_apiHistory;
    int m_apiFilterPos = 0, m_apiDecimationCount = 0, m_apiUp = 1, m_apiDown = 1;
    QJsonArray m_apiMessageResults;
    QMap<QString, QByteArray> m_apiMessageReceipts;
    QJsonObject m_apiMessageRequest;
    QElapsedTimer m_apiEncoderWait;
    QElapsedTimer m_apiStarved;
    void serviceAPI();
    //! The levels inverted when the settings say so, but not the carrier-off markers (DMR direct mode's gaps)
    void invertLevels(std::vector<int8_t>& levels) const
    {
        if (m_settings.m_invert) {
            for (auto& level : levels) {
                if (level != DSEModShaper::m_levelOff) { level = -level; }
            }
        }
    }
    void startAPI();
    void finishAPI(const QString& state, const QString& reason = QString());
    void generateAPI();
    void resampleAPI(const QByteArray& pcm);
    void configureAPIResampler(int sourceRate);
    void queueGatewayFeedback(const QByteArray& pcm, int sampleRate);
    void encodeAPI();
    int submitMessage(const QJsonObject& message, QString& error);

    void applySettings(const QStringList& settingsKeys, const DSEModSettings& settings, bool force);
    void createVocoder();
    bool vocoderMatchesRate() const;
    const int16_t *convertPCM(const int16_t *pcm, int inRate, int outRate);
    void createProtocol();
    void updateVocoderReady();
    void startTransmit();
    void stopTransmit(bool abort);
    void generate();
    void updatePosition();
    void writeSamples();
    bool frameSource(uint8_t *frame);
    void frameSink(uint32_t sequence, const uint8_t *frame);
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODPROCESSOR_H_
