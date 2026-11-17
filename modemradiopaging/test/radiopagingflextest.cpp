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

// Self test of the FLEX transmitter: frame builder and scheduler. A small decoder
// working on codewords (de-interleave, block information word, address field, vector
// field, message field) checks the frames the encoder produces. The frames of
// radiopagingflexgolden.inc are compared byte for byte. Prints one line per check and
// returns non-zero if any check fails.

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "radiopagingbch.h"
#include "radiopagingflex.h"
#include "radiopagingflexscheduler.h"

namespace {

int failures = 0;

void check(bool ok, const char *what)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) {
        failures++;
    }
}

typedef RadioPagingFlex Flex;
typedef RadioPagingFlexEncoder Encoder;
typedef RadioPagingFlexScheduler Sched;

// ---------------------------------------------------------------------------
// Decoder
// ---------------------------------------------------------------------------

struct DecodedMessage
{
    uint64_t m_capcode = 0;
    int m_kind = 0;             // 0 tone only, 1 numeric, 2 alphanumeric
    std::string m_text;
    int m_number = -1;
    bool m_priority = false;
    bool m_ok = true;
};

struct DecodedPhase
{
    bool m_ok = false;
    unsigned m_collapse = 0;
    unsigned m_carryOn = 0;
    unsigned m_priorityWords = 0;
    unsigned m_vectorStart = 0;
    std::vector<DecodedMessage> m_messages;
};

struct DecodedFrame
{
    bool m_ok = false;
    int m_mode = 0;
    unsigned m_cycle = 0;
    unsigned m_frame = 0;
    unsigned m_traffic = 0;
    std::vector<DecodedPhase> m_phases;
};

// A codes of the four modes, bytes in transmission order
const uint8_t modeCodes[4][4] = {
    {0x78, 0xf3, 0x59, 0x39},
    {0x84, 0xe7, 0x59, 0x39},
    {0x4f, 0x97, 0x59, 0x39},
    {0x21, 0x5f, 0x59, 0x39}
};

uint32_t groupSum(uint32_t word)
{
    return (word & 0xff) + ((word >> 8) & 0xff) + ((word >> 16) & 0x1f);
}

bool decodeWord(uint32_t word, uint32_t& data)
{
    RadioPagingBCH::Result r = RadioPagingBCH::decode(word);

    if ((r.m_bitErrors != 0) || r.m_parityError) {
        return false;
    }

    data = Flex::wordData(r.m_word);
    return true;
}

bool isLongFirst(uint32_t data)
{
    return ((data >= 1) && (data <= 32768)) || ((data >= 2064383u) && (data < 2064383u + 32768));
}

DecodedMessage decodeAlpha(const std::vector<uint32_t>& body)
{
    DecodedMessage m;
    m.m_kind = 2;
    m.m_ok = false;

    if (body.size() < 2) {
        return m;
    }

    uint32_t sum = 0;
    for (uint32_t w : body) {
        sum += groupSum(w);
    }
    // The check bits are included in the sum; the ones complement of the rest gives them
    uint32_t rest = sum - groupSum(body[0] & 0x3ff);
    bool kOk = ((~rest) & 0x3ff) == (body[0] & 0x3ff);
    bool headerOk = (((body[0] >> 11) & 3) == 3) && !((body[0] >> 10) & 1) && ((body[0] >> 19) & 1);

    m.m_number = (body[0] >> 13) & 0x3f;

    std::string text;
    uint32_t signature = body[1] & 0x7f;
    for (size_t i = 1; i < body.size(); i++)
    {
        for (int shift = (i == 1) ? 7 : 0; shift < 21; shift += 7) {
            text.push_back((char) ((body[i] >> shift) & 0x7f));
        }
    }
    while (!text.empty() && (text.back() == 0x03)) {
        text.pop_back();
    }

    uint32_t sigSum = 0;
    for (char c : text) {
        sigSum += (unsigned char) c;
    }
    m.m_text = text;
    m.m_ok = kOk && headerOk && (signature == ((~sigSum) & 0x7f));
    return m;
}

DecodedMessage decodeNumeric(const std::vector<uint32_t>& body, unsigned kLow)
{
    static const char digits[] = "0123456789.U -][";
    DecodedMessage m;
    m.m_kind = 1;

    if (body.empty()) {
        m.m_ok = false;
        return m;
    }

    uint32_t sum = 0;
    for (size_t i = 0; i < body.size(); i++) {
        sum += groupSum((i == 0) ? (body[0] & ~3u) : body[i]);
    }
    sum &= 0xff;
    sum = (sum & 0x3f) + (sum >> 6);
    uint32_t k = ~sum;
    m.m_ok = (((k >> 4) & 3) == (body[0] & 3)) && ((k & 0xf) == kLow);

    int totalBits = (int) body.size() * 21;
    for (int bit = 2; bit + 4 <= totalBits; bit += 4)
    {
        unsigned v = 0;
        for (int b = 0; b < 4; b++) {
            v |= ((body[(bit + b) / 21] >> ((bit + b) % 21)) & 1) << b;
        }
        m.m_text.push_back(digits[v]);
    }
    while (!m.m_text.empty() && (m.m_text.back() == ' ')) {
        m.m_text.pop_back();
    }

    return m;
}

// words: the 88 interleaved words of a phase
DecodedPhase decodePhase(const std::vector<uint32_t>& interleaved)
{
    DecodedPhase phase;
    std::vector<uint32_t> words = interleaved;
    std::vector<uint32_t> data(Flex::m_wordsPerFrame);

    if (words.size() != (size_t) Flex::m_wordsPerFrame) {
        return phase;
    }

    for (int b = 0; b < Flex::m_blocksPerFrame; b++) {
        Flex::deinterleaveBlock(&words[b * Flex::m_wordsPerBlock]);
    }
    for (int w = 0; w < Flex::m_wordsPerFrame; w++)
    {
        // Idle words are not codewords: all ones and all zeros
        if (words[w] == 0xffffffffu) {
            data[w] = 0x1fffff;
        } else if (words[w] == 0) {
            data[w] = 0;
        } else if (!decodeWord(words[w], data[w])) {
            return phase;
        }
    }

    uint32_t biw = data[0];
    if (!Flex::checksumValid(biw)) {
        return phase;
    }

    phase.m_priorityWords = (biw >> 4) & 0xf;
    unsigned addressStart = 1 + ((biw >> 8) & 3);
    phase.m_vectorStart = (biw >> 10) & 0x3f;
    phase.m_carryOn = (biw >> 16) & 3;
    phase.m_collapse = (biw >> 18) & 7;

    // Addresses
    struct Address
    {
        uint64_t m_capcode;
        bool m_long;
    };
    std::vector<Address> addresses;

    for (unsigned w = addressStart; w < phase.m_vectorStart;)
    {
        Address a;
        if (isLongFirst(data[w]))
        {
            if (w + 1 >= phase.m_vectorStart || !Flex::longAddressCapcode(data[w], data[w + 1], a.m_capcode)) {
                return phase;
            }
            a.m_long = true;
            w += 2;
        }
        else
        {
            a.m_capcode = Flex::shortAddressCapcode(data[w]);
            a.m_long = false;
            w++;
        }
        addresses.push_back(a);
    }

    // Vector field: one vector per address (two for a long address) up to the message field.
    // The first address without a vector starts the tone only addresses.
    unsigned ptr = phase.m_vectorStart;
    unsigned minStart = 1000;
    unsigned priorityLeft = phase.m_priorityWords;

    for (const Address& a : addresses)
    {
        DecodedMessage m;
        m.m_capcode = a.m_capcode;
        unsigned addressWords = a.m_long ? 2 : 1;
        m.m_priority = (priorityLeft >= addressWords);
        priorityLeft = m.m_priority ? priorityLeft - addressWords : 0;

        uint32_t v = (ptr < (unsigned) Flex::m_wordsPerFrame) ? data[ptr] : 0;
        unsigned type = (v >> 4) & 7;

        if ((ptr >= minStart) || !Flex::checksumValid(v) || ((type != 3) && (type != 5)))
        {
            m.m_kind = 0;
            phase.m_messages.push_back(m);
            continue;
        }

        unsigned start = (v >> 7) & 0x7f;
        unsigned count = (type == 5) ? ((v >> 14) & 0x7f) : (((v >> 14) & 7) + 1);
        std::vector<uint32_t> body;

        if (a.m_long)
        {
            body.push_back(data[ptr + 1]);
            if (start != ptr + 1) {
                minStart = std::min(minStart, start);
            }
            for (unsigned i = 0; i + 1 < count; i++) {
                body.push_back(data[start + i]);
            }
            ptr += 2;
        }
        else
        {
            minStart = std::min(minStart, start);
            for (unsigned i = 0; i < count; i++) {
                body.push_back(data[start + i]);
            }
            ptr++;
        }

        DecodedMessage d = (type == 5) ? decodeAlpha(body) : decodeNumeric(body, (v >> 17) & 0xf);
        d.m_capcode = m.m_capcode;
        d.m_priority = m.m_priority;
        phase.m_messages.push_back(d);
    }

    phase.m_ok = true;
    return phase;
}

// Symbols to bytes: the inverse of the mapping of the transmitter
std::vector<uint8_t> symbolsToBytes(const std::vector<uint8_t>& symbols, int levels, bool inverted)
{
    static const int ungray[4] = {0, 1, 3, 2};
    std::vector<uint8_t> bytes;
    int bitsPerSymbol = (levels == 2) ? 1 : 2;
    int cur = 0;
    int n = 0;

    for (uint8_t s : symbols)
    {
        int v = inverted ? levels - 1 - s : s;
        int bits = (levels == 2) ? v : ungray[v];
        cur = (cur << bitsPerSymbol) | bits;
        n += bitsPerSymbol;
        if (n == 8)
        {
            bytes.push_back(cur);
            cur = 0;
            n = 0;
        }
    }

    return bytes;
}

DecodedFrame decodeFrame(const Encoder::Frame& frame)
{
    DecodedFrame out;

    if (frame.m_blocks.size() != 2) {
        return out;
    }

    const RadioPagingFlex::SymbolBlock& sync = frame.m_blocks[0];
    const RadioPagingFlex::SymbolBlock& data = frame.m_blocks[1];

    if ((sync.m_baud != 1600) || (sync.m_levels != 2)) {
        return out;
    }

    int mode = (data.m_baud == 1600) ? ((data.m_levels == 2) ? 0 : 2) : ((data.m_levels == 2) ? 1 : 3);
    std::vector<uint8_t> s = symbolsToBytes(sync.m_symbols, 2, frame.m_inverted);
    std::vector<uint8_t> d = symbolsToBytes(data.m_symbols, data.m_levels, frame.m_inverted);

    // Sync part: bit sync, mode code, B, inverted mode code, frame information word
    if (s.size() != 18) {
        return out;
    }
    for (int i = 0; i < 4; i++)
    {
        if ((s[i] != 0xaa) || (s[4 + i] != modeCodes[mode][i]) || (s[10 + i] != (uint8_t) ~modeCodes[mode][i])) {
            return out;
        }
    }
    if ((s[8] != 0x55) || (s[9] != 0x55)) {
        return out;
    }

    uint32_t fiwWord = ((uint32_t) s[14] << 24) | ((uint32_t) s[15] << 16) | ((uint32_t) s[16] << 8) | s[17];
    uint32_t fiw;
    if (!decodeWord(fiwWord, fiw) || !Flex::checksumValid(fiw)) {
        return out;
    }

    out.m_mode = mode;
    out.m_cycle = (fiw >> 4) & 0xf;
    out.m_frame = (fiw >> 8) & 0x7f;
    out.m_traffic = (fiw >> 17) & 0xf;

    // Second sync: C pattern after the alternating pattern
    int phases = Flex::phaseCount((Flex::Mode) mode);
    int patternBits = (mode == 0) ? 4 : (mode == 3) ? 64 : 24;
    size_t s2Bytes = (2 * patternBits + 32) / 8;
    if (d.size() != s2Bytes + (size_t) (phases * Flex::m_wordsPerFrame * 4)) {
        return out;
    }

    // C: 16 bits after the pattern. 4-level patterns are 2 bits per symbol, so the bit positions are the same.
    uint32_t c = 0;
    for (int i = 0; i < 16; i++)
    {
        int bitPos = patternBits + i;
        c = (c << 1) | ((d[bitPos / 8] >> (7 - bitPos % 8)) & 1);
    }
    if (c != 0xed84) {
        return out;
    }

    // Phase data: bit k of the data bit stream belongs to phase k mod phases
    std::vector<std::vector<uint32_t>> words(phases, std::vector<uint32_t>(Flex::m_wordsPerFrame, 0));
    size_t totalBits = (size_t) phases * Flex::m_wordsPerFrame * 32;

    for (size_t k = 0; k < totalBits; k++)
    {
        size_t bitPos = s2Bytes * 8 + k;
        unsigned bit = (d[bitPos / 8] >> (7 - bitPos % 8)) & 1;
        size_t phase = k % phases;
        size_t index = k / phases;
        words[phase][index / 32] |= bit << (31 - index % 32);
    }

    for (int p = 0; p < phases; p++) {
        out.m_phases.push_back(decodePhase(words[p]));
    }

    out.m_ok = true;
    for (const DecodedPhase& p : out.m_phases)
    {
        if (!p.m_ok) {
            out.m_ok = false;
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

Encoder::TxMessage makeMessage(uint64_t capcode, Flex::MessageType type, const std::string& text, bool priority = false)
{
    Encoder::TxMessage m;
    m.m_capcode = capcode;
    m.m_type = type;
    m.m_text = text;
    m.m_priority = priority;
    return m;
}

// Frame index of the first frame of cycle 0 in which the capcode is eligible
uint64_t eligibleIndex(uint64_t capcode, int cycle = 0)
{
    return (uint64_t) cycle * Sched::m_framesPerCycle + Sched::capcodeSlot(capcode).m_frame;
}

const DecodedMessage *findMessage(const DecodedFrame& frame, uint64_t capcode)
{
    for (const DecodedPhase& p : frame.m_phases)
    {
        for (const DecodedMessage& m : p.m_messages)
        {
            if (m.m_capcode == capcode) {
                return &m;
            }
        }
    }

    return nullptr;
}

int phaseOfMessage(const DecodedFrame& frame, uint64_t capcode)
{
    for (size_t i = 0; i < frame.m_phases.size(); i++)
    {
        for (const DecodedMessage& m : frame.m_phases[i].m_messages)
        {
            if (m.m_capcode == capcode) {
                return (int) i;
            }
        }
    }

    return -1;
}

std::vector<uint8_t> hexBytes(std::string_view hex)
{
    std::vector<uint8_t> bytes;

    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        bytes.push_back((uint8_t) std::stoul(std::string(hex.substr(i, 2)), nullptr, 16));
    }

    return bytes;
}

const Flex::Mode allModes[4] = {Flex::Mode1600_2, Flex::Mode3200_2, Flex::Mode3200_4, Flex::Mode6400_4};

// ---------------------------------------------------------------------------
// Golden frames
// ---------------------------------------------------------------------------

struct GoldenMessage
{
    unsigned long long m_capcode;
    int m_type;
    const char *m_text;
    int m_priority;
    int m_number;
};

struct GoldenCase
{
    const char *m_name;
    int m_mode;
    int m_cycle;
    int m_frame;
    int m_collapse;
    int m_phase;
    int m_count;
    GoldenMessage m_messages[12];
    unsigned m_traffic;
    const char *m_sync;
    const char *m_data;
};

const GoldenCase goldenCases[] = {
#include "radiopagingflexgolden.inc"
};

void testGolden()
{
    int total = 0;
    int exact = 0;

    for (const GoldenCase& g : goldenCases)
    {
        total++;

        Flex::Mode mode = (Flex::Mode) g.m_mode;
        int phases = Flex::phaseCount(mode);
        std::vector<Flex::PhaseMessage> messages;

        for (int i = 0; i < g.m_count; i++)
        {
            Flex::PhaseMessage m;
            m.m_capcode = g.m_messages[i].m_capcode;
            m.m_type = (Flex::MessageType) g.m_messages[i].m_type;
            m.m_text = g.m_messages[i].m_text;
            m.m_priority = g.m_messages[i].m_priority != 0;
            m.m_number = g.m_messages[i].m_number;
            messages.push_back(m);
        }

        std::vector<std::vector<uint32_t>> words;
        std::vector<int> addressWords(phases, 0);
        bool built = true;
        bool allPacked = true;

        for (int p = 0; p < phases; p++)
        {
            Flex::PhaseParams params;
            params.m_mode = mode;
            params.m_phase = p;
            params.m_collapse = g.m_collapse;

            Flex::PhaseResult r = Flex::buildPhase(params, (p == g.m_phase) ? messages : std::vector<Flex::PhaseMessage>());
            built = built && r.m_ok;
            addressWords[p] = r.m_addressWords;
            for (bool packed : r.m_packed) {
                allPacked = allPacked && packed;
            }
            words.push_back(r.m_words);
        }

        Flex::FrameParams fp;
        fp.m_mode = mode;
        fp.m_cycle = g.m_cycle;
        fp.m_frame = g.m_frame;
        fp.m_traffic = Flex::lowTrafficFlags(addressWords, phases);

        Flex::FrameBytes bytes = Flex::assemble(fp, words);
        bool same = built && allPacked && (fp.m_traffic == g.m_traffic)
            && (bytes.m_sync == hexBytes(g.m_sync)) && (bytes.m_data == hexBytes(g.m_data));

        if (same)
        {
            exact++;
        }
        else
        {
            std::printf("FAIL: golden frame '%s' (traffic %u/%u, sync %s, data %s)\n", g.m_name, fp.m_traffic, g.m_traffic,
                bytes.m_sync == hexBytes(g.m_sync) ? "same" : "differs", bytes.m_data == hexBytes(g.m_data) ? "same" : "differs");
            failures++;
        }
    }

    char label[96];
    std::snprintf(label, sizeof(label), "golden frames byte exact (sync and data): %d of %d", exact, total);
    check(exact == total, label);
}

// ---------------------------------------------------------------------------
// Word level checks
// ---------------------------------------------------------------------------

void testWords()
{
    std::mt19937 rng(7);
    bool roundTrip = true;
    bool valid = true;
    bool interleave = true;

    for (int n = 0; n < 200; n++)
    {
        uint32_t data = rng() & 0x1fffff;
        uint32_t word = Flex::encodeWord(data);
        uint32_t back = 0;

        roundTrip = roundTrip && (Flex::wordData(word) == data) && decodeWord(word, back) && (back == data);
        valid = valid && (RadioPagingBCH::syndrome(word) == 0) && RadioPagingBCH::evenParity(word);
        // First transmitted bit is data bit 0
        roundTrip = roundTrip && (((word >> 31) & 1) == (data & 1));

        uint32_t block[8];
        uint32_t copy[8];
        for (int i = 0; i < 8; i++) {
            block[i] = copy[i] = rng();
        }
        Flex::interleaveBlock(block);
        // Byte 0 holds the first bit of the 8 words, word 0 in bit 7
        uint8_t first = block[0] >> 24;
        uint8_t expect = 0;
        for (int i = 0; i < 8; i++) {
            expect |= ((copy[i] >> 31) & 1) << (7 - i);
        }
        Flex::deinterleaveBlock(block);
        interleave = interleave && (first == expect) && (std::memcmp(block, copy, sizeof(block)) == 0);
    }

    check(roundTrip, "word: data bits sent first bit first, decode returns the data");
    check(valid, "word: valid BCH codeword with even parity");
    check(interleave, "word: block interleave and de-interleave");

    uint32_t cs = Flex::addChecksum(0x12340);
    check(Flex::checksumValid(cs) && !Flex::checksumValid(cs ^ 0x10), "word: 4-bit checksum");

    // Capcode <-> address words at the boundaries of the short and long ranges
    bool addresses = true;
    const uint64_t caps[] = {1, 2, 4096, 1933312, 2101249, 2101250, 1075843072ULL, 1075843073ULL, 3223326720ULL,
        3223326721ULL, 4297068542ULL, 3000000000ULL};
    for (uint64_t cap : caps)
    {
        if (Flex::isShortAddress(cap))
        {
            uint32_t w;
            addresses = addresses && decodeWord(Flex::shortAddressWord((uint32_t) cap), w) && !isLongFirst(w)
                && (Flex::shortAddressCapcode(w) == cap);
        }
        else
        {
            uint32_t pair[2];
            uint32_t d1;
            uint32_t d2;
            uint64_t back = 0;
            addresses = addresses && Flex::longAddressWords(cap, pair) && decodeWord(pair[0], d1) && decodeWord(pair[1], d2)
                && isLongFirst(d1) && Flex::longAddressCapcode(d1, d2, back) && (back == cap);
        }
    }
    check(addresses, "address: short and long address words at the range boundaries");
    check(!Flex::isValidAddress(0) && !Flex::isValidAddress(1933313) && !Flex::isValidAddress(2101248)
        && !Flex::isValidAddress(4297068543ULL), "address: capcodes outside the short and long ranges are invalid");

    // Symbol mapping: dibits most significant bit first, Gray code, 3 = highest frequency
    std::vector<uint8_t> byte(1, 0xb4);   // dibits 10 11 01 00
    std::vector<uint8_t> four = Flex::bytesToSymbols(byte, 4, false);
    std::vector<uint8_t> fourInv = Flex::bytesToSymbols(byte, 4, true);
    std::vector<uint8_t> two = Flex::bytesToSymbols(byte, 2, false);
    std::vector<uint8_t> twoInv = Flex::bytesToSymbols(byte, 2, true);
    check(four == std::vector<uint8_t>({3, 2, 1, 0}) && fourInv == std::vector<uint8_t>({0, 1, 2, 3}),
        "symbols: 4 levels Gray map 10 -> 3, 11 -> 2, 01 -> 1, 00 -> 0, inverted mirrored");
    check(two == std::vector<uint8_t>({1, 0, 1, 1, 0, 1, 0, 0}) && twoInv == std::vector<uint8_t>({0, 1, 0, 0, 1, 0, 1, 1}),
        "symbols: 2 levels bit 1 = 1 (highest), inverted mirrored");

    check((Flex::phaseCount(Flex::Mode1600_2) == 1) && (Flex::phaseCount(Flex::Mode3200_2) == 2)
        && (Flex::phaseCount(Flex::Mode3200_4) == 2) && (Flex::phaseCount(Flex::Mode6400_4) == 4)
        && (Flex::baud(Flex::Mode3200_4) == 1600) && (Flex::baud(Flex::Mode6400_4) == 3200)
        && (Flex::bitRate(Flex::Mode6400_4) == 6400) && (Flex::bitRate(Flex::Mode3200_4) == 3200),
        "mode: phases, baud and bit rate");
}

// ---------------------------------------------------------------------------
// Encoder checks
// ---------------------------------------------------------------------------

const std::string longText = "The quick brown fox jumps over the lazy dog 0123456789 and again: the quick brown fox jumps over the lazy dog";

void testMessages()
{
    struct Case
    {
        const char *m_label;
        uint64_t m_capcode;
        Flex::MessageType m_type;
        std::string m_text;
    };
    const Case cases[] = {
        {"alphanumeric short address", 1234567, Flex::Alphanumeric, "Hello World"},
        {"alphanumeric one character", 100, Flex::Alphanumeric, "A"},
        {"alphanumeric two characters", 101, Flex::Alphanumeric, "AB"},
        {"alphanumeric long text", 5000, Flex::Alphanumeric, longText},
        {"alphanumeric long address set 1-2", 2101249, Flex::Alphanumeric, "long address one"},
        {"alphanumeric long address set 1-3", 1075843073ULL, Flex::Alphanumeric, "abcdefg"},
        {"alphanumeric long address set 2-3", 4297068542ULL, Flex::Alphanumeric, "zz"},
        {"numeric short address", 1000, Flex::Numeric, "0123456789"},
        {"numeric all characters", 1001, Flex::Numeric, "12 U-]5[.9"},
        {"numeric one word long address", 2101250, Flex::Numeric, "1234"},
        {"numeric long address", 3000000000ULL, Flex::Numeric, "1234567890123456"},
        {"numeric longest", 1002, Flex::Numeric, "12345678901234567890123456789012345678901"},
        {"tone only short address", 4242, Flex::Tone, ""},
        {"tone only long address", 2200000, Flex::Tone, ""},
    };

    for (Flex::Mode mode : allModes)
    {
        for (int inverted = 0; inverted < 2; inverted++)
        {
            bool ok = true;
            std::string firstBad;

            for (const Case& c : cases)
            {
                Encoder enc;
                enc.setMode(mode);
                enc.setInverted(inverted != 0);

                Encoder::EnqueueResult er = enc.enqueue(makeMessage(c.m_capcode, c.m_type, c.m_text));
                Encoder::Frame f = enc.buildFrame(eligibleIndex(c.m_capcode, 3));
                DecodedFrame d = decodeFrame(f);
                const DecodedMessage *m = d.m_ok ? findMessage(d, c.m_capcode) : nullptr;
                int nbPhases = Flex::phaseCount(mode);
                bool good = er.m_ok && d.m_ok && m && (d.m_mode == (int) mode) && (d.m_cycle == 3)
                    && (d.m_frame == Sched::capcodeSlot(c.m_capcode).m_frame)
                    && (phaseOfMessage(d, c.m_capcode) == (int) (Sched::capcodeSlot(c.m_capcode).m_phase % nbPhases))
                    && (f.m_sentIds.size() == 1) && enc.pending() == 0;

                if (good)
                {
                    int kind = (c.m_type == Flex::Tone) ? 0 : (c.m_type == Flex::Numeric) ? 1 : 2;
                    good = (m->m_kind == kind) && m->m_ok && (m->m_text == c.m_text);
                    if (c.m_type == Flex::Alphanumeric) {
                        good = good && (m->m_number == 0);
                    }
                }
                if (!good && ok)
                {
                    ok = false;
                    firstBad = c.m_label;
                }
            }

            std::string label = std::string("decode ") + Flex::modeName(mode) + (inverted ? " inverted" : " normal")
                + ": alpha, numeric, tone, short and long addresses" + (ok ? "" : " (first bad: " + firstBad + ")");
            check(ok, label.c_str());
        }
    }
}

void testFrameLayout()
{
    bool symbols = true;
    bool mirror = true;
    bool sync = true;

    for (Flex::Mode mode : allModes)
    {
        Encoder normal;
        Encoder inverted;
        normal.setMode(mode);
        inverted.setMode(mode);
        inverted.setInverted(true);
        normal.enqueue(makeMessage(300, Flex::Alphanumeric, "layout"));
        inverted.enqueue(makeMessage(300, Flex::Alphanumeric, "layout"));

        Encoder::Frame a = normal.buildFrame(eligibleIndex(300));
        Encoder::Frame b = inverted.buildFrame(eligibleIndex(300));

        int bits = Flex::phaseCount(mode) * Flex::m_wordsPerFrame * 32;
        int s2Bits = ((mode == Flex::Mode1600_2) ? 4 : (mode == Flex::Mode6400_4) ? 64 : 24) * 2 + 32;
        size_t dataSymbols = (size_t) (bits + s2Bits) / ((Flex::levels(mode) == 4) ? 2 : 1);

        symbols = symbols && (a.m_blocks.size() == 2) && (a.m_blocks[0].m_symbols.size() == 144)
            && (a.m_blocks[1].m_symbols.size() == dataSymbols);
        sync = sync && (a.m_blocks[0].m_baud == 1600) && (a.m_blocks[0].m_levels == 2)
            && (a.m_blocks[1].m_baud == Flex::baud(mode)) && (a.m_blocks[1].m_levels == Flex::levels(mode));

        for (size_t i = 0; i < 2; i++)
        {
            int top = a.m_blocks[i].m_levels - 1;
            for (size_t k = 0; k < a.m_blocks[i].m_symbols.size(); k++)
            {
                symbols = symbols && (a.m_blocks[i].m_symbols[k] <= top);
                mirror = mirror && (a.m_blocks[i].m_symbols[k] + b.m_blocks[i].m_symbols[k] == top);
            }
        }
    }

    check(symbols, "frame: symbol counts of sync part and data part, symbols within the levels");
    check(sync, "frame: sync part 1600 baud 2 levels, data part at the baud and levels of the mode");
    check(mirror, "frame: inverted polarity mirrors every symbol of the frame");
}

void testIdleAndCounting()
{
    for (Flex::Mode mode : allModes)
    {
        Encoder enc;
        enc.setMode(mode);

        bool idle = true;
        bool counting = true;
        bool biw = true;

        // More than two cycles, then across the hour wrap at 1920 frames
        std::vector<uint64_t> indices;
        for (uint64_t i = 0; i < 300; i++) {
            indices.push_back(i);
        }
        for (uint64_t i = 1915; i < 1925; i++) {
            indices.push_back(i);
        }

        for (uint64_t i : indices)
        {
            Encoder::Frame f = enc.buildFrame(i);
            DecodedFrame d = decodeFrame(f);

            idle = idle && f.m_idle && !f.m_reserved && f.m_sentIds.empty() && d.m_ok;
            counting = counting && d.m_ok && (d.m_frame == i % 128) && (d.m_cycle == (i / 128) % 15);

            for (const DecodedPhase& p : d.m_phases) {
                biw = biw && p.m_ok && p.m_messages.empty() && (p.m_vectorStart == 1) && (p.m_priorityWords == 0)
                    && (p.m_collapse == 0);
            }
            // Nothing queued: every phase has its address field in block 0
            biw = biw && (d.m_traffic == 0xf);
        }

        std::string name = std::string(Flex::modeName(mode));
        check(idle, ("idle " + name + ": idle frame for every frame index when nothing is queued").c_str());
        check(counting, ("counting " + name + ": cycle and frame of 2+ cycles and across the hour wrap").c_str());
        check(biw, ("idle " + name + ": block information word without addresses, all low traffic flags").c_str());
    }
}

void testCollapse()
{
    bool eligible = true;
    bool sentOnce = true;
    bool collapseField = true;
    bool formula = true;

    for (int collapse = 0; collapse <= 7; collapse++)
    {
        // (capcode / 16) mod 128 = 21, 21 mod 2^collapse is the frame class
        uint64_t capcode = 21 * 16 + 5;

        for (int f = 0; f < 128; f++) {
            formula = formula && (Sched::frameEligible(capcode, f, collapse) == ((f % (1 << collapse)) == (21 % (1 << collapse))));
        }

        Encoder enc;
        std::string error;
        enc.setCollapse(collapse, error);
        enc.enqueue(makeMessage(capcode, Flex::Alphanumeric, "collapse"));

        int sentAt = -1;
        int sentCount = 0;

        for (uint64_t i = 0; i < 256; i++)
        {
            Encoder::Frame f = enc.buildFrame(i);
            DecodedFrame d = decodeFrame(f);

            for (const DecodedPhase& p : d.m_phases) {
                collapseField = collapseField && p.m_ok && (p.m_collapse == (unsigned) collapse);
            }
            if (!f.m_sentIds.empty())
            {
                sentCount++;
                if (sentAt < 0) {
                    sentAt = (int) i;
                }
                eligible = eligible && Sched::frameEligible(capcode, i % 128, collapse);
            }
        }

        // The first eligible frame at or after index 0
        int expected = (int) Sched::nextFrame(0, 21, collapse);
        sentOnce = sentOnce && (sentCount == 1) && (sentAt == expected);
    }

    check(formula, "collapse: eligible frames are those congruent to the capcode frame modulo 2^m");
    check(eligible, "collapse: a message is only sent in an eligible frame");
    check(sentOnce, "collapse: a message is sent once, in the first eligible frame, for collapse 0 to 7");
    check(collapseField, "collapse: block information word carries the collapse value in every frame");

    Encoder enc;
    std::string error;
    check(!enc.setCollapse(8, error) && !error.empty() && !enc.setCollapse(-1, error) && enc.collapse() == 0,
        "collapse: value outside 0-7 rejected");
}

void testReserved()
{
    Encoder enc;
    std::string error;
    std::vector<uint8_t> table(128, 0);

    // Collapse 7: a capcode is eligible in one frame only
    uint64_t capcode = 9 * 16;   // frame 9
    enc.setCollapse(7, error);

    table[9] = 1;
    check(enc.setReservedFrames(table, error), "reserved: table accepted");
    Encoder::EnqueueResult er = enc.enqueue(makeMessage(capcode, Flex::Alphanumeric, "reserved"));
    check(!er.m_ok && !er.m_error.empty() && enc.pending() == 0, "reserved: item whose only eligible frame is reserved rejected at enqueue");

    // With collapse 6 the capcode is also eligible in frame 73
    enc.setCollapse(6, error);
    er = enc.enqueue(makeMessage(capcode, Flex::Alphanumeric, "reserved"));
    check(er.m_ok, "reserved: accepted when another eligible frame is not reserved");

    Encoder::Frame f9 = enc.buildFrame(9);
    check(f9.m_reserved && f9.m_blocks.empty() && f9.m_bytes.m_sync.empty() && f9.m_sentIds.empty() && enc.pending() == 1,
        "reserved: reserved frame has no output and sends nothing");
    Encoder::Frame f73 = enc.buildFrame(73);
    DecodedFrame d = decodeFrame(f73);
    check(!f73.m_reserved && f73.m_sentIds.size() == 1 && d.m_ok && findMessage(d, capcode) && findMessage(d, capcode)->m_text == "reserved",
        "reserved: sent in the next eligible frame that is not reserved");

    bool otherFrames = true;
    for (int f = 0; f < 128; f++)
    {
        Encoder::Frame fr = enc.buildFrame(f);
        otherFrames = otherFrames && (fr.m_reserved == (f == 9)) && (fr.m_blocks.empty() == (f == 9));
    }
    check(otherFrames, "reserved: only the reserved frame has no output");

    // Frames 0-3 can never be reserved
    bool protectedOk = true;
    for (int f = 0; f < 4; f++)
    {
        std::vector<uint8_t> bad(128, 0);
        bad[f] = 1;
        std::string e;
        protectedOk = protectedOk && !enc.setReservedFrames(bad, e) && !e.empty() && !enc.reservedFrames()[f];
    }
    check(protectedOk, "reserved: frames 0 to 3 can not be reserved, table not applied");
    std::string e2;
    check(!enc.setReservedFrames(std::vector<uint8_t>(100, 0), e2) && enc.reservedFrames()[9] == 1, "reserved: table of the wrong size rejected");
    check(enc.setReservedFrames(std::vector<uint8_t>(), e2) && enc.reservedFrames()[9] == 0, "reserved: empty table clears the reservations");

    // Reservation added later: queued items that no frame can carry are reported
    Encoder late;
    late.setCollapse(7, error);
    late.enqueue(makeMessage(capcode, Flex::Tone, ""));
    std::vector<uint8_t> t2(128, 0);
    t2[9] = 1;
    late.setReservedFrames(t2, error);
    check(late.unschedulable().size() == 1, "reserved: queued item without a usable frame reported");
}

void testRejections()
{
    Encoder enc;
    int rejected = 0;

    rejected += enc.enqueue(makeMessage(0, Flex::Alphanumeric, "x")).m_ok ? 0 : 1;
    rejected += enc.enqueue(makeMessage(1933313, Flex::Alphanumeric, "x")).m_ok ? 0 : 1;
    rejected += enc.enqueue(makeMessage(4297068543ULL, Flex::Tone, "")).m_ok ? 0 : 1;
    rejected += enc.enqueue(makeMessage(10, Flex::Alphanumeric, "")).m_ok ? 0 : 1;
    rejected += enc.enqueue(makeMessage(10, Flex::Alphanumeric, "caf\xc3\xa9")).m_ok ? 0 : 1;
    rejected += enc.enqueue(makeMessage(10, Flex::Numeric, "")).m_ok ? 0 : 1;
    rejected += enc.enqueue(makeMessage(10, Flex::Numeric, "12a")).m_ok ? 0 : 1;
    rejected += enc.enqueue(makeMessage(10, Flex::Numeric, std::string(42, '1'))).m_ok ? 0 : 1;
    check((rejected == 8) && (enc.pending() == 0), "enqueue: invalid capcode and text rejected with an error");

    Encoder::EnqueueResult ok = enc.enqueue(makeMessage(10, Flex::Alphanumeric, std::string(251, 'a')));
    Encoder::EnqueueResult ok2 = enc.enqueue(makeMessage(10, Flex::Tone, "ignored"));
    Encoder::EnqueueResult ok3 = enc.enqueue(makeMessage(2101300, Flex::Alphanumeric, std::string(248, 'a')));
    check(ok.m_ok && ok2.m_ok && ok3.m_ok && ok.m_id != ok2.m_id && enc.pending() == 3,
        "enqueue: longest alphanumeric messages (251 short, 248 long address) and tone accepted, unique ids");
    check(enc.remove(ok.m_id) && !enc.remove(ok.m_id) && enc.pending() == 2, "enqueue: queued message removed by id");

    // STD-43A 4.1: 251 characters in a frame with a short address, 248 with a long one; one more is a second fragment
    Encoder::EnqueueResult f1 = enc.enqueue(makeMessage(10, Flex::Alphanumeric, std::string(252, 'a')));
    size_t queued = enc.pending();
    Encoder::EnqueueResult f2 = enc.enqueue(makeMessage(2101300, Flex::Alphanumeric, std::string(249, 'a')));
    check(f1.m_ok && f2.m_ok && (queued == 4) && (enc.pending() == 6),
        "enqueue: alphanumeric message one character longer than a frame in 2 fragments (short and long address)");
}

void testNumbers()
{
    Encoder enc;
    const uint64_t a = 777;
    const uint64_t b = 5555;

    // Messages of two capcodes, in order, several frames
    for (int i = 0; i < 70; i++) {
        enc.enqueue(makeMessage(a, Flex::Alphanumeric, "A" + std::to_string(i)));
    }
    for (int i = 0; i < 3; i++) {
        enc.enqueue(makeMessage(b, Flex::Alphanumeric, "B" + std::to_string(i)));
    }

    std::vector<std::pair<std::string, int>> sent;
    int maxPerFrame = 0;

    for (uint64_t i = 0; i < 128 * 15 && enc.pending() > 0; i++)
    {
        Encoder::Frame f = enc.buildFrame(i);
        DecodedFrame d = decodeFrame(f);

        for (const DecodedPhase& p : d.m_phases)
        {
            int perCapcode = 0;
            for (const DecodedMessage& m : p.m_messages)
            {
                if (m.m_capcode == a) {
                    perCapcode++;
                }
                sent.push_back(std::make_pair(m.m_text, m.m_number));
            }
            maxPerFrame = std::max(maxPerFrame, perCapcode);
        }
    }

    bool sequence = true;
    int countA = 0;
    int countB = 0;

    for (const auto& s : sent)
    {
        if (s.first[0] == 'A')
        {
            // First number is 0, advances by one and wraps from 63 to 0
            sequence = sequence && (s.second == countA % 64) && (s.first == "A" + std::to_string(countA));
            countA++;
        }
        else
        {
            sequence = sequence && (s.second == countB) && (s.first == "B" + std::to_string(countB));
            countB++;
        }
    }

    check(sequence && countA == 70 && countB == 3, "numbers: per capcode message number 0, 1 ... 63, 0, 1 in transmission order, other capcode independent");
    check(maxPerFrame == 2, "numbers: the same address is used at most twice in a frame");

    RadioPagingFlexScheduler::NumberCounter counter(3);
    counter.next(1);
    counter.next(2);
    counter.next(3);
    counter.next(1);
    counter.next(4);   // drops capcode 2, the least recently used
    check(counter.peek(1) == 1 && counter.peek(2) == -1 && counter.peek(3) == 0 && counter.peek(4) == 0 && counter.size() == 3,
        "numbers: least recently used capcode dropped when the table is full");
    counter.set(3, 63);
    check(counter.next(3) == 0, "numbers: counter wraps after 63");
    counter.set(3, 9);
    check((counter.upcoming(3, 9) == 0) && (counter.next(3, 9) == 0) && (counter.next(3, 9) == 1) && (counter.upcoming(5, 9) == 0),
        "numbers: counter wraps after the largest number of the network, upcoming does not take a number");
}

void testPlacement()
{
    // Priority addresses come first in the address field
    {
        Encoder enc;
        enc.enqueue(makeMessage(300, Flex::Alphanumeric, "normal one"));
        enc.enqueue(makeMessage(301, Flex::Tone, ""));
        enc.enqueue(makeMessage(302, Flex::Alphanumeric, "priority", true));
        enc.enqueue(makeMessage(303, Flex::Numeric, "123"));

        Encoder::Frame f = enc.buildFrame(eligibleIndex(300));
        DecodedFrame d = decodeFrame(f);
        bool ok = d.m_ok && (f.m_sentIds.size() == 4) && (d.m_phases[0].m_messages.size() == 4);

        if (ok)
        {
            const std::vector<DecodedMessage>& m = d.m_phases[0].m_messages;
            ok = (m[0].m_capcode == 302) && m[0].m_priority && (m[0].m_text == "priority") && (d.m_phases[0].m_priorityWords == 1)
                && (m[1].m_capcode == 300) && !m[1].m_priority && (m[1].m_text == "normal one")
                && (m[2].m_capcode == 303) && (m[2].m_text == "123")
                && (m[3].m_capcode == 301) && (m[3].m_kind == 0);
        }
        check(ok, "placement: priority address first, normal in queue order, tone only address last");
    }

    // A full phase passes messages to another phase (the most significant bit phase first)
    {
        Encoder enc;
        enc.setMode(Flex::Mode6400_4);
        enc.enqueue(makeMessage(300, Flex::Alphanumeric, std::string(200, 'x')));
        enc.enqueue(makeMessage(301, Flex::Alphanumeric, std::string(200, 'y')));
        Encoder::Frame f = enc.buildFrame(eligibleIndex(300));
        DecodedFrame d = decodeFrame(f);

        check(d.m_ok && f.m_sentIds.size() == 2 && phaseOfMessage(d, 300) == (int) (Sched::capcodeSlot(300).m_phase)
            && phaseOfMessage(d, 301) == 0, "placement: full phase passes a message to a most significant bit phase");

        Encoder single;
        single.enqueue(makeMessage(300, Flex::Alphanumeric, std::string(200, 'x')));
        single.enqueue(makeMessage(301, Flex::Alphanumeric, std::string(200, 'y')));
        Encoder::Frame first = single.buildFrame(eligibleIndex(300));
        Encoder::Frame second = single.buildFrame(eligibleIndex(300, 1));
        DecodedFrame d2 = decodeFrame(second);
        check(first.m_sentIds.size() == 1 && second.m_sentIds.size() == 1 && d2.m_ok && findMessage(d2, 301) && single.pending() == 0,
            "placement: single phase mode keeps the message for the next frame");
    }

    // The same address at most twice in a frame
    {
        Encoder enc;
        for (int i = 0; i < 3; i++) {
            enc.enqueue(makeMessage(400, Flex::Alphanumeric, "m" + std::to_string(i)));
        }
        Encoder::Frame f = enc.buildFrame(eligibleIndex(400));
        check(f.m_sentIds.size() == 2 && enc.pending() == 1, "placement: third message of an address waits for the next frame");
    }

    // Low traffic flags: address field longer than block 0 clears the flag
    {
        Encoder enc;
        for (int i = 0; i < 9; i++) {
            enc.enqueue(makeMessage(500 + i, Flex::Tone, ""));
        }
        Encoder::Frame f = enc.buildFrame(eligibleIndex(500));
        Encoder::Frame idle = enc.buildFrame(eligibleIndex(500, 1));
        DecodedFrame d = decodeFrame(f);
        check(d.m_ok && d.m_traffic == 0 && idle.m_traffic == 0xf, "placement: low traffic flags clear when the address field passes block 0");
    }

    // Long address with a vector: vector pair and message word placement
    {
        Encoder enc;
        enc.enqueue(makeMessage(3223326721ULL, Flex::Alphanumeric, "set two three"));
        enc.enqueue(makeMessage(3223326722ULL, Flex::Numeric, "99"));
        Encoder::Frame f = enc.buildFrame(eligibleIndex(3223326721ULL));
        DecodedFrame d = decodeFrame(f);
        check(d.m_ok && f.m_sentIds.size() == 2 && findMessage(d, 3223326721ULL) && findMessage(d, 3223326721ULL)->m_text == "set two three"
            && findMessage(d, 3223326722ULL) && findMessage(d, 3223326722ULL)->m_text == "99",
            "placement: two long address messages decoded from the vector pairs and message field");
    }
}

} // namespace

int main()
{
    testWords();
    testGolden();
    testMessages();
    testFrameLayout();
    testIdleAndCounting();
    testCollapse();
    testReserved();
    testRejections();
    testNumbers();
    testPlacement();

    std::printf("%s\n", failures == 0 ? "ALL PASSED" : "FAILURES");
    return failures == 0 ? 0 : 1;
}
