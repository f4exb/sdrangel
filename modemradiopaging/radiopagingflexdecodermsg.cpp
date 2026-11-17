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

// Address walk and message decoding of a phase of a FLEX frame

#include "radiopagingflexdecoder.h"

#include <algorithm>

namespace {

const int wordsPerFrame = RadioPagingFlex::m_wordsPerFrame;

// Address word ranges (21 bit address words)
const uint32_t addrLong1Max = 0x008000;
const uint32_t addrShortMin = 0x008001;
const uint32_t addrShortMax = 0x1e0000;
const uint32_t addrLong3Min = 0x1e0001;
const uint32_t addrLong3Max = 0x1e8000;
const uint32_t addrLong4Min = 0x1e8001;
const uint32_t addrLong4Max = 0x1f0000;
const uint32_t addrReserved1Min = 0x1f0001;
const uint32_t addrReserved1Max = 0x1f27ff;
const uint32_t addrInfoMin = 0x1f2800;
const uint32_t addrInfoMax = 0x1f67ff;
const uint32_t addrNetworkMin = 0x1f6800;
const uint32_t addrNetworkMax = 0x1f77ff;
const uint32_t addrTempMin = 0x1f7800;
const uint32_t addrTempMax = 0x1f780f;
const uint32_t addrOperatorMin = 0x1f7810;
const uint32_t addrOperatorMax = 0x1f781f;
const uint32_t addrReserved2Min = 0x1f7820;
const uint32_t addrReserved2Max = 0x1f7ffe;
const uint32_t addrLong2Min = 0x1f7fff;
const uint32_t addrLong2Max = 0x1ffffe;
const uint32_t shortOffset = 32768;

const char bcdChars[17] = "0123456789.U -][";

enum AddressClass {
    ClassLong1,
    ClassShort,
    ClassLong2,
    ClassLong3,
    ClassLong4,
    ClassReserved,
    ClassInfo,
    ClassNetwork,
    ClassTemporary,
    ClassOperator,
    ClassUnknown
};

AddressClass classify(uint32_t aw)
{
    if ((aw >= 1) && (aw <= addrLong1Max)) return ClassLong1;
    if ((aw >= addrShortMin) && (aw <= addrShortMax)) return ClassShort;
    if ((aw >= addrLong3Min) && (aw <= addrLong3Max)) return ClassLong3;
    if ((aw >= addrLong4Min) && (aw <= addrLong4Max)) return ClassLong4;
    if ((aw >= addrReserved1Min) && (aw <= addrReserved1Max)) return ClassReserved;
    if ((aw >= addrInfoMin) && (aw <= addrInfoMax)) return ClassInfo;
    if ((aw >= addrNetworkMin) && (aw <= addrNetworkMax)) return ClassNetwork;
    if ((aw >= addrTempMin) && (aw <= addrTempMax)) return ClassTemporary;
    if ((aw >= addrOperatorMin) && (aw <= addrOperatorMax)) return ClassOperator;
    if ((aw >= addrReserved2Min) && (aw <= addrReserved2Max)) return ClassReserved;
    if ((aw >= addrLong2Min) && (aw <= addrLong2Max)) return ClassLong2;
    return ClassUnknown;
}

bool isLongClass(AddressClass c)
{
    return (c == ClassLong1) || (c == ClassLong2) || (c == ClassLong3) || (c == ClassLong4);
}

RadioPagingFlexDecoder::AddressType addressType(AddressClass c)
{
    switch (c)
    {
    case ClassShort: return RadioPagingFlexDecoder::AddressShort;
    case ClassLong1:
    case ClassLong2:
    case ClassLong3:
    case ClassLong4: return RadioPagingFlexDecoder::AddressLong;
    case ClassNetwork: return RadioPagingFlexDecoder::AddressNetwork;
    case ClassTemporary: return RadioPagingFlexDecoder::AddressTemporary;
    case ClassOperator: return RadioPagingFlexDecoder::AddressOperator;
    case ClassInfo: return RadioPagingFlexDecoder::AddressInfoService;
    case ClassReserved: return RadioPagingFlexDecoder::AddressReserved;
    case ClassUnknown: break;
    }

    return RadioPagingFlexDecoder::AddressUnknown;
}

// Capcode of the two words of a long address, 0 if the pair is not a valid set
uint64_t longCapcode(uint32_t w1, uint32_t w2)
{
    AddressClass c1 = classify(w1);
    AddressClass c2 = classify(w2);

    if ((c1 == ClassLong1) && (c2 == ClassLong2)) {
        return (uint64_t) w1 + (uint64_t) (2097151u - w2) * shortOffset + 2068480ULL;
    }
    if ((c1 == ClassLong1) && ((c2 == ClassLong3) || (c2 == ClassLong4))) {
        return (uint64_t) w1 + (uint64_t) (w2 - 1933312u) * shortOffset + 2068480ULL;
    }
    if ((c1 == ClassLong2) && ((c2 == ClassLong3) || (c2 == ClassLong4))) {
        return (uint64_t) (w1 - 2064383u) + (uint64_t) (w2 - 1867776u) * shortOffset + 2068479ULL;
    }

    return 0;
}

uint32_t groupSums(uint32_t word)
{
    return (word & 0xff) + ((word >> 8) & 0xff) + ((word >> 16) & 0x1f);
}

const char *operatorSubtype(uint32_t aw)
{
    switch (aw & 0xf)
    {
    case 0: return "all";
    case 1: return "home";
    case 2: return "roaming";
    case 3: return "ssid";
    case 4: return "time";
    case 14: return "ssid_change";
    case 15: return "system_event";
    default: return "reserved";
    }
}

const char *operatorCategory(uint32_t aw)
{
    uint32_t lsb = aw & 0xf;

    if (lsb <= 4) {
        return "system_message";
    }
    if (lsb == 14) {
        return "ssid_change";
    }
    if (lsb == 15) {
        return "system_event";
    }
    return "reserved";
}

char statusChar(uint8_t status)
{
    switch (status)
    {
    case 1: return '.';
    case 2: return 'c';
    case 3: return 'X';
    default: return '_';
    }
}

// Hex of a bit stream (0, 1, -1 lost, 2 lost fragments): 4 bits a digit, the first bit in the least significant
// bit, the last digit padded with 0. A digit with a lost bit is '?', lost fragments are '|'
std::string hexFromBits(const std::vector<int8_t>& bits)
{
    static const char hexDigits[17] = "0123456789ABCDEF";
    std::string hex;
    unsigned value = 0;
    int count = 0;
    bool lost = false;

    auto flush = [&]() {
        if (count > 0) {
            hex += lost ? '?' : hexDigits[value & 0xf];
        }
        value = 0;
        count = 0;
        lost = false;
    };

    for (int8_t bit : bits)
    {
        if (bit == 2)
        {
            flush();
            hex += '|';
            continue;
        }

        if (bit < 0) {
            lost = true;
        } else {
            value |= (unsigned) bit << count;
        }

        if (++count == 4) {
            flush();
        }
    }

    flush();
    return hex;
}

// Status of each digit of hexFromBits(bits) from the status of each bit (that of its word: . received, c corrected,
// X lost): the worst of its bits; G for a lost fragment mark
std::string hexStatusFromBits(const std::vector<int8_t>& bits, const std::string& bitStatus)
{
    std::string status;
    char worst = '.';
    int count = 0;

    auto rank = [](char c) { return (c == 'X') ? 2 : (c == 'c') ? 1 : 0; };
    auto flush = [&]() {
        if (count > 0) {
            status += worst;
        }
        worst = '.';
        count = 0;
    };

    for (size_t i = 0; i < bits.size(); i++)
    {
        if (bits[i] == 2)
        {
            flush();
            status += 'G';
            continue;
        }

        // A bit without a status (none given) is received; a lost bit is lost whatever its word says
        char s = (bits[i] < 0) ? 'X' : (i < bitStatus.size()) ? bitStatus[i] : '.';

        if ((s != '.') && (s != 'c')) {
            s = 'X';
        }
        if (rank(s) > rank(worst)) {
            worst = s;
        }
        if (++count == 4) {
            flush();
        }
    }

    flush();
    return status;
}

// Number of data bits in a stream, without the lost fragment marks
int dataBitCount(const std::vector<int8_t>& bits)
{
    return (int) std::count_if(bits.begin(), bits.end(), [](int8_t b) { return b != 2; });
}

// Length in bits of the data of a HEX/binary or secure binary fragment of bits.size() bits (21 a word, a bit
// of a lost word is -1). The data end on a character boundary of blocking bits (STD-43A 3.10.1.2 p.61-62):
//   - data end in the middle of the last word: the rest of the word is the inverse of the last data bit;
//   - data end exactly at the end of the last word: nothing is added, except in the last fragment when the
//     last character is all 0s or all 1s: then a word of the inverse of the last data bit is added.
// The length is a multiple of blocking. Design rule: a fragment that is not the last is packed full (the
// transmitter sizes it so), so its fill is shorter than a character and it has no padding word; only the last
// fragment can have a longer fill, or the padding word that tells data from fill at an exact word end.
// state: 0 follows the rules, 1 does not, 2 can not be told (lost bits at the end, blocking unknown = 0)
int terminateBits(const std::vector<int8_t>& bits, int blocking, bool lastFragment, int& state)
{
    const int wordBits = 21;
    int n = (int) bits.size();

    state = 0;

    if (n <= 0) {
        return 0;
    }
    if (blocking <= 0)
    {
        state = 2;
        return n;
    }

    const int8_t *lw = &bits[n - wordBits];

    for (int i = 0; i < wordBits; i++)
    {
        if (lw[i] < 0)
        {
            state = 2;
            return n;
        }
    }

    // A fragment that is not the last is packed full: no fill and no padding word, the only bits after
    // its last character are the ones too few for another character (fewer than blocking). Their value
    // tells nothing (they may equal the last data bits): the data are the whole characters
    if (!lastFragment) {
        return n - (n % blocking);
    }

    // Rule (3): an extra word of the inverse of the last data bit
    if ((n >= 2 * wordBits) && (((n - wordBits) % blocking) == 0) && (n - wordBits >= blocking))
    {
        bool uniform = true;

        for (int i = 1; i < wordBits; i++)
        {
            if (lw[i] != lw[0])
            {
                uniform = false;
                break;
            }
        }

        if (uniform)
        {
            bool inverse = true;

            for (int i = 1; i <= blocking; i++)
            {
                int8_t b = bits[n - wordBits - i];

                if (b < 0)
                {
                    state = 2;
                    return n;
                }
                if (b == lw[0]) {
                    inverse = false;
                }
            }

            if (inverse) {
                return n - wordBits;
            }
        }
    }

    // Rule (2): fill in the last word, the run of equal bits at its end
    int run = 0;

    while ((run < wordBits) && (lw[wordBits - 1 - run] == lw[wordBits - 1])) {
        run++;
    }

    if (run == wordBits)
    {
        // A whole word of one value is not fill (fill is shorter); the bits after the last whole character are
        // too few to be data
        return n - (n % blocking);
    }
    if (run >= blocking)
    {
        // Last fragment: fill of blocking bits or more after the last character
        if (((n - run) % blocking) == 0) {
            return n - run;
        }
        state = 1;
        return n;
    }
    // Fewer than blocking equal bits at the end: the fill is mandatory only when the rest of the word could be
    // mistaken for data, that is when it could hold another character. The bits after the last whole character
    // are too few to be data, whether they are fill or not
    return n - (n % blocking);
}

// Signature sum of HEX/binary data (3.10.1.2 p.61): groups of 8 bits, the first bit of a group the least
// significant, a last group with fewer bits padded with 0. lost: a bit is lost or a fragment is missing
uint32_t hexSignatureSum(const std::vector<int8_t>& bits, bool& lost)
{
    uint32_t sum = 0;
    uint32_t acc = 0;

    lost = false;

    for (size_t i = 0; i < bits.size(); i++)
    {
        if ((bits[i] < 0) || (bits[i] == 2)) {
            lost = true;
        } else {
            acc |= (uint32_t) bits[i] << (i % 8);
        }

        if ((i % 8) == 7)
        {
            sum += acc;
            acc = 0;
        }
    }

    if ((bits.size() % 8) != 0) {
        sum += acc;
    }

    return sum;
}

} // namespace

RadioPagingFlexDecoder::Check RadioPagingFlexDecoder::combineChecks(Check a, Check b)
{
    if ((a == CheckFailed) || (b == CheckFailed)) {
        return CheckFailed;
    }
    if ((a == CheckIncomplete) || (b == CheckIncomplete)) {
        return CheckIncomplete;
    }
    if ((a == CheckOk) || (b == CheckOk)) {
        return CheckOk;
    }
    return CheckNone;
}

// Walk the address field. Each address has a vector in the vector field in the same order (one word for
// a short address, two for a long address). Addresses at the end of the address field that have no
// valid vector (tone only addresses) are not decoded
void RadioPagingFlexDecoder::walkAddresses(unsigned addressStart, unsigned vectorStart, unsigned priorityWords)
{
    BiwState& biw = pol().m_biw;
    int addrIdx = (int) addressStart;
    int vecCount = 0;
    int priorityEnd = (int) (addressStart + priorityWords);

    // Number of vector slots up to the last one with a valid checksum
    int maxVec = (int) (vectorStart - addressStart);

    if (biw.m_sysMsgType >= 0) {
        maxVec++;
    }
    if (maxVec > wordsPerFrame - (int) vectorStart) {
        maxVec = wordsPerFrame - (int) vectorStart;
    }

    int lastValid = -1;

    for (int vi = 0; vi < maxVec; vi++)
    {
        int wi = (int) vectorStart + vi;

        if ((m_w[wi].m_status == WordUncorrectable) || (m_w[wi].m_status == WordNotReceived)) {
            continue;
        }
        if (RadioPagingFlex::checksumValid(m_w[wi].m_data)) {
            lastValid = vi;
        }
    }

    int validVectors = lastValid + 1;
    int vectorBound = wordsPerFrame;

    for (; addrIdx < (int) vectorStart; addrIdx++)
    {
        int j = (int) vectorStart + vecCount;
        AddressContext a;

        a.m_priority = (addrIdx < priorityEnd);
        a.m_index = addrIdx;

        if (j >= wordsPerFrame) {
            break;
        }

        if ((m_w[addrIdx].m_status == WordUncorrectable) || (m_w[addrIdx].m_status == WordNotReceived))
        {
            vecCount++;
            continue;
        }

        uint32_t aw = m_w[addrIdx].m_data;
        AddressClass cls = classify(aw);

        // Not an address (all zeros, all ones): no message, it keeps its vector slot
        if (cls == ClassUnknown)
        {
            vecCount++;
            continue;
        }

        a.m_word = aw;
        a.m_type = addressType(cls);
        a.m_long = isLongClass(cls);

        if (cls == ClassTemporary) {
            a.m_tempSlot = (int) ((aw - addrTempMin) & 0xf);
        }

        if (a.m_long)
        {
            if (addrIdx + 1 >= (int) vectorStart) {
                break;
            }

            if ((m_w[addrIdx + 1].m_status == WordUncorrectable) || (m_w[addrIdx + 1].m_status == WordNotReceived))
            {
                addrIdx++;
                vecCount += 2;
                continue;
            }

            a.m_capcode = longCapcode(aw, m_w[addrIdx + 1].m_data);
            addrIdx++;

            if (a.m_capcode == 0)
            {
                vecCount += 2;
                continue;
            }
        }
        else
        {
            a.m_capcode = (uint64_t) aw - shortOffset;
        }

        // No valid vector left: a tone only address at the end of the address field. The standard allows it (an
        // address limited to tone only operation needs no vector), but the words that end an address field of noise
        // or of a damaged frame look the same, and pagers take tone only as a short message with the tone only
        // payload (Short Message / Tone Only vector, every digit a space). Such an address is not decoded
        if ((vecCount >= validVectors) || (j >= vectorBound)) {
            continue;
        }

        vecCount += a.m_long ? 2 : 1;

        if ((m_w[j].m_status == WordUncorrectable) || (m_w[j].m_status == WordNotReceived)) {
            continue;
        }

        // The message field starts where the vector field ends: the lowest message start of the vectors
        // seen so far. Vector slots from there on are message words
        if (RadioPagingFlex::checksumValid(m_w[j].m_data))
        {
            unsigned type = (m_w[j].m_data >> 4) & 7;
            int start = (int) ((m_w[j].m_data >> 7) & 0x7f);

            if ((type != 1) && (type != 2) && (start > j + (a.m_long ? 1 : 0))) {
                vectorBound = std::min(vectorBound, start);
            }
        }

        if (a.m_long && ((j + 1 >= wordsPerFrame) || (m_w[j + 1].m_status == WordUncorrectable)
            || (m_w[j + 1].m_status == WordNotReceived))) {
            continue;
        }

        // A vector word whose checksum fails (STD-43A 3.5.1) is in error, as the BCH code may correct a word wrong
        // (an all zero word passes the code): what it says is not read
        if (!RadioPagingFlex::checksumValid(m_w[j].m_data))
        {
            m_status.m_invalidVectors++;
            continue;
        }

        processVector(a, j);
    }

    // System message announced by a block information word: its vector is the last vector word
    if (biw.m_sysMsgType >= 0)
    {
        int type = biw.m_sysMsgType;
        int index = (int) vectorStart + validVectors - 1;

        if ((index > (int) vectorStart + vecCount - 1) && (index < wordsPerFrame)
            && (m_w[index].m_status != WordUncorrectable) && (m_w[index].m_status != WordNotReceived))
        {
            AddressContext a;

            a.m_word = addrOperatorMin + (uint32_t) type;
            a.m_capcode = (uint64_t) a.m_word - shortOffset;
            a.m_type = AddressOperator;
            a.m_index = -1;
            biw.m_sysMsgType = -1;
            processVector(a, index);
        }
    }
}

void RadioPagingFlexDecoder::handleTemporaryAddress(const AddressContext& a, int j, int vecType, int mw1, Message& proto)
{
    TempSlot& t = pol().m_temp[a.m_tempSlot];
    bool complete = true;
    bool initial = true;
    int number = -1;

    // Alphanumeric and secure messages may be in fragments: C continued, F 3 the first, N the message number
    if ((vecType == 5) || (vecType == 0))
    {
        int hi = a.m_long ? (j + 1) : mw1;

        if ((hi <= 87) && wordOk(hi))
        {
            const uint32_t hdr = m_w[hi].m_data;
            complete = ((hdr >> 10) & 1) == 0;
            initial = ((hdr >> 11) & 3) == 3;
            number = (int) ((hdr >> 13) & 0x3f);
        }
    }

    proto.m_tempSlot = a.m_tempSlot;

    // A fragment after the first: the members of its first fragment; without it, no members are guessed
    if (!initial)
    {
        int slot = reassemblyFind(a.m_capcode, number);

        if (slot >= 0)
        {
            const Message& first = pol().m_reassembly[slot].m_message;
            proto.m_groupTracked = first.m_groupTracked;
            proto.m_groupMembers = first.m_groupMembers;
            proto.m_groupGeneration = first.m_groupGeneration;

            // The group goes with the last fragment of its message, unless a new group took the slot
            if (t.m_active && (t.m_generation == first.m_groupGeneration))
            {
                t.m_lastFrame = absFrame();

                if (complete)
                {
                    setTempOutcome(a.m_tempSlot, GroupDelivered, t);
                    t = TempSlot();
                }
            }
        }

        return;
    }

    if (t.m_active)
    {
        proto.m_groupTracked = true;
        proto.m_groupMembers = t.m_members;
        proto.m_groupGeneration = t.m_generation;

        if (complete)
        {
            setTempOutcome(a.m_tempSlot, GroupDelivered, t);
            t = TempSlot();
        }
        else
        {
            t.m_delivering = true;
            t.m_lastFrame = absFrame();
        }
    }
}

// Decode the vector at word j and the message it points to
void RadioPagingFlexDecoder::processVector(const AddressContext& a, int j)
{
    uint32_t viw = m_w[j].m_data;
    int vecType = (viw >> 4) & 7;
    int mw1 = (viw >> 7) & 0x7f;
    int length;

    // Numeric vectors have a 3 bit length (words - 1), the others a 7 bit length
    if ((vecType == 3) || (vecType == 4) || (vecType == 7)) {
        length = (int) ((viw >> 14) & 7) + 1;
    } else {
        length = (int) ((viw >> 14) & 0x7f);
    }

    int mw2 = mw1 + (length - 1);

    // The first message word of a long address is in the second vector word
    if (a.m_long && (length > 0) && (vecType != 1) && (vecType != 2)) {
        mw2--;
    }

    Message proto = newMessage(a);

    if (a.m_index >= 0) {
        countWords(proto, a.m_index, a.m_index + (a.m_long ? 1 : 0));
    }
    countWords(proto, j, j + (a.m_long ? 1 : 0));

    if (vecType == 1)
    {
        decodeShortInstruction(proto, a, j);
        return;
    }

    // A short message vector has no message field: bits 7-20 are its data, not a start and a length
    if (vecType == 2)
    {
        if (a.m_type == AddressReserved) {
            return;
        }
        if (a.m_type == AddressTemporary) {
            handleTemporaryAddress(a, j, vecType, mw1, proto);
        }
        decodeShortMessage(proto, a, j);
        return;
    }

    if ((mw1 == 0) && (mw2 == 0)) {
        return;
    }

    // Network address with a message vector: a system message of the NID, decoded as any other message
    // (FLEX-93001 3.9.2 p.44)
    if (a.m_type == AddressNetwork) {
        proto.m_nidSystemMessage = true;
    }

    // Operator messaging address: the message is decoded as any other message
    if (a.m_type == AddressOperator)
    {
        proto.m_operatorSubtype = operatorSubtype(a.m_word);
        proto.m_operatorCategory = operatorCategory(a.m_word);
        proto.m_operatorPayload = ((mw1 <= 87) && wordOk(mw1)) ? (int64_t) m_w[mw1].m_data : -1;
    }

    if (a.m_type == AddressReserved) {
        return;
    }

    if (a.m_type == AddressTemporary) {
        handleTemporaryAddress(a, j, vecType, mw1, proto);
    }

    // A message vector of no words: the message has none, not even its header (the words at its start are those of
    // another message). The vector is all there is: no header nor data are read
    if ((length == 0) && ((vecType == 0) || (vecType == 5) || (vecType == 6)))
    {
        std::vector<uint32_t> raw(1, viw);

        if (a.m_long && (j + 1 < wordsPerFrame)) {
            raw.push_back(m_w[j + 1].m_data);
        }

        proto.m_type = (vecType == 0) ? TypeSecure : (vecType == 5) ? TypeAlphanumeric : TypeBinary;
        proto.m_noWords = true;
        setRaw(proto, raw);
        emitMessage(proto);
        return;
    }

    switch (vecType)
    {
    case 5:
    case 0:
        decodeAlpha(proto, a, j, vecType, mw1, mw2);
        break;
    case 3:
    case 4:
    case 7:
        decodeNumeric(proto, a, j, vecType, mw1, mw2);
        break;
    case 6:
        decodeBinary(proto, a, j, mw1, mw2);
        break;
    default:
        break;
    }
}

// Short instruction: 14 data bits in the vector. Type 0 assigns a temporary address to the pager of the
// address word, type 1 announces a system event
void RadioPagingFlexDecoder::decodeShortInstruction(Message& proto, const AddressContext& a, int j)
{
    uint32_t data = (m_w[j].m_data >> 7) & 0x3fff;
    unsigned type = data & 7;

    proto.m_type = TypeShortInstruction;
    proto.m_instructionType = (int) type;
    proto.m_instructionData = data;
    setRaw(proto, std::vector<uint32_t>(1, m_w[j].m_data));

    if (type == 0)
    {
        unsigned slot = (data >> 10) & 0xf;
        unsigned target = (data >> 3) & 0x7f;
        TempSlot& t = pol().m_temp[slot];

        // A group: the assignments of the slot to the same frame until its group message. An assignment to another
        // frame, or after the group message started, is a new group (the fragments in flight keep the old members)
        if (!t.m_active || t.m_delivering || (t.m_targetFrame != target))
        {
            // A group waiting for its message, given another frame: the first one is discarded
            if (t.m_active && !t.m_delivering) {
                setTempOutcome((int) slot, GroupDiscarded, t);
            }

            t = TempSlot();
            t.m_targetFrame = target;
            t.m_setupFrame = m_ctx.m_frame;
            t.m_setupCycle = m_ctx.m_cycle;
            t.m_active = true;
            t.m_generation = ++pol().m_tempGeneration;
        }

        t.m_lastFrame = absFrame();

        if (std::find(t.m_members.begin(), t.m_members.end(), a.m_capcode) == t.m_members.end()
            && ((int) t.m_members.size() < m_maxGroupMembers)) {
            t.m_members.push_back(a.m_capcode);
        }

        proto.m_instructionSlot = (int) slot;
        proto.m_instructionFrame = (int) target;
        proto.m_groupTracked = true;
        proto.m_groupMembers = t.m_members;
        proto.m_groupGeneration = t.m_generation;
    }
    else if ((type == 1) && (a.m_type == AddressOperator))
    {
        proto.m_eventFlags = (data >> 3) & 0x7ff;
        proto.m_operatorSubtype = operatorSubtype(a.m_word);
        proto.m_operatorCategory = operatorCategory(a.m_word);
    }

    emitMessage(proto);
}

// Short message: the message is in the vector word (two words for a long address)
void RadioPagingFlexDecoder::decodeShortMessage(Message& proto, const AddressContext& a, int j)
{
    uint32_t viw = m_w[j].m_data;
    uint32_t t = (viw >> 7) & 3;
    uint32_t d = (viw >> 9) & 0xfff;
    std::vector<uint32_t> raw(1, viw);

    proto.m_type = TypeShortMessage;
    proto.m_shortType = (int) t;

    // The second vector word of a long address carries d12-d32 (digits, or bits not used / idle)
    const bool second = a.m_long && (j + 1 < wordsPerFrame);
    const uint32_t v2 = second ? m_w[j + 1].m_data : 0;

    if (second) {
        raw.push_back(v2);
    }

    if ((t == 0) && (a.m_type == AddressNetwork))
    {
        // Network ID: F3-F0 M2-M0 A4-A0 (STD-43A Table 3.9.2-1 (1), Table 6.1.2-1)
        proto.m_networkId = true;
        proto.m_nidServiceArea = (int) (d & 0x1f);
        proto.m_nidMultiplier = (int) ((d >> 5) & 7);
        proto.m_nidTrafficFlags = (int) ((d >> 8) & 0xf);
    }
    else if (t == 0)
    {
        // Digits a b c in d0-d11, d to h in d12-d31 of a long address; unused digits are spaces
        std::string digits;

        digits += bcdChars[d & 0xf];
        digits += bcdChars[(d >> 4) & 0xf];
        digits += bcdChars[(d >> 8) & 0xf];

        if (second)
        {
            digits += bcdChars[v2 & 0xf];
            digits += bcdChars[(v2 >> 4) & 0xf];
            digits += bcdChars[(v2 >> 8) & 0xf];
            digits += bcdChars[(v2 >> 12) & 0xf];
            digits += bcdChars[(v2 >> 16) & 0xf];
        }

        proto.m_text = digits;
        proto.m_toneOnly = (digits.find_first_not_of(' ') == std::string::npos);

        // The tone only payload: a tone only page (Short Message / Tone Only vector, STD-43A Table 3.9.2-1)
        if (proto.m_toneOnly)
        {
            proto.m_type = TypeTone;
            proto.m_text.clear();
        }
    }
    else if (t == 1)
    {
        // Source S2-S0 in d0-d2, the other bits are not used
        proto.m_shortSource = (int) (d & 7);
    }
    else if (t == 2)
    {
        // Source S2-S0 in d0-d2, message number N5-N0 in d3-d8, retrieval flag R0 in d9
        proto.m_shortSource = (int) (d & 7);
        proto.m_messageNumber = (int) ((d >> 3) & 0x3f);
        proto.m_retrieval = ((d >> 9) & 1) != 0;
        checkSequence(proto, a);
    }

    setRaw(proto, raw);
    emitMessage(proto);
}

// Reassembly of fragments of alphanumeric, secure and binary messages
void RadioPagingFlexDecoder::handleFragment(Message& msg, const AddressContext& a, int vector, const FragmentInfo& info)
{
    // Only an alphanumeric message has a signature field (3.10.1.3); a secure message has none (3.10.1.4)
    const bool signatureUsed = (vector == 5);
    // HEX/binary and secure binary data are joined as one bit string over the fragments
    const bool secureHex = (msg.m_hexNibbleBits != 0);

    setRaw(msg, info.m_raw);

    switch (msg.m_fragment)
    {
    case FragmentUnknown:
        msg.m_complete = false;
        emitMessage(msg);
        return;

    case FragmentComplete:
        emitMessage(msg);
        return;

    case FragmentFirst:
    {
        reassemblyInvalidate(a.m_capcode, msg.m_messageNumber);

        int old = reassemblyFind(a.m_capcode, msg.m_messageNumber);

        if (old >= 0)
        {
            emitIncomplete(pol().m_reassembly[old]);
            pol().m_reassembly[old].m_active = false;
        }

        int index = reassemblyAlloc(a.m_capcode, msg.m_messageNumber, vector);
        Reassembly& slot = pol().m_reassembly[index];

        slot.m_signature = info.m_signature;
        slot.m_signatureSum = info.m_signatureSum;
        slot.m_signatureValid = info.m_wordsOk;
        slot.m_text = msg.m_text;
        slot.m_charStatus = msg.m_charStatus;
        slot.m_raw = info.m_raw;
        slot.m_bits = info.m_bits;
        slot.m_bitStatus = info.m_bitStatus;
        slot.m_fragments = 1;
        slot.m_expected = 0;
        slot.m_message = msg;
        slot.m_message.m_fragmented = true;
        slot.m_message.m_isFragment = false;
        slot.m_message.m_complete = false;

        if (m_reportFragments)
        {
            Message fragment = msg;

            fragment.m_fragmented = true;
            fragment.m_isFragment = true;
            emitMessage(fragment);
        }
        return;
    }

    case FragmentMiddle:
    case FragmentLast:
    {
        int index = reassemblyFind(a.m_capcode, msg.m_messageNumber);

        if (index < 0)
        {
            // The first fragment was not received
            msg.m_fragmented = true;
            msg.m_complete = false;
            emitMessage(msg);
            return;
        }

        Reassembly& slot = pol().m_reassembly[index];

        if (msg.m_fragmentNumber != slot.m_expected)
        {
            slot.m_gap = true;

            if (secureHex)
            {
                slot.m_bits.push_back(2);
                slot.m_bitStatus.push_back('G');
            }
            else
            {
                appendGap(slot);
            }
        }

        if (secureHex)
        {
            slot.m_bits.insert(slot.m_bits.end(), info.m_bits.begin(), info.m_bits.end());
            slot.m_bitStatus += info.m_bitStatus;
        }

        slot.m_signatureSum += info.m_signatureSum;

        if (!info.m_wordsOk) {
            slot.m_signatureValid = false;
        }

        // The hex of a secure message is made of the joined bit stream: a fragment need not end on a digit
        slot.m_text = secureHex ? hexFromBits(slot.m_bits) : (slot.m_text + msg.m_text);
        slot.m_charStatus = secureHex ? hexStatusFromBits(slot.m_bits, slot.m_bitStatus) : (slot.m_charStatus + msg.m_charStatus);
        slot.m_raw.insert(slot.m_raw.end(), info.m_raw.begin(), info.m_raw.end());
        slot.m_fragments++;
        slot.m_lastFrame = m_ctx.m_frame;
        slot.m_lastCycle = m_ctx.m_cycle;
        slot.m_expected = (msg.m_fragmentNumber + 1) % 3;

        Message& first = slot.m_message;

        first.m_cycle = m_ctx.m_cycle;
        first.m_frame = m_ctx.m_frame;
        first.m_wordsCorrected += msg.m_wordsCorrected;
        first.m_bitErrors += msg.m_bitErrors;
        first.m_wordsUncorrectable += msg.m_wordsUncorrectable;
        first.m_parityCorrections += msg.m_parityCorrections;
        first.m_dataBits += msg.m_dataBits;
        first.m_checksum = combineChecks(first.m_checksum, msg.m_checksum);
        first.m_recovered = first.m_recovered || msg.m_recovered;
        first.m_wordMap += "|" + msg.m_wordMap;

        if (msg.m_fragment == FragmentMiddle)
        {
            if (m_reportFragments)
            {
                Message fragment = msg;

                fragment.m_fragmented = true;
                fragment.m_isFragment = true;
                fragment.m_fragmentCount = slot.m_fragments;    // The how many-th fragment received
                emitMessage(fragment);
            }
            return;
        }

        Message out = first;

        out.m_text = slot.m_text;
        out.m_charStatus = slot.m_charStatus;
        out.m_fragment = FragmentLast;
        out.m_fragmentNumber = msg.m_fragmentNumber;
        out.m_fragmentCount = slot.m_fragments;
        out.m_fragmentGap = slot.m_gap;
        out.m_complete = !slot.m_gap;
        out.m_isFragment = false;

        if (signatureUsed)
        {
            if (slot.m_signatureValid) {
                out.m_signature = (slot.m_signature == ((~slot.m_signatureSum) & 0x7f)) ? CheckOk : CheckFailed;
            } else {
                out.m_signature = CheckIncomplete;
            }
        }
        else if ((vector == 6) && secureHex)
        {
            // HEX/binary: the signature (8 bit sums) covers the data bits of all fragments
            bool lost = false;
            uint32_t sum = hexSignatureSum(slot.m_bits, lost);

            if (!slot.m_signatureValid || slot.m_gap || lost) {
                out.m_signature = CheckIncomplete;
            } else {
                out.m_signature = (slot.m_signature == ((~sum) & 0xff)) ? CheckOk : CheckFailed;
            }
            out.m_fillState = std::max(out.m_fillState, msg.m_fillState);
        }
        else
        {
            out.m_signature = CheckNone;
        }

        if (secureHex) {
            out.m_dataBits = dataBitCount(slot.m_bits);
        }

        setRaw(out, slot.m_raw);
        slot.m_active = false;

        if (m_reportFragments)
        {
            Message fragment = msg;

            fragment.m_fragmented = true;
            fragment.m_isFragment = true;
            fragment.m_fragmentCount = slot.m_fragments;        // The how many-th fragment received
            emitMessage(fragment);
        }

        emitMessage(out);
        return;
    }
    }
}

// Alphanumeric (vector 5) and secure (vector 0) messages. Header word: K (10 bits), C continued, F fragment
// number, N message number, and R, M (first fragment) or the secure type t. The first character slot of the first
// word after the header is the signature
void RadioPagingFlexDecoder::decodeAlpha(Message& proto, const AddressContext& a, int j, int vecType, int mw1, int mw2)
{
    if ((mw1 > 87) || (mw2 > 87))
    {
        m_status.m_invalidVectors++;
        return;
    }

    const bool secure = (vecType == 0);
    int hdrIdx = a.m_long ? (j + 1) : mw1;
    bool hdrOk = (hdrIdx < wordsPerFrame) && wordOk(hdrIdx);
    uint32_t hdr = 0;
    uint32_t hdrK = 0;
    int hdrC = 0;
    int hdrF = 0;
    int hdrN = 0;
    int hdrR = 0;
    int hdrM = 0;
    int hdrU = 0;
    int hdrV = 0;
    bool hasFragControl = false;
    int secureType = -1;
    FragmentKind kind = FragmentUnknown;

    if (hdrOk)
    {
        hdr = m_w[hdrIdx].m_data;
        hdrK = hdr & 0x3ff;
        hdrC = (hdr >> 10) & 1;
        hdrF = (hdr >> 11) & 3;
        hdrN = (hdr >> 13) & 0x3f;

        if (hdrC == 0) {
            kind = (hdrF == 3) ? FragmentComplete : FragmentLast;
        } else {
            kind = (hdrF == 3) ? FragmentFirst : FragmentMiddle;
        }

        if (secure)
        {
            secureType = (hdr >> 19) & 3;
        }
        else if (hdrF == 3)
        {
            hdrR = (hdr >> 19) & 1;
            hdrM = (hdr >> 20) & 1;
        }
        else
        {
            // Fragment control bits U0 V0 of the second and later fragments (3.10.1.3): 00 enhanced
            // fragmentation not supported, 01 reserved, 10 starts in the default character mode, 11 in the
            // alternative character mode
            hdrU = (int) ((hdr >> 19) & 1);
            hdrV = (int) ((hdr >> 20) & 1);
            hasFragControl = true;
        }
    }

    // Retransmission of a message with a message number
    bool recovered = false;
    bool duplicate = false;
    int phaseIndex = m_name - 'A';

    if ((hdrF == 3) && !secure && hdrOk)
    {
        int count = std::max(0, mw2 - mw1 + 1);
        int result = checkRetransmission(a.m_capcode, hdrR != 0, hdrN, vecType, phaseIndex, &m_w[mw1], count,
            (hdrC == 0) ? 1 : 0, a.m_long, 0);

        // A retransmission is output again, marked as a duplicate
        duplicate = (result != 0);

        if (result == -1)
        {
            HistoryEntry *entry = m_historyMatch;

            if (entry)
            {
                int copy = std::min(count, entry->m_wordCount);

                for (int i = 0; i < copy; i++) {
                    m_w[mw1 + i] = entry->m_words[i];
                }
            }

            recovered = true;
        }
    }

    // Characters
    const bool initial = (hdrF == 3);
    // The first character slot of the first fragment of an alphanumeric message is the signature
    // (3.10.1.3). A secure message has none: the slot is character a0 (Fig. 3.10.1.4-1)
    const bool sigSlot = initial && !secure;
    // Header lost: the fragment is not known, so neither is the first slot. It is the signature of a message
    // in one piece if it is the signature of the characters received; otherwise it is shown as lost
    const bool sigUnknown = !hdrOk && !secure;
    int unknownSlot = -1;
    int startWord = a.m_long ? mw1 : (mw1 + 1);

    std::string text;
    std::string charStatus;         // One for each character of text
    std::vector<uint8_t> chars;     // 7-bit characters of the fragment as sent, signature slot excluded
    uint32_t signature = 0;
    uint32_t sum = 0;
    bool wordsOk = true;
    bool nulError = false;

    {
        for (int w = startWord; (w <= mw2) && (w < wordsPerFrame); w++)
        {
            if ((m_w[w].m_status == WordUncorrectable) && m_w[w].m_hasReceived)
            {
                // Received but not corrected: its characters as received, marked 'u' (the signature slot and
                // the slot of an unknown first character are not shown, as for a lost word)
                wordsOk = false;

                for (int slot = ((w == startWord) && sigSlot) ? 1 : 0; slot < 3; slot++)
                {
                    text += (char) ((m_w[w].m_received >> (7 * slot)) & 0x7f);
                    charStatus += 'u';
                    chars.push_back(0xff);  // Neither NUL nor ETX
                }
                continue;
            }
            if ((m_w[w].m_status == WordUncorrectable) || (m_w[w].m_status == WordNotReceived))
            {
                wordsOk = false;

                int lost = ((w == startWord) && sigSlot) ? 2 : 3;

                text.append(lost, '?');
                charStatus.append(lost, 'X');
                chars.insert(chars.end(), lost, 0xff);  // Neither NUL nor ETX
                continue;
            }

            uint32_t dw = m_w[w].m_data;
            const char status = (m_w[w].m_status == WordCorrected) ? 'c' : '.';

            for (int slot = 0; slot < 3; slot++)
            {
                uint32_t ch = (dw >> (7 * slot)) & 0x7f;

                if ((slot == 0) && (w == startWord) && sigSlot)
                {
                    signature = ch;
                    continue;
                }
                if ((slot == 0) && (w == startWord) && sigUnknown)
                {
                    unknownSlot = (int) ch;
                    continue;
                }

                chars.push_back((uint8_t) ch);

                // ETX and NUL are function characters: not displayed, not in the signature sum
                // (3.10.1.3, Fig. p.65)
                if (ch != 0x03) {
                    sum += ch;
                }
                if ((ch != 0x03) && (ch != 0))
                {
                    text += (char) ch;
                    charStatus += status;
                }
            }
        }

        // Enhanced Fragmentation (U0 V0 not 00, 3.10.1.3 p.66): NUL ($00) only as the reminder characters at
        // the end of a fragment; anywhere else it is a reception error
        if (hdrU || hdrV)
        {
            size_t end = chars.size();

            while ((end > 0) && ((chars[end - 1] == 0) || (chars[end - 1] == 0x03))) {
                end--;
            }
            for (size_t i = 0; i < end; i++)
            {
                if (chars[i] == 0)
                {
                    nulError = true;
                    break;
                }
            }
        }
    }

    Message msg = proto;

    bool unknownIsSignature = false;

    if (unknownSlot >= 0)
    {
        if (wordsOk && ((uint32_t) unknownSlot == ((~sum) & 0x7f))) {
            unknownIsSignature = true;
        } else {
            text.insert(text.begin(), '?');
            charStatus.insert(charStatus.begin(), 'X');
        }
    }

    msg.m_type = secure ? TypeSecure : TypeAlphanumeric;
    msg.m_text = text;
    msg.m_charStatus = charStatus;
    msg.m_fragment = kind;
    msg.m_fragmentNumber = hdrF;
    msg.m_messageNumber = hdrOk ? hdrN : -1;
    msg.m_retrieval = (hdrR != 0);
    msg.m_mailDrop = (hdrM != 0);
    msg.m_secureType = secureType;
    msg.m_recovered = recovered;
    msg.m_duplicate = duplicate;
    msg.m_fragmented = (kind == FragmentFirst) || (kind == FragmentMiddle) || (kind == FragmentLast);
    msg.m_fragmentControl = hasFragControl ? ((hdrU << 1) | hdrV) : -1;
    msg.m_nulError = nulError;

    // R is in the first fragment: the message takes its place in the sequence of the address there
    if (!secure && initial) {
        checkSequence(msg, a);
    }

    // Signature of the whole message in the first fragment. Checked here for a message in one piece
    if (sigSlot)
    {
        if (hdrC == 0) {
            msg.m_signature = !wordsOk ? CheckIncomplete : (signature == ((~sum) & 0x7f)) ? CheckOk : CheckFailed;
        }
    }
    else if (unknownIsSignature)
    {
        msg.m_signature = CheckOk;
    }

    // K: checksum of the words of this fragment (the K field of the header counts as zero)
    if (hdrOk)
    {
        uint32_t kSum = 0;
        bool allOk = true;

        if (a.m_long) {
            kSum += groupSums(hdr & ~0x3ffu);
        }

        for (int w = mw1; (w <= mw2) && (w < wordsPerFrame); w++)
        {
            if ((m_w[w].m_status == WordUncorrectable) || (m_w[w].m_status == WordNotReceived))
            {
                allOk = false;
                break;
            }

            uint32_t dw = m_w[w].m_data;

            if (!a.m_long && (w == mw1)) {
                dw &= ~0x3ffu;
            }

            kSum += groupSums(dw);
        }

        // K is ok or not: words of the fragment lost, it is not ok
        msg.m_checksum = (allOk && (hdrK == ((~kSum) & 0x3ff))) ? CheckOk : CheckFailed;
    }
    else
    {
        msg.m_checksum = CheckFailed;   // The header with K lost
    }

    // Registration acknowledgment: secure message with operation code "="
    if (secure && (secureType == 0) && !text.empty() && (text[0] == '=')) {
        msg.m_registrationAck = true;
    }

    // Secure messages other than t = 00 are not characters (STD-43A 3.10.1.4 p.70): 10 is binary data, 01 data
    // defined separately, 11 reserved. Their data bits after the header are output as hex. The last fragment of
    // binary data ends with fill bits, the inverse of the last data bit, or with a termination word of them; the
    // termination of 01 is not defined. Type 01 on a network address is the System NID Change Instruction
    // (FLEX-93001 3.9.7 p.48)
    std::vector<int8_t> secureBits;
    std::string secureBitStatus;

    if (secure && hdrOk && (secureType != 0))
    {
        for (int w = startWord; (w <= mw2) && (w < wordsPerFrame); w++)
        {
            bool lost = (m_w[w].m_status == WordUncorrectable) || (m_w[w].m_status == WordNotReceived);

            for (int b = 0; b < 21; b++) {
                secureBits.push_back(lost ? -1 : (int8_t) ((m_w[w].m_data >> b) & 1));
            }

            secureBitStatus.append(21, lost ? 'X' : statusChar(m_w[w].m_status));
        }

        // Binary data (t1t0 = 10): the termination of 3.10.1.4 p.70, characters of 1 bit. The termination of
        // t1t0 = 01 is not defined
        int secureFill = 0;

        if (secureType == 2) {
            secureBits.resize(terminateBits(secureBits, 1, hdrC == 0, secureFill));
        }

        msg.m_fillState = secureFill;

        secureBitStatus.resize(secureBits.size());
        msg.m_text = hexFromBits(secureBits);
        // The digits coloured as the characters are: the status of the words of their bits
        msg.m_charStatus = hexStatusFromBits(secureBits, secureBitStatus);
        msg.m_hexNibbleBits = 4;
        msg.m_hexLsbFirst = true;
        msg.m_dataBits = dataBitCount(secureBits);
        msg.m_nidChange = (secureType == 1) && (a.m_type == AddressNetwork);
        msg.m_payloadUnknown = (secureType != 2);
        msg.m_signature = CheckNone;
    }

    // Message words: for a long address the first one is the second vector word
    FragmentInfo info;

    if (a.m_long)
    {
        info.m_raw.push_back(m_w[j + 1].m_data);
        msg.m_wordMap += statusChar(m_w[j + 1].m_status);
    }

    for (int w = mw1; (w <= mw2) && (w < wordsPerFrame); w++)
    {
        info.m_raw.push_back(m_w[w].m_data);
        msg.m_wordMap += statusChar(m_w[w].m_status);
    }

    countWords(msg, mw1, mw2);
    info.m_signature = signature;
    info.m_signatureSum = sum;
    info.m_wordsOk = wordsOk;
    info.m_bits = secureBits;
    info.m_bitStatus = secureBitStatus;

    handleFragment(msg, a, vecType, info);
}

// Numeric messages (vectors 3, 4, 7): 4-bit digits. The vector holds the length (words - 1) and the low
// 4 bits of the checksum, the first message word the high 2 bits. A numbered numeric message has N, R and S
// in the first word
void RadioPagingFlexDecoder::decodeNumeric(Message& proto, const AddressContext& a, int j, int vecType, int mw1, int mw2)
{
    if ((mw1 > 87) || (mw2 > 87))
    {
        m_status.m_invalidVectors++;
        return;
    }

    const bool numbered = (vecType == 7);
    int skipBits = numbered ? 10 : 2;
    int number = -1;
    int retrieval = 0;
    int special = 0;
    int body0 = a.m_long ? (j + 1) : mw1;

    if (numbered && wordOk(body0))
    {
        uint32_t nh = m_w[body0].m_data;

        number = (nh >> 2) & 0x3f;
        retrieval = (nh >> 8) & 1;
        special = (nh >> 9) & 1;
    }

    bool recovered = false;
    bool duplicate = false;

    if (numbered && (number >= 0))
    {
        int count = std::max(0, mw2 - mw1 + 1);
        int result = checkRetransmission(a.m_capcode, retrieval != 0, number, vecType, m_name - 'A', &m_w[mw1], count, 1,
            a.m_long, 0);

        // A retransmission is output again, marked as a duplicate
        duplicate = (result != 0);

        if (result == -1)
        {
            HistoryEntry *entry = m_historyMatch;

            if (entry)
            {
                int copy = std::min(count, entry->m_wordCount);

                for (int i = 0; i < copy; i++) {
                    m_w[mw1 + i] = entry->m_words[i];
                }
            }

            recovered = true;
        }
    }

    // Digits: a continuous bit stream, 4 bits each, after the overhead bits of the first word. The status of a
    // digit (one for each character of digits): lost 'X', with a bit of a received word that could not be
    // corrected 'u' (its received bits), from a corrected word 'c', else '.'
    std::string digits;
    std::string digitStatus;
    char nibbleStatus = '.';
    std::vector<int> words;
    std::vector<uint32_t> raw;
    int bitCount = 0;
    uint8_t nibble = 0;

    if (a.m_long) {
        words.push_back(j + 1);
    }
    for (int w = mw1; (w <= mw2) && (w < wordsPerFrame); w++) {
        words.push_back(w);
    }

    for (int w : words)
    {
        raw.push_back(m_w[w].m_data);

        const bool uncertain = (m_w[w].m_status == WordUncorrectable) && m_w[w].m_hasReceived;

        if (!uncertain && ((m_w[w].m_status == WordUncorrectable) || (m_w[w].m_status == WordNotReceived)))
        {
            int lostBits = 21;

            if (bitCount < skipBits) {
                lostBits -= (skipBits - bitCount);
            }

            digits.append(lostBits / 4, '?');
            digitStatus.append(lostBits / 4, 'X');
            bitCount += 21;
            continue;
        }

        uint32_t dw = uncertain ? m_w[w].m_received : m_w[w].m_data;
        const bool corrected = (m_w[w].m_status == WordCorrected);

        for (int b = 0; b < 21; b++)
        {
            if (bitCount < skipBits)
            {
                bitCount++;
                continue;
            }

            int pos = (bitCount - skipBits) % 4;

            if (pos == 0)
            {
                nibble = 0;
                nibbleStatus = '.';
            }

            nibble |= ((dw >> b) & 1) << pos;
            if (uncertain) {
                nibbleStatus = 'u';
            } else if (corrected && (nibbleStatus != 'u')) {
                nibbleStatus = 'c';
            }

            if (pos == 3)
            {
                digits += bcdChars[nibble & 0xf];
                digitStatus += nibbleStatus;
            }

            bitCount++;
        }
    }

    // K checksum: low 4 bits in the vector, high 2 bits in the first message word
    uint32_t kLow = (m_w[j].m_data >> 17) & 0xf;
    uint32_t kHigh = wordOk(body0) ? (m_w[body0].m_data & 3) : 0;
    uint32_t kReceived = (kHigh << 4) | kLow;
    uint32_t kSum = 0;
    bool allOk = true;

    if (a.m_long)
    {
        if (wordOk(body0)) {
            kSum += groupSums(m_w[body0].m_data & ~3u);
        } else {
            allOk = false;
        }
    }

    for (int w = mw1; (w <= mw2) && (w < wordsPerFrame); w++)
    {
        if ((m_w[w].m_status == WordUncorrectable) || (m_w[w].m_status == WordNotReceived))
        {
            allOk = false;
            break;
        }

        uint32_t dw = m_w[w].m_data;

        if (!a.m_long && (w == mw1)) {
            dw &= ~3u;
        }

        kSum += groupSums(dw);
    }

    Message msg = proto;

    if (allOk)
    {
        kSum &= 0xff;
        kSum = (kSum & 0x3f) + (kSum >> 6);
        msg.m_checksum = (kReceived == ((~kSum) & 0x3f)) ? CheckOk : CheckFailed;
    }
    else
    {
        msg.m_checksum = CheckFailed;   // K is ok or not: words of the fragment lost, it is not ok
    }

    if (vecType == 4) {
        msg.m_type = TypeSpecialNumeric;
    } else if (numbered) {
        msg.m_type = special ? TypeNumberedSpecialNumeric : TypeNumberedNumeric;
    } else {
        msg.m_type = TypeNumeric;
    }

    // The unused digits of the last word are spaces (fill): not part of the message
    while (!digits.empty() && (digits.back() == ' '))
    {
        digits.pop_back();

        if (!digitStatus.empty()) {
            digitStatus.pop_back();
        }
    }

    msg.m_text = digits;
    msg.m_charStatus = digitStatus;
    msg.m_messageNumber = number;
    msg.m_retrieval = (retrieval != 0);
    msg.m_special = (special != 0) || (vecType == 4);
    msg.m_recovered = recovered;
    msg.m_duplicate = duplicate;
    checkSequence(msg, a);

    for (int w : words) {
        msg.m_wordMap += statusChar(m_w[w].m_status);
    }

    countWords(msg, mw1, mw2);
    setRaw(msg, raw);
    emitMessage(msg);
}

// Hex / binary messages (vector 6). First word: K (12 bits), C, F, N. The first fragment has a second
// header word: R, M, D, H, B, I and the signature. The data bits follow as a continuous stream of 4-bit
// digits. In the last fragment the bits after the data are the inverse of the last data bit
void RadioPagingFlexDecoder::decodeBinary(Message& proto, const AddressContext& a, int j, int mw1, int mw2)
{
    if ((mw1 > 87) || (mw2 > 87))
    {
        m_status.m_invalidVectors++;
        return;
    }

    int hexC = 0;
    int hexF = 0;
    int hexN = 0;
    int hexR = 0;
    int hexM = 0;
    int blocking = 0;
    bool displayRtl = false;
    bool headerMessage = false;
    bool statusInfo = false;
    int reservedBits = 0;
    uint32_t signature = 0;
    bool hasSignature = false;
    bool initial = false;
    FragmentKind kind = FragmentUnknown;
    int dataStart = mw1;
    int hdrIdx = a.m_long ? (j + 1) : mw1;
    bool hdrOk = (hdrIdx < wordsPerFrame) && wordOk(hdrIdx);

    if (hdrOk)
    {
        uint32_t hdr = m_w[hdrIdx].m_data;

        hexC = (hdr >> 12) & 1;
        hexF = (hdr >> 13) & 3;
        hexN = (hdr >> 15) & 0x3f;
        initial = (hexF == 3);

        if (hexC == 0) {
            kind = initial ? FragmentComplete : FragmentLast;
        } else {
            kind = initial ? FragmentFirst : FragmentMiddle;
        }

        // The second header word of the first fragment is the first word of the message field
        // for a long address, the second one for a short address
        int hdr2Idx = a.m_long ? mw1 : (mw1 + 1);

        if (initial)
        {
            dataStart = hdr2Idx + 1;

            if ((hdr2Idx <= mw2) && wordOk(hdr2Idx))
            {
                uint32_t hdr2 = m_w[hdr2Idx].m_data;

                hexR = hdr2 & 1;
                hexM = (hdr2 >> 1) & 1;
                displayRtl = ((hdr2 >> 2) & 1) != 0;
                headerMessage = ((hdr2 >> 3) & 1) != 0;
                blocking = (hdr2 >> 4) & 0xf;
                // R M D H B3-B0 s3-s0 I S7-S0 (STD-43A Fig. 3.10.1.2-1): s in bits 8-11, I in bit 12
                statusInfo = ((hdr2 >> 12) & 1) != 0;
                reservedBits = (int) ((hdr2 >> 8) & 0xf);
                signature = (hdr2 >> 13) & 0xff;
                hasSignature = true;
            }
        }
        else
        {
            dataStart = a.m_long ? mw1 : (mw1 + 1);
        }
    }

    bool recovered = false;
    bool duplicate = false;
    int phaseIndex = m_name - 'A';

    if (initial)
    {
        int count = std::max(0, mw2 - mw1 + 1);
        int result = checkRetransmission(a.m_capcode, hexR != 0, hexN, 6, phaseIndex, &m_w[mw1], count,
            (hexC == 0) ? 1 : 0, a.m_long, blocking);

        // A retransmission is output again, marked as a duplicate
        duplicate = (result != 0);

        if (result == -1)
        {
            HistoryEntry *entry = m_historyMatch;

            if (entry)
            {
                int copy = std::min(count, entry->m_wordCount);

                for (int i = 0; i < copy; i++) {
                    m_w[mw1 + i] = entry->m_words[i];
                }
            }

            recovered = true;
        }
    }

    // Data bits of the fragment, d0 of each word first, 21 a word; a lost word keeps its place (-1).
    // A fragment after the first has no header word 2: its blocking length is the one of the first fragment
    int blockingValue = blocking;
    bool blockingKnown = initial && hasSignature;

    if (!initial && hdrOk)
    {
        int slot = reassemblyFind(a.m_capcode, hexN);

        if (slot >= 0)
        {
            blockingValue = pol().m_reassembly[slot].m_message.m_blocking;
            blockingKnown = true;
        }
    }

    std::vector<int8_t> bits;
    std::string bitStatus;

    for (int w = dataStart; (w <= mw2) && (w < wordsPerFrame); w++)
    {
        bool lost = (m_w[w].m_status == WordUncorrectable) || (m_w[w].m_status == WordNotReceived);

        for (int b = 0; b < 21; b++) {
            bits.push_back(lost ? -1 : (int8_t) ((m_w[w].m_data >> b) & 1));
        }

        bitStatus.append(21, lost ? 'X' : statusChar(m_w[w].m_status));
    }

    int fillState = 0;

    bits.resize(terminateBits(bits, blockingKnown ? (blockingValue ? blockingValue : 16) : 0, hexC == 0, fillState));
    bitStatus.resize(bits.size());

    std::string hex = hexFromBits(bits);

    // Signature of the data (8 bit sums, 1's complement), checked for a message in one piece. The sum of a
    // message in fragments covers all fragments: it is checked when the last one arrives
    Check signatureCheck = CheckNone;

    if (initial && hasSignature && (hexC == 0))
    {
        bool lost = false;
        uint32_t sum = hexSignatureSum(bits, lost);

        signatureCheck = (lost || (fillState == 2)) ? CheckIncomplete : (signature == ((~sum) & 0xff)) ? CheckOk : CheckFailed;
    }

    // K: 12-bit fragment checksum (STD-43A 3.10.1.2): 1's complement of the sum of all words of the fragment
    // (header word 1, at the second vector word of a long address, and the message field), 3 groups of
    // bits per word, the K field counted as 0, fill and termination bits included. Ok or not: words lost, not ok
    Check kCheck = CheckFailed;

    if (hdrOk)
    {
        uint32_t kSum = groupSums(m_w[hdrIdx].m_data & ~0xfffu);
        bool allOk = true;

        for (int w = mw1; (w <= mw2) && (w < wordsPerFrame); w++)
        {
            if (!a.m_long && (w == mw1)) {
                continue;   // Header word 1, counted above
            }
            if (!wordOk(w))
            {
                allOk = false;
                break;
            }
            kSum += groupSums(m_w[w].m_data);
        }

        if (allOk) {
            kCheck = (((~kSum) & 0xfff) == (m_w[hdrIdx].m_data & 0xfff)) ? CheckOk : CheckFailed;
        }
    }

    Message msg = proto;

    msg.m_type = TypeBinary;
    msg.m_checksum = kCheck;
    msg.m_text = hex;
    msg.m_charStatus = hexStatusFromBits(bits, bitStatus);
    msg.m_hexNibbleBits = 4;
    msg.m_hexLsbFirst = true;
    msg.m_dataBits = (int) bits.size();
    msg.m_fillState = fillState;
    msg.m_fragment = kind;
    msg.m_fragmentNumber = hexF;
    msg.m_messageNumber = hdrOk ? hexN : -1;
    msg.m_retrieval = (hexR != 0);
    msg.m_mailDrop = (hexM != 0);
    msg.m_blocking = blocking;
    msg.m_displayRtl = displayRtl;
    msg.m_headerMessage = headerMessage;

    // R is in the first fragment (header word 2): the place of the message in the sequence of the address
    if (initial) {
        checkSequence(msg, a);
    }
    msg.m_statusInfo = statusInfo;
    msg.m_reservedBits = reservedBits;
    msg.m_signature = signatureCheck;
    msg.m_recovered = recovered;
    msg.m_duplicate = duplicate;
    msg.m_fragmented = (kind == FragmentFirst) || (kind == FragmentMiddle) || (kind == FragmentLast);

    // Continuation fragments take the blocking of the first fragment
    if (!initial) {
        msg.m_blocking = blockingValue;
    }

    FragmentInfo info;

    info.m_bits = bits;
    info.m_bitStatus = bitStatus;
    info.m_signature = signature;
    info.m_wordsOk = initial ? hasSignature : true;

    if (a.m_long)
    {
        info.m_raw.push_back(m_w[j + 1].m_data);
        msg.m_wordMap += statusChar(m_w[j + 1].m_status);
    }

    for (int w = mw1; (w <= mw2) && (w < wordsPerFrame); w++)
    {
        info.m_raw.push_back(m_w[w].m_data);
        msg.m_wordMap += statusChar(m_w[w].m_status);
    }

    countWords(msg, mw1, mw2);
    handleFragment(msg, a, 6, info);
}
