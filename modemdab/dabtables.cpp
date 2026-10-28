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

#include <cmath>

#include "dabtables.h"
#include "modemdr/text.h"

namespace modemdab
{

namespace
{

constexpr float Pi = 3.14159265358979323846f;

// Table 23: first carrier of each block of 32, i and n, for k < 0 then k > 0.
struct PRSRow
{
    int i;
    int n;
};

const PRSRow prsNegative[24] = {
    {0, 1}, {1, 2}, {2, 0}, {3, 1}, {0, 3}, {1, 2}, {2, 2}, {3, 3},
    {0, 2}, {1, 1}, {2, 2}, {3, 3}, {0, 1}, {1, 2}, {2, 3}, {3, 3},
    {0, 2}, {1, 2}, {2, 2}, {3, 1}, {0, 1}, {1, 3}, {2, 1}, {3, 2}
};

const PRSRow prsPositive[24] = {
    {0, 3}, {3, 1}, {2, 1}, {1, 1}, {0, 2}, {3, 2}, {2, 1}, {1, 0},
    {0, 2}, {3, 2}, {2, 3}, {1, 3}, {0, 0}, {3, 2}, {2, 1}, {1, 3},
    {0, 3}, {3, 3}, {2, 3}, {1, 0}, {0, 3}, {3, 0}, {2, 1}, {1, 1}
};

// Table 24: h(i, j) for j = 0..15 (j = 16..31 repeat these values).
const int prsH[4][16] = {
    {0, 2, 0, 0, 0, 0, 1, 1, 2, 0, 0, 0, 2, 2, 1, 1},
    {0, 3, 2, 3, 0, 1, 3, 0, 2, 1, 2, 3, 2, 3, 3, 0},
    {0, 0, 0, 2, 0, 2, 1, 3, 2, 2, 0, 2, 2, 0, 1, 3},
    {0, 1, 2, 1, 0, 3, 3, 2, 2, 3, 2, 1, 2, 1, 3, 2}
};

// Table 13: puncturing vectors VPI for PI = 1..24.
const char *puncturingVectors[24] = {
    "11001000100010001000100010001000",
    "11001000100010001100100010001000",
    "11001000110010001100100010001000",
    "11001000110010001100100011001000",
    "11001100110010001100100011001000",
    "11001100110010001100110011001000",
    "11001100110011001100110011001000",
    "11001100110011001100110011001100",
    "11101100110011001100110011001100",
    "11101100110011001110110011001100",
    "11101100111011001110110011001100",
    "11101100111011001110110011101100",
    "11101110111011001110110011101100",
    "11101110111011001110111011101100",
    "11101110111011101110111011101100",
    "11101110111011101110111011101110",
    "11111110111011101110111011101110",
    "11111110111011101111111011101110",
    "11111110111111101111111011101110",
    "11111110111111101111111011111110",
    "11111111111111101111111011111110",
    "11111111111111101111111111111110",
    "11111111111111111111111111111110",
    "11111111111111111111111111111111"
};

// Table 8 (index order) and table 15: bit rate, protection level, size, L1..L4, PI1..PI4, padding.
struct UEPRow
{
    int bitrate;
    int level;
    int size;
    int l[4];
    int pi[4];
    int padding;
};

const UEPRow uepRows[64] = {
    { 32, 5,  16, { 3,  4,  17, 0}, { 5,  3,  2,  0}, 0},
    { 32, 4,  21, { 3,  3,  18, 0}, {11,  6,  5,  0}, 0},
    { 32, 3,  24, { 3,  4,  14, 3}, {15,  9,  6,  8}, 0},
    { 32, 2,  29, { 3,  4,  14, 3}, {22, 13,  8, 13}, 0},
    { 32, 1,  35, { 3,  5,  13, 3}, {24, 17, 12, 17}, 4},
    { 48, 5,  24, { 4,  3,  26, 3}, { 5,  4,  2,  3}, 0},
    { 48, 4,  29, { 3,  4,  26, 3}, { 9,  6,  4,  6}, 0},
    { 48, 3,  35, { 3,  4,  26, 3}, {15, 10,  6,  9}, 4},
    { 48, 2,  42, { 3,  4,  26, 3}, {24, 14,  8, 15}, 0},
    { 48, 1,  52, { 3,  5,  25, 3}, {24, 18, 13, 18}, 0},
    { 56, 5,  29, { 6, 10,  23, 3}, { 5,  4,  2,  3}, 0},
    { 56, 4,  35, { 6, 10,  23, 3}, { 9,  6,  4,  5}, 0},
    { 56, 3,  42, { 6, 12,  21, 3}, {16,  7,  6,  9}, 0},
    { 56, 2,  52, { 6, 10,  23, 3}, {23, 13,  8, 13}, 8},
    { 64, 5,  32, { 6,  9,  31, 2}, { 5,  3,  2,  3}, 0},
    { 64, 4,  42, { 6,  9,  33, 0}, {11,  6,  5,  0}, 0},
    { 64, 3,  48, { 6, 12,  27, 3}, {16,  8,  6,  9}, 0},
    { 64, 2,  58, { 6, 10,  29, 3}, {23, 13,  8, 13}, 8},
    { 64, 1,  70, { 6, 11,  28, 3}, {24, 18, 12, 18}, 4},
    { 80, 5,  40, { 6, 10,  41, 3}, { 6,  3,  2,  3}, 0},
    { 80, 4,  52, { 6, 10,  41, 3}, {11,  6,  5,  6}, 0},
    { 80, 3,  58, { 6, 11,  40, 3}, {16,  8,  6,  7}, 0},
    { 80, 2,  70, { 6, 10,  41, 3}, {23, 13,  8, 13}, 8},
    { 80, 1,  84, { 6, 10,  41, 3}, {24, 17, 12, 18}, 4},
    { 96, 5,  48, { 7,  9,  53, 3}, { 5,  4,  2,  4}, 0},
    { 96, 4,  58, { 7, 10,  52, 3}, { 9,  6,  4,  6}, 0},
    { 96, 3,  70, { 6, 12,  51, 3}, {16,  9,  6, 10}, 4},
    { 96, 2,  84, { 6, 10,  53, 3}, {22, 12,  9, 12}, 0},
    { 96, 1, 104, { 6, 13,  50, 3}, {24, 18, 13, 19}, 0},
    {112, 5,  58, {14, 17,  50, 3}, { 5,  4,  2,  5}, 0},
    {112, 4,  70, {11, 21,  49, 3}, { 9,  6,  4,  8}, 0},
    {112, 3,  84, {11, 23,  47, 3}, {16,  8,  6,  9}, 0},
    {112, 2, 104, {11, 21,  49, 3}, {23, 12,  9, 14}, 4},
    {128, 5,  64, {12, 19,  62, 3}, { 5,  3,  2,  4}, 0},
    {128, 4,  84, {11, 21,  61, 3}, {11,  6,  5,  7}, 0},
    {128, 3,  96, {11, 22,  60, 3}, {16,  9,  6, 10}, 4},
    {128, 2, 116, {11, 21,  61, 3}, {22, 12,  9, 14}, 0},
    {128, 1, 140, {11, 20,  62, 3}, {24, 17, 13, 19}, 8},
    {160, 5,  80, {11, 19,  87, 3}, { 5,  4,  2,  4}, 0},
    {160, 4, 104, {11, 23,  83, 3}, {11,  6,  5,  9}, 0},
    {160, 3, 116, {11, 24,  82, 3}, {16,  8,  6, 11}, 0},
    {160, 2, 140, {11, 21,  85, 3}, {22, 11,  9, 13}, 0},
    {160, 1, 168, {11, 22,  84, 3}, {24, 18, 12, 19}, 0},
    {192, 5,  96, {11, 20, 110, 3}, { 6,  4,  2,  5}, 0},
    {192, 4, 116, {11, 22, 108, 3}, {10,  6,  4,  9}, 0},
    {192, 3, 140, {11, 24, 106, 3}, {16, 10,  6, 11}, 0},
    {192, 2, 168, {11, 20, 110, 3}, {22, 13,  9, 13}, 8},
    {192, 1, 208, {11, 21, 109, 3}, {24, 20, 13, 24}, 0},
    {224, 5, 116, {12, 22, 131, 3}, { 8,  6,  2,  6}, 4},
    {224, 4, 140, {12, 26, 127, 3}, {12,  8,  4, 11}, 0},
    {224, 3, 168, {11, 20, 134, 3}, {16, 10,  7,  9}, 0},
    {224, 2, 208, {11, 22, 132, 3}, {24, 16, 10, 15}, 0},
    {224, 1, 232, {11, 24, 130, 3}, {24, 20, 12, 20}, 4},
    {256, 5, 128, {11, 24, 154, 3}, { 6,  5,  2,  5}, 0},
    {256, 4, 168, {11, 24, 154, 3}, {12,  9,  5, 10}, 4},
    {256, 3, 192, {11, 27, 151, 3}, {16, 10,  7, 10}, 0},
    {256, 2, 232, {11, 22, 156, 3}, {24, 14, 10, 13}, 8},
    {256, 1, 280, {11, 26, 152, 3}, {24, 19, 14, 18}, 4},
    {320, 5, 160, {11, 26, 200, 3}, { 8,  5,  2,  6}, 4},
    {320, 4, 208, {11, 25, 201, 3}, {13,  9,  5, 10}, 8},
    {320, 2, 280, {11, 26, 200, 3}, {24, 17,  9, 17}, 0},
    {384, 5, 192, {11, 27, 247, 3}, { 8,  6,  2,  7}, 0},
    {384, 3, 280, {11, 24, 250, 3}, {16,  9,  7, 10}, 4},
    {384, 1, 416, {12, 28, 245, 3}, {24, 20, 14, 23}, 8}
};

// TS 101 756 table 12 (all countries except North America) and table 13 (North America).
const char *ptyNames[32] = {
    "None", "News", "Current Affairs", "Information", "Sport", "Education", "Drama", "Culture",
    "Science", "Varied", "Pop Music", "Rock Music", "Easy Listening", "Light Classical",
    "Serious Classical", "Other Music", "Weather", "Finance", "Children's", "Social Affairs",
    "Religion", "Phone In", "Travel", "Leisure", "Jazz Music", "Country Music",
    "National Music", "Oldies Music", "Folk Music", "Documentary", "", ""
};

const char *ptyNamesNorthAmerica[32] = {
    "None", "News", "Information", "Sports", "Talk", "Rock", "Classic Rock", "Adult Hits",
    "Soft Rock", "Top 40", "Country", "Oldies", "Soft", "Nostalgia", "Jazz", "Classical",
    "Rhythm and Blues", "Soft Rhythm and Blues", "Foreign Language", "Religious Music",
    "Religious Talk", "Personality", "Public", "College", "", "", "", "", "", "Weather", "", ""
};

// TS 101 756 tables 9 and 10
const char *languagesLatin[0x2c] = {
    "", "Albanian", "Breton", "Catalan", "Croatian", "Welsh", "Czech", "Danish",
    "German", "English", "Spanish", "Esperanto", "Estonian", "Basque", "Faroese", "French",
    "Frisian", "Irish", "Gaelic", "Galician", "Icelandic", "Italian", "Sami", "Latin",
    "Latvian", "Luxembourgian", "Lithuanian", "Hungarian", "Maltese", "Dutch", "Norwegian", "Occitan",
    "Polish", "Portuguese", "Romanian", "Romansh", "Serbian", "Slovak", "Slovene", "Finnish",
    "Swedish", "Turkish", "Flemish", "Walloon"
};

const char *languagesOther[0x40] = {
    "Background sound/clean feed", "", "", "", "", "Zulu", "Vietnamese", "Uzbek",
    "Urdu", "Ukranian", "Thai", "Telugu", "Tatar", "Tamil", "Tadzhik", "Swahili",
    "Sranan Tongo", "Somali", "Sinhalese", "Shona", "Serbo-Croat", "Rusyn", "Russian", "Quechua",
    "Pushtu", "Punjabi", "Persian", "Papiamento", "Oriya", "Nepali", "Ndebele", "Marathi",
    "Moldavian", "Malaysian", "Malagasay", "Macedonian", "Laotian", "Korean", "Khmer", "Kazakh",
    "Kannada", "Japanese", "Indonesian", "Hindi", "Hebrew", "Hausa", "Gurani", "Gujurati",
    "Greek", "Georgian", "Fulani", "Dari", "Chuvash", "Chinese", "Burmese", "Bulgarian",
    "Bengali", "Belorussian", "Bambora", "Azerbaijani", "Assamese", "Armenian", "Arabic", "Amharic"
};

} // anonymous namespace

const int DABTables::m_timeInterleavingDelays[16] = {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};

int ProtectionProfile::informationBits() const
{
    int blocks = 0;

    for (const auto& part : parts) {
        blocks += part.blocks;
    }
    return blocks * 32;
}

int ProtectionProfile::puncturedBits() const
{
    int bits = 0;

    for (const auto& part : parts) {
        bits += part.blocks * 4 * (8 + part.index);
    }
    return bits + 12 + paddingBits;
}

const std::array<std::complex<float>, ModeI::Carriers + 1>& DABTables::phaseReference()
{
    static const std::array<std::complex<float>, ModeI::Carriers + 1> table = [] {
        std::array<std::complex<float>, ModeI::Carriers + 1> t{};

        for (int k = -ModeI::Carriers / 2; k <= ModeI::Carriers / 2; k++)
        {
            if (k == 0)
            {
                t[k + ModeI::Carriers / 2] = 0.0f;
                continue;
            }

            const PRSRow *row;
            int kPrime;

            if (k < 0)
            {
                int index = (k + 768) / 32;
                row = &prsNegative[index];
                kPrime = -768 + 32 * index;
            }
            else
            {
                int index = (k - 1) / 32;
                row = &prsPositive[index];
                kPrime = 1 + 32 * index;
            }

            float phi = (Pi / 2.0f) * static_cast<float>(prsH[row->i][(k - kPrime) & 15] + row->n);
            t[k + ModeI::Carriers / 2] = std::polar(1.0f, phi);
        }
        return t;
    }();

    return table;
}

const std::array<int16_t, ModeI::Carriers>& DABTables::frequencyInterleaving()
{
    static const std::array<int16_t, ModeI::Carriers> table = [] {
        std::array<int16_t, ModeI::Carriers> t{};
        int pi = 0;
        int n = 0;

        for (int i = 0; i < ModeI::FFTSize; i++)
        {
            if (i > 0) {
                pi = (13 * pi + 511) % ModeI::FFTSize;
            }
            if ((pi >= 256) && (pi <= 1792) && (pi != 1024)) {
                t[n++] = static_cast<int16_t>(pi - 1024);
            }
        }
        return t;
    }();

    return table;
}

const std::array<uint8_t, 32>& DABTables::puncturingVector(int index)
{
    static const std::array<std::array<uint8_t, 32>, 24> table = [] {
        std::array<std::array<uint8_t, 32>, 24> t{};

        for (int pi = 0; pi < 24; pi++)
        {
            for (int i = 0; i < 32; i++) {
                t[pi][i] = puncturingVectors[pi][i] == '1' ? 1 : 0;
            }
        }
        return t;
    }();

    if (index < 1) {
        index = 1;
    } else if (index > 24) {
        index = 24;
    }
    return table[index - 1];
}

const std::array<uint8_t, 24>& DABTables::tailPuncturingVector()
{
    static const std::array<uint8_t, 24> table = {1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0};
    return table;
}

ProtectionProfile DABTables::ficProfile()
{
    ProtectionProfile profile;
    profile.parts = {{21, 16}, {3, 15}};
    return profile;
}

bool DABTables::uepTableEntry(int tableIndex, UEPTableEntry& entry)
{
    if ((tableIndex < 0) || (tableIndex >= 64)) {
        return false;
    }
    const UEPRow& row = uepRows[tableIndex];
    entry.sizeCU = row.size;
    entry.protectionLevel = row.level;
    entry.bitrate = row.bitrate;
    return true;
}

ProtectionProfile DABTables::uepProfile(int tableIndex)
{
    ProtectionProfile profile;

    if ((tableIndex < 0) || (tableIndex >= 64)) {
        return profile;
    }
    const UEPRow& row = uepRows[tableIndex];
    for (int i = 0; i < 4; i++)
    {
        if (row.l[i] > 0) {
            profile.parts.push_back({row.l[i], row.pi[i]});
        }
    }
    profile.paddingBits = row.padding;
    return profile;
}

int DABTables::eepBitrate(bool setB, int protectionLevel, int sizeCU)
{
    static const int sizesA[4] = {12, 8, 6, 4};
    static const int sizesB[4] = {27, 21, 18, 15};

    if ((protectionLevel < 0) || (protectionLevel > 3)) {
        return 0;
    }
    if (setB)
    {
        if (sizeCU % sizesB[protectionLevel]) {
            return 0;
        }
        return 32 * (sizeCU / sizesB[protectionLevel]);
    }
    else
    {
        if (sizeCU % sizesA[protectionLevel]) {
            return 0;
        }
        return 8 * (sizeCU / sizesA[protectionLevel]);
    }
}

ProtectionProfile DABTables::eepProfile(bool setB, int protectionLevel, int sizeCU)
{
    ProtectionProfile profile;
    int bitrate = eepBitrate(setB, protectionLevel, sizeCU);

    if (bitrate <= 0) {
        return profile;
    }

    if (setB)
    {
        // Table 20
        static const int pi1[4] = {10, 6, 4, 2};
        int n = bitrate / 32;
        profile.parts = {{24 * n - 3, pi1[protectionLevel]}, {3, pi1[protectionLevel] - 1}};
    }
    else
    {
        // Table 18
        int n = bitrate / 8;
        switch (protectionLevel)
        {
        case 0:
            profile.parts = {{6 * n - 3, 24}, {3, 23}};
            break;
        case 1:
            if (n == 1) {
                profile.parts = {{5, 13}, {1, 12}};
            } else {
                profile.parts = {{2 * n - 3, 14}, {4 * n + 3, 13}};
            }
            break;
        case 2:
            profile.parts = {{6 * n - 3, 8}, {3, 7}};
            break;
        default:
            profile.parts = {{4 * n - 3, 3}, {2 * n + 3, 2}};
            break;
        }
    }
    return profile;
}

const std::vector<uint8_t>& DABTables::prbs()
{
    static const std::vector<uint8_t> sequence = [] {
        // Long enough for the largest logical frame: 1824 kbit/s for 24 ms.
        const int length = 1824 * 24 + 64;
        std::vector<uint8_t> s(length);
        uint32_t reg = 0x1ff;

        for (int i = 0; i < length; i++)
        {
            uint8_t bit = static_cast<uint8_t>(((reg >> 8) ^ (reg >> 4)) & 1);
            reg = ((reg << 1) | bit) & 0x1ff;
            s[i] = bit;
        }
        return s;
    }();

    return sequence;
}

uint16_t DABTables::crc16(const uint8_t *data, std::size_t length)
{
    uint16_t crc = 0xffff;

    for (std::size_t i = 0; i < length; i++)
    {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
        }
    }
    return static_cast<uint16_t>(~crc ^ 0xffff);
}

bool DABTables::crc16Check(const uint8_t *data, std::size_t lengthWithCRC)
{
    if (lengthWithCRC < 2) {
        return false;
    }
    uint16_t crc = crc16(data, lengthWithCRC - 2);
    uint16_t received = static_cast<uint16_t>((data[lengthWithCRC - 2] << 8) | data[lengthWithCRC - 1]);
    return crc == received;
}

bool DABTables::fireCodeCheck(const uint8_t *superframe)
{
    // G(x) = x^16 + x^14 + x^13 + x^12 + x^11 + x^5 + x^3 + x^2 + x + 1, initial state zero.
    uint16_t crc = 0;

    for (int i = 2; i < 11; i++)
    {
        crc ^= static_cast<uint16_t>(superframe[i]) << 8;
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x782f) : static_cast<uint16_t>(crc << 1);
        }
    }
    uint16_t received = static_cast<uint16_t>((superframe[0] << 8) | superframe[1]);
    // An all-zero superframe has a zero Fire code: not a valid header.
    if ((received == 0) && (crc == 0))
    {
        bool allZero = true;
        for (int i = 2; i < 11; i++) {
            allZero = allZero && (superframe[i] == 0);
        }
        if (allZero) {
            return false;
        }
    }
    return crc == received;
}

uint16_t DABTables::crc16MPEG(const uint8_t *data, int startBit, int bitCount, uint16_t crc)
{
    for (int i = 0; i < bitCount; i++)
    {
        int bitIndex = startBit + i;
        int bit = (data[bitIndex >> 3] >> (7 - (bitIndex & 7))) & 1;
        int msb = (crc >> 15) & 1;
        crc = static_cast<uint16_t>(crc << 1);
        if (msb ^ bit) {
            crc ^= 0x8005;
        }
    }
    return crc;
}

uint8_t DABTables::tiiPattern(int mainId)
{
    // Table 26 lists, in ascending order, the 70 bytes with exactly four bits set.
    static const std::array<uint8_t, 70> patterns = [] {
        std::array<uint8_t, 70> t{};
        int n = 0;

        for (int v = 0; v < 256; v++)
        {
            int bits = 0;
            for (int b = 0; b < 8; b++) {
                bits += (v >> b) & 1;
            }
            if (bits == 4) {
                t[n++] = static_cast<uint8_t>(v);
            }
        }
        return t;
    }();

    if ((mainId < 0) || (mainId >= 70)) {
        return 0;
    }
    return patterns[mainId];
}

std::string DABTables::labelToUTF8(const uint8_t *data, std::size_t length, int charset)
{
    return modemdr::Text::toUTF8(data, length, charset);
}

std::string DABTables::programmeTypeName(int pty, bool northAmerica)
{
    if ((pty < 0) || (pty > 31)) {
        return "";
    }
    return northAmerica ? ptyNamesNorthAmerica[pty] : ptyNames[pty];
}

std::string DABTables::announcementTypeName(int type)
{
    static const char *names[] = {
        "Alarm", "Road traffic", "Transport", "Warning", "News", "Weather", "Event", "Special event",
        "Programme information", "Sport", "Financial"
    };

    if ((type < 0) || (type >= static_cast<int>(sizeof(names) / sizeof(names[0])))) {
        return "";
    }
    return names[type];
}

std::string DABTables::languageName(int language)
{
    if ((language >= 0) && (language < 0x2c)) {
        return languagesLatin[language];
    }
    if ((language >= 0x40) && (language < 0x80)) {
        return languagesOther[language - 0x40];
    }
    return "";
}

std::string DABTables::userApplicationName(int userApplicationType)
{
    switch (userApplicationType)
    {
    case 0x002: return "SlideShow";
    case 0x004: return "TPEG";
    case 0x007: return "SPI";
    case 0x009: return "DMB";
    case 0x00d: return "Filecasting";
    case 0x00e: return "FIS";
    case 0x44a: return "Journaline";
    default: return "";
    }
}

} // namespace modemdab
