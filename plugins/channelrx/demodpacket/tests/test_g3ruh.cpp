// Copyright (C) 2026 SDRangel contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cmath>
#include <iostream>
#include <fstream>
#include <random>
#include <stdexcept>
#include <vector>

#include "packetdemodg3ruh.h"
#include "packetdemodframer.h"
#include "packetdemodsettings.h"
#include "dsp/phasediscri.h"
#include "util/lfsr.h"
#include "util/simpleserializer.h"

#include "packettestsignal.h"

namespace {
using namespace PacketTest;
class Receiver
{
public:
    PacketDemodG3RUH detector;
    PacketDemodFramer framer;
    PacketDemodFramer::State state;
    PhaseDiscriminators discriminator;
    std::vector<QByteArray> received;
    Receiver()
    {
        discriminator.setFMScaling(38400.0f / (2.0f * 2500.0f));
        framer.setRequirePlausible(false);
        framer.setFrameHandler([this](const QByteArray& packet, bool viaChase) {
            require(!viaChase, "G3RUH must not invoke independent-symbol Chase");
            received.push_back(packet);
            return true;
        });
    }
    void feed(const std::vector<Complex>& iq)
    {
        // Deliberately vary feed boundaries. State must survive arbitrary chunks.
        size_t pos = 0;
        while (pos < iq.size()) {
            const size_t end = std::min(iq.size(), pos + 1 + (pos * 17) % 997);
            for (; pos < end; ++pos) {
                double power;
                Real deviation;
                const float sample = discriminator.phaseDiscriminatorDelta(iq[pos], power, deviation);
                int symbol;
                if (detector.process(sample, symbol)) {
                    framer.process(state, symbol, symbol ? 1.0f : -1.0f, false);
                }
            }
        }
    }
    void reset()
    {
        detector.reset(); state.reset(); discriminator.reset(); received.clear();
    }
};

void settingsTest()
{
    PacketDemodSettings settings;
    require(settings.getBaudRate() == 1200 && settings.isMLSEEnabled(), "AFSK defaults changed");
    settings.m_mode = PacketDemodSettings::ModeG3RUH9600;
    settings.m_rfBandwidth = 23000;
    settings.m_chase = 4;
    PacketDemodSettings copy;
    require(copy.deserialize(settings.serialize()), "settings deserialize failed");
    require(copy.getBaudRate() == 9600 && !copy.isMLSEEnabled(), "mode round-trip failed");
    require(copy.m_mlse && copy.m_chase == 4 && copy.m_rfBandwidth == 23000, "saved preferences lost");
    SimpleSerializer legacy(1);
    require(copy.deserialize(legacy.final()) && copy.isAFSK(), "old preset must reset an active 9600 mode");
    for (int invalid : {-1, 2, 9600, 2147483647}) {
        SimpleSerializer bad(1); bad.writeS32(41, invalid);
        require(copy.deserialize(bad.final()) && copy.isAFSK(), "invalid preset mode not normalized");
        require(!PacketDemodSettings::isValidMode(invalid), "invalid API mode accepted");
    }
    copy.applySettings(QStringList{"mode"}, settings);
    require(copy.getBaudRate() == 9600 && copy.m_rfBandwidth == 12500, "mode-only update changed bandwidth");
    require(!copy.deserialize(QByteArray("invalid")) && copy.isAFSK(), "invalid preset fallback failed");
}

void run()
{
    settingsTest();
    std::vector<QByteArray> expected;
    for (int i = 0; i < 40; ++i) { expected.push_back(frame(i, i % 4 == 0 ? 256 : 20 + i)); }
    const auto symbols = radioSymbols(hdlc(expected), true);
    int cases = 0;
    for (bool shaped : {false, true}) {
        for (bool inverted : {false, true}) {
            for (double timing : {0.0, 0.37, 1.25, 2.5, 3.75}) {
                Channel c; c.gaussian = shaped; c.inverted = inverted; c.timing = timing;
                Receiver rx; rx.feed(waveform(symbols, c));
                require(rx.received == expected, "clean waveform / phase / polarity failure");
                ++cases;
            }
        }
    }
    for (double ppm : {-1000.0, -250.0, 250.0, 1000.0}) {
        for (double offset : {-1500.0, 0.0, 1500.0}) {
            Channel c; c.ppm = ppm; c.offset = offset; c.timing = 0.61; c.drift = 20.0;
            Receiver rx; rx.feed(waveform(symbols, c));
            require(rx.received == expected, "clock offset / carrier drift failure"); ++cases;
        }
    }
    for (double deviation : {1500.0, 2500.0, 3000.0, 4500.0}) {
        Channel c; c.deviation = deviation; c.offset = 700; c.noise = 0.03;
        Receiver rx; rx.feed(waveform(symbols, c));
        require(rx.received == expected, "deviation / IQ noise failure"); ++cases;
    }
    Receiver rx;
    Channel c;
    const auto signal = waveform(symbols, c);
    rx.feed(std::vector<Complex>(signal.begin(), signal.begin() + signal.size() / 3));
    rx.reset(); rx.feed(signal);
    require(rx.received == expected, "reset / reacquisition failure");
    rx.reset();
    // Idle and arbitrary noise must not overflow HDLC counters or emit packets.
    for (int i = 0; i < 100000; ++i) {
        rx.framer.process(rx.state, 0, -1.0f, false);
    }
    require(rx.received.empty(), "idle carrier emitted a frame");
    rx.reset();
    rx.feed(waveform(radioSymbols(hdlc(expected), false), c));
    require(rx.received.empty(), "unscrambled stream accepted as G3RUH");
    auto damaged = expected; damaged[0][18] = char(damaged[0][18] ^ 1);
    rx.reset(); rx.feed(waveform(radioSymbols(hdlc(damaged), true), c));
    require(rx.received == std::vector<QByteArray>(expected.begin() + 1, expected.end()), "bad CRC was accepted or following frames lost");
    // Ensure the actual common framer still decodes plain AFSK NRZI levels.
    rx.reset();
    for (int level : radioSymbols(hdlc(expected), false)) {
        rx.framer.process(rx.state, level, level ? 1.0f : -1.0f, true);
    }
    require(rx.received == expected, "AFSK framing regression");
    PacketDemodFramer chase;
    PacketDemodFramer::State chaseState;
    std::vector<QByteArray> corrected;
    int recovered = 0;
    chase.setFrameHandler([&](const QByteArray& packet, bool viaChase) {
        corrected.push_back(packet);
        recovered += viaChase ? 1 : 0;
        return true;
    });
    const auto afskLevels = radioSymbols(hdlc(expected), false);
    for (size_t i = 0; i < afskLevels.size(); ++i) {
        const bool error = i == 600;
        const int level = afskLevels[i] ^ error;
        const float confidence = error ? 0.0001f : 1.0f;
        chase.process(chaseState, level, level ? confidence : -confidence, true);
    }
    require(corrected == expected && recovered == 1, "AFSK Chase recovery regression");
    std::cout << "PASS: " << cases << " waveform cases x 40 exact frames; settings, reset, CRC rejection, wrong mode and AFSK framing/Chase\n";
}
}

int main(int argc, char** argv)
{
    try {
        if (argc == 2) {
            // Optional independent-modem fixture: mono signed 16-bit PCM at
            // 38400 Hz, without a WAV header (e.g. Dire Wolf gen_packets).
            std::ifstream input(argv[1], std::ios::binary);
            require(input.good(), "cannot open PCM fixture");
            Receiver rx;
            char bytes[2];
            while (input.read(bytes, 2)) {
                const uint16_t raw = static_cast<unsigned char>(bytes[0])
                    | (static_cast<unsigned char>(bytes[1]) << 8);
                const float sample = (raw < 32768 ? int(raw) : int(raw) - 65536) / 16384.0f;
                int symbol;
                if (rx.detector.process(sample, symbol)) {
                    rx.framer.process(rx.state, symbol, symbol ? 1.0f : -1.0f, false);
                }
            }
            for (const auto& p : rx.received) { std::cout << p.toHex().constData() << '\n'; }
            require(!rx.received.empty(), "independent fixture decoded no frames");
        } else {
            run();
        }
    }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
    return 0;
}
