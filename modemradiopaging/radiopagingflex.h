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

#ifndef MODEMRADIOPAGING_RADIOPAGINGFLEX_H
#define MODEMRADIOPAGING_RADIOPAGINGFLEX_H

#include <cstdint>
#include <string>
#include <vector>

#include "export.h"

// FLEX transmission format: codeword helpers and frame builder.
//
// A frame is a sync part sent at 1600 baud with 2 levels (bit sync, a code naming the
// frame mode, the frame information word) followed by a second sync (S2) and the data
// part at the rate and number of levels of the frame mode. The data part is 11 blocks
// of 8 codewords for each phase. Codewords of a block are interleaved: the block is
// sent as 32 bytes, byte i holding bit i (first transmitted bit first) of codeword 0
// to 7 in its bits 7 to 0.
//
// Codeword: 21 data bits d0 to d20 (d0 sent first), 10 check bits, even parity. The
// words of this class are in transmission order, bit 31 is sent first, so the
// BCH code word of RadioPagingBCH is built from the data bits in reverse order.
//
// Modes and phases:
//   1600/2  1600 baud, 2 levels, phase A
//   3200/2  3200 baud, 2 levels, phases A and C, bits alternate A, C, A, C
//   3200/4  1600 baud, 4 levels, phases A and C, a symbol is (A bit, C bit)
//   6400/4  3200 baud, 4 levels, phases A, B, C, D, symbols alternate (A,B) and (C,D)
// Symbols: 2 levels 0 or 1, 4 levels 0 to 3. Symbol 1 (2 levels) and 3 (4 levels) are
// the highest frequency. Dibits are sent most significant bit first and mapped with
// a Gray code (00 -> 0, 01 -> 1, 11 -> 2, 10 -> 3). Inverted polarity mirrors the
// levels (symbol s becomes levels - 1 - s) of the whole frame including the sync part.
class MODEMRADIOPAGING_API RadioPagingFlex
{
public:
    enum Mode {
        Mode1600_2,
        Mode3200_2,
        Mode3200_4,
        Mode6400_4
    };

    enum MessageType {
        Tone,           //!< Address only
        Numeric,        //!< 0-9 U space - ] [ . (and _ as - and u as U)
        Alphanumeric,   //!< 7-bit characters
        ShortMessage,   //!< Short message vector (STD-43A 3.9.2), all the data in the vector, see ShortType
        Binary,         //!< HEX/binary message (vector 110, STD-43A 3.10.1.2): hex digits, see BinaryParams
        Secure,         //!< Secure message (vector 000, STD-43A 3.10.1.4), see BinaryParams::m_secureType
        ShortInstruction    //!< Short instruction vector (001, STD-43A 3.9.6), all the data in the vector, see InstructionParams
    };

    //! Formats of a numeric message (STD-43A 3.10.1.1): the vector tells the format; the characters are the same
    enum NumericFormat {
        NumericStandard,        //!< V = 011
        NumericSpecial,         //!< V = 100: the pager may display it as its ID-ROM says
        NumericNumbered,        //!< V = 111, S = 0: with a message number N and the retrieval flag R (8.4)
        NumericNumberedSpecial  //!< V = 111, S = 1
    };

    //! Short instruction vector (STD-43A 3.9.6, Table 3.9.6-1): the instruction i2 i1 i0 in bits 7-9 and the data d0-d10
    //! in bits 10-20
    struct InstructionParams
    {
        enum Type {
            TemporaryAddress = 0,   //!< 000: the pager listens to frame f6-f0 (d0-d6) for the temporary address a3-a0 (d7-d10)
            SystemEvent = 1         //!< 001: system event flags e10-e0 (d0-d10), with the System Event Notification address
        };

        Type m_type = TemporaryAddress;
        int m_slot = 0;             //!< Temporary address a3-a0: 0-15
        int m_frame = 0;            //!< Frame f6-f0 of the group message: 0-127
        unsigned m_events = 0;      //!< System event flags e10-e0: 0-0x7ff
    };

    //! Secure message type field t1 t0 (STD-43A 3.10.1.4)
    enum SecureType {
        SecureAlphanumeric = 0, //!< 00: 7-bit characters (JIS X 0201), unused characters ETX
        SecureSeparate = 1,     //!< 01: data defined separately, hex digits, termination not defined (unused bits 0)
        SecureBinary = 2        //!< 10: binary data, hex digits, terminated as binary data
    };

    //! Fields of the second header word of a HEX/binary message (STD-43A Fig. 3.10.1.2-1) and the type of a
    //! secure message. The data of both are hex digits: digit i holds data bits 4i to 4i+3, the least
    //! significant bit of the digit first (transmission starts from the LSB)
    struct BinaryParams
    {
        int m_blocking = 1;             //!< B: bits of a character 1-16 (16 is sent as 0000); default 0001
        bool m_mailDrop = false;        //!< M (also of an alphanumeric message)
        bool m_rightToLeft = false;     //!< D: display from right to left, not allowed with B = 1
        bool m_header = false;          //!< H: header message, a transparent message of the same number follows
        bool m_statusInfo = false;      //!< I: the first 8 data bits name the encoding method (reserved)
        SecureType m_secureType = SecureAlphanumeric;   //!< Secure message only
    };

    //! Types of short messages (STD-43A Table 3.9.2-1, t1 t0)
    enum ShortType {
        ShortNumeric = 0,   //!< 00: 3 digits (8 for a long address), numeric characters, the unused ones spaces.
                            //!< Every digit a space: the tone only payload
        ShortSource = 1,    //!< 01: source code S2-S0 (8.5 Source Indication), the other bits not used (0)
        ShortNumbered = 2   //!< 10: source code, message number N5-N0, retrieval flag R0 (8.4 Message Numbering)
    };

    static const int m_wordsPerBlock = 8;
    static const int m_blocksPerFrame = 11;
    static const int m_wordsPerFrame = 88;      //!< Per phase
    static const int m_maxPhases = 4;
    static const int m_syncBaud = 1600;
    static const int m_maxCollapse = 7;
    static const int m_maxAlphaChars = 251;     //!< Characters of an alphanumeric message in one frame, short address (STD-43A 4.1); longer messages are fragmented
    //!< Most message words (message field, and the second vector word of a long address) of one message in
    //!< a frame (STD-43A 4.1): 88 less the block information word, the address and the vector words: 85
    //!< with a short address, 84 with a long one (2 address words, 2 vector words, the first message word
    //!< in the second vector word)
    static int maxMessageWords(uint64_t capcode) { return isLongAddress(capcode) ? m_wordsPerFrame - 4 : m_wordsPerFrame - 3; }
    //!< Most characters of an alphanumeric message in one frame: 3 a word after the header word, less the
    //!< signature slot: 251 with a short address, 248 with a long one
    static int maxAlphaChars(uint64_t capcode) { return 3 * (maxMessageWords(capcode) - 1) - 1; }
    static const int m_maxNumericChars = 41;
    static const int m_maxNumberedNumericChars = 39;    //!< The numbered format has 10 overhead bits in the first word (STD-43A 3.10.1.1.2)
    static bool isNumberedFormat(NumericFormat format) { return (format == NumericNumbered) || (format == NumericNumberedSpecial); }
    static const int m_maxPriorityWords = 15;   //!< Priority address words of a phase
    static const uint64_t m_shortMin = 1;
    static const uint64_t m_shortMax = 1933312;
    static const uint64_t m_longMin = 2101249;
    static const uint64_t m_longMax = 4297068542ULL;

    // Message of one phase of a frame
    struct PhaseMessage
    {
        uint64_t m_capcode = 0;
        MessageType m_type = Alphanumeric;
        std::string m_text;
        bool m_priority = false;    //!< Address in the priority part of the address field
        int m_number = 0;           //!< Message number N 0-63: alphanumeric, HEX/binary, secure and numbered short messages
        bool m_retrieval = true;    //!< Message retrieval flag R: alphanumeric, HEX/binary and numbered short messages
        ShortType m_shortType = ShortNumeric;   //!< Short message only
        int m_source = 0;           //!< Short message source and numbered types: source code S2-S0, 0-7
        BinaryParams m_binary;      //!< HEX/binary and secure messages only
        InstructionParams m_instruction;    //!< Short instruction only
        NumericFormat m_numericFormat = NumericStandard;    //!< Numeric only; the numbered formats use m_number and m_retrieval
        int m_fragment = 0;         //!< Alphanumeric, HEX/binary and secure: fragment index, 0 the first (fragmentMessage)
        int m_fragments = 1;        //!< Alphanumeric, HEX/binary and secure: fragments of the message
        int m_signature = -1;       //!< Alphanumeric and HEX/binary first fragment: signature of the whole message, -1 of the text
    };

    static const int m_maxFragments = 64;   //!< Fragments of an alphanumeric, HEX/binary or secure message (an implementation limit)

    //!< Split an alphanumeric, HEX/binary or secure message into fragments of one frame each (STD-43A 4.1,
    //!< 3.10.1.2, 3.10.1.3, 3.10.1.4). A fragment that is not the last is packed full: its data end on a character
    //!< boundary with fewer than a character of bits left in its last word, no fill and no padding word
    //!< (alphanumeric: 3 characters a word after the signature slot of the first fragment, no ETX; binary data:
    //!< a multiple of the blocking length and of 4 bits, the hex digits; secure binary and data defined
    //!< separately: whole words; secure alphanumeric: 3 characters a word, no ETX). The last fragment takes
    //!< the rest with its termination. The fragments carry m_fragment, m_fragments and, alphanumeric and
    //!< HEX/binary, the signature of the whole message. Other types give one piece. False and an error if the
    //!< message is not valid
    //!< startFragment: the message is the rest of a message from that fragment on (its header words and signature
    //!< slot are of the first fragment only). firstMessageWords: the message words of the first piece, when the frame
    //!< it goes in has less room than STD-43A 4.1 allows (block information words 2 to 4, other messages); 0 all
    static bool fragmentMessage(const PhaseMessage& message, std::vector<PhaseMessage>& fragments, std::string& error,
        int startFragment = 0, int firstMessageWords = 0);

    struct PhaseParams
    {
        Mode m_mode = Mode1600_2;
        int m_phase = 0;            //!< Phase index: 0 to phaseCount(mode) - 1
        int m_collapse = 0;         //!< 0-7, sent in the block information word
        int m_carryOn = 0;          //!< 0-3, sent in the block information word
        std::vector<uint32_t> m_extraBiw;   //!< Block information words 2 to 4 (up to 3, as transmitted): before the address field
    };

    struct PhaseResult
    {
        bool m_ok = false;
        std::string m_error;
        std::vector<uint32_t> m_words;      //!< 88 interleaved words: 11 blocks of 32 bytes, big endian
        std::vector<bool> m_packed;         //!< Per input message, true if placed in the phase
        int m_addressWords = 0;             //!< Words of the address field
    };

    struct FrameParams
    {
        Mode m_mode = Mode1600_2;
        unsigned m_cycle = 0;       //!< 0-14
        unsigned m_frame = 0;       //!< 0-127
        unsigned m_traffic = 0;     //!< 4 low traffic flags of the frame information word
        bool m_roaming = false;     //!< Roaming network bit of the frame information word
    };

    struct FrameBytes
    {
        std::vector<uint8_t> m_sync;    //!< Bit sync, mode code, frame information word: 18 bytes at 1600 baud 2 levels
        std::vector<uint8_t> m_data;    //!< Second sync and phase data at the rate of the mode
    };

    struct SymbolBlock
    {
        int m_baud = 0;
        int m_levels = 0;                   //!< 2 or 4
        std::vector<uint8_t> m_symbols;     //!< 0 to m_levels - 1, 0 = lowest frequency
    };

    static int phaseCount(Mode mode);
    static int baud(Mode mode);
    static int levels(Mode mode);
    static int bitRate(Mode mode);
    static const char *modeName(Mode mode);

    //!< Word with the 21 data bits (d0 = bit 0) of a data word: reversed and extended with check bits and parity
    static uint32_t encodeWord(uint32_t data);
    //!< Data word (d0 = bit 0) of a transmitted word, no error correction
    static uint32_t wordData(uint32_t word);
    //!< Sets the 4-bit checksum in bits 3-0 of an information word (bits 3-0 must be 0)
    static uint32_t addChecksum(uint32_t data);
    static bool checksumValid(uint32_t data);

    static uint32_t frameInfoWord(unsigned cycle, unsigned frame, unsigned roaming, unsigned repeat, unsigned traffic);
    //!< Simulcast system ID words (STD-43A Table 3.7.2-1, 6.1.1): block information words 000 (local
    //!< channel ID 9 bits, coverage zone 5 bits) and 111 (country code 10 bits, traffic management flags 4 bits)
    static uint32_t ssid1Word(unsigned localId, unsigned coverageZone);
    static uint32_t ssid2Word(unsigned countryCode, unsigned trafficFlags);
    //!< Phase that carries the SSID words of a frame (STD-43A 6.1.1.3): 4 phases, the phase is the frame
    //!< modulo 4; 2 phases (a and c), frames 0 and 1 in the first, 2 and 3 in the second; 1 phase, the only one
    static int ssidPhase(Mode mode, unsigned frame);
    //!< addressStart: number of block information words minus 1, vectorStart: word index of the first vector
    static uint32_t blockInfoWord(unsigned priorityWords, unsigned addressStart, unsigned vectorStart, unsigned carryOn, unsigned collapse);

    // Time related block information words (STD-43A 3.7.2, Table 3.7.2-1 and 3.7.2-2). The time is the local
    // time (standard time of the zone, daylight saving time when it applies)
    //!< Date word, format 001: Y4-Y0 (year in a period of 32 years from 1994, the year modulo 32 after 2025),
    //!< d4-d0 day 1-31, m3-m0 month 1-12
    static uint32_t dateWord(int year, unsigned month, unsigned day);
    //!< The year itself in 1994-2025 (the period of the year field), else the latest year of that period with the
    //!< same calendar: leap or not, and the same weekday of January 1
    static int equivalentYear(int year);
    //!< Time word, format 010: H4-H0 hour 0-23, M5-M0 minute 0-59, s2-s0 second in eighths of a minute (7.5 s)
    static uint32_t timeWord(unsigned hour, unsigned minute, unsigned eighths);
    //!< System information word, format 101, A3-A0 = 0101 Additional Time Instruction: Z4-Z0 time zone code
    //!< (Table 3.7.2-3), L0 0 for daylight saving time and 1 for standard time, S5-S3 extended second (0-7,
    //!< 1/64 minute steps added to the time word)
    static uint32_t timeZoneWord(unsigned zone, bool daylightSaving, unsigned extendedSeconds);
    //!< Offset from UTC in minutes of a time zone code (Table 3.7.2-3); false for code 10000, which is not defined
    static bool timeZoneMinutes(unsigned zone, int& minutes);
    //!< Time zone code of an offset from UTC in minutes, -1 if the table has no such zone. UTC is code 00000
    static int timeZoneCode(int minutes);
    //!< Civil date and time of a number of seconds from the Unix epoch (no time zone applied)
    static void civilTime(int64_t seconds, int& year, unsigned& month, unsigned& day, unsigned& hour, unsigned& minute,
        unsigned& second);

    static bool isShortAddress(uint64_t capcode);
    static bool isLongAddress(uint64_t capcode);
    static bool isValidAddress(uint64_t capcode);

    //!< A capcode as written (STD-43A Appendix A): the digits of the address, or the CAPCODE forms with letters
    //!< [P|Q|R|S][fff][b]<letter><digits>. Only the address is sent, the rest tells how the pager is set up.
    //!< Exported on its own: the export of the class does not cover its nested classes
    struct MODEMRADIOPAGING_API CapcodeText
    {
        uint64_t m_capcode = 0;     //!< The address
        int m_digits = 0;           //!< Digits of the address field
        char m_letter = 0;          //!< A-L (standard rule), U-Z (no rule), 0 none
        char m_roaming = 0;         //!< 2nd alpha character P, Q, R or S, 0 none
        int m_frame = -1;           //!< fff: frame 0-127 of the pager, -1 none
        int m_collapse = -1;        //!< b: pager collapse 0-7, -1 none
        //!< Single (0), any (1) or all (2) phase pager, -1 not told (no letter)
        int phaseType() const;
        //!< A-L: subtracted from the address before the standard rule (the 1st to 4th address of a pager)
        int subtract() const;
        //!< U-X: phase 0-3 of the pager, else -1
        int phase() const;
        //!< The pager collapse: b, or 4 for A-L without b, -1 not told
        int pagerCollapse() const;
    };
    //!< False and the reason if the text is not a capcode form (the address range is not checked)
    static bool parseCapcode(const std::string& text, CapcodeText& capcode, std::string& error);
    //!< The address type of a capcode (Table A-1, operator messaging addresses Table 3.8.2.4-1)
    static std::string capcodeType(uint64_t capcode);
    //!< What a capcode as written tells: address type, pager phase, frame and collapse, and the frame and phase
    //!< of the standard rule (Appendix A 3): phase = (address / 4) mod 4, frame = (address / 16) mod 128
    static std::string capcodeDescription(const std::string& text);
    //!< Temporary addresses and the System Event Notification address, as capcodes (the address word less 0x8000, as
    //!< the short addresses): a message to one of them is the group message of a short instruction or the
    //!< notification (STD-43A 3.8.2.3, 3.8.2.4)
    static const uint64_t m_temporaryBase = 2029568;        //!< Address word 1F7800
    static const uint64_t m_temporaryCount = 16;
    static const uint64_t m_systemEventAddress = 2029599;   //!< Address word 1F781F
    static bool isTemporaryAddress(uint64_t capcode) { return (capcode >= m_temporaryBase) && (capcode < m_temporaryBase + m_temporaryCount); }
    //!< Short instruction vector (STD-43A Fig. 3.9.6-1): i2-i0 in bits 7-9, d0-d10 in bits 10-20; the second word of a
    //!< long address is 0 (the instruction is for short addresses and the notification, words[1] is 0)
    static void shortInstructionVector(const InstructionParams& instruction, bool longAddress, uint32_t words[2]);
    //!< Address of the message records: the capcode in 10 digits (the largest, 4297068542, has 10), the same in
    //!< RX, TX and the message center
    static std::string addressText(uint64_t capcode);
    static uint32_t shortAddressWord(uint32_t capcode);
    static bool longAddressWords(uint64_t capcode, uint32_t words[2]);
    //!< Capcode of a short address word (data bits) or of a pair of long address words (data bits)
    static uint64_t shortAddressCapcode(uint32_t data);
    static bool longAddressCapcode(uint32_t data1, uint32_t data2, uint64_t& capcode);

    static uint32_t alphaVector(unsigned start, unsigned words);
    static uint32_t numericVector(unsigned start, unsigned words, unsigned kBits, unsigned vectorType = m_vectorNumeric);
    //!< Short message vector (STD-43A Fig. 3.9.2-1, Table 3.9.2-1): t1 t0 in bits 7-8, d0-d11 in bits 9-20.
    //!< Numeric: digits a to c in d0-d11, d to h in d12-d31 of the second word of a long address (d32 = 0),
    //!< unused digits are spaces (HEX C). Source: S2-S0 in d0-d2. Numbered: S2-S0 in d0-d2, N5-N0 in d3-d8,
    //!< R0 in d9. Bits not used and idle bits are 0. words[0] the vector, words[1] the second word (long
    //!< address only; the check character does not cover it)
    static void shortMessageVector(ShortType type, const std::string& digits, int source, int number, bool retrieval,
        bool longAddress, uint32_t words[2]);
    static const int m_maxShortDigits = 3;
    static const int m_maxShortDigitsLong = 8;
    static const unsigned m_vectorTone = 2;
    static const unsigned m_vectorNumeric = 3;
    static const unsigned m_vectorSpecialNumeric = 4;
    static const unsigned m_vectorNumberedNumeric = 7;
    static const unsigned m_vectorAlpha = 5;
    static const unsigned m_vectorBinary = 6;
    static const unsigned m_vectorSecure = 0;
    static const unsigned m_vectorInstruction = 1;
    //!< Vector of a message with a 7-bit length (alphanumeric, HEX/binary, secure)
    static uint32_t messageVector(unsigned type, unsigned start, unsigned words);

    //!< Fill a phase with idle words: alternating all ones and all zeros, all zeros in the
    //!< phases carrying the least significant bit of 4-level symbols
    static void fillIdle(uint32_t *words, int phase, Mode mode);
    //!< Interleave the 8 words of a block in place (see class description)
    static void interleaveBlock(uint32_t *words);
    static void deinterleaveBlock(uint32_t *words);

    //!< Words of address, vector and message of a message (long addresses: the first message
    //!< word is in the second vector word). -1 and error if the message cannot be sent
    static int messageCost(const PhaseMessage& message, std::string *error = nullptr);
    //!< Check the capcode and text of a message
    static bool validateMessage(uint64_t capcode, MessageType type, const std::string& text, std::string& error);
    static bool validateMessage(uint64_t capcode, MessageType type, const std::string& text, std::string& error,
        const BinaryParams& binary);

    //!< Message words (headers, data and termination) of a fragment of a HEX/binary message, data bits d0
    //!< in bit 0. number: message number N, retrieval: R. The first fragment has header word 2 with the
    //!< signature (signature: of the whole message, -1 of hex); the last one the termination. Empty if the
    //!< parameters or the digits are not valid
    static std::vector<uint32_t> binaryBody(const std::string& hex, const BinaryParams& binary, int number, bool retrieval,
        int fragment = 0, int fragments = 1, int signature = -1);
    //!< Message words of a fragment of a secure message, data bits d0 in bit 0
    static std::vector<uint32_t> secureBody(const std::string& text, SecureType type, int number, int fragment = 0, int fragments = 1);
    //!< Message words of a fragment of a HEX/binary (header words: 2 in the first fragment, 1 in the others) or
    //!< secure binary (1 header word) message: the header and data words, and the termination word of a last
    //!< fragment. -1 if the digits are not valid
    static int binaryWords(const std::string& hex, int blocking, int headerWords, bool lastFragment = true);
    //!< Fragment number F of fragment index k (0 the first): 11, then 00, 01, 10, 00, ... (3.10.1.2 p.59)
    static unsigned fragmentNumber(int k) { return (k == 0) ? 3u : (unsigned) ((k - 1) % 3); }

    //!< Place messages (priority addresses first, then others, tone only addresses last) in
    //!< a phase and interleave. Messages that do not fit are left out
    static PhaseResult buildPhase(const PhaseParams& params, const std::vector<PhaseMessage>& messages);

    //!< Low traffic flags t0 (phase A) to t3 (phase D) for the number of address field words of
    //!< each phase (one block information word): set if the address field ends in block 0
    static unsigned lowTrafficFlags(const std::vector<int>& addressWords, int phases);

    //!< Sync part, frame information word, second sync and the phases (one vector of
    //!< m_wordsPerFrame interleaved words for each phase of the mode)
    static FrameBytes assemble(const FrameParams& params, const std::vector<std::vector<uint32_t>>& phases);
    //!< Symbols of the sync part (1600 baud, 2 levels) and of the data part (rate of the mode)
    static std::vector<SymbolBlock> toSymbols(const FrameBytes& bytes, Mode mode, bool inverted);
    //!< Symbols of bytes (most significant bit first) for the given number of levels
    static std::vector<uint8_t> bytesToSymbols(const std::vector<uint8_t>& bytes, int levels, bool inverted);

    //!< Emergency re-synchronization (STD-43A 3.2.1, Table 3.2.1-1): BS, Ar, inverted BS and
    //!< inverted Ar repeated at 1600 baud 2 levels, for the given number of bits (the last
    //!< cycle may be cut)
    static SymbolBlock ers(int bits, bool inverted);
    //!< Bits at 1600 baud the re-synchronization lasts at least: the battery saving cycle of a
    //!< pager with the collapse value, 2^collapse frames of 1.875 s (STD-43A 3.2.1)
    static int ersMinBits(int collapse);
    static const int m_frameMs = 1875;          //!< Frame duration, milliseconds
    static const int m_bitsPerFrame = 3000;     //!< At 1600 baud

    static const char m_numericChars[17];   //!< Index = 4-bit value
};

#endif // MODEMRADIOPAGING_RADIOPAGINGFLEX_H
