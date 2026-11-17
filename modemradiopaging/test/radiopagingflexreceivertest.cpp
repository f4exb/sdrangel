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

// Self test of the FLEX receiver: frames of the FLEX encoder are turned into FM
// discriminator samples (with noise, DC offset and a symbol clock error), received and
// decoded, and the messages are compared with the messages sent.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "radiopagingflex.h"
#include "radiopagingflexreceiver.h"
#include "radiopagingflexscheduler.h"

namespace {

int failures = 0;

void check(bool ok, const std::string& what)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) {
        failures++;
    }
}

std::string trimRight(std::string s)
{
    while (!s.empty() && (s.back() == ' ')) {
        s.pop_back();
    }
    return s;
}

std::string key(uint64_t capcode, const std::string& type, const std::string& text)
{
    return std::to_string(capcode) + "|" + type + "|" + trimRight(text);
}

struct Signal
{
    double m_sampleRate = 38400.0;
    double m_clockError = 100e-6;
    float m_dc = 0.05f;
    float m_noise = 0.08f;
    unsigned m_seed = 1;
    //! Time constant of the smoothing of the levels, in symbols at 3200 baud: a larger one is a narrower audio band
    double m_smoothing = 0.1;
};

// Discriminator samples for a sequence of symbol blocks sent back to back
std::vector<float> makeSamples(const std::vector<RadioPagingFlex::SymbolBlock>& blocks, const Signal& signal, int leadSamples)
{
    static const float levels4[4] = {-1.0f, -1.0f/3.0f, 1.0f/3.0f, 1.0f};
    std::mt19937 rng(signal.m_seed);
    std::normal_distribution<float> gauss(0.0f, signal.m_noise);
    std::uniform_real_distribution<float> uniform(-1.0f, 1.0f);
    std::vector<float> samples;

    for (int i = 0; i < leadSamples; i++) {
        samples.push_back(0.5f * uniform(rng) + gauss(rng));
    }

    float smoothed = 0.0f;
    float alpha = (float) (1.0 - std::exp(-1.0 / (signal.m_smoothing * signal.m_sampleRate / 3200.0)));
    double t = 0.0;     // Time in samples since the start of the first block
    double start = 0.0; // Start of the current symbol
    size_t n = 0;

    for (const auto& block : blocks)
    {
        double sps = signal.m_sampleRate / (block.m_baud * (1.0 + signal.m_clockError));

        for (uint8_t s : block.m_symbols)
        {
            float level = (block.m_levels == 4) ? levels4[s & 3] : ((s & 1) ? 1.0f : -1.0f);
            double end = start + sps;

            while (t < end)
            {
                smoothed += alpha * (level - smoothed);
                samples.push_back(smoothed + signal.m_dc + gauss(rng));
                t += 1.0;
                n++;
            }
            start = end;
        }
    }

    for (int i = 0; i < 2000; i++) {
        samples.push_back(0.5f * uniform(rng) + gauss(rng));
    }

    return samples;
}

void testMode(RadioPagingFlex::Mode mode, bool inverted, double sampleRate, double smoothing = 0.1)
{
    RadioPagingFlexEncoder encoder;
    encoder.setMode(mode);
    encoder.setInverted(inverted);

    std::set<std::string> sent;
    std::mt19937 rng(17 + (int) mode * 2 + (inverted ? 1 : 0));
    const char *texts[] = {"Hello", "FLEX test message 123", "A", "The quick brown fox jumps over the lazy dog"};

    for (int i = 0; i < 24; i++)
    {
        RadioPagingFlexEncoder::TxMessage m;
        bool longAddress = (i % 5) == 4;
        m.m_capcode = longAddress ? (2101249 + rng() % 1000000) : (1 + rng() % 1900000);
        int kind = i % 3;

        if (kind == 0)
        {
            m.m_type = RadioPagingFlex::Alphanumeric;
            m.m_text = texts[i % 4];
        }
        else if (kind == 1)
        {
            m.m_type = RadioPagingFlex::Numeric;
            m.m_text = std::to_string(rng() % 100000000);
        }
        else
        {
            m.m_type = RadioPagingFlex::Tone;
        }

        if (encoder.enqueue(m).m_ok)
        {
            std::string type = (kind == 0) ? "alphanumeric" : (kind == 1) ? "numeric" : "tone";
            sent.insert(key(m.m_capcode, type, m.m_type == RadioPagingFlex::Tone ? "" : m.m_text));
        }
    }

    std::vector<RadioPagingFlex::SymbolBlock> blocks;
    int frames = 0;

    for (uint64_t index = 0; (encoder.pending() > 0) || (frames < 3); index++)
    {
        RadioPagingFlexEncoder::Frame frame = encoder.buildFrame(index);
        if (!frame.m_reserved)
        {
            blocks.insert(blocks.end(), frame.m_blocks.begin(), frame.m_blocks.end());
            frames++;
        }
        if (index > 400) {
            break;
        }
    }

    Signal signal;
    signal.m_sampleRate = sampleRate;
    signal.m_seed = 5 + (unsigned) mode;
    signal.m_smoothing = smoothing;
    std::vector<float> samples = makeSamples(blocks, signal, 3000);

    RadioPagingFlexReceiver receiver;
    receiver.create(sampleRate);
    std::set<std::string> received;
    int wrongPolarity = 0;

    for (float s : samples)
    {
        receiver.process(s);
        while (receiver.hasMessage())
        {
            RadioPagingFlexDecoder::Message m = receiver.takeMessage();
            if (m.m_inverted != inverted) {
                wrongPolarity++;
            }
            received.insert(key(m.m_capcode, m.m_typeName, m.m_type == RadioPagingFlexDecoder::TypeTone ? "" : m.m_text));
        }
    }

    RadioPagingFlexReceiver::Status status = receiver.getStatus();
    char what[200];

    std::snprintf(what, sizeof(what), "FLEX RX %s %s at %.0f Hz: %u of %d frames received",
        RadioPagingFlex::modeName(mode), inverted ? "inverted" : "normal", sampleRate, status.m_frames, frames);
    check((int) status.m_frames == frames, what);

    std::vector<std::string> missing;
    std::set_difference(sent.begin(), sent.end(), received.begin(), received.end(), std::back_inserter(missing));
    std::vector<std::string> extra;
    std::set_difference(received.begin(), received.end(), sent.begin(), sent.end(), std::back_inserter(extra));

    std::snprintf(what, sizeof(what), "FLEX RX %s %s at %.0f Hz%s: %zu messages sent, %zu missing, %zu unexpected, %d wrong polarity",
        RadioPagingFlex::modeName(mode), inverted ? "inverted" : "normal", sampleRate, smoothing > 0.1 ? " narrow band" : "",
        sent.size(), missing.size(), extra.size(), wrongPolarity);
    check(missing.empty() && extra.empty() && (wrongPolarity == 0), what);

    // A narrow band (the audio filter of a receiver): the symbols run into each other, the decisions on their centre
    // decode what the decisions on the whole symbol lose
    if (smoothing > 0.1)
    {
        const RadioPagingFlexDecoder::Status& d = receiver.decoder().status();
        std::snprintf(what, sizeof(what), "FLEX RX %s narrow band: phases decoded from the centre of the symbols (%u), "
            "codewords taken from the other decision (%u)", RadioPagingFlex::modeName(mode), d.m_phasesFromCentre, d.m_codewordsFromCentre);
        check(d.m_phasesFromCentre > 0, what);
    }

    for (const auto& m : missing) {
        std::printf("   missing %s\n", m.c_str());
    }
    for (const auto& m : extra) {
        std::printf("   unexpected %s\n", m.c_str());
    }
}

void testNoiseOnly()
{
    std::mt19937 rng(3);
    std::normal_distribution<float> gauss(0.0f, 0.7f);
    RadioPagingFlexReceiver receiver;
    receiver.create(38400.0);

    for (int i = 0; i < 38400 * 60; i++) {
        receiver.process(gauss(rng));
    }

    RadioPagingFlexReceiver::Status status = receiver.getStatus();
    check((status.m_frames == 0) && !receiver.hasMessage(),
        "FLEX RX: one minute of noise gives no frame (" + std::to_string(status.m_syncs) + " false sync detections)");
}

void testPolarityFilter()
{
    RadioPagingFlexEncoder encoder;
    encoder.setMode(RadioPagingFlex::Mode1600_2);
    encoder.setInverted(true);
    RadioPagingFlexEncoder::TxMessage m;
    m.m_capcode = 123456;
    m.m_text = "x";
    encoder.enqueue(m);

    std::vector<RadioPagingFlex::SymbolBlock> blocks;
    for (uint64_t index = 0; index < 130; index++)
    {
        RadioPagingFlexEncoder::Frame frame = encoder.buildFrame(index);
        if (!frame.m_idle) {
            blocks.insert(blocks.end(), frame.m_blocks.begin(), frame.m_blocks.end());
        }
    }

    Signal signal;
    std::vector<float> samples = makeSamples(blocks, signal, 1000);
    RadioPagingFlexReceiver receiver;
    receiver.create(38400.0);
    receiver.setPolarities(true, false);
    for (float s : samples) {
        receiver.process(s);
    }
    check(receiver.getStatus().m_frames == 0, "FLEX RX: an inverted frame is not received with inverted polarity disabled");
}

// Frames with one message of a network of the given polarity
std::vector<RadioPagingFlex::SymbolBlock> networkBlocks(bool inverted, uint64_t capcode, const std::string& text, int& frames)
{
    RadioPagingFlexEncoder encoder;
    encoder.setMode(RadioPagingFlex::Mode1600_2);
    encoder.setInverted(inverted);
    RadioPagingFlexEncoder::TxMessage m;
    m.m_capcode = capcode;
    m.m_text = text;
    encoder.enqueue(m);

    std::vector<RadioPagingFlex::SymbolBlock> blocks;
    frames = 0;
    for (uint64_t index = 0; index < 130; index++)
    {
        RadioPagingFlexEncoder::Frame frame = encoder.buildFrame(index);
        if (!frame.m_idle)
        {
            blocks.insert(blocks.end(), frame.m_blocks.begin(), frame.m_blocks.end());
            frames++;
        }
    }
    return blocks;
}

// The same capcode on the other polarity is another network: both messages are output,
// neither is taken for a retransmission of the other
void testTwoNetworks()
{
    int normalFrames, invertedFrames;
    std::vector<RadioPagingFlex::SymbolBlock> blocks = networkBlocks(false, 123456, "same capcode", normalFrames);
    std::vector<RadioPagingFlex::SymbolBlock> inverted = networkBlocks(true, 123456, "same capcode", invertedFrames);
    blocks.insert(blocks.end(), inverted.begin(), inverted.end());

    Signal signal;
    std::vector<float> samples = makeSamples(blocks, signal, 1000);
    RadioPagingFlexReceiver receiver;
    receiver.create(38400.0);
    int normalMessages = 0;
    int invertedMessages = 0;

    for (float s : samples)
    {
        receiver.process(s);
        while (receiver.hasMessage())
        {
            RadioPagingFlexDecoder::Message m = receiver.takeMessage();
            if ((m.m_capcode == 123456) && (m.m_text == "same capcode")) {
                (m.m_inverted ? invertedMessages : normalMessages)++;
            }
        }
    }

    check((normalMessages == 1) && (invertedMessages == 1),
        "FLEX RX: the same capcode and message on both polarities gives one message per polarity ("
        + std::to_string(normalMessages) + " normal, " + std::to_string(invertedMessages) + " inverted)");
    check((int) receiver.decoder().status().m_frames == normalFrames + invertedFrames,
        "FLEX RX: the frames of both polarities are decoded ("
        + std::to_string(receiver.decoder().status().m_frames) + "/" + std::to_string(normalFrames + invertedFrames) + ")");
}

// Sync part symbol positions: bit sync 0-31, A 32-63, B 64-79, inverted A 80-111
std::vector<int> errorPositions(int start, int count, int stride)
{
    std::vector<int> positions;
    for (int i = 0; i < count; i++) {
        positions.push_back(start + i * stride);
    }
    return positions;
}

// One frame with one message whose sync 1 symbols are damaged at the given positions
bool receivedWithDamagedSync(const std::vector<int>& positions)
{
    RadioPagingFlexEncoder encoder;
    encoder.setMode(RadioPagingFlex::Mode3200_2);
    RadioPagingFlexEncoder::TxMessage m;
    m.m_capcode = 54321;
    m.m_text = "sync damage";
    encoder.enqueue(m);

    RadioPagingFlexEncoder::Frame frame;
    for (uint64_t index = 0; index < 200; index++)
    {
        frame = encoder.buildFrame(index);
        if (!frame.m_idle) {
            break;
        }
    }

    for (int p : positions) {
        frame.m_blocks[0].m_symbols[p] ^= 1;
    }

    Signal signal;
    signal.m_noise = 0.03f;
    std::vector<float> samples = makeSamples(frame.m_blocks, signal, 2000);
    RadioPagingFlexReceiver receiver;
    receiver.create(38400.0);
    bool found = false;

    for (float s : samples)
    {
        receiver.process(s);
        while (receiver.hasMessage()) {
            found = found || (receiver.takeMessage().m_text == "sync damage");
        }
    }

    return found;
}

// A frame whose bit sync 1 follows 16 bits that make a false sync 1 of the inverted polarity 48 bits early:
// the 16 bits and the reversals taken for an inverted A (6 bit errors), the reversals for an inverted B and A
// for an inverted "inverted A". Its frame information word fails; the true sync 1 that follows is received
bool receivedAfterFalseInvertedSync(RadioPagingFlex::Mode mode)
{
    static const uint32_t codeA[4] = {0x78f35939u, 0x84e75939u, 0x4f975939u, 0x215f5939u};
    RadioPagingFlexEncoder encoder;
    encoder.setMode(mode);
    RadioPagingFlexEncoder::TxMessage m;
    m.m_capcode = 54321;
    m.m_text = "false sync";
    encoder.enqueue(m);

    RadioPagingFlexEncoder::Frame frame;
    for (uint64_t index = 0; index < 200; index++)
    {
        frame = encoder.buildFrame(index);
        if (!frame.m_idle) {
            break;
        }
    }

    RadioPagingFlex::SymbolBlock lead;
    lead.m_baud = 1600;
    lead.m_levels = 2;
    uint32_t bits = ~(codeA[(int) mode] >> 16) & 0xffff;
    for (int i = 15; i >= 0; i--) {
        lead.m_symbols.push_back((bits >> i) & 1);
    }

    std::vector<RadioPagingFlex::SymbolBlock> blocks = {lead};
    blocks.insert(blocks.end(), frame.m_blocks.begin(), frame.m_blocks.end());

    Signal signal;
    signal.m_noise = 0.03f;
    signal.m_clockError = 0.0;
    std::vector<float> samples = makeSamples(blocks, signal, 2000);
    RadioPagingFlexReceiver receiver;
    receiver.create(38400.0);
    bool found = false;

    for (float s : samples)
    {
        receiver.process(s);
        while (receiver.hasMessage()) {
            found = found || (receiver.takeMessage().m_text == "false sync");
        }
    }

    return found;
}

void testFalseInvertedSync()
{
    const RadioPagingFlex::Mode modes[] = {RadioPagingFlex::Mode1600_2, RadioPagingFlex::Mode3200_2,
        RadioPagingFlex::Mode3200_4, RadioPagingFlex::Mode6400_4};

    for (RadioPagingFlex::Mode mode : modes)
    {
        check(receivedAfterFalseInvertedSync(mode), std::string("FLEX RX ") + RadioPagingFlex::modeName(mode)
            + ": a false inverted sync 1 before the true one does not lose the frame");
    }
}

void testDamagedSync()
{
    std::vector<int> p;

    check(receivedWithDamagedSync({}), "FLEX RX: undamaged sync 1 is received");

    p = errorPositions(32, 3, 9);
    for (int q : errorPositions(64, 3, 5)) { p.push_back(q); }
    for (int q : errorPositions(80, 2, 13)) { p.push_back(q); }
    check(receivedWithDamagedSync(p), "FLEX RX: 8 bit errors over A, B and inverted A are accepted");

    p = errorPositions(64, 12, 1);
    for (int q : errorPositions(32, 2, 11)) { p.push_back(q); }
    for (int q : errorPositions(80, 3, 9)) { p.push_back(q); }
    check(receivedWithDamagedSync(p), "FLEX RX: B destroyed (12 errors), A 2 errors and inverted A 3 errors: the two copies agree, accepted");

    p = errorPositions(64, 4, 3);
    for (int q : errorPositions(32, 3, 9)) { p.push_back(q); }
    for (int q : errorPositions(80, 3, 9)) { p.push_back(q); }
    check(!receivedWithDamagedSync(p), "FLEX RX: 3 errors in each copy of the mode code and 4 in B are rejected");
}

} // namespace

// Frames 0 to 2 back to back (or frame 1 alone), a message in frame 1 whose frame information word has three bit
// errors: two in bits 31-1 and the parity bit (BCH corrects the two, the parity is still wrong)
bool receivedWithDamagedFrameInfo(bool withPrevious)
{
    RadioPagingFlexEncoder encoder;
    encoder.setMode(RadioPagingFlex::Mode1600_2);
    RadioPagingFlexEncoder::TxMessage m;
    m.m_capcode = 54321;
    m.m_text = "frame info damage";
    m.m_frame = 1;
    encoder.enqueue(m);

    std::vector<RadioPagingFlex::SymbolBlock> blocks;

    for (uint64_t index = withPrevious ? 0 : 1; index < 3; index++)
    {
        RadioPagingFlexEncoder::Frame frame = encoder.buildFrame(index);

        if (index == 1)
        {
            // Sync 1 is bit sync (32), A (32), B (16), inverted A (32); the frame information word follows, its
            // parity bit last
            frame.m_blocks[0].m_symbols[115] ^= 1;
            frame.m_blocks[0].m_symbols[124] ^= 1;
            frame.m_blocks[0].m_symbols[143] ^= 1;
        }
        for (const auto& block : frame.m_blocks) {
            blocks.push_back(block);
        }
    }

    Signal signal;
    signal.m_noise = 0.03f;
    std::vector<float> samples = makeSamples(blocks, signal, 2000);
    RadioPagingFlexReceiver receiver;
    receiver.create(38400.0);
    bool found = false;

    for (float s : samples)
    {
        receiver.process(s);
        while (receiver.hasMessage()) {
            found = found || (receiver.takeMessage().m_text == "frame info damage");
        }
    }

    return found;
}

void testDamagedFrameInfo()
{
    check(receivedWithDamagedFrameInfo(true), "Frame information word with 3 errors (2 and parity) taken: the frame expected after the last one");
    check(!receivedWithDamagedFrameInfo(false), "Frame information word with 3 errors (2 and parity) rejected: no frame before it");
}

int main()
{
    const RadioPagingFlex::Mode modes[4] = {
        RadioPagingFlex::Mode1600_2, RadioPagingFlex::Mode3200_2, RadioPagingFlex::Mode3200_4, RadioPagingFlex::Mode6400_4
    };

    for (auto mode : modes)
    {
        testMode(mode, false, 38400.0);
        testMode(mode, true, 38400.0);
        testMode(mode, false, 44100.0);
    }

    // 4-level at 3200 baud through a narrow audio band
    testMode(RadioPagingFlex::Mode6400_4, false, 38400.0, 0.45);
    testNoiseOnly();
    testPolarityFilter();
    testTwoNetworks();
    testDamagedSync();
    testFalseInvertedSync();
    testDamagedFrameInfo();

    std::printf("%s (%d failure%s)\n", failures ? "FAILED" : "OK", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
