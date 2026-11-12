///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODSOURCE_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODSOURCE_H_

#include <atomic>
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>

#include <QObject>
#include <QByteArray>
#include <QRecursiveMutex>
#include <QThread>

#include "dsp/channelsamplesource.h"
#include "util/message.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/cwkeyer.h"

#include "util/movingaverage.h"
#include "audio/audiofifo.h"
#include "audio/audioresampler.h"

#include "dsemodsettings.h"
#include "dsemodaudiofile.h"
#include "dsemodlivedtmf.h"

class ChannelAPI;
class BasebandSampleSink;
class Feature;
class MbeEncoderPort;
class DSEModProcessor;

class DSEModSource : public QObject, public ChannelSampleSource
{
    Q_OBJECT
public:
    //! Send DTMF digits as tone frames (channel -> baseband -> source)
    class MsgSendDTMF : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getDigits() const { return m_digits; }
        bool getAutoTransmit() const { return m_autoTransmit; } //!< The transmission was started for the digits only: silence follows them
        static MsgSendDTMF* create(const QString& digits, bool autoTransmit = false) { return new MsgSendDTMF(digits, autoTransmit); }

    private:
        QString m_digits;
        bool m_autoTransmit;
        MsgSendDTMF(const QString& digits, bool autoTransmit) : Message(), m_digits(digits), m_autoTransmit(autoTransmit) { }
    };

    //! A text message for the protocol's data channel (channel -> baseband -> source -> processor)
    class MsgSendMessage : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getText() const { return m_text; }
        bool getAutoTransmit() const { return m_autoTransmit; } //!< The transmission was started for the message only
        static MsgSendMessage* create(const QString& text, bool autoTransmit = false) { return new MsgSendMessage(text, autoTransmit); }

    private:
        QString m_text;
        bool m_autoTransmit;
        MsgSendMessage(const QString& text, bool autoTransmit) : Message(), m_text(text), m_autoTransmit(autoTransmit) { }
    };

    //! An APRS information field to send as content type 2 (Angel61 3.3)
    class MsgSendAPRS : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getInfo() const { return m_info; }
        static MsgSendAPRS* create(const QString& info) { return new MsgSendAPRS(info); }

    private:
        QString m_info;
        MsgSendAPRS(const QString& info) : Message(), m_info(info) { }
    };

    //! A file to offer over Angel61 (3.4): channel -> baseband -> source -> processor
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

    //! One IP datagram for the tunnel of 3.5 (channel -> processor)
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

    //! What this station will route, for the advertise of 3.5 (channel -> processor)
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

    //! Tunnel control: to send (the decompressor's context state and acknowledgements), or heard from the peer for the header compressor (3.6) (channel -> processor)
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

    //! A file transfer's answer (channel -> processor): heard by a DSD Demodulator, for this
    //! station's transfer; or, `send`, owed by this station to `destination`, which sends it a file
    class MsgFileControl : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QByteArray& getControl() const { return m_control; }
        bool getSend() const { return m_send; }
        const QString& getDestination() const { return m_destination; }
        bool getAutoTransmit() const { return m_autoTransmit; }
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

    //! A file transfer's answer did not come in time (channel -> processor)
    class MsgFileNoAnswer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgFileNoAnswer* create() { return new MsgFileNoAnswer(); }

    private:
        MsgFileNoAnswer() : Message() { }
    };

    //! How a file transfer is going (processor -> channel -> GUI)
    class MsgReportFileTransfer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getState() const { return m_state; }             //!< DVAngel61FileSender::State
        int getBlocksSent() const { return m_blocksSent; }
        int getBlocksPending() const { return m_blocksPending; }
        int getBlocksTotal() const { return m_blocksTotal; }
        int getRounds() const { return m_rounds; }
        int getBlockSize() const { return m_blockSize; }     //!< bytes, for the rate the GUI shows
        bool wantsTransmission() const { return m_wantsTransmission; }
        static MsgReportFileTransfer* create(int state, int blocksSent, int blocksPending, int blocksTotal, int rounds, bool wantsTransmission, int blockSize = 0) {
            return new MsgReportFileTransfer(state, blocksSent, blocksPending, blocksTotal, rounds, wantsTransmission, blockSize);
        }

    private:
        int m_state;
        int m_blocksSent;
        int m_blocksPending;
        int m_blocksTotal;
        int m_rounds;
        int m_blockSize;
        bool m_wantsTransmission;
        MsgReportFileTransfer(int state, int blocksSent, int blocksPending, int blocksTotal, int rounds, bool wantsTransmission, int blockSize) :
            Message(), m_state(state), m_blocksSent(blocksSent), m_blocksPending(blocksPending),
            m_blocksTotal(blocksTotal), m_rounds(rounds), m_blockSize(blockSize), m_wantsTransmission(wantsTransmission)
        { }
    };

    //! The receiver side's report on the Angel61 destination (channel -> processor)
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
        qint64 m_timestampMs;
        MsgLinkState(int requestedTier, int sendTier, qint64 timestampMs) : Message(), m_requestedTier(requestedTier), m_sendTier(sendTier), m_timestampMs(timestampMs) { }
    };

    //! The protocol ended the transmission by itself (a data only transmission): processor -> channel
    class MsgReportTransmissionEnded : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgReportTransmissionEnded* create() { return new MsgReportTransmissionEnded(); }

    private:
        MsgReportTransmissionEnded() : Message() { }
    };

    //! The DTMF sequence has been sent (source -> channel)
    class MsgReportDTMFDone : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgReportDTMFDone* create() { return new MsgReportDTMFDone(); }

    private:
        MsgReportDTMFDone() : Message() { }
    };

    DSEModSource();
    virtual ~DSEModSource();

    virtual void pull(SampleVector::iterator begin, unsigned int nbSamples);
    virtual void pullOne(Sample& sample);
    virtual void prefetch(unsigned int nbSamples) { (void) nbSamples; }

    void setInputFile(DSEModAudioFile *file, QRecursiveMutex *mutex) { m_audioFile = file; m_fileMutex = mutex; }
    AudioFifo *getAudioFifo() { return &m_audioFifo; }
    //! Local pre-encode audio, or decoded Angel61 gateway audio, for the operator
    AudioFifo *getFeedbackAudioFifo() { return &m_feedbackAudioFifo; }
    void applyFeedbackAudioSampleRate(int sampleRate);
    int getFeedbackAudioSampleRate() const { return m_feedbackAudioSampleRate; }
    void playGatewayFeedback(const QByteArray& pcm, int sampleRate);
    void applyAudioSampleRate(int sampleRate);
    //! The vocoder audio rate of a mode: the resampler, tone, keyer and frame size follow
    void applyPcmSampleRate(int sampleRate);
    int getAudioSampleRate() const { return m_audioSampleRate; }
    void setChannel(ChannelAPI *channel);
    void sendMessage(const QString& text, bool autoTransmit = false); //!< Queue a text message for the protocol (Angel61)
    void setLinkState(int requestedTier, int sendTier, qint64 timestampMs); //!< The destination's rate request (Angel61)
    void sendAPRS(const QString& info);                                    //!< An APRS information field (Angel61 3.3)
    void sendFile(const QString& name, const QByteArray& data);            //!< Offer a file (Angel61 3.4)
    void sendDatagram(const QByteArray& datagram, bool autoTransmit = false);                         //!< One IP datagram (Angel61 3.5)
    void tunnelState(bool enabled, const QByteArray& advertise);           //!< What this station tunnels
    void tunnelControl(const QByteArray& control, bool send);              //!< Header compression control (Angel61 3.6)
    void fileControl(const QByteArray& control, bool send, const QString& destination, bool autoTransmit); //!< A file transfer's answer
    void fileNoAnswer();                                                    //!< ... that did not come
    void setSpectrumSink(BasebandSampleSink *sampleSink) { m_spectrumSink = sampleSink; }
    double getMagSq() const { return m_magsq; }
    uint32_t getAudioUnderrunCount() const { return m_audioUnderruns.load(); } //!< PCM frames short of microphone samples
    uint32_t getAudioOverrunCount() const { return m_audioOverruns.load(); }   //!< Microphone blocks discarded (buffer full)
    void getLevels(qreal& rmsLevel, qreal& peakLevel, int& numSamples) const
    {
        rmsLevel = m_rmsLevel;
        peakLevel = m_peakLevelOut;
        numSamples = m_levelNbSamples;
    }
    CWKeyer& getCWKeyer() { return m_cwKeyer; }
    DSEModProcessor *getProcessor() { return m_processor; }
    void setAMBEFeature(Feature *feature, std::shared_ptr<MbeEncoderPort> port);
    void sendDTMF(const QString& digits, bool autoTransmit); //!< Queue DTMF digits (0-9, A-D, *, #); other characters are ignored
    void applySettings(const QStringList& settingsKeys, const DSEModSettings& settings, bool force = false);
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);

    static constexpr int m_basebandSampleRate = 48000; //!< Modulator baseband sample rate
    //! The vocoder's audio rate: 8 kHz for the MBE codecs, 24 kHz for V1 (Angel61), 48 for Opus, set by the mode's protocol
    int getPcmSampleRate() const { return m_pcmSampleRate; }
    static constexpr int m_dtmfToneFrames = 5;         //!< DTMF digit duration in 20 ms frames (100 ms)
    static constexpr int m_dtmfGapFrames = 5;          //!< Silence between DTMF digits in 20 ms frames (100 ms)

private:
    int m_channelSampleRate;
    int m_pcmSampleRate;        //!< Vocoder audio sample rate (8000, 24000 or 48000)
    int m_pcmFrameSize;         //!< PCM samples per 20 ms frame at that rate (160 or 320)
    int m_channelFrequencyOffset;
    DSEModSettings m_settings;
    ChannelAPI *m_channel;
    BasebandSampleSink *m_spectrumSink;

    NCO m_carrierNco;
    NCO m_toneNco;
    float m_modPhasor;
    float m_envelope;                           //!< Carrier amplitude 0..1 (ramped at burst edges)
    static constexpr int m_envelopeSamples = 24;    //!< Ramp length: 0.5 ms
    Complex m_modSample;
    int m_peakDeviation;

    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;

    double m_magsq;
    MovingAverageUtil<double, double, 16> m_movingAverage;

    int m_audioSampleRate;
    AudioVector m_audioReadBuffer;
    unsigned int m_audioReadBufferFill;
    AudioFifo m_audioFifo;
    int m_feedbackAudioSampleRate;
    int m_feedbackInputSampleRate;
    AudioVector m_feedbackAudioBuffer;
    uint m_feedbackAudioBufferFill;
    AudioFifo m_feedbackAudioFifo;
    Interpolator m_feedbackInterpolator;
    Real m_feedbackInterpolatorDistance;
    Real m_feedbackInterpolatorDistanceRemain;
    //! One sample of local or gateway audio at m_feedbackInputSampleRate
    void pushFeedback(Real sample);
    void processOneFeedbackSample(Complex& ci);
    void configureFeedbackInputRate(int sampleRate);
    void flushFeedback();
    std::atomic<uint32_t> m_audioUnderruns;
    std::atomic<uint32_t> m_audioOverruns;
    // The microphone's rate to the encoder's, any ratio (44.1 kHz too), low-passed at the encoder's
    // band before the rate changes (configureMicResampler)
    Interpolator m_micInterpolator;
    Real m_micDistance = 1.0f;
    Real m_micDistanceRemain = 0.0f;
    void configureMicResampler();
    //! One microphone sample at the device's rate; the encoder's samples it completes into pcm
    void resampleMic(qint16 in, int16_t *pcm, int& produced, int n);
    CWKeyer m_cwKeyer;
    DSEModAudioFile *m_audioFile;   //!< Owned by the channel, opened and sought there; resampled to the vocoder's rate here
    QRecursiveMutex *m_fileMutex;   //!< Owned by the channel with the file
    QRecursiveMutex m_mutex;

    DSEModProcessor *m_processor;
    QThread m_processorThread;

    int m_pcmSampleCount;
    uint32_t m_pcmSequence;
    std::string m_dtmfDigits; //!< DTMF digits still to send, current one first
    int m_dtmfDigitFrame;     //!< Frame index within the current DTMF digit (tone then gap)
    bool m_dtmfOnly;          //!< The transmission carries DTMF digits only: silence instead of the speech input around them
    DSEModLiveDTMF m_liveDTMF;  //!< Angel61's live DTMF (ANGEL61.md 3.8): the speech input's keys as data, their tones muted
    //! Angel61 with tone detection: DTMF in the speech is detected and sent as data
    bool liveDTMF() const;
    //! The frame's speech through the detector and the delay line, its tones muted
    void processLiveDTMF(int16_t *pcm, int n);
    int m_generatePullCount;
    SampleVector m_spectrumBuffer;
    bool m_spectrumAudioActive;   //!< What m_spectrumBuffer holds: encoder audio (true) or IQ
    void feedSpectrumAudio(const int16_t *pcm, int n);

    quint32 m_levelCalcCount;
    qreal m_rmsLevel;
    qreal m_peakLevelOut;
    Real m_peakLevel;
    Real m_levelSum;

    static const int m_levelNbSamples;

    void modulateSample();
    void pushAudioFrame();
    int fillPCM(int16_t *pcm); //!< Returns the number of samples carrying the tone (Tone and CW inputs)
    bool fillDTMF(int16_t *pcm, float& low, float& high); //!< Returns true if the frame is a DTMF tone (false: gap)
    static int toneAmplitude(float peak); //!< AMBE tone frame amplitude field for a component of the given peak (16 bit scale)
    void calculateLevel(int16_t sample);
    float channelFilterCutoff(const DSEModSettings& settings) const;

private slots:
    void handleAudio();
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODSOURCE_H_
