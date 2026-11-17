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

// Self test of the sequential Golay paging format. Prints one line per check and
// returns non-zero if any check fails.

#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "radiopaginggsc.h"
#include "radiopaginggscdecoder.h"
#include "radiopaginggscreceiver.h"

namespace {

int passes = 0;
int failures = 0;

void check(bool ok, const char *what)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (ok) {
        passes++;
    } else {
        failures++;
    }
}

typedef RadioPagingGsc G;
typedef RadioPagingGscDecoder D;

void testCodes()
{
    bool encodeOk = true;
    bool errorsOk = true;
    bool complementOk = true;
    long patterns = 0;

    for (uint32_t data = 0; data < 4096; data++)
    {
        uint32_t cw = G::golayEncode((uint16_t) data);

        if ((G::golaySyndrome(cw) != 0) || ((cw & 0xfff) != data)) {
            encodeOk = false;
        }
        if (G::golaySyndrome(cw ^ 0x7fffff) != 0) {
            complementOk = false;
        }

        G::GolayResult clean = G::golayDecode(cw);
        if ((clean.m_word != cw) || (clean.m_data != data) || (clean.m_bitErrors != 0)) {
            errorsOk = false;
        }

        for (int i = 0; i < 23; i++)
        {
            uint32_t e1 = 1u << i;
            G::GolayResult r1 = G::golayDecode(cw ^ e1);
            patterns++;
            if ((r1.m_word != cw) || (r1.m_bitErrors != 1)) {
                errorsOk = false;
            }

            for (int j = i + 1; j < 23; j++)
            {
                uint32_t e2 = e1 | (1u << j);
                G::GolayResult r2 = G::golayDecode(cw ^ e2);
                patterns++;
                if ((r2.m_word != cw) || (r2.m_bitErrors != 2)) {
                    errorsOk = false;
                }

                for (int k = j + 1; k < 23; k++)
                {
                    G::GolayResult r3 = G::golayDecode(cw ^ e2 ^ (1u << k));
                    patterns++;
                    if ((r3.m_word != cw) || (r3.m_bitErrors != 3)) {
                        errorsOk = false;
                    }
                }
            }
        }
    }

    check(encodeOk, "Golay: all 4096 data words encode to codewords with data in bits 11-0");
    check(complementOk, "Golay: the complement of every codeword is a codeword");
    check(errorsOk, "Golay: all error patterns of weight 0-3 on all 4096 codewords decode to the original");
    std::printf("      %ld error patterns checked\n", patterns);

    check(G::golayEncode(0) == 0, "Golay: zero data gives the zero codeword");

    bool bchEncodeOk = true;
    bool bchErrorsOk = true;
    bool bchBeyond = true;
    long bchPatterns = 0;

    for (uint32_t data = 0; data < 128; data++)
    {
        uint16_t cw = G::bchEncode((uint8_t) data);

        if ((G::bchSyndrome(cw) != 0) || ((cw & 0x7f) != data)) {
            bchEncodeOk = false;
        }

        G::BchResult clean = G::bchDecode(cw);
        if ((clean.m_word != cw) || (clean.m_data != data) || (clean.m_bitErrors != 0)) {
            bchErrorsOk = false;
        }

        for (int i = 0; i < 15; i++)
        {
            uint16_t e1 = (uint16_t) (1u << i);
            G::BchResult r1 = G::bchDecode(cw ^ e1);
            bchPatterns++;
            if ((r1.m_word != cw) || (r1.m_bitErrors != 1)) {
                bchErrorsOk = false;
            }

            for (int j = i + 1; j < 15; j++)
            {
                G::BchResult r2 = G::bchDecode(cw ^ e1 ^ (1u << j));
                bchPatterns++;
                if ((r2.m_word != cw) || (r2.m_bitErrors != 2)) {
                    bchErrorsOk = false;
                }
            }
        }
    }

    // Some patterns of weight 3 are detected as uncorrectable, none are silently accepted as weight 0
    int detected = 0;
    for (int i = 0; i < 15; i++)
    {
        for (int j = i + 1; j < 15; j++)
        {
            for (int k = j + 1; k < 15; k++)
            {
                G::BchResult r = G::bchDecode((uint16_t) ((1u << i) | (1u << j) | (1u << k)));
                if (r.m_bitErrors < 0) {
                    detected++;
                } else if (r.m_bitErrors == 0) {
                    bchBeyond = false;
                }
            }
        }
    }

    check(bchEncodeOk, "BCH: all 128 data words encode to codewords with data in bits 6-0");
    check(bchErrorsOk, "BCH: all error patterns of weight 0-2 on all 128 codewords decode to the original");
    check(bchBeyond && (detected > 0), "BCH: weight 3 patterns are never taken for clean codewords and some are detected");
    std::printf("      %ld error patterns checked, %d of 455 weight 3 patterns detected\n", bchPatterns, detected);
}

void testAddresses()
{
    bool roundTrip = true;
    bool illegalOk = true;
    long count = 0;

    for (int index = 0; index < 10; index++)
    {
        for (int group = 0; group < 100; group++)
        {
            for (int low = 0; low < 1000; low++)
            {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "%d%02d%03d", index, group, low);
                std::string address(buf);
                int preamble;
                uint16_t w1, w2;
                bool ok = G::encodeAddress(address, preamble, w1, w2);
                std::string back;

                if (ok)
                {
                    count++;
                    if (!G::decodeAddress(w1, w2, preamble, back) || (back != address) || (w2 >= 2000)
                        || (G::word1Index(w1 ^ 0xfff) >= 0) || (G::word1Index(w1) < 0)) {
                        roundTrip = false;
                    }
                }
                else
                {
                    // The same digits are refused for every index
                    static const int lowIllegal[16] = { 0, 25, 51, 103, 206, 340, 363, 412, 445, 530, 642, 726, 782, 810, 825, 877 };
                    static const int highIllegal[7] = { 0, 292, 425, 584, 631, 841, 851 };
                    bool expected = false;

                    for (int v : lowIllegal) {
                        expected = expected || ((group < 50) && (v == low));
                    }
                    for (int v : highIllegal) {
                        expected = expected || ((group >= 50) && (v == low));
                    }
                    if (!expected) {
                        illegalOk = false;
                    }
                }
            }
        }
    }

    check(roundTrip, "Address: encode and decode agree for all legal addresses, word 2 below 2000, no word 1 collides with a complement");
    check(illegalOk, "Address: only the listed digit combinations are refused");
    std::printf("      %ld legal addresses\n", count);

    int preamble;
    uint16_t w1, w2;
    std::string error;
    check(!G::encodeAddress("12345", preamble, w1, w2, &error), "Address: 5 digits refused");
    check(!G::encodeAddress("12a456", preamble, w1, w2, &error), "Address: non digit refused");

    bool preambleOk = true;
    for (int i = 0; i < 10; i++)
    {
        uint32_t cw = G::golayEncode(G::preambleValue(i));
        G::GolayResult direct = G::golayDecode(cw);
        G::GolayResult flipped = G::golayDecode(cw ^ 0x7fffff);

        if ((G::preambleIndex(direct.m_data) != i) || (G::preambleIndex(flipped.m_data) >= 0)
            || (G::preambleIndex(flipped.m_data ^ 0xfff) != i)) {
            preambleOk = false;
        }
    }
    check(preambleOk, "Preamble values and their complements are distinguishable");
}

// Feed the bits, optionally inverted, after random noise
struct Decoded
{
    std::vector<D::Message> m_messages;
    D::Status m_status;
    bool m_availableBeforeFlush = false;
};

Decoded decode(const std::vector<uint8_t>& bits, D::Polarity polarity, bool invertBits, int noiseBits, unsigned seed, bool nbs = false)
{
    std::mt19937 rng(seed);
    D decoder(polarity);
    Decoded out;

    decoder.setNonBatterySaver(nbs);

    for (int i = 0; i < noiseBits; i++) {
        decoder.feed((int) (rng() & 1));
    }
    for (uint8_t bit : bits) {
        decoder.feed(invertBits ? (bit ^ 1) : bit);
    }

    out.m_availableBeforeFlush = decoder.hasMessage();
    decoder.flush();

    while (decoder.hasMessage()) {
        out.m_messages.push_back(decoder.takeMessage());
    }
    out.m_status = decoder.getStatus();

    return out;
}

G::TxMessage tx(const char *address, int number, G::MessageType type, const char *text = "")
{
    G::TxMessage m;

    m.m_address = address;
    m.m_addressNumber = number;
    m.m_type = type;
    m.m_text = text;

    return m;
}

bool matches(const D::Message& rx, const G::TxMessage& m)
{
    if ((rx.m_address.substr(1) != m.m_address.substr(1)) || (rx.m_addressNumber != m.m_addressNumber)) {
        return false;
    }
    // Non battery saver: no preamble, no index digit (N)
    if ((rx.m_address[0] != 'N') && (rx.m_address[0] != m.m_address[0])) {
        return false;
    }

    switch (m.m_type)
    {
    case G::Tone:
        return (rx.m_type == G::Tone) && (rx.m_rawBits == 0);
    case G::Voice:
        return (rx.m_type == G::Voice) && (rx.m_rawBits == 0);
    case G::Numeric:
        return (rx.m_type == G::Data) && (G::decodeNumeric(rx.m_rawData, rx.m_rawBits) == m.m_text);
    default:
        return (rx.m_type == G::Data) && (G::decodeAlphanumeric(rx.m_rawData, rx.m_rawBits) == m.m_text);
    }
}

// The pages of sent, in order. Updates of voice pages (end of the voice message) are not
// pages and are checked by the voice tests
bool allMatch(const Decoded& d, const std::vector<G::TxMessage>& sent)
{
    std::vector<D::Message> pages;

    for (const D::Message& m : d.m_messages)
    {
        if (!m.m_update) {
            pages.push_back(m);
        }
    }
    if (pages.size() != sent.size()) {
        return false;
    }
    for (size_t i = 0; i < sent.size(); i++)
    {
        if (!matches(pages[i], sent[i])) {
            return false;
        }
    }
    return true;
}

void roundTrip(const char *name, const std::vector<G::TxMessage>& sent, G::Mode mode = G::Auto)
{
    G::EncodeOptions options;
    options.m_mode = mode;
    G::EncodeResult enc = G::encode(sent, options);
    std::string label = name;

    if (!enc.m_ok)
    {
        std::printf("      encode error: %s\n", enc.m_error.c_str());
        check(false, (label + ": encode").c_str());
        return;
    }

    Decoded normal = decode(enc.m_bits, D::Normal, false, 0, 1);
    check(allMatch(normal, sent), (label + ": normal polarity round trip").c_str());

    Decoded inverted = decode(enc.m_bits, D::Inverted, true, 0, 1);
    check(allMatch(inverted, sent), (label + ": inverted bits, inverted decoder").c_str());

    Decoded wrongA = decode(enc.m_bits, D::Inverted, false, 0, 1);
    Decoded wrongB = decode(enc.m_bits, D::Normal, true, 0, 1);
    check(wrongA.m_messages.empty() && wrongB.m_messages.empty(), (label + ": the other polarity decoder stays silent").c_str());

    Decoded noisy = decode(enc.m_bits, D::Normal, false, 3000, 7);
    check(allMatch(noisy, sent), (label + ": preceded by random noise").c_str());
}

// Offsets of a single message transmission with a preamble (see the format description)
void injectGolay(std::vector<uint8_t>& bits, size_t start)
{
    static const int pairs[3] = { 1, 8, 17 };

    for (int p : pairs) {
        bits[start + 2 * p] ^= 1;
    }
}

void testErrors()
{
    std::vector<G::TxMessage> sent = { tx("100123", 3, G::Alphanumeric, "ERRORS IN ALL CODEWORDS OF THIS LONGER MESSAGE 0123456789") };
    G::EncodeResult enc = G::encode(sent);
    int blocks = ((int) enc.m_bits.size() - 1098) / 121;

    for (int i = 0; i < 18; i++) {
        injectGolay(enc.m_bits, 28 + 46 * i);
    }
    injectGolay(enc.m_bits, 884);
    injectGolay(enc.m_bits, 931);
    injectGolay(enc.m_bits, 1005);
    injectGolay(enc.m_bits, 1052);

    for (int b = 0; b < blocks; b++)
    {
        for (int k = 0; k < 8; k++)
        {
            enc.m_bits[1099 + 121 * b + 3 * 8 + k] ^= 1;
            enc.m_bits[1099 + 121 * b + 11 * 8 + k] ^= 1;
        }
    }

    Decoded d = decode(enc.m_bits, D::Normal, false, 0, 1);
    check(allMatch(d, sent), "Three errors in every Golay codeword and two in every BCH codeword still decode");
    check(!d.m_messages.empty() && (d.m_messages[0].m_errorsCorrected > 0) && (d.m_messages[0].m_errorsUncorrectable == 0)
        && (d.m_messages[0].m_checksumErrors == 0), "Corrections are reported, nothing uncorrectable");
    std::printf("      %d blocks, %d bits corrected\n", blocks, d.m_messages.empty() ? -1 : d.m_messages[0].m_errorsCorrected);

    // A tone only message with errors in its address
    std::vector<G::TxMessage> tone = { tx("100123", 2, G::Tone) };
    G::EncodeResult encTone = G::encode(tone);
    injectGolay(encTone.m_bits, 1005);
    injectGolay(encTone.m_bits, 1052);
    check(allMatch(decode(encTone.m_bits, D::Normal, false, 0, 1), tone), "Tone only with errors in the address words");

    // First data block with 1 and with 2 uncorrectable codewords: 7 of 8 good is still
    // data, 6 of 8 is taken for noise after a tone only page
    auto firstBlockBad = [](int nbBad, int first = 0) {
        std::vector<G::TxMessage> m = { tx("100123", 1, G::Alphanumeric, "FIRST BLOCK DAMAGED") };
        G::EncodeOptions single;
        single.m_mode = G::Single;
        std::vector<uint8_t> bits = G::encode(m, single).m_bits;
        const int block = 1098;
        for (int k = first; k < first + nbBad; k++)
        {
            // 3 bit errors that the code detects as uncorrectable
            bool done = false;
            for (int a = 0; (a < 15) && !done; a++) {
                for (int b = a + 1; (b < 15) && !done; b++) {
                    for (int c = b + 1; (c < 15) && !done; c++)
                    {
                        uint16_t cw[8];
                        G::deinterleaveBlock(&bits[block + 1], cw);
                        uint16_t e = (uint16_t) (cw[k] ^ (1u << a) ^ (1u << b) ^ (1u << c));
                        if (G::bchDecode(e).m_bitErrors < 0)
                        {
                            bits[block + 1 + a * 8 + k] ^= 1;
                            bits[block + 1 + b * 8 + k] ^= 1;
                            bits[block + 1 + c * 8 + k] ^= 1;
                            done = true;
                        }
                    }
                }
            }
        }
        return decode(bits, D::Normal, false, 0, 1);
    };
    Decoded oneBad = firstBlockBad(1);
    Decoded twoBad = firstBlockBad(2);
    check((oneBad.m_messages.size() == 1) && (oneBad.m_messages[0].m_type == G::Data) && (oneBad.m_messages[0].m_errorsUncorrectable == 1),
        "First data block with 1 uncorrectable codeword is data");
    check(!twoBad.m_messages.empty() && (twoBad.m_messages[0].m_type == G::Tone),
        "First data block with 2 uncorrectable codewords is taken for noise: tone only");

    // The characters of an uncorrectable codeword are '?', not those of its zeroed bits; the others are received
    if (!oneBad.m_messages.empty())
    {
        const D::Message& msg = oneBad.m_messages[0];
        std::string marked = G::interpret(msg.m_rawData, msg.m_rawBits, nullptr, &msg.m_rawLost).m_text;
        std::string unmarked = G::interpret(msg.m_rawData, msg.m_rawBits).m_text;
        std::string what = "Characters of an uncorrectable codeword are lost ('" + marked + "')";
        check((marked.compare(0, 19, "??RST BLOCK DAMAGED") == 0) && (unmarked.compare(0, 2, "??") != 0), what.c_str());
    }

    // The block end flag is in the last codeword: lost, the end of the message is not known
    Decoded lastBad = firstBlockBad(1, 6);
    check((lastBad.m_messages.size() == 1) && lastBad.m_messages[0].m_truncated,
        "Uncorrectable last codeword of a block: the message is truncated");
}

void testBatches()
{
    std::vector<G::TxMessage> three = {
        tx("100123", 1, G::Tone),
        tx("219789", 2, G::Numeric, "5551234 -U"),
        tx("100456", 4, G::Alphanumeric, "MEETING AT NOON"),
    };
    roundTrip("Batch of 3", three);

    G::EncodeResult enc = G::encode(three);
    check(enc.m_ok && (enc.m_slots == 16), "Batch of 3 uses 16 slots");

    std::vector<G::TxMessage> full;
    for (int i = 0; i < 16; i++)
    {
        char address[8];
        std::snprintf(address, sizeof(address), "1007%02d", i);
        full.push_back(tx(address, 1 + (i & 3), (i % 3 == 0) ? G::Tone : ((i % 3 == 1) ? G::Numeric : G::Alphanumeric),
            (i % 3 == 1) ? "0123456789" : ((i % 3 == 2) ? "PAGE" : "")));
    }
    roundTrip("Normal batch of 16", full);

    std::vector<G::TxMessage> extended;
    for (int i = 0; i < 20; i++)
    {
        char address[8];
        std::snprintf(address, sizeof(address), "1007%02d", i);
        extended.push_back(tx(address, 1 + ((i + 1) & 3), (i % 3 == 0) ? G::Numeric : ((i % 3 == 1) ? G::Tone : G::Alphanumeric),
            (i % 3 == 0) ? "98765 4321" : ((i % 3 == 2) ? "EXTENDED BATCH MESSAGE NUMBER" : "")));
    }
    roundTrip("Extended batch of 20", extended);
    G::EncodeResult encExt = G::encode(extended);
    // Numeric 2 slots, tone 1, alphanumeric 5 (address and 4 blocks): every 6 messages fill
    // 16 slots exactly, so start codes come before messages 7, 13 and 19, and the last batch
    // (2 + 1 slots) is filled out to 16: 3 * 16 + 3 + 16
    check(encExt.m_ok && (encExt.m_slots == 67), "Extended batch of 20: slots of addresses, data blocks, 3 start codes and fill");

    // 20 tone only pages: 16 addresses, the start code as the 17th block, 4 addresses and
    // 12 fill commas (Motorola extended batch, Figure 6-2)
    std::vector<G::TxMessage> tones;
    for (int i = 0; i < 20; i++)
    {
        char address[8];
        std::snprintf(address, sizeof(address), "1006%02d", i);
        tones.push_back(tx(address, 1 + (i & 3), G::Tone));
    }
    roundTrip("Extended batch of 20 tone only pages", tones);
    G::EncodeResult encTones = G::encode(tones);
    uint32_t sc = G::golayEncode(G::m_startCode);
    bool scAt17 = encTones.m_ok && (encTones.m_bits.size() == (size_t) (856 + 121 + 33 * 121))
        && (G::resolveDuplicated(&encTones.m_bits[856 + 121 + 16 * 121 + 28]) == sc);
    check(scAt17 && (encTones.m_slots == 33), "Extended batch: start code in the 17th block, no tone comma, 33 slots");

    // Word 1 of the repeated start code damaged into an address Word 1: Word 2 still
    // identifies the start code
    std::vector<uint8_t> fakeWord1;
    G::appendDuplicated(fakeWord1, G::golayEncode(721));
    std::vector<uint8_t> damagedSc(encTones.m_bits);
    std::copy(fakeWord1.begin(), fakeWord1.end(), damagedSc.begin() + 856 + 121 + 16 * 121 + 28);
    check(allMatch(decode(damagedSc, D::Normal, false, 0, 1), tones), "Repeated start code with Word 1 damaged into an address Word 1, found on Word 2");

    // A message started in the batch runs on past the 16th slot, the start code follows it
    std::vector<G::TxMessage> across;
    for (int i = 0; i < 15; i++)
    {
        char address[8];
        std::snprintf(address, sizeof(address), "1009%02d", i);
        across.push_back(tx(address, 1, G::Tone));
    }
    across.push_back(tx("100950", 2, G::Alphanumeric, "RUNS PAST THE SIXTEENTH"));
    across.push_back(tx("100951", 3, G::Tone));
    roundTrip("Batch with a message running past the 16th slot", across);
    check(G::encode(across).m_slots == 19 + 1 + 16, "Batch: message to slot 19, start code, then a new batch of 16 slots");

    // Tone only addresses with the 968 bit comma after them, as some encoders send them
    G::EncodeResult encComma = G::encode({ tx("100123", 1, G::Tone), tx("100456", 2, G::Alphanumeric, "AFTER COMMA") });
    std::vector<uint8_t> comma;
    G::appendComma(comma, G::m_toneCommaBits, 1);
    encComma.m_bits.insert(encComma.m_bits.begin() + 1098, comma.begin(), comma.end());
    check(allMatch(decode(encComma.m_bits, D::Normal, false, 0, 1), { tx("100123", 1, G::Tone), tx("100456", 2, G::Alphanumeric, "AFTER COMMA") }),
        "Tone only address followed by a 968 bit comma is decoded");

    // A shorter comma (3 blocks), then a start code and the next address: the comma is
    // skipped as long as it lasts, not by a fixed length
    std::vector<G::TxMessage> shortSent = { tx("100123", 1, G::Tone), tx("100456", 2, G::Alphanumeric, "AFTER SHORT COMMA") };
    G::EncodeResult encShort = G::encode(shortSent);
    std::vector<uint8_t> shortComma;
    G::appendComma(shortComma, 3 * G::m_blockBits, 1);
    uint32_t scWord = G::golayEncode(G::m_startCode);
    G::appendComma(shortComma, G::m_commaBits, scWord & 1);
    G::appendDuplicated(shortComma, scWord);
    shortComma.push_back((uint8_t) (((scWord ^ 0x7fffff) & 1) ^ 1));
    G::appendDuplicated(shortComma, scWord ^ 0x7fffff);
    encShort.m_bits.insert(encShort.m_bits.begin() + 1098, shortComma.begin(), shortComma.end());
    check(allMatch(decode(encShort.m_bits, D::Normal, false, 0, 1), shortSent), "Tone only address, a 3 block comma, a start code and an address");

    // The comma of the address after a tone only address with 1 and 2 bit errors (outcome
    // only: these blocks are also rejected as data by the damaged block rule)
    std::vector<G::TxMessage> damaged = { tx("100123", 1, G::Tone), tx("100456", 2, G::Tone), tx("100789", 3, G::Alphanumeric, "HI") };
    G::EncodeResult encDamaged = G::encode(damaged);
    std::vector<uint8_t> oneError(encDamaged.m_bits);
    oneError[1098 + 5] ^= 1;
    std::vector<uint8_t> twoErrors(oneError);
    twoErrors[1098 + 17] ^= 1;
    check(allMatch(decode(oneError, D::Normal, false, 0, 1), damaged) && allMatch(decode(twoErrors, D::Normal, false, 0, 1), damaged),
        "Address after a tone only address with 1 and 2 bit errors in its comma");

    // Extension with a new preamble and start code after 16 slots (Rep. 900-2 Annex I 2.2)
    std::vector<G::TxMessage> first16(tones.begin(), tones.begin() + 16);
    std::vector<G::TxMessage> last4(tones.begin() + 16, tones.end());
    G::EncodeOptions batchOptions;
    batchOptions.m_mode = G::Batch;
    std::vector<uint8_t> withPreamble = G::encode(first16, batchOptions).m_bits;
    std::vector<uint8_t> second4 = G::encode(last4, batchOptions).m_bits;
    withPreamble.insert(withPreamble.end(), second4.begin(), second4.end());
    Decoded dPre = decode(withPreamble, D::Normal, false, 0, 1);
    check(allMatch(dPre, tones) && (dPre.m_status.m_transmissions == 2), "Extended batch with a new preamble and start code (Rep. 900-2 Annex I 2.2)");

    // Slots of a single page and of a non battery saver page
    G::EncodeOptions single;
    single.m_mode = G::Single;
    G::EncodeOptions nbs;
    nbs.m_mode = G::NonBatterySaver;
    check((G::encode({ tx("100123", 1, G::Alphanumeric, "TWO BLOCKS HERE") }, single).m_slots == 3)
        && (G::encode({ tx("100123", 1, G::Tone) }, nbs).m_slots == 1), "Slots: single page of 2 blocks is 3, non battery saver tone only is 1");

    Decoded d = decode(encExt.m_bits, D::Normal, false, 0, 1);
    bool batchFlag = !d.m_messages.empty();
    for (const D::Message& m : d.m_messages) {
        batchFlag = batchFlag && m.m_batch && (m.m_preamble == 1);
    }
    check(batchFlag && (d.m_messages.size() == 20) && (d.m_messages[19].m_slot == 19), "Batch flag, preamble index and slots are reported");

    std::vector<G::TxMessage> mixedPreamble = { tx("100123", 1, G::Tone), tx("300123", 1, G::Tone) };
    check(!G::encode(mixedPreamble).m_ok, "Batch with different preamble values is refused");
    check(!G::encode({ tx("100123", 1, G::Voice), tx("100456", 1, G::Tone) }).m_ok, "Voice in a batch is refused");
}

void testNonBatterySaver()
{
    std::vector<G::TxMessage> sent = { tx("500123", 2, G::Alphanumeric, "NO PREAMBLE CODEWORDS") };
    G::EncodeOptions options;
    options.m_mode = G::NonBatterySaver;
    G::EncodeResult enc = G::encode(sent, options);
    Decoded off = decode(enc.m_bits, D::Normal, false, 0, 1, false);
    Decoded on = decode(enc.m_bits, D::Normal, false, 0, 1, true);
    Decoded onNoise = decode(enc.m_bits, D::Normal, false, 500, 3, true);
    Decoded onInverted = decode(enc.m_bits, D::Inverted, true, 0, 1, true);

    check(enc.m_ok && off.m_messages.empty(), "Non battery saver pattern is ignored unless enabled");
    check(allMatch(on, sent) && on.m_messages[0].m_nonBatterySaver && (on.m_messages[0].m_address[0] == 'N'), "Non battery saver alphanumeric round trip (index N)");
    check(allMatch(onNoise, sent), "Non battery saver after random noise");
    check(allMatch(onInverted, sent), "Non battery saver, inverted bits, inverted decoder");

    // Bit errors in the 1100 pattern, 2 of them within its last 80 bits, single and adjacent:
    // the pattern runs on to its end
    std::vector<uint8_t> damaged = enc.m_bits;
    for (int i : {100, 301, 302, 500, 690, 731, 732}) {
        damaged[i] ^= 1;
    }
    check(allMatch(decode(damaged, D::Normal, false, 0, 1, true), sent), "Non battery saver, bit errors in the pattern up to its end");

    // A bit error every 60 bits: no 80 bits of the pattern without one
    std::vector<uint8_t> noisy = enc.m_bits;
    for (int i = 30; i < G::m_nonBatterySaverBits; i += 60) {
        noisy[i] ^= 1;
    }
    check(allMatch(decode(noisy, D::Normal, false, 0, 1, true), sent), "Non battery saver, a bit error every 60 bits of the pattern");

    // A bit error in the comma of the first address
    std::vector<uint8_t> comma = enc.m_bits;
    comma[G::m_nonBatterySaverBits + 12] ^= 1;
    check(allMatch(decode(comma, D::Normal, false, 0, 1, true), sent), "Non battery saver, a bit error in the comma of the first address");

    std::vector<G::TxMessage> tone = { tx("500123", 1, G::Tone) };
    check(allMatch(decode(G::encode(tone, options).m_bits, D::Normal, false, 0, 1, true), tone), "Non battery saver tone only");
    std::vector<G::TxMessage> voice = { tx("500123", 3, G::Voice) };
    check(allMatch(decode(G::encode(voice, options).m_bits, D::Normal, false, 0, 1, true), voice), "Non battery saver voice");
}

void testInterpretation()
{
    std::vector<G::TxMessage> alpha = { tx("100123", 1, G::Alphanumeric, "HELLO WORLD") };
    std::vector<G::TxMessage> numeric = { tx("100123", 1, G::Numeric, "5551234") };
    Decoded a = decode(G::encode(alpha).m_bits, D::Normal, false, 0, 1);
    Decoded n = decode(G::encode(numeric).m_bits, D::Normal, false, 0, 1);

    G::Interpretation ia = G::interpret(a.m_messages[0].m_rawData, a.m_messages[0].m_rawBits);
    G::Interpretation in = G::interpret(n.m_messages[0].m_rawData, n.m_messages[0].m_rawBits);

    check((ia.m_type == "alphanumeric") && (ia.m_text == "HELLO WORLD") && (ia.m_altType == "numeric") && (ia.m_score > ia.m_altScore),
        "Interpretation of an alphanumeric message");
    check((in.m_type == "numeric") && (in.m_text == "5551234") && (in.m_altType == "alphanumeric") && (in.m_score > in.m_altScore),
        "Interpretation of a numeric message");
    std::printf("      alpha: %s %d / %s %d, numeric: %s %d / %s %d\n", ia.m_type.c_str(), ia.m_score, ia.m_altType.c_str(), ia.m_altScore,
        in.m_type.c_str(), in.m_score, in.m_altType.c_str(), in.m_altScore);

    check(G::interpret(std::vector<uint8_t>(), 0).m_type == "tone", "Interpretation without bits is tone only");

    // A long alphanumeric message of figures and punctuation, and the longest numeric page without the length penalty
    std::vector<G::TxMessage> longAlpha = { tx("100123", 1, G::Alphanumeric, "SWF 1.5345-55 YEN 117.70-80 SWF 1.3026-46 FT-100 2951.2") };
    std::vector<G::TxMessage> longNumeric = { tx("100123", 1, G::Numeric, "555123456789012") };
    Decoded la = decode(G::encode(longAlpha).m_bits, D::Normal, false, 0, 1);
    Decoded ln = decode(G::encode(longNumeric).m_bits, D::Normal, false, 0, 1);
    G::Interpretation ila = G::interpret(la.m_messages[0].m_rawData, la.m_messages[0].m_rawBits);
    G::Interpretation iln = G::interpret(ln.m_messages[0].m_rawData, ln.m_messages[0].m_rawBits);

    check((ila.m_type == "alphanumeric") && (ila.m_text == longAlpha[0].m_text),
        "Interpretation of a long alphanumeric message of figures");
    check((iln.m_type == "numeric") && (iln.m_text == longNumeric[0].m_text),
        "Interpretation of a 15 digit numeric message");
    std::printf("      long alpha: %s %d / %s %d, 15 digits: %s %d / %s %d\n", ila.m_type.c_str(), ila.m_score, ila.m_altType.c_str(), ila.m_altScore,
        iln.m_type.c_str(), iln.m_score, iln.m_altType.c_str(), iln.m_altScore);

    // A full block of alphanumeric text has no padding
    std::vector<G::TxMessage> full = { tx("100123", 1, G::Alphanumeric, "ABCDEFGH") };
    Decoded f = decode(G::encode(full).m_bits, D::Normal, false, 0, 1);
    G::Interpretation fi = f.m_messages.empty() ? G::Interpretation() : G::interpret(f.m_messages[0].m_rawData, f.m_messages[0].m_rawBits);
    check(!f.m_messages.empty() &&((fi.m_type == "alphanumeric" ? fi.m_text : fi.m_altText) == "ABCDEFGH"),
        "A message that fills a block (no padding) keeps the alphanumeric reading as a candidate");
    // Its numeric reading has Null nibbles between digits, which padding never is
    check((fi.m_type == "alphanumeric") && (fi.m_text == "ABCDEFGH"), "A full block of letters is alphanumeric");

    // A long number of digits only: the bits of an alphanumeric message are almost never digits only
    for (const char *number : {"123456789012345678901234", "555 123-4567 555 123-4567"})
    {
        std::vector<G::TxMessage> digitsOnly = { tx("100123", 1, G::Numeric, number) };
        Decoded dd = decode(G::encode(digitsOnly).m_bits, D::Normal, false, 0, 1);
        G::Interpretation di = dd.m_messages.empty() ? G::Interpretation() : G::interpret(dd.m_messages[0].m_rawData, dd.m_messages[0].m_rawBits);
        check(!dd.m_messages.empty() && (di.m_type == "numeric") && (di.m_text == number),
            (std::string("A long number of digits only is numeric: ") + number).c_str());
    }

    std::vector<G::TxMessage> letters = { tx("100123", 1, G::Numeric, "ABCDEFGHJLNPR U-0123") };
    check(allMatch(decode(G::encode(letters).m_bits, D::Normal, false, 0, 1), letters), "Numeric letters through the shift nibble");

    check(G::escapeControl(std::string("A\r") + "B") == "A<0x0D>B", "Control characters are escaped");

    // Encoding and decoding take the same Table VII: _ is 0x3F, a character outside the set is sent as space
    G::EncodeResult underscore = G::encode({ tx("100123", 1, G::Alphanumeric, "A_B~C") });
    Decoded du = decode(underscore.m_bits, D::Normal, false, 0, 1);
    check(!du.m_messages.empty() && (G::decodeAlphanumeric(du.m_messages[0].m_rawData, du.m_messages[0].m_rawBits) == "A_B C"),
        "Alphanumeric: _ is its Table VII code, a character outside the set is sent as space");
}

void testStreaming()
{
    // Message available as soon as its last block is in, without flush
    std::vector<G::TxMessage> sent = { tx("100123", 1, G::Alphanumeric, "STREAMING") };
    Decoded d = decode(G::encode(sent).m_bits, D::Normal, false, 0, 1);
    check(d.m_availableBeforeFlush && allMatch(d, sent), "Message is available before flush");

    // Truncated message
    G::EncodeResult enc = G::encode({ tx("100123", 1, G::Alphanumeric, "THIS MESSAGE HAS THREE BLOCKS OF TEXT.") });
    std::vector<uint8_t> cut(enc.m_bits.begin(), enc.m_bits.begin() + 1098 + 121 * 2);
    Decoded t = decode(cut, D::Normal, false, 0, 1);
    check((t.m_messages.size() == 1) && t.m_messages[0].m_truncated && (t.m_messages[0].m_rawBits == 96), "Cut off message is reported truncated after flush");

    // Two transmissions with noise in between
    std::mt19937 rng(5);
    std::vector<G::TxMessage> a = { tx("100123", 1, G::Alphanumeric, "FIRST") };
    std::vector<G::TxMessage> b = { tx("300456", 2, G::Numeric, "123") };
    std::vector<uint8_t> bits = G::encode(a).m_bits;
    for (int i = 0; i < 300; i++) {
        bits.push_back((uint8_t) (rng() & 1));
    }
    std::vector<uint8_t> second = G::encode(b).m_bits;
    bits.insert(bits.end(), second.begin(), second.end());
    Decoded two = decode(bits, D::Normal, false, 100, 2);
    check((two.m_messages.size() == 2) && matches(two.m_messages[0], a[0]) && matches(two.m_messages[1], b[0]) && (two.m_status.m_transmissions == 2),
        "Two transmissions separated by noise");

    // Back to back without any gap
    std::vector<uint8_t> tight = G::encode(a).m_bits;
    tight.insert(tight.end(), second.begin(), second.end());
    Decoded two2 = decode(tight, D::Normal, false, 0, 1);
    check((two2.m_messages.size() == 2) && matches(two2.m_messages[0], a[0]) && matches(two2.m_messages[1], b[0]), "Two transmissions back to back");

    // Individual transmission mode: a tone only page, then the next preamble at once
    // (Motorola Figure 6-2)
    std::vector<G::TxMessage> toneA = { tx("100123", 4, G::Tone) };
    std::vector<uint8_t> individual = G::encode(toneA).m_bits;
    individual.insert(individual.end(), second.begin(), second.end());
    Decoded ind = decode(individual, D::Normal, false, 0, 1);
    check((ind.m_messages.size() == 2) && matches(ind.m_messages[0], toneA[0]) && matches(ind.m_messages[1], b[0]),
        "Tone only page followed at once by the preamble of the next transmission");

    // A tone only page followed by noise (the carrier is gone) stays tone only
    int toneOnly = 0;
    G::EncodeResult encTone = G::encode(toneA);
    for (int t = 0; t < 200; t++)
    {
        std::vector<uint8_t> noisy = encTone.m_bits;
        for (int i = 0; i < 2000; i++) {
            noisy.push_back((uint8_t) (rng() & 1));
        }
        Decoded dn = decode(noisy, D::Normal, false, 0, 1);
        toneOnly += ((dn.m_messages.size() >= 1) && matches(dn.m_messages[0], toneA[0]) && !dn.m_messages[0].m_truncated) ? 1 : 0;
    }
    std::printf("      %d/200 tone only pages followed by noise decoded as tone only\n", toneOnly);
    check(toneOnly >= 190, "Tone only page followed by noise is tone only (not data) in at least 95% of cases");

    // Voice and tone only
    roundTrip("Voice", { tx("100123", 3, G::Voice) });
    roundTrip("Tone only", { tx("100123", 4, G::Tone) });
    roundTrip("Numeric", { tx("100123", 1, G::Numeric, "0123456789 -U") });
    roundTrip("Alphanumeric", { tx("100123", 2, G::Alphanumeric, "HELLO WORLD 123") });
    std::string longText;
    while (longText.size() < 200) {
        longText += "THE QUICK BROWN FOX 0123456789 ";
    }
    roundTrip("Long alphanumeric", { tx("750123", 1, G::Alphanumeric, longText.c_str()) });
    roundTrip("Group 50-99 numeric", { tx("968001", 1, G::Numeric, "42") });
    check(!G::encode({ tx("100123", 1, G::Alphanumeric, std::string(300, 'A').c_str()) }).m_ok, "Too long message refused");
    check(!G::encode({ tx("100123", 5, G::Tone) }).m_ok, "Address number 5 refused");
    check(!G::encode({ tx("100123", 1, G::Numeric, "12?") }).m_ok, "Character not allowed in numeric refused");
}

void testNoFalseLock()
{
    std::mt19937 rng(11);
    D normal(D::Normal);
    D inverted(D::Inverted);
    D nbs(D::Normal);

    nbs.setNonBatterySaver(true);

    for (int i = 0; i < 300000; i++)
    {
        int bit = (int) (rng() & 1);
        normal.feed(bit);
        inverted.feed(bit);
        nbs.feed(bit);
    }
    // Alternating bits and a lot of equal bits
    for (int i = 0; i < 5000; i++)
    {
        normal.feed(i & 1);
        inverted.feed(i & 1);
    }
    for (int i = 0; i < 5000; i++)
    {
        normal.feed(1);
        inverted.feed(0);
    }

    check((normal.getStatus().m_transmissions == 0) && (inverted.getStatus().m_transmissions == 0) && (nbs.getStatus().m_transmissions == 0)
        && !normal.hasMessage() && !inverted.hasMessage() && !nbs.hasMessage(), "No lock on 300000 random bits, a comma and a constant");
}

// Decodes one alphanumeric message whose start code Word 1 and Word 2 get the given numbers
// of single symbol errors (the first symbol of a pair: with hard symbols the pair sums to
// zero and the first symbol decides, so it is a bit error) and of flipped symbol pairs
bool startCodeReceived(int singles1, int pairs1, int singles2, int pairs2)
{
    G::TxMessage message;
    message.m_address = "123456";
    message.m_type = G::Alphanumeric;
    message.m_text = "START";
    G::EncodeOptions options;
    options.m_mode = G::Single;
    G::EncodeResult result = G::encode({message}, options);
    std::vector<uint8_t> bits = result.m_bits;

    // Locate the start code: 46 bits of the duplicated codeword of the start code value
    uint32_t start = G::golayEncode(G::m_startCode);
    int offset = -1;
    for (int o = 0; (o + 46 <= (int) bits.size()) && (offset < 0); o++)
    {
        bool match = true;
        for (int i = 0; (i < 23) && match; i++) {
            match = (bits[o + 2 * i] == ((start >> i) & 1)) && (bits[o + 2 * i + 1] == ((start >> i) & 1));
        }
        if (match) {
            offset = o;
        }
    }
    if (offset < 0) {
        return false;
    }

    auto damage = [&](int base, int singles, int pairs) {
        for (int i = 0; i < singles; i++) {
            bits[base + 2 * (i * 5 % 23)] ^= 1;
        }
        for (int i = 0; i < pairs; i++)
        {
            int p = (i * 7 + 3) % 23;
            bits[base + 2 * p] ^= 1;
            bits[base + 2 * p + 1] ^= 1;
        }
    };
    damage(offset, singles1, pairs1);
    damage(offset + 47, singles2, pairs2);

    D decoder(D::Normal);
    for (uint8_t b : bits) {
        decoder.feed(b);
    }
    decoder.flush();

    bool found = false;
    while (decoder.hasMessage())
    {
        D::Message m = decoder.takeMessage();
        found = found || (m.m_address == "123456");
    }
    return found;
}

void testStartCodeCopies()
{
    check(startCodeReceived(0, 0, 0, 0), "Start code: clean Word 1 and Word 2");
    check(startCodeReceived(0, 3, 0, 3), "Start code: 3 flipped pairs in each word, both decode");
    check(startCodeReceived(3, 0, 0, 8), "Start code: 3 first symbol errors in Word 1, Word 2 destroyed, detected on Word 1");
    check(startCodeReceived(0, 0, 0, 8), "Start code: Word 2 destroyed (8 flipped pairs), detected on Word 1 alone");
    check(startCodeReceived(0, 8, 0, 0), "Start code: Word 1 destroyed (8 flipped pairs), detected on Word 2 alone");
    check(!startCodeReceived(0, 4, 0, 4), "Start code: both words beyond their code (4 flipped pairs each), rejected");

    // Every preamble value, single and batch: no rotation of a preamble word and no comma
    // on the word grid is taken for the start code, the message is found
    bool all = true;
    for (int index = 0; index < 10; index++)
    {
        for (int batch = 0; batch < 2; batch++)
        {
            char address[8];
            std::snprintf(address, sizeof(address), "%d00123", index);
            std::vector<G::TxMessage> sent = { tx(address, 2, G::Alphanumeric, "GRID") };
            G::EncodeOptions options;
            options.m_mode = batch ? G::Batch : G::Single;
            Decoded d = decode(G::encode(sent, options).m_bits, D::Normal, false, 0, 1);
            all = all && allMatch(d, sent) && (d.m_messages[0].m_preamble == index) && (d.m_messages[0].m_batch == (batch != 0));
        }
    }
    check(all, "Start code: found on the word grid after all 10 preamble values, single and batch");

    // Lock on a later preamble word: the first 10 preamble words are damaged
    std::vector<G::TxMessage> late = { tx("100123", 1, G::Numeric, "123") };
    G::EncodeResult enc = G::encode(late);
    for (int w = 0; w < 10; w++)
    {
        // 4 flipped pairs, at different bits in every word
        for (int p = 0; p < 4; p++)
        {
            int bit = (p * 5 + w * 7) % 23;
            enc.m_bits[28 + 46 * w + 2 * bit] ^= 1;
            enc.m_bits[28 + 46 * w + 2 * bit + 1] ^= 1;
        }
    }
    check(allMatch(decode(enc.m_bits, D::Normal, false, 0, 1), late), "Start code: found after a lock on the 13th preamble word");

    // One symbol of timing offset (a symbol lost before the start code): the start code
    // position is refined and the 600 bit/s blocks are still in step
    std::vector<G::TxMessage> slip = { tx("100123", 3, G::Alphanumeric, "TIMING OF THE BLOCKS") };
    G::EncodeResult encSlip = G::encode(slip);
    std::vector<uint8_t> lost(encSlip.m_bits);
    lost.erase(lost.begin() + 28 + 46 * 17 + 10);
    check(allMatch(decode(lost, D::Normal, false, 0, 1), slip), "Start code: one symbol lost in the last preamble word, blocks in step");
    std::vector<uint8_t> extra(encSlip.m_bits);
    extra.insert(extra.begin() + 28 + 46 * 17 + 10, extra[28 + 46 * 17 + 10]);
    check(allMatch(decode(extra, D::Normal, false, 0, 1), slip), "Start code: one symbol inserted in the last preamble word, blocks in step");
}

// Packs values of nbBits bits, least significant bit first, into message bits
std::vector<uint8_t> packValues(const std::vector<unsigned>& values, int nbBits, int& total)
{
    std::vector<uint8_t> packed((values.size() * nbBits + 7) / 8, 0);
    int pos = 0;

    for (unsigned v : values)
    {
        for (int j = 0; j < nbBits; j++, pos++)
        {
            if ((v >> j) & 1) {
                packed[pos >> 3] |= (uint8_t) (0x80 >> (pos & 7));
            }
        }
    }

    total = pos;
    return packed;
}

void testCharacterSets()
{
    int n;
    std::vector<uint8_t> p;
    std::string bar = G::tripleBar();

    p = packValues({ 1, 0xe, 2, 0xa, 0xa }, 4, n);
    check(G::decodeNumeric(p, n) == "1" + bar + "2", "Numeric 0xe is the triple bar, Null gives no character");
    p = packValues({ 1, 2, 3, 0xf }, 4, n);
    check(G::decodeNumeric(p, n) == "123E", "Numeric shift at the end is E");
    p = packValues({ 1, 0xf, 0xa, 0xa, 0xa }, 4, n);
    check(G::decodeNumeric(p, n) == "1E", "Numeric shift followed by Null (padding) is E");
    p = packValues({ 0xf, 0x0, 0xf, 0x5, 0xf, 0xe, 0xf, 0xf }, 4, n);
    check(G::decodeNumeric(p, n) == "A R<0x0F>", "Numeric shifted set: A, space, R and the spare as <0x0F>");
    p = packValues({ 0x21, 0x3c, 0x3f, 0x3e, 0x22, 0x3e, 0x3e }, 6, n);
    check(G::decodeAlphanumeric(p, n) == "A\xE2\x90\x8D_B", "Alphanumeric: the Table VII glyphs (0x3C CR/LF as the symbol for CR, 0x3F _), Null/EOM gives no character");

    // The status of each character: a code point each, c for the bits of a corrected codeword, X lost ('?')
    {
        std::string status;
        int m;
        p = packValues({ 1, 2, 3, 4, 0xe, 0xf, 0x0 }, 4, n);
        std::vector<uint8_t> corrected = packValues({ 0, 0xf, 0, 0, 0, 0, 0xf }, 4, m);
        std::vector<uint8_t> lost = packValues({ 0, 0, 0xf, 0, 0, 0, 0 }, 4, m);
        std::string text = G::decodeNumeric(p, n, &lost, &corrected, &status);
        check((text == "12?4" + bar + "A") && (status == ".cX..c"),
            "Numeric status: a digit corrected, one lost, the triple bar one character, a shifted digit as its worst nibble");

        p = packValues({ 0x21, 0x22, 0x23, 0x3c, 0x24 }, 6, n);
        corrected = packValues({ 0, 0x3f, 0, 0, 0 }, 6, m);
        lost = packValues({ 0, 0, 0, 0, 0x3f }, 6, m);
        text = G::decodeAlphanumeric(p, n, nullptr, &lost, &corrected, &status);
        check((text == "ABC\xE2\x90\x8D?") && (status == ".c..X"), "Alphanumeric status: one for each code point, the lost one X");

        G::Interpretation in = G::interpret(p, n, nullptr, &lost, &corrected);
        std::string numericStatus;
        G::decodeNumeric(p, n, &lost, &corrected, &numericStatus);
        check(in.m_status == ((in.m_type == "alphanumeric") ? std::string(".c..X") : numericStatus),
            "Interpretation: the status of the text chosen");
    }

    std::vector<G::TxMessage> sent = { tx("100123", 1, G::Numeric, ("12" + bar + "34").c_str()) };
    check(allMatch(decode(G::encode(sent).m_bits, D::Normal, false, 0, 1), sent), "Numeric triple bar in UTF-8 round trip");
    G::EncodeResult eq = G::encode({ tx("100123", 1, G::Numeric, "1=2*3") });
    Decoded deq = decode(eq.m_bits, D::Normal, false, 0, 1);
    check(!deq.m_messages.empty() && (G::decodeNumeric(deq.m_messages[0].m_rawData, deq.m_messages[0].m_rawBits) == "1" + bar + "2" + bar + "3"),
        "Numeric = and * are sent as the triple bar");
}

// Preamble, start code and address of a page (all 300 bit/s words) received through
// Gaussian noise, decided on the first symbol of each bit (hard) and on the sum of both
// symbols (soft)
// Voice audio as 600 baud symbols: the sign of three tones and noise
void appendAudio(std::vector<uint8_t>& bits, int symbols, unsigned seed)
{
    std::mt19937 rng(seed);
    std::normal_distribution<float> gauss(0.0f, 0.3f);
    const double pi = 3.14159265358979;
    double p1 = (rng() % 1000) * 0.006, p2 = (rng() % 1000) * 0.006, p3 = (rng() % 1000) * 0.006;

    for (int i = 0; i < symbols; i++)
    {
        double t = i / 600.0;
        double v = std::sin(2 * pi * 311 * t + p1) + 0.7 * std::sin(2 * pi * 527 * t + p2) + 0.5 * std::sin(2 * pi * 883 * t + p3) + gauss(rng);
        bits.push_back(v > 0 ? 1 : 0);
    }
}

Decoded decodeVoice(const std::vector<uint8_t>& bits, double timeout = 50.0)
{
    D decoder(D::Normal);
    Decoded out;

    decoder.setVoiceTimeout(timeout);
    for (uint8_t b : bits) {
        decoder.feed(b);
    }
    decoder.flush();
    while (decoder.hasMessage()) {
        out.m_messages.push_back(decoder.takeMessage());
    }

    return out;
}

bool voiceEnded(const Decoded& d, D::VoiceEnd end, float seconds, size_t total)
{
    if ((d.m_messages.size() != total) || (d.m_messages[0].m_type != G::Voice) || d.m_messages[0].m_update) {
        return false;
    }
    for (const D::Message& m : d.m_messages)
    {
        if (m.m_update) {
            return (m.m_type == G::Voice) && (m.m_voiceEnd == end) && (std::fabs(m.m_voiceSeconds - seconds) < 0.05f)
                && (m.m_address == d.m_messages[0].m_address);
        }
    }
    return false;
}

// Voice page: alert time, voice message, turn-off word (Motorola GSC manual, Figure 6-1)
void testVoice()
{
    std::vector<G::TxMessage> voice = { tx("100123", 3, G::Voice) };
    G::EncodeOptions single;
    single.m_mode = G::Single;
    std::vector<uint8_t> page = G::encode(voice, single).m_bits;

    std::vector<uint8_t> turnOff(page);
    appendAudio(turnOff, 1800, 1);
    G::appendActivationCode(turnOff);
    appendAudio(turnOff, 300, 2);
    Decoded d = decodeVoice(turnOff);
    check(voiceEnded(d, D::VoiceTurnOff, 3.0f, 2), "Voice: reported at once, ended by the turn-off word after 3 s");
    for (const D::Message& m : d.m_messages) {
        if (m.m_update) {
            std::printf("      voice end %d after %.3f s\n", (int) m.m_voiceEnd, m.m_voiceSeconds);
        }
    }

    // Turn-off word with Word 1 destroyed: Word 2 alone ends the voice message
    std::vector<uint8_t> word2Only(page);
    appendAudio(word2Only, 1200, 3);
    size_t at = word2Only.size();
    G::appendActivationCode(word2Only);
    for (int k = 0; k < 8; k++)
    {
        int bit = (k * 3 + 1) % 23;
        word2Only[at + 28 + 2 * bit] ^= 1;
        word2Only[at + 28 + 2 * bit + 1] ^= 1;
    }
    appendAudio(word2Only, 300, 4);
    check(voiceEnded(decodeVoice(word2Only), D::VoiceTurnOff, 2.0f, 2), "Voice: turn-off word found on Word 2 alone");

    // Tone only addresses interleaved in the alert time
    std::vector<uint8_t> interleaved(page);
    std::vector<G::TxMessage> tones;
    for (int i = 0; i < 8; i++)
    {
        char address[8];
        std::snprintf(address, sizeof(address), "1006%02d", i);
        tones.push_back(tx(address, 1 + (i & 3), G::Tone));
        int p;
        uint16_t w1, w2;
        G::encodeAddress(address, p, w1, w2);
        uint32_t first = G::golayEncode(w1);
        uint32_t second = G::golayEncode(w2);
        if (i & 2) {
            first ^= 0x7fffff;
        }
        if (i & 1) {
            second ^= 0x7fffff;
        }
        G::appendCodewordPair(interleaved, first, second);
    }
    appendAudio(interleaved, 1800, 5);
    G::appendActivationCode(interleaved);
    Decoded di = decodeVoice(interleaved);
    bool tonesOk = (di.m_messages.size() == 10);
    for (int i = 0; tonesOk && (i < 8); i++) {
        tonesOk = matches(di.m_messages[1 + i], tones[i]) && di.m_messages[1 + i].m_interleaved;
    }
    check(tonesOk && voiceEnded(di, D::VoiceTurnOff, (8 * 121 + 1800) / 600.0f, 10), "Voice: 8 tone only addresses interleaved in the alert time, then the turn-off word");

    // A new transmission ends the voice message
    std::vector<uint8_t> next(page);
    appendAudio(next, 2400, 6);
    std::vector<G::TxMessage> tone = { tx("100456", 2, G::Tone) };
    std::vector<uint8_t> second = G::encode(tone, single).m_bits;
    next.insert(next.end(), second.begin(), second.end());
    Decoded dn = decodeVoice(next);
    check(voiceEnded(dn, D::VoiceNewTransmission, 4.0f, 3) && matches(dn.m_messages[2], tone[0]), "Voice: ended by a new preamble, the next page is decoded");

    // Timeout and signal lost
    std::vector<uint8_t> longAudio(page);
    appendAudio(longAudio, 3000, 7);
    check(voiceEnded(decodeVoice(longAudio, 3.0), D::VoiceTimeout, 3.0f, 2), "Voice: ended by the timeout");
    std::vector<uint8_t> lost(page);
    appendAudio(lost, 900, 8);
    check(voiceEnded(decodeVoice(lost), D::VoiceSignalLost, 1.5f, 2), "Voice: signal lost (flush)");

    // 60 s of audio and of random symbols: nothing but the voice page and its timeout
    for (int kind = 0; kind < 2; kind++)
    {
        std::vector<uint8_t> quiet(page);
        if (kind == 0)
        {
            appendAudio(quiet, 36000, 9);
        }
        else
        {
            std::mt19937 rng(10);
            for (int i = 0; i < 36000; i++) {
                quiet.push_back((uint8_t) (rng() & 1));
            }
        }
        check(voiceEnded(decodeVoice(quiet), D::VoiceTimeout, 50.0f, 2),
            kind ? "Voice: 60 s of random symbols give no word, ended by the 50 s timeout" : "Voice: 60 s of voice audio give no word, ended by the 50 s timeout");
    }
}

// Pages decoded of trials, hard and soft, at a symbol noise level
void softTrials(float sigma, int trials, int& hardOk, int& softOk)
{
    std::mt19937 rng(17);
    std::normal_distribution<float> gauss(0.0f, sigma);
    std::vector<G::TxMessage> sent = { tx("100123", 2, G::Tone) };
    G::EncodeResult enc = G::encode(sent);

    hardOk = 0;
    softOk = 0;

    for (int t = 0; t < trials; t++)
    {
        D hard(D::Normal);
        D soft(D::Normal);

        for (uint8_t b : enc.m_bits)
        {
            float s = (b ? 1.0f : -1.0f) + gauss(rng);
            hard.feed(s > 0.0f ? 1 : 0);
            soft.feedSoft(s);
        }
        for (int i = 0; i < 200; i++)
        {
            float s = ((i & 1) ? 1.0f : -1.0f) + gauss(rng);
            hard.feed(s > 0.0f ? 1 : 0);
            soft.feedSoft(s);
        }
        hard.flush();
        soft.flush();

        Decoded dh, ds;
        while (hard.hasMessage()) {
            dh.m_messages.push_back(hard.takeMessage());
        }
        while (soft.hasMessage()) {
            ds.m_messages.push_back(soft.takeMessage());
        }
        // Only the words count: what follows the address is a 600 bit/s comma
        auto addressFound = [&](const Decoded& d) {
            return (d.m_messages.size() >= 1) && (d.m_messages[0].m_address == sent[0].m_address)
                && (d.m_messages[0].m_addressNumber == sent[0].m_addressNumber);
        };
        hardOk += addressFound(dh) ? 1 : 0;
        softOk += addressFound(ds) ? 1 : 0;
    }
}

void testSoftDecisions()
{
    const int trials = 200;
    int hardOk, softOk;
    std::mt19937 rng(19);

    for (float sigma : { 1.0f, 1.2f })
    {
        softTrials(sigma, trials, hardOk, softOk);
        std::printf("      symbol noise sigma %.1f: %d/%d hard, %d/%d soft\n", sigma, hardOk, trials, softOk, trials);
    }

    softTrials(0.8f, trials, hardOk, softOk);
    std::printf("      symbol noise sigma 0.8: %d/%d hard, %d/%d soft\n", hardOk, trials, softOk, trials);
    check(softOk > hardOk + trials / 4, "300 bit/s words decided on the sum of both soft symbols decode more pages than on one symbol");

    // Soft commas sum to noise: the fill commas of a batch and the noise after it never
    // give an address
    std::normal_distribution<float> light(0.0f, 0.3f);
    std::vector<G::TxMessage> batch = { tx("219789", 2, G::Numeric, "5551234"), tx("219456", 1, G::Tone), tx("219457", 4, G::Alphanumeric, "BATCH") };
    G::EncodeResult encBatch = G::encode(batch);
    int exact = 0;

    for (int t = 0; t < 200; t++)
    {
        D d(D::Normal);
        Decoded out;

        for (uint8_t b : encBatch.m_bits) {
            d.feedSoft((b ? 1.0f : -1.0f) + light(rng));
        }
        for (int i = 0; i < 2000; i++) {
            d.feedSoft(((rng() & 1) ? 1.0f : -1.0f) + light(rng));
        }
        d.flush();
        while (d.hasMessage()) {
            out.m_messages.push_back(d.takeMessage());
        }
        exact += allMatch(out, batch) ? 1 : 0;
    }
    check(exact == 200, "Soft symbols: 200 noisy batches give exactly their messages, nothing from the fill commas or the noise after");

    // Strong noise: count addresses that were never sent
    std::normal_distribution<float> strong(0.0f, 1.0f);
    int falseAddresses = 0;
    for (int t = 0; t < 500; t++)
    {
        D d(D::Normal);
        for (uint8_t b : encBatch.m_bits) {
            d.feedSoft((b ? 1.0f : -1.0f) + strong(rng));
        }
        for (int i = 0; i < 2000; i++) {
            d.feedSoft(((rng() & 1) ? 1.0f : -1.0f) + strong(rng));
        }
        d.flush();
        while (d.hasMessage())
        {
            D::Message m = d.takeMessage();
            bool known = false;
            for (const G::TxMessage& s : batch) {
                known = known || (m.m_address.substr(1) == s.m_address.substr(1));
            }
            falseAddresses += known ? 0 : 1;
        }
    }
    std::printf("      symbol noise sigma 1.0: %d addresses never sent in 500 batches\n", falseAddresses);
}

// The signal lost in a batch transmission and noise after (soft symbols spread as the discriminator spreads noise):
// for a cut at every half address word, no address that was not sent, and each message text, up to its first lost
// bit, is the start of the text sent
void testSignalLost()
{
    std::mt19937 rng(11);
    std::normal_distribution<float> clean(0.0f, 0.05f);
    std::normal_distribution<float> noise(0.0f, 1.5f);
    std::vector<G::TxMessage> batch = { tx("219789", 2, G::Numeric, "5551234"), tx("219456", 1, G::Tone),
        tx("219457", 4, G::Alphanumeric, "GSC SWEEP") };
    G::EncodeResult enc = G::encode(batch);
    int foreign = 0, wrongText = 0, cuts = 0;

    for (size_t cut = enc.m_bits.size() / 4; cut <= enc.m_bits.size(); cut += 23)
    {
        D d(D::Normal);

        for (size_t i = 0; i < cut; i++) {
            d.feedSoft((enc.m_bits[i] ? 1.0f : -1.0f) + clean(rng));
        }
        for (int i = 0; i < 3000; i++) {
            d.feedSoft(noise(rng));
        }
        d.flush();
        cuts++;

        while (d.hasMessage())
        {
            D::Message m = d.takeMessage();
            const G::TxMessage *sent = nullptr;

            for (const G::TxMessage& s : batch)
            {
                if (m.m_address.substr(1) == s.m_address.substr(1)) {
                    sent = &s;
                }
            }
            if (!sent)
            {
                foreign++;
                continue;
            }
            if ((m.m_type != G::Data) || (sent->m_type == G::Tone)) {
                continue;
            }

            // The bits up to the first lost one
            int bits = m.m_rawBits;
            for (int i = 0; i < m.m_rawBits; i++)
            {
                if (m.m_rawLost[i >> 3] & (0x80 >> (i & 7)))
                {
                    bits = i;
                    break;
                }
            }
            std::string text = (sent->m_type == G::Numeric) ? G::decodeNumeric(m.m_rawData, bits) : G::decodeAlphanumeric(m.m_rawData, bits);
            if (!text.empty()) {
                text.pop_back();    // The last character may be cut
            }
            wrongText += (sent->m_text.compare(0, text.size(), text) != 0) ? 1 : 0;
        }
    }

    std::printf("      signal lost at %d points: %d addresses never sent, %d messages with a wrong text\n", cuts, foreign, wrongText);
    check(foreign == 0, "Signal lost in a transmission, noise after: no address that was not sent");
    check(wrongText == 0, "Signal lost in a transmission, noise after: nothing wrong is added to a message without a mark");
}

// FM discriminator output for a bit stream, binary 1 = positive deviation, between random symbols
std::vector<float> makeWaveform(const std::vector<uint8_t>& bits, double sampleRate, bool inverted, float dc, float noise, int lead, int tail, unsigned seed)
{
    std::mt19937 rng(seed);
    std::normal_distribution<float> gauss(0.0f, noise);
    std::vector<float> levels;

    for (int i = 0; i < lead; i++) {
        levels.push_back((rng() & 1) ? 1.0f : -1.0f);
    }
    for (uint8_t b : bits)
    {
        float level = b ? 1.0f : -1.0f;
        levels.push_back(inverted ? -level : level);
    }
    for (int i = 0; i < tail; i++) {
        levels.push_back((rng() & 1) ? 1.0f : -1.0f);
    }

    double samplesPerSymbol = sampleRate / RadioPagingGscReceiver::m_baud;
    float alpha = (float) (1.0 - std::exp(-1.0 / (0.1 * samplesPerSymbol)));
    std::vector<float> samples;
    float smoothed = 0.0f;

    for (double t = 0.0; (size_t) (t / samplesPerSymbol) < levels.size(); t += 1.0)
    {
        smoothed += alpha * (levels[(size_t) (t / samplesPerSymbol)] - smoothed);
        samples.push_back(smoothed + dc + gauss(rng));
    }

    return samples;
}

void receive(const std::vector<G::TxMessage>& sent, G::Mode mode, bool inverted, const char *name)
{
    G::EncodeOptions options;
    options.m_mode = mode;
    std::vector<float> samples = makeWaveform(G::encode(sent, options).m_bits, 38400.0, inverted, 0.06f, 0.15f, 300, 300, 23);
    RadioPagingGscReceiver receiver;
    Decoded d;
    bool locked = false;

    receiver.create(38400.0);
    for (float s : samples)
    {
        receiver.process(s);
        locked = locked || receiver.getStatus().m_locked;
    }
    receiver.flush();

    bool polarityOk = true;
    while (receiver.hasMessage())
    {
        D::Message m = receiver.takeMessage();
        polarityOk = polarityOk && (m.m_polarity == (inverted ? D::Inverted : D::Normal));
        d.m_messages.push_back(m);
    }

    std::string label = std::string("Receiver: ") + name + (inverted ? ", inverted" : ", normal");
    check(locked, (label + ": the demodulator locks on the preamble").c_str());
    check(allMatch(d, sent) && polarityOk, (label + ": all messages, from the decoder of the right polarity").c_str());
}

// Non battery saver transmission through the receiver with both polarities enabled
Decoded receiveNbs(const std::vector<G::TxMessage>& sent, bool inverted)
{
    G::EncodeOptions options;
    options.m_mode = G::NonBatterySaver;
    std::vector<float> samples = makeWaveform(G::encode(sent, options).m_bits, 38400.0, inverted, 0.06f, 0.15f, 300, 300, 29);
    RadioPagingGscReceiver receiver;
    Decoded d;

    receiver.create(38400.0);
    receiver.setNonBatterySaver(true);
    for (float s : samples) {
        receiver.process(s);
    }
    receiver.flush();
    while (receiver.hasMessage()) {
        d.m_messages.push_back(receiver.takeMessage());
    }

    return d;
}

void testReceiverNonBatterySaver()
{
    // Consecutive calls: the data page tells the polarity of the whole transmission
    std::vector<G::TxMessage> calls = { tx("500123", 2, G::Tone), tx("500456", 3, G::Tone), tx("500789", 1, G::Alphanumeric, "POLARITY") };
    Decoded n = receiveNbs(calls, false);
    Decoded i = receiveNbs(calls, true);
    check(allMatch(n, calls) && (n.m_messages[0].m_polarity == D::Normal), "Receiver, non battery saver: tone, tone, data, once each, normal polarity");
    check(allMatch(i, calls) && (i.m_messages[0].m_polarity == D::Inverted),
        "Receiver, non battery saver inverted: the block check of the data page decides, tone pages get their right address numbers");

    // Voice: the activation code decides
    std::vector<G::TxMessage> voice = { tx("500123", 3, G::Voice) };
    Decoded vi = receiveNbs(voice, true);
    check(allMatch(vi, voice) && (vi.m_messages[0].m_polarity == D::Inverted), "Receiver, non battery saver inverted voice: the activation code decides");

    // Tone only: nothing tells the polarity, a positive shift is a binary 1 (normal)
    std::vector<G::TxMessage> tone = { tx("500123", 2, G::Tone) };
    Decoded tn = receiveNbs(tone, false);
    Decoded ti = receiveNbs(tone, true);
    check(allMatch(tn, tone) && (ti.m_messages.size() == 1) && (ti.m_messages[0].m_polarity == D::Normal),
        "Receiver, non battery saver tone only: one message, from the normal decoder");
}

void testReceiverVoice()
{
    std::vector<G::TxMessage> voice = { tx("100123", 3, G::Voice) };
    G::EncodeOptions single;
    single.m_mode = G::Single;
    std::vector<uint8_t> bits = G::encode(voice, single).m_bits;
    appendAudio(bits, 3000, 11);
    G::appendActivationCode(bits);
    std::vector<G::TxMessage> tone = { tx("100456", 2, G::Tone) };
    std::vector<uint8_t> second = G::encode(tone, single).m_bits;
    bits.insert(bits.end(), second.begin(), second.end());

    for (int inverted = 0; inverted < 2; inverted++)
    {
        std::vector<float> samples = makeWaveform(bits, 38400.0, inverted != 0, 0.06f, 0.15f, 300, 300, 31);
        RadioPagingGscReceiver receiver;
        Decoded d;
        receiver.create(38400.0);
        for (float s : samples) {
            receiver.process(s);
        }
        receiver.flush();
        while (receiver.hasMessage()) {
            d.m_messages.push_back(receiver.takeMessage());
        }
        bool ok = (d.m_messages.size() == 3) && voiceEnded(d, D::VoiceTurnOff, 5.0f, 3) && matches(d.m_messages[2], tone[0]);
        check(ok, inverted ? "Receiver, inverted: voice page, 5 s of audio, turn-off word, next page" : "Receiver: voice page, 5 s of audio, turn-off word, next page");
    }
}

// The same batch twice: the pages of the second copy are marked duplicate; on the other
// polarity (another network) they are not
void testReceiverDuplicates()
{
    std::vector<G::TxMessage> batch = {
        tx("219789", 2, G::Numeric, "5551234"),
        tx("219456", 1, G::Tone),
        tx("219457", 4, G::Alphanumeric, "BATCH"),
    };
    G::EncodeOptions options;
    options.m_mode = G::Batch;
    std::vector<uint8_t> bits = G::encode(batch, options).m_bits;
    std::vector<float> parts[3] = {
        makeWaveform(bits, 38400.0, false, 0.06f, 0.15f, 300, 3000, 41),
        makeWaveform(bits, 38400.0, false, 0.06f, 0.15f, 300, 3000, 42),
        makeWaveform(bits, 38400.0, true, 0.06f, 0.15f, 300, 3000, 43)
    };
    RadioPagingGscReceiver receiver;
    std::vector<D::Message> out[3];

    receiver.create(38400.0);
    for (int p = 0; p < 3; p++)
    {
        for (float s : parts[p])
        {
            receiver.process(s);
            while (receiver.hasMessage()) {
                out[p].push_back(receiver.takeMessage());
            }
        }
    }

    bool firstNew = (out[0].size() == batch.size());
    bool againDup = (out[1].size() == batch.size());
    bool otherNew = (out[2].size() == batch.size());

    for (const auto& m : out[0]) { firstNew = firstNew && !m.m_duplicate; }
    // A tone only page has nothing a pager stores: it is not a duplicate
    for (const auto& m : out[1]) { againDup = againDup && (m.m_duplicate == (m.m_rawBits > 0)); }
    for (const auto& m : out[2]) { otherNew = otherNew && !m.m_duplicate; }

    check(firstNew, "Receiver duplicates: first copy of the batch is new");
    check(againDup, "Receiver duplicates: second copy of the batch is output again, marked duplicate (tone only pages are not)");
    check(otherNew, "Receiver duplicates: the same batch on the other polarity is not a duplicate");
}

void testReceiver()
{
    testReceiverNonBatterySaver();
    testReceiverVoice();
    testReceiverDuplicates();

    std::vector<G::TxMessage> single = { tx("100123", 3, G::Alphanumeric, "RECEIVED OVER THE AIR") };
    std::vector<G::TxMessage> batch = {
        tx("219789", 2, G::Numeric, "5551234"),
        tx("219456", 1, G::Tone),
        tx("219457", 4, G::Alphanumeric, "BATCH"),
    };

    for (int inverted = 0; inverted < 2; inverted++)
    {
        receive(single, G::Single, inverted != 0, "single alphanumeric");
        receive(batch, G::Batch, inverted != 0, "batch of 3");
    }
}

} // namespace

int main()
{
    // Address of the message records: 6 characters (zero padded, N index of non battery saver) and the number 1-4
    check((RadioPagingGsc::addressText("123456", 2) == "123456/2") && (RadioPagingGsc::addressText("42", 1) == "000042/1")
        && (RadioPagingGsc::addressText("N23456", 4) == "N23456/4"), "record address: 6 characters, / and the address number");
    // Address number 1-4 and function A-D are the same four address word combinations
    check((RadioPagingGsc::functionLetter(1) == 'A') && (RadioPagingGsc::functionLetter(4) == 'D')
        && (RadioPagingGsc::functionLetter(0) == 0) && (RadioPagingGsc::functionLetter(5) == 0),
        "function letter of the address number");
    check((RadioPagingGsc::addressNumber('1') == 1) && (RadioPagingGsc::addressNumber('4') == 4)
        && (RadioPagingGsc::addressNumber('B') == 2) && (RadioPagingGsc::addressNumber('d') == 4)
        && (RadioPagingGsc::addressNumber('0') == 0) && (RadioPagingGsc::addressNumber('5') == 0)
        && (RadioPagingGsc::addressNumber('E') == 0), "address number of a number or a function letter");

    // Cap codes: the suffix digit gives the address number (1-4 and 5-8: 1-4; 9 and 0: 1 and 2)
    const char suffixes[] = "1234567890";
    const int numbers[] = {1, 2, 3, 4, 1, 2, 3, 4, 1, 2};
    bool suffixOk = (RadioPagingGsc::suffixAddressNumber('x') == 0);

    for (int i = 0; i < 10; i++) {
        suffixOk = suffixOk && (RadioPagingGsc::suffixAddressNumber(suffixes[i]) == numbers[i]);
    }

    check(suffixOk, "suffix digit: address number");
    std::string capAddress;
    int capNumber = -1;
    check(RadioPagingGsc::parseCapCode("1234567", capAddress, capNumber) && (capAddress == "123456") && (capNumber == 3),
        "7 digit cap code: the address and the number of the suffix");
    check(RadioPagingGsc::parseCapCode("123456", capAddress, capNumber) && (capAddress == "123456") && (capNumber == 0),
        "6 digit cap code: no suffix");
    check(RadioPagingGsc::parseCapCode("23456", capAddress, capNumber) && (capAddress == "023456") && (capNumber == 0),
        "5 digit cap code: index 0");
    check(!RadioPagingGsc::parseCapCode("12345678", capAddress, capNumber) && !RadioPagingGsc::parseCapCode("12a456", capAddress, capNumber)
        && !RadioPagingGsc::parseCapCode("", capAddress, capNumber) && !RadioPagingGsc::parseCapCode("1000001", capAddress, capNumber),
        "cap codes refused: 8 digits, not a digit, empty, address digits not allowed");
    int capPreamble;
    uint16_t capWord1, capWord2;
    RadioPagingGsc::encodeAddress("123456", capPreamble, capWord1, capWord2);
    check(RadioPagingGsc::capCodeText("1234567") == "index 1, group 23, address 456, suffix 7 = 3 (C) -> preamble 4, word 1 "
        + std::to_string(capWord1) + ", word 2 " + std::to_string(capWord2), "cap code text");
    check(RadioPagingGsc::capCodeText("23456").find("index 0 (padded), group 23, address 456 -> preamble 3") == 0,
        "cap code text: 5 digits");
    testCodes();
    testStartCodeCopies();
    testCharacterSets();
    testVoice();
    testSoftDecisions();
    testSignalLost();
    testReceiver();
    testAddresses();
    testStreaming();
    testErrors();
    testBatches();
    testNonBatterySaver();
    testInterpretation();
    testNoFalseLock();

    std::printf("%d passed, %d failed\n", passes, failures);
    return failures ? 1 : 0;
}
