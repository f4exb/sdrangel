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

// Self test of the modemradiopaging primitives. Prints one line per check and
// returns non-zero if any check fails.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

#include <string>

#include "radiopagingbch.h"
#include "radiopagingdedup.h"
#include "radiopagingfskdemod.h"
#include "radiopagingpocsag.h"
#include "radiopagingpocsagdecoder.h"
#include "radiopagingpocsagreceiver.h"

namespace {

int failures = 0;

void check(bool ok, const char *what)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) {
        failures++;
    }
}

void testBCH()
{
    std::mt19937 rng(1);
    bool encodeOk = true;
    bool singleOk = true;
    bool doubleOk = true;
    bool parityOk = true;

    for (int n = 0; n < 200; n++)
    {
        uint32_t data = rng() & 0x1fffff;
        uint32_t word = RadioPagingBCH::encode(data);

        RadioPagingBCH::Result clean = RadioPagingBCH::decode(word);
        if ((RadioPagingBCH::syndrome(word) != 0) || (RadioPagingBCH::data(word) != data)
            || (clean.m_bitErrors != 0) || clean.m_parityError) {
            encodeOk = false;
        }

        for (int i = 1; i < 32; i++)
        {
            RadioPagingBCH::Result r1 = RadioPagingBCH::decode(word ^ (1u << i));
            if ((r1.m_word != word) || (r1.m_bitErrors != 1)) {
                singleOk = false;
            }

            for (int j = i + 1; j < 32; j++)
            {
                RadioPagingBCH::Result r2 = RadioPagingBCH::decode(word ^ (1u << i) ^ (1u << j));
                if ((r2.m_word != word) || (r2.m_bitErrors != 2)) {
                    doubleOk = false;
                }
            }
        }

        RadioPagingBCH::Result rp = RadioPagingBCH::decode(word ^ 1u);
        if ((rp.m_word != word) || !rp.m_parityError || (rp.m_bitErrors != 0)) {
            parityOk = false;
        }
    }

    // One error in bits 31-1 and the parity bit: two errors, corrected. Every pattern of three errors
    // in the 32 bits is detected (minimum distance 6 with the parity bit)
    bool oneAndParityOk = true;
    bool tripleOk = true;

    for (int n = 0; n < 20; n++)
    {
        uint32_t word = RadioPagingBCH::encode(rng() & 0x1fffff);

        for (int i = 1; i < 32; i++)
        {
            RadioPagingBCH::Result r = RadioPagingBCH::decode(word ^ (1u << i) ^ 1u);
            if ((r.m_word != word) || (r.m_bitErrors != 1) || !r.m_parityError) {
                oneAndParityOk = false;
            }
        }

        for (int i = 0; i < 32; i++)
        {
            for (int j = i + 1; j < 32; j++)
            {
                for (int k = j + 1; k < 32; k++)
                {
                    uint32_t bad = word ^ (1u << i) ^ (1u << j) ^ (1u << k);
                    RadioPagingBCH::Result r = RadioPagingBCH::decode(bad);
                    if ((r.m_bitErrors != -1) || (r.m_word != bad)) {
                        tripleOk = false;
                    }
                }
            }
        }
    }

    check(encodeOk, "BCH encode gives zero syndrome, even parity and the data back");
    check(singleOk, "BCH corrects every single bit error in bits 31-1");
    check(doubleOk, "BCH corrects every double bit error in bits 31-1");
    check(parityOk, "BCH reports a parity bit error and repairs it");
    check(oneAndParityOk, "BCH corrects one bit error in bits 31-1 with a parity bit error");
    check(tripleOk, "BCH reports every three bit error pattern as uncorrectable");
    // Addresses of the message records, the same in RX, TX and the message center
    check((RadioPagingPocsag::addressText(1234567, 2) == "1234567C") && (RadioPagingPocsag::addressText(8, 0) == "0000008A"),
        "POCSAG record address: 7 digits and the function letter");
    check(RadioPagingBCH::reverse(0x80000001u) == 0x80000001u
        && RadioPagingBCH::reverse(0x00000002u) == 0x40000000u, "BCH bit reverse");
}

// Generates FM discriminator output for random symbols with smoothed transitions,
// DC offset, a symbol clock error and Gaussian noise, demodulates it and counts
// symbol errors after acquisition
int runFsk(double sampleRate, double symbolRate, int levels, double clockError, float noise, float dc, int nbSymbols)
{
    std::mt19937 rng(levels * 1000 + (int) symbolRate);
    std::normal_distribution<float> gauss(0.0f, noise);
    const float levelValues4[4] = {-1.0f, -1.0f/3.0f, 1.0f/3.0f, 1.0f};

    std::vector<int> sent(nbSymbols);
    for (auto& s : sent) {
        s = (int) (rng() % (unsigned) levels);
    }

    RadioPagingFskDemod demod;
    demod.create(sampleRate, symbolRate, levels);
    std::vector<int> received;

    // A 4-level demodulator is given the levels measured on the 2-level sync before it (FLEX
    // sync 1); 2-level hunts with the means of the symbols
    if (levels == 4)
    {
        demod.setLevels(dc, 1.0f);
        demod.setHoldLevels(true);
    }

    double txSamplesPerSymbol = sampleRate / (symbolRate * (1.0 + clockError));
    double t = 0.0;
    float smoothed = 0.0f;
    // One pole smoothing, time constant of a tenth of a symbol, to give finite rise times
    float alpha = (float) (1.0 - std::exp(-1.0 / (0.1 * txSamplesPerSymbol)));
    int nbSamples = (int) (nbSymbols * txSamplesPerSymbol);

    for (int n = 0; n < nbSamples; n++)
    {
        int index = (int) (t / txSamplesPerSymbol);
        if (index >= nbSymbols) {
            break;
        }
        float target = (levels == 4) ? levelValues4[sent[index]] : (sent[index] ? 1.0f : -1.0f);
        smoothed += alpha * (target - smoothed);
        t += 1.0;

        RadioPagingFskDemod::Symbol symbol;
        if (demod.process(smoothed + dc + gauss(rng), symbol)) {
            received.push_back(symbol.m_value);
        }
    }

    // Find the alignment with fewest errors, ignoring the first symbols (acquisition)
    const int skip = 200;
    int best = nbSymbols;

    for (int lag = -4; lag <= 4; lag++)
    {
        int errors = 0;
        int compared = 0;

        for (int i = skip; i < (int) received.size(); i++)
        {
            int j = i + lag;
            if ((j < 0) || (j >= nbSymbols)) {
                continue;
            }
            compared++;
            if (received[i] != sent[j]) {
                errors++;
            }
        }

        if ((compared > nbSymbols / 2) && (errors < best)) {
            best = errors;
        }
    }

    return best;
}

void testFsk()
{
    char what[160];
    struct Case { double fs; double rate; int levels; };
    const Case cases[] = {
        {38400.0, 512.0, 2},
        {38400.0, 600.0, 2},
        {38400.0, 1200.0, 2},
        {38400.0, 1600.0, 2},
        {38400.0, 2400.0, 2},
        {38400.0, 3200.0, 2},
        {48000.0, 512.0, 2},      // 93.75 samples per symbol
        {38400.0, 1600.0, 4},
        {38400.0, 3200.0, 4},
        {44100.0, 3200.0, 4}      // 13.78 samples per symbol
    };

    for (const auto& c : cases)
    {
        int errors = runFsk(c.fs, c.rate, c.levels, 200e-6, 0.15f, 0.1f, 4000);
        std::snprintf(what, sizeof(what), "FSK %d-level %.0f sym/s at %.0f Hz, 200 ppm clock error, DC 0.1, noise 0.15: %d symbol errors",
            c.levels, c.rate, c.fs, errors);
        check(errors == 0, what);
    }
}

// FM discriminator output for a bit stream: binary 1 is a negative deviation for normal
// polarity. Lead and tail are random symbols before and after the transmission.
std::vector<float> makeWaveform(const std::vector<uint8_t>& bits, double sampleRate, double baud,
                                bool inverted, float dc, float noise, int lead, int tail, unsigned seed)
{
    std::mt19937 rng(seed);
    std::normal_distribution<float> gauss(0.0f, noise);
    std::vector<float> levels;

    for (int i = 0; i < lead; i++) {
        levels.push_back((rng() & 1) ? 1.0f : -1.0f);
    }
    for (uint8_t b : bits)
    {
        float level = b ? -1.0f : 1.0f;
        levels.push_back(inverted ? -level : level);
    }
    for (int i = 0; i < tail; i++) {
        levels.push_back((rng() & 1) ? 1.0f : -1.0f);
    }

    double samplesPerSymbol = sampleRate / baud;
    float alpha = (float) (1.0 - std::exp(-1.0 / (0.1 * samplesPerSymbol)));
    std::vector<float> samples;
    float smoothed = 0.0f;
    double t = 0.0;

    for (;;)
    {
        size_t index = (size_t) (t / samplesPerSymbol);
        if (index >= levels.size()) {
            break;
        }
        smoothed += alpha * (levels[index] - smoothed);
        samples.push_back(smoothed + dc + gauss(rng));
        t += 1.0;
    }

    return samples;
}

bool sameMessages(const std::vector<RadioPagingPocsagDecoder::Message>& rx, const std::vector<RadioPagingPocsag::TxMessage>& tx, std::string& why);
std::vector<RadioPagingPocsag::TxMessage> pocsagTestMessages();

// The receiver of one speed, messages split by polarity
struct PocsagLeg
{
    RadioPagingPocsagReceiver m_receiver;
    std::vector<RadioPagingPocsagDecoder::Message> m_normalMessages;
    std::vector<RadioPagingPocsagDecoder::Message> m_invertedMessages;
    bool m_lockedInPreamble = false;

    PocsagLeg(double sampleRate, double baud) {
        m_receiver.create(sampleRate, (int) baud);
    }

    void process(float sample)
    {
        m_receiver.process(sample);

        RadioPagingPocsagReceiver::Status status = m_receiver.getStatus();
        if (status.m_preamble && status.m_locked) {
            m_lockedInPreamble = true;
        }

        while (m_receiver.hasMessage())
        {
            RadioPagingPocsagDecoder::Message m = m_receiver.takeMessage();
            if (m.m_polarity == RadioPagingPocsagDecoder::Inverted) {
                m_invertedMessages.push_back(m);
            } else {
                m_normalMessages.push_back(m);
            }
        }
    }
};

// Transmission with only the last preambleBits of the preamble, received at one speed
bool decodedWithShortPreamble(int preambleBits, double baud, bool& lockedInPreamble)
{
    std::vector<RadioPagingPocsag::TxMessage> messages = pocsagTestMessages();
    RadioPagingPocsag::EncodeResult result = RadioPagingPocsag::encode(messages);
    std::vector<uint8_t> bits(result.m_bits.begin() + (RadioPagingPocsag::m_preambleBits - preambleBits), result.m_bits.end());
    std::vector<float> samples = makeWaveform(bits, 38400.0, baud, false, 0.08f, 0.12f, 300, 300, 21);
    PocsagLeg leg(38400.0, baud);

    for (float s : samples) {
        leg.process(s);
    }

    lockedInPreamble = leg.m_lockedInPreamble;
    std::string why;
    return sameMessages(leg.m_normalMessages, messages, why);
}

void testPocsagPreambleLock()
{
    const double bauds[3] = {512.0, 1200.0, 2400.0};
    char what[160];

    for (double baud : bauds)
    {
        bool locked = false;
        bool ok = decodedWithShortPreamble(RadioPagingPocsag::m_preambleBits, baud, locked);
        std::snprintf(what, sizeof(what), "POCSAG %.0f baud: the demodulator locks on the preamble", baud);
        check(locked, what);
        std::snprintf(what, sizeof(what), "POCSAG %.0f baud: full preamble, all messages", baud);
        check(ok, what);

        ok = decodedWithShortPreamble(64, baud, locked);
        std::snprintf(what, sizeof(what), "POCSAG %.0f baud: only 64 bits of preamble received, locked %s, all messages %s",
            baud, locked ? "yes" : "no", ok ? "yes" : "no");
        check(ok, what);

        ok = decodedWithShortPreamble(0, baud, locked);
        std::snprintf(what, sizeof(what), "POCSAG %.0f baud: no preamble at all, all messages %s", baud, ok ? "yes" : "no");
        check(ok, what);
    }

    // A transmission of one batch (no sync codeword after it) whose preamble has bit errors in its last bits, and a
    // message codeword that needs a correction: the batch still follows a preamble, the message is decoded whole (the
    // rule for a sync codeword alone would end it at the corrected codeword)
    for (double baud : bauds)
    {
        std::vector<RadioPagingPocsag::TxMessage> messages(1);
        messages[0].m_address = 1100105;
        messages[0].m_function = 3;
        messages[0].m_type = RadioPagingPocsag::Alphanumeric;
        messages[0].m_text = "RP SENSITIVITY 05 ABCDEFGHIJKLMNOP";
        std::vector<uint8_t> bits = RadioPagingPocsag::encode(messages).m_bits;
        const int preamble = RadioPagingPocsag::m_preambleBits;
        bits[preamble - 3] ^= 1;
        bits[preamble - 11] ^= 1;
        bits[preamble + 32 + 32 * 6 + 9] ^= 1;
        std::vector<float> samples = makeWaveform(bits, 38400.0, baud, false, 0.08f, 0.12f, 300, 300, 21);
        PocsagLeg leg(38400.0, baud);

        for (float s : samples) {
            leg.process(s);
        }

        std::string why;
        std::snprintf(what, sizeof(what), "POCSAG %.0f baud: one batch, bit errors at the end of the preamble and in a codeword: decoded whole", baud);
        check(sameMessages(leg.m_normalMessages, messages, why), what);
    }
}

bool sameMessages(const std::vector<RadioPagingPocsagDecoder::Message>& rx, const std::vector<RadioPagingPocsag::TxMessage>& tx, std::string& why)
{
    if (rx.size() != tx.size())
    {
        why = "decoded " + std::to_string(rx.size()) + " messages, sent " + std::to_string(tx.size()) + " (";
        for (const auto& r : rx) {
            why += std::to_string(r.m_address) + "/" + std::to_string(r.m_rawBits) + "b ";
        }
        why += ")";
        return false;
    }

    for (size_t i = 0; i < tx.size(); i++)
    {
        const auto& r = rx[i];
        const auto& t = tx[i];
        RadioPagingPocsag::Interpretation interp = RadioPagingPocsag::interpret(r.m_rawData, r.m_rawBits);
        std::string expectedType = (t.m_type == RadioPagingPocsag::Tone) ? "tone"
            : (t.m_type == RadioPagingPocsag::Numeric ? "numeric" : "alphanumeric");

        if ((r.m_address != t.m_address) || (r.m_function != t.m_function)
            || (interp.m_type != expectedType) || (interp.m_text != t.m_text))
        {
            why = "message " + std::to_string(i) + " differs: address " + std::to_string(r.m_address)
                + " function " + std::to_string(r.m_function) + " type " + interp.m_type
                + " text '" + RadioPagingPocsag::escapeControl(interp.m_text) + "'";
            return false;
        }
    }

    return true;
}

std::vector<RadioPagingPocsag::TxMessage> pocsagTestMessages()
{
    std::vector<RadioPagingPocsag::TxMessage> messages;
    RadioPagingPocsag::TxMessage m;

    m.m_address = 1234567; m.m_function = 0; m.m_type = RadioPagingPocsag::Numeric; m.m_text = "0123456789 -";
    messages.push_back(m);
    m.m_address = 1005432; m.m_function = 2; m.m_type = RadioPagingPocsag::Alphanumeric; m.m_text = "Hello World 123";
    messages.push_back(m);
    m.m_address = 1005433; m.m_function = 1; m.m_type = RadioPagingPocsag::Tone; m.m_text = "";
    messages.push_back(m);
    m.m_address = 1005440; m.m_function = 3; m.m_type = RadioPagingPocsag::Alphanumeric;
    m.m_text = "";
    for (int i = 0; i < 150; i++) {
        m.m_text.push_back((char) ('a' + (i % 26)));
    }
    messages.push_back(m);
    m.m_address = 100; m.m_function = 0; m.m_type = RadioPagingPocsag::Alphanumeric; m.m_text = "A\r\nB";
    messages.push_back(m);

    return messages;
}

void testPocsagCodec()
{
    std::vector<RadioPagingPocsag::TxMessage> messages = pocsagTestMessages();
    RadioPagingPocsag::EncodeResult result = RadioPagingPocsag::encode(messages);

    check(result.m_ok, "POCSAG encode succeeds");
    check(result.m_codewords.size() % 17 == 0, "POCSAG output is a whole number of 17 codeword batches");
    check(result.m_bits.size() == (size_t) (576 + 32 * result.m_codewords.size()), "POCSAG output bit count is preamble plus codewords");

    bool syncOk = true;
    for (size_t i = 0; i < result.m_codewords.size(); i += 17) {
        syncOk = syncOk && (result.m_codewords[i] == RadioPagingPocsag::m_syncWord);
    }
    check(syncOk, "POCSAG batches start with the sync codeword");

    // The address of the first message is in frame 7, so slot 14 or 15 of the first batch
    uint32_t first = result.m_codewords[1 + 14];
    check((first & 0x80000000u) == 0 && (RadioPagingBCH::data(first) >> 2) == (1234567u >> 3), "POCSAG address is placed in its frame");

    RadioPagingPocsag::TxMessage bad;
    bad.m_address = 5; bad.m_type = RadioPagingPocsag::Numeric; bad.m_text = "12a";
    check(!RadioPagingPocsag::encode({bad}).m_ok, "POCSAG rejects a character that is not numeric");
    bad.m_address = 2007664; bad.m_text = "12";
    check(!RadioPagingPocsag::encode({bad}).m_ok, "POCSAG rejects an address that looks like the idle codeword");
    check(RadioPagingPocsag::escapeControl(std::string("A\r\n\x7f") + "B") == "A<0x0D><0x0A><0x7F>B", "control characters are escaped");
}

void testPocsagLink()
{
    std::vector<RadioPagingPocsag::TxMessage> messages = pocsagTestMessages();
    RadioPagingPocsag::EncodeResult clean = RadioPagingPocsag::encode(messages);
    const double fs = 38400.0;
    const double bauds[3] = {512.0, 1200.0, 2400.0};
    char what[200];

    // One bit error in every codeword, in different positions, the sync codewords included
    std::vector<uint8_t> damaged = clean.m_bits;
    for (size_t i = 0; i < clean.m_codewords.size(); i++) {
        damaged[576 + 32 * i + (i * 7) % 32] ^= 1;
    }

    for (int variant = 0; variant < 2; variant++)
    {
        for (int pol = 0; pol < 2; pol++)
        {
            for (int s = 0; s < 3; s++)
            {
                bool inverted = (pol == 1);
                const std::vector<uint8_t>& bits = (variant == 0) ? clean.m_bits : damaged;
                std::vector<float> samples = makeWaveform(bits, fs, bauds[s], inverted, 0.08f, 0.12f, 300, 300, 7 + s);

                // All three speeds listen to the same signal
                PocsagLeg legs[3] = {PocsagLeg(fs, bauds[0]), PocsagLeg(fs, bauds[1]), PocsagLeg(fs, bauds[2])};
                for (float sample : samples) {
                    for (auto& leg : legs) {
                        leg.process(sample);
                    }
                }

                std::string why;
                const auto& right = inverted ? legs[s].m_invertedMessages : legs[s].m_normalMessages;
                const auto& wrongPol = inverted ? legs[s].m_normalMessages : legs[s].m_invertedMessages;
                bool ok = sameMessages(right, messages, why);
                std::snprintf(what, sizeof(what), "POCSAG %s, %.0f baud, %s polarity: all messages decoded%s%s",
                    variant ? "with a bit error in every codeword" : "clean", bauds[s], inverted ? "inverted" : "normal",
                    ok ? "" : " -- ", why.c_str());
                check(ok, what);

                bool others = wrongPol.empty();
                for (int o = 0; o < 3; o++)
                {
                    if (o != s) {
                        others = others && legs[o].m_normalMessages.empty() && legs[o].m_invertedMessages.empty();
                    }
                }
                std::snprintf(what, sizeof(what), "POCSAG %.0f baud, %s polarity: other speeds and the other polarity stay silent",
                    bauds[s], inverted ? "inverted" : "normal");
                check(others, what);

                if (variant == 1)
                {
                    int corrected = 0;
                    for (const auto& m : right) {
                        corrected += m.m_errorsCorrected;
                    }
                    check(corrected > 0, "POCSAG reports corrected bit errors");
                }
            }
        }
    }
}

// Feeds bits straight to a decoder (normal polarity: binary 1 is symbol 0) and returns the messages
std::vector<RadioPagingPocsagDecoder::Message> decodeBits(const std::vector<uint8_t>& bits)
{
    RadioPagingPocsagDecoder decoder(RadioPagingPocsagDecoder::Normal);
    std::vector<RadioPagingPocsagDecoder::Message> messages;

    for (uint8_t b : bits)
    {
        decoder.feed(b ? 0 : 1);
        while (decoder.hasMessage()) {
            messages.push_back(decoder.takeMessage());
        }
    }
    // A few bits of silence after the transmission
    for (int i = 0; i < 64; i++)
    {
        decoder.feed(i & 1);
        while (decoder.hasMessage()) {
            messages.push_back(decoder.takeMessage());
        }
    }

    return messages;
}

void flipBits(std::vector<uint8_t>& bits, size_t codewordIndex, int count)
{
    for (int i = 0; i < count; i++) {
        bits[RadioPagingPocsag::m_preambleBits + 32 * codewordIndex + 3 + 7 * i] ^= 1;
    }
}

bool hasAddressText(const std::vector<RadioPagingPocsagDecoder::Message>& messages, uint32_t address, const std::string& text)
{
    for (const auto& m : messages)
    {
        if ((m.m_address == address) && (RadioPagingPocsag::interpret(m.m_rawData, m.m_rawBits).m_text == text)) {
            return true;
        }
    }
    return false;
}

// Interpretation of long messages: a long alphanumeric message of figures and punctuation is alphanumeric, a
// numeric message of 15 digits (the longest without the length penalty) is numeric
void testPocsagLongNumeric()
{
    RadioPagingPocsag::TxMessage a;
    a.m_address = 1234567;
    a.m_function = 3;
    a.m_type = RadioPagingPocsag::Alphanumeric;
    a.m_text = "SWF 1.5345-55 YEN 117.70-80 SWF 1.3026-46 FT-100 2951.2";
    RadioPagingPocsag::TxMessage n = a;
    n.m_function = 0;
    n.m_type = RadioPagingPocsag::Numeric;
    n.m_text = "555123456789012";

    std::vector<RadioPagingPocsagDecoder::Message> messages = decodeBits(RadioPagingPocsag::encode({ a }).m_bits);
    RadioPagingPocsag::Interpretation ia = messages.empty() ? RadioPagingPocsag::Interpretation()
        : RadioPagingPocsag::interpret(messages[0].m_rawData, messages[0].m_rawBits);
    messages = decodeBits(RadioPagingPocsag::encode({ n }).m_bits);
    RadioPagingPocsag::Interpretation in = messages.empty() ? RadioPagingPocsag::Interpretation()
        : RadioPagingPocsag::interpret(messages[0].m_rawData, messages[0].m_rawBits);

    check((ia.m_type == "alphanumeric") && (ia.m_text == a.m_text), "POCSAG: a long alphanumeric message of figures is alphanumeric");
    check((in.m_type == "numeric") && (in.m_text == n.m_text), "POCSAG: a 15 digit numeric message is numeric");
    std::printf("      long alpha: %s %d / %s %d, 15 digits: %s %d / %s %d\n", ia.m_type.c_str(), ia.m_score, ia.m_altType.c_str(),
        ia.m_altScore, in.m_type.c_str(), in.m_score, in.m_altType.c_str(), in.m_altScore);

    // A short numeric message whose 7-bit reading is printable: "0612 4455" reads "B0"U. The double quotes take a
    // little off the alphanumeric reading (was a tie, which an alphanumeric message wins); a short alphanumeric
    // message with quotes stays alphanumeric
    RadioPagingPocsag::TxMessage shortNumeric = n;
    shortNumeric.m_text = "0612 4455";
    RadioPagingPocsag::TxMessage quoted = a;
    quoted.m_text = "Say \"OK\"";
    messages = decodeBits(RadioPagingPocsag::encode({ shortNumeric }).m_bits);
    RadioPagingPocsag::Interpretation is = messages.empty() ? RadioPagingPocsag::Interpretation()
        : RadioPagingPocsag::interpret(messages[0].m_rawData, messages[0].m_rawBits);
    messages = decodeBits(RadioPagingPocsag::encode({ quoted }).m_bits);
    RadioPagingPocsag::Interpretation iq = messages.empty() ? RadioPagingPocsag::Interpretation()
        : RadioPagingPocsag::interpret(messages[0].m_rawData, messages[0].m_rawBits);
    check((is.m_type == "numeric") && (is.m_text.rfind("0612 4455", 0) == 0),
        "POCSAG: a short numeric message whose 7-bit reading has double quotes is numeric");
    check((iq.m_type == "alphanumeric") && (iq.m_text == quoted.m_text), "POCSAG: a short alphanumeric message with double quotes is alphanumeric");
    std::printf("      0612 4455: %s %d / %s %d (%s), Say \"OK\": %s %d / %s %d\n", is.m_type.c_str(), is.m_score, is.m_altType.c_str(),
        is.m_altScore, is.m_altText.c_str(), iq.m_type.c_str(), iq.m_score, iq.m_altType.c_str(), iq.m_altScore);
}

void testPocsagSyncLoss()
{
    RadioPagingPocsag::TxMessage m;
    m.m_address = 1234567;
    m.m_function = 3;
    m.m_type = RadioPagingPocsag::Alphanumeric;
    m.m_text = std::string(120, 'A');
    RadioPagingPocsag::EncodeResult r = RadioPagingPocsag::encode({ m });
    const size_t batchWords = RadioPagingPocsag::m_codewordsPerBatch + 1;

    // One sync codeword missed (4 bit errors, beyond the 2 corrected): reception goes on at the expected
    // position and the next sync codeword confirms the batch
    std::vector<uint8_t> bits = r.m_bits;
    flipBits(bits, batchWords, 4);
    std::vector<RadioPagingPocsagDecoder::Message> messages = decodeBits(bits);
    check((messages.size() == 1) && (messages[0].m_end != RadioPagingPocsagDecoder::EndCut) && hasAddressText(messages, 1234567, m.m_text),
        "POCSAG: one sync codeword missed, the message is received whole");

    // Two sync codewords missed in a row: sync loss, the message ends there
    flipBits(bits, 2 * batchWords, 4);
    messages = decodeBits(bits);
    check((messages.size() == 1) && (messages[0].m_end == RadioPagingPocsagDecoder::EndCut), "POCSAG: two sync codewords missed in a row, sync loss");

    // A message whose last codeword is the last of the last batch: the transmission ends with the batch (M.584-2 1),
    // nothing follows the message (no idle codeword: the encoder sends a batch of idle codewords then, transmitters
    // on the air do not), then noise. It is whole. A message cut in the middle of a batch by the end of the signal is
    // still cut. The address of RIC 1234567 is in frame 7 (codeword 14)
    const size_t batchBits = batchWords * 32;
    auto decodeThenNoise = [](const std::vector<uint8_t>& transmission) {
        RadioPagingPocsagDecoder decoder(RadioPagingPocsagDecoder::Normal);
        std::vector<RadioPagingPocsagDecoder::Message> out;
        uint32_t noise = 12345;

        for (size_t i = 0; i < transmission.size() + 3 * 17 * 32; i++)
        {
            noise = noise * 1103515245u + 12345u;
            decoder.feed((i < transmission.size()) ? (transmission[i] ? 0 : 1) : (int) ((noise >> 16) & 1));

            while (decoder.hasMessage()) {
                out.push_back(decoder.takeMessage());
            }
        }

        decoder.flush();

        while (decoder.hasMessage()) {
            out.push_back(decoder.takeMessage());
        }

        return out;
    };
    int filling = 0;
    int fillingWhole = 0;

    for (int length = 1; length <= 120; length++)
    {
        RadioPagingPocsag::TxMessage f = m;
        f.m_text = std::string(length, 'B');
        std::vector<uint8_t> transmission = RadioPagingPocsag::encode({ f }).m_bits;
        const uint32_t idle = RadioPagingPocsag::m_idleWord;
        bool idleBatch = transmission.size() > batchBits;

        // The last batch all idle codewords: the message filled the one before
        for (size_t w = 1; idleBatch && (w < batchWords); w++)
        {
            uint32_t word = 0;

            for (int b = 0; b < 32; b++) {
                word = (word << 1) | transmission[transmission.size() - batchBits + w * 32 + b];
            }

            idleBatch = (word == idle);
        }

        if (!idleBatch) {
            continue;
        }

        transmission.resize(transmission.size() - batchBits);
        messages = decodeThenNoise(transmission);
        filling++;
        fillingWhole += ((messages.size() == 1) && (messages[0].m_end == RadioPagingPocsagDecoder::EndBatch)
            && hasAddressText(messages, 1234567, f.m_text)) ? 1 : 0;
    }

    check((filling > 0) && (fillingWhole == filling), "POCSAG: a message that fills its last batch ends with the transmission, whole");
    std::printf("      messages filling their last batch: %d, whole: %d\n", filling, fillingWhole);

    // The signal ends in the middle of a batch, in the message: cut. The 120 characters are 42 codewords, from codeword
    // 15 of batch 1 to codeword 9 of batch 4: the signal ends after codeword 8 of batch 3
    std::vector<uint8_t> partial = r.m_bits;
    partial.resize(partial.size() - 2 * batchBits + 9 * 32);
    // (The noise here is hard bits, as clean as a signal: what it decodes to after the cut is not looked at)
    messages = decodeThenNoise(partial);
    check(!messages.empty() && (messages[0].m_address == 1234567) && (messages[0].m_end == RadioPagingPocsagDecoder::EndCut)
        && (messages[0].m_wordStatus.size() < 42),
        "POCSAG: the signal ends in the middle of a message: cut");

    // The end of a transmission followed by noise: the batch after the last one is not confirmed, no message
    RadioPagingPocsag::TxMessage tone;
    tone.m_address = 1000;
    tone.m_type = RadioPagingPocsag::Tone;
    bits = RadioPagingPocsag::encode({ tone }).m_bits;
    std::mt19937 rng(7);

    for (int i = 0; i < 32 * (int) batchWords * 3; i++) {
        bits.push_back(rng() & 1);
    }

    messages = decodeBits(bits);
    check((messages.size() == 1) && (messages[0].m_address == 1000), "POCSAG: noise after a transmission gives no message");
}

// The signal lost in the last batch of a transmission (no sync codeword follows it) and noise after: for each cut,
// in the middle of each codeword of the batch, the messages decoded have only the addresses sent and each text, up to
// a codeword marked lost, is the start of the text sent. Symbols are fed with their error: small for the transmission,
// as noise gives after the cut
// An idle codeword received with 4 bit errors that the code corrects to an address codeword (at distance 6 from idle)
// gives no page
void testPocsagDamagedIdle()
{
    uint32_t damaged = 0;

    for (int a = 0; (a < 32) && !damaged; a++)
    {
        for (int b = a + 1; (b < 32) && !damaged; b++)
        {
            for (int c = b + 1; (c < 32) && !damaged; c++)
            {
                for (int d = c + 1; (d < 32) && !damaged; d++)
                {
                    uint32_t w = RadioPagingPocsag::m_idleWord ^ (1u << a) ^ (1u << b) ^ (1u << c) ^ (1u << d);
                    RadioPagingBCH::Result r = RadioPagingBCH::decode(w);
                    if ((r.m_bitErrors >= 0) && ((r.m_word & 0x80000000u) == 0) && (r.m_word != RadioPagingPocsag::m_idleWord)) {
                        damaged = w;
                    }
                }
            }
        }
    }

    RadioPagingPocsag::TxMessage tone;
    tone.m_address = 1000;
    tone.m_type = RadioPagingPocsag::Tone;
    RadioPagingPocsag::EncodeResult enc = RadioPagingPocsag::encode({ tone });
    std::vector<uint8_t> bits = enc.m_bits;
    // The last codeword of the batch is an idle codeword: replaced by the damaged one
    const size_t last = enc.m_codewords.size() - 1;
    check(enc.m_codewords[last] == RadioPagingPocsag::m_idleWord, "POCSAG: the batch ends with an idle codeword");
    for (int i = 0; i < 32; i++) {
        bits[RadioPagingPocsag::m_preambleBits + 32 * last + i] = (damaged >> (31 - i)) & 1;
    }

    // The input stops after the transmission: everything received is released
    RadioPagingPocsagDecoder decoder(RadioPagingPocsagDecoder::Normal);
    std::vector<RadioPagingPocsagDecoder::Message> messages;
    for (uint8_t b : bits) {
        decoder.feed(b ? 0 : 1);
    }
    decoder.flush();
    while (decoder.hasMessage()) {
        messages.push_back(decoder.takeMessage());
    }
    check((damaged != 0) && (messages.size() == 1) && (messages[0].m_address == 1000),
        "POCSAG: an idle codeword with 4 bit errors corrected to an address gives no page");
}

void testPocsagSignalLost()
{
    RadioPagingPocsag::TxMessage a, n;
    a.m_address = 1234567;
    a.m_function = 3;
    a.m_type = RadioPagingPocsag::Alphanumeric;
    a.m_text = "POCSAG SWEEP 0123456789";
    n.m_address = 7654;
    n.m_function = 0;
    n.m_type = RadioPagingPocsag::Numeric;
    n.m_text = "0123456789";
    std::vector<uint8_t> sent = RadioPagingPocsag::encode({ a, n }).m_bits;
    const int lastBatch = RadioPagingPocsag::m_preambleBits + 2 * 32 + RadioPagingPocsag::m_codewordsPerBatch * 32;
    int foreign = 0, wrongText = 0, received = 0;

    for (int seed = 0; seed < 4; seed++)
    {
        for (int word = 0; word < RadioPagingPocsag::m_codewordsPerBatch; word++)
        {
            const int cut = lastBatch + 32 * word + 16;
            std::mt19937 rng(100 * seed + word);
            std::uniform_real_distribution<float> noiseError(0.7f, 1.2f);
            RadioPagingPocsagDecoder decoder(RadioPagingPocsagDecoder::Normal);
            decoder.setNoiseQuality(0.59f);
            std::vector<RadioPagingPocsagDecoder::Message> messages;

            for (int i = 0; i < cut + 32 * 17 * 3; i++)
            {
                if (i < cut) {
                    decoder.feed(sent[i] ? 0 : 1, 0.05f);
                } else {
                    decoder.feed((int) (rng() & 1), noiseError(rng));
                }
                while (decoder.hasMessage()) {
                    messages.push_back(decoder.takeMessage());
                }
            }

            for (const auto& m : messages)
            {
                // The text of the codewords up to the first one marked lost (X)
                size_t lost = m.m_wordStatus.find('X');
                int bits = (lost == std::string::npos) ? m.m_rawBits : std::min(m.m_rawBits, (int) lost * 20);
                std::vector<uint8_t> raw(m.m_rawData.begin(), m.m_rawData.begin() + (bits + 7) / 8);
                std::string text = RadioPagingPocsag::interpret(raw, bits).m_text;

                if (m.m_address == a.m_address) {
                    wrongText += (a.m_text.compare(0, text.size(), text) != 0) ? 1 : 0;
                } else if (m.m_address == n.m_address) {
                    wrongText += (n.m_text.compare(0, text.size(), text) != 0) ? 1 : 0;
                } else {
                    foreign++;
                }
                received++;
            }
        }
    }

    std::printf("      signal lost in the last batch: %d messages, %d foreign, %d with a wrong text\n", received, foreign, wrongText);
    check(foreign == 0, "POCSAG: signal lost in the last batch, noise gives no address");
    check(wrongText == 0, "POCSAG: signal lost in the last batch, noise adds nothing to a message");
    check(received >= 4 * RadioPagingPocsag::m_codewordsPerBatch, "POCSAG: signal lost in the last batch, the message started is received");
}

// Bits fed as a strong clean signal: the soft value of each symbol known (sync codewords are also matched on them)
std::vector<RadioPagingPocsagDecoder::Message> decodeCleanSoft(const std::vector<uint8_t>& bits)
{
    RadioPagingPocsagDecoder decoder(RadioPagingPocsagDecoder::Normal);
    std::vector<RadioPagingPocsagDecoder::Message> messages;

    for (uint8_t b : bits)
    {
        // Binary 1 is a negative deviation for normal polarity
        const int symbol = b ? 0 : 1;
        decoder.feed(symbol, 0.05f, symbol ? 1.0f : -1.0f);
        while (decoder.hasMessage()) {
            messages.push_back(decoder.takeMessage());
        }
    }
    decoder.flush();
    while (decoder.hasMessage()) {
        messages.push_back(decoder.takeMessage());
    }
    return messages;
}

// A sync codeword look alike in the first batch of a transmission (within 4 bit errors, the soft match, straddling two
// message codewords of the text "THIRD AT 2400 INVERTED" at RIC 1234569C) is data: the batch after the preamble is not
// given up for it, the page is received
void testPocsagSyncLookAlike()
{
    RadioPagingPocsag::TxMessage m;
    m.m_address = 1234569;
    m.m_function = 2;
    m.m_type = RadioPagingPocsag::Alphanumeric;
    m.m_text = "THIRD AT 2400 INVERTED";
    std::vector<uint8_t> bits = RadioPagingPocsag::encode({ m }).m_bits;

    // The look alike is there: a window within 4 bit errors of the sync codeword, not at a codeword boundary
    int closest = 32;
    for (size_t i = RadioPagingPocsag::m_preambleBits + 32; i + 32 <= bits.size(); i++)
    {
        uint32_t w = 0;
        for (int k = 0; k < 32; k++) {
            w = (w << 1) | bits[i + k];
        }
        if ((i - RadioPagingPocsag::m_preambleBits) % 32 != 0) {
            closest = std::min(closest, __builtin_popcount(w ^ RadioPagingPocsag::m_syncWord));
        }
    }
    check(closest <= RadioPagingPocsagDecoder::m_maxSoftSyncErrors, "POCSAG: the text has a sync codeword look alike in its codewords");

    // After the preamble (the receiver marks the candidate so)
    std::vector<RadioPagingPocsagDecoder::Message> messages;
    {
        RadioPagingPocsagDecoder decoder(RadioPagingPocsagDecoder::Normal);
        int syncs = 0;
        for (uint8_t b : bits)
        {
            const int symbol = b ? 0 : 1;
            decoder.feed(symbol, 0.05f, symbol ? 1.0f : -1.0f);
            if (decoder.syncWords() != (uint32_t) syncs)
            {
                if ((syncs == 0) && !decoder.getStatus().m_confirmed) {
                    decoder.setPreambleBefore();
                }
                syncs = decoder.syncWords();
            }
            while (decoder.hasMessage()) {
                messages.push_back(decoder.takeMessage());
            }
        }
        decoder.flush();
        while (decoder.hasMessage()) {
            messages.push_back(decoder.takeMessage());
        }
    }
    check(hasAddressText(messages, m.m_address, m.m_text), "POCSAG: a sync look alike in the text after the preamble, the page is received");

    // The preamble missed (the sync codeword found alone): an exact sync codeword does not give way to the look alike
    // with more bit errors
    std::vector<uint8_t> noPreamble(bits.begin() + RadioPagingPocsag::m_preambleBits, bits.end());
    messages = decodeCleanSoft(noPreamble);
    check(hasAddressText(messages, m.m_address, m.m_text), "POCSAG: a sync look alike in the text without the preamble, the page is received");
}

// Bits fed as a strong clean signal, a candidate batch marked as after the preamble when reversals end right before its
// sync codeword (as the receiver does)
std::vector<RadioPagingPocsagDecoder::Message> decodeCleanSoftPreamble(const std::vector<uint8_t>& bits)
{
    RadioPagingPocsagDecoder decoder(RadioPagingPocsagDecoder::Normal);
    std::vector<RadioPagingPocsagDecoder::Message> messages;
    uint32_t syncs = 0;

    for (size_t i = 0; i < bits.size(); i++)
    {
        const int symbol = bits[i] ? 0 : 1;
        decoder.feed(symbol, 0.05f, symbol ? 1.0f : -1.0f);

        if (decoder.syncWords() != syncs)
        {
            syncs = decoder.syncWords();
            // 16 reversals end within a codeword before the 32 bits of the sync codeword
            bool preamble = false;
            for (size_t end = (i >= 32 + 16) ? i - 32 - 32 : 0; (end + 16 <= i - 32 + 1) && (i >= 32 + 16) && !preamble; end++)
            {
                bool reversals = true;
                for (size_t k = end + 1; k < end + 16; k++) {
                    reversals = reversals && (bits[k] != bits[k - 1]);
                }
                preamble = reversals;
            }
            if (preamble && !decoder.getStatus().m_confirmed) {
                decoder.setPreambleBefore();
            }
        }
        while (decoder.hasMessage()) {
            messages.push_back(decoder.takeMessage());
        }
    }
    decoder.flush();
    while (decoder.hasMessage()) {
        messages.push_back(decoder.takeMessage());
    }
    return messages;
}

// Two transmissions at the same speed and polarity, the second one starting (with a short preamble) right after the
// first one ends, within the batch the decoder reads after the missed sync codeword: the sync codeword of the second
// one is taken there, both pages are received. Also when the first one was found without its preamble (its sync
// codeword exact)
void testPocsagTransmissionAfterTransmission()
{
    RadioPagingPocsag::TxMessage a, b;
    a.m_address = 1300001;
    a.m_function = 0;
    a.m_type = RadioPagingPocsag::Alphanumeric;
    a.m_text = "FIRST TRANSMISSION";
    b.m_address = 1300002;
    b.m_function = 1;
    b.m_type = RadioPagingPocsag::Alphanumeric;
    b.m_text = "SECOND TRANSMISSION";
    std::vector<uint8_t> first = RadioPagingPocsag::encode({ a }).m_bits;
    std::vector<uint8_t> second = RadioPagingPocsag::encode({ b }).m_bits;
    int bothWithPreamble = 0, bothWithout = 0, cases = 0;

    for (int preamble = 40; preamble <= 360; preamble += 40)
    {
        std::vector<uint8_t> bits = first;
        bits.insert(bits.end(), second.begin() + (RadioPagingPocsag::m_preambleBits - preamble), second.end());
        std::vector<RadioPagingPocsagDecoder::Message> messages = decodeCleanSoftPreamble(bits);
        bothWithPreamble += (hasAddressText(messages, a.m_address, a.m_text) && hasAddressText(messages, b.m_address, b.m_text)) ? 1 : 0;

        std::vector<uint8_t> noPreamble(bits.begin() + RadioPagingPocsag::m_preambleBits, bits.end());
        messages = decodeCleanSoft(noPreamble);
        bothWithout += (hasAddressText(messages, a.m_address, a.m_text) && hasAddressText(messages, b.m_address, b.m_text)) ? 1 : 0;
        cases++;
    }

    std::printf("      a transmission right after another: both received %d of %d (after the preamble), %d of %d (first found without it)\n",
        bothWithPreamble, cases, bothWithout, cases);
    check(bothWithPreamble == cases, "POCSAG: a transmission right after another, both pages received");
    check(bothWithout == cases, "POCSAG: a transmission right after another found without its preamble, both pages received");
}

// The end of the sync codeword (18 bits) is the complement of the start of the idle codeword: where a codeword is
// followed by an idle codeword, the decoder of the other polarity sees the sync codeword but for the last 14 bits of
// that codeword. A transmission with a message codeword whose last 14 bits complete it, before an idle codeword in the
// same slot of three batches, makes the other polarity find a sync codeword and confirm it one batch later. The
// receiver (both polarities) gives the page in the polarity sent and nothing in the other one; the mirror case too
void testPocsagOtherPolarity()
{
    // A message codeword whose last 14 bits are the complement of the first 14 of the sync codeword
    const uint32_t tail = (~RadioPagingPocsag::m_syncWord >> 18) & 0x3fff;
    uint32_t c = 0;
    for (uint32_t data = 1u << 20; (data < (1u << 21)) && !c; data++)
    {
        uint32_t w = RadioPagingBCH::encode(data);
        if ((w & 0x3fff) == tail) {
            c = w;
        }
    }
    check(c != 0, "POCSAG: a message codeword ending as the complement of the start of the sync codeword");

    // Address 1300001 (frame 1, function 0) in slot 3, the message codeword in slot 4, idle codewords elsewhere
    const uint32_t address = 1300001;
    const uint32_t addressWord = RadioPagingBCH::encode(((address >> 3) & 0x3ffff) << 2);
    std::vector<uint8_t> bits;
    for (int i = 0; i < RadioPagingPocsag::m_preambleBits; i++) {
        bits.push_back((i & 1) ? 0 : 1);
    }
    auto addWord = [&bits](uint32_t w) {
        for (int k = 31; k >= 0; k--) {
            bits.push_back((w >> k) & 1);
        }
    };
    for (int batch = 0; batch < 3; batch++)
    {
        addWord(RadioPagingPocsag::m_syncWord);
        for (int slot = 0; slot < RadioPagingPocsag::m_codewordsPerBatch; slot++) {
            addWord((slot == 3) ? addressWord : (slot == 4) ? c : RadioPagingPocsag::m_idleWord);
        }
    }

    for (int inverted = 0; inverted < 2; inverted++)
    {
        std::vector<float> samples = makeWaveform(bits, 38400.0, 1200.0, inverted, 0.0f, 0.02f, 300, 300, 5 + inverted);
        PocsagLeg leg(38400.0, 1200.0);
        for (float s : samples) {
            leg.process(s);
        }
        leg.m_receiver.flush();
        while (leg.m_receiver.hasMessage())
        {
            RadioPagingPocsagDecoder::Message m = leg.m_receiver.takeMessage();
            (m.m_polarity == RadioPagingPocsagDecoder::Inverted ? leg.m_invertedMessages : leg.m_normalMessages).push_back(m);
        }
        const auto& sent = inverted ? leg.m_invertedMessages : leg.m_normalMessages;
        const auto& other = inverted ? leg.m_normalMessages : leg.m_invertedMessages;
        bool got = false;
        for (const auto& m : sent) {
            got = got || (m.m_address == address);
        }
        std::printf("      %s transmission: %d messages in its polarity, %d in the other\n", inverted ? "inverted" : "normal",
            (int) sent.size(), (int) other.size());
        check(got, inverted ? "POCSAG: inverted transmission, the page is received" : "POCSAG: normal transmission, the page is received");
        check(other.empty(), inverted ? "POCSAG: inverted transmission, nothing in normal polarity"
            : "POCSAG: normal transmission, nothing in inverted polarity");
    }
}

// The end of a transmission followed (after a gap) by another strong clean signal, as a transmission at another speed
// read at this one: its words are not a fade of this transmission, no page is made of them
void testPocsagOtherSignalAfter()
{
    RadioPagingPocsag::TxMessage a;
    a.m_address = 1200001;
    a.m_function = 0;
    a.m_type = RadioPagingPocsag::Alphanumeric;
    a.m_text = "INV AT 512";
    std::vector<uint8_t> sent = RadioPagingPocsag::encode({ a }).m_bits;
    int foreign = 0, received = 0, transmissions = 0;

    for (int seed = 0; seed < 50; seed++)
    {
        std::vector<uint8_t> bits = sent;
        std::mt19937 rng(seed);
        for (int i = 0; i < 32 * 17 * 4; i++) {
            bits.push_back(rng() & 1);
        }
        std::vector<RadioPagingPocsagDecoder::Message> messages = decodeCleanSoft(bits);
        bool got = false;
        for (const auto& m : messages)
        {
            if (m.m_address == a.m_address) {
                got = true;
            } else {
                foreign++;
            }
        }
        received += got ? 1 : 0;
        transmissions++;
    }

    std::printf("      another signal after a transmission: %d of %d pages, %d foreign\n", received, transmissions, foreign);
    check(received == transmissions, "POCSAG: another signal after a transmission, the page is received");
    check(foreign == 0, "POCSAG: another signal after a transmission gives no page");
}

void testPocsagStandardRules()
{
    // Encoder: an idle or address codeword between a message and the next message's address,
    // and an idle codeword at the end of the transmission
    std::vector<RadioPagingPocsag::TxMessage> messages = pocsagTestMessages();
    RadioPagingPocsag::TxMessage m;
    m.m_address = 9; m.m_function = 3; m.m_type = RadioPagingPocsag::Alphanumeric; m.m_text = "abc";
    messages.push_back(m);
    m.m_address = 10; m.m_text = "def";     // Frame 2, right after the previous message
    messages.push_back(m);
    RadioPagingPocsag::EncodeResult result = RadioPagingPocsag::encode(messages);

    bool separated = true;
    uint32_t previous = RadioPagingPocsag::m_idleWord;
    for (uint32_t w : result.m_codewords)
    {
        if (w == RadioPagingPocsag::m_syncWord) {
            continue;
        }
        bool isAddress = ((w & 0x80000000u) == 0) && (w != RadioPagingPocsag::m_idleWord);
        if (isAddress && ((previous & 0x80000000u) != 0)) {
            separated = false;
        }
        previous = w;
    }
    check(separated, "POCSAG encoder: no address codeword directly after a message codeword");
    check(result.m_codewords.back() == RadioPagingPocsag::m_idleWord, "POCSAG encoder: the last codeword is an idle codeword");

    // A message that fills a batch exactly is followed by a batch of idle codewords
    RadioPagingPocsag::TxMessage full;
    full.m_address = 0; full.m_type = RadioPagingPocsag::Numeric; full.m_text = std::string(75, '1');
    RadioPagingPocsag::EncodeResult fullResult = RadioPagingPocsag::encode({full});
    check(fullResult.m_codewords.back() == RadioPagingPocsag::m_idleWord, "POCSAG encoder: a full batch is followed by idle codewords");

    // Decoder: all messages from bits alone
    std::vector<RadioPagingPocsagDecoder::Message> all = decodeBits(result.m_bits);
    check(all.size() == messages.size(), "POCSAG decoder: all messages of a multi-batch transmission");

    // Joining during a transmission, without the preamble: start at the second batch
    std::vector<uint8_t> late(result.m_bits.begin() + RadioPagingPocsag::m_preambleBits + 17 * 32 - 5, result.m_bits.end());
    std::vector<RadioPagingPocsagDecoder::Message> joined = decodeBits(late);
    bool lateOk = hasAddressText(joined, 9, "abc") && hasAddressText(joined, 10, "def") && hasAddressText(joined, 100, "A\r\nB");
    check(lateOk, "POCSAG decoder: joining at the second batch without preamble decodes the later messages");

    // No preamble at all: a single batch is decoded because it is valid by itself
    RadioPagingPocsag::TxMessage one;
    one.m_address = 77; one.m_type = RadioPagingPocsag::Alphanumeric; one.m_text = "single";
    RadioPagingPocsag::EncodeOptions noPreamble;
    noPreamble.m_preamble = false;
    RadioPagingPocsag::EncodeResult single = RadioPagingPocsag::encode({one}, noPreamble);
    check(single.m_codewords.size() == 17, "POCSAG encoder: a short message is a single batch");
    std::vector<uint8_t> singleBits(single.m_bits);
    singleBits.insert(singleBits.begin(), 40, 0);
    check(hasAddressText(decodeBits(singleBits), 77, "single"), "POCSAG decoder: a valid single batch is decoded without preamble");

    // A single batch with an undecodable codeword has no confirming sync codeword: its codewords received exactly show a
    // transmission, it is decoded
    std::vector<uint8_t> badSingle = single.m_bits;
    for (int i = 0; i < 3; i++) {
        badSingle[32 * 15 + 2 + 9 * i] ^= 1;    // 3 errors in the last idle codeword
    }
    // (the undecodable codeword, an idle codeword after the message, is kept in it marked lost)
    std::vector<RadioPagingPocsagDecoder::Message> badDecoded = decodeBits(badSingle);
    check((badDecoded.size() == 1) && (badDecoded[0].m_address == 77)
        && (RadioPagingPocsag::interpret(badDecoded[0].m_rawData, badDecoded[0].m_rawBits).m_text.compare(0, 6, "single") == 0)
        && (badDecoded[0].m_wordStatus.back() == 'X'), "POCSAG decoder: an unconfirmed batch with an undecodable codeword is decoded");

    // A sync codeword alone whose address needs a correction: what noise, a carrier or the data of another protocol give
    // (the code corrects half of the words to some codeword). No message (found with a radio round trip: tone pages
    // of 2 corrected bits in a FLEX transmission)
    for (int errors : {1, 2})
    {
        std::vector<uint8_t> corrected = single.m_bits;
        for (int i = 0; i < errors; i++) {
            corrected[32 + 3 + 11 * i] ^= 1;    // The address codeword, the first after the sync codeword
        }
        check(decodeBits(corrected).empty(), (std::string("POCSAG decoder: a sync codeword alone with an address of ") + std::to_string(errors)
            + " corrected bits gives no message").c_str());
    }

    // A sync codeword pattern alone, noise after it: not decoded
    std::vector<uint8_t> syncAlone(single.m_bits.begin(), single.m_bits.begin() + 32);
    std::mt19937 noiseRng(5);
    for (int i = 0; i < 32 * 17; i++) {
        syncAlone.push_back(noiseRng() & 1);
    }
    check(decodeBits(syncAlone).empty(), "POCSAG decoder: a sync codeword pattern followed by noise gives no message");

    // The same damage in the last batch of a multi-batch transmission: its sync codeword is
    // confirmed by the previous one, the batch is decoded
    std::vector<uint8_t> badLast = result.m_bits;
    flipBits(badLast, result.m_codewords.size() - 1, 3);
    check(decodeBits(badLast).size() == messages.size(), "POCSAG decoder: the last batch of a confirmed transmission is decoded with an undecodable codeword");

    // Two successive undecodable codewords end a message; the next messages are still decoded
    RadioPagingPocsag::TxMessage longMessage;
    longMessage.m_address = 1000; longMessage.m_type = RadioPagingPocsag::Alphanumeric;
    longMessage.m_text = std::string(60, 'x');
    RadioPagingPocsag::TxMessage after;
    after.m_address = 1001; after.m_type = RadioPagingPocsag::Alphanumeric; after.m_text = "after";
    RadioPagingPocsag::EncodeResult twoResult = RadioPagingPocsag::encode({longMessage, after});
    std::vector<uint8_t> two = twoResult.m_bits;
    // Codewords of the first batch: sync (index 0), address of 1000 in frame 0 (index 1),
    // message codewords from index 2
    flipBits(two, 4, 3);
    flipBits(two, 5, 3);
    std::vector<RadioPagingPocsagDecoder::Message> twoMessages = decodeBits(two);
    bool cut = false;
    for (const auto& msg : twoMessages)
    {
        if (msg.m_address == 1000) {
            cut = (msg.m_end == RadioPagingPocsagDecoder::EndCut) && (msg.m_errorsUncorrectable == 2) && (msg.m_rawBits == 2 * 20 + 20);
        }
    }
    check(cut, "POCSAG decoder: two successive undecodable codewords end the message");
    check(hasAddressText(twoMessages, 1001, "after"), "POCSAG decoder: the message after them is decoded");

    // A single undecodable codeword stays in the message
    std::vector<uint8_t> oneBad = twoResult.m_bits;
    flipBits(oneBad, 4, 3);
    bool kept = false;
    for (const auto& msg : decodeBits(oneBad))
    {
        if (msg.m_address == 1000) {
            kept = (msg.m_end != RadioPagingPocsagDecoder::EndCut) && (msg.m_errorsUncorrectable == 1);
        }
    }
    check(kept, "POCSAG decoder: one undecodable codeword is kept in the message");
}

void testBerWindow()
{
    RadioPagingBerWindow w;
    check(!w.valid(), "BER window: no codeword, no value");

    for (int i = 0; i < 10; i++) {
        w.add(0, 32, false);
    }
    check(w.valid() && (w.ber() == 0.0f) && (w.m_uncorrectable == 0), "BER window: clean codewords give 0");

    w.add(2, 32, false);
    check((w.m_errorBits == 2) && (w.m_uncorrectable == 0), "BER window: corrected bits are counted, nothing uncorrectable");

    w.add(0, 32, true);
    check((w.m_errorBits == 2 + RadioPagingBerWindow::m_uncorrectableBits) && (w.m_uncorrectable == 1),
        "BER window: an uncorrectable codeword counts its capped errors and itself");

    for (int i = 0; i < 4 * RadioPagingBerWindow::m_windowCodewords; i++) {
        w.add(0, 32, false);
    }
    check((w.m_uncorrectable == 0) && (w.m_errorBits == 0), "BER window: old errors leave the window");

    // POCSAG decoder: a clean batch gives a valid window without errors
    RadioPagingPocsagDecoder d(RadioPagingPocsagDecoder::Normal);
    std::vector<uint32_t> words = { RadioPagingPocsag::m_syncWord };
    for (int i = 0; i < RadioPagingPocsag::m_codewordsPerBatch; i++) {
        words.push_back(RadioPagingPocsag::m_idleWord);
    }
    words.push_back(RadioPagingPocsag::m_syncWord);
    for (uint32_t word : words)
    {
        for (int b = 31; b >= 0; b--) {
            d.feed(((word >> b) & 1) ? 0 : 1);  // Binary 1 is a negative deviation for normal polarity
        }
    }
    RadioPagingBerWindow ber = d.getStatus().m_ber;
    check(ber.valid() && (ber.m_errorBits == 0) && (ber.m_uncorrectable == 0), "POCSAG decoder: BER of a clean batch is 0");
}

// The same transmission twice: the second copy of each message is marked duplicate; the same
// transmission on the other polarity is another network: not a duplicate
void testPocsagDuplicates()
{
    std::vector<RadioPagingPocsag::TxMessage> messages = pocsagTestMessages();
    RadioPagingPocsag::EncodeResult result = RadioPagingPocsag::encode(messages);
    std::vector<float> first = makeWaveform(result.m_bits, 38400.0, 1200.0, false, 0.08f, 0.12f, 300, 2000, 31);
    std::vector<float> again = makeWaveform(result.m_bits, 38400.0, 1200.0, false, 0.08f, 0.12f, 300, 2000, 32);
    std::vector<float> other = makeWaveform(result.m_bits, 38400.0, 1200.0, true, 0.08f, 0.12f, 300, 2000, 33);
    RadioPagingPocsagReceiver receiver;
    receiver.create(38400.0, 1200);
    std::vector<RadioPagingPocsagDecoder::Message> out[3];
    const std::vector<float> *parts[3] = { &first, &again, &other };

    for (int p = 0; p < 3; p++)
    {
        for (float s : *parts[p])
        {
            receiver.process(s);
            while (receiver.hasMessage()) {
                out[p].push_back(receiver.takeMessage());
            }
        }
    }

    bool firstNew = !out[0].empty();
    bool againDup = (out[1].size() == out[0].size());
    bool otherNew = (out[2].size() == out[0].size());

    for (const auto& m : out[0]) { firstNew = firstNew && !m.m_duplicate; }
    // A tone only page has nothing a pager stores: it is not a duplicate
    for (const auto& m : out[1]) { againDup = againDup && (m.m_duplicate == (m.m_rawBits > 0)); }
    for (const auto& m : out[2]) { otherNew = otherNew && !m.m_duplicate && (m.m_polarity == RadioPagingPocsagDecoder::Inverted); }

    check(firstNew, "POCSAG duplicates: first copy is not a duplicate");
    check(againDup, "POCSAG duplicates: second copy is output again, marked duplicate (tone only pages are not)");
    check(otherNew, "POCSAG duplicates: the same messages on the other polarity are not duplicates");
}

// Duplicate detection of a receiver: the last 16 messages of each key (frequency, protocol, polarity, address),
// within the window
void testDedup()
{
    std::vector<uint8_t> a = {1, 2, 3};
    std::vector<uint8_t> b = {4, 5, 6};

    {
        RadioPagingDedup dedup("pocsag");
        dedup.setFrequency(466025000);
        bool ok = !dedup.check(0, "1234567A", a, 24, 0.0) && dedup.check(0, "1234567A", a, 24, 1.0);
        ok = ok && !dedup.check(1, "1234567A", a, 24, 2.0);         // Other polarity
        ok = ok && !dedup.check(0, "1234567B", a, 24, 3.0);         // Other function
        ok = ok && !dedup.check(0, "1234567A", b, 24, 4.0);         // Other payload
        dedup.setFrequency(466075000);
        ok = ok && !dedup.check(0, "1234567A", a, 24, 5.0);         // Other frequency
        dedup.setFrequency(466025000);
        ok = ok && dedup.check(0, "1234567A", b, 24, 6.0);
        check(ok, "dedup: the key is the frequency, polarity and address with its function");
    }
    {
        RadioPagingDedup pocsag("pocsag");
        RadioPagingDedup gsc("gsc");
        pocsag.check(0, "100", a, 24, 0.0);
        // Each receiver has its own: the protocol is part of the key
        check(!gsc.check(0, "100", a, 24, 0.0), "dedup: the protocol is part of the key");
    }
    {
        RadioPagingDedup dedup("flex");
        bool ok = true;
        for (int i = 0; i < 16; i++) {
            ok = ok && !dedup.check(0, "0000004096", std::vector<uint8_t>(1, (uint8_t) i), 8, i);
        }
        ok = ok && dedup.check(0, "0000004096", std::vector<uint8_t>(1, 0), 8, 20.0);     // Still in the backlog
        ok = ok && !dedup.check(0, "0000004096", std::vector<uint8_t>(1, 16), 8, 21.0);   // 17th: the oldest (0) goes
        ok = ok && !dedup.check(0, "0000004096", std::vector<uint8_t>(1, 0), 8, 22.0);
        // Another address does not take the place of these
        for (int i = 0; i < 40; i++) {
            dedup.check(0, "0000008192", std::vector<uint8_t>(1, (uint8_t) i), 8, 23.0);
        }
        ok = ok && dedup.check(0, "0000004096", std::vector<uint8_t>(1, 16), 8, 24.0);
        check(ok, "dedup: the last 16 messages of each key are remembered");
    }
    {
        RadioPagingDedup dedup("pocsag");
        dedup.setWindow(60.0);
        bool ok = !dedup.check(0, "1A", a, 24, 0.0) && dedup.check(0, "1A", a, 24, 59.0) && !dedup.check(0, "1A", a, 24, 61.0);
        ok = ok && !dedup.check(0, "1A", std::vector<uint8_t>(), 0, 62.0) && !dedup.check(0, "1A", std::vector<uint8_t>(), 0, 63.0);
        check(ok, "dedup: a message older than the window is forgotten, a message without payload is not remembered");
    }
    {
        RadioPagingDedup dedup("pocsag");
        for (size_t i = 0; i <= RadioPagingDedup::m_maxKeys; i++) {
            dedup.check(0, std::to_string(i), a, 24, 0.0);
        }
        // The least recently used key ("0") was dropped
        check((dedup.size() == RadioPagingDedup::m_maxKeys) && !dedup.check(0, "0", a, 24, 1.0)
            && dedup.check(0, std::to_string(RadioPagingDedup::m_maxKeys), a, 24, 1.0), "dedup: the least recently used key is dropped when full");
    }
}

} // namespace

int main()
{
    testBCH();
    testPocsagCodec();
    testPocsagLink();
    testPocsagStandardRules();
    testPocsagSyncLoss();
    testPocsagSignalLost();
    testPocsagSyncLookAlike();
    testPocsagOtherSignalAfter();
    testPocsagTransmissionAfterTransmission();
    testPocsagOtherPolarity();
    testPocsagDamagedIdle();
    testPocsagLongNumeric();
    testPocsagPreambleLock();
    testFsk();
    testBerWindow();
    testPocsagDuplicates();
    testDedup();
    std::printf("%s (%d failure%s)\n", failures ? "FAILED" : "OK", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
