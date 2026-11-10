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

#ifndef INCLUDE_DSP_DV_DVANGEL61_H
#define INCLUDE_DSP_DV_DVANGEL61_H

#include <algorithm>
#include <complex>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

#include "export.h"

/**
 * Angel61 air interface (ANGEL61.md): the four profiles' frames, the frame kinds, the header with
 * its copies, the payload blocks, the codes with their S-random interleavers, the V1 packet packing
 * and the gateway's records (dvangel61frames.cpp); and what every profile shares above the frames -
 * callsigns and talkgroups, the data slots, the message field's fragments and their reassembly -
 * and VHF's pulse (dvangel61.cpp). HF's OFDM is DVAngel61OFDM's. Shared by the DSE Modulator, the
 * DSD Demodulator, the gateway and the tests so that both ends of the link use one implementation.
 *
 * Bit arrays hold one bit per byte in transmission order; LLRs are positive for a 0; symbols are
 * complex with unit mean power. A frame is a vector of cells: the VHF profiles' frame symbols in
 * time order, or the HF profiles' OFDM cells, symbol l carrier k at l Nc + k.
 */
class SDRBASE_API DVAngel61
{
public:
    // ---- Pulse shaping (2.1) ----------------------------------------------------------------
    static const int RrcSpan = 24;            //!< symbols
    static const int MaxSamplesPerSymbol = 10;
    static const int MaxRrcTaps = RrcSpan * MaxSamplesPerSymbol + 1;
    static const float RrcAlpha;
    //! The pulse's taps at samplesPerSymbol: 241 at 10 (NB), 121 at 5 (WB)
    static int rrcTapCount(int samplesPerSymbol) { return RrcSpan * samplesPerSymbol + 1; }
    /**
     * VHF's pulse: the root raised cosine, alpha 0.2, span 24 symbols at samplesPerSymbol (10 in
     * NB, 5 in WB, at 48 kS/s), unit energy: a transmitter shaping unit power symbols with it
     * (multiplied by sqrt(samples per symbol) for unit mean output power) and a receiver matched
     * filtering with it see the symbols at their sent amplitude times the same at the symbol
     * instants. The span is where the truncation meets the adjacent channel mask with margin
     * (NB, at +-6.25 kHz in 4 kHz: 12 symbols -55 dB, 16 -59, 20 -61, 22 -65, 24 -70, 28 -66, 32
     * -71 dB). taps holds rrcTapCount(samplesPerSymbol).
     */
    static void rrcTaps(int samplesPerSymbol, float *taps);

    //! A QPSK symbol, unit power: index bit 1 -> I sign, bit 0 -> Q sign (0 positive)
    static std::complex<float> qpskSymbol(int index);

    //! Codec families (3.1): the station slot's 4 bit field. 1 was L1, revision 2's (ANGEL61.md 12), retired
    enum CodecFamily { CodecFamilyNone = 0, CodecFamilyL1 = 1, CodecFamilyO1 = 2, CodecFamilyV1 = 3 };

    // ---- Callsigns (2.6) --------------------------------------------------------------------
    static const uint64_t CallsignNone = 0;
    static const uint64_t CallsignBroadcast = 0xFFFFFFFFFFFFull;
    static const uint64_t CallsignLongFirst = 262144000000000ull;                //!< 40^9: the first long code
    static const uint64_t CallsignLongEnd = CallsignLongFirst + (1ull << 44);  //!< past the last one
    static const uint64_t CallsignTalkgroupFirst = CallsignLongEnd;             //!< talkgroup 0 (2.6)
    static const uint64_t CallsignTalkgroupEnd = CallsignTalkgroupFirst + (1ull << 32);
    static const int LongCallsignCharacters = 17;  //!< of A-Z, 0-9 and "/", before the SSID
    /**
     * A callsign in 48 bits (2.6). Up to 9 characters of base-40 (M17's alphabet: A-Z, 0-9,
     * "-", "/", "."): the characters themselves. Longer, up to 17 of A-Z, 0-9 and "/" and an
     * SSID of 0 to 15 ("EA8/G4ABC/QRP-7"): a long code, the base callsign (up to 7 characters)
     * and a 6 bit hash of the whole, or a 43 bit hash of the whole when the base callsign is
     * longer; its text goes in the long callsign slot (0x7). "CQ" and "" -> broadcast / none.
     * "TG n" -> talkgroup n. Returns CallsignNone for anything else. Encoding a long callsign
     * also makes it known to decodeCallsign.
     */
    static uint64_t encodeCallsign(const std::string& callsign);
    /**
     * The text of a code. A long code whose text has not been heard yet (encoded, or taken from a
     * long callsign slot, anywhere in this process) comes back as its base callsign and "..."
     * ("G4ABC..."), or "#" and the hash in hex when the code does not hold the base.
     */
    static std::string decodeCallsign(uint64_t code);
    static bool isLongCallsign(uint64_t code) { return (code >= CallsignLongFirst) && (code < CallsignLongEnd); }
    //! A talkgroup destination (2.6): a group call, "TG n"
    static bool isTalkgroup(uint64_t code) { return (code >= CallsignTalkgroupFirst) && (code < CallsignTalkgroupEnd); }
    static uint64_t talkgroupCode(uint32_t group) { return CallsignTalkgroupFirst + group; }
    static uint32_t talkgroupOf(uint64_t code) { return (uint32_t) (code - CallsignTalkgroupFirst); }
    //! A destination that names no one station: broadcast (CQ) or a talkgroup. Wherever a station
    //! asks whether it is addressed - the rate request, a file's answers, the tunnel - a group is CQ
    static bool isGroup(uint64_t code) { return (code == CallsignBroadcast) || isTalkgroup(code); }
    //! "TG n" (any case, one or more spaces): true and the group
    static bool parseTalkgroup(const std::string& text, uint32_t& group);
    static std::string talkgroupText(uint32_t group) { return "TG " + std::to_string(group); }
    //! A receiver's group list: comma separated groups, "9" or "TG 9"; anything else is skipped
    static std::vector<uint32_t> parseTalkgroupList(const std::string& text);
    //! Whether a call to this destination is played with this group list (2.6): a talkgroup only
    //! when listed; CQ, a callsign, and everything when the list is empty, always
    static bool talkgroupPlayed(uint64_t destination, const std::vector<uint32_t>& groups);
    //! Whether a destination's text is broadcast or a talkgroup
    static bool isGroupText(const std::string& text) { return isGroup(encodeCallsign(text)); }
    //! Whether decodeCallsign gives the callsign itself (not a long code still unheard)
    static bool callsignKnown(uint64_t code);
    /**
     * The station a callsign names: its base callsign and SSID, without a visited country
     * prefix or an operating suffix ("EA8/G4ABC/P-7" -> "G4ABC-7"), the form matched when a
     * station asks whether it is addressed and the form APRS takes (no "/" in an APRS address).
     */
    static std::string stationIdentity(const std::string& callsign);
    static bool sameStation(const std::string& a, const std::string& b);

    // ---- Data slots (2.5) --------------------------------------------------------------------
    static const int SlotBits = 100;
    static const int SlotContentBits = 96;

    enum SlotID {
        SlotIdle = 0x0,
        SlotRouting = 0x1,
        SlotPath = 0x2,
        SlotStation = 0x3,
        SlotPosition = 0x4,
        SlotText = 0x5,
        SlotAck = 0x6,
        SlotLongCallsign = 0x7,
        SlotDTMF = 0x8
    };

    struct Routing { uint64_t m_source = 0; uint64_t m_destination = CallsignBroadcast; };
    struct Path { uint64_t m_repeater1 = 0; uint64_t m_repeater2 = 0; };
    struct Station
    {
        int m_protocolVersion = ProtocolVersion;
        int m_codecFamily = CodecFamilyV1; //!< the family of the voice mode used next (3.1): V1 or O1
        int m_codecModel = 0;
        int m_nextTier = 0;            //!< the tier of the voice mode used next (2.10)
        int m_powerClass = 0;
        int m_antennaClass = 0;
        uint64_t m_locator = 0;        //!< 48 bits
        char m_symbolTable = 0;        //!< APRS symbol (3.3), 0 when unspecified
        char m_symbolCode = 0;
        bool m_tunnelOffered = false;  //!< this station offers an IP tunnel (3.5)
    };
    struct Position
    {
        double m_latitude = 0.0;       //!< degrees, north positive
        double m_longitude = 0.0;      //!< degrees, east positive
        int m_altitude = AltitudeUnknown; //!< metres above mean sea level
        int m_speed = SpeedUnknown;    //!< km/h, 1022 = more
        int m_course = CourseUnknown;  //!< degrees true
        int m_fixAge = NoFix;          //!< seconds, 0..126, NoFix = no position
        int m_source = 0;              //!< 0 GNSS, 1 by hand, 2 network or other
        static const int AltitudeUnknown = -100000;
        static const int SpeedUnknown = -1;
        static const int CourseUnknown = -1;
        static const int NoFix = 0x7F;
    };
    struct TextFragment
    {
        int m_id = 0;                  //!< 0..15
        int m_index = 0;               //!< 0..15
        int m_total = 1;               //!< 1..15
        std::string m_text;            //!< up to 12 characters of 7 bit ASCII
        static const int Characters = 12;
    };
    struct Ack { int m_messageId = 0; uint64_t m_receiver = 0; int m_blockErrorRate = 0; };
    //! The DTMF slot (3.8): a window of up to 17 keys of a sequence, sharing a tone length and gap
    struct DTMF
    {
        int m_sequence = 0;            //!< 0..15
        int m_first = 0;               //!< the first key's index in the sequence, 0..63
        int m_toneMs = 100;            //!< 20..620, in steps of 20
        int m_gapMs = 100;             //!< 20..620, in steps of 20
        std::string m_keys;            //!< 1..17 keys: 0-9, A-D, *, #
        static const int MaxKeys = 17;
        static const int UnitMs = 20;
        static const int MaxUnits = 31;
    };

    static int slotID(const uint8_t *slot) { return (slot[0] << 3) | (slot[1] << 2) | (slot[2] << 1) | slot[3]; }
    static void packIdleSlot(uint8_t *slot);
    static void packRoutingSlot(const Routing& routing, uint8_t *slot);
    static void packPathSlot(const Path& path, uint8_t *slot);
    static void packStationSlot(const Station& station, uint8_t *slot);
    static void packPositionSlot(const Position& position, uint8_t *slot);
    static void packTextSlot(const TextFragment& fragment, uint8_t *slot);
    static void packAckSlot(const Ack& ack, uint8_t *slot);
    //! False when the window cannot be sent: no key, more than 17, or a character that is no key
    static bool packDTMFSlot(const DTMF& dtmf, uint8_t *slot);
    //! The text of a long callsign (2.6): false when it is not one
    static bool packLongCallsignSlot(const std::string& callsign, uint8_t *slot);
    static bool unpackRoutingSlot(const uint8_t *slot, Routing& routing);
    static bool unpackPathSlot(const uint8_t *slot, Path& path);
    static bool unpackStationSlot(const uint8_t *slot, Station& station);
    static bool unpackPositionSlot(const uint8_t *slot, Position& position);
    static bool unpackTextSlot(const uint8_t *slot, TextFragment& fragment);
    static bool unpackAckSlot(const uint8_t *slot, Ack& ack);
    //! False for an invalid slot (3.8: a count or a length out of range), which is dropped whole
    static bool unpackDTMFSlot(const uint8_t *slot, DTMF& dtmf);
    //! A length in the DTMF slot's 20 ms units, 1..31
    static int dtmfUnits(int ms) { return std::max(1, std::min((int) DTMF::MaxUnits, (ms + DTMF::UnitMs / 2) / DTMF::UnitMs)); }
    static bool unpackLongCallsignSlot(const uint8_t *slot, std::string& callsign);
    //! Position field encodings (3.2)
    static uint32_t encodeLatitude(double latitude);
    static uint32_t encodeLongitude(double longitude);
    static double decodeLatitude(uint32_t u);
    static double decodeLongitude(uint32_t u);

    // ---- Message field (3.2) ------------------------------------------------------------------
    //! Content of the message field (3.2)
    enum ContentType {
        ContentText = 0,               //!< UTF-8 text
        ContentBinary = 1,
        ContentAPRS = 2,               //!< an APRS information field (3.3)
        ContentFileOffer = 3,          //!< file transfer (3.4): the name, size and CRC-32
        ContentFileData = 4,           //!< a block of the file: the header carries the transfer id and block index
        ContentFileControl = 5,        //!< the receiver's answer: accept, reject, missing, complete, abort
        ContentIPDatagram = 6,         //!< IP tunnelling (3.5): one IPv4 or IPv6 datagram
        ContentTunnelControl = 7       //!< the two ends agreeing: advertise, refuse, close, repair
    };

    struct MessageFragment
    {
        int m_contentType = 0;         //!< ContentType
        int m_flags = 0;
        int m_id = 0;                  //!< 0..255
        int m_index = 0;               //!< 0..255
        int m_total = 1;               //!< 1..255
        std::vector<uint8_t> m_payload; //!< the field's payload bytes; fragment 0 begins with the 16 bit message byte length
        //! Content type 4 (3.4): the index and total bytes are one 16 bit block index instead
        int blockIndex() const { return (m_index << 8) | m_total; }
        void setBlockIndex(int index) { m_index = (index >> 8) & 0xFF; m_total = index & 0xFF; }
    };
    //! (Packed, unpacked and a message fragmented by the field's size: DVAngel61)
    //! Split text into slot fragments (7 bit ASCII, up to 15 x 12 characters; other characters become '?')
    static int fragmentText(const std::string& text, int id, std::vector<TextFragment>& fragments);

    // ---- Profiles (2.1) ---------------------------------------------------------------------
    enum Profile { ProfileNB = 0, ProfileWB = 1, ProfileHFNB = 2, ProfileHFWB = 3 };
    static const int NbProfiles = 4;
    static bool isHF(Profile profile) { return profile >= ProfileHFNB; }
    static const char *profileName(Profile profile);
    static const int ProtocolVersion = 3;
    static const int CodecModelV1 = 1;

    // ---- Frame types (2.4): type = profile << 5 | kind ----------------------------------------
    enum Kind {
        KindReserved = 0x00,
        KindVoice = 0x01,        //!< 0x01 to 0x04: voice at tier 0 to 3 (WB's 0x04 is Opus audio)
        KindVoiceData1 = 0x05,   //!< 0x05 to 0x08: tier 0 to 3 with data under it, one step
        KindVoiceData2 = 0x09,   //!< 0x09 to 0x0C: two steps
        KindData = 0x10,         //!< VHF data, QPSK
        KindDataQAM16 = 0x12,    //!< VHF data, 16-QAM
        KindGroupData = 0x14,    //!< 0x14 to 0x17: HF data at tier 0 to 3, one frame of a 4-frame group
        KindIdle = 0x1E
    };
    static int typeOf(Profile profile, int kind) { return (profile << 5) | (kind & 0x1F); }
    static Profile profileOf(int type) { return (Profile) ((type >> 5) & 3); }
    static int kindOf(int type) { return type & 0x1F; }
    //! A type this revision's receivers act on: a known kind of its profile, with data under voice
    //! only where its packets keep the profile's floor (2.3)
    static bool isKnownType(int type);
    static bool isVoice(int type);               //!< kinds 0x01 to 0x0C (WB's 0x04 included)
    static bool isOpus(int type) { return (profileOf(type) == ProfileWB) && (kindOf(type) == 0x04); }
    static int dataSteps(int type);              //!< 0, 1 or 2 steps of data under voice
    static bool isData(int type) { int k = kindOf(type); return (k == KindData) || (k == KindDataQAM16) || isGroupData(type); }
    static bool isGroupData(int type) { int k = kindOf(type); return isHF(profileOf(type)) && (k >= KindGroupData) && (k < KindGroupData + 4); }
    static bool isIdle(int type) { return kindOf(type) == KindIdle; }
    //! The tier (0..3) of a voice, data-under-voice or HF data type; idle is tier 0; VHF data -1
    static int tierOf(int type);
    static int voiceType(Profile profile, int tier) { return typeOf(profile, KindVoice + tier); }
    //! The type sent for a tier with `steps` of data under it, or -1 when the profile does not allow it
    static int voiceDataType(Profile profile, int tier, int steps);
    static const char *typeName(int type);

    // ---- V1 packets (3.1.1) ------------------------------------------------------------------
    static const int CodebookBits = 10;
    static const int OpusBits = 1472;            //!< one 80 ms Opus packet, 184 bytes (WB tier 3)
    static int packetsPerFrame(Profile profile) { return isHF(profile) ? 18 : 6; }
    //! Codebooks of a tier's voice: NB 3 4 6 12, WB 6 12 16 (tier 3 Opus: 0), HF-NB 2 3 4 6, HF-WB 3 6 8 12
    static int tierCodebooks(Profile profile, int tier);
    static int stepCodebooks(Profile profile);   //!< 2 NB, 4 WB, 1 HF
    static int floorCodebooks(Profile profile) { return profile == ProfileHFNB ? 2 : 3; }
    //! Codebooks each V1 packet of the type carries (the tier's less the data steps); 0 for no V1
    static int codebooksOf(int type);
    static int packetBits(int type) { return CodebookBits * codebooksOf(type); }
    //! The first 10 m bits of an n-codebook packet: the m-codebook packet of the same frame
    static void truncatePacket(const uint8_t *packet, int codebooks, uint8_t *out) { for (int i = 0; i < CodebookBits * codebooks; i++) { out[i] = packet[i]; } }

    // ---- Codes (2.7) -------------------------------------------------------------------------
    enum CodeId {
        CodeNB_T0, CodeNB_T1, CodeNB_T2, CodeNB_T3, CodeNB_DQ, CodeNB_DX,
        CodeWB_T0, CodeWB_T1, CodeWB_T2, CodeWB_T3, CodeWB_DQ,
        CodeHN_T0, CodeHN_T1, CodeHN_T2, CodeHN_T3,
        CodeHW_T0, CodeHW_T1, CodeHW_T2, CodeHW_T3,
        CodeHN_D0, CodeHN_D1, CodeHN_D2, CodeHN_D3, //!< HF data groups at tier 0 to 3: one codeword over 4 frames
        CodeHW_D0, CodeHW_D1, CodeHW_D2, CodeHW_D3,
        NbCodes,
        CodeNone = -1
    };
    enum Constellation { QPSK, QAM16 };
    static CodeId codeOf(int type);
    static Constellation constellationOf(int type);
    static int bitsPerCell(Constellation c) { return c == QPSK ? 2 : 4; }
    static int infoBits(int type);               //!< K, with the CRC-12
    static int codedBits(int type);              //!< n (over the whole group for HF data)
    //! Coded bits one frame carries: n, or n / 4 for a data group
    static int frameCodedBits(int type) { return isGroupData(type) ? codedBits(type) / GroupFrames : codedBits(type); }

    /**
     * Quasi-cyclic LDPC code from the generated tables: systematic (the K information bits, then
     * the parity), dual-diagonal parity, layered normalised min-sum decoding (factor 0.8).
     */
    class SDRBASE_API LDPC
    {
    public:
        explicit LDPC(CodeId code);
        int getN() const { return m_n; }
        int getK() const { return m_k; }
        const short *getPerm() const { return m_perm; }
        void encode(const uint8_t *info, uint8_t *codeword) const;
        //! Iterations used when the syndrome cleared, else -1; info (and codeword, posterior) either way
        int decode(const float *llr, uint8_t *info, int maxIterations = 30, uint8_t *codeword = nullptr, float *posterior = nullptr) const;
        bool syndromeZero(const uint8_t *codeword) const;

    private:
        int m_n, m_k, m_z, m_mb, m_kb, m_nb;
        const short *m_base;
        const short *m_perm;
        std::vector<int> m_rowStart;
        std::vector<int> m_column;
        int m_maxDegree;
    };
    static const LDPC& ldpc(CodeId code);

    // ---- Layouts (2.2) -----------------------------------------------------------------------
    enum CellKind { CellUW = 0, CellPilot = 1, CellHeader = 2, CellPayload = 3 };
    struct SDRBASE_API Layout
    {
        Profile m_profile;
        int m_cells;                             //!< frame symbols (VHF) or L x Nc cells (HF)
        int m_carriers;                          //!< HF: Nc; VHF: 1
        int m_symbols;                           //!< HF: L OFDM symbols; VHF: the frame symbols
        int m_headerCopies;                      //!< 2 (NB, WB, HF-NB) or 3 (HF-WB)
        std::vector<signed char> m_kind;         //!< CellKind of each cell
        std::vector<std::complex<float>> m_known;//!< the UW or pilot value of a known cell, else 0
        std::vector<int> m_headerCells;          //!< cell of header cell number c (24 x copies of them)
        std::vector<int> m_payloadCells;         //!< cell of payload cell p, in payload order
        std::vector<int> m_pilotCells;
        std::vector<int> m_uwCells;
        int payloadCount() const { return (int) m_payloadCells.size(); }
    };
    static const Layout& layout(Profile profile);

    // ---- Payload block (2.3): offsets within the K information bits --------------------------
    static const int FieldHeaderBits = 32;
    static int voiceBits(int type);              //!< the voice field: packets x 10 codebooks, or Opus
    static int slotsOf(int type);                //!< 1, 2 (NB and WB tier 0... 2.3), or 4 (a data group)
    static int slotOffset(int type, int slot) { return voiceBits(type) + slot * SlotBits; }
    //! The message field: data frames and data under voice; 0 elsewhere
    static int messageFieldBits(int type);
    static int messageFieldOffset(int type) { return slotOffset(type, slotsOf(type)); }
    static int messageFieldBytes(int type) { int b = messageFieldBits(type); return b > FieldHeaderBits ? (b - FieldHeaderBits) / 8 : 0; }
    static int crcOffset(int type) { return infoBits(type) - 12; }
    static void putVoicePacket(uint8_t *block, int type, int packet, const uint8_t *bits);
    static void getVoicePacket(const uint8_t *block, int type, int packet, uint8_t *bits);
    static void putOpus(uint8_t *block, const uint8_t *bits) { for (int i = 0; i < OpusBits; i++) { block[i] = bits[i]; } }
    static void packMessageField(const DVAngel61::MessageFragment& fragment, int bytes, uint8_t *bits);
    static void unpackMessageField(const uint8_t *bits, int bytes, DVAngel61::MessageFragment& fragment);
    //! Split a message into fragments of `bytes` (fragment 0 begins with the 16 bit length). Returns the count, 0 if too long.
    static int fragmentMessage(const std::vector<uint8_t>& message, int contentType, int id, int bytes,
        std::vector<DVAngel61::MessageFragment>& fragments, int flags = 0);

    // ---- Header (2.4, 2.7) -------------------------------------------------------------------
    static const int HeaderFieldBits = 16;
    static const int HeaderInfoBits = 24;
    static const int HeaderCodedBits = 48;       //!< two Golay(24,12) words, bit interleaved, scrambled
    static const int HeaderCells = 24;           //!< QPSK cells of one copy: cell c carries bits 2c, 2c + 1
    struct Header
    {
        int m_type = 0;
        int m_counter = 0;                       //!< 0..63
        bool m_lastFrame = false;
        int m_tier = 0;                          //!< the rate request (2.10)
        bool operator==(const Header& h) const { return (m_type == h.m_type) && (m_counter == h.m_counter) && (m_lastFrame == h.m_lastFrame) && (m_tier == h.m_tier); }
    };
    static void headerFieldBits(const Header& header, uint8_t *bits);   //!< the 16 field bits
    static void encodeHeader(const Header& header, uint8_t *bits);      //!< 48 coded bits
    //! From 48 LLRs (the copies already added): true when the CRC-8 matches
    static bool decodeHeader(const float *llr, Header& header, int *errors = nullptr);

    // ---- CRCs (2.7): both from all ones -----------------------------------------------------
    static uint8_t crc8(const uint8_t *bits, int nbBits);
    static uint16_t crc12(const uint8_t *bits, int nbBits);
    /**
     * The block's CRC-12: over the 16 field bits of the frame's header (of each of a data group's
     * four headers, in frame order), then the block's information bits before the CRC (2.7).
     */
    static uint16_t blockCrc(const Header *headers, int nbHeaders, const uint8_t *block, int type);
    static void finishBlock(uint8_t *block, const Header& header) { finishBlock(block, &header, 1); }
    static void finishBlock(uint8_t *block, const Header *headers, int nbHeaders);
    static bool checkBlock(const uint8_t *block, const Header& header) { return checkBlock(block, &header, 1); }
    static bool checkBlock(const uint8_t *block, const Header *headers, int nbHeaders);

    // ---- The network's frame record (plan C7) -------------------------------------------------
    /**
     * A confirmed frame as a gateway carries it, the layout of revision 2's "angel61-frame" record:
     * the type, the counter (6 bits) with 0x40 on the last frame, the rate request, then the K
     * information bits MSB first. Single frames: a data group's block is four frames' and is not
     * carried.
     */
    static std::vector<uint8_t> packRecord(const Header& header, const uint8_t *block);
    //! The header and the block (unpacked bits) of a record whose type is a known single-frame one,
    //! whose length is its type's and whose CRC-12 passes (a revision 2 record fails it)
    static bool unpackRecord(const uint8_t *record, int size, Header& header, std::vector<uint8_t>& block);
    static int recordBytes(int type) { return 3 + (infoBits(type) + 7) / 8; }
    /**
     * A whole message as a gateway carries it (the "angel61-message" entry, plan C7): one that came
     * in an HF data group, whose block is beyond a record. The content type, the field's four flag
     * bits, then the message's bytes. Text, binary and APRS only (a file's and the tunnel's answers
     * cannot come back through a gateway, 2.6), up to MaxRelayMessageBytes: the network's packet
     * holds it in base64 within 1200 bytes.
     */
    static const int MaxRelayMessageBytes = 700;
    static bool relayable(int contentType) { return (contentType == ContentText) || (contentType == ContentBinary) || (contentType == ContentAPRS); }
    static std::vector<uint8_t> packRelayMessage(int contentType, int flags, const std::vector<uint8_t>& message);
    //! False unless the content type is relayable, the flags four bits and the message 1 to MaxRelayMessageBytes
    static bool unpackRelayMessage(const uint8_t *bytes, int size, int& contentType, int& flags, std::vector<uint8_t>& message);

    // ---- Data groups (2.3) --------------------------------------------------------------------
    static const int GroupFrames = 4;
    //! A group starts at counter c with c mod 4 == 2: the position of a frame in its group
    static int groupIndex(int counter) { return (counter + 2) & 3; }
    /**
     * The four headers of a group from any one of them: the same type and rate request, counters c
     * to c + 3, the last-frame flag only on the fourth (lastOfFourth). A receiver that lost the
     * fourth header tries lastOfFourth both ways.
     */
    static void groupHeaders(const Header& any, bool lastOfFourth, Header *headers);
    /**
     * The four frames of a data group (cells: 4 x the layout's cells, frame f from f x cells): each
     * frame's known cells and its own header, and the group's one codeword (the block, CRC written
     * with finishBlock over the four headers) scrambled with the first frame's counter and spread
     * over the four frames' payload cells in position order.
     */
    static void buildGroup(const Header *headers, const uint8_t *block, std::complex<float> *cells);
    /**
     * A group's block from its four frames' payload LLRs (each frame's payloadLLR, one after the
     * other) and any one of its headers. fourthLast: the fourth frame's last-frame flag when its
     * header decoded (0 or 1), -1 when it did not (both are tried against the CRC-12). headers
     * receives the four confirmed headers. True when the CRC-12 over the headers and block passes.
     */
    static bool decodeGroup(const float *llr, const Header& any, int fourthLast, uint8_t *block, Header *headers = nullptr,
        int *iterations = nullptr);

    // ---- Symbols -----------------------------------------------------------------------------
    static void mapCells(Constellation c, const uint8_t *bits, int nbCells, std::complex<float> *cells);
    static void demapCells(Constellation c, const std::complex<float> *cells, int nbCells, float noiseVariance, float *llr);
    /**
     * Amplitude-aware likelihoods (ANGEL61.md 5): an equalised cell's error is the noise and the
     * channel estimate's error times the symbol, so its variance is noise + estimateError |x|^2
     * (both relative to the unit constellation), larger for 16-QAM's outer points than its inner
     * ones. Each point's metric |y - x|^2 / v(x) + ln v(x), max-log. QPSK's |x|^2 is 1: the fixed form
     */
    static void demapCellsAmplitude(Constellation c, const std::complex<float> *cells, int nbCells, float noiseVariance,
        float estimateError, float *llr);

    // ---- Coding chain (2.7) ------------------------------------------------------------------
    //! A codeword scrambled with PN15 (the counter) and put in transmission order: position perm[i] <- coded bit i
    static void codewordToPositions(const uint8_t *codeword, int counter, CodeId code, uint8_t *positions);
    //! The inverse on LLRs: coded bit i <- position perm[i], descrambled
    static void positionsToCodeword(const float *positions, int counter, CodeId code, float *coded);

    // ---- Frames --------------------------------------------------------------------------------
    /**
     * Every cell of a frame: known cells, the header's copies, and the block (K bits, CRC written
     * with finishBlock) encoded, scrambled, interleaved and mapped. Not for a data group's frames.
     */
    static void buildFrame(const Header& header, const uint8_t *block, std::complex<float> *cells);
    //! The 48 header LLRs of a frame of equalised cells, the copies added
    static void headerLLR(Profile profile, const std::complex<float> *cells, float noiseVariance, float *llr);
    //! The payload LLRs of a frame in position order (payloadCount x bits per cell)
    static void payloadLLR(const std::complex<float> *cells, int type, float noiseVariance, float *llr);
    /**
     * A frame's block from its position ordered LLRs: descramble, deinterleave, decode, and check
     * the CRC-12 over the header and the block. True when the block and header are confirmed.
     */
    static bool decodePayload(const float *llr, const Header& header, uint8_t *block, int *iterations = nullptr,
        uint8_t *codeword = nullptr, float *posterior = nullptr);
    /**
     * A decoded codeword back to the frame's payload cells (in payload order), for a receiver's
     * decoding-aided estimation: the cells it was sent as, and with the decoder's posteriors each
     * cell's confidence, the least sure of its bits' |LLR|. Not for a data group's frames.
     */
    static void payloadCells(const uint8_t *codeword, int counter, int type, std::complex<float> *cells,
        const float *posterior = nullptr, float *confidence = nullptr);

    // ---- Timing (2.2, 2.8) -------------------------------------------------------------------
    static int frameMs(Profile profile) { return isHF(profile) ? 240 : 80; }
    //! The routing slot is reserved in the first slot of a frame with this counter (2.8)
    static bool routingFrame(Profile profile, int counter) { return isHF(profile) ? (counter % 3) == 0 : (counter & 1) == 0; }
};

/**
 * The receiver's reassembly of text fragments (slot 0x5) and message fields (3.2), keyed
 * by source callsign and message id, with the duplicate and expiry rules of 3.2.
 */
class SDRBASE_API DVAngel61Reassembler
{
public:
    static const int ExpiryMs = 30000;

    struct Message
    {
        uint64_t m_source = 0;
        int m_contentType = 0;         //!< 0 text (also for slot text), 1 binary
        int m_flags = 0;               //!< the message field's four flag bits (content type 6: the header compression form, 3.6)
        std::vector<uint8_t> m_data;   //!< bytes: UTF-8 or 7 bit text, or binary
        std::string text() const { return std::string(m_data.begin(), m_data.end()); }
    };

    //! timeMs: the receiver's clock for the expiry. Returns true when a message completed.
    bool addTextFragment(uint64_t source, const DVAngel61::TextFragment& fragment, int64_t timeMs, Message& completed);
    bool addMessageFragment(uint64_t source, const DVAngel61::MessageFragment& fragment, int64_t timeMs, Message& completed);
    void expire(int64_t timeMs);
    //! Messages being collected (the delivered ones kept against repeats do not count)
    int pending() const;

    //! A message still being collected: the fragments it lacks (3.5's repair)
    struct Incomplete
    {
        int m_id = 0;
        int m_total = 0;
        std::vector<std::pair<int, int>> m_missing;   //!< first fragment index, count
    };
    //! A source's messages of a content type still incomplete, a fragment of which arrived at or after sinceMs
    std::vector<Incomplete> incomplete(uint64_t source, int contentType, int64_t sinceMs) const;

private:
    struct Partial
    {
        int m_total = 0;
        int m_contentType = 0;
        int m_flags = 0;
        int m_length = -1;              //!< message field: byte length from fragment 0, -1 unknown
        int64_t m_lastTimeMs = 0;
        bool m_complete = false;        //!< Delivered; kept until it expires so that repeated fragments are duplicates
        std::map<int, std::vector<uint8_t>> m_fragments; //!< index -> payload (bytes or characters)
    };
    std::map<std::pair<uint64_t, int>, Partial> m_text;
    std::map<std::pair<uint64_t, int>, Partial> m_messages;
};

/**
 * A relay's V1 path between any two profiles (3.1.1): no vocoder, only truncation and repacking.
 * Each input frame's packets are cut to the output type's codebooks and queued; an output frame
 * takes the next 6 (VHF) or 18 (HF) of them, so three VHF frames make one HF frame. The rate only
 * goes down: an input with fewer codebooks than the output is refused.
 */
class SDRBASE_API DVAngel61Repacker
{
public:
    DVAngel61Repacker() : m_outType(-1) {}
    //! The output's voice type (any V1 voice or voice-with-data type); clears the queue
    void setOutput(int type) { m_outType = type; m_packets.clear(); }
    //! One input frame's packets. False when the input is not V1 voice or carries too few codebooks.
    bool pushFrame(const uint8_t *block, int inType);
    //! Output frames ready
    int framesReady() const;
    //! The next output frame's packets into its block (voiceBits of the output type)
    bool popFrame(uint8_t *block);
    int queuedPackets() const { return (int) m_packets.size(); }

private:
    int m_outType;
    std::deque<std::vector<uint8_t>> m_packets;
};

/**
 * The transmitter's slot queue (ANGEL61.md 2.8), revision 2's schedule with the profile's frame
 * length, routing cadence and periods: the preamble's fixed slots; the routing in the first slot of
 * every even frame (VHF) or every third (HF); earliest deadline first with a 500 ms lookahead for
 * station and parameters, position, path and the long callsigns; text fragments in the remaining
 * slots; routing again, then idle.
 */
class SDRBASE_API DVAngel61SlotQueue
{
public:
    explicit DVAngel61SlotQueue(DVAngel61::Profile profile = DVAngel61::ProfileNB);
    void setProfile(DVAngel61::Profile profile) { m_profile = profile; reset(); }
    DVAngel61::Profile getProfile() const { return m_profile; }
    //! Key down: forget the schedule, keep the configuration
    void reset();
    void setRouting(const DVAngel61::Routing& routing) { m_routing = routing; updateLongCallsigns(); }
    void setPath(const DVAngel61::Path& path) { m_path = path; m_hasPath = (path.m_repeater1 != 0); updateLongCallsigns(); }
    void clearPath() { m_hasPath = false; updateLongCallsigns(); }
    void setStation(const DVAngel61::Station& station) { m_station = station; }
    void setPosition(const DVAngel61::Position& position, bool moving);
    void clearPosition() { m_hasPosition = false; }
    bool queueText(const std::string& text);
    int pendingFragments() const { return (int) m_fragments.size(); }
    /**
     * DTMF (3.8): keys of the current sequence, or of a new one when newSequence (or when there is
     * none yet), with their tone length and gap. Until every key has gone out twice, in different
     * frames, the DTMF slot takes every slot but the reserved routing slot. The keys queued before a
     * key down are kept for the transmission. False when no key is valid.
     */
    bool queueDTMF(const std::string& keys, int toneMs, int gapMs, bool newSequence);
    //! Keys not yet sent twice
    int pendingDTMF() const { return (int) m_dtmf.size(); }
    //! The DTMF keys still queued dropped (a transmission aborted: not into the next one)
    void clearDTMF() { m_dtmf.clear(); }
    //! The text still queued dropped, and its repeat (a relay's session over: the caller's text)
    void clearText() { m_fragments.clear(); m_lastMessage.clear(); m_repeatPending = false; }
    //! Queued at most: a relay's text and keys come from the network, and a key's index is 6 bits
    //! with a receiver's window half of them (DSDAngel61Session)
    static const int MaxTextFragments = 64;
    static const int MaxDTMFKeys = 32;
    //! The current sequence's id, -1 before the first
    int getDTMFSequence() const { return m_dtmfSequence; }
    int getDTMFSlotsSent() const { return m_dtmfSlotsSent; }
    /**
     * The slots of the next frame: counter its frame counter, nbSlots its slot count, slots nbSlots x
     * 100 bits. Frames 0 and 1 are the preamble; a frame with lastFrame carries the routing. Advances
     * the queue's clock by one frame of the profile.
     */
    void nextSlots(int counter, int nbSlots, bool lastFrame, uint8_t *slots);

    int periodMs(int item) const { return periodOf((Item) item); }
    int getMissedDeadlines() const { return m_missedDeadlines; }
    int getFragmentsSent() const { return m_fragmentsSent; }
    int getRoutingSent() const { return m_routingSent; }
    int getIdleSent() const { return m_idleSent; }

private:
    enum Item { ItemStation, ItemPosition, ItemPath, ItemLong0, ItemLong1, ItemLong2, ItemLong3, ItemCount };
    static const int LookaheadMs = 500;

    DVAngel61::Profile m_profile;
    DVAngel61::Routing m_routing;
    DVAngel61::Path m_path;
    bool m_hasPath;
    std::vector<uint64_t> m_longCallsigns;
    DVAngel61::Station m_station;
    DVAngel61::Position m_position;
    bool m_hasPosition;
    bool m_moving;
    std::deque<DVAngel61::TextFragment> m_fragments;
    std::vector<DVAngel61::TextFragment> m_lastMessage;
    struct DTMFKey { int m_sequence; int m_index; char m_key; int m_toneUnits; int m_gapUnits; int m_sends; int m_lastSentMs; };
    std::deque<DTMFKey> m_dtmf;         //!< keys not yet sent twice, in order
    int m_dtmfSequence;
    int m_dtmfNextIndex;
    int m_dtmfSlotsSent;
    bool m_repeatPending;
    int m_nextTextId;
    int m_timeMs;
    bool m_routingThisFrame;
    int m_deadline[ItemCount];
    int m_missedDeadlines;
    int m_fragmentsSent;
    int m_routingSent;
    int m_idleSent;

    void updateLongCallsigns();
    int periodOf(Item item) const;
    bool enabled(Item item) const;
    void fillItem(Item item, uint8_t *slot);
    void fillFree(uint8_t *slot);
    //! The DTMF slot, when a key is due: its window from the earliest key not yet sent twice and not
    //! sent in this frame
    bool fillDTMF(uint8_t *slot);
};

#endif // INCLUDE_DSP_DV_DVANGEL61_H
