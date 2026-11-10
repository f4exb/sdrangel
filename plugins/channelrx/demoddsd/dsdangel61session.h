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

#ifndef INCLUDE_DSDANGEL61SESSION_H
#define INCLUDE_DSDANGEL61SESSION_H

#include <complex>
#include <cstdint>
#include <map>
#include <string>
#include <set>
#include <vector>

#include "dsp/dv/dvangel61.h"
#include "dsp/dv/dvangel61file.h"
#include "dsp/dv/dvangel61hc.h"
#include "dsp/dv/dvangel61ip.h"

#include "dsdprotocol.h"

/**
 * What an Angel61 revision 3 transmission carries (sdrbase/dsp/dv/ANGEL61.md section 5), from
 * the frames a receiver confirms: the base of the DSD Demodulator's revision 3 branches, HF
 * (DSDAngel61Session, the OFDM receiver) and VHF (DSDAngel61Vhf, DSDAngel61Receiver). A branch feeds it
 * IQ at 48 kS/s (process) and its receiver's frames (frameHeard, confirmed), and chooses the rate
 * rule (judgeRate). No Qt: the harness drives it.
 *
 * - **A transmission** starts with its first confirmed frame (header and payload through the CRC-12,
 *   which covers the header: header fields are acted on only then, 2.4) and ends with a confirmed
 *   last-frame flag, or when for EndAfterFrames frame periods nothing is confirmed and no header of
 *   the profile is heard. Frames whose header was heard keep it going while their payloads fail, so
 *   that a tier failing outright is judged, not split into a preamble and a last frame.
 * - **Frames lost** are the gaps in the confirmed frames' counters and the frames heard after the
 *   last confirmed one; a data group (2.3) is confirmed as its four frames at once, with its last
 *   frame. Those heard before the first confirmed one (within EndAfterFrames periods of it) count
 *   only when no voice or data frame is confirmed: a receiver acquiring loses the first frames of
 *   a clean transmission, which says nothing of the tier, but a transmission whose preamble and
 *   voice all failed is judged on them.
 * - **The slots** give the routing, path, station, position, long callsigns and slot text; the
 *   message fields (voice with data, and the data groups) give messages through revision 2's
 *   reassembler: text, APRS, file transfer (3.4: offers, blocks and answers, through revision 2's
 *   file receiver; a block is one group's field) and IP tunnelling (3.5, 3.6: datagrams checked and
 *   decompressed per source, tunnel control), as revision 2's session does them.
 * - **Voice** (V1, 3.1.1): each voice frame gives a DSDVoiceFrame of its packets (6 VHF, 18 HF) for
 *   the sink's V1 decoder; within a voice transmission a lost frame, or an idle frame filling a pause,
 *   is an erased one (its packets concealed, lost audio keeps its time), and the end is a flush.
 * - **The rate request** (2.10): per correspondent from its transmissions of at least
 *   minimumFrames frames, the block error rate, the most consecutive frames lost and the mean and
 *   least SNR; the branch's rule decides (HF: the mean SNR3k, with holds; VHF: revision 2's, the
 *   least Es/N0). A send tier older than a minute, or of another profile, is back to 0. The tier
 *   judged is the last confirmed voice or data frame's; with none confirmed, the tier of the voice or
 *   data headers heard (HeardTierFrames in a row agreeing, and the station slot's, 2.5, if heard).
 */
class DSDAngel61Session
{
public:
    static const int SampleRate = 48000;
    static const int HeardTierFrames = 3;           //!< agreeing headers that judge a tier nothing of which was confirmed
    static const int EndAfterFrames = 8;            //!< frame periods without a confirmed frame that end a transmission (HF 1.9 s, VHF 0.64 s)
    static constexpr float DowngradeBler = 0.04f;
    static const int DowngradeConsecutive = 3;
    static constexpr float UpgradeBler = 0.01f;
    static constexpr float UpgradeMarginDB = 3.0f;
    static const int HoldStartMs = 60000;
    static const int HoldMaxMs = 16 * 60000;
    static const int ExpiryMs = 60000;               //!< a send tier older than this is back to 0 (as revision 2)

    struct Message
    {
        uint64_t m_source = 0;
        uint64_t m_destination = 0;
        int m_contentType = 0;
        std::string m_text;
        bool m_fromSlots = false;
        bool m_dtmf = false;         //!< the keys of a DTMF sequence (3.8), '?' for one lost
        int64_t m_sample = 0;
    };

    /**
     * A transmission as it ended, for the sink's events and contact reports: the session's own state
     * is the next transmission's once it begins, which can be in the same block (one station right
     * after another)
     */
    struct Ended
    {
        bool m_hasRouting = false;
        DVAngel61::Routing m_routing;
        bool m_hasPosition = false;
        DVAngel61::Position m_position;
        int m_framesExpected = 0;
        int m_framesLost = 0;
        int m_voiceFrames = 0;
        float m_meanSnrDB = 0.0f;
        float m_minSnrDB = 0.0f;
        int m_textMessages = 0;         //!< messages delivered, DTMF aside
        int64_t m_beginSample = 0;
        int64_t m_endSample = 0;
    };

    //! A DTMF key to play (3.8): released in its sequence's order, once
    struct DTMFKey
    {
        char m_key = 0;
        int m_toneMs = 100;
        int m_gapMs = 100;
        int64_t m_sample = 0;
    };
    static const int DTMFSequenceEndMs = 2000;      //!< a sequence has ended when no new key came for this long
    static const int DTMFMaxKeys = 64;              //!< keys held for a sequence (3.8)
    static const int DTMFMaxText = 256;             //!< a sequence's text (a long transmission may repeat keys without end)
    static const int DTMFSkipFrames = 4;            //!< frame periods a later key waits before a missing one is given up

    struct PositionReport
    {
        uint64_t m_source = 0;
        DVAngel61::Position m_position;
        int64_t m_sample = 0;
    };

    struct Correspondent
    {
        uint64_t m_callsign = 0;
        uint64_t m_destination = 0;
        DVAngel61::Profile m_profile = DVAngel61::ProfileHFNB;
        int m_requestedTier = 0;     //!< the tier its last confirmed frame asked of its destination
        int m_sendTier = 0;          //!< the tier this station would ask of it
        int m_heardTier = -1;        //!< the tier of its last judged transmission (-1: none judged)
        float m_blockErrorRate = 0.0f;
        float m_meanSnrDB = 0.0f;    //!< over its last transmission's frames whose header was accepted
        float m_minSnrDB = 0.0f;     //!< the least of them
        int m_frames = 0;
        int m_maxConsecutiveLost = 0;
        int64_t m_sample = 0;        //!< when its last transmission ended
        int m_holdFailures[4] = { 0, 0, 0, 0 };        //!< consecutive failures of each tier
        int64_t m_holdUntil[4] = { -1, -1, -1, -1 };   //!< sample until which a raise to the tier is held
    };

    /**
     * frameSamples: the profile's frame at 48 kS/s; minimumFrames: the shortest transmission that
     * judges the rate (about a second)
     */
    DSDAngel61Session(DVAngel61::Profile profile, int frameSamples, int minimumFrames);
    virtual ~DSDAngel61Session() {}
    DSDAngel61Session(const DSDAngel61Session&) = delete;
    DSDAngel61Session& operator=(const DSDAngel61Session&) = delete;

    DVAngel61::Profile getProfile() const { return m_profile; }
    //! The receiver and the session afresh
    virtual void reset();
    //! IQ at 48 kS/s
    virtual void process(const std::complex<float> *iq, int n) = 0;
    //! n samples not looked at (an FM decoder holds the channel): time passes, nothing is heard
    virtual void skip(int n) { advance(n); }
    virtual bool isLocked() const = 0;
    virtual int getAcquisitions() const = 0;
    //! The constellation display: the receiver's equalised payload symbols (unit mean power) of confirmed frames
    virtual void setScopeEnabled(bool enable) { (void) enable; }
    virtual std::vector<std::complex<float>> takeScopeSymbols() { return {}; }

    bool isActive() const { return m_active; }
    int64_t getSample() const { return m_sample; }

    // The transmission's metadata (the last transmission's, once it has ended)
    bool hasRouting() const { return m_hasRouting; }
    const DVAngel61::Routing& getRouting() const { return m_routing; }
    bool hasPath() const { return m_hasPath; }
    const DVAngel61::Path& getPath() const { return m_path; }
    bool hasStation() const { return m_hasStation; }
    const DVAngel61::Station& getStation() const { return m_station; }
    bool hasPosition() const { return m_hasPosition; }
    const DVAngel61::Position& getPosition() const { return m_position; }
    int getFrameType() const { return m_lastType; }
    int getRequestedTier() const { return m_requestedTier; }
    //! The tier this station would ask of the current transmission's source
    int getSendTier() const;
    int getFramesExpected() const { return m_framesExpected; }
    int getFramesLost() const { return m_framesLost; }
    int getVoiceFrames() const { return m_voiceFrames; }
    int getMaxConsecutiveLost() const { return m_maxConsecutiveLost; }
    float getBlockErrorRate() const { return m_framesExpected ? (float) m_framesLost / m_framesExpected : 0.0f; }
    float getMeanSnrDB() const;
    float getMinSnrDB() const { return m_snrCount ? m_minSnrDB : 0.0f; }
    float getFrequencyHz() const { return m_frequencyHz; }
    //! Message fields of a reserved content type (8 to 15)
    int getUnhandledMessages() const { return m_unhandledMessages; }

    // ---- File transfer (3.4), as revision 2's session ----------------------------------------
    //! The largest file this station takes, 0 for no limit
    void setMaxFileSize(uint32_t bytes) { m_fileReceiver.setMaxFileSize(bytes); }
    const DVAngel61FileReceiver& getFileReceiver() const { return m_fileReceiver; }
    //! A file that arrived complete, taken once
    bool takeCompletedFile(uint64_t& source, std::string& name, std::vector<uint8_t>& data);
    //! The answer this station owes the transmitter of a file, at the end of a transmission
    bool takeFileControl(DVAngel61FileControl& control) { return m_fileReceiver.nextControl(control); }
    //! The operator confirms every offer before its file is kept (3.4)
    void setFileConfirm(bool confirm) { m_fileReceiver.setConfirm(confirm); }
    //! The operator's decision on a file awaiting it: accepted (its answer is due), or refused
    void decideFile(bool accept)
    {
        if (!m_fileReceiver.isAwaitingConfirmation()) {
            return;
        }
        if (accept) {
            m_fileReceiver.accept();
        } else {
            m_fileReceiver.reject(DVAngel61FileControl::ReasonOperator);
        }
    }
    //! The unfinished transfers a station keeps for a resume (3.4), and this one's kept there
    void setFilePartials(DVAngel61FilePartials *partials) { m_fileReceiver.setPartials(partials); }
    void keepFilePartial() { m_fileReceiver.keepPartial(); }
    //! Control messages heard from other stations, for a file this instance is transmitting
    std::vector<DVAngel61FileControl> takeHeardFileControls();
    //! Refuse the transfer being received (the operator)
    void rejectFile(int reason) { m_fileReceiver.reject(reason); }

    // ---- IP tunnelling (3.5, 3.6), as revision 2's session -----------------------------------
    std::vector<std::pair<uint64_t, std::vector<uint8_t>>> takeDatagrams();
    std::vector<std::pair<uint64_t, DVAngel61IP::Control>> takeTunnelControls();
    std::vector<std::pair<uint64_t, DVAngel61IP::Control>> takeCompressionControls();
    bool hasPeerTunnel() const { return m_peerTunnelValid; }
    const DVAngel61IP::Control& getPeerTunnel() const { return m_peerTunnel; }
    int getDatagramsRefused() const { return m_datagramsRefused; }
    int getDatagramsDiscarded() const { return m_datagramsDiscarded; }
    //! "SOURCE > DEST | mode | BLER | SNR | request": the mean SNR3k on HF, the least Es/N0 on VHF
    std::string getStatusText() const;

    std::vector<Message> takeMessages();
    //! DTMF keys to play, in order (3.8)
    std::vector<DTMFKey> takeDTMFKeys();
    //! The keys of the DTMF sequence arriving ('?' for a key lost), empty when none is
    const std::string& getDTMFText() const { return m_dtmfText; }
    //! The voice for the sink (V1 frames, erasures and the end's flush), in order
    std::vector<DSDVoiceFrame> takeVoice();
    std::vector<PositionReport> takePositions();
    std::vector<std::string> takeAPRSPackets();
    //! Correspondents whose record changed (a transmission of theirs ended)
    std::vector<Correspondent> takeCorrespondentUpdates();
    /**
     * The transmission's frames for a gateway (plan C7), in order, as DVAngel61::packRecord gives
     * them: one a frame period, so their counters run on. A frame lost is an idle frame with its
     * counter (a relay sends silence for it), a data group's four frames are four idle ones (not
     * relayed), and a transmission that ends without its last frame confirmed ends with an idle frame
     * flagged last. The caller takes them as they come; at most MaxGatewayRecords wait.
     */
    std::vector<std::vector<uint8_t>> takeGatewayRecords();
    static const int MaxGatewayRecords = 64;
    /**
     * The messages for a gateway that no record carries (plan C7): a text, binary or APRS message
     * completed from HF data groups (a group's block is beyond a record), as
     * DVAngel61::packRelayMessage gives it, up to its MaxRelayMessageBytes. A message of single
     * frames goes in their records. At most MaxGatewayMessages wait.
     */
    std::vector<std::vector<uint8_t>> takeGatewayMessages();
    static const int MaxGatewayMessages = 16;
    //! The stations kept by received callsign (their rate records, their compression contexts): each
    //! transmission may name a new one, so the least recently heard goes beyond these
    static const int MaxCorrespondents = 256;
    static const int MaxDecompressorSources = 32;
    const std::map<uint64_t, Correspondent>& getCorrespondents() const { return m_correspondents; }
    //! Transmissions begun and ended since the last call (for the sink's events)
    int takeBegins() { int n = m_begins; m_begins = 0; return n; }
    int takeEnds() { int n = m_ends; m_ends = 0; return n; }
    //! The transmissions that ended since the last call, as they were (at most MaxEnded kept)
    std::vector<Ended> takeEnded() { std::vector<Ended> e; e.swap(m_ended); return e; }
    static const int MaxEnded = 16;

protected:
    //! The rate rule on a transmission judged at `tier` that ended at `sample` (the record's
    //! error rate, consecutive losses and SNRs are filled in): the correspondent's send tier
    virtual void judgeRate(Correspondent& c, int tier, int64_t sample) = 0;

    // What a branch's receiver gives, in order for each frame:
    //! A frame period the receiver decided: its SNR counts (dB; its measure is the branch's) when its
    //! header was accepted; the carrier offset it tracks; the accepted header, which keeps the
    //! transmission going while payloads fail (never acted on: 2.4)
    void frameHeard(bool headerOk, float snrDb, float frequencyHz, const DVAngel61::Header *header = nullptr);
    //! A confirmed unit: one frame, or a data group's four; the first counter and the last header
    //! channelErrors, channelBits: what the payload's code corrected, on a voice frame the BER shown
    void confirmed(const DVAngel61::Header& header, int firstCounter, int frames, const uint8_t *block, int64_t sample,
        int channelErrors = 0, int channelBits = 0);
    //! After a block of samples: n more, the transmission ended if nothing was confirmed for a while
    void advance(int n);

    DVAngel61::Profile m_profile;
    int m_frameSamples;
    int m_minimumFrames;
    int64_t m_sample;

    bool m_active;
    int64_t m_lastConfirmedSample;
    int64_t m_lastHeardSample;      //!< the last header of the profile heard while active
    int m_heardUnconfirmed;         //!< frames whose header was heard since the last confirmed unit
    int m_heardTier;                //!< the tier of the voice or data headers heard, -1 for none
    int m_heardTierFrames;          //!< how many of them agree on it
    int64_t m_preHeardSample;       //!< the same while no transmission is active: frames heard before one begins
    int m_preHeard;
    int m_preHeardTier;
    int m_preHeardTierFrames;
    int m_startLost;                //!< the frames heard before the first confirmed one, and their tier
    int m_startTier;
    int m_startTierFrames;
    std::deque<std::vector<uint8_t>> m_gatewayRecords;
    std::deque<std::vector<uint8_t>> m_gatewayMessages;
    bool m_fieldFromGroup = false;      //!< the message field being processed is a data group's
    bool m_gatewayLastSent;         //!< the transmission's last frame is in them
    //! A frame period for the gateway: the frame, or an idle one with the counter (a loss)
    void gatewayRecord(const DVAngel61::Header& header, const uint8_t *block);
    void gatewayIdle(int counter, bool last);
    bool m_hasRouting, m_hasPath, m_hasStation, m_hasPosition;
    DVAngel61::Routing m_routing;
    DVAngel61::Path m_path;
    DVAngel61::Station m_station;
    DVAngel61::Position m_position;
    int m_lastType;
    int m_lastCounter;
    int m_requestedTier;
    int m_judgedTier;               //!< the tier of the transmission's last voice or data frame, -1 for none
    int m_framesExpected;
    int m_framesLost;
    int m_voiceFrames;
    int m_maxConsecutiveLost;
    double m_snrSum;                //!< linear SNR of the frames whose header was accepted
    int m_snrCount;
    float m_minSnrDB;
    double m_frameSnr = -1.0;       //!< the frame being processed's linear SNR, -1 when its header was not accepted
    float m_frequencyHz;
    int m_unhandledMessages;
    int m_begins, m_ends;
    std::vector<Ended> m_ended;
    int64_t m_beginSample = 0;
    int m_textMessages = 0;

    DVAngel61Reassembler m_reassembler;
    DVAngel61FileReceiver m_fileReceiver;
    bool m_fileDelivered = false;
    std::vector<DVAngel61FileControl> m_heardControls;
    std::vector<std::pair<uint64_t, std::vector<uint8_t>>> m_datagrams;
    std::vector<std::pair<uint64_t, DVAngel61IP::Control>> m_tunnelControls;
    std::vector<std::pair<uint64_t, DVAngel61IP::Control>> m_compressionControls;
    DVAngel61IP::Control m_peerTunnel;
    bool m_peerTunnelValid = false;
    std::set<uint64_t> m_repairingSources;                   //!< stations whose advertise says they answer a repair (3.5)
    std::set<std::pair<uint64_t, int>> m_repairsRequested;   //!< one round per datagram: asked for already
    void requestRepairs(int64_t sample);
    int m_datagramsRefused = 0;
    int m_datagramsDiscarded = 0;
    std::map<uint64_t, DVAngel61HC::Decompressor> m_decompressors;   //!< per source: its contexts outlive a transmission
    std::map<uint64_t, int64_t> m_decompressorUsed;                  //!< per source: the sample it was last used at
    std::vector<Message> m_messages;
    std::vector<DSDVoiceFrame> m_voice;
    bool m_voiceStarted = false;    //!< the transmission has had a voice frame: its losses and pauses are concealed
    DSDVoiceFrame::Rate m_voiceRate = DSDVoiceFrame::RateVocos;   //!< V1, or Opus (WB tier 3): what a loss conceals
    //! V1 frames of `frames` periods lost (or a pause) in the voice: their packets erased
    void voiceErasures(int frames, int64_t sample);
    //! A voice frame's packets for the decoder
    void voiceFrame(int type, const uint8_t *block, int64_t sample, int channelErrors, int channelBits);
    std::vector<PositionReport> m_positions;
    std::vector<std::string> m_aprsPackets;

    // DTMF (3.8): the sequence arriving, its keys received ahead of the next to release
    struct DTMFReceived { char m_key; int m_toneMs; int m_gapMs; int64_t m_sample; };
    uint64_t m_dtmfSource = 0;
    int m_dtmfSequence = -1;
    int m_dtmfNext = 0;
    std::map<int, DTMFReceived> m_dtmfReceived;     //!< by index (mod 64)
    std::string m_dtmfText;
    int64_t m_dtmfLastSample = -1;                  //!< when its last new key came
    std::vector<DTMFKey> m_dtmfKeys;
    void dtmfSlot(const DVAngel61::DTMF& dtmf, int64_t sample);
    //! Release the keys in order; a missing one is skipped once a later one has waited DTMFSkipFrames
    //! (every key goes twice, frames apart), or at once when flush
    void dtmfRelease(int64_t sample, bool flush);
    //! The sequence's keys as a message, when it has any
    void dtmfEnd();
    std::map<uint64_t, Correspondent> m_correspondents;
    std::vector<Correspondent> m_correspondentUpdates;

    void countSnr();
    void processSlot(const uint8_t *slot, int64_t sample);
    void processMessageField(const DVAngel61::MessageFragment& fragment, int64_t sample);
    void deliver(const Message& message);
    void emitAPRS(const std::string& info);
    void begin(int64_t sample);
    void end(int64_t sample);
};

#endif // INCLUDE_DSDANGEL61SESSION_H
