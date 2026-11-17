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

#ifndef INCLUDE_RADIOPAGINGMODSOURCE_H
#define INCLUDE_RADIOPAGINGMODSOURCE_H

#include <QMutex>
#include <QString>

#include <deque>
#include <functional>
#include <memory>
#include <vector>

#include "dsp/channelsamplesource.h"
#include "dsp/nco.h"
#include "util/movingaverage.h"

#include "radiopagingfskmod.h"
#include "radiopagingtxstream.h"
#include "radiopagingmodsettings.h"

class ChannelAPI;

// Sample source of the radio paging modulator. The symbols come from the stream of the
// protocol and mode of the channel (RadioPagingTxStream), a block ahead of the modulator:
//   Idle      no carrier; a transmission starts when the stream has work
//   RampUp    the first symbols, power rising over the ramp range
//   Tx        the symbols
//   RampDown  after the last symbol, power falling over the ramp range
// One-shot: the carrier is on for each transmission. Network: one transmission without end,
// with idle fill. A change of protocol, mode or of the FLEX mixing option ends the transmission (ramp down) and drops
// what is queued.
// Muting outputs no samples. POCSAG holds its state (a transmission resumes when unmuted);
// FLEX ends the transmission, which starts again with the re-synchronization when unmuted,
// since its frames must keep their time.
class RadioPagingModSource : public ChannelSampleSource
{
public:
    //! A message to transmit, its speed and polarity resolved. FLEX: the mode is that of the
    //! network (the settings)
    struct Request
    {
        quint64 m_id = 0;
        RadioPagingModSettings::Protocol m_protocol = RadioPagingModSettings::Pocsag;
        RadioPagingPocsag::TxMessage m_pocsag;
        int m_pocsagBaud = 1200;
        bool m_pocsagInverted = false;
        RadioPagingPocsagTxStream::Schedule m_pocsagSchedule;   //!< Repeats and send delay (network)
        RadioPagingFlexEncoder::TxMessage m_flex;           //!< With its retransmissions and send delay (network)
        bool m_flexInverted = false;    //!< The network polarity, or the other one (frames given to it when free)
        RadioPagingGsc::TxMessage m_gsc;
        bool m_gscInverted = false;     //!< The polarity of the transmission the message goes in
        RadioPagingGscTxStream::Voice m_voice;  //!< GSC voice page: the sound, at the rate of the modulator
        bool m_live = false;            //!< GSC live voice page: the voice comes with pushLive, up to the voice length setting
    };

    enum State {
        Idle,
        RampUp,
        Tx,
        RampDown
    };

    struct Status
    {
        State m_state = Idle;
        int m_queued = 0;               //!< Messages in the stream, not in a block yet
        int m_inFlight = 0;             //!< Messages in blocks being sent
        quint64 m_currentId = 0;        //!< Of a message being sent, 0 if none
        quint64 m_lastSentId = 0;       //!< Of the last message sent, 0 if none
        quint64 m_sent = 0;             //!< Messages sent
        quint64 m_dropped = 0;          //!< Messages cleared, aborted or refused by the stream
        quint64 m_sampleCounter = 0;    //!< Channel samples produced, not counting muted ones
        bool m_ers = false;             //!< FLEX emergency re-synchronization being sent
        int m_ersMs = 0;                //!< Length of the last re-synchronization, ms
        quint64 m_ersCount = 0;         //!< Re-synchronizations sent
        bool m_liveActive = false;      //!< A live voice page is being sent
        bool m_liveOpen = false;        //!< Its alert time is over: the voice is taken
        quint64 m_liveId = 0;
        int m_systemEventFrames = 0;    //!< FLEX: frames of the System Event Notification still to send, 0 when none
        unsigned m_systemEvents = 0;    //!< Its event flags
    };

    RadioPagingModSource();
    virtual ~RadioPagingModSource();

    virtual void pull(SampleVector::iterator begin, unsigned int nbSamples);
    virtual void pullOne(Sample& sample);
    virtual void prefetch(unsigned int nbSamples) { (void) nbSamples; }

    double getMagSq() const { return m_magsq; }
    void getLevels(qreal& rmsLevel, qreal& peakLevel, int& numSamples) const
    {
        rmsLevel = m_rmsLevel;
        peakLevel = m_peakLevelOut;
        numSamples = m_levelNbSamples;
    }
    void applySettings(const QStringList& settingsKeys, const RadioPagingModSettings& settings, bool force = false);
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void setChannel(ChannelAPI *channel) { m_channel = channel; }
    int getChannelSampleRate() const { return m_channelSampleRate; }
    //! Clock of the transmission starts, milliseconds since the Unix epoch (UTC)
    void setClock(const std::function<qint64()>& clock) { m_clock = clock; }

    //! The configuration of the mixed stream (FLEX network with POCSAG) of the settings
    static RadioPagingFlexEncoder::Roaming roaming(const RadioPagingModSettings& settings);
    static RadioPagingFlexEncoder::TimeService timeService(const RadioPagingModSettings& settings);
    static RadioPagingFlexEncoder::Numbering numbering(const RadioPagingModSettings& settings);
    static RadioPagingMixTxStream::Config mixConfig(const RadioPagingModSettings& settings);

    void addRequest(const Request& request);
    //! Called (from the thread of the source, with its lock held: do not call it back) when a message
    //! was sent (true) or will not be (false)
    void setNotify(const std::function<void(quint64 id, bool sent)>& notify) { m_notify = notify; }
    //! Called when the blocks of a message are pushed to the modulator (before they are on air)
    void setNotifyTransmitted(const std::function<void(quint64 id)>& notify) { m_notifyTransmitted = notify; }
    //! Called when a message can not be transmitted at all (it does not encode, the protocol is not the channel's)
    void setNotifyFailed(const std::function<void(quint64 id)>& notify) { m_notifyFailed = notify; }
    //! Called when a copy of a message sent before (a retransmission) was sent. A copy that is not sent is not reported
    void setNotifyRepeated(const std::function<void(quint64 id)>& notify) { m_notifyRepeated = notify; }
    //! FLEX: called when the blocks of a message or of a copy (repeat) are pushed, with how it went (message number,
    //! fragments, R); before its sent or repeated call
    void setNotifyStatus(const std::function<void(quint64 id, bool repeat, const RadioPagingFlexEncoder::SentStatus& status)>& notify) {
        m_notifyStatus = notify;
    }
    //! Voice of the live voice page being sent (taken once its alert time is over)
    void pushLive(const std::vector<float>& samples);
    //! The live voice page ends: the turn-off word follows
    void endLive();
    //! Drop the messages queued
    void clear();
    //! Drop the messages queued; one-shot: end the transmission being sent (ramp down)
    void abort();
    //! FLEX network: the System Event Notification in each frame for frames (RadioPagingFlexEncoder::notifySystemEvent).
    //! False and an error for another protocol, one-shot or out of range. Cancel: events 0
    bool notifySystemEvent(unsigned events, int frames, std::string& error);
    Status getStatus() const;

private:
    struct InFlight
    {
        quint64 m_endSymbol;            //!< Symbols pushed when the block was complete
        std::vector<uint64_t> m_ids;
        std::vector<uint64_t> m_repeatIds;
    };

    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    RadioPagingModSettings m_settings;
    ChannelAPI *m_channel;
    std::function<qint64()> m_clock;

    NCO m_carrierNco;
    RadioPagingFskMod m_mod;
    Real m_linearGain;
    Complex m_modSample;

    mutable QMutex m_mutex;             //!< Stream, state and status: requests and getStatus() come from other threads
    std::unique_ptr<RadioPagingTxStream> m_stream;
    RadioPagingModSettings::Protocol m_streamProtocol;
    RadioPagingModSettings::Mode m_streamMode;
    bool m_streamMixed;                 //!< The stream is a RadioPagingMixTxStream (FLEX network mixed with POCSAG)
    int m_streamFlexMode = -1;          //!< The FLEX speed the stream was made for
    RadioPagingMixTxStream *m_mix;      //!< m_stream if mixed, else null
    bool m_streamEnded;                 //!< The transmission has no more blocks
    quint64 m_pushed;                   //!< Symbols pushed to the modulator in the transmission
    quint64 m_ersStart;                 //!< Symbols of the transmission before its re-synchronization
    quint64 m_ersEnd;                   //!< Symbols up to its end; equal to start if none
    std::deque<InFlight> m_inFlight;
    Status m_status;
    State m_state;
    Real m_pow;                         //!< Ramp, dB
    Real m_powRamp;                     //!< dB per sample

    double m_magsq;
    MovingAverageUtil<double, double, 16> m_movingAverage;

    quint32 m_levelCalcCount;
    qreal m_rmsLevel;
    qreal m_peakLevelOut;
    Real m_peakLevel;
    Real m_levelSum;
    static const int m_levelNbSamples = 480;    // 10 ms at 48 kHz

    std::function<void(quint64, bool)> m_notify;
    std::function<void(quint64)> m_notifyTransmitted;
    std::function<void(quint64)> m_notifyFailed;
    std::function<void(quint64)> m_notifyRepeated;
    std::function<void(quint64, bool, const RadioPagingFlexEncoder::SentStatus&)> m_notifyStatus;
    void fail(const std::vector<uint64_t>& ids);
    void drop(const std::vector<uint64_t>& ids);
    void makeStream();
    bool mixing() const;
    RadioPagingModSettings::Mode effectiveMode() const;
    void applyStreamDefaults();
    void dropInFlight();
    void endTransmission();
    void startTransmission();
    void feed();
    void complete();
    void modulateSample();
    void calculateLevel(Real sample);
    Real rampStep(Real ms) const;
};

#endif // INCLUDE_RADIOPAGINGMODSOURCE_H
