///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2020, 2022-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
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

#ifndef INCLUDE_DSDDEMODSINK_H
#define INCLUDE_DSDDEMODSINK_H

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <QDateTime>
#include <QMutex>
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include "dsp/channelsamplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/filtermbe.h"
#include "audio/audiofifo.h"
#include "util/movingaverage.h"

#include "dsddemodsettings.h"
#include "dsddemodulator.h"
#include "dsdangel61gateway.h"
#include "dsdprotocoldmr.h"
#include "dsdvocodermbelib.h"
#include "dsdvocodercodec2.h"
#include "dsdvocoderopus.h"
#include "dsdvocodervocos.h"
#include "dsp/dv/dvdtmf.h"
#include "dsp/dspcommands.h"

class BasebandSampleSink;
class ChannelAPI;
class Feature;
class MessageQueue;
class DSDProtocolDStar;
struct DVFrameEvent;

/**
 * Channel sample sink: NCO and channel filter to 48 kHz IQ, the demodulator
 * core, the vocoder (mbevocoder in software or the AMBE feature) and the audio
 * output, the constellation scope and the status report.
 */
class DSDDemodSink : public ChannelSampleSink {
public:
    struct Event
    {
        quint64 m_sequence = 0;
        QString m_text;
    };

    /** A text message received on the channel (DMR, D-STAR, Angel61...) */
    struct TextMessage
    {
        quint64 m_sequence = 0;
        QString m_protocol;
        QString m_source;
        QString m_destination;
        QString m_text;
    };

    /** What the GUI and the Web API report; copied out under a lock */
    struct Report
    {
        bool m_locked = false;
        QString m_protocol;         //!< Protocol locked to, empty when searching
        QString m_syncType;         //!< Last sync pattern name
        bool m_inverted = false;
        float m_deviationHz = 0.0f; //!< Outer symbol level (peak deviation)
        float m_frequencyErrorHz = 0.0f;
        float m_clockPpm = 0.0f;
        float m_snrDB = 0.0f;
        float m_berEstimate = 0.0f; //!< From the vocoder's corrections (0 without a software vocoder)
        int m_scopeSampleRate = 48000; //!< Raw discriminator rate while searching, symbol rate while locked
        int m_syncCount = 0;
        int m_voiceFrames = 0;          //!< Voice frames since the channel started, erasures aside...
        int m_voiceFramesErased = 0;    //!< ...and those erased
        int64_t m_correctedBits = 0;    //!< The channel errors their codes corrected since the channel started...
        int64_t m_protectedBits = 0;    //!< ...of so many bits
        bool m_slot1Voice = false;  //!< Voice frames recently on slot 1 (or the single stream)
        bool m_slot2Voice = false;
        quint64 m_angel61IPRxPackets = 0; //!< Valid, reassembled IP datagrams received over Angel61
        quint64 m_angel61IPRxBytes = 0;   //!< Full IP payload bytes in those datagrams
        QStringList m_angel61HeardCallsigns; //!< Angel61 source callsigns, most recently heard first
        QString m_statusText;       //!< Protocol dependent status line
        QString m_dtmf;             //!< The keys of the last DTMF sequence received (any protocol)
        QString m_angel61FilePending; //!< An Angel61 file offer awaiting the operator's confirmation, empty for none
        TextMessage m_lastMessage;  //!< The last text message on any protocol (DTMF aside), m_sequence 0 until one...
        QString m_lastMessageTime;  //!< ...and when it came (UTC, ISO 8601)
        QVector<Event> m_events;    //!< Recent protocol events, oldest first
        QVector<TextMessage> m_messages; //!< Recent text messages, oldest first
    };

    DSDDemodSink();
    ~DSDDemodSink();
    DSDDemodSink(const DSDDemodSink&) = delete;
    DSDDemodSink& operator=(const DSDDemodSink&) = delete;

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void applyAudioSampleRate(int sampleRate);
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void setDeviceCenterFrequency(qint64 frequency) { m_deviceCenterFrequency = frequency; }
	void applySettings(const QStringList& settingsKeys, const DSDDemodSettings& settings, bool force = false);
    AudioFifo *getAudioFifo1() { return &m_audioFifo1; }
    AudioFifo *getAudioFifo2() { return &m_audioFifo2; }
    void setAudioFifoLabel(const QString& label) {
        m_audioFifo1.setLabel("1:" + label);
        m_audioFifo2.setLabel("2:" + label);
    }
    int getAudioSampleRate() const { return m_audioSampleRate; }
    void setChannel(ChannelAPI *channel) { m_channel = channel; }

	//! From the channel's thread: returns once no feed is in progress, so that the scope may then be deleted
	void setScopeXYSink(BasebandSampleSink* scopeSink)
	{
		QMutexLocker locker(&m_scopeMutex);
		m_scopeXY = scopeSink;
	}
	void configureMyPosition(float myLatitude, float myLongitude);
    //! The operator's decision on an Angel61 file offer awaiting confirmation (any thread)
    void setAngel61FileDecision(bool accept) { m_angel61FileDecision = accept ? 1 : 2; }

	double getMagSq() { return m_magsq; }

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

    Report getReport() const;
    QVector<QJsonObject> takeContactReports();
    //! The AMBE feature's port, or none (from the channel's thread: the DSP thread takes a copy)
    void setAmbePort(std::shared_ptr<MbeEncoderPort> port);

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

    /** One audio stream (a DMR slot, or the only stream of the other protocols) */
    struct Stream
    {
        DSDVocoderMbelib m_vocoder;
        DSDVocoderOpus m_opus;                  //!< Angel61 WB audio (48 kHz)
        DSDVocoderVocos m_vocos;                //!< Angel61 revision 3 voice, V1 (24 kHz)
        DSDVocoderCodec2 m_codec2;              //!< M17 voice
        MBEAudioInterpolatorFilter m_filter;    //!< The optional high pass on the 8 kHz audio
        Interpolator m_resampler;               //!< The vocoder rate (8, 16 or 48 kHz) to the audio device rate, any ratio
        int m_vocoderRate = 8000;               //!< The rate the resampler was created for
        Real m_resampleDistance = 1.0f;         //!< vocoder rate / audio rate
        Real m_resampleRemain = 0.0f;
        AudioVector m_audioBuffer;
        int64_t m_lastVoiceSample = -1;    //!< Sample index of the last voice frame
        int64_t m_correctedBits = 0;       //!< Vocoder corrections since the last report
        int64_t m_protectedBits = 0;
        int m_voiceFramesTotal = 0;        //!< Since the channel started (the report's counters)
        int m_voiceFramesErasedTotal = 0;
        int64_t m_correctedBitsTotal = 0;
        int64_t m_protectedBitsTotal = 0;
        // Angel61 DTMF (ANGEL61.md 3.8): the keys received, made into tones in the stream's audio
        DVDTMF::Generator m_dtmf;
        std::deque<DSDAngel61Session::DTMFKey> m_dtmfPending;
        int64_t m_dtmfClock = -1;          //!< the channel's sample the tones played alone have reached
        // DTMF heard on the other protocols: AMBE's tone frames, and a detector on the decoded voice
        char m_toneKey = 0;                //!< the tone frames' key of the run
        int m_toneRun = 0;                 //!< how many in a row
        int64_t m_toneKeySample = -1;      //!< the last tone frame of a key: the detector's hearing of it is dropped
        DVDTMF::Detector m_dtmfDetector{8000};
        std::string m_dtmfHeard;           //!< the sequence heard
        QString m_dtmfProtocol;            //!< on which protocol
        int64_t m_dtmfHeardSample = -1;    //!< its last key

        //! A new transmission: the vocoder must not carry the previous call's parameters into it
        void restart()
        {
            m_vocoder.reset();
            m_opus.reset();
            m_vocos.reset();
            m_codec2.reset();
            m_resampleRemain = 0.0f; // the audio filters settle in a few samples on their own
        }
    };

    struct DMRCallState
    {
        bool m_active = false;
        int64_t m_startSample = 0;
        int m_colorCode = -1;
        DSDProtocolDMR::LinkControl m_linkControl;
    };

    int m_channelSampleRate;
    qint64 m_deviceCenterFrequency = 0;
	int m_channelFrequencyOffset;
	DSDDemodSettings m_settings;
    ChannelAPI *m_channel;
    // The AMBE feature through its port, never the feature itself: the port outlives it, and a
    // feature being destroyed closes it first (a call in flight completes, later ones return
    // false) - a feature pointer here could be used after the feature was freed, as a
    // configuration load frees its features before telling anyone
    std::shared_ptr<MbeEncoderPort> m_ambePort;
    QMutex m_ambePortMutex;
    std::shared_ptr<MbeEncoderPort> ambePort()
    {
        QMutexLocker locker(&m_ambePortMutex);
        return m_ambePort;
    }
    int m_audioSampleRate;
    int m_upsampling;               //!< Whole 8 kHz multiples for the AMBE feature, which upsamples by an integer itself
    QVector<qint16> m_demodBuffer;
    int m_demodBufferFill;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_interpolatorDistance;
	Real m_interpolatorDistanceRemain;

    MovingAverageUtil<Real, double, 16> m_movingAverage;
    double m_magsq;
    double m_magsqSum;
    double m_magsqPeak;
    int  m_magsqCount;
    MagSqLevelsStore m_magSqLevelStore;

    DSDDemodulator m_demodulator;
    std::vector<std::complex<float>> m_iqBlock;
    int64_t m_sampleCount;          //!< 48 kHz samples fed to the demodulator
    Stream m_streams[2];
    float m_myLatitude;
    float m_myLongitude;
    bool m_hasMyPosition;
    mutable QMutex m_positionMutex;
    bool getMyPosition(float& latitude, float& longitude) const
    {
        QMutexLocker locker(&m_positionMutex);
        latitude = m_myLatitude;
        longitude = m_myLongitude;
        return m_hasMyPosition;
    }
    QString m_noVocoderText;    //!< Voice frames arrive that the software vocoder cannot decode

	SampleVector m_scopeSampleBuffer;
	AudioFifo m_audioFifo1;
    AudioFifo m_audioFifo2;
	BasebandSampleSink* m_scopeXY;
	QMutex m_scopeMutex;            //!< Held while the scope is fed (the GUI deletes it on closing)
    bool m_scopeLocked;

    mutable QMutex m_reportMutex;
    Report m_report;
    float m_berEstimate;
    DMRCallState m_dmrCalls[2];
    const DSDProtocolDMR *m_dmrMetadataDecoder;
    const DSDProtocolDStar *m_dstarMetadataDecoder;
    quint64 m_dmrAliasRevision[2];
    quint64 m_dmrGPSRevision[2];
    quint64 m_dmrGPSMapRevision[2];
    quint64 m_dmrTextRevision[2];
    int64_t m_lastDMRGPSEventSample[2];
    struct DataRevisions        //!< The last text and YSF GPS position seen of a decoder
    {
        quint64 m_text = 0;
        quint64 m_position = 0;
    };
    const DSDProtocol *m_dataTextDecoder = nullptr; //!< The decoder locked to, or last locked to
    QHash<const DSDProtocol*, DataRevisions> m_dataRevisions;
    QString m_statusText;                       //!< As last made (statusTextDue)
    int64_t m_statusTextSample = 0;
    bool m_dstarMapPositionValid;
    double m_lastDStarMapLatitude;
    double m_lastDStarMapLongitude;
    QString m_lastDStarMapName;
    mutable QMutex m_contactMutex;
    QVector<QJsonObject> m_contactReports;
    DSDAngel61Gateway m_angel61Gateway;    //!< Revision 3 (plan C7): the sessions' calls for a gateway
    quint64 m_angel61IPRxPackets;   //!< Valid, reassembled IP datagrams received since sink start
    quint64 m_angel61IPRxBytes;
    QStringList m_angel61HeardCallsigns;
    std::vector<uint32_t> m_angel61Groups;  //!< the talkgroups played (m_settings.m_angel61Groups), empty: all
    //! Whether the Angel61 call now received is to be played: not a talkgroup outside the group list
    bool angel61CallPlayed() const;
    /**
     * Angel61 DTMF (3.8): the keys' tones over the start of `audio` (n samples at `rate`, full scale
     * 32767), in place of what was there; returns how many samples they took. A key begins only when
     * the one before has played, at the rate of the audio it goes into.
     */
    int overlayDTMF(Stream& stream, float *audio, int n, int rate);
    //! DTMF keys with no voice to go into (a transmission of DTMF alone): their tones by the channel's time
    void playDTMFAlone();
    //! A frame's tone frame key (0: none): a key once it has lasted two frames (40 ms)
    void toneFrameKey(Stream& stream, char key);
    //! The detector on a stream's decoded voice (full scale 32767, 8 kHz)
    void detectDTMF(Stream& stream, const float *audio, int n);
    //! A key heard on a stream; fromToneFrames: AMBE's (the detector's of the same tone dropped)
    void heardDTMF(Stream& stream, char key, bool fromToneFrames);
    //! Sequences with no key for 2 s: their keys as text and an event
    void endHeardDTMF();
    QString m_lastDTMF;                     //!< the keys of the last DTMF sequence received (the report)
    QVector<Event> m_events;
    quint64 m_eventSequence;
    QVector<TextMessage> m_messages;
    quint64 m_messageSequence;
    QString m_lastDStarText;        //!< The last D-STAR slow data text reported as a message
    QString m_lastAPRSPacket;       //!< The last APRS (D-PRS) packet forwarded, for the duplicate window
    QDateTime m_lastAPRSTime;
    quint64 m_lastDPRSRevision;     //!< Of the decoder, so that a repeat inside a transmission is seen
    uint32_t m_angel61FileCrc = 0;  //!< Of the file being received, so that an offer is reported once
    bool m_angel61FileRejected = false;
    DVAngel61FilePartials m_angel61Partials;   //!< unfinished transfers kept for a resume (3.4), on disk under the file directory
    std::atomic<int> m_angel61FileDecision{0}; //!< the operator's: 1 accept, 2 refuse, 0 none (any thread)
    QString m_angel61FilePending;
    TextMessage m_lastMessage;      //!< The report's last message (DTMF aside)
    QString m_lastMessageTime;
    bool m_angel61ChannelBusy = false;           //!< for the DSE Modulators' channel access (3.5)
    uint64_t m_angel61EndedSource = 0;           //!< ... the transmission that ended last: whose, to whom
    uint64_t m_angel61EndedDestination = 0;
    QHash<MessageQueue*, quint64> m_dvSuppressedCalls;

    void processBlock();
    void processFrame(const DSDVoiceFrame& frame);
    void publishDVEvent(const DSDProtocolEvent& event);
    void publishDVFrameEvent(const DVFrameEvent& event);
    //! nbSamples of audio at vocoderRate to the stream's FIFO through its resampler
    void outputAudio(Stream& stream, int streamIndex, const float *audio, int nbSamples, int vocoderRate);
    void createResampler(Stream& stream, int vocoderRate);
    void updateReport();
    void updateDMRCallEvents(bool slot1Voice, bool slot2Voice);
    void updateDStarMap();
    //! An APRS packet (D-PRS in the D-STAR slow data) to the APRS feature and any other "packets" consumer
    void forwardAPRS(const QString& tnc2);
    //! An Angel61 session's traffic (events, messages, rate records, the gateway, contacts), and
    //! with `report` the report of it
    void updateAngel61(DSDAngel61Session& session, bool report);
    //! The file transfer of an Angel61 transmission (3.4): the offer, the file, the answer (when
    //! the transmission has just ended), of a DSDAngel61Session
    template <class Session> void updateAngel61File(Session& session, bool ended);
    //! The received file under the configured directory; the path written, empty when it was not
    QString writeAngel61File(const QString& name, const std::vector<uint8_t>& data);
    //! A file control message to the DSE Modulators of this instance, through the angel61 pipe
    void sendAngel61FileControl(const DVAngel61FileControl& control, uint64_t destination, bool owed);
    void updateAngel61FileDecision();
    void sendAngel61ChannelState(bool busy);
    QString angel61PartialDirectory() const;
    void loadAngel61Partials();
    void persistAngel61Partials();
    //! IP tunnelling (3.5): datagrams and tunnel control out to the instance's DSE Modulators
    template <class Session> void updateAngel61Tunnel(Session& session);
    void addAngel61HeardCallsign(uint64_t encodedCallsign);
    void sendAngel61Packet(const QString& text);
    void updateDataText();
    bool sendPositionToMap(const QString& name, const QString& label, const QString& text,
        double latitude, double longitude);
    void addDMRCallEvent(int slot, const QString& action, const DMRCallState& call, float durationSeconds = -1.0f);
    void addEvent(const QString& text);
    void addTextMessage(const QString& protocol, const QString& source, const QString& destination, const QString& text, bool dtmf = false);
    void reportVoiceCounters(Report& report) const;
    void updateBerEstimate(bool locked, bool voice);
    bool statusTextDue();
    QString formatStatusText() const;
    QString formatDMR() const;
    QString formatDStar() const;
    static DSDDemodulator::Protocol toProtocol(int protocol);
};

#endif // INCLUDE_DSDDEMODSINK_H
