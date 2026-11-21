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

#include "radiopagingflex.h"

#include <algorithm>
#include <climits>
#include <cctype>
#include <cstdio>
#include <cstring>

#include "radiopagingbch.h"

const char RadioPagingFlex::m_numericChars[17] = "0123456789.U -][";
const int RadioPagingFlex::m_wordsPerBlock;
const int RadioPagingFlex::m_blocksPerFrame;
const int RadioPagingFlex::m_wordsPerFrame;
const int RadioPagingFlex::m_maxPhases;
const int RadioPagingFlex::m_syncBaud;
const int RadioPagingFlex::m_maxCollapse;
const int RadioPagingFlex::m_maxAlphaChars;
const int RadioPagingFlex::m_maxNumericChars;
const int RadioPagingFlex::m_maxNumberedNumericChars;
const int RadioPagingFlex::m_maxPriorityWords;
const uint64_t RadioPagingFlex::m_shortMin;
const uint64_t RadioPagingFlex::m_shortMax;
const uint64_t RadioPagingFlex::m_longMin;
const uint64_t RadioPagingFlex::m_longMax;
const unsigned RadioPagingFlex::m_vectorTone;
const unsigned RadioPagingFlex::m_vectorNumeric;
const unsigned RadioPagingFlex::m_vectorSpecialNumeric;
const unsigned RadioPagingFlex::m_vectorNumberedNumeric;
const unsigned RadioPagingFlex::m_vectorAlpha;
const unsigned RadioPagingFlex::m_vectorBinary;
const int RadioPagingFlex::m_maxFragments;
const unsigned RadioPagingFlex::m_vectorSecure;
const int RadioPagingFlex::m_frameMs;
const int RadioPagingFlex::m_bitsPerFrame;

namespace {

const uint32_t shortAddressOffset = 32768;
const uint32_t idleOnes = 0xffffffffu;
const uint32_t idleZeros = 0x00000000u;
const int maxNumericWords = 8;
const int numericOverheadBits = 2;
const int numberedOverheadBits = 10;

int numericOverhead(RadioPagingFlex::NumericFormat format)
{
    return ((format == RadioPagingFlex::NumericNumbered) || (format == RadioPagingFlex::NumericNumberedSpecial))
        ? numberedOverheadBits : numericOverheadBits;
}
const uint32_t alphaEtx = 0x03;
const int biwWords = 1;

// Second sync: 16 bit pattern and its inverse, sent most significant bit first
const uint32_t syncC = 0xed84;
const uint32_t syncCInv = 0x127b;

// Bit sync (32 bits), mode code (A, 32 bits), B (16 bits) and the inverted mode code
const uint8_t syncBs1[4] = {0xaa, 0xaa, 0xaa, 0xaa};
const uint8_t syncB[2] = {0x55, 0x55};
const uint8_t syncA[4][4] = {
    {0x78, 0xf3, 0x59, 0x39},   // 1600/2
    {0x84, 0xe7, 0x59, 0x39},   // 3200/2
    {0x4f, 0x97, 0x59, 0x39},   // 3200/4
    {0x21, 0x5f, 0x59, 0x39}    // 6400/4
};
const uint8_t syncAInv[4][4] = {
    {0x87, 0x0c, 0xa6, 0xc6},
    {0x7b, 0x18, 0xa6, 0xc6},
    {0xb0, 0x68, 0xa6, 0xc6},
    {0xde, 0xa0, 0xa6, 0xc6}
};

// Emergency re-synchronization cycle, transmission order (STD-43A Table 3.2.1-1): BS, Ar,
// inverted BS, inverted Ar
const uint8_t ersCycle[12] = {
    0xaa, 0xaa,
    0xcb, 0x20, 0x59, 0x39,
    0x55, 0x55,
    0x34, 0xdf, 0xa6, 0xc6
};

uint32_t reverse21(uint32_t value)
{
    return RadioPagingBCH::reverse(value & 0x1fffff) >> 11;
}

// Bits of a message word group sum used by the message checksums
uint32_t groupSum(uint32_t word)
{
    return (word & 0xff) + ((word >> 8) & 0xff) + ((word >> 16) & 0x1f);
}

int numericValue(char c)
{
    if ((c >= '0') && (c <= '9')) {
        return c - '0';
    }

    switch (c)
    {
    case '.': return 0x0a;
    case 'U':
    case 'u': return 0x0b;
    case ' ': return 0x0c;
    case '-':
    case '_': return 0x0d;
    case ']': return 0x0e;
    case '[': return 0x0f;
    default: return 0;
    }
}

bool numericCharValid(char c)
{
    return ((c >= '0') && (c <= '9')) || (c == '-') || (c == '_') || (c == '[') || (c == ']')
        || (c == ' ') || (c == 'U') || (c == 'u') || (c == '.');
}

uint32_t alphaSignature(const std::string& text)
{
    uint32_t sum = 0;

    for (char c : text)
    {
        unsigned ch = ((unsigned char) c) & 0x7f;
        if (ch != alphaEtx) {
            sum += ch;
        }
    }

    return (~sum) & 0x7f;
}

// Message body words (data bits, not yet encoded) of a fragment of an alphanumeric message (STD-43A Fig.
// 3.10.1.3-1, -2). retrieval: R of the first fragment. mailDrop: M of the first fragment. signature: of the whole
// message, -1 of the text
std::vector<uint32_t> alphaBody(const std::string& text, int number, bool retrieval, bool mailDrop, int fragment, int fragments, int signature)
{
    const bool first = (fragment == 0);
    const bool last = (fragment == fragments - 1);
    std::vector<uint32_t> msg(1, 0);

    // Header word: K in bits 0-9 (set last), C in bit 10 (more fragments follow), F in bits 11-12, N in bits
    // 13-18; then R and M in the first fragment, the fragment control bits U0 V0 = 00 (enhanced
    // fragmentation not supported) in the others
    msg[0] = ((last ? 0u : 1u) << 10) | (RadioPagingFlex::fragmentNumber(fragment) << 11) | (((uint32_t) number & 0x3f) << 13);

    if (first && retrieval) {
        msg[0] |= 1u << 19;
    }
    if (first && mailDrop) {
        msg[0] |= 1u << 20;
    }

    // Characters, 7 bits each, 3 a word from word 2; the first character slot of the first fragment holds the
    // signature. Unused character slots of the last word hold ETX
    int shift = 0;

    if (first)
    {
        msg.push_back((signature >= 0) ? ((uint32_t) signature & 0x7f) : alphaSignature(text));
        shift = 7;
    }

    for (char c : text)
    {
        if (shift == 0) {
            msg.push_back(0);
        }
        msg.back() |= (((uint32_t) (unsigned char) c) & 0x7f) << shift;
        shift = (shift + 7) % 21;
    }
    while (shift != 0)
    {
        msg.back() |= alphaEtx << shift;
        shift = (shift + 7) % 21;
    }

    // K: 1's complement of the sum of 3 groups per word of all words, K counted as 0, 10 bits kept
    uint32_t sum = 0;
    for (uint32_t w : msg) {
        sum += groupSum(w);
    }
    msg[0] |= (~sum) & 0x3ff;

    return msg;
}

// Message body words of a numeric message and the 6-bit checksum
// Message words of a numeric message: the digits 4 bits each from bit 2 of the first word (STD-43A 3.10.1.1); the
// numbered format has N5-N0 in bits 2-7, R in bit 8 and S in bit 9 and the digits from bit 10 (3.10.1.1.2). K
// covers every bit of the message words: 4 bits in the vector (kBits), 2 in bits 0-1 of the first word
std::vector<uint32_t> numericBody(const std::string& text, uint32_t& kBits, RadioPagingFlex::NumericFormat format = RadioPagingFlex::NumericStandard,
    int number = 0, bool retrieval = false)
{
    std::vector<uint32_t> msg(maxNumericWords, 0);
    const int overhead = numericOverhead(format);
    const bool numbered = (overhead == numberedOverheadBits);
    int bit = overhead;

    if (numbered)
    {
        msg[0] |= ((uint32_t) number & 0x3f) << 2;
        msg[0] |= (retrieval ? 1u : 0u) << 8;
        msg[0] |= (format == RadioPagingFlex::NumericNumberedSpecial ? 1u : 0u) << 9;
    }

    for (char c : text)
    {
        uint32_t nibble = numericValue(c);
        for (int b = 0; b < 4; b++)
        {
            int wordIndex = bit / 21;
            if (wordIndex >= maxNumericWords) {
                break;
            }
            if (nibble & (1u << b)) {
                msg[wordIndex] |= 1u << (bit % 21);
            }
            bit++;
        }
    }

    // Spaces in the unused digit positions of the last word
    int last = (bit > 0) ? (bit - 1) / 21 : 0;
    int endBit = (last + 1) * 21;

    while (bit + 4 <= endBit)
    {
        for (int b = 0; b < 4; b++)
        {
            if (0x0c & (1u << b)) {
                msg[bit / 21] |= 1u << (bit % 21);
            }
            bit++;
        }
    }

    // Checksum: sum of 3 groups per word, 8 bits kept, top 2 bits added to the low 6, inverted
    uint32_t k = 0;
    for (int i = 0; i <= last; i++) {
        k += groupSum(msg[i]);
    }
    k &= 0xff;
    k = (k & 0x3f) + (k >> 6);
    k = ~k;
    msg[0] |= (k >> 4) & 0x03;
    kBits = k & 0xf;

    msg.resize(last + 1);
    return msg;
}

int hexValue(char c)
{
    if ((c >= '0') && (c <= '9')) {
        return c - '0';
    }
    if ((c >= 'a') && (c <= 'f')) {
        return c - 'a' + 10;
    }
    if ((c >= 'A') && (c <= 'F')) {
        return c - 'A' + 10;
    }
    return -1;
}

// Data bits of hex digits: digit i holds bits 4i to 4i+3, its least significant bit first. Empty if a
// character is not a hex digit
std::vector<uint8_t> hexBits(const std::string& hex)
{
    std::vector<uint8_t> bits;

    bits.reserve(hex.size() * 4);

    for (char c : hex)
    {
        int v = hexValue(c);

        if (v < 0) {
            return std::vector<uint8_t>();
        }
        for (int b = 0; b < 4; b++) {
            bits.push_back((uint8_t) ((v >> b) & 1));
        }
    }

    return bits;
}

// True if the last blocking bits (the last character) are all 0s or all 1s
bool lastCharacterUniform(const std::vector<uint8_t>& bits, int blocking)
{
    if ((blocking <= 0) || ((int) bits.size() < blocking)) {
        return false;
    }
    for (size_t i = bits.size() - blocking; i < bits.size(); i++)
    {
        if (bits[i] != bits.back()) {
            return false;
        }
    }
    return true;
}

// Data words of binary data in the last (here the only) fragment, terminated (STD-43A 3.10.1.2 p.61-62 rule
// (3), 3.10.1.4 p.70): data ending in the middle of a word, the rest of the word is the inverse of the last
// data bit; data ending exactly at a word end, a word of the inverse of the last data bit is added when the
// last character (blocking bits) is all 0s or all 1s. Not terminated (blocking 0): the unused bits are 0
std::vector<uint32_t> binaryDataWords(const std::vector<uint8_t>& bits, int blocking)
{
    const int wordBits = 21;
    std::vector<uint32_t> words((bits.size() + wordBits - 1) / wordBits, 0);

    for (size_t i = 0; i < bits.size(); i++)
    {
        if (bits[i]) {
            words[i / wordBits] |= 1u << (i % wordBits);
        }
    }

    if ((blocking > 0) && !bits.empty())
    {
        uint32_t inverse = bits.back() ? 0u : 1u;
        size_t rest = bits.size() % wordBits;

        if (rest != 0)
        {
            for (size_t b = rest; b < (size_t) wordBits; b++) {
                words.back() |= inverse << b;
            }
        }
        else if (lastCharacterUniform(bits, blocking))
        {
            words.push_back(inverse ? 0x1fffffu : 0u);
        }
    }

    return words;
}

// 1's complement of the sum of 8-bit groups of the data bits, the first bit of a group the least significant,
// a last group of fewer bits completed with 0 (STD-43A 3.10.1.2 p.61); termination bits are not included
uint32_t binarySignature(const std::vector<uint8_t>& bits)
{
    uint32_t sum = 0;

    for (size_t i = 0; i < bits.size(); i++) {
        sum += (uint32_t) bits[i] << (i % 8);
    }

    return (~sum) & 0xff;
}

int estimateBody(const RadioPagingFlex::PhaseMessage& message)
{
    size_t len = message.m_text.size();

    switch (message.m_type)
    {
    case RadioPagingFlex::Tone:
        return 0;
    case RadioPagingFlex::Alphanumeric:
    {
        if (len == 0) {
            return -1;
        }
        // Header word, then the characters, 3 a word; the first fragment has the signature slot first
        int slots = (int) len + ((message.m_fragment == 0) ? 1 : 0);
        int words = 1 + (slots + 2) / 3;
        return (words > RadioPagingFlex::maxMessageWords(message.m_capcode)) ? -1 : words;
    }
    case RadioPagingFlex::Numeric:
    {
        const size_t most = (numericOverhead(message.m_numericFormat) == numberedOverheadBits)
            ? (size_t) RadioPagingFlex::m_maxNumberedNumericChars : (size_t) RadioPagingFlex::m_maxNumericChars;

        if ((len == 0) || (len > most)) {
            return -1;
        }
        int words = ((int) len * 4 + numericOverhead(message.m_numericFormat) + 20) / 21;
        return std::max(1, std::min(words, maxNumericWords));
    }
    case RadioPagingFlex::ShortMessage:
    case RadioPagingFlex::ShortInstruction:
        return 0;
    case RadioPagingFlex::Binary:
    {
        // Header word 2 in the first fragment only, the termination in the last one only
        int words = RadioPagingFlex::binaryWords(message.m_text, message.m_binary.m_blocking, (message.m_fragment == 0) ? 2 : 1,
            message.m_fragment == message.m_fragments - 1);
        return (words > RadioPagingFlex::maxMessageWords(message.m_capcode)) ? -1 : words;
    }
    case RadioPagingFlex::Secure:
    {
        if (len == 0) {
            return -1;
        }

        int words = -1;

        switch (message.m_binary.m_secureType)
        {
        case RadioPagingFlex::SecureAlphanumeric:
            words = 1 + (int) ((len + 2) / 3);
            break;
        case RadioPagingFlex::SecureBinary:
            words = RadioPagingFlex::binaryWords(message.m_text, 1, 1, message.m_fragment == message.m_fragments - 1);
            break;
        case RadioPagingFlex::SecureSeparate:
            words = hexBits(message.m_text).empty() ? -1 : 1 + (int) ((len * 4 + 20) / 21);
            break;
        }

        return (words > RadioPagingFlex::maxMessageWords(message.m_capcode)) ? -1 : words;
    }
    }

    return -1;
}

} // namespace

int RadioPagingFlex::phaseCount(Mode mode)
{
    switch (mode)
    {
    case Mode1600_2: return 1;
    case Mode3200_2:
    case Mode3200_4: return 2;
    case Mode6400_4: return 4;
    }

    return 1;
}

int RadioPagingFlex::baud(Mode mode)
{
    switch (mode)
    {
    case Mode1600_2: return 1600;
    case Mode3200_2: return 3200;
    case Mode3200_4: return 1600;
    case Mode6400_4: return 3200;
    }

    return 1600;
}

int RadioPagingFlex::levels(Mode mode)
{
    return ((mode == Mode3200_4) || (mode == Mode6400_4)) ? 4 : 2;
}

int RadioPagingFlex::bitRate(Mode mode)
{
    return baud(mode) * (levels(mode) / 2);
}

const char *RadioPagingFlex::modeName(Mode mode)
{
    switch (mode)
    {
    case Mode1600_2: return "1600/2";
    case Mode3200_2: return "3200/2";
    case Mode3200_4: return "3200/4";
    case Mode6400_4: return "6400/4";
    }

    return "?";
}

uint32_t RadioPagingFlex::encodeWord(uint32_t data)
{
    return RadioPagingBCH::encode(reverse21(data));
}

uint32_t RadioPagingFlex::wordData(uint32_t word)
{
    return reverse21(RadioPagingBCH::data(word));
}

uint32_t RadioPagingFlex::addChecksum(uint32_t data)
{
    uint32_t sum = ((data >> 4) & 0xf) + ((data >> 8) & 0xf) + ((data >> 12) & 0xf) + ((data >> 16) & 0xf) + ((data >> 20) & 0x1);
    return data | ((~sum) & 0xf);
}

bool RadioPagingFlex::checksumValid(uint32_t data)
{
    uint32_t sum = (data & 0xf) + ((data >> 4) & 0xf) + ((data >> 8) & 0xf) + ((data >> 12) & 0xf) + ((data >> 16) & 0xf) + ((data >> 20) & 0x1);
    return (sum & 0xf) == 0xf;
}

uint32_t RadioPagingFlex::frameInfoWord(unsigned cycle, unsigned frame, unsigned roaming, unsigned repeat, unsigned traffic)
{
    uint32_t data = (cycle & 0xf) << 4;
    data |= (frame & 0x7f) << 8;
    data |= (roaming & 1) << 15;
    data |= (repeat & 1) << 16;
    data |= (traffic & 0xf) << 17;
    return encodeWord(addChecksum(data));
}

// Word types 000 and 111 in bits 6-4; the checksum is in bits 3-0 as in every information word
uint32_t RadioPagingFlex::ssid1Word(unsigned localId, unsigned coverageZone)
{
    uint32_t data = (coverageZone & 0x1f) << 7;
    data |= (localId & 0x1ff) << 12;
    return encodeWord(addChecksum(data));
}

uint32_t RadioPagingFlex::ssid2Word(unsigned countryCode, unsigned trafficFlags)
{
    uint32_t data = 7u << 4;
    data |= (trafficFlags & 0xf) << 7;
    data |= (countryCode & 0x3ff) << 11;
    return encodeWord(addChecksum(data));
}

int RadioPagingFlex::ssidPhase(Mode mode, unsigned frame)
{
    int slot = (int) (frame & 3);

    switch (phaseCount(mode))
    {
    case 4: return slot;
    case 2: return (slot < 2) ? 0 : 1;
    default: return 0;
    }
}

uint32_t RadioPagingFlex::blockInfoWord(unsigned priorityWords, unsigned addressStart, unsigned vectorStart, unsigned carryOn, unsigned collapse)
{
    uint32_t data = (priorityWords & 0xf) << 4;
    data |= (addressStart & 0x3) << 8;
    data |= (vectorStart & 0x3f) << 10;
    data |= (carryOn & 0x3) << 16;
    data |= (collapse & 0x7) << 18;
    return encodeWord(addChecksum(data));
}

uint32_t RadioPagingFlex::dateWord(int year, unsigned month, unsigned day)
{
    // The year field counts from 1994 in a period of 32 years (00000-11111: 1994-2025); later years take the
    // field modulo 32, the receiver places the year by its own reference
    unsigned yearField = (unsigned) (((year - 1994) % 32 + 32) % 32);
    uint32_t data = (1u << 4) | (yearField << 7) | ((day & 0x1f) << 12) | ((month & 0xf) << 17);
    return encodeWord(addChecksum(data));
}

int RadioPagingFlex::equivalentYear(int year)
{
    auto leap = [](int y) { return ((y % 4) == 0) && (((y % 100) != 0) || ((y % 400) == 0)); };
    // Weekday of January 1 (0 Sunday), Gregorian calendar
    auto january1 = [](int y) {
        int p = y - 1;
        return (((1 + 5 * (p % 4) + 4 * (p % 100) + 6 * (p % 400)) % 7) + 7) % 7;
    };

    if ((year >= 1994) && (year <= 2025)) {
        return year;
    }

    for (int y = 2025; y >= 1994; y--)
    {
        if ((leap(y) == leap(year)) && (january1(y) == january1(year))) {
            return y;
        }
    }

    return 1994 + (((year - 1994) % 32) + 32) % 32;
}

uint32_t RadioPagingFlex::timeWord(unsigned hour, unsigned minute, unsigned eighths)
{
    uint32_t data = (2u << 4) | ((hour & 0x1f) << 7) | ((minute & 0x3f) << 12) | ((eighths & 7) << 18);
    return encodeWord(addChecksum(data));
}

uint32_t RadioPagingFlex::timeZoneWord(unsigned zone, bool daylightSaving, unsigned extendedSeconds)
{
    // I0-I4 zone, I5 L0, I6 reserved (0), I7-I9 S3-S5, in bits 11-20 after A3-A0 = 0101 in bits 7-10
    uint32_t info = (zone & 0x1f) | ((daylightSaving ? 0u : 1u) << 5) | ((extendedSeconds & 7) << 7);
    uint32_t data = (5u << 4) | (5u << 7) | (info << 11);
    return encodeWord(addChecksum(data));
}

namespace {

// Table 3.7.2-3: offset from Greenwich in minutes of each zone code; code 10000 is not defined (INT32_MIN)
const int timeZoneTable[32] = {
    0, 60, 120, 180, 240, 300, 360, 420, 480, 540, 600, 660, 720, 210, 270, 330,
    INT32_MIN, 345, 390, 570, -210, -660, -600, -540, -480, -420, -360, -300, -240, -180, -120, -60
};

} // namespace

bool RadioPagingFlex::timeZoneMinutes(unsigned zone, int& minutes)
{
    minutes = 0;

    if ((zone >= 32) || (timeZoneTable[zone] == INT32_MIN)) {
        return false;
    }

    minutes = timeZoneTable[zone];
    return true;
}

int RadioPagingFlex::timeZoneCode(int minutes)
{
    for (int code = 0; code < 32; code++)
    {
        if (timeZoneTable[code] == minutes) {
            return code;
        }
    }

    return -1;
}

void RadioPagingFlex::civilTime(int64_t seconds, int& year, unsigned& month, unsigned& day, unsigned& hour, unsigned& minute,
    unsigned& second)
{
    // Days from the epoch to a proleptic Gregorian date (era of 400 years, March based years)
    int64_t days = (seconds >= 0) ? seconds / 86400 : -((-seconds + 86399) / 86400);
    int64_t rest = seconds - days * 86400;
    days += 719468;
    int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    int64_t doe = days - era * 146097;
    int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int64_t mp = (5 * doy + 2) / 153;

    day = (unsigned) (doy - (153 * mp + 2) / 5 + 1);
    month = (unsigned) ((mp < 10) ? mp + 3 : mp - 9);
    year = (int) (yoe + era * 400 + ((month <= 2) ? 1 : 0));
    hour = (unsigned) (rest / 3600);
    minute = (unsigned) ((rest % 3600) / 60);
    second = (unsigned) (rest % 60);
}

std::string RadioPagingFlex::addressText(uint64_t capcode)
{
    char digits[24];
    std::snprintf(digits, sizeof(digits), "%010llu", (unsigned long long) capcode);
    return digits;
}

// STD-43A Appendix A 4: A-D, E-H and I-L are single, any and all phase pagers of the standard rule that subtract
// 0-3; U-X single phase pagers of phase 0-3, Y any phase and Z all phase pagers without the rule
int RadioPagingFlex::CapcodeText::phaseType() const
{
    if ((m_letter >= 'A') && (m_letter <= 'L')) {
        return (m_letter - 'A') / 4;
    }

    return (m_letter >= 'U') && (m_letter <= 'X') ? 0 : (m_letter == 'Y') ? 1 : (m_letter == 'Z') ? 2 : -1;
}

int RadioPagingFlex::CapcodeText::subtract() const
{
    return ((m_letter >= 'A') && (m_letter <= 'L')) ? (m_letter - 'A') % 4 : 0;
}

int RadioPagingFlex::CapcodeText::phase() const
{
    return ((m_letter >= 'U') && (m_letter <= 'X')) ? m_letter - 'U' : -1;
}

// Appendix A 1: with A-L and no b the pager collapse is 4
int RadioPagingFlex::CapcodeText::pagerCollapse() const
{
    if (m_collapse >= 0) {
        return m_collapse;
    }

    return ((m_letter >= 'A') && (m_letter <= 'L')) ? 4 : -1;
}

// Appendix A 1 forms: digits; [b]A-L digits; [fff b]U-Z digits; each optionally after the 2nd alpha character
// P, Q, R or S (Extended CAPCODE)
bool RadioPagingFlex::parseCapcode(const std::string& text, CapcodeText& capcode, std::string& error)
{
    std::string s;

    for (char c : text)
    {
        if (c != ' ') {
            s += (char) std::toupper((unsigned char) c);
        }
    }

    capcode = CapcodeText();
    size_t i = 0;

    if (!s.empty() && ((s[0] == 'P') || (s[0] == 'Q') || (s[0] == 'R') || (s[0] == 'S')))
    {
        capcode.m_roaming = s[0];
        i = 1;
    }

    size_t prefixStart = i;
    while ((i < s.size()) && std::isdigit((unsigned char) s[i])) {
        i++;
    }

    std::string prefix = s.substr(prefixStart, i - prefixStart);
    std::string digits;

    if (i == s.size())
    {
        // Digits only: the address (a 2nd alpha character needs the letter)
        if (capcode.m_roaming)
        {
            error = "The 2nd alpha character P, Q, R or S comes before the letter of the capcode";
            return false;
        }

        digits = prefix;
    }
    else
    {
        char letter = s[i++];

        if (!(((letter >= 'A') && (letter <= 'L')) || ((letter >= 'U') && (letter <= 'Z'))))
        {
            error = std::string("Capcode letter ") + letter + " is not A to L or U to Z";
            return false;
        }

        capcode.m_letter = letter;
        digits = s.substr(i);

        for (char c : digits)
        {
            if (!std::isdigit((unsigned char) c))
            {
                error = "Only digits follow the letter of the capcode";
                return false;
            }
        }

        if (prefix.size() == 1)
        {
            capcode.m_collapse = prefix[0] - '0';
        }
        else if (prefix.size() == 4)
        {
            if (letter < 'U')
            {
                error = "The frame fff comes with the letters U to Z only (A to L: the frame is in the address)";
                return false;
            }

            capcode.m_frame = std::stoi(prefix.substr(0, 3));
            capcode.m_collapse = prefix[3] - '0';
        }
        else if (!prefix.empty())
        {
            error = "Before the letter: the collapse b (1 digit) or the frame and collapse fffb (4 digits)";
            return false;
        }

        if (capcode.m_frame > 127)
        {
            error = "Frame fff must be 000 to 127";
            return false;
        }
        if (capcode.m_collapse > 7)
        {
            error = "Collapse b must be 0 to 7";
            return false;
        }
    }

    if (digits.empty() || (digits.size() > 10))
    {
        error = "The address has 1 to 10 digits";
        return false;
    }

    capcode.m_digits = (int) digits.size();
    capcode.m_capcode = std::stoull(digits);
    return true;
}

// Table A-1; the operator messaging addresses of Table 3.8.2.4-1, the temporary addresses of 3.8.2.3
std::string RadioPagingFlex::capcodeType(uint64_t capcode)
{
    static const char *operatorMessaging[16] = {
        "system message for all pagers", "system message for all pagers in the home area", "system message for all roaming pagers",
        "system message for all SSID pagers", "time related message for all pagers", "reserved", "reserved", "reserved", "reserved",
        "reserved", "reserved", "reserved", "reserved", "reserved", "SSID change instruction", "system event notification"
    };
    struct Range { uint64_t m_last; const char *m_name; };
    static const Range ranges[] = {
        {0ULL, "Unused"},
        {1933312ULL, "Short address"},
        {1998848ULL, "Illegal"},
        {2009087ULL, "Reserved for future use"},
        {2025471ULL, "Information service address"},
        {2029567ULL, "Network address"},
        {2029583ULL, "Temporary address"},
        {2029599ULL, "Operator messaging address"},
        {2031614ULL, "Reserved for future use"},
        {2101248ULL, "Unused"},
        {102101250ULL, "Long address 1-2 (uncoordinated)"},
        {402101250ULL, "Long address 1-2 (by country)"},
        {1075843072ULL, "Long address 1-2 (global)"},
        {2149584896ULL, "Long address 1-3 (global)"},
        {3223326720ULL, "Long address 1-4 (global)"},
        {3923326750ULL, "Long address 2-3 (by country)"},
        {4280000000ULL, "Long address 2-3 (reserved)"},
        {4285000000ULL, "Information service, long address 2-3 (global)"},
        {4290000000ULL, "Information service, long address 2-3 (by country)"},
        {4291000000ULL, "Information service, long address 2-3 (per country for world wide use)"},
        {4297068542ULL, "Information service, long address 2-3 (reserved)"}
    };

    for (const Range& range : ranges)
    {
        if (capcode > range.m_last) {
            continue;
        }

        std::string name = range.m_name;

        if (isTemporaryAddress(capcode)) {
            name += " " + std::to_string(capcode - m_temporaryBase);
        } else if ((capcode >= m_temporaryBase + m_temporaryCount) && (capcode <= m_systemEventAddress)) {
            name += std::string(": ") + operatorMessaging[capcode - m_temporaryBase - m_temporaryCount];
        } else if ((capcode > 4290000000ULL) && (capcode <= 4291000000ULL)) {
            name += ", country code " + std::to_string((capcode - 4290000001ULL) / 1000);   // 1000 addresses a country (Note 3)
        }

        return name;
    }

    return "Reserved for future use (not defined)";
}

std::string RadioPagingFlex::capcodeDescription(const std::string& text)
{
    static const char *phaseTypes[3] = {"single phase", "any phase", "all phase"};
    // Appendix A 1: Frame Offset capable and TMF following of the 2nd alpha character
    static const char *roaming[4] = {
        "no roaming functions (no Frame Offset, no TMF)", "follows the TMF, no Frame Offset",
        "follows the Frame Offset of BIW 101, no TMF", "follows the Frame Offset of BIW 101 and the TMF"
    };
    CapcodeText c;
    std::string error;

    if (!parseCapcode(text, c, error)) {
        return error;
    }

    std::string description = capcodeType(c.m_capcode) + " " + std::to_string(c.m_capcode);
    const bool sendable = isValidAddress(c.m_capcode) || isTemporaryAddress(c.m_capcode) || (c.m_capcode == m_systemEventAddress);

    if (!sendable) {
        description += ", not sent by the encoder";
    }
    if (c.m_letter)
    {
        description += std::string(". ") + c.m_letter + ": " + phaseTypes[c.phaseType()] + " pager";

        if (c.phase() >= 0) {
            description += std::string(", phase ") + (char) ('A' + c.phase());
        }
        if (c.subtract() > 0) {
            description += ", address " + std::to_string(c.subtract() + 1) + " of the pager (subtract " + std::to_string(c.subtract()) + ")";
        }
        if (c.m_letter >= 'U') {
            description += ", no standard rule";
        }
    }
    if (c.m_roaming) {
        description += std::string(". ") + c.m_roaming + ": " + roaming[c.m_roaming - 'P'];
    }
    if (c.m_frame >= 0)
    {
        char frame[8];
        std::snprintf(frame, sizeof(frame), "%03d", c.m_frame);
        description += std::string(". Frame ") + frame + " of the pager";
    }
    if (c.pagerCollapse() >= 0)
    {
        int m = c.pagerCollapse();
        char seconds[16];
        std::snprintf(seconds, sizeof(seconds), "%g", (1 << m) * 1.875);
        description += ". Pager collapse " + std::to_string(m) + ((c.m_collapse < 0) ? " (no b)" : "") + ": every "
            + std::to_string(1 << m) + " frames (" + seconds + " s); the smaller of it and the system collapse applies, "
            "the system collapse with multiple transmission (STD-43A 3.1.2)";
    }

    // The standard rule on the address less the subtraction; the encoder applies it on the address
    uint64_t ruled = c.m_capcode - std::min<uint64_t>(c.m_capcode, (uint64_t) c.subtract());
    char rule[64];
    std::snprintf(rule, sizeof(rule), "frame %03u, phase %c", (unsigned) ((ruled / 16) % 128), (char) ('A' + (ruled / 4) % 4));
    char sent[64];
    std::snprintf(sent, sizeof(sent), "frame %03u, phase %c", (unsigned) ((c.m_capcode / 16) % 128), (char) ('A' + (c.m_capcode / 4) % 4));

    if ((c.m_letter >= 'A') && (c.m_letter <= 'L')) {
        description += std::string(".\nStandard rule: ") + rule;
    }
    if (sendable)
    {
        description += std::string(".\nSent: the address only, in ") + sent + " of 4 (standard rule on the address)";

        if ((c.m_frame >= 0) && ((unsigned) c.m_frame != (unsigned) ((c.m_capcode / 16) % 128))) {
            description += ", not in the frame of the pager";
        } else if (std::string(rule) != std::string(sent) && (c.m_letter >= 'A') && (c.m_letter <= 'L')) {
            description += ", not in the frame and phase of the standard rule";
        }
    }

    return description + ".";
}

bool RadioPagingFlex::isShortAddress(uint64_t capcode)
{
    return (capcode >= m_shortMin) && (capcode <= m_shortMax);
}

bool RadioPagingFlex::isLongAddress(uint64_t capcode)
{
    return (capcode >= m_longMin) && (capcode <= m_longMax);
}

bool RadioPagingFlex::isValidAddress(uint64_t capcode)
{
    return isShortAddress(capcode) || isLongAddress(capcode);
}

uint32_t RadioPagingFlex::shortAddressWord(uint32_t capcode)
{
    return encodeWord((capcode + shortAddressOffset) & 0x1fffff);
}

bool RadioPagingFlex::longAddressWords(uint64_t capcode, uint32_t words[2])
{
    uint32_t w1;
    uint32_t w2;

    if ((capcode >= 2101249ULL) && (capcode <= 1075843072ULL))
    {
        uint64_t result = capcode - 2068481ULL;
        w1 = (uint32_t) (result % shortAddressOffset) + 1;
        w2 = 2097151u - (uint32_t) (result / shortAddressOffset);
    }
    else if ((capcode >= 1075843073ULL) && (capcode <= 3223326720ULL))
    {
        uint64_t result = capcode - 2068481ULL;
        w1 = (uint32_t) (result % shortAddressOffset) + 1;
        w2 = (uint32_t) (result / shortAddressOffset) + 1933312u;
    }
    else if ((capcode >= 3223326721ULL) && (capcode <= m_longMax))
    {
        uint64_t result = capcode - 2068479ULL;
        w1 = (uint32_t) (result % shortAddressOffset) + 2064383u;
        w2 = (uint32_t) (result / shortAddressOffset) + 1867776u;
    }
    else
    {
        return false;
    }

    words[0] = encodeWord(w1);
    words[1] = encodeWord(w2);
    return true;
}

uint64_t RadioPagingFlex::shortAddressCapcode(uint32_t data)
{
    return (uint64_t) data - shortAddressOffset;
}

bool RadioPagingFlex::longAddressCapcode(uint32_t data1, uint32_t data2, uint64_t& capcode)
{
    if ((data1 >= 1) && (data1 <= shortAddressOffset))
    {
        if ((data2 >= 2064383u) && (data2 <= 2097151u)) {
            capcode = (uint64_t) (2097151u - data2) * shortAddressOffset + (data1 - 1) + 2068481ULL;
        } else if ((data2 >= 1966081u) && (data2 <= 2031616u)) {
            capcode = (uint64_t) (data2 - 1933312u) * shortAddressOffset + (data1 - 1) + 2068481ULL;
        } else {
            return false;
        }
        return true;
    }
    if ((data1 >= 2064383u) && (data1 < 2064383u + shortAddressOffset) && (data2 >= 1966081u) && (data2 <= 1998848u))
    {
        capcode = (uint64_t) (data2 - 1867776u) * shortAddressOffset + (data1 - 2064383u) + 2068479ULL;
        return true;
    }

    return false;
}

uint32_t RadioPagingFlex::alphaVector(unsigned start, unsigned words)
{
    return messageVector(m_vectorAlpha, start, words);
}

uint32_t RadioPagingFlex::messageVector(unsigned type, unsigned start, unsigned words)
{
    uint32_t data = ((type & 7) << 4) | ((start & 0x7f) << 7) | ((words & 0x7f) << 14);
    return encodeWord(addChecksum(data));
}

int RadioPagingFlex::binaryWords(const std::string& hex, int blocking, int headerWords, bool lastFragment)
{
    std::vector<uint8_t> bits = hexBits(hex);

    if (bits.empty() || (blocking < 1) || (blocking > 16)) {
        return -1;
    }

    return headerWords + (int) binaryDataWords(bits, lastFragment ? blocking : 0).size();
}

std::vector<uint32_t> RadioPagingFlex::binaryBody(const std::string& hex, const BinaryParams& binary, int number, bool retrieval,
    int fragment, int fragments, int signature)
{
    std::vector<uint8_t> bits = hexBits(hex);

    if (bits.empty() || (binary.m_blocking < 1) || (binary.m_blocking > 16) || (fragment < 0) || (fragment >= fragments)) {
        return std::vector<uint32_t>();
    }

    const bool first = (fragment == 0);
    const bool last = (fragment == fragments - 1);
    // A fragment that is not the last is packed full: no fill and no padding word (the bits after its last
    // character, fewer than a character, are 0)
    std::vector<uint32_t> data = binaryDataWords(bits, last ? binary.m_blocking : 0);
    std::vector<uint32_t> msg(first ? 2 : 1, 0);

    // Header word 1 (Fig. 3.10.1.2-1, -2): K in bits 0-11 (set last), C in bit 12 (more fragments follow),
    // F in bits 13-14, N in bits 15-20
    msg[0] = ((last ? 0u : 1u) << 12) | (fragmentNumber(fragment) << 13) | (((uint32_t) number & 0x3f) << 15);

    // Header word 2, first fragment only: R, M, D, H, B (16 sent as 0000), s = 0000 (reserved), I, S (of the
    // whole message: all its fragments)
    if (first)
    {
        uint32_t s = (signature >= 0) ? ((uint32_t) signature & 0xff) : binarySignature(bits);

        msg[1] = (retrieval ? 1u : 0u)
            | ((binary.m_mailDrop ? 1u : 0u) << 1)
            | ((binary.m_rightToLeft ? 1u : 0u) << 2)
            | ((binary.m_header ? 1u : 0u) << 3)
            | (((uint32_t) binary.m_blocking & 0xf) << 4)
            | ((binary.m_statusInfo ? 1u : 0u) << 12)
            | (s << 13);
    }
    msg.insert(msg.end(), data.begin(), data.end());

    // K: 1's complement of the sum of 3 groups per word of all words, K counted as 0, 12 bits kept (p.59)
    uint32_t sum = 0;
    for (uint32_t w : msg) {
        sum += groupSum(w);
    }
    msg[0] |= (~sum) & 0xfff;

    return msg;
}

std::vector<uint32_t> RadioPagingFlex::secureBody(const std::string& text, SecureType type, int number, int fragment, int fragments)
{
    if ((fragment < 0) || (fragment >= fragments)) {
        return std::vector<uint32_t>();
    }

    const bool last = (fragment == fragments - 1);
    std::vector<uint32_t> msg(1, 0);

    // Header word of every fragment (Fig. 3.10.1.4-1): K in bits 0-9 (set last), C in bit 10 (more fragments
    // follow), F in bits 11-12, N in bits 13-18, t1 t0 in bits 19-20. The data start at word 2, there is
    // no signature
    msg[0] = ((last ? 0u : 1u) << 10) | (fragmentNumber(fragment) << 11) | (((uint32_t) number & 0x3f) << 13)
        | (((uint32_t) type & 3) << 19);

    if (type == SecureAlphanumeric)
    {
        if (text.empty()) {
            return std::vector<uint32_t>();
        }

        // 7-bit characters, 3 a word from the first slot of word 2; unused slots of the last word ETX
        int shift = 0;

        for (char c : text)
        {
            if (shift == 0) {
                msg.push_back(0);
            }
            msg.back() |= (((uint32_t) (unsigned char) c) & 0x7f) << shift;
            shift = (shift + 7) % 21;
        }
        while (shift != 0)
        {
            msg.back() |= alphaEtx << shift;
            shift = (shift + 7) % 21;
        }
    }
    else if ((type == SecureBinary) || (type == SecureSeparate))
    {
        std::vector<uint8_t> bits = hexBits(text);

        if (bits.empty()) {
            return std::vector<uint32_t>();
        }

        // Binary data are terminated as 1-bit characters (p.70), in the last fragment; the termination of
        // data defined separately is not defined: the unused bits are 0
        std::vector<uint32_t> data = binaryDataWords(bits, ((type == SecureBinary) && last) ? 1 : 0);
        msg.insert(msg.end(), data.begin(), data.end());
    }
    else
    {
        return std::vector<uint32_t>();
    }

    // K: 1's complement of the sum of 3 groups per word of all words, K counted as 0, 10 bits kept (p.69)
    uint32_t sum = 0;
    for (uint32_t w : msg) {
        sum += groupSum(w);
    }
    msg[0] |= (~sum) & 0x3ff;

    return msg;
}

bool RadioPagingFlex::fragmentMessage(const PhaseMessage& message, std::vector<PhaseMessage>& fragments, std::string& error,
    int startFragment, int firstMessageWords)
{
    fragments.clear();

    if (!validateMessage(message.m_capcode, message.m_type, message.m_text, error, message.m_binary)) {
        return false;
    }

    PhaseMessage whole = message;
    whole.m_fragment = 0;
    whole.m_fragments = 1;
    whole.m_signature = -1;

    if ((message.m_type != Binary) && (message.m_type != Secure) && (message.m_type != Alphanumeric))
    {
        fragments.push_back(whole);
        return true;
    }

    const bool alphanumeric = (message.m_type == Alphanumeric);
    const bool alpha = (message.m_type == Secure) && (message.m_binary.m_secureType == SecureAlphanumeric);
    // Characters: of B bits (HEX/binary); of 1 bit (secure binary, and data defined separately, which has
    // no termination: its fragments end on whole words so that their bits join)
    const int blocking = (message.m_type == Binary) ? message.m_binary.m_blocking : 1;
    const size_t len = message.m_text.size();
    // Message words of a frame for the address (STD-43A 4.1): 85 short, 84 long; the first piece fewer when its
    // frame has less room
    const int fullWords = maxMessageWords(message.m_capcode);
    std::vector<std::string> pieces;
    size_t pos = 0;

    while (pos < len)
    {
        const int messageWords = (pieces.empty() && (firstMessageWords > 0)) ? std::min(firstMessageWords, fullWords) : fullWords;
        // The first fragment of the message (not of the rest of one)
        const bool first = pieces.empty() && (startFragment == 0);
        // Message words of the fragment: header words 2 (HEX/binary first fragment) or 1, then the data
        const int headerWords = ((message.m_type == Binary) && first) ? 2 : 1;
        const int dataWords = messageWords - headerWords;

        if (dataWords < 1)
        {
            error = "No room for a fragment";
            return false;
        }
        const std::string rest = message.m_text.substr(pos);
        size_t take;

        if (alphanumeric)
        {
            // 3 characters a word, the signature slot first in the first fragment (STD-43A 4.1: 251 characters
            // with a short address). A fragment that is not the last has full words (no ETX): with the
            // fragment control bits U0 V0 = 00 a character may continue in the next fragment (3.10.1.3)
            const size_t room = (size_t) dataWords * 3 - (first ? 1 : 0);
            take = std::min(rest.size(), room);
        }
        else if (alpha)
        {
            // 3 characters a word; a fragment that is not the last has full words (no ETX)
            take = (rest.size() <= (size_t) dataWords * 3) ? rest.size() : (size_t) dataWords * 3;
        }
        else
        {
            // The rest as the last fragment, with its termination, if it fits
            int lastWords = binaryWords(rest, blocking, headerWords, true);

            if ((message.m_type == Secure) && (message.m_binary.m_secureType == SecureSeparate)) {
                lastWords = headerWords + (int) ((rest.size() * 4 + 20) / 21);
            }

            if (lastWords <= messageWords)
            {
                take = rest.size();
            }
            else
            {
                // Packed full: the most bits, a multiple of the blocking length and of 4 (hex digits), with
                // fewer than blocking bits left in the last word; fewer words if no size fits
                int a = blocking, b = 4;
                while (b != 0)
                {
                    int t = a % b;
                    a = b;
                    b = t;
                }
                const int unit = blocking * 4 / a;
                const int restBits = (int) rest.size() * 4;
                int bits = unit;

                // A fragment that is not the last leaves data for the last one: the rest does not fit with
                // its termination, but it may fit without it
                for (int w = dataWords; w >= 1; w--)
                {
                    int cap = w * 21;
                    int fit = (cap / unit) * unit;

                    if ((fit > 0) && (fit < restBits) && (cap - fit < blocking))
                    {
                        bits = fit;
                        break;
                    }
                }

                take = (size_t) (bits / 4);
            }
        }

        if (take == 0)
        {
            error = "No room for a fragment";
            return false;
        }

        pieces.push_back(message.m_text.substr(pos, take));
        pos += take;

        if (startFragment + (int) pieces.size() > m_maxFragments)
        {
            error = "Message too long: more than " + std::to_string(m_maxFragments) + " fragments";
            return false;
        }
    }

    // Signature of the whole message, in the first fragment: HEX/binary in header word 2, alphanumeric in the
    // first character slot. A secure message has none; the rest of a message has the first fragment already
    const int signature = (startFragment > 0) ? -1 : (message.m_type == Binary) ? (int) binarySignature(hexBits(message.m_text))
        : alphanumeric ? (int) alphaSignature(message.m_text) : -1;

    for (size_t k = 0; k < pieces.size(); k++)
    {
        PhaseMessage f = whole;
        f.m_text = pieces[k];
        f.m_fragment = startFragment + (int) k;
        f.m_fragments = startFragment + (int) pieces.size();
        f.m_signature = (k == 0) ? signature : -1;
        fragments.push_back(f);
    }

    return true;
}

uint32_t RadioPagingFlex::numericVector(unsigned start, unsigned words, unsigned kBits, unsigned vectorType)
{
    // The length field holds the number of words minus 1
    unsigned length = (words > 0) ? words - 1 : 0;
    uint32_t data = (vectorType << 4) | ((start & 0x7f) << 7) | ((length & 0x7) << 14) | ((kBits & 0xf) << 17);
    return encodeWord(addChecksum(data));
}

void RadioPagingFlex::shortMessageVector(ShortType type, const std::string& digits, int source, int number, bool retrieval,
    bool longAddress, uint32_t words[2])
{
    uint32_t d = 0;     // d0-d11
    uint32_t d2 = 0;    // d12-d32

    if (type == ShortNumeric)
    {
        uint32_t nibbles[m_maxShortDigitsLong];

        for (int i = 0; i < m_maxShortDigitsLong; i++) {
            nibbles[i] = (i < (int) digits.size()) ? (uint32_t) numericValue(digits[i]) : 0x0cu;
        }

        d = nibbles[0] | (nibbles[1] << 4) | (nibbles[2] << 8);
        d2 = nibbles[3] | (nibbles[4] << 4) | (nibbles[5] << 8) | (nibbles[6] << 12) | (nibbles[7] << 16);
    }
    else if (type == ShortSource)
    {
        d = (uint32_t) source & 7;
    }
    else
    {
        d = ((uint32_t) source & 7) | (((uint32_t) number & 0x3f) << 3) | ((retrieval ? 1u : 0u) << 9);
    }

    words[0] = encodeWord(addChecksum((m_vectorTone << 4) | (((uint32_t) type & 3) << 7) | (d << 9)));
    words[1] = longAddress ? encodeWord(d2) : 0;
}

void RadioPagingFlex::shortInstructionVector(const InstructionParams& instruction, bool longAddress, uint32_t words[2])
{
    // The data: a temporary address is a3-a0 above the frame f6-f0; the events fill d0-d10. Unused bits are 0
    uint32_t d = (instruction.m_type == InstructionParams::SystemEvent)
        ? (instruction.m_events & 0x7ffu)
        : (((uint32_t) instruction.m_frame & 0x7f) | (((uint32_t) instruction.m_slot & 0xf) << 7));

    words[0] = encodeWord(addChecksum((m_vectorInstruction << 4) | (((uint32_t) instruction.m_type & 7) << 7) | (d << 10)));
    words[1] = longAddress ? encodeWord(0) : 0;
}

void RadioPagingFlex::fillIdle(uint32_t *words, int phase, Mode mode)
{
    bool lsbPhase = false;

    if (mode == Mode3200_4) {
        lsbPhase = (phase == 1);
    } else if (mode == Mode6400_4) {
        lsbPhase = ((phase == 1) || (phase == 3));
    }

    for (int w = 0; w < m_wordsPerFrame; w++) {
        words[w] = lsbPhase ? idleZeros : (((w % 2) == 0) ? idleOnes : idleZeros);
    }
}

void RadioPagingFlex::interleaveBlock(uint32_t *words)
{
    uint8_t bytes[32];

    for (int i = 0; i < 32; i++)
    {
        uint8_t b = 0;
        for (int k = 0; k < m_wordsPerBlock; k++) {
            b |= ((words[k] >> (31 - i)) & 1) << (7 - k);
        }
        bytes[i] = b;
    }
    for (int k = 0; k < m_wordsPerBlock; k++) {
        words[k] = ((uint32_t) bytes[4 * k] << 24) | ((uint32_t) bytes[4 * k + 1] << 16)
                 | ((uint32_t) bytes[4 * k + 2] << 8) | bytes[4 * k + 3];
    }
}

void RadioPagingFlex::deinterleaveBlock(uint32_t *words)
{
    uint8_t bytes[32];
    uint32_t out[m_wordsPerBlock];

    for (int k = 0; k < m_wordsPerBlock; k++)
    {
        bytes[4 * k] = words[k] >> 24;
        bytes[4 * k + 1] = words[k] >> 16;
        bytes[4 * k + 2] = words[k] >> 8;
        bytes[4 * k + 3] = words[k];
        out[k] = 0;
    }
    for (int i = 0; i < 32; i++)
    {
        for (int k = 0; k < m_wordsPerBlock; k++) {
            out[k] |= (uint32_t) ((bytes[i] >> (7 - k)) & 1) << (31 - i);
        }
    }
    for (int k = 0; k < m_wordsPerBlock; k++) {
        words[k] = out[k];
    }
}

bool RadioPagingFlex::validateMessage(uint64_t capcode, MessageType type, const std::string& text, std::string& error)
{
    return validateMessage(capcode, type, text, error, BinaryParams());
}

bool RadioPagingFlex::validateMessage(uint64_t capcode, MessageType type, const std::string& text, std::string& error,
    const BinaryParams& binary)
{
    if (!isValidAddress(capcode) && !isTemporaryAddress(capcode) && (capcode != m_systemEventAddress))
    {
        error = "Capcode out of range";
        return false;
    }
    if (type == Numeric)
    {
        if (text.empty())
        {
            error = "Empty numeric message";
            return false;
        }
        if (text.size() > (size_t) m_maxNumericChars)
        {
            error = "Numeric message too long";
            return false;
        }
        for (char c : text)
        {
            if (!numericCharValid(c))
            {
                error = std::string("Character not allowed in numeric message: ") + c;
                return false;
            }
        }
    }
    else if (type == ShortMessage)
    {
        // Empty: every digit a space, the tone only payload
        size_t maxDigits = isLongAddress(capcode) ? m_maxShortDigitsLong : m_maxShortDigits;

        if (text.size() > maxDigits)
        {
            error = "Short message too long: " + std::to_string(maxDigits) + " digits at most";
            return false;
        }
        for (char c : text)
        {
            if (!numericCharValid(c))
            {
                error = std::string("Character not allowed in short message: ") + c;
                return false;
            }
        }
    }
    else if (type == Alphanumeric)
    {
        if (text.empty())
        {
            error = "Empty alphanumeric message";
            return false;
        }
        for (char c : text)
        {
            if (((unsigned char) c) > 0x7f)
            {
                error = "Character above 0x7f in alphanumeric message";
                return false;
            }
        }
    }
    else if (type == Binary)
    {
        if (text.empty())
        {
            error = "Empty HEX/binary message";
            return false;
        }
        if (hexBits(text).empty())
        {
            error = "HEX/binary message: hex digits only";
            return false;
        }
        if ((binary.m_blocking < 1) || (binary.m_blocking > 16))
        {
            error = "HEX/binary message: blocking length 1 to 16 bits";
            return false;
        }
        // Display direction: only for characters of a blocking length other than 0001 (3.10.1.2 p.60)
        if (binary.m_rightToLeft && (binary.m_blocking == 1))
        {
            error = "HEX/binary message: right to left display not allowed with a blocking length of 1";
            return false;
        }
        // A message ends on a character boundary (3.10.1.2 p.61)
        if (((text.size() * 4) % (size_t) binary.m_blocking) != 0)
        {
            error = "HEX/binary message: " + std::to_string(text.size() * 4) + " bits, not a multiple of the blocking length "
                + std::to_string(binary.m_blocking);
            return false;
        }
    }
    else if (type == Secure)
    {
        if (text.empty())
        {
            error = "Empty secure message";
            return false;
        }
        if (binary.m_secureType == SecureAlphanumeric)
        {
            for (char c : text)
            {
                if (((unsigned char) c) > 0x7f)
                {
                    error = "Character above 0x7f in secure message";
                    return false;
                }
            }
        }
        else if ((binary.m_secureType == SecureBinary) || (binary.m_secureType == SecureSeparate))
        {
            if (hexBits(text).empty())
            {
                error = "Secure binary message: hex digits only";
                return false;
            }
        }
        else
        {
            error = "Secure message type: alphanumeric, separate or binary (11 is reserved)";
            return false;
        }
    }

    return true;
}

int RadioPagingFlex::messageCost(const PhaseMessage& message, std::string *error)
{
    std::string reason;

    if (!validateMessage(message.m_capcode, message.m_type, message.m_text, reason, message.m_binary))
    {
        if (error) {
            *error = reason;
        }
        return -1;
    }

    bool isLong = isLongAddress(message.m_capcode);
    int addressWords = isLong ? 2 : 1;

    if (message.m_type == Tone) {
        return addressWords;
    }

    int vectorWords = isLong ? 2 : 1;

    if (message.m_type == ShortInstruction)
    {
        const InstructionParams& i = message.m_instruction;
        const bool temporary = (i.m_type == InstructionParams::TemporaryAddress);
        const bool event = (i.m_type == InstructionParams::SystemEvent);

        // The assignment is sent to an address of a pager, the notification to its own address (STD-43A 3.8.2.4)
        if ((!temporary && !event) || (temporary && !isValidAddress(message.m_capcode)) || (event && (message.m_capcode != m_systemEventAddress))
            || (i.m_slot < 0) || (i.m_slot >= (int) m_temporaryCount) || (i.m_frame < 0) || (i.m_frame >= 128) || (i.m_events > 0x7ffu))
        {
            if (error) {
                *error = "Short instruction: a temporary address (0-15, frame 0-127) to the address of a pager, or the system event flags "
                    "(0-2047) to the System Event Notification address";
            }
            return -1;
        }

        return addressWords + vectorWords;
    }
    if (message.m_capcode == m_systemEventAddress)
    {
        if (error) {
            *error = "The System Event Notification address carries a short instruction";
        }
        return -1;
    }
    if (message.m_type == ShortMessage)
    {
        if (((message.m_shortType != ShortNumeric) && (message.m_shortType != ShortSource) && (message.m_shortType != ShortNumbered))
            || (message.m_source < 0) || (message.m_source > 7) || ((message.m_shortType != ShortNumeric) && !message.m_text.empty()))
        {
            if (error) {
                *error = "Short message: type numeric, source or numbered, source 0 to 7, digits only in the numeric type";
            }
            return -1;
        }

        return addressWords + vectorWords;
    }

    int body = estimateBody(message);

    // HEX/binary and secure: a fragment longer than a frame (fragmentMessage makes fragments that fit)
    if (body < 0)
    {
        if (error) {
            *error = "Message too long for a frame";
        }
        return -1;
    }

    // The first message word of a long address is in the second vector word
    if (isLong && (body > 0)) {
        body--;
    }

    return addressWords + vectorWords + body;
}

RadioPagingFlex::PhaseResult RadioPagingFlex::buildPhase(const PhaseParams& params, const std::vector<PhaseMessage>& messages)
{
    struct Info
    {
        int m_addressWords = 0;
        int m_vectorWords = 0;
        int m_bodyWords = 0;        // Estimate, used for the capacity only
        bool m_long = false;
        bool m_valid = false;
        bool m_packed = false;
    };

    PhaseResult result;
    result.m_packed.assign(messages.size(), false);

    uint32_t words[m_wordsPerFrame];
    fillIdle(words, params.m_phase, params.m_mode);

    std::vector<Info> info(messages.size());
    std::vector<int> order;

    // Priority addresses first, then the others; tone only addresses (no vector) last
    for (int pass = 0; pass < 3; pass++)
    {
        for (size_t i = 0; i < messages.size(); i++)
        {
            const PhaseMessage& m = messages[i];
            bool isTone = (m.m_type == Tone);
            bool inPass = (pass == 0) ? (m.m_priority && !isTone) : (pass == 1) ? (!m.m_priority && !isTone) : isTone;

            if (!inPass || !(isValidAddress(m.m_capcode) || isTemporaryAddress(m.m_capcode) || (m.m_capcode == m_systemEventAddress))) {
                continue;
            }

            Info& in = info[i];
            in.m_long = isLongAddress(m.m_capcode);
            in.m_addressWords = in.m_long ? 2 : 1;
            in.m_vectorWords = isTone ? 0 : (in.m_long ? 2 : 1);

            if (!isTone && (m.m_type != ShortMessage) && (m.m_type != ShortInstruction))
            {
                int body = estimateBody(m);
                if (body < 0) {
                    continue;
                }
                in.m_bodyWords = body;
            }

            in.m_valid = true;
            order.push_back((int) i);
        }
    }

    int totalAddress = 0;
    int totalVector = 0;
    int priorityWords = 0;
    // Block information word 1 and the others (2 to 4), before the address field
    const int extra = (int) std::min<size_t>(params.m_extraBiw.size(), 3);
    const int biwCount = biwWords + extra;
    int capacity = m_wordsPerFrame - biwCount;
    size_t toneStart = order.size();

    for (size_t k = 0; k < order.size(); k++)
    {
        if (messages[order[k]].m_type == Tone)
        {
            toneStart = k;
            break;
        }
    }

    // Greedy packing, stops at the first message that does not fit
    for (size_t k = 0; k < toneStart; k++)
    {
        Info& in = info[order[k]];
        int bodyWords = in.m_bodyWords;

        if (in.m_long && (bodyWords > 0)) {
            bodyWords--;
        }

        int needed = in.m_addressWords + in.m_vectorWords + bodyWords;
        if (needed > capacity) {
            break;
        }

        in.m_packed = true;
        totalAddress += in.m_addressWords;
        totalVector += in.m_vectorWords;
        capacity -= needed;
        if (messages[order[k]].m_priority) {
            priorityWords += in.m_addressWords;
        }
    }

    for (size_t k = toneStart; k < order.size(); k++)
    {
        Info& in = info[order[k]];

        if (in.m_addressWords > capacity) {
            break;
        }

        in.m_packed = true;
        totalAddress += in.m_addressWords;
        capacity -= in.m_addressWords;
    }

    result.m_addressWords = totalAddress;
    priorityWords = std::min(priorityWords, m_maxPriorityWords);

    unsigned vectorStart = (unsigned) (biwCount + totalAddress);
    if (vectorStart > 63)
    {
        result.m_error = "Address field too long";
        return result;
    }

    int fwc = 0;
    words[fwc++] = blockInfoWord(priorityWords, (unsigned) extra, vectorStart, params.m_carryOn, params.m_collapse);

    for (int i = 0; i < extra; i++) {
        words[fwc++] = params.m_extraBiw[i];
    }

    // Address field
    for (int idx : order)
    {
        if (!info[idx].m_packed) {
            continue;
        }

        if (info[idx].m_long)
        {
            uint32_t pair[2] = {0, 0};
            longAddressWords(messages[idx].m_capcode, pair);
            words[fwc++] = pair[0];
            words[fwc++] = pair[1];
        }
        else
        {
            words[fwc++] = shortAddressWord((uint32_t) messages[idx].m_capcode);
        }
    }

    // Vector field and message field
    unsigned vecCursor = vectorStart;
    unsigned msgCursor = vectorStart + (unsigned) totalVector;

    for (size_t k = 0; k < toneStart; k++)
    {
        int idx = order[k];
        const PhaseMessage& m = messages[idx];
        const Info& in = info[idx];

        if (!in.m_packed) {
            continue;
        }

        std::vector<uint32_t> body;
        uint32_t vector;

        if (m.m_type == ShortInstruction)
        {
            // No message field: the vector word(s) only
            uint32_t vw[2];
            shortInstructionVector(m.m_instruction, in.m_long, vw);

            if (vecCursor < (unsigned) m_wordsPerFrame) {
                words[vecCursor++] = vw[0];
            }
            if (in.m_long && (vecCursor < (unsigned) m_wordsPerFrame)) {
                words[vecCursor++] = vw[1];
            }
            continue;
        }

        if (m.m_type == ShortMessage)
        {
            // No message field: the vector word(s) only
            uint32_t vw[2];
            shortMessageVector(m.m_shortType, m.m_text, m.m_source, m.m_number, m.m_retrieval, in.m_long, vw);

            if (vecCursor < (unsigned) m_wordsPerFrame) {
                words[vecCursor++] = vw[0];
            }
            if (in.m_long && (vecCursor < (unsigned) m_wordsPerFrame)) {
                words[vecCursor++] = vw[1];
            }
            continue;
        }

        if (m.m_type == Alphanumeric)
        {
            body = alphaBody(m.m_text, m.m_number, m.m_retrieval, m.m_binary.m_mailDrop, m.m_fragment, m.m_fragments, m.m_signature);
            vector = alphaVector(msgCursor, (unsigned) body.size());
        }
        else if (m.m_type == Binary)
        {
            body = binaryBody(m.m_text, m.m_binary, m.m_number, m.m_retrieval, m.m_fragment, m.m_fragments, m.m_signature);
            vector = messageVector(m_vectorBinary, msgCursor, (unsigned) body.size());
        }
        else if (m.m_type == Secure)
        {
            // No message numbering for secure messages (3.10.1.4 p.70): N tells the messages of an address apart
            body = secureBody(m.m_text, m.m_binary.m_secureType, m.m_number, m.m_fragment, m.m_fragments);
            vector = messageVector(m_vectorSecure, msgCursor, (unsigned) body.size());
        }
        else
        {
            uint32_t kBits = 0;
            body = numericBody(m.m_text, kBits, m.m_numericFormat, m.m_number, m.m_retrieval);
            // A one word numeric message of a long address is in the second vector word
            unsigned start = (in.m_long && (in.m_bodyWords == 1)) ? vecCursor + 1 : msgCursor;
            const unsigned type = (m.m_numericFormat == NumericSpecial) ? m_vectorSpecialNumeric
                : ((m.m_numericFormat == NumericNumbered) || (m.m_numericFormat == NumericNumberedSpecial)) ? m_vectorNumberedNumeric
                : m_vectorNumeric;
            vector = numericVector(start, (unsigned) body.size(), kBits, type);
        }

        for (uint32_t& w : body) {
            w = encodeWord(w);
        }

        if (vecCursor < (unsigned) m_wordsPerFrame) {
            words[vecCursor++] = vector;
        }
        if (in.m_long && (vecCursor < (unsigned) m_wordsPerFrame)) {
            words[vecCursor++] = body.empty() ? encodeWord(0) : body[0];
        }

        for (size_t j = in.m_long ? 1 : 0; j < body.size(); j++)
        {
            if (msgCursor < (unsigned) m_wordsPerFrame) {
                words[msgCursor++] = body[j];
            }
        }
    }

    if ((msgCursor > (unsigned) m_wordsPerFrame) || (vecCursor > (unsigned) m_wordsPerFrame))
    {
        result.m_error = "Phase overflow";
        return result;
    }

    for (int b = 0; b < m_blocksPerFrame; b++) {
        interleaveBlock(words + b * m_wordsPerBlock);
    }

    for (size_t i = 0; i < messages.size(); i++) {
        result.m_packed[i] = info[i].m_packed;
    }

    result.m_words.assign(words, words + m_wordsPerFrame);
    result.m_ok = true;
    return result;
}

unsigned RadioPagingFlex::lowTrafficFlags(const std::vector<int>& addressWords, int phases)
{
    unsigned flags = 0;

    for (int p = 0; (p < phases) && (p < m_maxPhases) && (p < (int) addressWords.size()); p++)
    {
        if (biwWords + addressWords[p] <= m_wordsPerBlock) {
            flags |= 1u << p;
        }
    }

    if (phases == 1)
    {
        flags = (flags & 1) ? 0xf : 0;
    }
    else if (phases == 2)
    {
        unsigned t0 = flags & 1;
        unsigned t1 = (flags >> 1) & 1;
        flags = t0 | (t0 << 1) | (t1 << 2) | (t1 << 3);
    }

    return flags & 0xf;
}

RadioPagingFlex::FrameBytes RadioPagingFlex::assemble(const FrameParams& params, const std::vector<std::vector<uint32_t>>& phases)
{
    FrameBytes out;
    int mode = (int) params.m_mode;
    int phaseTotal = phaseCount(params.m_mode);
    bool fourLevel = (levels(params.m_mode) == 4);

    if ((int) phases.size() != phaseTotal) {
        return out;
    }

    // Sync part: bit sync, mode code, B, inverted mode code, frame information word
    out.m_sync.insert(out.m_sync.end(), syncBs1, syncBs1 + 4);
    out.m_sync.insert(out.m_sync.end(), syncA[mode], syncA[mode] + 4);
    out.m_sync.insert(out.m_sync.end(), syncB, syncB + 2);
    out.m_sync.insert(out.m_sync.end(), syncAInv[mode], syncAInv[mode] + 4);

    uint32_t fiw = frameInfoWord(params.m_cycle, params.m_frame, params.m_roaming ? 1 : 0, 0, params.m_traffic);
    for (int s = 24; s >= 0; s -= 8) {
        out.m_sync.push_back((fiw >> s) & 0xff);
    }

    // Second sync: alternating pattern, C, inverted alternating pattern and inverted C. The
    // pattern has 4, 24 or 64 bits (4-level: 2 bit symbols 10, 00, 10, 00 ...)
    int patternBits = (params.m_mode == Mode1600_2) ? 4 : (params.m_mode == Mode6400_4) ? 64 : 24;
    std::vector<uint8_t> bits;

    auto pattern = [&](bool inverse) {
        if (fourLevel)
        {
            for (int s = 0; s < patternBits / 2; s++)
            {
                bool one = (((s & 1) == 0) != inverse);
                bits.push_back(one ? 1 : 0);
                bits.push_back(0);
            }
        }
        else
        {
            for (int i = 0; i < patternBits; i++) {
                bits.push_back((((i & 1) == 0) != inverse) ? 1 : 0);
            }
        }
    };
    auto sixteen = [&](uint32_t value) {
        for (int i = 15; i >= 0; i--) {
            bits.push_back((value >> i) & 1);
        }
    };

    pattern(false);
    sixteen(syncC);
    pattern(true);
    sixteen(syncCInv);

    for (size_t i = 0; i < bits.size(); i += 8)
    {
        uint8_t b = 0;
        for (size_t j = 0; j < 8; j++) {
            b = (b << 1) | ((i + j < bits.size()) ? bits[i + j] : 0);
        }
        out.m_data.push_back(b);
    }

    // Phase data. The bits of the phases are interleaved word by word (most significant bit first)
    uint8_t cur = 0;
    int curBits = 0;

    auto put = [&](int count, unsigned value) {
        cur = (cur << count) | (value & ((1u << count) - 1));
        curBits += count;
        if (curBits == 8)
        {
            out.m_data.push_back(cur);
            cur = 0;
            curBits = 0;
        }
    };

    for (int w = 0; w < m_wordsPerFrame; w++)
    {
        if (phaseTotal == 1)
        {
            for (int s = 24; s >= 0; s -= 8) {
                out.m_data.push_back((phases[0][w] >> s) & 0xff);
            }
            continue;
        }

        for (int bit = 31; bit >= 0; bit--)
        {
            unsigned a = (phases[0][w] >> bit) & 1;
            unsigned c = (phases[1][w] >> bit) & 1;

            if (phaseTotal == 2)
            {
                if (fourLevel)
                {
                    put(2, (a << 1) | c);
                }
                else
                {
                    put(1, a);
                    put(1, c);
                }
            }
            else
            {
                unsigned b = c;
                unsigned c3 = (phases[2][w] >> bit) & 1;
                unsigned d = (phases[3][w] >> bit) & 1;
                put(2, (a << 1) | b);
                put(2, (c3 << 1) | d);
            }
        }
    }

    return out;
}

std::vector<uint8_t> RadioPagingFlex::bytesToSymbols(const std::vector<uint8_t>& bytes, int levelCount, bool inverted)
{
    static const uint8_t gray[4] = {0, 1, 3, 2};
    std::vector<uint8_t> symbols;

    for (uint8_t byte : bytes)
    {
        if (levelCount == 2)
        {
            for (int i = 7; i >= 0; i--)
            {
                uint8_t s = (byte >> i) & 1;
                symbols.push_back(inverted ? 1 - s : s);
            }
        }
        else
        {
            for (int i = 6; i >= 0; i -= 2)
            {
                uint8_t s = gray[(byte >> i) & 3];
                symbols.push_back(inverted ? 3 - s : s);
            }
        }
    }

    return symbols;
}

std::vector<RadioPagingFlex::SymbolBlock> RadioPagingFlex::toSymbols(const FrameBytes& bytes, Mode mode, bool inverted)
{
    std::vector<SymbolBlock> blocks(2);

    blocks[0].m_baud = m_syncBaud;
    blocks[0].m_levels = 2;
    blocks[0].m_symbols = bytesToSymbols(bytes.m_sync, 2, inverted);
    blocks[1].m_baud = baud(mode);
    blocks[1].m_levels = levels(mode);
    blocks[1].m_symbols = bytesToSymbols(bytes.m_data, levels(mode), inverted);
    return blocks;
}

RadioPagingFlex::SymbolBlock RadioPagingFlex::ers(int bits, bool inverted)
{
    SymbolBlock block;
    std::vector<uint8_t> cycle(ersCycle, ersCycle + sizeof(ersCycle));
    std::vector<uint8_t> cycleSymbols = bytesToSymbols(cycle, 2, inverted);

    block.m_baud = m_syncBaud;
    block.m_levels = 2;
    block.m_symbols.reserve(std::max(0, bits));

    for (int i = 0; i < bits; i++) {
        block.m_symbols.push_back(cycleSymbols[i % cycleSymbols.size()]);
    }

    return block;
}

int RadioPagingFlex::ersMinBits(int collapse)
{
    collapse = std::max(0, std::min(m_maxCollapse, collapse));
    return m_bitsPerFrame << collapse;
}
