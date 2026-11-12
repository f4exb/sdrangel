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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODANGEL61_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODANGEL61_H_

#include <deque>
#include <map>

#include "dsp/dv/dvangel61file.h"
#include "dsp/dv/dvangel61hc.h"
#include "dsp/dv/dvangel61ip.h"
#include "dsp/dv/dvangel61.h"

#include "dsemodprotocol.h"
#include "dsemodangel61data.h"

/**
 * Angel61 revision 3 (sdrbase/dsp/dv/ANGEL61.md, sections 2 and 4; plan items C4 and C5): the
 * frames of every profile. NB and WB go to the processor's linear path (nextSymbols, revision 2's
 * pulses), HF-NB and HF-WB to its OFDM path (nextCells, DVAngel61OFDM).
 *
 * **A voice transmission** (2.8): the two-frame preamble of idle frames, voice frames at the
 * transmission's tier, then an idle frame with the last-frame flag. A voice frame carries 6 (VHF) or
 * 18 (HF) V1 packets (3.1.1), pulled from the frame source as the encoder made them (16 codebooks,
 * DSEModVocoder::RateVocos) and truncated to the frame kind's codebook count; silence packets when
 * the encoder has none.
 *
 * **Data under voice** (2.3, 2.8): while message-field traffic is queued a voice frame goes as its
 * tier's one-step kind, its packets a step lower and the field carrying the next fragment. A voice
 * transmission is never lengthened for data.
 *
 * **A data transmission** (a message with no speech input, a file, a datagram, API data): the
 * preamble, data frames until nothing is left, the last frame. On VHF single data frames, QPSK, or
 * 16-QAM at tier 3 (2.8); on HF 4-frame data groups at the tier, starting at the counters c with c mod
 * 4 = 2 (2.3). The fields carry text and API messages, APRS, file transfer (the offer, blocks of the
 * field's size, the rounds the answers ask for) and IP tunnelling (datagrams, header compression,
 * tunnel control), with revision 2's queue rules.
 *
 * **Tiers**: the rate setting, or on Auto what the destination asked of this station. WB's tier 3 is
 * audio, not speech: Opus (family O1, 3.1.2), one 80 ms packet of 184 bytes a frame from 48 kHz audio
 * (DSEModVocoder::RateOpus80), with no data under it (2.8).
 *
 * The slots come from the revision 3 slot queue with the profile's periods (routing every other frame
 * on VHF and every third on HF, station and parameters, position, path, long callsigns, text).
 */
class DSEModAngel61 : public DSEModProtocol, public DSEModAngel61Data
{
public:
    explicit DSEModAngel61(DVAngel61::Profile profile);

    virtual void start(const DSEModSettings& settings);
    //! A relay with relayed text or messages still to send goes on until they are out (drainRelay)
    virtual void stop();
    virtual void abort();
    virtual bool isActive() const { return m_state != StateIdle; }
    virtual bool nextFrame(std::vector<int8_t>& levels) { (void) levels; return false; }
    virtual bool nextSymbols(std::vector<std::complex<float>>& symbols);
    virtual bool nextCells(std::vector<std::complex<float>>& cells, int& constellation);
    virtual bool queueMessage(const std::string& text);
    virtual void setPosition(bool valid, double latitude, double longitude, float altitude, int speed = -1, int course = -1, int source = 1);
    virtual void setLinkState(int requestedTier, int sendTier) { m_linkTier = requestedTier & 3; m_requestTier = sendTier & 3; }

    virtual DSEModSettings::DSEModMode getMode() const;
    //! V1, or Opus for WB's tier 3 (known once start() has chosen the tier)
    virtual DSEModVocoder::Rate getVocoderRate() const { return isOpusTier() ? DSEModVocoder::RateOpus80 : DSEModVocoder::RateVocos; }
    //! WB takes 48 kHz until a transmission opens its encoder, as its tier 3 may be Opus
    virtual int getInputSampleRate() const { return m_profile == DVAngel61::ProfileWB ? 48000 : 24000; }
    virtual Shaping getShaping() const { return isHF() ? ShapingOFDM : ShapingLinear; }
    virtual int getOFDMProfile() const { return m_profile; }
    //! A transmission that may carry a 16-QAM frame is at 16-QAM's level throughout (as revision 2)
    virtual float getLinearLevel() const;
    /**
     * The shaper's RMS for each constellation, of full scale: the I and Q peaks of the shaped signal
     * are 1.37 (QPSK) and 1.79 (16-QAM) times its RMS, so these put them at 0.89 of full scale,
     * -1 dBFS, where the FM modes' constant envelope is. The envelope's peaks are higher (1.24 and
     * 1.16 of full scale): the RF amplifier sees 2.8 and 2.3 dB more peak than FM's at the same
     * gain, the back-off a linear modulation needs (ANGEL61.md 2.1, output power by mode)
     */
    static constexpr float QPSKLevel = 0.65f;
    static constexpr float QAM16Level = 0.5f;
    //! A message and no speech input: a data only transmission
    static bool wantsDataOnly(const DSEModSettings& settings);
    virtual int getLinearSamplesPerSymbol() const { return m_profile == DVAngel61::ProfileWB ? 5 : 10; }
    virtual const char *getName() const;
    //! A data transmission, or one for DTMF alone: no speech to encode
    virtual bool isDataOnly() const { return m_dataOnly || m_dtmfOnly; }
    //! Encoded frames (V1 or Opus packets) consumed in the transmission
    virtual uint32_t getVoiceFrameCount() const { return m_packetsSent; }
    //! A voice frame takes a frame's packets at once (Opus: its one)
    virtual int getFramesPerBlock() const { return isOpusTier() ? 1 : DVAngel61::packetsPerFrame(m_profile); }
    //! The preamble's two idle frames take no packets, less a little margin for the encoder's 20 ms
    //! input, which makes one packet then two: primed without the credit, the queue sat that much deep
    //! and the delay control trimmed packets out of the first seconds of speech
    virtual int getPrimeCredit() const { return isOpusTier() ? 2 : 2 * DVAngel61::packetsPerFrame(m_profile) - 2; }

    // ---- DSEModAngel61Data ----------------------------------------------------------------------
    virtual void setAPRS(const std::string& info) { m_apiData.assign(info.begin(), info.end()); m_apiContentType = DVAngel61::ContentAPRS; }
    virtual void setDataMessage(const std::vector<uint8_t>& data, int contentType) { m_apiData = data; m_apiContentType = contentType; }
    virtual bool startFile(const std::string& name, const std::vector<uint8_t>& data) { return startFile(name, data, std::string()); }
    bool startFile(const std::string& name, const std::vector<uint8_t>& data, const std::string& mediaType);
    virtual void handleFileControl(const DVAngel61FileControl& control) { m_fileSender.handleControl(control); }
    virtual bool queueFileControl(const DVAngel61FileControl& control, const std::string& destination);
    virtual bool fileNoAnswer() { return m_fileSender.handleNoAnswer(); }
    //! HF: the offer every this many data groups (about 8 s on HF-NB): a field is a whole group
    static constexpr int HFOfferIntervalGroups = 8;
    //! A transmission keyed up for the tunnel is no longer than this, so that the peer can answer (3.5)
    static constexpr int MaxTunnelBurstMs = 30000;
    //! The advertise goes at the head of a transmission this long after the last (3.5)
    static constexpr int TunnelAdvertiseMs = 30000;
    //! A repair request is answered this long after the transmission it is for (TunnelRepairWindow, 3.5)
    static constexpr int TunnelRepairWindowMs = 10000;
    int getRepairsSent() const { return m_repairsSent; }
    virtual const DVAngel61FileSender& getFileSender() const { return m_fileSender; }
    virtual bool fileWantsTransmission() const { return m_fileSender.getState() == DVAngel61FileSender::Sending; }
    virtual bool queueDatagram(const std::vector<uint8_t>& datagram);
    virtual bool queueTunnelControl(const DVAngel61IP::Control& control);
    virtual void setTunnel(bool enabled, const DVAngel61IP::Control& advertise);
    virtual void handlePeerTunnelControl(const DVAngel61IP::Control& control);
    virtual bool queueDTMF(const std::string& keys, int toneMs, int gapMs, bool newSequence, bool hold, bool only);
    /**
     * A relay (C7): the keys of a DTMF slot of a frame it relays, each once (the source's windows come
     * twice and overlap), under this station's own sequence: a new sequence of the source is a new one
     * here, and a key whose slots were lost is skipped
     */
    void relayDTMF(const DVAngel61::DTMF& dtmf);
    //! The next start() is a relay's (plan C7): relayFrame then gives it the call's frames
    void setRelay() { m_relayNextStart = true; }
    /**
     * An API session is over: what it left goes - its tier cap, its relay, its data, the caller's text,
     * messages and keys - and this station's own stays: the tunnel and its datagrams, a file transfer,
     * the position. (The processor made a new protocol instead, which lost the second as well: the
     * tunnel was off until a setting changed, and a file transfer was dropped)
     */
    void endSession();
    bool isRelaying() const { return m_relaying; }
    /**
     * A relay (C7): a frame of the call it relays (a record's header and block), whatever its kind.
     * Its DTMF (relayDTMF); its text slots and message field, reassembled (3.2) and queued again in
     * this station's own slots and fields, as the profiles' fields differ: text, binary and APRS
     * messages (a file transfer's and the tunnel's are not, as their answers cannot come back, 2.6);
     * its position slot, the caller's position in this station's slots (a relay never sends its own).
     * The voice is the caller's (DSEModProcessor::relayAngel61API)
     */
    void relayFrame(const DVAngel61::Header& header, const std::vector<uint8_t>& block);
    //! A relay (C7): a whole message of the call (a data group's, which no record carries), queued as relayFrame's are
    void relayMessage(int contentType, int flags, const std::vector<uint8_t>& message);
    int getRelayedTexts() const { return m_relayedTexts; }
    int getRelayedMessages() const { return m_relayedMessages; }
    //! Idle frames a relay sent in its call for want of the call's packets (voiceBlock)
    int getRelayHoldFrames() const { return m_relayHoldFrames; }
    static constexpr int MaxRelayedMessages = 16;   //!< A relay's messages waiting at most
    static constexpr int MaxOtherFields = 8;        //!< Fields a part sent message lets go by before it is fragmented again
    virtual void setDataOnlyNext(bool forMessage) { m_dataOnlyNext = true; m_dataOnlyNextMessage = m_dataOnlyNextMessage || forMessage; }
    //! A relay goes on after its call's last frame for its text and messages at most this long
    static constexpr int RelayDrainMs = 15000;
    int getDTMFSlotsSent() const { return m_slots.getDTMFSlotsSent(); }
    bool tunnelWantsTransmission() const { return m_tunnelEnabled && !m_messages.empty(); }
    int getTunnelDropped() const { return m_tunnelDropped; }
    const DVAngel61HC::Compressor& getCompressor() const { return m_compressor; }

    DVAngel61::Profile getProfile() const { return m_profile; }
    bool isHF() const { return DVAngel61::isHF(m_profile); }
    //! The transmission's voice is Opus (WB tier 3)
    bool isOpusTier() const { return DVAngel61::isOpus(DVAngel61::voiceType(m_profile, m_tier)); }
    //! The tier a transmission with these settings uses: the rate setting, or on Auto what the
    //! destination asked of this station (Low 0, Medium 1, High 2, Tier3 3), at most the cap
    int tierOf(const DSEModSettings& settings) const;
    //! These settings would make a transmission other than the one running: another tier, or
    //! another source, destination or path (as encoded: case and spaces aside)
    bool transmissionChanges(const DSEModSettings& settings) const;
    /**
     * A relay (plan C7): the highest tier of `profile` that sends a call arriving as `inputType`
     * (a voice type of any profile) without raising its rate, since a relay only truncates (3.1.1):
     * the tier's V1 codebooks no more than the input's, never WB's Opus tier from V1. An Opus input
     * allows any tier (WB's tier 3 passes it; a V1 tier needs it decoded and encoded again). -1
     * when even the lowest tier needs more codebooks than the input carries.
     */
    static int relayTierCap(DVAngel61::Profile profile, int inputType);
    //! The highest tier start() may choose (a relay's cap), 3 for none
    void setTierCap(int tier) { m_tierCap = std::max(0, std::min(3, tier)); }
    int getTier() const { return m_tier; }
    //! The data frame type of a data transmission at a tier: HF's group of the tier, VHF's data frame
    //! (16-QAM at tier 3)
    int dataType(int tier) const;
    //! The message field's payload bytes of a tier's data frames (or group)
    int fieldBytes(int tier) const { return DVAngel61::messageFieldBytes(dataType(tier)); }
    //! The largest datagram a tier's data frames carry (255 fragments, the 16-bit length first)
    int maxDatagram(int tier) const { return 255 * fieldBytes(tier) - 2; }
    int getFrameCount() const { return m_frameCount; }
    //! The transmission's routing (tests: a tunnel's goes to its peer)
    const DVAngel61::Routing& getRouting() const { return m_routing; }
    int getGroupsSent() const { return m_groupsSent; }
    int getDataFramesSent() const { return m_dataFramesSent; }
    int getVoiceFramesSent() const { return m_voiceFramesSent; }
    int getVoiceDataFramesSent() const { return m_voiceDataFramesSent; }
    //! The type of the last frame built
    int getLastType() const { return m_lastType; }

private:
    enum State { StateIdle, StatePreamble, StateRun };

    DVAngel61::Profile m_profile;
    DVAngel61SlotQueue m_slots;
    DVAngel61::Routing m_routing;
    DVAngel61::Position m_position;
    DSEModSettings m_settings;
    bool m_positionValid;
    State m_state;
    bool m_stopRequested;
    bool m_dataOnly;
    int m_counter;
    int m_frameCount;
    int m_groupsSent;
    int m_dataFramesSent;
    int m_voiceFramesSent;
    int m_voiceDataFramesSent;
    uint32_t m_packetsSent;
    int m_lastType;
    int m_tier;
    int m_linkTier;                  //!< the tier the destination asked of this station (Auto)
    int m_requestTier;               //!< the tier this station asks of the destination (every header)
    int m_tierCap;                   //!< the highest tier start() may choose (a relay's input)

    // HF: the group being sent, its four frames' cells, frame by frame
    std::vector<std::complex<float>> m_groupCells;
    int m_groupFrame;                //!< the next of its frames to send, GroupFrames when none
    int m_groupConstellation;

    // Messages for the field, revision 2's queue: fragmented for the field that carries them
    struct PendingMessage
    {
        std::vector<uint8_t> m_bytes;
        int m_contentType = 0;
        int m_flags = 0;
        int m_id = 0;
        int m_repeats = 1;
        int m_fieldBytes = 0;
        std::vector<DVAngel61::MessageFragment> m_fragments;
        size_t m_next = 0;
        int m_otherFields = 0;      //!< Fields of another size since its last fragment (nextMessageField)
        bool m_repair = false;      //!< a repair's fragments, as they were sent: never fragmented again (3.5)
    };
    //! The datagrams of the last transmission, as sent, for a repair (3.5): message id -> fragments
    struct SentDatagram
    {
        int m_fieldBytes = 0;
        std::vector<DVAngel61::MessageFragment> m_fragments;
    };
    std::map<int, SentDatagram> m_sentDatagrams;
    int64_t m_endedMs;              //!< steady clock: the last transmission's end, for the repair window
    int m_repairsSent;
    std::deque<PendingMessage> m_messages;
    std::deque<PendingMessage> m_heldMessages;   //!< this station's own (datagrams, tunnel control), set aside while it relays
    int m_messageId;
    std::vector<uint8_t> m_apiData;
    int m_apiContentType;
    //! About 30 s of data: past that a tunnel's datagrams are dropped here, not aged on the air (3.5)
    static constexpr int MaxTunnelFragments = 32;
    int m_burstFrames;                  //!< the frames a tunnel's transmission may last, 0 for no limit
    int64_t m_lastAdvertiseMs;          //!< steady clock
    bool m_tunnelEnabled;
    DVAngel61IP::Control m_tunnelAdvertise;
    bool m_tunnelAdvertisePending;
    int m_tunnelDropped;
    DVAngel61HC::Compressor m_compressor;
    DVAngel61FileSender m_fileSender;
    int m_nextTransferId;
    uint64_t m_answerDestination;    //!< a file answer's station: where a transmission keyed up for it goes (0: none)

    // DTMF (3.8)
    bool m_dtmfOnly;                 //!< the transmission is for DTMF alone
    bool m_dtmfOnlyPending;          //!< ... the next one, queued before it started
    int m_dtmfHoldFrames;            //!< frames still to send idle for a Send's keys to play
    int m_relaySequence;             //!< a relay: the source's sequence, -1 for none yet
    int m_relayNext;                 //!< ... and the index of its next key
    bool m_relayNextStart;           //!< setRelay: the next start is a relay's
    bool m_relaying;                 //!< this transmission is a relay's
    bool m_dataOnlyNext = false;     //!< setDataOnlyNext()
    bool m_relaySequenceQueued = false;   //!< A key of the relayed DTMF sequence is in the queue
    bool m_dataOnlyNextMessage = false;
    bool m_relayDraining;            //!< ... its call ended, its text and messages still going out
    int m_relayDrainFrames;          //!< frames sent since the call ended
    int m_relayFrames;               //!< the call's frames seen (relayFrame): the reassembler's clock
    int m_relayedTexts;
    int m_relayedMessages;
    int m_relayHoldFrames;
    DVAngel61Reassembler m_relayReassembler;
    //! Relayed text or messages still to send
    bool relayPending() const { return m_relaying && (!m_messages.empty() || (m_slots.pendingFragments() > 0)); }
    //! The DTMF of the transmission is still to go out, or to play
    bool dtmfDue() const { return (m_slots.pendingDTMF() > 0) || (m_dtmfHoldFrames > 0); }

    bool dataWaiting() const { return !m_messages.empty() || (!m_relaying && fileWantsTransmission()); }
    //! A data transmission goes on while data waits, and a tunnel's for MaxTunnelBurstMs at most
    bool dataContinues() const { return dataWaiting() && ((m_burstFrames == 0) || (m_frameCount < m_burstFrames)); }
    int queuedFragments(int bytes) const;
    bool nextMessageField(int bytes, DVAngel61::MessageFragment& fragment);
    //! The next frame's header and block, whatever the profile; false when the transmission is over
    //! (HF's groups are built whole, and taken frame by frame by nextCells)
    bool nextBlock(DVAngel61::Header& header, std::vector<uint8_t>& block);
    void idleBlock(bool last, DVAngel61::Header& header, std::vector<uint8_t>& block);
    void voiceBlock(DVAngel61::Header& header, std::vector<uint8_t>& block);
    void dataBlock(DVAngel61::Header& header, std::vector<uint8_t>& block);
    void buildGroup();
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODANGEL61_H_
