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

#ifndef MODEMRADIOPAGING_RADIOPAGINGFLEXDECODER_H
#define MODEMRADIOPAGING_RADIOPAGINGFLEXDECODER_H

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "export.h"
#include "radiopagingberwindow.h"
#include "radiopagingflex.h"

// FLEX frame decoder: codewords of a received frame to messages.
//
// The input is a whole frame: for each phase the 88 received codewords of the data part.
// Symbol timing, sync search and slicing are not part of this class. The decoder keeps the
// state that spans frames (fragments of long messages, message history for duplicates and
// retransmissions, temporary group assignments, repeated subframes, network time) for each
// of the two polarities separately: a frame received with inverted polarity belongs to
// a different logical network than a frame received with normal polarity.
//
// Codeword convention of RxPhase: the same as the words of RadioPagingFlex::buildPhase and
// RadioPagingFlex::assemble. A word is in transmission order, bit 31 is the first bit received, the
// BCH code word of RadioPagingBCH is the word itself. The 88 words of a phase are 11 blocks of
// 8 words, each block interleaved (see RadioPagingFlex::interleaveBlock): the decoder
// deinterleaves them. A frame made by RadioPagingFlexEncoder is fed back with the words
// of RadioPagingFlex::PhaseResult.
class MODEMRADIOPAGING_API RadioPagingFlexDecoder
{
public:
    enum WordStatus {
        WordNotReceived,
        WordClean,
        WordCorrected,
        WordUncorrectable   //!< Includes erased words
    };

    enum AddressType {
        AddressShort,
        AddressLong,
        AddressNetwork,
        AddressTemporary,
        AddressOperator,        //!< Operator messaging address
        AddressInfoService,
        AddressReserved,
        AddressUnknown
    };

    enum MessageType {
        TypeUnknown,
        TypeTone,               //!< Address without vector
        TypeAlphanumeric,
        TypeSecure,
        TypeNumeric,
        TypeSpecialNumeric,
        TypeNumberedNumeric,
        TypeNumberedSpecialNumeric,
        TypeBinary,             //!< Hex / binary vector
        TypeShortMessage,       //!< Message in the vector word (vector type 2)
        TypeShortInstruction
    };

    enum Check {
        CheckNone,              //!< Not applicable or not evaluated
        CheckOk,
        CheckFailed,
        CheckIncomplete         //!< Signature S: characters of the message lost (a fragment missing); never the checksum K, ok or failed
    };

    enum FragmentKind {
        FragmentUnknown,        //!< Header word lost
        FragmentComplete,       //!< One message, C = 0, F = 3
        FragmentFirst,          //!< C = 1, F = 3
        FragmentMiddle,         //!< C = 1, F != 3
        FragmentLast            //!< C = 0, F != 3
    };

    //! Codewords of one phase of the data part
    struct RxPhase
    {
        char m_name = 'A';                  //!< 'A' to 'D'
        std::vector<uint32_t> m_words;      //!< 88 interleaved codewords (see class description)
        //! The same codewords from another decision on the symbols (their centre, may be empty = none): a codeword
        //! that cannot be corrected is taken from here if it can (in the address field only if it is received without
        //! error and differs from the other by one bit at most: a wrong address would be a message to another pager)
        std::vector<uint32_t> m_centreWords;
        //! 88 flags (may be empty = none): non zero if the codeword is lost (carrier lost). The
        //! index is the codeword index after deinterleaving, block * 8 + word in block
        std::vector<uint8_t> m_erased;
        //! Reliability of each received bit (may be empty = none), 32 for each word of m_words in transmission
        //! order (index word * 32 + 31 - bit): the larger the more reliable. Only used to choose the characters
        //! shown for a word that cannot be corrected (see Word::m_received)
        std::vector<uint8_t> m_reliability;
    };

    struct RxFrame
    {
        RadioPagingFlex::Mode m_mode = RadioPagingFlex::Mode1600_2;
        bool m_inverted = false;            //!< Frame received with inverted polarity
        bool m_hasFiw = false;              //!< m_fiw holds the received frame information word
        uint32_t m_fiw = 0;                 //!< Received FIW codeword (transmission order). Decoded and checked
        unsigned m_cycle = 0;               //!< 0-14, used if m_hasFiw is false
        unsigned m_frame = 0;               //!< 0-127, used if m_hasFiw is false
        bool m_roaming = false;             //!< FIW n flag, used if m_hasFiw is false
        bool m_repeat = false;              //!< FIW r flag (multiple transmission), used if m_hasFiw is false
        unsigned m_traffic = 0;             //!< FIW t3-t0, used if m_hasFiw is false
        std::vector<RxPhase> m_phases;      //!< One entry for each phase of the mode
        double m_quality = 1.0;             //!< Frame level signal quality 0-1 from the receiver, copied to messages
    };

    //! Extra block information word (the words after the first one)
    struct ExtraBiw
    {
        enum Kind { Lost, Ssid1, Date, Time, SystemInfo, Ssid2, Reserved };

        Kind m_kind = Lost;
        int m_index = 0;                    //!< Word index in the phase
        uint32_t m_word = 0;                //!< Data bits
        // Ssid1
        unsigned m_localId = 0;
        unsigned m_coverageZone = 0;
        // Date
        int m_year = 0;
        unsigned m_month = 0;
        unsigned m_day = 0;
        bool m_dateValid = false;
        // Time
        unsigned m_hour = 0;
        unsigned m_minute = 0;
        unsigned m_second7_5 = 0;           //!< Seconds in steps of 7.5 s
        bool m_timeValid = false;
        // SystemInfo
        unsigned m_infoType = 0;            //!< A field: 0-3 system message, 4/5 time zone, 6 channel set up
        unsigned m_info = 0;                //!< 10 bit I field
        unsigned m_zone = 0;                //!< Time zone code (A = 4, 5)
        bool m_zoneDefined = false;         //!< The code is in Table 3.7.2-3 (10000 is not defined)
        int m_zoneMinutes = 0;              //!< Offset from UTC, 0 if the code is not defined
        unsigned m_dst = 0;                 //!< 0 = daylight saving time, 1 = standard
        unsigned m_extendedSeconds = 0;     //!< 0-7 steps of 0.9375 s
        unsigned m_frameOffset = 0;         //!< Channel set up (A = 6)
        unsigned m_maxCarryOn = 0;
        bool m_nidMessage = false;
        bool m_systemMessage = false;
        // Ssid2
        unsigned m_country = 0;
        unsigned m_tmf = 0;
    };

    //! Block information of one phase of the last decoded frame
    struct BlockInfo
    {
        char m_phase = 'A';
        bool m_valid = false;               //!< First block information word usable and offsets valid
        bool m_idle = false;                //!< No address
        unsigned m_priorityWords = 0;
        unsigned m_addressStart = 0;        //!< Word index of the first address word
        unsigned m_vectorStart = 0;         //!< Word index of the first vector word
        unsigned m_carryOn = 0;
        unsigned m_collapse = 0;
        int m_wordsCorrected = 0;
        int m_wordsUncorrectable = 0;
        bool m_waiting = false;             //!< Repeated subframe held back until more copies arrive
        std::vector<ExtraBiw> m_extra;
    };

    struct FrameReport
    {
        bool m_fiwValid = false;
        RadioPagingFlex::Mode m_mode = RadioPagingFlex::Mode1600_2;    //!< Speed and levels of the frame
        bool m_inverted = false;            //!< Polarity of the frame (its network)
        unsigned m_cycle = 0;
        unsigned m_frame = 0;
        bool m_roaming = false;
        bool m_repeat = false;
        int m_transmissions = 1;            //!< 1-4
        int m_tdCollapse = -1;              //!< -1 = system collapse, else 5-7
        unsigned m_traffic = 0;             //!< Low traffic flags t3-t0 when m_repeat is false
        std::vector<BlockInfo> m_blocks;
    };

    //! Message found in a phase
    struct Message
    {
        // Address
        uint64_t m_capcode = 0;
        std::string m_address;              //!< Capcode as 10 digits, zero padded
        bool m_longAddress = false;
        AddressType m_addressType = AddressUnknown;
        bool m_priority = false;            //!< Address in the priority part of the address field

        // Type
        MessageType m_type = TypeUnknown;
        std::string m_typeName;             //!< alphanumeric, secure, numeric, special_numeric, numbered_numeric,
                                            //!< numbered_special_numeric, tone, binary, short_message,
                                            //!< short_instruction, unknown

        // Content. Raw payload: the message words (header words included, vector words excluded; the first
        // word of a long address message is the second vector word) in message order, 21 data bits d0-d20
        // each, d0 first, bits packed 8 per byte with the first bit in the most significant bit. Words
        // that were lost are zero. For a reassembled message the words of all fragments follow each other.
        // A short message and a short instruction have no message words: their payload is the vector word(s)
        std::vector<uint8_t> m_rawData;
        int m_rawBits = 0;
        //! Alphanumeric: 7-bit characters (the default charset), lost characters are '?', the characters of a
        //! received word that could not be corrected are its received bits, unprintable characters stay.
        //! Numeric: digits 0-9 . U space - ] [, lost digits '?', the digits of a received word that could not be
        //! corrected are its received bits. Binary, and secure messages of type t = 01, 10, 11: hex digits of the data bits (see
        //! m_hexNibbleBits). Short message: digits. Else empty
        std::string m_text;
        //! m_text is hex: the data bits as one stream in transmission order (d0 of each word first), this many bits
        //! a digit, 0 if m_text is not hex. A digit with a lost bit is '?'; for a secure message a lost fragment is '|'
        int m_hexNibbleBits = 0;
        bool m_hexLsbFirst = true;          //!< The first bit of a digit is its least significant bit
        bool m_payloadUnknown = false;      //!< The layout of the payload is not published
        bool m_noWords = false;             //!< The vector gives a message of no words (no header, no data): raw is the vector
        std::string m_wordMap;              //!< One character for each message word: . clean, c corrected, X lost; fragments split by |
        //! Alphanumeric and secure characters: one for each byte of m_text, . received, c in a word that was corrected,
        //! u in a received word that could not be corrected (its received bits), X lost ('?'), G a lost fragment of unknown length (its byte in m_text is m_gapCode, not a character).
        //! Numeric digits and the hex digits of HEX/binary and secure binary messages too (a hex digit: the worst of its 4 bits,
        //! G its lost fragment mark '|'). Empty otherwise
        std::string m_charStatus;

        // Errors in the address, vector and message words
        int m_wordsCorrected = 0;
        int m_bitErrors = 0;                //!< Bits corrected
        int m_wordsUncorrectable = 0;
        int m_parityCorrections = 0;        //!< Words where only the parity bit was wrong (counted in m_wordsCorrected)

        // Reception
        RadioPagingFlex::Mode m_mode = RadioPagingFlex::Mode1600_2;
        int m_baud = 1600;
        int m_levels = 2;
        int m_bitRate = 1600;
        bool m_inverted = false;
        double m_quality = 1.0;
        unsigned m_cycle = 0;               //!< Frame of the last fragment
        unsigned m_frame = 0;
        unsigned m_firstCycle = 0;          //!< Frame of the first fragment
        unsigned m_firstFrame = 0;
        char m_phase = 'A';
        //!< Multiple transmission (STD-43A 3.4.2): the transmissions of the content (1 if none) and the copies of the
        //!< subframe received, combined
        int m_transmissions = 1;
        int m_copies = 1;

        // Message header fields
        int m_messageNumber = -1;           //!< N, -1 if the message has none
        bool m_retrieval = false;           //!< R flag (alphanumeric, binary, numbered numeric, short message type 2)
        std::vector<int> m_missedNumbers;   //!< R = 1: numbers of the sequence of the address skipped before this one (missed messages)
        bool m_mailDrop = false;            //!< M flag of an alphanumeric first fragment
        bool m_special = false;             //!< S flag of numbered numeric: special format
        int m_secureType = -1;              //!< t1 t0 of a secure message
        bool m_registrationAck = false;     //!< Secure message with operation code "="
        bool m_recovered = false;           //!< Words taken from an earlier copy of the message
        bool m_duplicate = false;           //!< Retransmission of a message already output (with m_recovered: it repaired it)
        Check m_checksum = CheckNone;       //!< K checksum of the message words
        Check m_signature = CheckNone;      //!< Signature of the text of alphanumeric and binary messages

        // Fragments
        bool m_fragmented = false;          //!< Part of a message sent in fragments
        bool m_isFragment = false;          //!< This is one fragment (only if fragments are reported)
        FragmentKind m_fragment = FragmentUnknown;
        int m_fragmentNumber = 0;           //!< F field of the last fragment
        int m_fragmentCount = 1;            //!< Fragments received
        bool m_complete = true;             //!< False: fragments are missing, timed out or the header is lost
        bool m_fragmentGap = false;         //!< The fragment numbers show a lost fragment

        // Binary
        int m_blocking = 0;                 //!< B field: bits per character, 0 = 16
        bool m_displayRtl = false;          //!< D
        bool m_headerMessage = false;       //!< H
        bool m_statusInfo = false;          //!< I: the first 8 data bits give the encoding method (reserved)
        int m_reservedBits = 0;             //!< s3-s0, reserved, 0 by default
        int m_fragmentControl = -1;         //!< U0 V0 of an alphanumeric fragment after the first (as 2 bits, U0 high), else -1
        bool m_nulError = false;            //!< Enhanced fragmentation: a NUL character that is not a reminder at the end of the fragment
        //! HEX/binary and secure binary: how the end of the data was found. 0 by the termination rules, 1 the
        //! bits at the end do not follow them, 2 can not be told (lost bits at the end, blocking length unknown)
        int m_fillState = 0;
        int m_dataBits = 0;                 //!< Data bits in m_text without the termination bits (binary, secure hex)

        // Short message (vector type 2)
        int m_shortType = -1;               //!< t: 0 digits, 1 source, 2 source with number, 3 reserved
        int m_shortSource = -1;             //!< S2-S0 of types 1 and 2
        bool m_toneOnly = false;            //!< Type 0, every digit a space: the tone only payload
        // Type 0 for a network address (STD-43A Table 3.9.2-1, 6.1.2): the network ID, no digits
        bool m_networkId = false;
        int m_nidServiceArea = -1;          //!< A4-A0
        int m_nidMultiplier = -1;           //!< M2-M0
        int m_nidTrafficFlags = -1;         //!< F3-F0

        // Short instruction
        int m_instructionType = -1;         //!< 0 temporary address, 1 system event
        unsigned m_instructionData = 0;     //!< 14 bit
        int m_instructionSlot = -1;         //!< Temporary address slot
        int m_instructionFrame = -1;        //!< Frame of the group message
        unsigned m_eventFlags = 0;

        // Temporary address (group)
        int m_tempSlot = -1;                //!< Slot 0-15 if the address is a temporary address
        bool m_groupTracked = false;        //!< The group assignment was received
        std::vector<uint64_t> m_groupMembers;
        uint64_t m_groupGeneration = 0;     //!< The group the members are of (the fragments of a message keep it)

        // Network address with a vector other than the short message vector: a system message of the NID
        // (FLEX-93001 3.9.2 p.44). A secure message of type t = 01 is the System NID Change Instruction
        // (FLEX-93001 3.9.7 p.48), its data layout is not published (m_payloadUnknown, as any secure t = 01)
        bool m_nidSystemMessage = false;
        bool m_nidChange = false;

        // Operator messaging address
        std::string m_operatorSubtype;
        std::string m_operatorCategory;
        int64_t m_operatorPayload = -1;     //!< First message word, -1 if lost
    };

    struct Status
    {
        uint32_t m_frames = 0;
        uint32_t m_fiwFailures = 0;
        uint32_t m_codewords = 0;
        uint32_t m_codewordsClean = 0;
        uint32_t m_codewordsCorrected = 0;
        uint32_t m_codewordsUncorrectable = 0;  //!< Includes erased
        uint32_t m_codewordsErased = 0;
        uint32_t m_codewordsFromCentre = 0;     //!< Not correctable, taken from the other decision (RxPhase::m_centreWords)
        uint32_t m_phasesFromCentre = 0;        //!< Phases decoded from the decision on the centre of the symbols
        uint32_t m_messages = 0;
        uint32_t m_duplicates = 0;              //!< Retransmissions of messages already output (marked duplicate)
        uint32_t m_recovered = 0;               //!< Messages repaired with words of an earlier copy
        uint32_t m_incomplete = 0;
        uint32_t m_invalidVectors = 0;
        RadioPagingBerWindow m_ber;             //!< Of the frames received recently
        RadioPagingBerWindow m_phaseBer[4];     //!< The same for each phase A to D
    };

    //! Network time and identification, from the block information words. A value is valid
    //! when it was received several times with the same content
    struct NetworkTime
    {
        bool m_dateValid = false;
        int m_year = 0;
        unsigned m_month = 0;
        unsigned m_day = 0;
        bool m_timeValid = false;
        unsigned m_hour = 0;
        unsigned m_minute = 0;
        unsigned m_second7_5 = 0;           //!< As received, steps of 7.5 s
        bool m_zoneValid = false;
        unsigned m_zone = 0;
        bool m_zoneDefined = false;         //!< The code is in Table 3.7.2-3 (10000 is not defined)
        int m_zoneMinutes = 0;
        unsigned m_dst = 0;                 //!< 0 = daylight saving time, 1 = standard
        unsigned m_extendedSeconds = 0;
        //! 0 standard: the time word is the time of frame 0 of the cycle (STD-43A 3.7.2), the frame offset is added;
        //! 1 direct: the time of the frame that carries it. Standard until the time words tell otherwise
        int m_mode = 0;
        bool m_modeConfirmed = false;       //!< The mode was told by the time words of a cycle, not assumed
        //! Time of the last decoded frame: hour, minute, seconds (valid if m_timeValid)
        unsigned m_currentHour = 0;
        unsigned m_currentMinute = 0;
        double m_currentSecond = 0.0;
        //! Frames since each was last received (confirmed), as of the last frame of the network; -1 not valid
        int m_dateAge = -1;
        int m_timeAge = -1;
        int m_zoneAge = -1;
        // Identification
        bool m_ssid1Valid = false;
        unsigned m_localId = 0;
        unsigned m_coverageZone = 0;
        bool m_ssid2Valid = false;
        unsigned m_country = 0;
        unsigned m_tmf = 0;
    };

    RadioPagingFlexDecoder();

    //! Clears the state of all polarities and the counters
    void reset();
    //! Messages of the frame, and fragments that timed out (marked incomplete)
    std::vector<Message> decodeFrame(const RxFrame& frame);
    //! Fragments in progress as incomplete messages, repeated subframes waiting for copies are
    //! decoded with the copies received. The message history is kept
    std::vector<Message> flush();

    //! A temporary address (group) assigned and waiting for its group message
    struct GroupSlot
    {
        int m_slot = 0;
        std::vector<uint64_t> m_members;
        unsigned m_targetFrame = 0;     //!< Frame of the group message
        unsigned m_setupCycle = 0;      //!< Cycle and frame of the assignment
        unsigned m_setupFrame = 0;
    };

    //! What a temporary address slot is doing (STD-43A 3.8.2 short instruction, temporary address assignment)
    enum GroupState {
        GroupFree,
        GroupAssigning,         //!< Members assigned in the last frame
        GroupWaiting,           //!< Assigned, its group message not come yet
        GroupDelivering,        //!< The group message started, more fragments to come
        GroupDelivered,         //!< The group message ended (recently)
        GroupMissed,            //!< The frame of the group message went by without it (recently)
        GroupDiscarded,         //!< Assigned again to another frame before its group message (recently)
        GroupTimeout            //!< The group message started and its last fragment did not come (recently)
    };
    struct GroupSlotState
    {
        GroupState m_state = GroupFree;
        int m_members = 0;
        unsigned m_targetFrame = 0;
        int m_framesAgo = 0;            //!< Frames since the last assignment, fragment or outcome
    };
    //!< The 16 slots of the network of the polarity; an outcome (delivered, missed, discarded, timeout) is shown for
    //!< m_outcomeFrames frames
    std::vector<GroupSlotState> groupStates(bool inverted) const;
    static const char *groupStateName(GroupState state);
    static const int m_outcomeFrames = 16;

    const Status& status() const { return m_status; }
    const FrameReport& lastFrame() const { return m_report; }
    //!< The last frame of the network of a polarity, and its bit error rates (all phases, each phase A to D)
    const FrameReport& lastFrame(bool inverted) const { return m_pol[inverted ? 1 : 0].m_report; }
    const RadioPagingBerWindow& ber(bool inverted) const { return m_pol[inverted ? 1 : 0].m_ber; }
    const RadioPagingBerWindow& phaseBer(bool inverted, int phase) const { return m_pol[inverted ? 1 : 0].m_phaseBer[phase & 3]; }
    NetworkTime networkTime(bool inverted = false) const;
    //!< Groups in use on the network of the polarity
    std::vector<GroupSlot> groups(bool inverted) const;
    //!< Fragmented messages being reassembled on the network of the polarity
    int reassemblies(bool inverted) const;
    //!< Messages in the message history (duplicate detection), of a polarity (0 normal, 1 inverted) or of both (-1)
    int historyCount(int polarity = -1) const;

    //! Report each fragment as a message too. Default off: the reassembled message only
    void setReportFragments(bool enable) { m_reportFragments = enable; }
    //! Largest message number of the network (0-63, default 63): after it the sequence of an address is 0 again.
    //! Messages with R = 1 of an address are checked for skipped numbers (STD-43A 3.10.1.3 N and R, 4.3.7)
    void setMaxMessageNumber(int maxNumber) { m_maxMessageNumber = std::max(0, std::min(63, maxNumber)); }
    int maxMessageNumber() const { return m_maxMessageNumber; }
    //! Year used to place the 5 bit year of the date in a 32 year period (|year - reference| <= 16). Default 2026
    void setReferenceYear(int year) { m_referenceYear = year; }
    //! The year field is the year - 1994 as sent, no period selection
    void setFixedYearBase(bool enable) { m_fixedYear = enable; }

    static const char *addressTypeName(AddressType type);
    static const char *messageTypeName(MessageType type);
    //!< "unknown", "complete", "first", "middle", "last"
    static const char *fragmentKindName(FragmentKind kind);

    static const int m_reassemblySlots = 16;
    //!< Byte of a lost fragment of unknown length in the text of a reassembled message (SUB), status G
    static const char m_gapCode = 0x1a;
    static const int m_reassemblyMaxLength = 4096;
    static const int m_reassemblyTimeout = 64;      //!< Frames without a fragment
    static const int m_historySize = 1024;
    static const int m_historyBacklog = 16;         //!< Messages of a capcode in the history
    //!< Frames a message stays in the history: shorter than the hour of the frame numbering (1920 frames), so that the
    //!< age of an entry is not ambiguous
    static const int m_historyWindow = 960;
    static const int m_tempSlots = 16;
    //!< Frames a group stays without its group message: the frame of the group message is a frame number of the
    //!< cycle (STD-43A 3.9.6), within 128 frames of the assignment; a group delivering in fragments, from its last one
    static const int m_tempTimeout = 128;
    //!< Pagers of a group: the assignments the protocol can carry in the 128 frames before the group message, 4
    //!< phases a frame of 43 address and vector word pairs each (88 words less the block information word, STD-43A
    //!< 3.4.1), 128 x 4 x 43; the standard sets no other limit
    static const int m_maxGroupMembers = 128 * 4 * 43;
    static const int m_voteThreshold = 3;
    static const int m_voteRing = 8;

private:
    struct Word
    {
        uint32_t m_data = 0;                //!< 21 data bits d0-d20 (d0 in bit 0)
        //! Data bits of a word that could not be corrected: as received, or the word one flip of its three least
        //! reliable bits and BCH correct to (soft decision, the most likely), shown marked uncertain
        uint32_t m_received = 0;
        bool m_hasReceived = false;         //!< m_received holds the bits of a received (not erased) word
        uint8_t m_status = WordNotReceived;
        uint8_t m_bits = 0;                 //!< Bits corrected
        bool m_parity = false;              //!< Only the parity bit was wrong
    };

    struct FrameContext
    {
        unsigned m_cycle = 0;
        unsigned m_frame = 0;
        RadioPagingFlex::Mode m_mode = RadioPagingFlex::Mode1600_2;
        int m_baud = 1600;
        int m_levels = 2;
        int m_bitRate = 1600;
        int m_pol = 0;
        double m_quality = 1.0;
        int m_transmissions = 1;
        int m_copies = 1;                   //!< Copies of the subframe combined (multiple transmission)
        int m_tdCollapse = -1;
    };

    struct AddressContext
    {
        uint64_t m_capcode = 0;
        bool m_long = false;
        AddressType m_type = AddressUnknown;
        uint32_t m_word = 0;
        int m_index = 0;                    //!< Word index of the first address word
        bool m_priority = false;
        int m_tempSlot = -1;
    };

    //! What the reassembly needs of one fragment besides the message fields
    struct FragmentInfo
    {
        uint32_t m_signature = 0;           //!< Signature field of the first fragment
        uint32_t m_signatureSum = 0;        //!< Sum of the characters of this fragment
        bool m_wordsOk = true;              //!< No lost words in the fragment
        std::vector<uint32_t> m_raw;        //!< Message words of the fragment
        std::vector<int8_t> m_bits;         //!< Data bits of a secure hex fragment: 0, 1, -1 lost
        std::string m_bitStatus;            //!< Of m_bits: the status of the word of each bit (., c, X)
    };

    static const int m_maxSubframeWords = 44;   //!< Subframe of a 2x multiple transmission

    struct SubframeCopy
    {
        bool m_received = false;
        int m_uncorrectable = 0;
        Word m_words[m_maxSubframeWords];
    };

    //! The transmissions of one subframe content of a phase (multiple transmission): the k-th
    //! copy is subframe k of the frame (k - 1) repeat intervals after the first transmission
    struct SubframeStore
    {
        uint32_t m_origin = 0;              //!< Frame of the first transmission, 0-1919 in the hour
        uint32_t m_interval = 1;            //!< Repeat interval, frames
        bool m_decoded = false;             //!< Decoded: later copies are only collected
        int m_expected = 0;                 //!< Number of transmissions
        int m_received = 0;
        int m_words = 0;
        char m_phase = 'A';
        FrameContext m_context;             //!< Of the first transmission frame
        SubframeCopy m_copies[4];
    };

    struct TempSlot
    {
        bool m_active = false;
        std::vector<uint64_t> m_members;
        unsigned m_targetFrame = 0;
        unsigned m_setupFrame = 0;
        unsigned m_setupCycle = 0;
        uint32_t m_lastFrame = 0;       //!< Absolute frame of the last assignment, or of the last fragment once delivering
        bool m_delivering = false;      //!< The group message started (its first fragment came)
        uint64_t m_generation = 0;      //!< The group of the slot: a new assignment after the delivery is a new group
    };

    struct Reassembly
    {
        bool m_active = false;
        uint64_t m_capcode = 0;
        int m_number = 0;
        int m_expected = 0;                 //!< Next F
        int m_vector = 0;                   //!< Vector type of the stream
        uint32_t m_signature = 0;
        uint32_t m_signatureSum = 0;
        bool m_signatureValid = true;
        uint32_t m_lastFrame = 0;
        uint32_t m_lastCycle = 0;
        int m_fragments = 0;
        bool m_gap = false;
        bool m_checksumFailed = false;
        std::string m_text;
        std::vector<uint32_t> m_raw;
        std::vector<int8_t> m_bits;         //!< Secure hex: data bits of the fragments, 2 where fragments are lost
        std::string m_bitStatus;            //!< Of m_bits: the status of the word of each bit, G where fragments are lost
        std::string m_charStatus;           //!< Of m_text: see Message::m_charStatus
        Message m_message;                  //!< Fields of the first fragment, counters of all
    };

    struct HistoryEntry
    {
        bool m_active = false;
        uint64_t m_capcode = 0;
        int m_number = 0;
        int m_vector = 0;
        int m_phase = 0;
        int m_wordCount = 0;
        int m_fragCount = 0;
        bool m_long = false;
        int m_blocking = 0;
        bool m_hasUncorrectable = false;
        uint32_t m_frame = 0;
        Word m_words[RadioPagingFlex::m_wordsPerFrame];
    };

    struct BiwState
    {
        bool m_ssid1 = false;
        unsigned m_localId = 0;
        unsigned m_coverage = 0;
        bool m_ssid2 = false;
        unsigned m_country = 0;
        unsigned m_tmf = 0;
        bool m_seenDate = false;
        int m_year = 0;
        unsigned m_month = 0;
        unsigned m_day = 0;
        bool m_seenTime = false;
        unsigned m_hour = 0;
        unsigned m_minute = 0;
        unsigned m_second = 0;
        bool m_seenZone = false;
        unsigned m_zone = 0;
        bool m_zoneDefined = false;
        int m_zoneMinutes = 0;
        unsigned m_dst = 0;
        unsigned m_extSec = 0;
        int m_sysMsgType = -1;              //!< A field of a system message word of this frame, else -1
        int m_voteTimeCount = 0;
        unsigned m_voteHour = 0;
        unsigned m_voteMinute = 0;
        int m_dateRingCount = 0;
        int m_dateRingIndex = 0;
        int m_dateYear[8];
        unsigned m_dateMonth[8];
        unsigned m_dateDay[8];
        int m_zoneRingCount = 0;
        int m_zoneRingIndex = 0;
        unsigned m_zoneZone[8];
        unsigned m_zoneDst[8];
        unsigned m_zoneExt[8];
        // Standard until the time words tell otherwise (STD-43A 3.7.2: the time of frame 0 of the cycle)
        int m_modeDetected = 0;
        bool m_modeConfirmed = false;
        // Frames of the network since each component was last confirmed: expired a little over an hour after
        uint32_t m_dateAge = 0;
        uint32_t m_timeAge = 0;
        uint32_t m_zoneAge = 0;
        unsigned m_timeFrame = 0;           //!< Frame (in its cycle) of the time word last confirmed
        int m_modeVotesStd = 0;
        int m_modeVotesDir = 0;
        unsigned m_modeMinute = 0;
        unsigned m_modeSecond = 0;
        unsigned m_modeFrame = 0;
        unsigned m_modeCycle = 0;
        bool m_modeValid = false;
    };

    struct TempOutcome
    {
        GroupState m_state = GroupFree;
        uint32_t m_absFrame = 0;
        int m_members = 0;
        unsigned m_targetFrame = 0;
    };

    struct PolarityState
    {
        TempSlot m_temp[16];
        TempOutcome m_tempOutcome[16];      //!< How the last group of each slot ended
        uint32_t m_lastAbsFrame = 0;        //!< Of the last frame of the network
        uint64_t m_tempGeneration = 0;      //!< Groups started
        std::vector<Reassembly> m_reassembly;
        std::vector<HistoryEntry> m_history;
        std::vector<SubframeStore> m_stores[4];     //!< Per phase, contents waiting for copies
        BiwState m_biw;
        int m_lastCollapse = 0;             //!< System collapse of the last BIW 1 that decoded (clean or corrected)
        std::unordered_map<uint64_t, int> m_lastNumbers;    //!< Last number with R = 1 of each address
        // The last frame of the network and its bit error rates: the other polarity is another network, whose
        // frames may come between
        FrameReport m_report;
        RadioPagingBerWindow m_ber;
        RadioPagingBerWindow m_phaseBer[4];
    };

    Status m_status;
    FrameReport m_report;
    FrameContext m_ctx;
    PolarityState m_pol[2];
    bool m_reportFragments;
    int m_maxMessageNumber = 63;
    bool m_fixedYear;
    int m_referenceYear;

    // Current phase
    Word m_w[RadioPagingFlex::m_wordsPerFrame];
    char m_name;
    std::vector<Message> *m_out;
    BlockInfo *m_block;

    PolarityState& pol() { return m_pol[m_ctx.m_pol]; }
    uint32_t absFrame() const { return m_ctx.m_cycle * 128 + m_ctx.m_frame; }
    void setTempOutcome(int slot, GroupState state, const TempSlot& group);

    static Word decodeWord(uint32_t raw, bool erased);
    static bool softDecode(uint32_t raw, const uint8_t reliability[32], uint32_t& codeword);
    bool decodeFiw(const RxFrame& frame);
    void decodePhase(const RxPhase& phase);
    void decodeSubframes(int phaseIndex);
    void combineSubframes(SubframeStore& store, Word *out, int& remaining) const;
    void parsePhase();
    void parseExtraBiw(int index);
    void walkAddresses(unsigned addressStart, unsigned vectorStart, unsigned priorityWords);
    void processVector(const AddressContext& a, int j);
    void decodeShortInstruction(Message& proto, const AddressContext& a, int j);
    void decodeAlpha(Message& proto, const AddressContext& a, int j, int vecType, int mw1, int mw2);
    void decodeNumeric(Message& proto, const AddressContext& a, int j, int vecType, int mw1, int mw2);
    void decodeShortMessage(Message& proto, const AddressContext& a, int j);
    void decodeBinary(Message& proto, const AddressContext& a, int j, int mw1, int mw2);
    void handleTemporaryAddress(const AddressContext& a, int j, int vecType, int mw1, Message& proto);
    void handleFragment(Message& msg, const AddressContext& a, int vector, const FragmentInfo& info);
    static Check combineChecks(Check a, Check b);

    // Messages
    Message newMessage(const AddressContext& a) const;
    void countWord(Message& msg, const Word& word) const;
    void countWords(Message& msg, int first, int last) const;
    static void setRaw(Message& msg, const std::vector<uint32_t>& words);
    void emitMessage(Message& msg);
    bool wordOk(int index) const;

    // Fragment reassembly
    int reassemblyFind(uint64_t capcode, int number);
    int reassemblyAlloc(uint64_t capcode, int number, int vector);
    void reassemblyInvalidate(uint64_t capcode, int excludeNumber);
    void reassemblyExpire();
    void emitIncomplete(Reassembly& slot);
    //!< A lost fragment of unknown length in the characters of a reassembly: m_gapCode, status G
    static void appendGap(Reassembly& slot);

    // History: as a pager, the last m_historyBacklog messages of each capcode
    //!< A stored copy of a message among the messages of the capcode: the same number, vector, phase and length,
    //!< and the same words (K and R apart, words lost in either copy not compared). Null if there is none
    HistoryEntry *historyFind(uint64_t capcode, int number, int vector, int phase, const Word *words, int wordCount,
        int fragCount, bool isLong, int blocking);
    static uint32_t retransmissionMask(int vector, bool isLong, int index);
    //!< The numbers skipped before the message in the sequence of its address (R = 1 only; group messages of a
    //!< temporary address have no numbers, 3.8.2.3)
    void checkSequence(Message& msg, const AddressContext& a);
    static const size_t m_maxSequenceAddresses = 25000;
    //!< A new message of a capcode: the oldest of its own is replaced when it has m_historyBacklog, else a free entry,
    //!< else the oldest of all
    HistoryEntry *historyStore(uint64_t capcode, int number, int vector, int phase, const Word *words, int wordCount,
        int fragCount, bool isLong, int blocking, uint32_t frame);
    void historyExpire();
    //!< 0 a new message, 1 a duplicate, -1 a duplicate that repairs the stored copy (m_historyMatch): its words are
    //!< those to decode
    int checkRetransmission(uint64_t capcode, bool retrieval, int number, int vector, int phase, const Word *words,
        int wordCount, int fragCount, bool isLong, int blocking);
    HistoryEntry *m_historyMatch = nullptr;     //!< The stored copy found by the last checkRetransmission

    // Network time
    void voteDate(int year, unsigned month, unsigned day);
    void voteTime(unsigned hour, unsigned minute, unsigned second);
    void voteZone(unsigned zone, unsigned dst, unsigned ext);
    static bool dateValid(int year, unsigned month, unsigned day);
    static void nextDay(int y, unsigned m, unsigned d, int& ny, unsigned& nm, unsigned& nd);
    static bool datesAgree(int y1, unsigned m1, unsigned d1, int y2, unsigned m2, unsigned d2);
    //!< The minute of a time word can be the local time of the cycle: zoneOffset is the offset of the zone in minutes
    //!< when it is known, INT_MIN if not
    static bool timeMatchesCycle(unsigned minute, unsigned cycle, int zoneOffset);
    static int zoneMinutes(unsigned zone);

    void expireTempGroups();
    //!< The time components of the network of the frame age by the frames elapsed; expired after an hour
    void expireTime(uint32_t elapsed);
    //!< Decode the combined copies of a subframe content in the context of its first transmission
    void decodeStore(SubframeStore& store);
};

#endif // MODEMRADIOPAGING_RADIOPAGINGFLEXDECODER_H
