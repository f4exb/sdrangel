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

// Self test of the FSK modulator: the encoders' symbols are modulated at channel sample
// rates the up channelizer can give (38400 Hz requested from several device rates), FM
// demodulated at that rate and received by the POCSAG, GSC and FLEX receivers. The peak
// deviation and the power outside the occupied bandwidth are measured.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "dsp/firfilter.h"

#include "radiopagingfskmod.h"
#include "radiopagingpocsag.h"
#include "radiopagingpocsagreceiver.h"
#include "radiopaginggsc.h"
#include "radiopaginggscreceiver.h"
#include "radiopagingflex.h"
#include "radiopagingflexscheduler.h"
#include "radiopagingflexreceiver.h"

namespace {

int passes = 0;
int failures = 0;

void check(bool ok, const std::string& what)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what.c_str());
    (ok ? passes : failures)++;
}

// Channel sample rates of a 38400 Hz request: the internal rate itself, and baseband rates
// divided by a power of two (37.5 kHz from 2.4 MS/s, 48 kHz from 768 kS/s, 62.5 kHz from
// 1 MS/s, 75 kHz from 2.4 MS/s)
const int channelRates[] = {37500, 38400, 48000, 62500, 75000};

struct Signal
{
    std::vector<Complex> m_iq;
    int m_rate;
};

// Carrier, the symbol blocks, carrier
Signal modulate(int rate, double deviation, double shaping, const std::vector<std::pair<int, std::vector<float>>>& blocks)
{
    RadioPagingFskMod mod;
    Signal s;

    mod.setChannelSampleRate(rate);
    mod.setDeviation(deviation);
    mod.setShaping(shaping);
    s.m_rate = rate;

    for (int i = 0; i < rate / 10; i++) {
        s.m_iq.push_back(mod.next());
    }
    for (const auto& b : blocks) {
        mod.push(b.first, b.second);
    }
    while (mod.busy()) {
        s.m_iq.push_back(mod.next());
    }
    for (int i = 0; i < rate / 5; i++) {
        s.m_iq.push_back(mod.next());
    }

    return s;
}

// Complex white noise of the same power in 38400 Hz at every rate
void addNoise(Signal& s, float sigma38400, unsigned seed)
{
    std::mt19937 rng(seed);
    std::normal_distribution<float> gauss(0.0f, sigma38400 * std::sqrt((float) s.m_rate / 38400.0f));

    for (Complex& c : s.m_iq) {
        c += Complex(gauss(rng), gauss(rng));
    }
}

// FM discriminator: frequency in units of the deviation
std::vector<float> discriminate(const Signal& s, double deviation)
{
    std::vector<float> out;
    Complex last(1.0f, 0.0f);
    double scale = s.m_rate / (2.0 * M_PI * deviation);

    for (const Complex& c : s.m_iq)
    {
        out.push_back((float) (std::arg(c * std::conj(last)) * scale));
        last = c;
    }

    return out;
}

// Instantaneous frequency of the noiseless signal, Hz, after the first 50 ms of carrier (the
// filters fill from zero): the deviation of the outer symbols (median of |f| above 0.8 of the
// nominal deviation: the symbol plateaus) and the largest value (overshoot at transitions)
struct Deviation
{
    double m_symbols;
    double m_peak;
};

Deviation measureDeviation(const Signal& s, double nominal)
{
    std::vector<double> outer;
    double peak = 0.0;

    for (size_t i = s.m_rate / 20 + 1; i < s.m_iq.size(); i++)
    {
        double f = std::fabs(std::arg(s.m_iq[i] * std::conj(s.m_iq[i - 1]))) * s.m_rate / (2.0 * M_PI);

        peak = std::max(peak, f);
        if (f > 0.8 * nominal) {
            outer.push_back(f);
        }
    }

    std::sort(outer.begin(), outer.end());
    return Deviation{outer.empty() ? 0.0 : outer[outer.size() / 2], peak};
}

// Share of the power outside +/- bandwidth / 2: measured through a sharp low pass
double outOfBand(const Signal& s, double bandwidth)
{
    Lowpass<Complex> lpf;
    double total = 0.0;
    double inBand = 0.0;

    lpf.create(1001, s.m_rate, bandwidth / 2.0);

    for (size_t i = 0; i < s.m_iq.size(); i++)
    {
        Complex f = lpf.filter(s.m_iq[i]);

        if (i >= 1001)
        {
            total += std::norm(s.m_iq[i - 500]);
            inBand += std::norm(f);
        }
    }

    return 1.0 - inBand / total;
}

// The deviation of the symbols within 3%, spikes within 10%, under 1% of the power outside
// the bandwidth (0.5% each side: STD-43A 2.1.7 occupied bandwidth)
bool spectrumOk(const Signal& s, double deviation, double bandwidth, std::string& text)
{
    Deviation d = measureDeviation(s, deviation);
    double oob = outOfBand(s, bandwidth);
    char buf[160];

    std::snprintf(buf, sizeof(buf), "deviation %.0f Hz (peak %.0f Hz), %.3f%% outside %.0f Hz",
        d.m_symbols, d.m_peak, oob * 100.0, bandwidth);
    text = buf;

    return (std::fabs(d.m_symbols - deviation) < 0.03 * deviation) && (d.m_peak < 1.1 * deviation) && (oob < 0.01);
}

std::vector<float> nrz(const std::vector<uint8_t>& bits, bool oneIsPositive)
{
    std::vector<float> levels;

    for (uint8_t b : bits) {
        levels.push_back(((b != 0) == oneIsPositive) ? 1.0f : -1.0f);
    }

    return levels;
}

// POCSAG: binary 1 is a negative deviation (normal polarity). Shaping cutoff at the baud
// rate, RF bandwidth 2 x (deviation + baud)
void testPocsag()
{
    const double deviation = 4500.0;
    const int bauds[] = {512, 1200, 2400};

    for (int baud : bauds)
    {
        RadioPagingPocsag::TxMessage alpha, numeric;
        alpha.m_address = 1234567;
        alpha.m_function = 3;
        alpha.m_type = RadioPagingPocsag::Alphanumeric;
        alpha.m_text = "POCSAG MODULATOR TIMING 0123456789";
        numeric.m_address = 7654;
        numeric.m_function = 0;
        numeric.m_type = RadioPagingPocsag::Numeric;
        numeric.m_text = "0123456789";
        RadioPagingPocsag::EncodeResult enc = RadioPagingPocsag::encode({alpha, numeric});
        double bandwidth = 2.0 * (deviation + baud);

        for (int rate : channelRates)
        {
            for (int noisy = 0; noisy < 2; noisy++)
            {
                Signal s = modulate(rate, deviation, baud, {{baud, nrz(enc.m_bits, false)}});

                if (!noisy)
                {
                    std::string text;
                    bool ok = spectrumOk(s, deviation, bandwidth, text);
                    check(ok, "POCSAG " + std::to_string(baud) + " at " + std::to_string(rate) + " Hz: " + text);
                }
                else
                {
                    addNoise(s, 0.35f, (unsigned) (baud + rate));
                }

                RadioPagingPocsagReceiver rx;
                rx.create(rate, baud);
                int ok = 0, errors = 0;

                for (float f : discriminate(s, deviation))
                {
                    rx.process(f);

                    while (rx.hasMessage())
                    {
                        RadioPagingPocsagDecoder::Message m = rx.takeMessage();
                        std::string text = RadioPagingPocsag::interpret(m.m_rawData, m.m_rawBits).m_text;
                        bool good = ((m.m_address == alpha.m_address) && (text == alpha.m_text))
                            || ((m.m_address == numeric.m_address) && (text == numeric.m_text));
                        ok += good ? 1 : 0;
                        errors += m.m_errorsCorrected;
                    }
                }

                check(ok == 2, "POCSAG " + std::to_string(baud) + " at " + std::to_string(rate) + " Hz"
                    + (noisy ? " with noise" : "") + ": " + std::to_string(ok) + "/2 messages, "
                    + std::to_string(errors) + " bits corrected");
            }
        }
    }
}

RadioPagingGsc::TxMessage gsc(const char *address, int number, RadioPagingGsc::MessageType type, const char *text)
{
    RadioPagingGsc::TxMessage m;
    m.m_address = address;
    m.m_addressNumber = number;
    m.m_type = type;
    m.m_text = text;
    return m;
}

// GSC: binary 1 is a positive deviation. 600 baud symbols (300 bit/s words are sent as
// pairs of symbols), shaping cutoff 600 Hz, RF bandwidth 2 x (deviation + 600)
void testGsc()
{
    const double deviation = 4500.0;
    const double bandwidth = 2.0 * (deviation + 600.0);
    std::vector<RadioPagingGsc::TxMessage> messages = {
        gsc("219789", 2, RadioPagingGsc::Numeric, "5551234"),
        gsc("219457", 4, RadioPagingGsc::Alphanumeric, "GSC MODULATOR TIMING")
    };
    RadioPagingGsc::EncodeResult enc = RadioPagingGsc::encode(messages);

    for (int rate : channelRates)
    {
        for (int noisy = 0; noisy < 2; noisy++)
        {
            Signal s = modulate(rate, deviation, 600.0, {{600, nrz(enc.m_bits, true)}});

            if (!noisy)
            {
                std::string text;
                bool ok = spectrumOk(s, deviation, bandwidth, text);
                check(ok, "GSC at " + std::to_string(rate) + " Hz: " + text);
            }
            else
            {
                addNoise(s, 0.35f, (unsigned) rate);
            }

            RadioPagingGscReceiver rx;
            rx.create(rate);
            int ok = 0;

            for (float f : discriminate(s, deviation))
            {
                rx.process(f);

                while (rx.hasMessage())
                {
                    RadioPagingGscDecoder::Message m = rx.takeMessage();
                    std::string text = RadioPagingGsc::interpret(m.m_rawData, m.m_rawBits).m_text;

                    for (const auto& tx : messages) {
                        ok += ((m.m_address == tx.m_address) && (m.m_addressNumber == tx.m_addressNumber) && (text == tx.m_text)) ? 1 : 0;
                    }
                }
            }
            rx.flush();
            while (rx.hasMessage()) {
                rx.takeMessage();
            }

            check(ok == 2, "GSC at " + std::to_string(rate) + " Hz" + (noisy ? " with noise" : "") + ": "
                + std::to_string(ok) + "/2 messages");
        }
    }
}

// FLEX: shaping cutoff 3.2 kHz for all modes (STD-43A 2.1.9), RF bandwidth 16 kHz (2.1.7)
void testFlex()
{
    static const float levels4[4] = {-1.0f, -1.0f / 3.0f, 1.0f / 3.0f, 1.0f};
    const double deviation = 4800.0;
    const double bandwidth = 16000.0;
    const RadioPagingFlex::Mode modes[] = {RadioPagingFlex::Mode1600_2, RadioPagingFlex::Mode3200_2,
        RadioPagingFlex::Mode3200_4, RadioPagingFlex::Mode6400_4};
    const uint64_t capcodes[] = {123456, 234567};

    for (RadioPagingFlex::Mode mode : modes)
    {
        RadioPagingFlexEncoder encoder;
        encoder.setMode(mode);

        for (uint64_t capcode : capcodes)
        {
            RadioPagingFlexEncoder::TxMessage m;
            m.m_capcode = capcode;
            m.m_type = RadioPagingFlex::Alphanumeric;
            m.m_text = "FLEX modulator timing";
            encoder.enqueue(m);
        }

        std::vector<std::pair<int, std::vector<float>>> blocks;

        for (uint64_t index = 0; (encoder.pending() > 0) && (index < 50); index++)
        {
            RadioPagingFlexEncoder::Frame frame = encoder.buildFrame(index);

            for (const auto& block : frame.m_blocks)
            {
                std::vector<float> levels;
                for (uint8_t sym : block.m_symbols) {
                    levels.push_back((block.m_levels == 4) ? levels4[sym & 3] : ((sym & 1) ? 1.0f : -1.0f));
                }
                blocks.push_back({block.m_baud, levels});
            }
        }

        for (int rate : channelRates)
        {
            for (int noisy = 0; noisy < 2; noisy++)
            {
                Signal s = modulate(rate, deviation, 3200.0, blocks);

                if (!noisy)
                {
                    std::string text;
                    bool ok = spectrumOk(s, deviation, bandwidth, text);
                    check(ok, std::string("FLEX ") + RadioPagingFlex::modeName(mode) + " at " + std::to_string(rate)
                        + " Hz: " + text);
                }
                else
                {
                    addNoise(s, 0.2f, (unsigned) rate + (unsigned) mode);
                }

                RadioPagingFlexReceiver rx;
                rx.create(rate);
                int ok = 0;
                std::vector<float> fm = discriminate(s, deviation);

                for (size_t i = 0; i < fm.size(); i++)
                {
                    rx.process(fm[i], std::norm(s.m_iq[i]));

                    while (rx.hasMessage())
                    {
                        RadioPagingFlexDecoder::Message msg = rx.takeMessage();
                        ok += (msg.m_text == "FLEX modulator timing") ? 1 : 0;
                    }
                }

                check(ok == 2, std::string("FLEX ") + RadioPagingFlex::modeName(mode) + " at " + std::to_string(rate)
                    + " Hz" + (noisy ? " with noise" : "") + ": " + std::to_string(ok) + "/2 messages");
            }
        }
    }
}

} // namespace

// 750 bps does not divide 38400: a block has exactly its length at every channel rate, and
// the symbols alternate at 375 Hz. A block with its own deviation sets the FM sensitivity
// from its first symbol
void testFractionalAndDeviation()
{
    for (int rate : channelRates)
    {
        RadioPagingFskMod mod;
        mod.setChannelSampleRate(rate);
        mod.setDeviation(4500.0);
        mod.setShaping(750.0);

        std::vector<float> levels;
        for (int i = 0; i < 300; i++) {
            levels.push_back((i & 1) ? -1.0f : 1.0f);
        }

        // Samples from the first to the last symbol: 300 symbols of 1 / 750 s = 0.4 s
        mod.push(750.0, levels, 750.0, 3000.0);
        size_t samples = 0;
        while (mod.queued() > 0)
        {
            mod.next();
            samples++;
        }

        double seconds = (double) samples / rate;
        char what[160];
        std::snprintf(what, sizeof(what), "750 bps block of 300 symbols lasts 0.4 s at %d Hz (%.5f s)", rate, seconds);
        check(std::fabs(seconds - 0.4) < 0.0005, what);
    }

    // Deviation of the block: peak frequency of a 1,0 run at 750 bps, 3000 Hz then 4500 Hz
    RadioPagingFskMod mod;
    mod.setChannelSampleRate(48000);
    mod.setDeviation(4500.0);
    mod.setShaping(750.0);
    std::vector<float> run(100, 1.0f);
    mod.push(750.0, run, 750.0, 3000.0);
    mod.push(750.0, run, 750.0, 4500.0);
    Signal t;
    t.m_rate = 48000;
    while (mod.busy()) {
        t.m_iq.push_back(mod.next());
    }
    std::vector<float> f = discriminate(t, 1000.0);
    size_t half = f.size() / 2;
    double first = f[half / 2] * 1000.0;
    double second = f[half + half / 2] * 1000.0;
    char what[160];
    std::snprintf(what, sizeof(what), "Block deviation: 3000 Hz then 4500 Hz (%.0f, %.0f)", first, second);
    check((std::fabs(first - 3000.0) < 30.0) && (std::fabs(second - 4500.0) < 45.0), what);

    // The last symbol is held one symbol more after it, on its deviation, before busy() ends
    {
        RadioPagingFskMod hold;
        hold.setChannelSampleRate(48000);
        hold.setDeviation(4500.0);
        hold.setShaping(600.0);
        hold.push(600.0, std::vector<float>{1.0f, -1.0f, 1.0f, -1.0f}, 600.0, 4500.0);
        size_t samples = 0;
        int afterLast = 0;
        Signal h;
        h.m_rate = 48000;

        while (hold.busy())
        {
            h.m_iq.push_back(hold.next());
            samples++;
            if (hold.queued() == 0) {
                afterLast++;
            }
        }

        std::vector<float> d = discriminate(h, 1000.0);
        double end = d[d.size() - 2] * 1000.0;
        std::snprintf(what, sizeof(what), "Last symbol held one symbol (80 samples) more: %d samples after it, %.0f Hz at the end",
            afterLast, end);
        check((afterLast >= 80) && (end < -4000.0), what);
    }

    // After a block with a fractional symbol clock the next block still starts with its own
    // deviation (the fraction carried over is not zero)
    {
        RadioPagingFskMod fraction;
        fraction.setChannelSampleRate(48000);
        fraction.setDeviation(4800.0);
        fraction.setShaping(3200.0);
        fraction.push(1601.37, std::vector<float>(40, 1.0f), 3200.0, 4800.0);
        fraction.push(750.0, std::vector<float>(100, 1.0f), 750.0, 3000.0);
        Signal u;
        u.m_rate = 48000;
        while (fraction.busy()) {
            u.m_iq.push_back(fraction.next());
        }
        std::vector<float> g = discriminate(u, 1000.0);
        double last = g[g.size() - 200] * 1000.0;       // Inside the second block, settled
        std::snprintf(what, sizeof(what), "Deviation of a block after a fractional rate block: 3000 Hz (%.0f)", last);
        check(std::fabs(last - 3000.0) < 30.0, what);
    }
}

int main()
{
    testPocsag();
    testGsc();
    testFlex();
    testFractionalAndDeviation();

    std::printf("%d passed, %d failed\n", passes, failures);
    return failures ? 1 : 0;
}
