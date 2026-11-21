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

// Decodes a recording of FM discriminator audio (what multimon-ng reads) with the radio paging demodulator sink: the
// audio is FM modulated again into IQ at the channel sample rate and fed to the sink, all legs running. Each record is
// written as one line of JSON: {"t": seconds into the recording, "r": the message record}.
//
//   sox IN.flac -t raw -r 22050 -e signed -b 16 -c 1 - | radiopagingaudiodecode 22050 [deviation] > OUT.jsonl
//
// The audio is mono signed 16 bit at the rate given. Its level is arbitrary (the gain of the discriminator): it is
// scaled so that the 99.9th percentile of the first minute of its magnitude is the deviation given (default 4800 Hz,
// the outer FLEX level; the receivers measure the levels on each sync anyway).

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include "dsp/dsptypes.h"
#include "util/messagequeue.h"
#include "maincore.h"

#include "radiopagingdemodsink.h"
#include "radiopagingflexreceiver.h"

namespace {

const double channelRate = RadioPagingDemodSettings::m_channelSampleRate;

// Reads up to n samples of the input
size_t readAudio(std::vector<int16_t>& buffer, size_t n)
{
    buffer.resize(n);
    size_t got = std::fread(buffer.data(), sizeof(int16_t), n, stdin);
    buffer.resize(got);
    return got;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: radiopagingaudiodecode RATE [DEVIATION] < audio.s16 > records.jsonl\n");
        return 1;
    }

    // GSC voice files and the like go nowhere near the data of the user
    QStandardPaths::setTestModeEnabled(true);

    // --direct: the audio straight into the FLEX receiver (no FM modulation, no channel filters): the discriminator
    // output the leg would give, normalised to the deviation
    bool direct = false;
    std::vector<char*> args;

    for (int i = 0; i < argc; i++)
    {
        if (std::string(argv[i]) == "--direct") {
            direct = true;
        } else {
            args.push_back(argv[i]);
        }
    }

    argc = (int) args.size();
    argv = args.data();

    const double audioRate = std::atof(argv[1]);
    const double deviation = (argc > 2) ? std::atof(argv[2]) : 4800.0;

    MessageQueue queue;
    RadioPagingDemodSink sink;
    RadioPagingDemodSettings settings;
    sink.setMessageQueueToChannel(&queue);
    sink.applyChannelSettings((int) channelRate, 0, true);
    sink.applySettings(QStringList(), settings, true);

    // The scale: from the first minute of audio
    std::vector<int16_t> audio;
    std::vector<int16_t> first;
    readAudio(first, (size_t) (audioRate * 60.0));
    std::vector<int> magnitudes(first.size());
    std::transform(first.begin(), first.end(), magnitudes.begin(), [](int16_t s) { return std::abs((int) s); });
    double reference = 1.0;

    if (!magnitudes.empty())
    {
        size_t k = (size_t) (0.999 * (magnitudes.size() - 1));
        std::nth_element(magnitudes.begin(), magnitudes.begin() + k, magnitudes.end());
        reference = std::max(1, magnitudes[k]);
    }

    const double scale = deviation / reference;
    std::fprintf(stderr, "audio %.0f Hz, level %.0f is %.0f Hz of deviation\n", audioRate, reference, deviation);

    // Linear interpolation from the audio rate to the channel rate, FM modulation
    const double step = audioRate / channelRate;
    double position = 0.0;          // In audio samples, from the start of the recording
    double base = 0.0;              // Audio sample index of buffer[0]
    double phase = 0.0;
    const double amplitude = 0.5 * SDR_RX_SCALED;
    SampleVector iq;
    std::vector<int16_t> buffer = first;
    int16_t previousLast = 0;
    bool havePrevious = false;
    long long records = 0;

    RadioPagingFlexReceiver flex;
    flex.create(channelRate);

    auto drainFlex = [&](double seconds) {
        while (flex.hasMessage())
        {
            RadioPagingFlexDecoder::Message m = flex.takeMessage();
            QJsonObject d;
            d["capcode"] = (qint64) m.m_capcode;
            d["word_map"] = QString::fromStdString(m.m_wordMap);
            QJsonObject r;
            r["protocol"] = "flex";
            r["kind"] = "message";
            r["direction"] = "rx";
            r["msgType"] = QString::fromStdString(m.m_typeName);
            r["text"] = QString::fromStdString(m.m_text);
            r["errorsUncorrectable"] = (int) std::count(m.m_wordMap.begin(), m.m_wordMap.end(), 'X');
            r["protocolData"] = d;
            std::printf("{\"t\":%.3f,\"r\":%s}\n", seconds, QJsonDocument(r).toJson(QJsonDocument::Compact).constData());
            records++;
        }
    };

    auto drain = [&](double seconds) {
        Message *message;

        while ((message = queue.pop()))
        {
            if (MainCore::MsgMessageRecord::match(*message))
            {
                const MessageRecord& r = ((MainCore::MsgMessageRecord *) message)->getRecord();
                std::printf("{\"t\":%.3f,\"r\":%s}\n", seconds, r.toJsonLine().trimmed().constData());
                records++;
            }
            delete message;
        }
    };

    while (!buffer.empty())
    {
        iq.clear();

        // Samples between the last of the previous buffer and this one are interpolated with it
        auto at = [&](double index) -> double {
            double local = index - base;
            if (local < 0.0) {
                return havePrevious ? previousLast + (buffer[0] - previousLast) * (local + 1.0) : buffer[0];
            }
            size_t i = (size_t) local;
            double frac = local - (double) i;
            double a = buffer[i];
            double b = (i + 1 < buffer.size()) ? buffer[i + 1] : buffer[i];
            return a + (b - a) * frac;
        };

        const double end = base + (double) buffer.size() - 1.0;

        if (direct)
        {
            for (; position < end; position += step) {
                flex.process((float) (at(position) * scale / 4800.0));
            }

            drainFlex(position / audioRate);
            previousLast = buffer.back();
            havePrevious = true;
            base += (double) buffer.size();

            if (readAudio(buffer, (size_t) audioRate) == 0) {
                break;
            }
            continue;
        }

        for (; position < end; position += step)
        {
            phase += 2.0 * M_PI * at(position) * scale / channelRate;
            if (phase > M_PI) {
                phase -= 2.0 * M_PI;
            } else if (phase < -M_PI) {
                phase += 2.0 * M_PI;
            }
            iq.push_back(Sample((FixReal) (amplitude * std::cos(phase)), (FixReal) (amplitude * std::sin(phase))));
        }

        for (size_t i = 0; i < iq.size(); i += 4096)
        {
            size_t stop = std::min(iq.size(), i + 4096);
            sink.feed(iq.begin() + i, iq.begin() + stop);
        }

        drain(position / audioRate);
        previousLast = buffer.back();
        havePrevious = true;
        base += (double) buffer.size();

        if (readAudio(buffer, (size_t) audioRate) == 0) {
            break;
        }
    }

    // A second of silence: the last transmission ends
    iq.assign((size_t) channelRate, Sample((FixReal) amplitude, 0));
    sink.feed(iq.begin(), iq.end());
    drain(position / audioRate);
    std::fprintf(stderr, "%lld records, %.0f s of audio\n", records, position / audioRate);

    if (direct)
    {
        RadioPagingFlexReceiver::Status s = flex.getStatus();
        std::fprintf(stderr, "FLEX: sync 1 %u, frames %u, frame information failures %u\n", s.m_syncs, s.m_frames, s.m_frameInfoFailures);
        const RadioPagingFlexDecoder::Status& d = flex.decoder().status();
        std::fprintf(stderr, "FLEX codewords: clean %u, corrected %u, uncorrectable %u (erased %u), from the other decision %u; "
            "phases decoded from the centre of the symbols %u\n", d.m_codewordsClean, d.m_codewordsCorrected,
            d.m_codewordsUncorrectable, d.m_codewordsErased, d.m_codewordsFromCentre, d.m_phasesFromCentre);
    }
    return 0;
}
