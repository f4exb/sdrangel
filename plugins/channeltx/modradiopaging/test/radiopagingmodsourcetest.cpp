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

// Self test of the radio paging modulator source: messages are produced by the source at a
// channel sample rate and offset, with noise added, received through the radio paging
// demodulator sink, and the records are compared with what was sent. The source states
// (ramps, queue, abort, network idle fill, change of mode) are checked on the samples.

#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "dsp/dsptypes.h"
#include "util/messagequeue.h"
#include "maincore.h"

#include "radiopagingpocsag.h"
#include "radiopagingflex.h"
#include "radiopagingtxstream.h"
#include "radiopagingmodsource.h"
#include "radiopagingdemodsink.h"

namespace {

int passes = 0;
int failures = 0;

// Clock of the tests: 2026-01-01 00:00:00 UTC, a cycle boundary (frame 0) of the FLEX time grid
const qint64 flexEpochMs = 1767225600000LL;

void check(bool ok, const std::string& what)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (ok) {
        passes++;
    } else {
        failures++;
    }
}

std::vector<RadioPagingPocsag::TxMessage> pocsagMessages()
{
    RadioPagingPocsag::TxMessage a, b;
    a.m_address = 1234567;
    a.m_function = 3;
    a.m_type = RadioPagingPocsag::Alphanumeric;
    a.m_text = "POCSAG FROM THE MODULATOR";
    b.m_address = 7654;
    b.m_function = 0;
    b.m_type = RadioPagingPocsag::Numeric;
    b.m_text = "0123456789";
    return { a, b };
}

RadioPagingModSource::Request pocsagRequest(quint64 id, int index, int baud, bool inverted)
{
    RadioPagingModSource::Request request;
    request.m_id = id;
    request.m_protocol = RadioPagingModSettings::Pocsag;
    request.m_pocsag = pocsagMessages()[index];
    request.m_pocsagBaud = baud;
    request.m_pocsagInverted = inverted;
    return request;
}

// In the mode of the network (the settings)
RadioPagingModSource::Request flexRequest(quint64 id, uint64_t capcode, const std::string& text, bool inverted)
{
    RadioPagingModSource::Request request;
    request.m_id = id;
    request.m_protocol = RadioPagingModSettings::Flex;
    request.m_flex.m_capcode = capcode;
    request.m_flex.m_type = RadioPagingFlex::Alphanumeric;
    request.m_flex.m_text = text;
    request.m_flexInverted = inverted;
    return request;
}

struct Received
{
    std::set<std::string> m_keys;       //!< "protocol mode address polarity text"
    int m_records = 0;
    int m_voiceStarts = 0;              //!< Voice page records (partial)
    std::vector<std::string> m_voiceEnds;   //!< The end of each voice message: the reason
    double m_voiceSeconds = 0.0;        //!< Length of the last voice message
    int m_systemEvents = 0;             //!< FLEX System Event Notification records
    unsigned m_eventFlags = 0;          //!< Their event flags, or-ed
    std::map<std::string, QVariantMap> m_flexTimes;     //!< The FLEX network time of the records, by their text
};

// The source output, with noise, into the demodulator sink at the same rate and offset
struct Loop
{
    RadioPagingModSource m_source;
    RadioPagingModSettings m_settings;
    MessageQueue m_queue;
    RadioPagingDemodSink m_sink;
    std::mt19937 m_rng{7};
    std::normal_distribution<float> m_noise{0.0f, 0.02f};
    int m_sampleRate;
    Received m_received;

    Loop(int sampleRate, int offset, RadioPagingModSettings::Protocol protocol = RadioPagingModSettings::Pocsag,
        RadioPagingModSettings::Mode mode = RadioPagingModSettings::OneShot) :
        m_sampleRate(sampleRate)
    {
        m_settings.m_protocol = protocol;
        m_settings.m_mode = mode;
        m_settings.m_deviation = RadioPagingModSettings::defaultDeviation(protocol);
        m_source.setClock([]() { return flexEpochMs; });
        m_source.applySettings(QStringList(), m_settings, true);
        m_source.applyChannelSettings(sampleRate, offset, true);
        RadioPagingDemodSettings rxSettings;
        rxSettings.m_gscVoiceRecord = false;    // The voice recording has a test of its own
        m_sink.applySettings(QStringList(), rxSettings, true);
        m_sink.setMessageQueueToChannel(&m_queue);
        m_sink.applyChannelSettings(sampleRate, offset, true);
    }

    void apply(const QStringList& keys)
    {
        m_source.applySettings(keys, m_settings);
    }

    // Samples pulled from the source, noise added, fed to the sink
    void run(int nbSamples, std::vector<float> *magnitudes = nullptr)
    {
        SampleVector block(1024);
        SampleVector rx(1024);

        while (nbSamples > 0)
        {
            int n = std::min(nbSamples, (int) block.size());
            m_source.pull(block.begin(), n);

            for (int i = 0; i < n; i++)
            {
                float re = block[i].m_real / SDR_TX_SCALEF;
                float im = block[i].m_imag / SDR_TX_SCALEF;

                if (magnitudes) {
                    magnitudes->push_back(std::sqrt(re * re + im * im));
                }

                rx[i] = Sample((FixReal) (0.5 * SDR_RX_SCALED * (re + m_noise(m_rng))),
                    (FixReal) (0.5 * SDR_RX_SCALED * (im + m_noise(m_rng))));
            }

            m_sink.feed(rx.begin(), rx.begin() + n);
            nbSamples -= n;
        }
    }

    void runSeconds(double seconds, std::vector<float> *magnitudes = nullptr)
    {
        run((int) (seconds * m_sampleRate), magnitudes);
    }

    // Until the source is idle with nothing queued, then a second of carrier off
    void runToEnd(std::vector<float> *magnitudes = nullptr)
    {
        for (int i = 0; i < 600; i++)
        {
            RadioPagingModSource::Status status = m_source.getStatus();

            if ((status.m_state == RadioPagingModSource::Idle) && (status.m_queued == 0)) {
                break;
            }
            run(m_sampleRate / 10, magnitudes);
        }
        run(m_sampleRate, magnitudes);
    }

    // Records received so far, added to those taken before
    const Received& take()
    {
        Message *message;

        while ((message = m_queue.pop()))
        {
            if (MainCore::MsgMessageRecord::match(*message))
            {
                const MessageRecord& rec = ((MainCore::MsgMessageRecord *) message)->getRecord();

                if (rec.m_msgType == "voice")
                {
                    if (rec.m_protocolData.contains("voice_end"))
                    {
                        m_received.m_voiceEnds.push_back(rec.m_protocolData["voice_end"].toString().toStdString());
                        m_received.m_voiceSeconds = rec.m_protocolData["voice_seconds"].toDouble();
                    }
                    else {
                        m_received.m_voiceStarts++;
                    }
                }

                if (rec.m_protocolData.contains("event_flags"))
                {
                    m_received.m_systemEvents++;
                    m_received.m_eventFlags |= rec.m_protocolData["event_flags"].toUInt();
                }

                if (rec.m_protocolData.contains("flextime")) {
                    m_received.m_flexTimes[rec.m_text.toStdString()] = rec.m_protocolData["flextime"].toMap();
                }

                m_received.m_records++;
                m_received.m_keys.insert((rec.m_protocol + " " + rec.m_mode + " " + rec.m_address + " "
                    + (rec.m_polarity == MessageRecord::PolarityInverted ? "inverted " : "normal ") + rec.m_text).toStdString());
            }
            delete message;
        }

        return m_received;
    }
};

std::set<std::string> expected(int baud, bool inverted, int index = -1)
{
    std::string mode = "pocsag " + std::to_string(baud);
    std::string polarity = inverted ? "inverted " : "normal ";
    std::set<std::string> keys;

    if (index != 1) {
        keys.insert(mode + " 1234567D " + polarity + "POCSAG FROM THE MODULATOR");
    }
    if (index != 0) {
        keys.insert(mode + " 0007654A " + polarity + "0123456789");
    }

    return keys;
}

std::string flexKey(RadioPagingFlex::Mode mode, uint64_t capcode, bool inverted, const std::string& text)
{
    char address[16];
    std::snprintf(address, sizeof(address), "%010llu", (unsigned long long) capcode);   // As the receiver shows it
    return std::string("flex ") + RadioPagingFlex::modeName(mode) + " " + address
        + (inverted ? " inverted " : " normal ") + text;
}

bool contains(const Received& received, const std::set<std::string>& keys)
{
    for (const std::string& key : keys)
    {
        if (!received.m_keys.count(key))
        {
            std::printf("      missing: %s\n", key.c_str());
            for (const std::string& got : received.m_keys) {
                std::printf("      received: %s\n", got.c_str());
            }
            return false;
        }
    }

    return true;
}

// First and one past the last sample with carrier
void carrierSpan(const std::vector<float>& magnitudes, size_t& first, size_t& last)
{
    first = 0;
    while ((first < magnitudes.size()) && (magnitudes[first] == 0.0f)) {
        first++;
    }
    last = magnitudes.size();
    while ((last > 0) && (magnitudes[last - 1] == 0.0f)) {
        last--;
    }
}

// ---------------------------------------------------------------------------
// POCSAG one-shot
// ---------------------------------------------------------------------------

void testRoundTrip(int sampleRate, int offset)
{
    for (int baud : {512, 1200, 2400})
    {
        for (bool inverted : {false, true})
        {
            Loop loop(sampleRate, offset);
            loop.m_source.addRequest(pocsagRequest(1, 0, baud, inverted));
            loop.m_source.addRequest(pocsagRequest(2, 1, baud, inverted));
            loop.run(sampleRate / 2);                   // Some carrier off first
            loop.runToEnd();
            const Received& received = loop.take();
            RadioPagingModSource::Status status = loop.m_source.getStatus();

            char what[160];
            std::snprintf(what, sizeof(what), "POCSAG %d %s at %d Hz, offset %d Hz: both pages received, sent 2",
                baud, inverted ? "inverted" : "normal", sampleRate, offset);
            check(contains(received, expected(baud, inverted)) && (status.m_sent == 2)
                && (status.m_lastSentId == 2) && (status.m_currentId == 0) && (status.m_inFlight == 0), what);
        }
    }
}

void testRamps()
{
    const int sampleRate = 48000;
    Loop loop(sampleRate, 0);
    std::vector<float> magnitudes;

    loop.run(1000, &magnitudes);
    loop.m_source.addRequest(pocsagRequest(5, 0, 1200, false));
    loop.runToEnd(&magnitudes);

    // Off, then the ramp up over 5 ms (240 samples) from -60 dB, full power, the ramp down
    // and off
    size_t first, last;
    carrierSpan(magnitudes, first, last);

    bool off = (first >= 1000) && (last > first);
    bool rampUp = off && (magnitudes[first] < 0.01f) && (magnitudes[first + 120] > 0.02f)
        && (magnitudes[first + 120] < 0.1f) && (std::fabs(magnitudes[first + 300] - 1.0f) < 0.01f);
    bool rampDown = off && (last > 300) && (magnitudes[last - 1] < 0.01f)
        && (std::fabs(magnitudes[last - 300] - 1.0f) < 0.01f);
    std::printf("      carrier on %zu to %zu, %.4f %.4f %.4f ... %.4f %.4f\n", first, last,
        off ? magnitudes[first] : 0.0f, off ? magnitudes[first + 120] : 0.0f, off ? magnitudes[first + 300] : 0.0f,
        off ? magnitudes[last - 300] : 0.0f, off ? magnitudes[last - 1] : 0.0f);
    check(rampUp, "Ramp up: from -60 dB to full power in 5 ms");
    check(rampDown, "Ramp down: from full power to -60 dB in 5 ms, then no carrier");

    // Carrier on for the bits of the transmission (preamble and batches) and the ramps
    double seconds = (last - first) / (double) sampleRate;
    double bitsSeconds = RadioPagingPocsag::encode({ pocsagMessages()[0] }).m_bits.size() / 1200.0;
    std::printf("      carrier %.3f s, bits %.3f s\n", seconds, bitsSeconds);
    check((seconds > bitsSeconds) && (seconds < bitsSeconds + 0.02), "Carrier on for the bits and the ramps");
}

void testQueueAndAbort()
{
    const int sampleRate = 48000;

    // Two messages queued: two transmissions, one after the other
    {
        Loop loop(sampleRate, 0);
        loop.m_source.addRequest(pocsagRequest(10, 0, 1200, false));
        loop.m_source.addRequest(pocsagRequest(11, 1, 512, true));
        loop.run(sampleRate / 10);
        RadioPagingModSource::Status during = loop.m_source.getStatus();
        loop.runToEnd();
        const Received& received = loop.take();
        RadioPagingModSource::Status status = loop.m_source.getStatus();
        check((during.m_currentId == 10) && (during.m_queued == 1) && (during.m_inFlight == 1),
            "Queue: the first message is sent, the second waits");
        check(contains(received, expected(1200, false, 0)) && contains(received, expected(512, true, 1))
            && (status.m_sent == 2) && (status.m_lastSentId == 11), "Queue: both messages sent in order");
    }

    // Abort during the first transmission: it ends (ramp down), the second is dropped
    {
        Loop loop(sampleRate, 0);
        loop.m_source.addRequest(pocsagRequest(20, 0, 512, false));
        loop.m_source.addRequest(pocsagRequest(21, 1, 512, false));
        loop.run(sampleRate / 4);
        loop.m_source.abort();
        loop.run(sampleRate / 100);
        RadioPagingModSource::Status status = loop.m_source.getStatus();
        check((status.m_state == RadioPagingModSource::Idle) && (status.m_queued == 0) && (status.m_sent == 0)
            && (status.m_currentId == 0) && (status.m_dropped == 2), "Abort: carrier off within the ramp, both dropped, nothing counted as sent");
    }

    // Clear: waiting messages dropped, the one being sent goes on
    {
        Loop loop(sampleRate, 0);
        loop.m_source.addRequest(pocsagRequest(30, 0, 2400, false));
        loop.m_source.addRequest(pocsagRequest(31, 1, 2400, false));
        loop.run(sampleRate / 10);
        loop.m_source.clear();
        loop.runToEnd();
        RadioPagingModSource::Status status = loop.m_source.getStatus();
        check((status.m_sent == 1) && (status.m_lastSentId == 30) && (status.m_dropped == 1),
            "Clear: the message being sent completes, the waiting one is dropped");
    }
}

// Abort in each state: ramping up (cut, dropped), ramping down (completes, sent), idle
// (nothing changes), and a message queued after an abort is sent
void testAbortStates()
{
    const int sampleRate = 48000;

    {
        Loop loop(sampleRate, 0);
        loop.m_source.addRequest(pocsagRequest(40, 0, 1200, false));
        loop.run(10);       // In the 5 ms ramp up
        RadioPagingModSource::Status during = loop.m_source.getStatus();
        loop.m_source.abort();
        loop.run(sampleRate / 10);
        RadioPagingModSource::Status status = loop.m_source.getStatus();
        check((during.m_state == RadioPagingModSource::RampUp) && (status.m_state == RadioPagingModSource::Idle)
            && (status.m_sent == 0) && (status.m_dropped == 1), "Abort while ramping up: cut, counted as dropped");
    }

    {
        Loop loop(sampleRate, 0);
        loop.m_source.addRequest(pocsagRequest(41, 0, 2400, false));

        // Until the ramp down has started
        RadioPagingModSource::Status status;
        for (int i = 0; i < 20000; i++)
        {
            loop.run(10);
            status = loop.m_source.getStatus();
            if (status.m_state == RadioPagingModSource::RampDown) {
                break;
            }
        }

        bool rampingDown = status.m_state == RadioPagingModSource::RampDown;
        loop.m_source.abort();
        loop.runToEnd();
        status = loop.m_source.getStatus();
        check(rampingDown && (status.m_sent == 1) && (status.m_lastSentId == 41) && (status.m_dropped == 0),
            "Abort while ramping down: the message completes, counted as sent");
    }

    {
        Loop loop(sampleRate, 0);
        loop.m_source.abort();
        RadioPagingModSource::Status idle = loop.m_source.getStatus();
        loop.m_source.addRequest(pocsagRequest(42, 0, 1200, true));
        loop.runToEnd();
        const Received& received = loop.take();
        RadioPagingModSource::Status status = loop.m_source.getStatus();
        check((idle.m_state == RadioPagingModSource::Idle) && (idle.m_dropped == 0)
            && contains(received, expected(1200, true, 0)) && (status.m_sent == 1),
            "Abort while idle changes nothing; a message after it is sent");
    }
}

// No ramps (0 dB range, then 0 ms ramps): full power from the first sample to the last
void testZeroRamps()
{
    const int sampleRate = 48000;

    for (int variant = 0; variant < 2; variant++)
    {
        Loop loop(sampleRate, 0);
        QStringList keys;

        if (variant == 0)
        {
            loop.m_settings.m_rampRange = 0;
            keys << "rampRange";
        }
        else
        {
            loop.m_settings.m_rampUpMs = 0.0f;
            loop.m_settings.m_rampDownMs = 0.0f;
            keys << "rampUpMs" << "rampDownMs";
        }

        loop.apply(keys);
        std::vector<float> magnitudes;
        loop.m_source.addRequest(pocsagRequest(50, 0, 1200, false));
        loop.runToEnd(&magnitudes);
        const Received& received = loop.take();

        size_t first, last;
        carrierSpan(magnitudes, first, last);
        bool full = (last > first) && (std::fabs(magnitudes[first] - 1.0f) < 0.01f) && (std::fabs(magnitudes[last - 1] - 1.0f) < 0.01f);
        check(full && contains(received, expected(1200, false, 0)) && (loop.m_source.getStatus().m_sent == 1),
            variant == 0 ? "Ramp range 0 dB: full power at once, page received" : "Ramps of 0 ms: full power at once, page received");
    }
}

// The deviation changed during a transmission: the FM sensitivity changes, the symbols and
// filters are kept, the pages are received
void testDeviationChange()
{
    const int sampleRate = 48000;
    Loop loop(sampleRate, 0);
    loop.m_source.addRequest(pocsagRequest(60, 0, 512, false));

    // Peak instantaneous frequency over the preamble (1.125 s at 512 baud): 0.1 s to 0.4 s
    // before the change, 0.5 s to 0.8 s after it
    auto peakFrequency = [&](int skip, int count) {
        SampleVector block(1);
        double peak = 0.0;
        Complex previous(0.0f, 0.0f);

        for (int i = 0; i < skip + count; i++)
        {
            loop.m_source.pull(block.begin(), 1);
            Complex c(block[0].m_real / SDR_TX_SCALEF, block[0].m_imag / SDR_TX_SCALEF);

            if (i > skip) {
                peak = std::max(peak, std::fabs((double) std::arg(c * std::conj(previous))) * sampleRate / (2.0 * M_PI));
            }
            previous = c;
        }
        return peak;
    };

    double before = peakFrequency(sampleRate / 10, 3 * sampleRate / 10);
    loop.m_settings.m_deviation = 4000.0f;
    loop.apply(QStringList("deviation"));
    double after = peakFrequency(sampleRate / 10, 3 * sampleRate / 10);
    loop.runToEnd();
    const Received& received = loop.take();

    std::printf("      peak frequency %.0f Hz, then %.0f Hz\n", before, after);
    check((std::fabs(after / before - 4000.0 / 4500.0) < 0.01) && contains(received, expected(512, false, 0))
        && (loop.m_source.getStatus().m_sent == 1),
        "Deviation changed during a transmission: frequency scaled, page received");
}

// Requests the stream refuses, or for another protocol, are counted as dropped
void testRefused()
{
    Loop loop(48000, 0);
    RadioPagingModSource::Request bad = pocsagRequest(70, 0, 1200, false);
    bad.m_pocsag.m_address = RadioPagingPocsag::m_maxAddress + 1;
    loop.m_source.addRequest(bad);
    loop.m_source.addRequest(flexRequest(71, 1234, "FLEX", false));
    RadioPagingModSource::Status status = loop.m_source.getStatus();
    check((status.m_dropped == 2) && (status.m_queued == 0) && (status.m_state == RadioPagingModSource::Idle),
        "POCSAG address above 21 bits and a FLEX request on a POCSAG channel: dropped");
}

// ---------------------------------------------------------------------------
// POCSAG network
// ---------------------------------------------------------------------------

// Carrier on without messages (idle batches); a message of the default group goes in at a
// batch boundary, one of another group after a new preamble; the carrier stays on; abort
// only drops what is queued; a change to one-shot ends the transmission
void testPocsagNetwork()
{
    const int sampleRate = 48000;
    Loop loop(sampleRate, 0, RadioPagingModSettings::Pocsag, RadioPagingModSettings::Network);
    std::vector<float> magnitudes;

    loop.runSeconds(1.0, &magnitudes);
    RadioPagingModSource::Status idle = loop.m_source.getStatus();
    bool carrier = true;
    for (size_t i = sampleRate / 10; i < magnitudes.size(); i++) {
        carrier = carrier && (std::fabs(magnitudes[i] - 1.0f) < 0.01f);
    }
    check((idle.m_state == RadioPagingModSource::Tx) && carrier && (idle.m_sent == 0),
        "POCSAG network: carrier on with idle batches, nothing to send");

    loop.m_source.addRequest(pocsagRequest(80, 0, 1200, false));
    loop.runSeconds(2.0);
    const Received& first = loop.take();
    RadioPagingModSource::Status afterFirst = loop.m_source.getStatus();
    check(contains(first, expected(1200, false, 0)) && (afterFirst.m_sent == 1) && (afterFirst.m_lastSentId == 80)
        && (afterFirst.m_state == RadioPagingModSource::Tx), "POCSAG network: a message of the default group received, carrier stays on");

    loop.m_source.addRequest(pocsagRequest(81, 1, 512, true));
    loop.m_source.addRequest(pocsagRequest(82, 0, 2400, false));
    loop.runSeconds(6.0);
    const Received& second = loop.take();
    RadioPagingModSource::Status afterSecond = loop.m_source.getStatus();
    check(contains(second, expected(512, true, 1)) && contains(second, expected(2400, false, 0)) && (afterSecond.m_sent == 3)
        && (afterSecond.m_state == RadioPagingModSource::Tx), "POCSAG network: messages of other groups received after a new preamble");

    // 1200 normal is sent at its next turn: 3 groups in turn, the 2400 group was the last
    loop.m_source.addRequest(pocsagRequest(83, 0, 1200, false));
    loop.m_source.addRequest(pocsagRequest(84, 1, 1200, false));
    loop.m_source.abort();
    loop.runSeconds(0.5);
    RadioPagingModSource::Status aborted = loop.m_source.getStatus();
    check((aborted.m_dropped == 2) && (aborted.m_state == RadioPagingModSource::Tx) && (aborted.m_queued == 0),
        "POCSAG network abort: the queue is dropped, the carrier stays on");

    loop.m_settings.m_mode = RadioPagingModSettings::OneShot;
    loop.apply(QStringList("mode"));
    loop.runSeconds(0.1);
    RadioPagingModSource::Status ended = loop.m_source.getStatus();
    check(ended.m_state == RadioPagingModSource::Idle, "POCSAG network to one-shot: the transmission ends");
}

// ---------------------------------------------------------------------------
// FLEX
// ---------------------------------------------------------------------------

// One-shot: re-synchronization then the frames of the message, received; the carrier is on
// for exactly the re-synchronization and the frames (1.875 s each) at the sample rate
void testFlexOneShot(int sampleRate, int offset, RadioPagingFlex::Mode mode, bool inverted, bool timing)
{
    Loop loop(sampleRate, offset, RadioPagingModSettings::Flex);
    loop.m_settings.m_flexMode = mode;
    loop.m_settings.m_flexInverted = inverted;
    // Collapse 1: the re-synchronization reaches pagers waking every 2 frames (3.75 s), the
    // message goes in the first frame after it
    loop.m_settings.m_flexCollapse = 1;
    loop.apply(QStringList({"flexMode", "flexInverted", "flexCollapse"}));
    std::vector<float> magnitudes;
    const uint64_t capcode = 1234567;
    const std::string text = "FLEX FROM THE MODULATOR";
    uint64_t firstFrame;
    int ersBits;
    RadioPagingFlexTxStream::plan(flexEpochMs, 1, firstFrame, ersBits);
    double ersSeconds = ersBits / (double) RadioPagingFlex::m_syncBaud;

    loop.m_source.addRequest(flexRequest(90, capcode, text, inverted));
    loop.runSeconds(ersSeconds / 2.0, &magnitudes);
    RadioPagingModSource::Status during = loop.m_source.getStatus();
    RadioPagingDemodSink::Status rxDuring = loop.m_sink.getStatus();
    loop.runToEnd(&magnitudes);
    const Received& received = loop.take();
    RadioPagingModSource::Status status = loop.m_source.getStatus();
    RadioPagingDemodSink::Status rx = loop.m_sink.getStatus();

    char what[200];
    std::snprintf(what, sizeof(what), "FLEX one-shot %s %s at %d Hz, offset %d Hz: received, sent 1",
        RadioPagingFlex::modeName(mode), inverted ? "inverted" : "normal", sampleRate, offset);
    check(contains(received, { flexKey(mode, capcode, inverted, text) }) && (status.m_sent == 1)
        && (status.m_lastSentId == 90) && (status.m_state == RadioPagingModSource::Idle), what);

    std::printf("      ERS %.3f s: tx %d (%d ms, %llu), rx %d (%.3f s, %u)\n", ersSeconds, during.m_ers ? 1 : 0, status.m_ersMs,
        (unsigned long long) status.m_ersCount, rxDuring.m_flexErs ? 1 : 0, rx.m_flexErsSeconds, rx.m_flexErsCount);
    check(during.m_ers && !status.m_ers && (status.m_ersMs == ersBits * 1000 / RadioPagingFlex::m_syncBaud) && (status.m_ersCount == 1),
        "FLEX one-shot: ERS shown in the TX status while sent, with its length");
    check(rxDuring.m_flexErs && !rx.m_flexErs && (rx.m_flexErsCount == 1) && (std::fabs(rx.m_flexErsSeconds - ersSeconds) < 0.1),
        "FLEX one-shot: ERS shown in the RX status while received, with its length");

    if (timing)
    {
        size_t first, last;
        carrierSpan(magnitudes, first, last);
        double seconds = (last - first) / (double) sampleRate;
        double expectedSeconds = ersBits / (double) RadioPagingFlex::m_syncBaud + RadioPagingFlex::m_frameMs / 1000.0;
        std::printf("      carrier %.4f s, re-synchronization and one frame %.4f s\n", seconds, expectedSeconds);
        std::snprintf(what, sizeof(what), "FLEX one-shot at %d Hz, collapse 1: carrier on for the re-synchronization of 2 frames and one 1.875 s frame",
            sampleRate);
        check((seconds > expectedSeconds) && (seconds < expectedSeconds + 0.01), what);
    }
}

// Network in 3200/4 normal: idle frames from the start; messages of both polarities, the
// inverted one in a frame given to the other polarity; a change of network polarity ends
// the transmission and starts it again with the re-synchronization, the queue kept; mute
// ends the transmission, unmute starts it again; abort keeps the carrier
void testFlexNetwork()
{
    const int sampleRate = 48000;
    Loop loop(sampleRate, 0, RadioPagingModSettings::Flex, RadioPagingModSettings::Network);
    loop.m_settings.m_flexCollapse = 1;
    loop.m_settings.m_flexMode = RadioPagingFlex::Mode3200_4;
    loop.apply(QStringList({"flexCollapse", "flexMode"}));

    loop.runSeconds(3.0);
    RadioPagingModSource::Status idle = loop.m_source.getStatus();
    check((idle.m_state == RadioPagingModSource::Tx) && (idle.m_sent == 0), "FLEX network: carrier on with idle frames");

    const std::string text = "FLEX NETWORK 3200/4";
    loop.m_source.addRequest(flexRequest(100, 2101300, text, true));
    loop.m_source.addRequest(flexRequest(102, 1234567, "NORMAL", false));
    loop.runSeconds(9.0);
    const Received& received = loop.take();
    RadioPagingModSource::Status sent = loop.m_source.getStatus();
    check(contains(received, { flexKey(RadioPagingFlex::Mode3200_4, 2101300, true, text),
            flexKey(RadioPagingFlex::Mode3200_4, 1234567, false, "NORMAL") }) && (sent.m_sent == 2)
        && (sent.m_state == RadioPagingModSource::Tx) && (sent.m_ersCount == 1),
        "FLEX network collapse 1: messages of both polarities received, one re-synchronization");

    // The network goes inverted: a new transmission, a new re-synchronization, the message queued kept
    loop.m_settings.m_flexInverted = true;
    loop.apply(QStringList("flexInverted"));
    loop.m_source.addRequest(flexRequest(103, 1234567, "AFTER", true));
    loop.runSeconds(10.0);
    const Received& afterChange = loop.take();
    RadioPagingModSource::Status changed = loop.m_source.getStatus();
    check(contains(afterChange, { flexKey(RadioPagingFlex::Mode3200_4, 1234567, true, "AFTER") }) && (changed.m_sent == 3)
        && (changed.m_dropped == 0) && (changed.m_ersCount == 2) && (changed.m_state == RadioPagingModSource::Tx),
        "FLEX network polarity change: restarted with a re-synchronization, the queued message sent");

    loop.m_settings.m_channelMute = true;
    loop.apply(QStringList("channelMute"));
    loop.runSeconds(0.5);
    RadioPagingModSource::Status muted = loop.m_source.getStatus();
    loop.m_settings.m_channelMute = false;
    loop.apply(QStringList("channelMute"));
    loop.runSeconds(0.5);
    RadioPagingModSource::Status unmuted = loop.m_source.getStatus();
    check((muted.m_state == RadioPagingModSource::Idle) && (unmuted.m_state != RadioPagingModSource::Idle),
        "FLEX network mute: the transmission ends, unmuted it starts again");

    loop.m_source.addRequest(flexRequest(101, 1234, "NOT SENT", false));
    loop.m_source.abort();
    loop.runSeconds(0.5);
    RadioPagingModSource::Status aborted = loop.m_source.getStatus();
    check((aborted.m_dropped == 1) && (aborted.m_state != RadioPagingModSource::Idle),
        "FLEX network abort: the queue is dropped, the carrier stays on");

    // The network start, the polarity change and the restart after unmuting; idle and message
    // frames, normal and inverted, are not taken for one
    loop.runSeconds(6.0);
    RadioPagingDemodSink::Status rx = loop.m_sink.getStatus();
    RadioPagingModSource::Status tx = loop.m_source.getStatus();
    std::printf("      ERS sent %llu, received %u\n", (unsigned long long) tx.m_ersCount, rx.m_flexErsCount);
    check((tx.m_ersCount == 3) && (rx.m_flexErsCount == 3), "FLEX network: each ERS sent is received once, frames give none");

    // Another protocol: the transmission ends, POCSAG one-shot has nothing to send
    loop.m_settings.m_protocol = RadioPagingModSettings::Pocsag;
    loop.m_settings.m_mode = RadioPagingModSettings::OneShot;
    loop.apply(QStringList({"protocol", "mode"}));
    loop.runSeconds(0.1);
    check(loop.m_source.getStatus().m_state == RadioPagingModSource::Idle, "FLEX network to POCSAG one-shot: the transmission ends");
}

// FLEX System Event Notification: network only, received in the frames sent, the frames left count down to 0
void testFlexSystemEvent()
{
    std::string error;

    {
        Loop oneShot(48000, 0, RadioPagingModSettings::Flex, RadioPagingModSettings::OneShot);
        Loop gsc(48000, 0, RadioPagingModSettings::Gsc, RadioPagingModSettings::Network);
        check(!oneShot.m_source.notifySystemEvent(1, 128, error) && !gsc.m_source.notifySystemEvent(1, 128, error),
            "System Event Notification: refused in one-shot and for GSC");
    }

    Loop loop(48000, 0, RadioPagingModSettings::Flex, RadioPagingModSettings::Network);
    loop.m_settings.m_flexMode = RadioPagingFlex::Mode1600_2;
    loop.apply(QStringList("flexMode"));
    loop.runSeconds(2.0);
    check(!loop.m_source.notifySystemEvent(1, 100, error) && loop.m_source.notifySystemEvent(0x005, 128, error)
        && (loop.m_source.getStatus().m_systemEventFrames > 0) && (loop.m_source.getStatus().m_systemEvents == 0x005),
        "System Event Notification: 100 frames refused, 128 frames started");
    loop.m_source.addRequest(flexRequest(200, 1234567, "DURING THE EVENT", false));
    loop.runSeconds(20.0);
    const Received& during = loop.take();
    int left = loop.m_source.getStatus().m_systemEventFrames;
    char what[200];
    std::snprintf(what, sizeof(what), "System Event Notification: received in the frames sent (%d records, %d frames left), the message too",
        during.m_systemEvents, left);
    check((during.m_systemEvents >= 8) && (during.m_eventFlags == 0x005) && (left > 0) && (left < 128)
        && contains(during, { flexKey(RadioPagingFlex::Mode1600_2, 1234567, false, "DURING THE EVENT") }), what);

    check(loop.m_source.notifySystemEvent(0, 128, error) && (loop.m_source.getStatus().m_systemEventFrames == 0),
        "System Event Notification: stopped");
    loop.runSeconds(2.0);
    int before = loop.take().m_systemEvents;
    loop.runSeconds(6.0);
    check(loop.take().m_systemEvents == before, "System Event Notification: none after it is stopped");
}

// Roaming: the system ID words are sent, the messages are still received (plain FLEX and mixed with POCSAG)
void testFlexRoaming(bool mixed)
{
    const int sampleRate = 48000;
    Loop loop(sampleRate, 0, RadioPagingModSettings::Flex, RadioPagingModSettings::Network);
    loop.m_settings.m_flexMode = RadioPagingFlex::Mode3200_4;
    loop.m_settings.m_flexRoaming = true;
    loop.m_settings.m_ssidLocalId = 300;
    loop.m_settings.m_ssidCoverageZone = 17;
    loop.m_settings.m_ssidCountryCode = 262;
    loop.m_settings.m_ssidTrafficFlags = 5;
    loop.m_settings.m_flexMixPocsag = mixed;
    // The time service too, in the same request: another FLEX mode, the stream is made again with all of them (was:
    // a stream made again, as at start, did not get the roaming nor the time, sent only after another change)
    loop.m_settings.m_flexTime = true;
    loop.m_settings.m_flexTimeSendZone = true;
    loop.m_settings.m_flexTimeZoneMinutes = 120;
    loop.apply(QStringList({"flexMode", "flexRoaming", "ssidLocalId", "ssidCoverageZone", "ssidCountryCode", "ssidTrafficFlags",
        "mixPocsag", "flexTime", "flexTimeSendZone", "flexTimeZoneMinutes"}));
    loop.runSeconds(3.0);

    loop.m_source.addRequest(flexRequest(200, 2101300, "ROAMING TEXT", true));
    loop.m_source.addRequest(flexRequest(201, 1234567, "ROAMING TWO", false));
    loop.runSeconds(9.0);
    const Received& received = loop.take();
    check(contains(received, { flexKey(RadioPagingFlex::Mode3200_4, 2101300, true, "ROAMING TEXT"),
            flexKey(RadioPagingFlex::Mode3200_4, 1234567, false, "ROAMING TWO") })
        && (loop.m_source.getStatus().m_sent == 2),
        mixed ? "FLEX roaming mixed: messages received" : "FLEX roaming: messages received");

    // The system ID and the time of the network received, in its frames of the normal polarity
    const RadioPagingFlexDecoder::NetworkTime& network = loop.m_sink.getStatus().m_flexTimePol[0];
    check(network.m_ssid1Valid && (network.m_localId == 300) && (network.m_coverageZone == 17),
        mixed ? "FLEX roaming mixed: system ID received" : "FLEX roaming: system ID received");
    check(network.m_timeValid && network.m_zoneValid && (network.m_zoneMinutes == 120),
        mixed ? "FLEX roaming mixed: time and zone received" : "FLEX roaming: time and zone received");

    // A message once the time is known: its record has the network time of its frame, as multimon-ng FLEX_NEXT
    loop.m_source.addRequest(flexRequest(202, 1234571, "ROAMING TIME", false));
    loop.runSeconds(5.0);
    const QVariantMap ft = loop.take().m_flexTimes.count("ROAMING TIME") ? loop.take().m_flexTimes.at("ROAMING TIME") : QVariantMap();
    check(ft.contains("timestamp") && ft["timestamp"].toString().endsWith("+2h") && (ft["precision_seconds"].toDouble() == 0.9375)
            && (ft["time_mode"].toString() == "standard") && ft["tz"].toMap().contains("age_frames")
            && (ft["tz"].toMap()["offset_min"].toInt() == 120),
        mixed ? "FLEX roaming mixed: network time in the record" : "FLEX roaming: network time in the record");
}

// ---------------------------------------------------------------------------
// GSC
// ---------------------------------------------------------------------------

RadioPagingModSource::Request gscRequest(quint64 id, const std::string& address, int number, RadioPagingGsc::MessageType type,
    const std::string& text, bool inverted)
{
    RadioPagingModSource::Request request;
    request.m_id = id;
    request.m_protocol = RadioPagingModSettings::Gsc;
    request.m_gsc.m_address = address;
    request.m_gsc.m_addressNumber = number;
    request.m_gsc.m_type = type;
    request.m_gsc.m_text = text;
    request.m_gscInverted = inverted;
    return request;
}

int gscPreambleOf(const std::string& address)
{
    RadioPagingGsc::TxMessage m;
    RadioPagingGsc::EncodeOptions options;
    m.m_address = address;
    options.m_mode = RadioPagingGsc::Single;
    return RadioPagingGsc::encode({ m }, options).m_preamble;
}

std::string gscKey(const std::string& address, int number, bool inverted, const std::string& text)
{
    return "gsc 600 " + address + "/" + std::to_string(number) + (inverted ? " inverted " : " normal ") + text;
}

// Messages in each mode and polarity: two of one preamble value, one of another, a tone page
void testGsc(int sampleRate, int offset, RadioPagingGsc::Mode mode, bool inverted)
{
    Loop loop(sampleRate, offset, RadioPagingModSettings::Gsc);
    loop.m_settings.m_gscMode = (int) mode;
    loop.m_settings.m_gscInverted = inverted;
    loop.apply(QStringList({"gscMode", "gscInverted"}));

    std::string a = "123456";
    std::string same, other;
    for (int i = 0; (i < 1000) && (same.empty() || other.empty()); i++)
    {
        char address[8];
        std::snprintf(address, sizeof(address), "%06d", 200000 + i * 7);
        int p = gscPreambleOf(address);

        if (p >= 0) {
            (p == gscPreambleOf(a) ? same : other) = address;
        }
    }

    loop.m_source.addRequest(gscRequest(300, a, 2, RadioPagingGsc::Alphanumeric, "GSC FROM THE MODULATOR", inverted));
    loop.m_source.addRequest(gscRequest(301, same, 1, RadioPagingGsc::Numeric, "0123456789", inverted));
    loop.m_source.addRequest(gscRequest(302, other, 3, RadioPagingGsc::Alphanumeric, "OTHER PAGE", inverted));
    loop.m_source.addRequest(gscRequest(303, a, 4, RadioPagingGsc::Tone, "", inverted));
    loop.run(sampleRate / 2);
    loop.runToEnd();
    const Received& received = loop.take();
    RadioPagingModSource::Status status = loop.m_source.getStatus();

    // Without the preamble of a non battery saver transmission the index digit of the address is not known: N
    auto shown = [mode](const std::string& address) {
        return (mode == RadioPagingGsc::NonBatterySaver) ? "N" + address.substr(1) : address;
    };

    std::set<std::string> keys = {
        gscKey(shown(a), 2, inverted, "GSC FROM THE MODULATOR"),
        gscKey(shown(same), 1, inverted, "0123456789"),
        gscKey(shown(other), 3, inverted, "OTHER PAGE"),
        gscKey(shown(a), 4, inverted, "")
    };
    static const char *names[] = {"auto", "single", "batch", "non battery saver"};
    char what[200];
    std::snprintf(what, sizeof(what), "GSC %s %s at %d Hz, offset %d Hz: 4 pages received, sent 4", names[(int) mode],
        inverted ? "inverted" : "normal", sampleRate, offset);
    check(contains(received, keys) && (status.m_sent == 4) && (status.m_dropped == 0) && (status.m_lastSentId >= 300)
        && (status.m_inFlight == 0) && (status.m_queued == 0) && (status.m_state == RadioPagingModSource::Idle), what);
}

// A voice page: the page, the alert, the voice and the turn-off word, received as a voice message
// that ends at the turn-off word, followed by a tone page in the same transmission stream
void testGscVoice(int sampleRate, int offset, bool inverted)
{
    Loop loop(sampleRate, offset, RadioPagingModSettings::Gsc);
    loop.m_settings.m_gscInverted = inverted;
    loop.apply(QStringList("gscInverted"));

    RadioPagingWav::Audio audio;
    audio.m_rate = RadioPagingWav::m_sampleRate;

    for (int i = 0; i < 2 * RadioPagingWav::m_sampleRate; i++) {
        audio.m_samples.push_back(0.5f * std::sin(2.0 * M_PI * 1000.0 * i / RadioPagingWav::m_sampleRate));
    }

    RadioPagingModSource::Request voice = gscRequest(320, "123456", 2, RadioPagingGsc::Voice, "", inverted);
    voice.m_voice = std::make_shared<const RadioPagingWav::Audio>(audio);
    loop.m_source.addRequest(voice);
    loop.m_source.addRequest(gscRequest(321, "123456", 3, RadioPagingGsc::Tone, "", inverted));
    loop.run(sampleRate / 2);
    loop.runToEnd();
    const Received& received = loop.take();
    RadioPagingModSource::Status status = loop.m_source.getStatus();
    char what[220];

    std::snprintf(what, sizeof(what), "GSC voice page %s at %d Hz, offset %d Hz: one voice message ended by the turn-off word, 2 s alert and 2 s voice, sent 2",
        inverted ? "inverted" : "normal", sampleRate, offset);
    check((status.m_sent == 2) && (status.m_dropped == 0) && (received.m_voiceStarts == 1) && (received.m_voiceEnds.size() == 1)
        && (received.m_voiceEnds[0] == "turn_off") && (std::fabs(received.m_voiceSeconds - 4.0) < 0.3)
        && contains(received, { gscKey("123456", 3, inverted, "") }), what);
}

// A live voice page: the page and the alert time, then the voice pushed as it comes (the carrier
// unmodulated when none is waiting), the turn-off word after End
void testGscLive(int sampleRate)
{
    Loop loop(sampleRate, 0, RadioPagingModSettings::Gsc);
    std::vector<std::pair<quint64, bool>> events;
    loop.m_source.setNotify([&events](quint64 id, bool sent) { events.push_back({id, sent}); });
    std::vector<quint64> transmitted;
    loop.m_source.setNotifyTransmitted([&transmitted](quint64 id) { transmitted.push_back(id); });

    RadioPagingModSource::Request live = gscRequest(330, "123456", 2, RadioPagingGsc::Voice, "", false);
    live.m_live = true;
    loop.m_source.addRequest(live);
    loop.m_source.addRequest(gscRequest(331, "123456", 3, RadioPagingGsc::Tone, "", false));

    // Until the alert time is over the voice is not taken
    int steps = 0;

    while (!loop.m_source.getStatus().m_liveOpen && (steps++ < 200)) {
        loop.runSeconds(0.05);
    }

    RadioPagingModSource::Status open = loop.m_source.getStatus();
    bool opened = open.m_liveActive && open.m_liveOpen && (open.m_liveId == 330) && (steps > 60) && (steps < 120);
    // The live page is on air from its first block, not from its last
    bool onAir = (transmitted.size() == 1) && (transmitted[0] == 330);

    // 1.5 s of 1 kHz at the rate of the modulator, 20 ms at a time, with a gap
    std::vector<float> chunk(RadioPagingWav::m_sampleRate / 50);
    size_t at = 0;

    for (int i = 0; i < 75; i++)
    {
        for (float& v : chunk) {
            v = 0.5f * std::sin(2.0 * M_PI * 1000.0 * (at++) / RadioPagingWav::m_sampleRate);
        }

        loop.m_source.pushLive(chunk);
        loop.runSeconds(0.02);
    }

    loop.runSeconds(0.3);                   // No voice: the carrier stays on
    bool stillOn = loop.m_source.getStatus().m_liveOpen && (loop.m_source.getStatus().m_state == RadioPagingModSource::Tx);
    loop.m_source.endLive();
    loop.runToEnd();
    const Received& received = loop.take();
    RadioPagingModSource::Status status = loop.m_source.getStatus();

    check(opened && onAir && stillOn && (status.m_sent == 2) && (status.m_dropped == 0) && !status.m_liveActive
        && (received.m_voiceStarts == 1) && (received.m_voiceEnds.size() == 1) && (received.m_voiceEnds[0] == "turn_off")
        && (received.m_voiceSeconds > 3.6) && (received.m_voiceSeconds < 4.4)
        && contains(received, { gscKey("123456", 3, false, "") })
        && (events.size() == 2) && (events[0] == std::make_pair<quint64, bool>(330, true)) && events[1].second,
        "GSC live voice page: the voice is taken after the alert time, ends on End, both pages sent and reported");

    // Aborted while the alert time goes on: dropped and reported
    Loop aborted(sampleRate, 0, RadioPagingModSettings::Gsc);
    std::vector<std::pair<quint64, bool>> abortEvents;
    aborted.m_source.setNotify([&abortEvents](quint64 id, bool sent) { abortEvents.push_back({id, sent}); });
    aborted.m_source.addRequest(live);
    aborted.runSeconds(1.0);
    aborted.m_source.abort();
    aborted.runToEnd();
    check((abortEvents.size() == 1) && (abortEvents[0] == std::make_pair<quint64, bool>(330, false))
        && !aborted.m_source.getStatus().m_liveActive, "GSC live voice page: an abort during the alert time drops it and reports it");
}

// GSC has no network mode; no voice; the mode setting is ignored
void testGscNoNetwork()
{
    Loop loop(48000, 0, RadioPagingModSettings::Gsc, RadioPagingModSettings::Network);
    RadioPagingModSource::Status idle;

    loop.runSeconds(1.0);
    idle = loop.m_source.getStatus();
    check(idle.m_state == RadioPagingModSource::Idle, "GSC: network mode setting ignored, no carrier without a message");

    loop.m_source.addRequest(gscRequest(310, "123456", 1, RadioPagingGsc::Voice, "", false));
    check(loop.m_source.getStatus().m_dropped == 1, "GSC: a voice page without a sound is refused");

    loop.m_source.addRequest(gscRequest(311, "123456", 1, RadioPagingGsc::Tone, "", false));
    loop.runToEnd();
    check((loop.m_source.getStatus().m_sent == 1) && (loop.m_source.getStatus().m_state == RadioPagingModSource::Idle),
        "GSC with the network setting: one-shot, carrier off after the page");
}

// FLEX network mixed with POCSAG: FLEX and POCSAG (all speeds, both polarities) messages of one
// transmission are received; one re-synchronization; the option off restarts the network
void testMixed(int sampleRate, int offset)
{
    Loop loop(sampleRate, offset, RadioPagingModSettings::Flex, RadioPagingModSettings::Network);
    loop.m_settings.m_flexMixPocsag = true;
    loop.m_settings.m_flexMode = RadioPagingFlex::Mode3200_2;
    loop.m_settings.m_flexCollapse = 1;
    loop.apply(QStringList({"mixPocsag", "flexMode", "flexCollapse"}));

    RadioPagingModSource::Request f1 = flexRequest(200, 1234567, "MIXED FLEX", false);
    RadioPagingModSource::Request f2 = flexRequest(201, 2101300, "MIXED INVERTED", true);
    loop.m_source.addRequest(f1);
    loop.m_source.addRequest(f2);
    loop.m_source.addRequest(pocsagRequest(210, 0, 1200, false));
    loop.m_source.addRequest(pocsagRequest(211, 1, 1200, false));
    loop.m_source.addRequest(pocsagRequest(212, 0, 512, false));
    loop.m_source.addRequest(pocsagRequest(213, 1, 2400, true));
    loop.runSeconds(45.0);

    const Received& received = loop.take();
    RadioPagingModSource::Status status = loop.m_source.getStatus();
    RadioPagingDemodSink::Status rx = loop.m_sink.getStatus();

    std::set<std::string> keys = expected(1200, false);
    std::set<std::string> more = expected(512, false, 0);
    std::set<std::string> fast = expected(2400, true, 1);
    keys.insert(more.begin(), more.end());
    keys.insert(fast.begin(), fast.end());
    keys.insert(flexKey(RadioPagingFlex::Mode3200_2, 1234567, false, "MIXED FLEX"));
    keys.insert(flexKey(RadioPagingFlex::Mode3200_2, 2101300, true, "MIXED INVERTED"));

    char what[200];
    std::snprintf(what, sizeof(what), "FLEX mixed with POCSAG at %d Hz, offset %d Hz: FLEX (both polarities) and POCSAG 512, 1200, 2400 received", sampleRate, offset);
    check(contains(received, keys) && (status.m_sent == 6) && (status.m_dropped == 0) && (status.m_queued == 0) && (status.m_inFlight == 0)
        && (status.m_ersCount == 1) && (rx.m_flexErsCount == 1) && (status.m_state == RadioPagingModSource::Tx), what);

    // The option off: the stream changes, the transmission restarts with a re-synchronization
    loop.m_settings.m_flexMixPocsag = false;
    loop.apply(QStringList("mixPocsag"));
    loop.runSeconds(6.0);
    RadioPagingModSource::Status off = loop.m_source.getStatus();
    check((off.m_ersCount == 2) && (off.m_state != RadioPagingModSource::Idle), "FLEX mixing off: the network restarts with a re-synchronization");

    // Without the option a POCSAG request on the FLEX channel is dropped
    loop.m_source.addRequest(pocsagRequest(214, 0, 1200, false));
    RadioPagingModSource::Status refused = loop.m_source.getStatus();
    check(refused.m_dropped == off.m_dropped + 1, "FLEX channel without mixing: a POCSAG request is dropped");
}

// Cut in a frame (abort in one-shot, mute in network): the messages of the frame are
// counted as dropped, sent + dropped is what was requested
void testFlexCutAccounting()
{
    const int sampleRate = 48000;
    const double frameSeconds = RadioPagingFlex::m_frameMs / 1000.0;

    {
        Loop loop(sampleRate, 0, RadioPagingModSettings::Flex);
        loop.m_source.addRequest(flexRequest(110, 1234, "FIRST", false));
        loop.m_source.addRequest(flexRequest(111, 5678, "SECOND", false));
        loop.runSeconds(frameSeconds + 0.3);    // Re-synchronization of a frame, then in the frame
        RadioPagingModSource::Status during = loop.m_source.getStatus();
        loop.m_source.abort();
        loop.runSeconds(0.1);
        RadioPagingModSource::Status status = loop.m_source.getStatus();
        check((during.m_queued == 0) && (status.m_state == RadioPagingModSource::Idle) && (status.m_sent == 0)
            && (status.m_dropped == 2) && (status.m_inFlight == 0) && (status.m_queued == 0),
            "FLEX one-shot abort in the frame: its messages counted as dropped");
    }

    {
        Loop loop(sampleRate, 0, RadioPagingModSettings::Flex, RadioPagingModSettings::Network);
        loop.m_source.addRequest(flexRequest(120, 1234, "MUTED", false));
        loop.runSeconds(frameSeconds + 0.3);
        loop.m_settings.m_channelMute = true;
        loop.apply(QStringList("channelMute"));
        loop.runSeconds(0.1);
        RadioPagingModSource::Status muted = loop.m_source.getStatus();
        loop.m_settings.m_channelMute = false;
        loop.apply(QStringList("channelMute"));
        loop.runSeconds(2 * frameSeconds);
        RadioPagingModSource::Status status = loop.m_source.getStatus();
        check((muted.m_dropped == 1) && (status.m_sent + status.m_dropped == 1) && (status.m_queued == 0) && (status.m_inFlight == 0),
            "FLEX network mute in the frame: its message counted as dropped, the network goes on");
    }
}

} // namespace

int main()
{
    testRoundTrip(48000, 0);
    testRoundTrip(62500, 10000);
    testRoundTrip(38400, -5000);
    testRamps();
    testQueueAndAbort();
    testAbortStates();
    testZeroRamps();
    testDeviationChange();
    testRefused();
    testPocsagNetwork();

    for (int mode = (int) RadioPagingFlex::Mode1600_2; mode <= (int) RadioPagingFlex::Mode6400_4; mode++)
    {
        for (bool inverted : {false, true}) {
            testFlexOneShot(48000, 0, (RadioPagingFlex::Mode) mode, inverted, (mode == 0) && !inverted);
        }
    }

    testFlexOneShot(38400, -5000, RadioPagingFlex::Mode1600_2, false, true);
    testFlexOneShot(62500, 10000, RadioPagingFlex::Mode6400_4, false, true);
    testFlexNetwork();
    testFlexSystemEvent();
    testFlexRoaming(false);
    testFlexRoaming(true);
    testFlexCutAccounting();
    testMixed(48000, 0);
    testMixed(38400, -5000);
    testMixed(62500, 10000);

    for (int mode = (int) RadioPagingGsc::Auto; mode <= (int) RadioPagingGsc::NonBatterySaver; mode++)
    {
        for (bool inverted : {false, true}) {
            testGsc(48000, 0, (RadioPagingGsc::Mode) mode, inverted);
        }
    }

    testGsc(38400, -5000, RadioPagingGsc::Auto, false);
    testGsc(62500, 10000, RadioPagingGsc::Batch, true);
    testGscLive(48000);
    testGscVoice(48000, 0, false);
    testGscVoice(48000, 0, true);
    testGscVoice(62500, 10000, false);
    testGscNoNetwork();

    std::printf("%d passed, %d failed\n", passes, failures);
    return failures ? 1 : 0;
}
