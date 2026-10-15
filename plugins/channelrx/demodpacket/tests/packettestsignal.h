// Copyright (C) 2026 SDRangel contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QByteArray>
#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include "dsp/dsptypes.h"
#include "util/lfsr.h"

namespace PacketTest {
constexpr double pi = 3.14159265358979323846;
void require(bool ok, const char* message)
{
    if (!ok) { throw std::runtime_error(message); }
}

// Independent bit-at-a-time CRC-16/X.25 oracle (not the production CRC table).
QByteArray frame(int index, int length)
{
    QByteArray bytes;
    for (const char c : std::string("APRS  ")) { bytes.append(char(c << 1)); }
    bytes.append(char(0x60));
    for (const char c : std::string("N0CALL")) { bytes.append(char(c << 1)); }
    bytes.append(char(0x61));
    bytes.append(char(0x03));
    bytes.append(char(0xf0));
    std::mt19937 rng(1234 + index);
    for (int i = 0; i < length; ++i) {
        bytes.append(char(i % 7 == 0 ? 0xff : rng() & 0xff));
    }
    uint16_t crc = 0xffff;
    for (const unsigned char b : bytes) {
        crc ^= b;
        for (int i = 0; i < 8; ++i) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0x8408 : 0);
        }
    }
    crc ^= 0xffff;
    bytes.append(char(crc & 0xff));
    bytes.append(char(crc >> 8));
    return bytes;
}

std::vector<int> hdlc(const std::vector<QByteArray>& frames)
{
    std::vector<int> bits;
    auto flag = [&]() { for (int j = 0; j < 8; ++j) { bits.push_back((0x7e >> j) & 1); } };
    for (int j = 0; j < 64; ++j) { flag(); }
    for (const auto& bytes : frames)
    {
        int ones = 0;
        for (const unsigned char b : bytes) {
            for (int j = 0; j < 8; ++j) {
                const int bit = (b >> j) & 1;
                bits.push_back(bit);
                ones = bit ? ones + 1 : 0;
                if (ones == 5) { bits.push_back(0); ones = 0; }
            }
        }
        flag(); // shared closing/opening flag, no reset of scrambler/NRZI
    }
    for (int j = 0; j < 16; ++j) { flag(); }
    return bits;
}

std::vector<int> radioSymbols(const std::vector<int>& bits, bool scrambled)
{
    LFSR tx(0x10800, 0); // The actual SDRangel Packet Modulator scrambler.
    int level = 0;
    std::vector<int> symbols;
    for (const int bit : bits) {
        level ^= !bit;
        symbols.push_back(scrambled ? tx.scramble(level) : level);
    }
    return symbols;
}

struct Channel
{
    double sampleRate = 38400.0;
    double timing = 0.0; // sample offset, includes fractions of a sample
    double ppm = 0.0;
    double offset = 0.0; // Hz
    double drift = 0.0; // Hz/second
    double deviation = 3000.0;
    double noise = 0.0; // standard deviation per I/Q component at unit amplitude
    bool inverted = false;
    bool gaussian = true;
};

// Sample an independently generated Gaussian-filtered NRZ waveform at an
// arbitrary clock. Integrate the Gaussian over each symbol (BT=0.5). This does
// not reuse the receive FIR or PLL and can reveal clock/ordering errors.
std::vector<Complex> waveform(const std::vector<int>& symbols, const Channel& c)
{
    const double samplesPerSymbol = (c.sampleRate / 9600.0) / (1.0 + c.ppm * 1e-6);
    const double a = std::sqrt(2.0) * pi * 0.5 / std::sqrt(std::log(2.0));
    const size_t count = static_cast<size_t>(symbols.size() * samplesPerSymbol);
    std::vector<Complex> iq;
    iq.reserve(count);
    std::mt19937 rng(0x9600);
    std::normal_distribution<float> normal(0.0f, 1.0f);
    double phase = 0.4;
    for (size_t i = 0; i < count; ++i)
    {
        const double t = (i + c.timing) / samplesPerSymbol;
        const int center = static_cast<int>(std::floor(t));
        double value = 0.0;
        if (c.gaussian) {
            for (int j = center - 3; j <= center + 3; ++j) {
                const int p = std::max(0, std::min(j, static_cast<int>(symbols.size()) - 1));
                value += (symbols[p] ? 1.0 : -1.0)
                    * 0.5 * (std::erf(a * (t - j)) - std::erf(a * (t - j - 1)));
            }
        } else {
            const int p = std::max(0, std::min(center, static_cast<int>(symbols.size()) - 1));
            value = symbols[p] ? 1.0 : -1.0;
        }
        phase += 2.0 * pi * (c.offset + c.drift * i / c.sampleRate
            + (c.inverted ? -1.0 : 1.0) * c.deviation * value) / c.sampleRate;
        iq.emplace_back(std::cos(phase) + c.noise * normal(rng),
            std::sin(phase) + c.noise * normal(rng));
    }
    return iq;
}

}
