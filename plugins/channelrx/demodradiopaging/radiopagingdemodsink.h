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

#ifndef INCLUDE_RADIOPAGINGDEMODSINK_H
#define INCLUDE_RADIOPAGINGDEMODSINK_H

#include <QMutex>
#include <QString>

#include <atomic>
#include <memory>
#include <vector>

#include "dsp/channelsamplesink.h"
#include "dsp/wavfilerecord.h"
#include "dsp/phasediscri.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/firfilter.h"
#include "audio/audiofifo.h"
#include "util/movingaverage.h"
#include "util/messagerecord.h"

#include "radiopagingpocsagreceiver.h"
#include "radiopagingflexreceiver.h"
#include "radiopaginggscreceiver.h"
#include "radiopagingcharset.h"

#include "radiopagingdemodsettings.h"

class ChannelAPI;
class MessageQueue;
class ScopeVis;

// Channel IQ at 38400 Hz is shared by one leg per protocol. Each leg has its own filter at
// the protocol's Carson bandwidth (skipped when not narrower than the channel filter) and
// its own FM discriminator scaled to the protocol's deviation, and feeds the receivers of
// all its speeds, each with both polarities. Nothing is shared between the legs: every
// receiver hunts for its own sync. Decoded messages are sent to the channel as records
class RadioPagingDemodSink : public ChannelSampleSink {
public:
    struct Status
    {
        bool m_enabled[3];              //!< POCSAG, FLEX, GSC decoding on
        RadioPagingBerWindow m_ber[3];  //!< Of the reception in progress, or the last one
        bool m_berLive[3];              //!< m_ber is of a reception in progress
        float m_offset[3];              //!< Carrier frequency offset (DC offset) of the same reception, Hz
        bool m_offsetValid[3];          //!< m_offset was measured
        int m_pocsagBatches;            //!< Batches of the POCSAG transmission in progress (or last)
        // FLEX: the network of the last frame
        RadioPagingFlexDecoder::FrameReport m_flexFrame;
        RadioPagingFlexDecoder::NetworkTime m_flexTime;
        RadioPagingBerWindow m_flexPhaseBer[4];     //!< Of each phase A to D, of the recent frames
        std::vector<RadioPagingFlexDecoder::GroupSlot> m_flexGroups;
        int m_flexReassemblies;
        // GSC: the last page received
        int m_gscPreamble;              //!< Preamble value index, -1 non battery saver or none yet
        bool m_gscBatch;
        bool m_gscNbs;
        int m_gscSlot;
        bool m_pocsagSync[RadioPagingDemodSettings::m_nbPocsagSpeeds];      //!< A decoder of the speed is synchronised
        bool m_pocsagInverted[RadioPagingDemodSettings::m_nbPocsagSpeeds];  //!< ... and it is the inverted one
        bool m_flexSync;
        QString m_flexMode;
        bool m_flexInverted;
        float m_flexQuality;
        bool m_flexErs;                 //!< Emergency re-synchronization being received
        float m_flexErsSeconds;         //!< Length of the current or last one
        quint32 m_flexErsCount;         //!< Re-synchronizations received
        bool m_gscSync;
        bool m_gscVoice;
        bool m_gscAudio;                //!< The voice message is being played
        bool m_gscInverted;
        quint32 m_messages[3];          //!< POCSAG, FLEX, GSC records sent since start
        quint32 m_locksDropped[3] = {0, 0, 0};  //!< Locks found in the data of a FLEX frame being received, dropped
        int m_remembered[3];            //!< Messages remembered for duplicate detection
        qint64 m_resetTime[3];          //!< When the state of the protocol was last reset, ms since epoch
        qint64 m_signalMs = 0;          //!< Time of the signal processed since the start, from its samples: a clock that
                                        //!< follows the signal (a file played faster or paused too)

        //! The network of one polarity of a protocol
        struct Polarity
        {
            bool m_seen = false;        //!< Synchronised or a message received on the polarity since the last reset
            bool m_sync = false;
            RadioPagingBerWindow m_ber; //!< Of the reception in progress, or the last one
            bool m_berLive = false;
            float m_offset = 0.0f;      //!< Carrier frequency offset of the same reception, Hz
            bool m_offsetValid = false;
            quint32 m_messages = 0;     //!< Records sent since start
            int m_remembered = 0;       //!< Messages remembered for duplicate detection
        };
        Polarity m_pol[3][2];           //!< POCSAG, FLEX, GSC; normal, inverted
        bool m_pocsagSpeedSync[2][RadioPagingDemodSettings::m_nbPocsagSpeeds];     //!< By polarity, a decoder of the speed is synchronised
        // FLEX: the network of each polarity, of its last frame
        QString m_flexModePol[2];
        RadioPagingFlexDecoder::FrameReport m_flexFramePol[2];
        RadioPagingFlexDecoder::NetworkTime m_flexTimePol[2];
        RadioPagingBerWindow m_flexPhaseBerPol[2][4];
        std::vector<RadioPagingFlexDecoder::GroupSlot> m_flexGroupsPol[2];
        std::vector<RadioPagingFlexDecoder::GroupSlotState> m_flexGroupStatesPol[2];   //!< The 16 slots of each polarity
        int m_flexReassembliesPol[2];
        bool m_gscVoicePol[2];
    };

    RadioPagingDemodSink();
    ~RadioPagingDemodSink();

    //! Signals the scope shows (RadioPagingDemodSettings::m_scopeCh1 and m_scopeCh2), at the channel rate
    enum ScopeSignal {
        ScopeI,                         //!< Channel IQ, full scale 1
        ScopeQ,
        ScopeMagSq,                     //!< Channel power, full scale 1
        ScopePocsagFm,                  //!< Discriminator of each leg, 1 at the deviation of the protocol
        ScopeFlexFm,
        ScopeGscFm,
        ScopePocsagShift,               //!< Carrier offset correction of each leg, kHz
        ScopeFlexShift,
        ScopeGscShift,
        ScopePocsagSync,                //!< 1 while a receiver of the protocol is synchronised
        ScopeFlexSync,
        ScopeGscSync,
        ScopeSignalCount
    };
    static QString scopeSignalName(int signal);
    void setScopeSink(ScopeVis *scopeSink) { m_scopeSink = scopeSink; }
    //! The scope costs time for each sample: it is fed only while it is shown (any thread)
    void setScopeEnabled(bool enabled) { m_scopeEnabled.store(enabled, std::memory_order_relaxed); }

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const QStringList& settingsKeys, const RadioPagingDemodSettings& settings, bool force = false);
    void setMessageQueueToChannel(MessageQueue *messageQueue) { m_messageQueueToChannel = messageQueue; }
    void setChannel(ChannelAPI *channel) { m_channel = channel; }

    double getMagSq() const { return m_magsq; }
    AudioFifo *getAudioFifo() { return &m_audioFifo; }
    void setAudioFifoLabel(const QString& label) { m_audioFifo.setLabel(label); }
    //!< Centre frequency of the device, Hz: with the channel offset it names the voice files and is the frequency of
    //!< the receivers (part of their duplicate key)
    void setCenterFrequency(qint64 frequency)
    {
        m_centerFrequency = frequency;
        applyReceiverFrequency();
    }
    void applyAudioSampleRate(int sampleRate);
    int getAudioSampleRate() const { return m_audioSampleRate; }
    Status getStatus() const;
    //!< Forget what the receivers of a protocol remember (0 POCSAG, 1 FLEX, 2 GSC, -1 all): duplicates,
    //!< fragments, groups, message numbers, network time
    //!< Releases the messages being received (decoded as far as they were received) and clears the protocol's state;
    //!< protocol -1: all of them (the input stopped)
    void resetProtocol(int protocol);

    void getMagSqLevels(double& avg, double& peak, int& nbSamples)
    {
        if (m_magsqCount > 0)
        {
            m_magsq = m_magsqSum / m_magsqCount;
            m_magSqLevelStore.m_magsq = m_magsq;
            m_magSqLevelStore.m_magsqPeak = m_magsqPeak;
        }

        avg = m_magSqLevelStore.m_magsq;
        peak = m_magSqLevelStore.m_magsqPeak;
        nbSamples = m_magsqCount == 0 ? 1 : m_magsqCount;

        m_magsqSum = 0.0f;
        m_magsqPeak = 0.0f;
        m_magsqCount = 0;
    }

private:
    struct MagSqLevelsStore
    {
        MagSqLevelsStore() :
            m_magsq(1e-12),
            m_magsqPeak(1e-12)
        {}
        double m_magsq;
        double m_magsqPeak;
    };

    //! Low pass whose taps are changed without clearing its samples (same number of taps, so
    //! the same delay): the leg changes width without a transient
    struct LegFilter : public Lowpass<Complex>
    {
        void retune(int nTaps, double sampleRate, double cutoff) {
            FirFilterGenerators::generateLowPassFilter(nTaps, sampleRate, cutoff, this->m_taps);
        }
    };

    // Frequency correction, filter and FM discriminator of one protocol. Hunting, the leg is
    // the channel wide, centred on the last carrier offset, which decays to 0. While a
    // transmission is received it is moved onto the carrier offset measured on the known
    // patterns and narrowed: POCSAG and FLEX to the Carson bandwidth, GSC to the Carson
    // bandwidth and the offset room
    struct Leg
    {
        NCO m_nco;
        LegFilter m_filter;
        PhaseDiscriminators m_discri;
        double m_huntBandwidth = 0.0;
        double m_trackBandwidth = 0.0;
        double m_deviation = 1.0;       //!< Hz at discriminator output 1
        double m_maxShift = 0.0;        //!< Largest correction: the room the channel leaves, Hz
        bool m_tracking = false;
        double m_shift = 0.0;           //!< Correction applied: the signal is moved down by it, Hz
        double m_huntCentre = 0.0;      //!< Correction while hunting, Hz
        int m_fits = 0;                 //!< Measurements of the transmission being received
        quint32 m_messagesAtStart = 0;
        uint32_t m_framesAtStart = 0;
        int m_pendingSamples = 0;       //!< Samples until a correction reaches the discriminator
        float m_pendingDelta = 0.0f;    //!< DC change of that correction, discriminator units

        void create(double huntBandwidth, double trackBandwidth, double channelBandwidth, double deviation);
        void setTracking(bool tracking);
        //!< Move the correction; the receivers' levels move by the returned delta once it has
        //!< gone through the filter (takeHandover())
        void setShift(double shift);
        bool takeHandover(float& delta);
        Real demod(const Complex& sample);
    };

    RadioPagingDemodSettings m_settings;
    ChannelAPI *m_channel;
    MessageQueue *m_messageQueueToChannel;
    int m_channelSampleRate;
    int m_channelFrequencyOffset;

    NCO m_nco;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;

    double m_magsq;
    double m_magsqSum;
    double m_magsqPeak;
    int m_magsqCount;
    MagSqLevelsStore m_magSqLevelStore;
    MovingAverageUtil<Real, double, 16> m_movingAverage;

    Leg m_pocsagLeg;
    Leg m_flexLeg;
    Leg m_gscLeg;
    RadioPagingPocsagReceiver m_pocsag[RadioPagingDemodSettings::m_nbPocsagSpeeds];
    RadioPagingFlexReceiver m_flex;
    RadioPagingGscReceiver m_gsc;
    QString m_gscVoiceId[2];            //!< Record id of the voice page in progress, by polarity
    RadioPagingBerWindow m_lastBer[3];  //!< BER of the last reception of each protocol
    float m_lastOffset[3] = {0.0f, 0.0f, 0.0f};         //!< Carrier offset of the last reception, Hz
    bool m_lastOffsetValid[3] = {false, false, false};
    int m_lastPocsagBatches = 0;
    int m_lastGscPreamble = -1;
    bool m_lastGscBatch = false;
    bool m_lastGscNbs = false;
    int m_lastGscSlot = 0;
    qint64 m_resetTime[3] = {0, 0, 0};  //!< When the state of each protocol was last reset, ms since epoch
    //! What is kept of the reception of each polarity of each protocol between the status updates
    struct PolarityLast
    {
        bool m_seen = false;
        RadioPagingBerWindow m_ber;
        float m_offset = 0.0f;
        bool m_offsetValid = false;
    };
    PolarityLast m_polLast[3][2];
    quint32 m_polMessages[3][2] = {{0, 0}, {0, 0}, {0, 0}};    //!< Records sent, by polarity (m_statusMutex)
    bool m_polMessageSeen[3][2] = {{false, false}, {false, false}, {false, false}}; //!< A record sent since the reset (m_statusMutex)
    QString m_flexLastMode[2];
    RadioPagingFlexDecoder::FrameReport m_flexLastFrame[2];
    RadioPagingBerWindow m_flexLastPhaseBer[2][4];

    // GSC voice audio: the GSC leg discriminator, DC blocked, low pass filtered and
    // resampled to the audio rate, only while a voice page is open
    AudioFifo m_audioFifo;
    AudioVector m_audioBuffer;
    std::size_t m_audioBufferFill;
    int m_audioSampleRate;
    Interpolator m_audioInterpolator;
    Real m_audioDistance;
    Real m_audioDistanceRemain;
    bool m_audioOpen;
    // One voice message at a time is recorded, that of the GSC receiver in a voice message
    std::unique_ptr<WavFileRecord> m_voiceRecorder; //!< Set while the voice message is recorded
    QString m_voiceFile;                //!< The file being recorded
    qint64 m_centerFrequency = 0;
    QString m_voiceFileDone;            //!< The file of the voice message that ended, for its record
    std::vector<qint16> m_voiceBuffer;  //!< Samples waiting to be written to the file
    Real m_dcIn;
    Real m_dcOut;

    mutable QMutex m_statusMutex;
    Status m_status;
    int m_statusCount;
    quint64 m_statusPeriods = 0;        //!< Status periods of signal processed since the start

    void processOneSample(Complex& ci);
    // Scope: the two signals selected, in blocks
    static const int m_scopeBufferSize = 1000;
    ScopeVis *m_scopeSink = nullptr;
    std::atomic<bool> m_scopeEnabled{false};
    ComplexVector m_scopeBuffer;
    int m_scopeBufferIndex = 0;
    Real scopeValue(int signal, const Complex& ci, Real magsq, const Real fm[3], const bool sync[3]) const;
    void sampleToScope(const Complex& sample);
    void applyLegs();
    //!< A receiver measured the DC offset dc on a known pattern: the leg absorbs it
    void legMeasured(Leg& leg, int protocol, float dc);
    //!< The receivers of the leg hunt again: back to the hunting width and centre
    void legReleased(Leg& leg, int protocol, bool confirmed);
    int m_afcCount = 0;                 //!< Status periods since the last decay of the hunting centres
    void decayHuntCentres();
    void processAudio(Real fm);
    void pushAudio(Real sample);
    //!< The voice is written to a WAV file in the voice directory, at the audio rate, whatever the volume
    void startVoiceRecording();
    void recordAudio(Real sample);
    //!< Closes the file; its name, empty if none was recorded
    QString finishVoiceRecording();
    void closeAudio();
    void updateStatus();
    void updatePolarityStatus(Status& status, const RadioPagingFlexReceiver::Status& flex, const RadioPagingGscReceiver::Status& gsc);
    void sendRecord(MessageRecord& record, int protocol);
    void flushProtocol(int protocol);
    void sendPocsag(const RadioPagingPocsagDecoder::Message& message, int baud);
    void sendFlex(const RadioPagingFlexDecoder::Message& message);
    //!< Frequency of the channel to the receivers
    void applyReceiverFrequency();
    void sendGsc(const RadioPagingGscDecoder::Message& message);
    static MessageRecord::Polarity polarity(bool inverted);
    static QString checkName(int check);
    static void insertAlternative(MessageRecord& record, const std::string& type, const std::string& text, int score);
    //!< Characters below 0x20 and 0x7f as <0xNN>, other bytes kept (UTF-8 passes)
    static QString escapeText(const std::string& text);
    //!< The codepage of the key in the protocol (the custom codepage of the protocol too), else the default of the protocol
    const RadioPagingCharset::Codepage *codepage(const RadioPagingCharset::Key& key, const char *protocol) const;
    //!< The custom codepage of each protocol (POCSAG, FLEX, GSC), made of its codes in the settings
    std::unique_ptr<RadioPagingCustomCodepage> m_customCodepage[3];
    void makeCustomCodepages(const QStringList& settingsKeys, bool force);
    //!< The custom codes of a protocol for its record, when its custom codepage is used
    void insertCustomCodes(MessageRecord& record, const RadioPagingCharset::Codepage *charset, int protocol) const;
    //!< The codepage of HEX/binary messages (FLEX Kanji, STD-43A Japanese Text Service)
    static bool isKanji(const RadioPagingCharset::Codepage *codepage);
    static QByteArray toByteArray(const std::vector<uint8_t>& data);
};

#endif // INCLUDE_RADIOPAGINGDEMODSINK_H
