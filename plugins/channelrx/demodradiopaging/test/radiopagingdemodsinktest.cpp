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

// Self test of the radio paging demodulator sink: POCSAG, FLEX and GSC transmissions are
// FM modulated one after the other into one IQ stream, received through the channel
// sink with all legs running, and the records are compared with what was sent.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <functional>
#include <random>
#include <set>
#include <string>
#include <vector>

#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThreadPool>

#include "dsp/dsptypes.h"
#include "util/messagequeue.h"
#include "maincore.h"

#include "radiopagingpocsag.h"
#include "radiopagingflex.h"
#include "radiopagingflexscheduler.h"
#include "radiopaginggsc.h"
#include "radiopagingwav.h"
#include "radiopagingfskmod.h"

#include "radiopagingvoicestore.h"
#include "radiopagingdemodsink.h"

namespace {

int passes = 0;
int failures = 0;
bool dcCorrection = true;   // The sweeps: legs following the measured carrier offset (DC correction)

void check(bool ok, const std::string& what)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (ok) {
        passes++;
    } else {
        failures++;
    }
}

const double sampleRate = RadioPagingDemodSettings::m_channelSampleRate;

// FM modulator: instantaneous frequency in Hz to IQ samples, with noise
struct Modulator
{
    double m_phase = 0.0;
    float m_smoothed = 0.0f;
    double m_offset = 0.0;              //!< Carrier frequency offset, Hz
    std::mt19937 m_rng{3};
    std::normal_distribution<float> m_noise{0.0f, 0.05f};
    SampleVector m_samples;

    void sample(double frequency)
    {
        m_phase += 2.0 * M_PI * (frequency + m_offset) / sampleRate;
        double a = 0.5 * SDR_RX_SCALED;
        m_samples.push_back(Sample(
            (FixReal) (a * (std::cos(m_phase) + m_noise(m_rng))),
            (FixReal) (a * (std::sin(m_phase) + m_noise(m_rng)))));
    }

    // Levels -1 to 1 at the baud rate, smoothed, times the deviation
    void symbols(const std::vector<float>& levels, double baud, double deviation)
    {
        double sps = sampleRate / baud;
        float alpha = (float) (1.0 - std::exp(-1.0 / (0.15 * sps)));
        double t = 0.0;

        for (float level : levels)
        {
            double end = t + sps;
            for (; t < end; t += 1.0)
            {
                m_smoothed += alpha * (level - m_smoothed);
                sample(m_smoothed * deviation);
            }
            t = end;
        }
    }

    void noise(int n)
    {
        std::uniform_real_distribution<double> uniform(-8000.0, 8000.0);
        for (int i = 0; i < n; i++) {
            sample(uniform(m_rng));
        }
    }
};

struct Expected
{
    std::string m_protocol;
    std::string m_mode;
    std::string m_address;
    bool m_inverted;
};

std::string key(const std::string& protocol, const std::string& mode, const std::string& address, bool inverted)
{
    return protocol + " " + mode + " " + address + (inverted ? " inverted" : " normal");
}

void addPocsag(Modulator& mod, std::set<std::string>& sent, int baud, uint32_t address, const char *text, bool inverted)
{
    RadioPagingPocsag::TxMessage m;
    m.m_address = address;
    m.m_function = 3;
    m.m_type = RadioPagingPocsag::Alphanumeric;
    m.m_text = text;
    RadioPagingPocsag::EncodeResult enc = RadioPagingPocsag::encode({ m });
    std::vector<float> levels;

    // Binary 1 is a negative deviation
    for (uint8_t b : enc.m_bits) {
        levels.push_back((b ? -1.0f : 1.0f) * (inverted ? -1.0f : 1.0f));
    }
    mod.symbols(levels, baud, 4500.0);
    mod.noise(4000);

    char a[16];
    std::snprintf(a, sizeof(a), "%07uD", address);     // Function 3 is D
    sent.insert(key("pocsag", std::to_string(baud), a, inverted));
}

void addFlex(Modulator& mod, std::set<std::string>& sent, RadioPagingFlex::Mode mode, const std::vector<uint64_t>& capcodes)
{
    static const float levels4[4] = {-1.0f, -1.0f/3.0f, 1.0f/3.0f, 1.0f};
    RadioPagingFlexEncoder encoder;
    encoder.setMode(mode);

    for (uint64_t capcode : capcodes)
    {
        RadioPagingFlexEncoder::TxMessage m;
        m.m_capcode = capcode;
        m.m_type = RadioPagingFlex::Alphanumeric;
        m.m_text = "FLEX through the channel";
        if (encoder.enqueue(m).m_ok)
        {
            char a[16];
            std::snprintf(a, sizeof(a), "%010llu", (unsigned long long) capcode);
            sent.insert(key("flex", RadioPagingFlex::modeName(mode), a, false));
        }
    }

    for (uint64_t index = 0; (encoder.pending() > 0) && (index < 50); index++)
    {
        RadioPagingFlexEncoder::Frame frame = encoder.buildFrame(index);

        for (const auto& block : frame.m_blocks)
        {
            std::vector<float> levels;
            for (uint8_t s : block.m_symbols) {
                levels.push_back((block.m_levels == 4) ? levels4[s & 3] : ((s & 1) ? 1.0f : -1.0f));
            }
            mod.symbols(levels, block.m_baud, 4800.0);
        }
    }
    mod.noise(4000);
}

void addGsc(Modulator& mod, std::set<std::string>& sent, const std::vector<RadioPagingGsc::TxMessage>& messages)
{
    RadioPagingGsc::EncodeResult enc = RadioPagingGsc::encode(messages);
    std::vector<float> levels;

    // Binary 1 is a positive deviation
    for (uint8_t b : enc.m_bits) {
        levels.push_back(b ? 1.0f : -1.0f);
    }
    mod.symbols(levels, 600, 4500.0);
    mod.noise(4000);

    // Capcode and function
    for (const auto& m : messages) {
        sent.insert(key("gsc", "600", m.m_address + "/" + std::to_string(m.m_addressNumber), false));
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

// Pages of every protocol received with a carrier frequency offset (transmitter and receiver
// oscillators): records received per protocol at a channel bandwidth
std::vector<int> receiveWithOffset(double offset, float rfBandwidth, std::vector<float>& measured)
{
    Modulator mod;
    std::set<std::string> sent;

    mod.m_offset = offset;
    mod.noise(3000);
    addPocsag(mod, sent, 1200, 1234567, "POCSAG WITH A FREQUENCY OFFSET", false);
    addPocsag(mod, sent, 2400, 2000001, "POCSAG 2400 OFFSET", false);
    addFlex(mod, sent, RadioPagingFlex::Mode1600_2, { 123456 });
    addFlex(mod, sent, RadioPagingFlex::Mode6400_4, { 345678 });
    addGsc(mod, sent, { gsc("100123", 3, RadioPagingGsc::Alphanumeric, "GSC OFFSET") });

    MessageQueue queue;
    RadioPagingDemodSink sink;
    RadioPagingDemodSettings settings;
    settings.m_rfBandwidth = rfBandwidth;
    settings.m_pocsagDcCorrection = dcCorrection;
    settings.m_flexDcCorrection = dcCorrection;
    settings.m_gscDcCorrection = dcCorrection;
    sink.setMessageQueueToChannel(&queue);
    sink.applyChannelSettings((int) sampleRate, 0, true);
    sink.applySettings(QStringList(), settings, true);

    for (size_t i = 0; i < mod.m_samples.size(); i += 4096)
    {
        size_t end = std::min(mod.m_samples.size(), i + 4096);
        sink.feed(mod.m_samples.begin() + i, mod.m_samples.begin() + end);
    }

    RadioPagingDemodSink::Status status = sink.getStatus();
    measured.assign(3, 1e9f);
    for (int p = 0; p < 3; p++)
    {
        if (status.m_offsetValid[p]) {
            measured[p] = status.m_offset[p];
        }
    }

    std::vector<int> received(3, 0);
    Message *message;

    while ((message = queue.pop()))
    {
        if (MainCore::MsgMessageRecord::match(*message))
        {
            const MessageRecord& r = ((MainCore::MsgMessageRecord *) message)->getRecord();
            if (sent.count(key(r.m_protocol.toStdString(), r.m_mode.toStdString(), r.m_address.toStdString(),
                r.m_polarity == MessageRecord::PolarityInverted)))
            {
                received[(r.m_protocol == "pocsag") ? 0 : (r.m_protocol == "flex") ? 1 : 2]++;
            }
        }
        delete message;
    }

    return received;
}

// One carrier is one protocol at a time: the data of FLEX frames (any bits, among them by chance a POCSAG sync codeword
// and an address of 2 corrected bits) gives no POCSAG or GSC message, and a POCSAG and a GSC transmission after them are
// received (the decoders kept hunting). Found with a radio round trip: POCSAG tone pages in a FLEX group transmission
// FLEX frames of the 4 modes full of text of every character (any data: the bits of a lot of different words)
void addFlexFrames(Modulator& mod, std::set<std::string>& sent)
{
    std::mt19937 rng(20261120);
    std::uniform_int_distribution<int> character(32, 126);
    static const float levels4[4] = {-1.0f, -1.0f/3.0f, 1.0f/3.0f, 1.0f};

    for (RadioPagingFlex::Mode mode : {RadioPagingFlex::Mode1600_2, RadioPagingFlex::Mode3200_2, RadioPagingFlex::Mode3200_4, RadioPagingFlex::Mode6400_4})
    {
        RadioPagingFlexEncoder encoder;
        encoder.setMode(mode);

        // Frames full of text of every character: the bits of a lot of different words
        for (int i = 0; i < 24; i++)
        {
            RadioPagingFlexEncoder::TxMessage m;
            m.m_capcode = 300000 + 17 * i + 1000 * (int) mode;
            m.m_type = RadioPagingFlex::Alphanumeric;

            for (int c = 0; c < 200; c++) {
                m.m_text += (char) character(rng);
            }

            if (encoder.enqueue(m).m_ok)
            {
                char a[16];
                std::snprintf(a, sizeof(a), "%010llu", (unsigned long long) m.m_capcode);
                sent.insert(key("flex", RadioPagingFlex::modeName(mode), a, false));
            }
        }

        for (uint64_t index = 0; (encoder.pending() > 0) && (index < 100); index++)
        {
            for (const auto& block : encoder.buildFrame(index).m_blocks)
            {
                std::vector<float> levels;
                for (uint8_t s : block.m_symbols) {
                    levels.push_back((block.m_levels == 4) ? levels4[s & 3] : ((s & 1) ? 1.0f : -1.0f));
                }
                mod.symbols(levels, block.m_baud, 4800.0);
            }
        }
    }
}

// A FLEX transmission alone: POCSAG and GSC are never synchronised (a sync codeword or a preamble found by chance in
// the frames is not a signal: the protocol shows none). Seen on the radio: POCSAG and GSC polarities shown in a FLEX
// transmission
void testFlexAloneNoOtherSync()
{
    Modulator mod;
    std::set<std::string> sent;

    // A weak FLEX transmission: noisy symbols, where the other protocols find a sync of a moment (from a noise of 0.7
    // a POCSAG or GSC sync codeword or preamble is found)
    mod.m_noise = std::normal_distribution<float>(0.0f, 1.0f);
    mod.noise(3000);
    addFlexFrames(mod, sent);
    mod.noise(3000);

    MessageQueue queue;
    RadioPagingDemodSink sink;
    RadioPagingDemodSettings settings;
    sink.setMessageQueueToChannel(&queue);
    sink.applyChannelSettings((int) sampleRate, 0, true);
    sink.applySettings(QStringList(), settings, true);
    bool otherSync = false;

    // The status between the blocks: a sync of a moment is seen too
    for (size_t i = 0; i < mod.m_samples.size(); i += 4096)
    {
        size_t end = std::min(mod.m_samples.size(), i + 4096);
        sink.feed(mod.m_samples.begin() + i, mod.m_samples.begin() + end);
        RadioPagingDemodSink::Status status = sink.getStatus();

        for (int pol = 0; pol < 2; pol++) {
            otherSync = otherSync || status.m_pol[0][pol].m_sync || status.m_pol[2][pol].m_sync;
        }
    }

    RadioPagingDemodSink::Status status = sink.getStatus();
    const bool seen = status.m_pol[0][0].m_seen || status.m_pol[0][1].m_seen || status.m_pol[2][0].m_seen || status.m_pol[2][1].m_seen;
    std::printf("      POCSAG or GSC synchronised: %s, a polarity shown: %s\n", otherSync ? "yes" : "no", seen ? "yes" : "no");
    check(!otherSync && !seen, "FLEX alone: POCSAG and GSC never synchronised, no polarity of theirs shown");

    Message *message;

    while ((message = queue.pop())) {
        delete message;
    }
}

// A POCSAG page of one batch after its preamble: no second sync codeword confirms it, the batch decoded does. The
// POCSAG row shows it (in sync while the batch is decoded, its polarity kept after)
void testPocsagOneBatchShown()
{
    Modulator mod;
    std::set<std::string> sent;

    mod.noise(3000);
    addPocsag(mod, sent, 1200, 1234567, "HI", false);

    MessageQueue queue;
    RadioPagingDemodSink sink;
    RadioPagingDemodSettings settings;
    sink.setMessageQueueToChannel(&queue);
    sink.applyChannelSettings((int) sampleRate, 0, true);
    sink.applySettings(QStringList(), settings, true);
    bool sync = false;

    for (size_t i = 0; i < mod.m_samples.size(); i += 1024)
    {
        size_t end = std::min(mod.m_samples.size(), i + 1024);
        sink.feed(mod.m_samples.begin() + i, mod.m_samples.begin() + end);
        sync = sync || sink.getStatus().m_pol[0][0].m_sync;
    }

    RadioPagingDemodSink::Status status = sink.getStatus();
    int received = 0;
    Message *message;

    while ((message = queue.pop()))
    {
        if (MainCore::MsgMessageRecord::match(*message) && (((MainCore::MsgMessageRecord *) message)->getRecord().m_protocol == "pocsag")) {
            received++;
        }
        delete message;
    }

    std::printf("      POCSAG one batch: received %d, in sync seen %s, polarity shown %s\n", received, sync ? "yes" : "no",
        status.m_pol[0][0].m_seen ? "yes" : "no");
    check((received == 1) && sync && status.m_pol[0][0].m_seen, "POCSAG one batch after its preamble: received, in sync while decoded, polarity shown");
}

void testNoFalseLocks()
{
    Modulator mod;
    std::set<std::string> sent;

    mod.noise(3000);
    addFlexFrames(mod, sent);

    // Right after the last frame: the other protocols are received
    addPocsag(mod, sent, 1200, 1234567, "POCSAG AFTER THE FLEX FRAMES", false);
    addGsc(mod, sent, { gsc("100123", 3, RadioPagingGsc::Alphanumeric, "GSC AFTER FLEX") });

    MessageQueue queue;
    RadioPagingDemodSink sink;
    RadioPagingDemodSettings settings;
    sink.setMessageQueueToChannel(&queue);
    sink.applyChannelSettings((int) sampleRate, 0, true);
    sink.applySettings(QStringList(), settings, true);

    for (size_t i = 0; i < mod.m_samples.size(); i += 4096)
    {
        size_t end = std::min(mod.m_samples.size(), i + 4096);
        sink.feed(mod.m_samples.begin() + i, mod.m_samples.begin() + end);
    }

    int flex = 0;
    int pocsag = 0;
    int gscReceived = 0;
    int unexpected = 0;
    Message *message;

    while ((message = queue.pop()))
    {
        if (MainCore::MsgMessageRecord::match(*message))
        {
            const MessageRecord& r = ((MainCore::MsgMessageRecord *) message)->getRecord();
            const bool expected = sent.count(key(r.m_protocol.toStdString(), r.m_mode.toStdString(), r.m_address.toStdString(),
                r.m_polarity == MessageRecord::PolarityInverted)) > 0;

            if (r.m_kind != MessageRecord::KindMessage) {
                // Events
            } else if (!expected) {
                unexpected++;
                std::printf("      unexpected: %s %s %s %s\n", qPrintable(r.m_protocol), qPrintable(r.m_mode), qPrintable(r.m_address),
                    qPrintable(r.m_msgType));
            } else if (r.m_protocol == "flex") {
                flex++;
            } else if (r.m_protocol == "pocsag") {
                pocsag++;
            } else {
                gscReceived++;
            }
        }
        delete message;
    }

    RadioPagingDemodSink::Status status = sink.getStatus();
    std::printf("      FLEX %d/96, POCSAG %d/1, GSC %d/1, locks dropped in FLEX frames: POCSAG %u, GSC %u\n", flex, pocsag, gscReceived,
        status.m_locksDropped[0], status.m_locksDropped[2]);
    check(unexpected == 0, "FLEX frames of any data: no POCSAG or GSC message decoded in them");
    check((flex == 96) && (pocsag == 1) && (gscReceived == 1), "FLEX received, then POCSAG and GSC right after the frames");
}

void testFrequencyOffset()
{
    const double offsets[] = {0.0, 1000.0, -1000.0, 2000.0, -2000.0, 3000.0, -3000.0};

    for (float bandwidth : {16000.0f, 20000.0f})
    {
        for (double offset : offsets)
        {
            std::vector<float> m;
            std::vector<int> r = receiveWithOffset(offset, bandwidth, m);
            std::printf("      %5.0f Hz bandwidth, %+5.0f Hz offset: POCSAG %d/2 FLEX %d/2 GSC %d/1, measured %+.0f %+.0f %+.0f Hz\n",
                bandwidth, offset, r[0], r[1], r[2], m[0], m[1], m[2]);
            if ((bandwidth == 20000.0f) && (std::fabs(offset) <= 2000.0))
            {
                check((r[0] == 2) && (r[1] == 2) && (r[2] == 1), "Pages are received with a "
                    + std::to_string((int) offset) + " Hz offset at a 20 kHz channel bandwidth");
                check((std::fabs(m[0] - offset) < 150.0) && (std::fabs(m[1] - offset) < 150.0) && (std::fabs(m[2] - offset) < 150.0),
                    "The status gives the " + std::to_string((int) offset) + " Hz offset for each protocol");
            }
        }
    }

    // DC correction off: the legs stay on the channel centre with the offset room, the
    // offset is still measured and reported
    dcCorrection = false;
    for (double offset : {0.0, 1500.0, -1500.0})
    {
        std::vector<float> m;
        std::vector<int> r = receiveWithOffset(offset, 20000.0f, m);
        std::printf("      no DC correction, %+5.0f Hz offset: POCSAG %d/2 FLEX %d/2 GSC %d/1, measured %+.0f %+.0f %+.0f Hz\n",
            offset, r[0], r[1], r[2], m[0], m[1], m[2]);
        check((r[0] == 2) && (r[1] == 2) && (r[2] == 1), "Without DC correction pages are received with a "
            + std::to_string((int) offset) + " Hz offset");
        check((std::fabs(m[0] - offset) < 150.0) && (std::fabs(m[1] - offset) < 150.0) && (std::fabs(m[2] - offset) < 150.0),
            "Without DC correction the status gives the " + std::to_string((int) offset) + " Hz offset");
    }
    dcCorrection = true;
}

// Sweep of carrier offset and noise ("radiopagingdemodsinktest sweep"): transmissions made by
// the FSK modulator, separated by noise only, received through the sink. For each case the
// pages received and the error of the reported carrier offset (df) when each page is output

struct SweepCase
{
    std::string m_name;
    int m_protocol;                     // 0 POCSAG, 1 FLEX, 2 GSC
    std::vector<std::pair<int, std::vector<float>>> m_blocks;
    double m_deviation;
    double m_shaping;
    std::set<std::string> m_sent;
};

std::vector<float> nrzLevels(const std::vector<uint8_t>& bits, bool oneIsPositive)
{
    std::vector<float> levels;
    for (uint8_t b : bits) {
        levels.push_back(((b != 0) == oneIsPositive) ? 1.0f : -1.0f);
    }
    return levels;
}

std::vector<SweepCase> sweepCases()
{
    std::vector<SweepCase> cases;

    for (int baud : {512, 1200, 2400})
    {
        SweepCase c;
        RadioPagingPocsag::TxMessage a, b;
        a.m_address = 1234567; a.m_function = 3; a.m_type = RadioPagingPocsag::Alphanumeric; a.m_text = "POCSAG SWEEP 0123456789";
        b.m_address = 7654; b.m_function = 0; b.m_type = RadioPagingPocsag::Numeric; b.m_text = "0123456789";
        c.m_name = "POCSAG " + std::to_string(baud);
        c.m_protocol = 0;
        c.m_blocks = {{baud, nrzLevels(RadioPagingPocsag::encode({a, b}).m_bits, false)}};
        c.m_deviation = 4500.0;
        c.m_shaping = baud;
        c.m_sent = {key("pocsag", std::to_string(baud), "1234567D", false), key("pocsag", std::to_string(baud), "0007654A", false)};
        cases.push_back(c);
    }

    const std::pair<RadioPagingGsc::Mode, const char *> gscModes[] = {
        {RadioPagingGsc::Single, "single"}, {RadioPagingGsc::Batch, "batch"}, {RadioPagingGsc::NonBatterySaver, "NBS"}};

    for (const auto& gm : gscModes)
    {
        SweepCase c;
        RadioPagingGsc::EncodeOptions options;
        options.m_mode = gm.first;
        std::vector<RadioPagingGsc::TxMessage> messages = {gsc("219789", 2, RadioPagingGsc::Numeric, "5551234")};
        if (gm.first != RadioPagingGsc::Single) {
            messages.push_back(gsc("219457", 4, RadioPagingGsc::Alphanumeric, "GSC SWEEP"));
        }
        c.m_name = std::string("GSC ") + gm.second;
        c.m_protocol = 2;
        c.m_blocks = {{600, nrzLevels(RadioPagingGsc::encode(messages, options).m_bits, true)}};
        c.m_deviation = 4500.0;
        c.m_shaping = 600.0;
        for (const auto& m : messages) {
            std::string address = (gm.first == RadioPagingGsc::NonBatterySaver) ? "N" + m.m_address.substr(1) : m.m_address;
            c.m_sent.insert(key("gsc", "600", address + "/" + std::to_string(m.m_addressNumber), false));
        }
        cases.push_back(c);
    }

    static const float levels4[4] = {-1.0f, -1.0f / 3.0f, 1.0f / 3.0f, 1.0f};
    const RadioPagingFlex::Mode modes[] = {RadioPagingFlex::Mode1600_2, RadioPagingFlex::Mode3200_2,
        RadioPagingFlex::Mode3200_4, RadioPagingFlex::Mode6400_4};

    for (RadioPagingFlex::Mode mode : modes)
    {
        SweepCase c;
        RadioPagingFlexEncoder encoder;
        encoder.setMode(mode);
        for (uint64_t capcode : {123456ULL, 234567ULL})
        {
            RadioPagingFlexEncoder::TxMessage m;
            m.m_capcode = capcode;
            m.m_type = RadioPagingFlex::Alphanumeric;
            m.m_text = "FLEX sweep page";
            encoder.enqueue(m);
            char a[16];
            std::snprintf(a, sizeof(a), "%010llu", (unsigned long long) capcode);
            c.m_sent.insert(key("flex", RadioPagingFlex::modeName(mode), a, false));
        }
        for (uint64_t index = 0; (encoder.pending() > 0) && (index < 50); index++)
        {
            RadioPagingFlexEncoder::Frame frame = encoder.buildFrame(index);
            for (const auto& block : frame.m_blocks)
            {
                std::vector<float> levels;
                for (uint8_t s : block.m_symbols) {
                    levels.push_back((block.m_levels == 4) ? levels4[s & 3] : ((s & 1) ? 1.0f : -1.0f));
                }
                c.m_blocks.push_back({block.m_baud, levels});
            }
        }
        c.m_name = std::string("FLEX ") + RadioPagingFlex::modeName(mode);
        c.m_protocol = 1;
        c.m_deviation = 4800.0;
        c.m_shaping = 3200.0;
        cases.push_back(c);
    }

    return cases;
}

struct SweepResult
{
    int m_received = 0;
    int m_sent = 0;
    double m_dfErrorSum = 0.0;          // |df - offset| of the pages received
    double m_dfErrorMax = 0.0;
};

// Noise only, the transmission (repeated), noise only. sigma: noise per component, the
// carrier amplitude being 1
SweepResult sweepRun(const SweepCase& c, double offset, float sigma, int repeats, unsigned seed)
{
    std::mt19937 rng(seed);
    std::normal_distribution<float> gauss(0.0f, sigma);
    SampleVector samples;
    const double a = 0.5 * SDR_RX_SCALED;
    double phase = 0.0;

    auto noiseOnly = [&](int n) {
        for (int i = 0; i < n; i++) {
            samples.push_back(Sample((FixReal) (a * gauss(rng)), (FixReal) (a * gauss(rng))));
        }
    };

    noiseOnly((int) sampleRate / 2);

    for (int r = 0; r < repeats; r++)
    {
        RadioPagingFskMod mod;
        mod.setChannelSampleRate((int) sampleRate);
        mod.setDeviation(c.m_deviation);
        mod.setShaping(c.m_shaping);
        for (const auto& b : c.m_blocks) {
            mod.push(b.first, b.second);
        }
        while (mod.busy())
        {
            phase += 2.0 * M_PI * offset / sampleRate;
            Complex s = mod.next() * Complex((float) std::cos(phase), (float) std::sin(phase));
            samples.push_back(Sample((FixReal) (a * (s.real() + gauss(rng))), (FixReal) (a * (s.imag() + gauss(rng)))));
        }
        noiseOnly((int) sampleRate / 2);
    }

    MessageQueue queue;
    RadioPagingDemodSink sink;
    RadioPagingDemodSettings settings;
    settings.m_pocsagDcCorrection = dcCorrection;
    settings.m_flexDcCorrection = dcCorrection;
    settings.m_gscDcCorrection = dcCorrection;
    sink.setMessageQueueToChannel(&queue);
    sink.applyChannelSettings((int) sampleRate, 0, true);
    sink.applySettings(QStringList(), settings, true);

    SweepResult result;
    result.m_sent = (int) c.m_sent.size() * repeats;
    quint32 lastCount = 0;
    const size_t chunk = 1024;

    for (size_t i = 0; i < samples.size(); i += chunk)
    {
        size_t end = std::min(samples.size(), i + chunk);
        sink.feed(samples.begin() + i, samples.begin() + end);

        RadioPagingDemodSink::Status st = sink.getStatus();
        Message *message;

        while ((message = queue.pop()))
        {
            if (MainCore::MsgMessageRecord::match(*message))
            {
                const MessageRecord& rec = ((MainCore::MsgMessageRecord *) message)->getRecord();
                // Each repeat is a transmission of its own: its pages are output tagged duplicate
                if (c.m_sent.count(key(rec.m_protocol.toStdString(), rec.m_mode.toStdString(), rec.m_address.toStdString(),
                    rec.m_polarity == MessageRecord::PolarityInverted)))
                {
                    result.m_received++;
                    double err = st.m_offsetValid[c.m_protocol] ? std::fabs(st.m_offset[c.m_protocol] - offset) : 99999.0;
                    result.m_dfErrorSum += err;
                    result.m_dfErrorMax = std::max(result.m_dfErrorMax, err);
                }
            }
            delete message;
        }
        lastCount = st.m_messages[c.m_protocol];
    }
    (void) lastCount;

    return result;
}

void sweep()
{
    const double offsets[] = {-3000.0, -1500.0, 0.0, 1500.0, 3000.0};
    const float sigmas[] = {0.1f, 0.3f, 0.45f, 0.6f};
    const int repeats = 3;
    std::vector<SweepCase> cases = sweepCases();

    std::printf("sweep: %d transmissions per case, noise sigma per component (carrier 1); C/N in 16 kHz:", repeats);
    for (float s : sigmas) {
        std::printf(" %.2f=%.1f dB", s, 10.0 * std::log10(1.0 / (2.0 * s * s * 16000.0 / sampleRate)));
    }
    std::printf("\n%-12s", "case");
    for (float s : sigmas) {
        std::printf(" | sigma %.2f: pages, df err mean/max Hz ", s);
    }
    std::printf("\n");

    for (const SweepCase& c : cases)
    {
        for (double offset : offsets)
        {
            std::printf("%-12s %+5.0f", c.m_name.c_str(), offset);
            for (float sigma : sigmas)
            {
                SweepResult r = sweepRun(c, offset, sigma, repeats, (unsigned) (offset + 10000.0 + sigma * 1000.0));
                std::printf(" | %2d/%-2d %5.0f %5.0f", r.m_received, r.m_sent,
                    r.m_received ? r.m_dfErrorSum / r.m_received : 0.0, r.m_dfErrorMax);
            }
            std::printf("\n");
            std::fflush(stdout);
        }
    }
}

// Sensitivity of a leg configuration ("radiopagingdemodsinktest sensitivity afc|carson|room2k|channel
// [deep]"): pages received out of those sent over noise levels around the threshold
void sensitivity(const std::string& config, bool deep)
{
    // afc (the default configuration): legs following the measured carrier. Otherwise fixed
    // legs: Carson, Carson + 2 kHz each side, or the channel
    dcCorrection = (config == "afc");

    if (config == "carson") {
        RadioPagingDemodSettings::m_legOffsetAllowance = 0.0;
    } else if (config == "channel") {
        RadioPagingDemodSettings::m_legOffsetAllowance = 1.0e6;     // Legs wider than the channel: not filtered
    } else {
        RadioPagingDemodSettings::m_legOffsetAllowance = 2000.0;
    }

    // Deep: the slow protocols, whose threshold lies below the normal range
    std::vector<double> offsets = deep ? std::vector<double>{0.0, 2000.0} : std::vector<double>{0.0, 2000.0, 3000.0};
    std::vector<float> sigmas = deep ? std::vector<float>{0.9f, 1.0f, 1.1f, 1.2f, 1.3f, 1.4f, 1.5f, 1.6f}
        : std::vector<float>{0.50f, 0.55f, 0.60f, 0.65f, 0.70f, 0.75f, 0.80f, 0.85f};
    const int repeats = 10;
    std::vector<std::string> selected = deep
        ? std::vector<std::string>{"POCSAG 512", "POCSAG 1200", "GSC single", "GSC batch", "GSC NBS", "FLEX 1600/2"}
        : std::vector<std::string>{"POCSAG 512", "POCSAG 1200", "POCSAG 2400", "GSC single", "GSC batch", "GSC NBS", "FLEX 1600/2", "FLEX 6400/4"};
    std::vector<SweepCase> all = sweepCases();

    std::printf("sensitivity %s (leg room %.0f Hz), %d transmissions per cell; percent of pages received at sigma (C/N in 16 kHz):\n",
        config.c_str(), RadioPagingDemodSettings::m_legOffsetAllowance, repeats);
    std::printf("%-12s %5s", "case", "off");
    for (float s : sigmas) {
        std::printf(" %4.2f(%4.1f)", s, 10.0 * std::log10(1.0 / (2.0 * s * s * 16000.0 / sampleRate)));
    }
    std::printf("\n");

    for (const std::string& name : selected)
    {
        for (const SweepCase& c : all)
        {
            if (c.m_name != name) {
                continue;
            }
            for (double offset : offsets)
            {
                std::printf("%-12s %+5.0f", c.m_name.c_str(), offset);
                for (float sigma : sigmas)
                {
                    SweepResult r = sweepRun(c, offset, sigma, repeats, (unsigned) (offset + 20000.0 + sigma * 1000.0));
                    std::printf(" %10.0f%%", 100.0 * r.m_received / std::max(1, r.m_sent));
                }
                std::printf("\n");
                std::fflush(stdout);
            }
        }
    }
}

// GSC voice page, a voice tone, then the turn-off word if wanted
std::vector<Sample> voicePage(double toneSeconds, bool turnOff)
{
    Modulator mod;
    RadioPagingGsc::EncodeOptions single;
    single.m_mode = RadioPagingGsc::Single;
    std::vector<uint8_t> bits = RadioPagingGsc::encode({ gsc("100123", 3, RadioPagingGsc::Voice, "") }, single).m_bits;
    std::vector<float> levels;

    mod.noise(3000);
    for (uint8_t b : bits) {
        levels.push_back(b ? 1.0f : -1.0f);
    }
    mod.symbols(levels, 600, 4500.0);

    // 1 kHz at 3 kHz deviation
    for (int i = 0; i < (int) (toneSeconds * sampleRate); i++) {
        mod.sample(3000.0 * std::sin(2.0 * M_PI * 1000.0 * i / sampleRate));
    }

    if (turnOff)
    {
        std::vector<uint8_t> off;
        std::vector<float> offLevels;
        RadioPagingGsc::appendActivationCode(off);
        for (uint8_t b : off) {
            offLevels.push_back(b ? 1.0f : -1.0f);
        }
        mod.symbols(offLevels, 600, 4500.0);
    }

    mod.noise((int) sampleRate / 2);
    return mod.m_samples;
}

struct Audio
{
    size_t m_samples = 0;
    int m_crossings = 0;    //!< Of the middle of the voice: 0.5 s to 2.5 s
    int m_peak = 0;
};

// Only GSC on, audio at 48 kHz; the audio FIFO is read as the audio device would
Audio receiveAudio(const std::vector<Sample>& samples, bool audio, int voiceTimeout, const QString& recordDirectory = QString(),
    std::vector<QString> *files = nullptr)
{
    RadioPagingDemodSink sink;
    RadioPagingDemodSettings settings;
    MessageQueue queue;
    settings.m_pocsagEnabled = false;
    settings.m_flexEnabled = false;
    settings.m_gscAudio = audio;
    settings.m_gscVoiceRecord = !recordDirectory.isEmpty();
    settings.m_gscVoiceDirectory = recordDirectory;
    settings.m_gscVoiceTimeout = voiceTimeout;
    sink.setMessageQueueToChannel(&queue);
    sink.applyChannelSettings((int) sampleRate, 0, true);
    sink.applySettings(QStringList(), settings, true);
    sink.applyAudioSampleRate(48000);

    Audio out;
    AudioVector buffer(48000);
    qint16 last = 0;
    const size_t chunk = 1024;

    for (size_t i = 0; i < samples.size(); i += chunk)
    {
        size_t end = std::min(samples.size(), i + chunk);
        sink.feed(samples.begin() + i, samples.begin() + end);
        uint32_t n = sink.getAudioFifo()->read((quint8*) &buffer[0], buffer.size());

        for (uint32_t k = 0; k < n; k++)
        {
            qint16 v = buffer[k].l;
            size_t at = out.m_samples + k;

            // Away from the alert start and from the turn-off word at the end
            if ((at > 24000) && (at <= 120000))
            {
                out.m_crossings += ((v >= 0) != (last >= 0)) ? 1 : 0;
                out.m_peak = std::max(out.m_peak, (int) std::abs(v));
            }
            last = v;
        }
        out.m_samples += n;
    }

    while (queue.size() > 0)
    {
        Message *message = queue.pop();

        if (files && MainCore::MsgMessageRecord::match(*message))
        {
            const MessageRecord& r = ((MainCore::MsgMessageRecord *) message)->getRecord();

            if (r.m_protocolData.contains("voice_file")) {
                files->push_back(r.m_protocolData["voice_file"].toString());
            }
        }

        delete message;
    }

    return out;
}

// The voice recorded to a WAV file in a directory: its path is in the record of the end of the voice
void testVoiceRecording()
{
    QTemporaryDir directory;
    check(directory.isValid(), "Voice recording: temporary directory");

    std::vector<Sample> page = voicePage(5.0, true);
    std::vector<QString> files;

    // Audio off: the recording does not depend on it
    receiveAudio(page, false, 50, directory.path(), &files);
    check(files.size() == 1, "Voice recording: the record of the end of the voice has the file");

    RadioPagingWav::Audio wav;
    std::string error;
    bool loaded = !files.empty() && RadioPagingWav::load(files[0].toStdString(), wav, error, 50.0, false);
    std::printf("      voice file %s: %s, %zu samples\n", files.empty() ? "" : files[0].toStdString().c_str(),
        loaded ? "read" : error.c_str(), wav.m_samples.size());
    check(loaded && files[0].startsWith(directory.path()), "Voice recording: a WAV file in the directory");

    // 2.8 s of voice at 38400 Hz after the resampling; 1 kHz at 3 kHz of the 4.5 kHz deviation
    double seconds = wav.m_samples.size() / (double) RadioPagingWav::m_sampleRate;
    int crossings = 0;
    float peak = 0.0f;

    for (size_t i = 1; i < wav.m_samples.size(); i++)
    {
        if ((i > 19200) && (i <= 96000)) {
            crossings += ((wav.m_samples[i] >= 0.0f) != (wav.m_samples[i - 1] >= 0.0f)) ? 1 : 0;
        }

        // Away from the start and from the turn-off word at the end
        if ((i > 19200) && (i <= 96000)) {
            peak = std::max(peak, std::fabs(wav.m_samples[i]));
        }
    }

    std::printf("      voice file peak %.3f\n", peak);

    double frequency = crossings / (2.0 * 2.0);
    check((seconds > 2.8) && (seconds < 3.2), "Voice recording: the length of the voice message");
    check((frequency > 950.0) && (frequency < 1050.0), "Voice recording: the voice tone at its frequency");
    check((peak > 0.85 * 3000.0 / 4500.0) && (peak < 1.15 * 3000.0 / 4500.0), "Voice recording: full scale is the deviation, whatever the volume");

    // Recording off: no file, no path
    files.clear();
    receiveAudio(page, true, 50, QString(), &files);
    check(files.empty(), "Voice recording off: no file in the record");
}

void testVoiceAudio()
{
    // 5 s after the activation code: 2.2 s of alert time are not played, then 2.8 s of
    // voice and the turn-off word
    std::vector<Sample> page = voicePage(5.0, true);
    Audio on = receiveAudio(page, true, 50);
    double seconds = on.m_samples / 48000.0;
    double frequency = on.m_crossings / (2.0 * 2.0);
    std::printf("      voice audio %.2f s, %.0f Hz, peak %d\n", seconds, frequency, on.m_peak);
    check((seconds > 2.8) && (seconds < 3.2), "Voice audio: opens after the alert time, closes on the turn-off word");
    check((frequency > 950.0) && (frequency < 1050.0), "Voice audio: the voice tone at its frequency");
    const double level = 32767 * 3000.0 / 4500.0;    // 3 kHz of the 4.5 kHz GSC deviation
    check((on.m_peak > 0.85 * level) && (on.m_peak < 1.15 * level), "Voice audio: level of the deviation at volume 1");

    check(receiveAudio(page, false, 50).m_samples == 0, "Voice audio off: nothing is output");

    // The voice timeout counts from the activation code
    Audio timeout = receiveAudio(voicePage(6.0, false), true, 4);
    std::printf("      voice audio with a 4 s voice timeout %.2f s\n", timeout.m_samples / 48000.0);
    check((timeout.m_samples > 1.7 * 48000) && (timeout.m_samples < 1.9 * 48000), "Voice audio: closes at the longest voice message");
}

// Voice files: the ones that may be deleted are older than the days kept and not the newest ones
void testVoiceStore()
{
    QTemporaryDir directory;
    QDateTime now = QDateTime::currentDateTime();
    QStringList names;

    for (int i = 0; i < 8; i++)
    {
        QString name = QString("%1/146.5_rx_%2.wav").arg(directory.path()).arg(i);
        QFile file(name);
        file.open(QIODevice::WriteOnly);
        file.write(QByteArray(100, 'x'));
        file.close();
        file.open(QIODevice::ReadWrite);        // The time is set on a file that is not written to afterwards
        file.setFileTime(now.addSecs(-(qint64) i * 86400), QFileDevice::FileModificationTime);
        file.close();
        names.append(name);
    }

    QFile other(directory.path() + "/notes.wav");
    other.open(QIODevice::WriteOnly);
    other.close();

    RadioPagingVoiceStore::Stats stats = RadioPagingVoiceStore::stats(directory.path(), 3, 2, now);
    check((stats.m_files == 8) && (stats.m_bytes == 800) && (stats.m_expired == 4) && (stats.m_expiredBytes == 400)
        && (stats.m_oldest < stats.m_newest) && (stats.m_freeBytes > 0), "Voice store: statistics, 4 of 8 files older than 3 days");
    check(RadioPagingVoiceStore::stats(directory.path(), 3, 6, now).m_expired == 2, "Voice store: the newest 6 files are kept whatever their age");
    check(RadioPagingVoiceStore::stats(directory.path(), 0, 0, now).m_expired == 0, "Voice store: 0 days, no file is deleted for its age");

    int deleted = RadioPagingVoiceStore::cleanup(directory.path(), 3, 2, 3, QString(), now);
    check((deleted == 3) && !QFile::exists(names[7]) && !QFile::exists(names[6]) && !QFile::exists(names[5]) && QFile::exists(names[4]),
        "Voice store: a cleanup of at most 3 files deletes the oldest ones");
    check((RadioPagingVoiceStore::cleanup(directory.path(), 3, 2, 5, names[4], now) == 0) && QFile::exists(names[4]),
        "Voice store: the file just written is not deleted");
    check((RadioPagingVoiceStore::cleanup(directory.path(), 3, 2, -1, QString(), now) == 1) && !QFile::exists(names[4])
        && QFile::exists(names[3]) && QFile::exists(directory.path() + "/notes.wav"), "Voice store: all at once, other files are left");
    check((RadioPagingVoiceStore::fileAlias("146.5/MHz x") == "146.5_MHz_x") && (RadioPagingVoiceStore::fileAlias("") == "rx"),
        "Voice store: the alias in a file name");

    // In the background (the end of a recording on the DSP thread): the old files go in a thread of the pool
    QFile old(directory.path() + "/146.5_rx_old.wav");
    old.open(QIODevice::WriteOnly);
    old.close();
    old.open(QIODevice::ReadWrite);
    old.setFileTime(now.addSecs(-10 * 86400), QFileDevice::FileModificationTime);
    old.close();
    RadioPagingVoiceStore::cleanupInBackground(directory.path(), 3, 2, 5, QString());
    QThreadPool::globalInstance()->waitForDone();
    check(!QFile::exists(old.fileName()) && QFile::exists(names[3]), "Voice store: a cleanup in the background deletes the old files");
}

} // namespace

// The input stops right at the end of a transmission (no samples after it), or in the data part of a FLEX frame: the
// sink releases what it was receiving when told so (resetProtocol(-1)), and only then
void testInputStopped()
{
    struct Case
    {
        std::string m_name;
        std::function<void(Modulator&, std::set<std::string>&)> m_add;
        double m_keep;  // Share of the transmission received
    };
    const std::vector<Case> cases = {
        {"POCSAG 512", [](Modulator& m, std::set<std::string>& s) { addPocsag(m, s, 512, 7654, "POCSAG 512", false); }, 1.0},
        {"POCSAG 2400", [](Modulator& m, std::set<std::string>& s) { addPocsag(m, s, 2400, 2000001, "POCSAG 2400", false); }, 1.0},
        {"FLEX 1600/2", [](Modulator& m, std::set<std::string>& s) { addFlex(m, s, RadioPagingFlex::Mode1600_2, { 123456 }); }, 1.0},
        {"FLEX 6400/4", [](Modulator& m, std::set<std::string>& s) { addFlex(m, s, RadioPagingFlex::Mode6400_4, { 345678 }); }, 1.0},
        {"FLEX 6400/4 frame cut", [](Modulator& m, std::set<std::string>& s) { addFlex(m, s, RadioPagingFlex::Mode6400_4, { 345678 }); }, 0.6},
        {"GSC", [](Modulator& m, std::set<std::string>& s) { addGsc(m, s, { gsc("100123", 3, RadioPagingGsc::Alphanumeric, "GSC SINGLE") }); }, 1.0},
    };

    for (const Case& c : cases)
    {
        Modulator mod;
        std::set<std::string> sent;
        mod.noise(3000);
        const size_t start = mod.m_samples.size();
        c.m_add(mod, sent);
        // Without the noise the helpers add after the transmission
        size_t end = mod.m_samples.size() - 4000;
        end = start + (size_t) ((double) (end - start) * c.m_keep);
        mod.m_samples.resize(end);

        MessageQueue queue;
        RadioPagingDemodSink sink;
        sink.setMessageQueueToChannel(&queue);
        sink.applyChannelSettings((int) sampleRate, 0, true);
        sink.feed(mod.m_samples.begin(), mod.m_samples.end());

        auto received = [&]() {
            std::set<std::string> got;
            Message *message;
            while ((message = queue.pop()))
            {
                if (MainCore::MsgMessageRecord::match(*message))
                {
                    const MessageRecord& r = ((MainCore::MsgMessageRecord *) message)->getRecord();
                    got.insert(key(r.m_protocol.toStdString(), r.m_mode.toStdString(), r.m_address.toStdString(),
                        r.m_polarity == MessageRecord::PolarityInverted));
                }
                delete message;
            }
            return got;
        };

        std::set<std::string> before = received();
        sink.resetProtocol(-1);
        std::set<std::string> after = received();
        std::printf("      %s: %zu before the input stopped, %zu released after\n", c.m_name.c_str(), before.size(), after.size());

        before.insert(after.begin(), after.end());
        check(before == sent, c.m_name + ": the input stops at the end of the transmission, its pages are received");
    }
}

int main(int argc, char **argv)
{
    if ((argc > 1) && (std::string(argv[1]) == "sweep"))
    {
        sweep();
        return 0;
    }
    if ((argc > 1) && (std::string(argv[1]) == "sensitivity"))
    {
        sensitivity(argc > 2 ? std::string(argv[2]) : std::string(), (argc > 3) && (std::string(argv[3]) == "deep"));
        return 0;
    }

    // Nothing of the tests goes to the data directory of the user
    QStandardPaths::setTestModeEnabled(true);

    Modulator mod;
    std::set<std::string> sent;

    mod.noise(3000);
    addPocsag(mod, sent, 1200, 1234567, "POCSAG 1200 THROUGH THE CHANNEL", false);
    addPocsag(mod, sent, 512, 7654, "POCSAG 512", false);
    addPocsag(mod, sent, 2400, 2000001, "POCSAG 2400", false);
    addPocsag(mod, sent, 1200, 1000002, "POCSAG INVERTED", true);
    addFlex(mod, sent, RadioPagingFlex::Mode1600_2, { 123456, 234567 });
    addFlex(mod, sent, RadioPagingFlex::Mode3200_4, { 345678 });
    addGsc(mod, sent, { gsc("219789", 2, RadioPagingGsc::Numeric, "5551234"), gsc("219456", 1, RadioPagingGsc::Tone, ""),
        gsc("219457", 4, RadioPagingGsc::Alphanumeric, "GSC BATCH") });
    addGsc(mod, sent, { gsc("100123", 3, RadioPagingGsc::Alphanumeric, "GSC SINGLE") });

    MessageQueue queue;
    RadioPagingDemodSink sink;
    sink.setMessageQueueToChannel(&queue);
    sink.applyChannelSettings((int) sampleRate, 0, true);

    const size_t chunk = 4096;
    for (size_t i = 0; i < mod.m_samples.size(); i += chunk)
    {
        size_t end = std::min(mod.m_samples.size(), i + chunk);
        sink.feed(mod.m_samples.begin() + i, mod.m_samples.begin() + end);
    }

    std::set<std::string> received;
    int records = 0;
    bool fieldsOk = true;
    Message *message;

    while ((message = queue.pop()))
    {
        if (MainCore::MsgMessageRecord::match(*message))
        {
            const MessageRecord& r = ((MainCore::MsgMessageRecord *) message)->getRecord();
            records++;
            received.insert(key(r.m_protocol.toStdString(), r.m_mode.toStdString(), r.m_address.toStdString(),
                r.m_polarity == MessageRecord::PolarityInverted));
            fieldsOk = fieldsOk && !r.m_id.isEmpty() && (r.m_direction == MessageRecord::DirectionRX)
                && (r.m_kind == MessageRecord::KindMessage) && (r.m_protocolData.value("interpretedBy").toString() == "decoder")
                && (r.m_speed > 0);
            std::printf("      %s %s %s %s %s \"%s\"\n", r.m_protocol.toStdString().c_str(), r.m_mode.toStdString().c_str(),
                r.m_address.toStdString().c_str(), r.m_polarity == MessageRecord::PolarityInverted ? "I" : "N",
                r.m_msgType.toStdString().c_str(), r.m_text.toStdString().c_str());
        }
        delete message;
    }

    for (const auto& s : sent)
    {
        if (!received.count(s)) {
            std::printf("      missing: %s\n", s.c_str());
        }
    }
    for (const auto& r : received)
    {
        if (!sent.count(r)) {
            std::printf("      unexpected: %s\n", r.c_str());
        }
    }

    check(received == sent, "Every page sent is received once, with its protocol, mode, address and polarity, nothing else ("
        + std::to_string(sent.size()) + " sent, " + std::to_string(records) + " records)");
    check((size_t) records == sent.size(), "No duplicate records");
    check(fieldsOk, "Records carry an id, direction rx, kind message, speed and interpretedBy decoder");

    RadioPagingDemodSink::Status status = sink.getStatus();
    check((status.m_messages[0] == 4) && (status.m_messages[1] == 3) && (status.m_messages[2] == 4), "Status counts the records per protocol");

    testInputStopped();
    testVoiceAudio();
    testVoiceRecording();
    testVoiceStore();
    testNoFalseLocks();
    testFlexAloneNoOtherSync();
    testPocsagOneBatchShown();
    testFrequencyOffset();

    std::printf("%d passed, %d failed\n", passes, failures);
    return failures ? 1 : 0;
}
