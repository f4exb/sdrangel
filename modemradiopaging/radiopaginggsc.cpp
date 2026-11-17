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

#include "radiopaginggsc.h"

#include <algorithm>
#include <array>
#include <cstdio>

const uint16_t RadioPagingGsc::m_startCode;
const uint16_t RadioPagingGsc::m_activationCode;
const int RadioPagingGsc::m_preambleValues;
const int RadioPagingGsc::m_commaBits;
const int RadioPagingGsc::m_codewordBits;
const int RadioPagingGsc::m_preambleCodewords;
const int RadioPagingGsc::m_blockBits;
const int RadioPagingGsc::m_toneCommaBits;
const int RadioPagingGsc::m_slotBits;
const int RadioPagingGsc::m_payloadBitsPerBlock;
const int RadioPagingGsc::m_maxBlocks;
const int RadioPagingGsc::m_maxMessageBits;
const int RadioPagingGsc::m_batchSlots;
const int RadioPagingGsc::m_maxBatchMessages;
const int RadioPagingGsc::m_nonBatterySaverBits;
const int RadioPagingGsc::m_dataBitRate;

namespace {

const uint32_t golayGenerator = 0xc75;      // x^11 + x^10 + x^6 + x^5 + x^4 + x^2 + 1
const uint32_t bchGenerator = 0x117;        // x^8 + x^4 + x^2 + x + 1
const uint32_t golayComplement = 0x7fffff;

const uint16_t preambleValues[10] = {
    2030, 1628, 3198, 647, 191, 3315, 1949, 2540, 1560, 2335
};

const uint16_t word1Values[50] = {
     721, 2731, 2952, 1387, 1578, 1708, 2650, 1747, 2580, 1376,
    2692,  696, 1667, 3800, 3552, 3424, 1384, 3595,  876, 3124,
    2285, 2608,  899, 3684, 3129, 2124, 1287, 2616, 1647, 3216,
     375, 1232, 2824, 1840,  408, 3127, 3387,  882, 3468, 3267,
    1575, 3463, 3152, 2572, 1252, 2592, 1552,  835, 1440,  160
};

// Last three address digits that cannot be used, for groups 0-49 and 50-99
const int illegalLow[16] = { 0, 25, 51, 103, 206, 340, 363, 412, 445, 530, 642, 726, 782, 810, 825, 877 };
const int illegalHigh[7] = { 0, 292, 425, 584, 631, 841, 851 };

// Remainder of a polynomial of degree below 23 divided by the Golay generator
uint32_t golayRemainder(uint32_t poly)
{
    for (int bit = 22; bit >= 11; bit--)
    {
        if (poly & (1u << bit)) {
            poly ^= golayGenerator << (bit - 11);
        }
    }

    return poly & 0x7ff;
}

// Remainder of a polynomial of degree below 15 divided by the BCH generator
uint32_t bchRemainder(uint32_t poly)
{
    for (int bit = 14; bit >= 8; bit--)
    {
        if (poly & (1u << bit)) {
            poly ^= bchGenerator << (bit - 8);
        }
    }

    return poly & 0xff;
}

// Check bits for each remainder of the data part, and the error pattern for each
// syndrome of the correctable patterns (weight 0 to 3)
struct GolayTables
{
    std::array<uint16_t, 2048> m_check;
    std::array<uint32_t, 2048> m_pattern;
    std::array<int8_t, 2048> m_weight;

    GolayTables()
    {
        m_check.fill(0);
        m_pattern.fill(0);
        m_weight.fill(-1);

        // The check bits c give remainder(c << 12) = remainder(data) for a codeword
        for (uint32_t c = 0; c < 2048; c++) {
            m_check[golayRemainder(c << 12)] = (uint16_t) c;
        }

        m_weight[0] = 0;

        for (int i = 0; i < 23; i++)
        {
            uint32_t e1 = 1u << i;
            add(e1, 1);

            for (int j = i + 1; j < 23; j++)
            {
                uint32_t e2 = e1 | (1u << j);
                add(e2, 2);

                for (int k = j + 1; k < 23; k++) {
                    add(e2 | (1u << k), 3);
                }
            }
        }
    }

    void add(uint32_t pattern, int weight)
    {
        uint32_t s = golayRemainder(pattern);
        m_pattern[s] = pattern;
        m_weight[s] = (int8_t) weight;
    }
};

struct BchTables
{
    std::array<uint16_t, 256> m_check;
    std::array<uint16_t, 256> m_pattern;
    std::array<int8_t, 256> m_weight;

    BchTables()
    {
        m_check.fill(0);
        m_pattern.fill(0);
        m_weight.fill(-1);

        for (uint32_t c = 0; c < 256; c++) {
            m_check[bchRemainder(c << 7)] = (uint16_t) c;
        }

        m_weight[0] = 0;

        for (int i = 0; i < 15; i++)
        {
            uint32_t e1 = 1u << i;
            add(e1, 1);

            for (int j = i + 1; j < 15; j++) {
                add(e1 | (1u << j), 2);
            }
        }
    }

    void add(uint32_t pattern, int weight)
    {
        uint32_t s = bchRemainder(pattern);
        m_pattern[s] = (uint16_t) pattern;
        m_weight[s] = (int8_t) weight;
    }
};

const GolayTables& golayTables()
{
    static const GolayTables tables;
    return tables;
}

const BchTables& bchTables()
{
    static const BchTables tables;
    return tables;
}

// The characters of Table VII (the glyphs of the default codepage), 0 for Null
const char alphaChars[65] =
    " !\"#$%&'()*+,-./0123456789:;<=>?"
    "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\r]\0_";

const char numericShiftChars[17] = "ABCDE FGHJ\0LNPR\0";

const char tripleBarUtf8[] = "\xe2\x89\xa1";    // U+2261 IDENTICAL TO

const int alphaNull = 0x3e;
const int numericNull = 0xa;
const int numericTripleBar = 0xe;
const int numericShift = 0xf;
const int numericShiftSpare = 0xf;          // In the shifted set

void appendSpare(std::string& text, unsigned code)
{
    char buf[8];
    std::snprintf(buf, sizeof(buf), "<0x%02X>", code);
    text += buf;
}

// The code of an ASCII character in Table VII (the default codepage: lowercase as uppercase, \ { } LF as @ [ ] CR),
// space for the others
int alphaCode(char c)
{
    const std::string text(1, c);
    size_t pos = 0;
    int code = RadioPagingCharset::encodeOne(*RadioPagingCharset::defaultFor("gsc"), text, pos);
    return (code < 0) ? 0 : code;
}

// Nibble sequence of a numeric character: 0-15, or 0xf0 + n for shift followed by n.
// -1 if the character is not allowed
int numericCode(char c)
{
    if ((c >= '0') && (c <= '9')) {
        return c - '0';
    }

    switch (c)
    {
    case 'U': case 'u': return 0xb;
    case ' ': return 0xc;
    case '-': return 0xd;
    case '=': case '*': return 0xe;
    case 'A': case 'a': return 0xf0;
    case 'B': case 'b': return 0xf1;
    case 'C': case 'c': return 0xf2;
    case 'D': case 'd': return 0xf3;
    case 'E': case 'e': return 0xf4;
    case 'F': case 'f': return 0xf6;
    case 'G': case 'g': return 0xf7;
    case 'H': case 'h': return 0xf8;
    case 'J': case 'j': return 0xf9;
    case 'L': case 'l': return 0xfb;
    case 'N': case 'n': return 0xfc;
    case 'P': case 'p': return 0xfd;
    case 'R': case 'r': return 0xfe;
    default: return -1;
    }
}

void appendLsbFirst(std::vector<uint8_t>& bits, unsigned value, int nbBits)
{
    for (int i = 0; i < nbBits; i++) {
        bits.push_back((value >> i) & 1);
    }
}

int streamBit(const std::vector<uint8_t>& packed, int index)
{
    return (packed[index >> 3] >> (7 - (index & 7))) & 1;
}

// Value of nbBits bits (at most 8) of the packed stream starting at index, first bit least significant
unsigned streamValue(const std::vector<uint8_t>& packed, int index, int nbBits)
{
    unsigned v = 0;

    for (int i = 0; i < nbBits; i++) {
        v |= (unsigned) streamBit(packed, index + i) << i;
    }

    return v;
}

// Appends the payload of a message as blocks. Returns false and sets error if it does not fit
bool appendBlocks(std::vector<uint8_t>& out, const RadioPagingGsc::TxMessage& message, std::string& error)
{
    std::vector<uint8_t> payload;

    if (!RadioPagingGsc::messageBits(message, payload, error)) {
        return false;
    }

    int nbBlocks = (int) payload.size() / RadioPagingGsc::m_payloadBitsPerBlock;

    if (nbBlocks > RadioPagingGsc::m_maxBlocks)
    {
        error = "Message too long";
        return false;
    }

    for (int b = 0; b < nbBlocks; b++)
    {
        const uint8_t *p = &payload[b * RadioPagingGsc::m_payloadBitsPerBlock];
        uint8_t words[8];
        uint16_t codewords[8];
        uint32_t sum = 0;

        for (int k = 0; k < 6; k++)
        {
            words[k] = 0;
            for (int i = 0; i < 7; i++) {
                words[k] |= (uint8_t) (p[7 * k + i] << i);
            }
        }

        words[6] = 0;
        for (int i = 0; i < 6; i++) {
            words[6] |= (uint8_t) (p[42 + i] << i);
        }
        if (b + 1 < nbBlocks) {
            words[6] |= 0x40;       // More blocks follow
        }

        for (int k = 0; k < 7; k++)
        {
            codewords[k] = RadioPagingGsc::bchEncode(words[k]);
            sum += codewords[k];
        }

        codewords[7] = RadioPagingGsc::bchEncode(sum & 0x7f);
        RadioPagingGsc::appendBlock(out, codewords);
    }

    return true;
}

} // namespace

bool RadioPagingGsc::messageBits(const TxMessage& message, std::vector<uint8_t>& payload, std::string& error)
{
    const std::string& text = message.m_text;

    if (message.m_type == Alphanumeric)
    {
        for (char c : text)
        {
            unsigned code = message.m_codes ? (unsigned char) c : (unsigned) alphaCode(c);

            if (code > 0x3f)
            {
                error = "Code above 0x3F in alphanumeric message";
                return false;
            }

            appendLsbFirst(payload, code, 6);
        }
        while (payload.empty() || ((payload.size() % m_payloadBitsPerBlock) != 0)) {
            appendLsbFirst(payload, alphaNull, 6);
        }
    }
    else
    {
        const size_t tripleBarLength = sizeof(tripleBarUtf8) - 1;

        for (size_t pos = 0; pos < text.size(); pos++)
        {
            char c = text[pos];
            int code;

            if (text.compare(pos, tripleBarLength, tripleBarUtf8) == 0)
            {
                code = numericTripleBar;
                pos += tripleBarLength - 1;
            }
            else
            {
                code = numericCode(c);
            }

            if (code < 0)
            {
                error = std::string("Character not allowed in numeric message: ") + c;
                return false;
            }
            if (code > 0xf)
            {
                appendLsbFirst(payload, 0xf, 4);
                code &= 0xf;
            }
            appendLsbFirst(payload, code, 4);
        }
        while (payload.empty() || ((payload.size() % m_payloadBitsPerBlock) != 0)) {
            appendLsbFirst(payload, numericNull, 4);
        }
    }

    return true;
}

void RadioPagingGsc::appendCodewordPair(std::vector<uint8_t>& bits, uint32_t first, uint32_t second)
{
    appendComma(bits, m_commaBits, first & 1);
    appendDuplicated(bits, first);
    bits.push_back((second & 1) ^ 1);
    appendDuplicated(bits, second);
}

void RadioPagingGsc::appendActivationCode(std::vector<uint8_t>& bits)
{
    uint32_t activation = golayEncode(m_activationCode);
    appendCodewordPair(bits, activation, activation ^ golayComplement);
}

uint32_t RadioPagingGsc::golayEncode(uint16_t data)
{
    uint32_t d = data & 0xfff;
    return d | ((uint32_t) golayTables().m_check[golayRemainder(d)] << 12);
}

uint32_t RadioPagingGsc::golaySyndrome(uint32_t word)
{
    return golayRemainder(word & 0x7fffff);
}

RadioPagingGsc::GolayResult RadioPagingGsc::golayDecode(uint32_t word)
{
    const GolayTables& t = golayTables();
    uint32_t w = word & 0x7fffff;
    uint32_t s = golayRemainder(w);
    GolayResult result;

    result.m_word = w ^ t.m_pattern[s];
    result.m_data = (uint16_t) (result.m_word & 0xfff);
    result.m_bitErrors = t.m_weight[s];

    return result;
}

uint16_t RadioPagingGsc::bchEncode(uint8_t data)
{
    uint32_t d = data & 0x7f;
    return (uint16_t) (d | ((uint32_t) bchTables().m_check[bchRemainder(d)] << 7));
}

uint32_t RadioPagingGsc::bchSyndrome(uint16_t word)
{
    return bchRemainder(word & 0x7fff);
}

RadioPagingGsc::BchResult RadioPagingGsc::bchDecode(uint16_t word)
{
    const BchTables& t = bchTables();
    uint32_t w = word & 0x7fff;
    uint32_t s = bchRemainder(w);
    BchResult result;

    result.m_bitErrors = t.m_weight[s];
    result.m_word = (uint16_t) ((result.m_bitErrors < 0) ? w : (w ^ t.m_pattern[s]));
    result.m_data = (uint8_t) (result.m_word & 0x7f);

    return result;
}

uint16_t RadioPagingGsc::preambleValue(int index)
{
    const int count = (int) (sizeof(preambleValues) / sizeof(preambleValues[0]));
    return ((index >= 0) && (index < count)) ? preambleValues[index] : preambleValues[0];
}

int RadioPagingGsc::preambleIndex(uint16_t value)
{
    for (int i = 0; i < 10; i++)
    {
        if (preambleValues[i] == value) {
            return i;
        }
    }

    return -1;
}

int RadioPagingGsc::word1Index(uint16_t word1)
{
    for (int i = 0; i < 50; i++)
    {
        if (word1Values[i] == word1) {
            return i;
        }
    }

    return -1;
}

bool RadioPagingGsc::encodeAddress(const std::string& address, int& preamble, uint16_t& word1, uint16_t& word2, std::string *error)
{
    if (address.size() != 6)
    {
        if (error) {
            *error = "Address must have 6 digits";
        }
        return false;
    }

    int d[6];

    for (int i = 0; i < 6; i++)
    {
        if ((address[i] < '0') || (address[i] > '9'))
        {
            if (error) {
                *error = "Address must have digits only";
            }
            return false;
        }
        d[i] = address[i] - '0';
    }

    int group = d[1] * 10 + d[2];
    int low = d[3] * 100 + d[4] * 10 + d[5];

    if (group < 50)
    {
        for (int v : illegalLow)
        {
            if (v == low)
            {
                if (error) {
                    *error = "Address digits not allowed for this group";
                }
                return false;
            }
        }
    }
    else
    {
        for (int v : illegalHigh)
        {
            if (v == low)
            {
                if (error) {
                    *error = "Address digits not allowed for this group";
                }
                return false;
            }
        }
    }

    int doubled = 2 * low;

    preamble = (d[0] + d[2]) % 10;
    word1 = word1Values[group % 50];
    word2 = (uint16_t) ((doubled / 100) * 100 + (doubled % 100) / 2 + ((group >= 50) ? 50 : 0));

    return true;
}

std::string RadioPagingGsc::addressText(const std::string& address, int addressNumber)
{
    // The 6 characters of the address, zero padded, then / and the address number
    std::string text = (address.size() < 6) ? std::string(6 - address.size(), '0') + address : address;
    return text + '/' + (char) ('0' + addressNumber);
}

char RadioPagingGsc::functionLetter(int addressNumber)
{
    return ((addressNumber >= 1) && (addressNumber <= 4)) ? (char) ('A' + addressNumber - 1) : 0;
}

int RadioPagingGsc::addressNumber(char numberOrFunction)
{
    if ((numberOrFunction >= '1') && (numberOrFunction <= '4')) {
        return numberOrFunction - '0';
    }
    if ((numberOrFunction >= 'A') && (numberOrFunction <= 'D')) {
        return numberOrFunction - 'A' + 1;
    }
    if ((numberOrFunction >= 'a') && (numberOrFunction <= 'd')) {
        return numberOrFunction - 'a' + 1;
    }

    return 0;
}

int RadioPagingGsc::suffixAddressNumber(char digit)
{
    if ((digit < '0') || (digit > '9')) {
        return 0;
    }

    // Suffix 1-4 and 5-8: address 1-4 (functions A-D); 9 and 0: address 1 and 2 (functions A and B)
    int d = digit - '0';
    return (d == 0) ? 2 : (d == 9) ? 1 : ((d - 1) % 4) + 1;
}

bool RadioPagingGsc::parseCapCode(const std::string& capCode, std::string& address, int& addressNumber, std::string *error)
{
    if (capCode.empty() || (capCode.size() > 7))
    {
        if (error) {
            *error = "Cap code must have 1 to 7 digits";
        }
        return false;
    }

    for (char c : capCode)
    {
        if ((c < '0') || (c > '9'))
        {
            if (error) {
                *error = "Cap code must have digits only";
            }
            return false;
        }
    }

    address = (capCode.size() == 7) ? capCode.substr(0, 6) : std::string(6 - capCode.size(), '0') + capCode;
    addressNumber = (capCode.size() == 7) ? suffixAddressNumber(capCode[6]) : 0;

    int preamble;
    uint16_t word1, word2;
    return encodeAddress(address, preamble, word1, word2, error);
}

std::string RadioPagingGsc::capCodeText(const std::string& capCode)
{
    std::string address, error;
    int number;

    if (!parseCapCode(capCode, address, number, &error)) {
        return error;
    }

    int preamble;
    uint16_t word1, word2;
    encodeAddress(address, preamble, word1, word2);

    std::string text = "index " + address.substr(0, 1) + ((capCode.size() < 6) ? " (padded)" : "")
        + ", group " + address.substr(1, 2) + ", address " + address.substr(3, 3);

    if (number > 0) {
        text += std::string(", suffix ") + capCode[6] + " = " + (char) ('0' + number) + " (" + functionLetter(number) + ")";
    }

    text += " -> preamble " + std::to_string(preamble) + ", word 1 " + std::to_string(word1)
        + ", word 2 " + std::to_string(word2);
    return text;
}

bool RadioPagingGsc::decodeAddress(uint16_t word1, uint16_t word2, int preamble, std::string& address)
{
    int group = word1Index(word1);

    if (group < 0) {
        return false;
    }

    bool high = (word2 % 100) >= 50;
    int raw = word2 - (high ? 50 : 0);
    int low = (raw / 100) * 50 + (raw % 100);

    if (low > 999) {
        return false;
    }
    if (high) {
        group += 50;
    }

    if (group < 50)
    {
        for (int v : illegalLow)
        {
            if (v == low) {
                return false;
            }
        }
    }
    else
    {
        for (int v : illegalHigh)
        {
            if (v == low) {
                return false;
            }
        }
    }

    // Without a preamble (non battery saver) there is no index digit: N takes its place
    address.assign(6, 'N');
    if (preamble >= 0) {
        address[0] = (char) ('0' + ((preamble - (group % 10) + 10) % 10));
    }
    address[1] = (char) ('0' + group / 10);
    address[2] = (char) ('0' + group % 10);
    address[3] = (char) ('0' + low / 100);
    address[4] = (char) ('0' + (low / 10) % 10);
    address[5] = (char) ('0' + low % 10);

    return true;
}

void RadioPagingGsc::appendComma(std::vector<uint8_t>& bits, int nbBits, int firstBit)
{
    int bit = firstBit & 1;

    for (int i = 0; i < nbBits; i++)
    {
        bits.push_back((uint8_t) bit);
        bit ^= 1;
    }
}

void RadioPagingGsc::appendDuplicated(std::vector<uint8_t>& bits, uint32_t codeword)
{
    for (int i = 0; i < 23; i++)
    {
        uint8_t bit = (codeword >> i) & 1;
        bits.push_back(bit);
        bits.push_back(bit);
    }
}

void RadioPagingGsc::appendBlock(std::vector<uint8_t>& bits, const uint16_t codewords[8])
{
    bits.push_back((codewords[0] & 1) ^ 1);

    for (int j = 0; j < 15; j++)
    {
        for (int k = 0; k < 8; k++) {
            bits.push_back((codewords[k] >> j) & 1);
        }
    }
}

uint32_t RadioPagingGsc::resolveDuplicated(const uint8_t *bits)
{
    uint32_t word = 0;

    for (int i = 0; i < 23; i++) {
        word |= (uint32_t) (bits[2 * i] & 1) << i;
    }

    return word;
}

uint32_t RadioPagingGsc::resolveDuplicated(const float *soft)
{
    uint32_t word = 0;

    for (int i = 0; i < 23; i++)
    {
        float sum = soft[2 * i] + soft[2 * i + 1];
        bool one = (sum > 0.0f) || ((sum == 0.0f) && (soft[2 * i] > 0.0f));

        word |= (uint32_t) (one ? 1 : 0) << i;
    }

    return word;
}

const char *RadioPagingGsc::tripleBar()
{
    return tripleBarUtf8;
}

void RadioPagingGsc::deinterleaveBlock(const uint8_t *bits, uint16_t codewords[8])
{
    for (int k = 0; k < 8; k++) {
        codewords[k] = 0;
    }

    for (int j = 0; j < 15; j++)
    {
        for (int k = 0; k < 8; k++) {
            codewords[k] |= (uint16_t) ((bits[j * 8 + k] & 1) << j);
        }
    }
}

RadioPagingGsc::EncodeResult RadioPagingGsc::encode(const std::vector<TxMessage>& messages, const EncodeOptions& options)
{
    EncodeResult result;

    if (messages.empty())
    {
        result.m_error = "No messages";
        return result;
    }

    Mode mode = options.m_mode;

    if (mode == Auto) {
        mode = (messages.size() == 1) ? Single : Batch;
    }
    // Non battery saver: any number of consecutive paging calls after the pattern
    // (Rep. 900-2 Annex I 2.5)
    if ((mode == Single) && (messages.size() != 1))
    {
        result.m_error = "This mode takes exactly one message";
        return result;
    }
    if (messages.size() > (size_t) m_maxBatchMessages)
    {
        result.m_error = "Too many messages for a batch";
        return result;
    }

    std::vector<uint16_t> word1(messages.size());
    std::vector<uint16_t> word2(messages.size());
    int preamble = -1;

    for (size_t i = 0; i < messages.size(); i++)
    {
        const TxMessage& message = messages[i];
        int p;

        if (!encodeAddress(message.m_address, p, word1[i], word2[i], &result.m_error)) {
            return result;
        }
        if ((message.m_addressNumber < 1) || (message.m_addressNumber > 4))
        {
            result.m_error = "Address number must be 1 to 4";
            return result;
        }
        if (message.m_type == Data)
        {
            result.m_error = "Message type must be tone, voice, numeric or alphanumeric";
            return result;
        }
        if ((mode == Batch) && (message.m_type == Voice))
        {
            result.m_error = "Voice messages cannot be sent in a batch";
            return result;
        }
        if ((mode == Batch) && (i > 0) && (p != preamble))
        {
            result.m_error = "Messages of a batch must use the same preamble value";
            return result;
        }
        preamble = p;
    }

    std::vector<uint8_t>& bits = result.m_bits;

    if (mode == NonBatterySaver)
    {
        for (int i = 0; i < m_nonBatterySaverBits / 4; i++)
        {
            bits.push_back(1);
            bits.push_back(1);
            bits.push_back(0);
            bits.push_back(0);
        }
        result.m_preamble = -1;
    }
    else
    {
        uint32_t word = golayEncode(preambleValue(preamble));

        if (mode == Batch) {
            word ^= golayComplement;
        }

        appendComma(bits, m_commaBits, word & 1);
        for (int i = 0; i < m_preambleCodewords; i++) {
            appendDuplicated(bits, word);
        }

        uint32_t start = golayEncode(m_startCode);
        appendCodewordPair(bits, start, start ^ golayComplement);
        result.m_preamble = preamble;
    }

    // Batch slots are blocks of 121 bits: an address, a data block, a repeated start code
    // or a fill comma (Rep. 900-2 Annex I 2.1 and 2.6: a data block has the length of an
    // address block and takes its place in the batch). 'used' counts the slots of the
    // current batch after its start code
    size_t slotsStart = bits.size();
    int used = 0;

    for (size_t i = 0; i < messages.size(); i++)
    {
        const TxMessage& message = messages[i];
        int function = message.m_addressNumber - 1;

        // A batch holds 16 slots. A message started in it runs on to its end (Rep. 900-2
        // Annex I 2.1, which would also allow a message to start in the 17th block; the
        // start code is sent there instead), and the next address extends the batch with
        // the start code, without a new preamble (Motorola GSC manual, Figure 6-2,
        // extended batch transmission mode)
        if ((mode == Batch) && (used >= m_batchSlots))
        {
            uint32_t startCode = golayEncode(m_startCode);
            appendCodewordPair(bits, startCode, startCode ^ golayComplement);
            used = 0;
        }

        size_t messageStart = bits.size();
        uint32_t first = golayEncode(word1[i]);
        uint32_t second = golayEncode(word2[i]);

        if (function & 2) {
            first ^= golayComplement;
        }
        if (function & 1) {
            second ^= golayComplement;
        }
        appendCodewordPair(bits, first, second);

        switch (message.m_type)
        {
        case Voice:
            appendActivationCode(bits);
            break;
        case Numeric:
        case Alphanumeric:
            if (!appendBlocks(bits, message, result.m_error))
            {
                bits.clear();
                return result;
            }
            break;
        default:
            // A tone only page is its address alone: the next address, start code or
            // preamble follows at once (Rep. 900-2 Annex I 2.5; Motorola GSC manual,
            // Figure 6-2, individual and batch transmission modes)
            break;
        }

        const int blocks = (int) ((bits.size() - messageStart) / m_blockBits);
        result.m_messageSlots.push_back(used);
        result.m_messageBlocks.push_back(blocks);
        used += blocks;
    }

    // Unfilled batches are filled out with comma (Rep. 900-2 Annex I 2.1)
    if (mode == Batch)
    {
        for (; used < m_batchSlots; used++) {
            appendComma(bits, m_slotBits, 1);
        }
    }

    result.m_slots = (int) ((bits.size() - slotsStart) / m_blockBits);
    result.m_batch = (mode == Batch);

    result.m_ok = true;
    return result;
}

RadioPagingGsc::EncodeResult RadioPagingGsc::encode(const std::vector<TxMessage>& messages)
{
    return encode(messages, EncodeOptions());
}

namespace {

// Code points of a UTF-8 string
int utf8Length(const std::string& s)
{
    return (int) std::count_if(s.begin(), s.end(), [](char c) { return (((unsigned char) c) & 0xc0) != 0x80; });
}

// The worse of two statuses: X lost, c corrected, . received
char worseStatus(char a, char b)
{
    auto rank = [](char c) { return (c == 'X') ? 2 : (c == 'c') ? 1 : 0; };
    return (rank(b) > rank(a)) ? b : a;
}

} // namespace

std::string RadioPagingGsc::decodeNumeric(const std::vector<uint8_t>& packed, int nbBits, const std::vector<uint8_t> *lost,
    const std::vector<uint8_t> *corrected, std::string *status)
{
    // 0x0-0xd, Null (0xa) is not looked up
    static const char numericChars[] = "0123456789\0U -";
    std::string text;
    bool shifted = false;
    char shiftStatus = '.';
    int length = 0;

    if (lost && (lost->size() < packed.size())) {
        lost = nullptr;
    }
    if (corrected && (corrected->size() < packed.size())) {
        corrected = nullptr;
    }
    if (status) {
        status->clear();
    }

    // The characters added to the text take the status s
    auto mark = [&](char s) {
        const int now = utf8Length(text);

        if (status) {
            status->append((size_t) (now - length), s);
        }

        length = now;
    };

    for (int i = 0; i + 4 <= nbBits; i += 4)
    {
        unsigned v = streamValue(packed, i, 4);
        const char digitStatus = (corrected && (streamValue(*corrected, i, 4) != 0)) ? 'c' : '.';

        // A digit of a codeword that could not be corrected, and the shift state with it, are not known
        if (lost && (streamValue(*lost, i, 4) != 0))
        {
            text.push_back('?');
            mark('X');
            shifted = false;
            continue;
        }

        if (shifted)
        {
            shifted = false;
            const char s = worseStatus(shiftStatus, digitStatus);

            // A shift followed by Null is the E of the pagers without the shifted set
            if (v == (unsigned) numericNull) {
                text.push_back('E');
            } else if (v == (unsigned) numericShiftSpare) {
                appendSpare(text, v);
            } else {
                text.push_back(numericShiftChars[v]);
            }

            mark(s);
        }
        else if (v == (unsigned) numericShift)
        {
            shifted = true;
            shiftStatus = digitStatus;
        }
        else if (v == (unsigned) numericTripleBar)
        {
            text += tripleBarUtf8;
            mark(digitStatus);
        }
        else if (v != (unsigned) numericNull)
        {
            text.push_back(numericChars[v]);
            mark(digitStatus);
        }
    }

    // A shift at the end has no character to shift
    if (shifted)
    {
        text.push_back('E');
        mark(shiftStatus);
    }

    return text;
}

std::string RadioPagingGsc::decodeAlphanumeric(const std::vector<uint8_t>& packed, int nbBits, const RadioPagingCharset::Codepage *codepage,
    const std::vector<uint8_t> *lost, const std::vector<uint8_t> *corrected, std::string *status)
{
    const RadioPagingCharset::Codepage& page = codepage ? *codepage : *RadioPagingCharset::defaultFor("gsc");
    std::string text;
    std::vector<uint8_t> codes;
    std::string codeStatus;

    if (lost && (lost->size() < packed.size())) {
        lost = nullptr;
    }
    if (corrected && (corrected->size() < packed.size())) {
        corrected = nullptr;
    }
    if (status) {
        status->clear();
    }

    // A run of codes decoded together (a codepage may join codes): its characters take the status of their code
    // when there is one character a code, else the worst of the run
    auto flush = [&]() {
        const std::string piece = RadioPagingCharset::decode(page, codes);
        const int n = utf8Length(piece);

        if (status)
        {
            if (n == (int) codeStatus.size())
            {
                *status += codeStatus;
            }
            else
            {
                char worst = '.';

                for (char s : codeStatus) {
                    worst = worseStatus(worst, s);
                }

                status->append((size_t) n, worst);
            }
        }

        text += piece;
        codes.clear();
        codeStatus.clear();
    };

    for (int i = 0; i + 6 <= nbBits; i += 6)
    {
        unsigned code = streamValue(packed, i, 6);

        // A character of a codeword that could not be corrected is not known: the run before it is decoded
        if (lost && (streamValue(*lost, i, 6) != 0))
        {
            flush();
            text.push_back('?');

            if (status) {
                status->push_back('X');
            }

            continue;
        }

        // Null/EOM is padding and does not give a character
        if (code != (unsigned) alphaNull)
        {
            codes.push_back((uint8_t) code);
            codeStatus.push_back((corrected && (streamValue(*corrected, i, 6) != 0)) ? 'c' : '.');
        }
    }

    flush();
    return text;
}

int RadioPagingGsc::scoreAlphanumeric(const std::vector<uint8_t>& packed, int nbBits)
{
    int score = 0;
    int fill = 0;

    for (int i = 0; i + 6 <= nbBits; i += 6)
    {
        unsigned code = streamValue(packed, i, 6);

        if (code == (unsigned) alphaNull)
        {
            fill++;
            continue;
        }

        // Null is the padding at the end: one followed by a character is a sign of the wrong type
        score -= 5 * fill;
        fill = 0;
        char c = alphaChars[code];
        unsigned char u = (unsigned char) c;

        if (c == 0) {
            score -= 5;
        } else if (((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9')) || (c == ' ')) {
            score += 3;
        } else if ((u >= 0x20) && (u <= 0x7e)) {
            score -= 2;
        } else {
            score -= 5;
        }
    }

    // The padding of a short message is strong evidence for the type
    return score + 2 * fill * fill;
}

int RadioPagingGsc::scoreNumeric(const std::vector<uint8_t>& packed, int nbBits)
{
    int nbNibbles = nbBits / 4;
    int score = 0;
    int fill = 0;
    int nbU = 0;
    int firstU = -1;

    for (int i = 0; i < nbNibbles; i++)
    {
        unsigned n = streamValue(packed, 4 * i, 4);

        if (n == 0xb)
        {
            nbU++;
            if (firstU < 0) {
                firstU = i;
            }
        }
    }

    // A single leading U is a common prefix, any other U is a sign of the wrong type
    bool prefix = (nbU == 1) && (firstU == 0);

    for (int i = 0; i < nbNibbles; i++)
    {
        unsigned n = streamValue(packed, 4 * i, 4);

        if (n == 0xa)
        {
            fill++;
            continue;
        }

        // Null is the padding at the end: one followed by a digit is a sign of the wrong type
        score -= 5 * fill;
        fill = 0;

        if (n <= 9)
        {
            score += 3;
        }
        else if (n == 0xb)
        {
            score -= prefix ? 1 : 15;
        }
        else
        {
            score -= 2;
        }
    }

    return score + fill * fill;
}

int RadioPagingGsc::longNumericPenalty(const std::string& numeric)
{
    int nbChars = 0;
    bool plain = true;

    // Characters, not bytes: the triple bar is UTF-8
    for (char c : numeric)
    {
        if ((((unsigned char) c) & 0xc0) != 0x80) {
            nbChars++;
        }
        plain = plain && (((c >= '0') && (c <= '9')) || (c == ' ') || (c == '-'));
    }

    // Only digits, spaces and hyphens: the bits of an alphanumeric message are almost never that (3/4 of the codes
    // a digit, to the power of the length), a long number is a number
    if (plain) {
        return 0;
    }

    // Numeric pages are short; a long one is most likely an alphanumeric message. Each digit
    // above the limit costs twice what a digit scores
    return (nbChars > m_longNumericChars) ? 6 * (nbChars - m_longNumericChars) : 0;
}

RadioPagingGsc::Interpretation RadioPagingGsc::interpret(const std::vector<uint8_t>& packed, int nbBits,
    const RadioPagingCharset::Codepage *codepage, const std::vector<uint8_t> *lost, const std::vector<uint8_t> *corrected)
{
    Interpretation result;

    if (nbBits <= 0)
    {
        result.m_type = "tone";
        return result;
    }

    std::string alphaStatus;
    std::string numericStatus;
    std::string alpha = decodeAlphanumeric(packed, nbBits, codepage, lost, corrected, &alphaStatus);
    std::string numeric = decodeNumeric(packed, nbBits, lost, corrected, &numericStatus);
    int alphaScore = scoreAlphanumeric(packed, nbBits);
    int numericScore = scoreNumeric(packed, nbBits) - longNumericPenalty(numeric);

    // A tie is alphanumeric
    if (numericScore > alphaScore)
    {
        result.m_type = "numeric";
        result.m_text = numeric;
        result.m_status = numericStatus;
        result.m_score = numericScore;
        result.m_altType = "alphanumeric";
        result.m_altText = alpha;
        result.m_altScore = alphaScore;
    }
    else
    {
        result.m_type = "alphanumeric";
        result.m_text = alpha;
        result.m_status = alphaStatus;
        result.m_score = alphaScore;
        result.m_altType = "numeric";
        result.m_altText = numeric;
        result.m_altScore = numericScore;
    }

    return result;
}

std::string RadioPagingGsc::escapeControl(const std::string& text)
{
    std::string out;

    for (char c : text)
    {
        unsigned char u = (unsigned char) c;
        if ((u < 0x20) || (u == 0x7f))
        {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "<0x%02X>", u);
            out += buf;
        }
        else
        {
            out.push_back(c);
        }
    }

    return out;
}
