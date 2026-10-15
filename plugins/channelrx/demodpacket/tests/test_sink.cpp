// Copyright (C) 2026 SDRangel contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCoreApplication>
#include <iostream>
#include <fstream>
#include "packettestsignal.h"
#include "packetdemodsink.h"
#include "packetdemodwebapiadapter.h"
#include "SWGChannelSettings.h"
#include "maincore.h"

using namespace PacketTest;

namespace {
void feed(PacketDemodSink& sink, const std::vector<Complex>& iq)
{
    SampleVector samples;
    samples.reserve(iq.size());
    for (const auto& s : iq) {
        samples.emplace_back(static_cast<FixReal>(s.real() * SDR_RX_SCALEF * 0.5f),
            static_cast<FixReal>(s.imag() * SDR_RX_SCALEF * 0.5f));
    }
    for (size_t pos = 0; pos < samples.size();) {
        const size_t end = std::min(samples.size(), pos + 137);
        sink.feed(samples.begin() + pos, samples.begin() + end);
        pos = end;
    }
}

std::vector<QByteArray> packets(MessageQueue& queue)
{
    std::vector<QByteArray> result;
    while (Message* message = queue.pop()) {
        if (MainCore::MsgPacket::match(*message)) {
            result.push_back(static_cast<MainCore::MsgPacket*>(message)->getPacket());
        }
        delete message;
    }
    return result;
}

std::vector<Complex> afsk(const std::vector<QByteArray>& frames)
{
    const auto symbols = radioSymbols(hdlc(frames), false);
    std::vector<Complex> iq;
    double audio = 0.0, carrier = 0.0;
    for (const int symbol : symbols) {
        for (int i = 0; i < 32; ++i) {
            audio += 2.0 * pi * (symbol ? 1200 : 2200) / 38400.0;
            carrier += 2.0 * pi * 2500.0 * std::sin(audio) / 38400.0;
            iq.emplace_back(std::cos(carrier), std::sin(carrier));
        }
    }
    return iq;
}

void run()
{
    PacketDemodWebAPIAdapter adapter;
    SWGSDRangel::SWGChannelSettings request;
    request.setPacketDemodSettings(new SWGSDRangel::SWGPacketDemodSettings());
    request.getPacketDemodSettings()->init();
    QString error;
    for (int mode : {0, 1, 0, 1}) {
        request.getPacketDemodSettings()->setMode(mode);
        require(adapter.webapiSettingsPutPatch(false, QStringList{"mode"}, request, error) == 200,
            "valid mode rejected by API");
        SWGSDRangel::SWGChannelSettings response;
        adapter.webapiSettingsGet(response, error);
        require(response.getPacketDemodSettings()->getMode() == mode, "API mode round-trip failed");
        require(response.getPacketDemodSettings()->getRfBandwidth() == 12500.0f,
            "mode-only API update changed RF bandwidth");
    }
    for (int invalid : {-1, 2, 9600, 2147483647}) {
        request.getPacketDemodSettings()->setMode(invalid);
        require(adapter.webapiSettingsPutPatch(false, QStringList{"mode"}, request, error) == 400,
            "invalid mode accepted by API");
        SWGSDRangel::SWGChannelSettings response;
        adapter.webapiSettingsGet(response, error);
        require(response.getPacketDemodSettings()->getMode() == 1,
            "rejected API request modified the mode");
    }
    PacketDemodSink sink(nullptr);
    MessageQueue queue;
    sink.setMessageQueueToChannel(&queue);
    sink.setChannel(nullptr);
    PacketDemodSettings settings;
    settings.m_mode = PacketDemodSettings::ModeG3RUH9600;
    settings.m_rfBandwidth = 20000.0f;
    // Intentionally leave m_mlse=true and m_chase=6: mode must gate both in DSP.
    std::vector<QByteArray> expected;
    for (int i = 0; i < 10; ++i) { expected.push_back(frame(i, 32 + i * 17)); }
    // Identical consecutive frames must not inherit MLSE duplicate suppression.
    expected.back() = expected[expected.size() - 2];
    const auto symbols = radioSymbols(hdlc(expected), true);
    int cases = 0;
    int failures = 0;
    for (double rate : {32000.0, 38400.0, 48000.0, 96000.0}) {
        for (double offset : {-1000.0, 1000.0}) {
            Channel c; c.sampleRate = rate; c.offset = offset; c.ppm = 250; c.timing = 0.43;
            sink.applySettings(QStringList(), settings, true);
            sink.applyChannelSettings(static_cast<int>(rate), 0, true);
            feed(sink, waveform(symbols, c));
            const auto decoded = packets(queue);
            std::cout << "sink rate=" << rate << " offset=" << offset << " frames=" << decoded.size() << std::endl;
            if (decoded != expected) { ++failures; }
            ++cases;
        }
    }
    require(failures == 0, "sink resampler / RF filter failed");
    // Switch the same running sink to AFSK and back using partial settings.
    // Test both old AFSK detectors, then return with m_mlse still true.
    const std::vector<QByteArray> afskExpected(expected.begin(), expected.begin() + 3);
    for (bool mlse : {false, true}) {
        settings.m_mode = PacketDemodSettings::ModeAFSK1200;
        settings.m_rfBandwidth = 12500.0f;
        settings.m_mlse = mlse;
        sink.applyChannelSettings(38400, 0, true);
        sink.applySettings(QStringList{"mode", "rfBandwidth", "mlse"}, settings);
        std::mt19937 rng(123);
        std::normal_distribution<float> normal(0.0f, 0.001f);
        std::vector<Complex> noise(38400);
        for (auto& x : noise) { x = Complex(normal(rng), normal(rng)); }
        feed(sink, noise); // MLSE's burst gate needs an observed noise floor.
        feed(sink, afsk(afskExpected));
        feed(sink, noise); // Close the burst and allow deferred decoding.
        feed(sink, noise);
        feed(sink, noise);
        const auto afskPackets = packets(queue);
        std::cout << "AFSK mlse=" << mlse << " frames=" << afskPackets.size() << std::endl;
        require(afskPackets == afskExpected, "9600 -> 1200 mode switch failed");
        settings.m_mode = PacketDemodSettings::ModeG3RUH9600;
        settings.m_rfBandwidth = 20000.0f;
        sink.applySettings(QStringList{"mode", "rfBandwidth"}, settings);
        feed(sink, waveform(symbols, Channel()));
        require(packets(queue) == expected, "1200 -> 9600 mode switch failed");
    }
    std::cout << "PASS: sink " << cases << " resampling cases x 10 exact frames; AFSK correlator/MLSE mode switching\n";
}
}

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    try {
        if (argc == 2) {
            // Optional external capture: interleaved little-endian float32 IQ,
            // normalized to +/-1, at 38400 Hz and already centered on the signal.
            std::ifstream input(argv[1], std::ios::binary);
            require(input.good(), "cannot open IQ fixture");
            std::vector<Complex> iq;
            float pair[2];
            while (input.read(reinterpret_cast<char*>(pair), sizeof(pair))) {
                iq.emplace_back(pair[0], pair[1]);
            }
            PacketDemodSink sink(nullptr);
            MessageQueue queue;
            sink.setChannel(nullptr);
            sink.setMessageQueueToChannel(&queue);
            PacketDemodSettings settings;
            settings.m_mode = PacketDemodSettings::ModeG3RUH9600;
            settings.m_rfBandwidth = 20000.0f;
            sink.applySettings(QStringList(), settings, true);
            feed(sink, iq);
            const auto decoded = packets(queue);
            for (const auto& packet : decoded) { std::cout << packet.toHex().constData() << '\n'; }
            require(!decoded.empty(), "IQ fixture decoded no frames");
        } else {
            run();
        }
    }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
    return 0;
}
