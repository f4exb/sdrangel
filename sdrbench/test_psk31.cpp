///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                    //
// Some code by AI                                                               //
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

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>

#include <QDebug>
#include <QElapsedTimer>
#include <QVector>

#include "dsp/raisedcosine.h"
#include "mainbench.h"
#include "psk31demodsink.h"
#include "util/psk31.h"

namespace {

constexpr float sampleRate = 1000.0f;
constexpr int samplesPerSymbol = 32;
constexpr float measurementBandwidth = 100.0f;
constexpr int noiseDurationSeconds = 3600;
constexpr float noiseLevelDbFS = -40.0f;

struct TrialOptions
{
    float snrDb100Hz = std::numeric_limits<float>::infinity();
    float frequencyOffset = 0.0f;
    float clockErrorPpm = 0.0f;
    int timingOffset = 0;
    float amplitude = 0.2f;
    float initialPhase = std::numeric_limits<float>::quiet_NaN();
    float interfererOffset = 0.0f;
    float interfererRelativeDb = -std::numeric_limits<float>::infinity();
    float rfBandwidth = measurementBandwidth;
    int phaseGlitchStart = -1;
    int phaseGlitchLength = 0;
    quint32 seed = 1;
};

struct NoiseResult
{
    int characters = 0;
    qint64 lockedSamples = 0;
    int lockAcquisitions = 0;
};

QVector<bool> encodeBits(const QString& text)
{
    PSK31Encoder encoder;
    QVector<bool> bits;

    // Reversing phase on every idle zero gives the Costas loop and symbol clock
    // enough time to acquire before any scored text arrives.
    bits.fill(false, 256);

    for (QChar character : text)
    {
        unsigned encoded;
        unsigned int bitCount;
        encoder.encode(character, encoded, bitCount);

        for (unsigned int i = 0; i < bitCount; ++i) {
            bits.append((encoded >> i) & 1U);
        }
    }

    for (int i = 0; i < 32; ++i) {
        bits.append(false);
    }

    return bits;
}

QVector<float> modulate(const QVector<bool>& bits)
{
    QVector<float> waveform;
    waveform.reserve((bits.size() + 4) * samplesPerSymbol);

    RaisedCosine<Real> pulseShape;
    pulseShape.create(1.0, 2, samplesPerSymbol, true);

    bool symbol = false;

    for (bool bit : bits)
    {
        if (!bit) {
            symbol = !symbol;
        }

        for (int sample = 0; sample < samplesPerSymbol; ++sample) {
            waveform.append(pulseShape.filter(sample == 1 ? (symbol ? 1.0f : -1.0f) : 0.0f));
        }
    }

    // Flush the pulse-shaping filter.
    for (int i = 0; i < 3 * samplesPerSymbol; ++i) {
        waveform.append(pulseShape.filter(0.0f));
    }

    return waveform;
}

QString runTrial(const QVector<float>& source, const TrialOptions& options, float* measuredSNR = nullptr)
{
    const double clockScale = 1.0 + options.clockErrorPpm * 1.0e-6;
    QVector<float> resampled;
    resampled.reserve(static_cast<int>(source.size() * clockScale) + 1);

    for (double position = options.timingOffset; position < source.size() - 1; position += 1.0 / clockScale)
    {
        const int index = static_cast<int>(position);
        const float fraction = static_cast<float>(position - index);
        resampled.append(source[index] + fraction * (source[index + 1] - source[index]));
    }

    double meanSignalPower = 0.0;
    for (float sample : resampled) {
        meanSignalPower += sample * sample;
    }
    meanSignalPower = options.amplitude * options.amplitude * meanSignalPower / resampled.size();

    // SNR is quoted in the demodulator's 100 Hz RF bandwidth. The generated
    // complex AWGN spans the 1000 sample/s simulation bandwidth.
    double noisePower = 0.0;
    if (std::isfinite(options.snrDb100Hz))
    {
        const double ratio = std::pow(10.0, options.snrDb100Hz / 10.0);
        noisePower = meanSignalPower * (sampleRate / measurementBandwidth) / ratio;
    }

    std::mt19937 generator(options.seed);
    std::normal_distribution<float> gaussian(0.0f, std::sqrt(noisePower / 2.0));
    std::uniform_real_distribution<float> phaseDistribution(-M_PI, M_PI);
    const float initialPhase = std::isfinite(options.initialPhase)
        ? options.initialPhase : phaseDistribution(generator);
    const float interfererAmplitude = std::isfinite(options.interfererRelativeDb)
        ? options.amplitude * std::pow(10.0f, options.interfererRelativeDb / 20.0f)
        : 0.0f;

    PSK31DemodSink sink;
    PSK31DemodSettings settings;
    settings.m_rfBandwidth = options.rfBandwidth;
    sink.applySettings(QStringList(), settings, true);
    sink.applyChannelSettings(sampleRate, 0, true);

    QString received;
    sink.setCharacterSink([&received](QChar character) { received.append(character); });

    for (int i = 0; i < resampled.size(); ++i)
    {
        const float carrierPhase = initialPhase + 2.0f * M_PI * options.frequencyOffset * i / sampleRate;
        Complex sample = options.amplitude * resampled[i] * Complex(std::cos(carrierPhase), std::sin(carrierPhase));

        if ((i >= options.phaseGlitchStart) && (i < options.phaseGlitchStart + options.phaseGlitchLength)) {
            sample = -sample;
        }

        if (interfererAmplitude > 0.0f)
        {
            const float interfererPhase = 0.37f + 2.0f * M_PI * options.interfererOffset * i / sampleRate;
            sample += interfererAmplitude * Complex(std::cos(interfererPhase), std::sin(interfererPhase));
        }

        sample += Complex(gaussian(generator), gaussian(generator));
        sink.feedTestSample(sample);
    }

    if (measuredSNR) {
        *measuredSNR = sink.getSNR();
    }

    return received;
}

NoiseResult runNoiseTrial(int durationSeconds, float levelDbFS, quint32 seed)
{
    const float noisePower = std::pow(10.0f, levelDbFS / 10.0f);
    std::mt19937 generator(seed);
    std::normal_distribution<float> gaussian(0.0f, std::sqrt(noisePower / 2.0f));

    PSK31DemodSink sink;
    PSK31DemodSettings settings;
    settings.m_rfBandwidth = measurementBandwidth;
    sink.applySettings(QStringList(), settings, true);
    sink.applyChannelSettings(sampleRate, 0, true);

    NoiseResult result;
    sink.setCharacterSink([&result](QChar) { ++result.characters; });
    bool wasLocked = false;
    const int sampleCount = static_cast<int>(durationSeconds * sampleRate);

    for (int i = 0; i < sampleCount; ++i)
    {
        sink.feedTestSample(Complex(gaussian(generator), gaussian(generator)));
        const bool locked = sink.isLocked();
        result.lockedSamples += locked;
        result.lockAcquisitions += locked && !wasLocked;
        wasLocked = locked;
    }

    return result;
}

void measureNoiseRejection()
{
    const NoiseResult noise = runNoiseTrial(noiseDurationSeconds, noiseLevelDbFS, 6000);
    qInfo().noquote() << "Noise_only_duration_s,noise_level_dBFS,locked_samples,lock_acquisitions,false_characters";
    qInfo().noquote() << QString("%1,%2,%3,%4,%5")
        .arg(noiseDurationSeconds)
        .arg(noiseLevelDbFS, 0, 'f', 0)
        .arg(noise.lockedSamples)
        .arg(noise.lockAcquisitions)
        .arg(noise.characters);
}

void measureCPUPerformance()
{
    constexpr int blockSize = 4096;
    constexpr qint64 minimumElapsedMilliseconds = 1000;

    QVector<Complex> block(blockSize);
    for (int i = 0; i < blockSize; ++i)
    {
        const Real inPhase = ((i / samplesPerSymbol) & 1) ? 0.2f : -0.2f;
        const Real quadrature = 0.002f * std::sin(2.0f * M_PI * i / 97.0f);
        block[i] = Complex(inPhase, quadrature);
    }

    PSK31DemodSink sink;
    PSK31DemodSettings settings;
    settings.m_rfBandwidth = measurementBandwidth;
    sink.applySettings(QStringList(), settings, true);
    sink.applyChannelSettings(sampleRate, 0, true);

    qint64 processedSamples = 0;
    QElapsedTimer timer;
    timer.start();

    do
    {
        for (const Complex& sample : block) {
            sink.feedTestSample(sample);
        }
        processedSamples += blockSize;
    } while (timer.elapsed() < minimumElapsedMilliseconds);

    const double elapsedSeconds = timer.nsecsElapsed() * 1.0e-9;
    const double samplesPerSecond = processedSamples / elapsedSeconds;
    const double realTimeFactor = samplesPerSecond / sampleRate;

    qInfo().noquote() << "CPU_internal_samples_per_second,real_time_factor,elapsed_seconds";
    qInfo().noquote() << QString("%1,%2,%3")
        .arg(samplesPerSecond, 0, 'f', 0)
        .arg(realTimeFactor, 0, 'f', 0)
        .arg(elapsedSeconds, 0, 'f', 3);
}

bool framePassed(const QVector<float>& waveform, const QString& payload, const TrialOptions& options)
{
    return runTrial(waveform, options).contains(payload);
}

int trailingCharacterErrors(const QString& received, const QString& payload)
{
    const QString scored = received.right(payload.size());
    int errors = std::abs(scored.size() - payload.size());
    const int compared = std::min(scored.size(), payload.size());

    for (int i = 0; i < compared; ++i) {
        errors += scored[i] != payload[payload.size() - compared + i];
    }

    return errors;
}

void measurePerformance(const QVector<float>& waveform, const QString& payload)
{
    qInfo().noquote() << "PSK31 RX performance benchmark";
    qInfo().noquote() << "SNR definition: average signal power / AWGN power in 100 Hz; acquired carrier; exact-frame criterion";
    qInfo().noquote() << "SNR_dB,frames_passed,frames_total";

    for (int snrDb = 0; snrDb <= 16; ++snrDb)
    {
        int passed = 0;
        constexpr int trials = 50;

        for (int trial = 0; trial < trials; ++trial)
        {
            TrialOptions options;
            options.snrDb100Hz = snrDb;
            options.initialPhase = 0.0f; // Measure AWGN sensitivity after carrier acquisition.
            options.seed = 1000 + trial;
            passed += framePassed(waveform, payload, options);
        }

        qInfo().noquote() << QString("%1,%2,%3").arg(snrDb).arg(passed).arg(trials);
    }

    qInfo().noquote() << "Residual_frequency_Hz,frames_passed,frames_total";
    for (int offset = -28; offset <= 28; ++offset)
    {
        int passed = 0;
        constexpr int trials = 10;

        for (int trial = 0; trial < trials; ++trial)
        {
            TrialOptions options;
            options.frequencyOffset = offset;
            options.seed = 2000 + trial;
            passed += framePassed(waveform, payload, options);
        }

        qInfo().noquote() << QString("%1,%2,%3").arg(offset).arg(passed).arg(trials);
    }

    qInfo().noquote() << "Sample_clock_error_ppm,frames_passed,frames_total,total_character_errors";
    for (int ppm = -5000; ppm <= 5000; ppm += 500)
    {
        int passed = 0;
        int characterErrors = 0;
        constexpr int trials = 5;

        for (int trial = 0; trial < trials; ++trial)
        {
            TrialOptions options;
            options.clockErrorPpm = ppm;
            options.timingOffset = trial * 6;
            options.seed = 3000 + trial;
            const QString received = runTrial(waveform, options);
            passed += received.contains(payload);
            characterErrors += trailingCharacterErrors(received, payload);
        }

        qInfo().noquote() << QString("%1,%2,%3,%4").arg(ppm).arg(passed).arg(trials).arg(characterErrors);
    }

    qInfo().noquote() << "Signal_level_dBFS,timing_phases_passed,timing_phases_total";
    for (int levelDb = -10; levelDb >= -60; levelDb -= 5)
    {
        int passed = 0;
        constexpr int trials = 8;

        for (int trial = 0; trial < trials; ++trial)
        {
            TrialOptions options;
            options.amplitude = std::pow(10.0f, levelDb / 20.0f);
            options.timingOffset = trial * 4;
            options.seed = 4000 + trial;
            passed += framePassed(waveform, payload, options);
        }

        qInfo().noquote() << QString("%1,%2,%3").arg(levelDb).arg(passed).arg(trials);
    }

    qInfo().noquote() << "Interferer_offset_Hz,interferer_over_signal_dB,frames_passed,frames_total";
    for (int offset : {25, 50, 75, 100})
    {
        for (int relativeDb : {-20, -10, 0, 6, 10})
        {
            int passed = 0;
            constexpr int trials = 5;

            for (int trial = 0; trial < trials; ++trial)
            {
                TrialOptions options;
                options.interfererOffset = offset;
                options.interfererRelativeDb = relativeDb;
                options.seed = 5000 + trial;
                passed += framePassed(waveform, payload, options);
            }

            qInfo().noquote() << QString("%1,%2,%3,%4")
                .arg(offset).arg(relativeDb).arg(passed).arg(trials);
        }
    }

    measureNoiseRejection();
    measureCPUPerformance();
}

}

void MainBench::testPSK31(const QString& argsStr)
{
    PSK31Encoder encoder;
    PSK31Decoder decoder;
    QVector<int> decoded;
    PSK31DemodSettings serverSettings;

    // Server builds have no scope GUI. Serializing their settings must remain safe.
    bool success = !serverSettings.serialize().isEmpty();

    PSK31DemodSettings edgeSettings;
    edgeSettings.m_rfBandwidth = 0.0f;
    edgeSettings.m_reverseAPIPort = 1;
    edgeSettings.m_udpPort = 65535;
    PSK31DemodSettings restoredEdgeSettings;
    const bool edgeSettingsRestored = restoredEdgeSettings.deserialize(edgeSettings.serialize());
    const bool settingsValidationPassed = edgeSettingsRestored
        && (restoredEdgeSettings.m_rfBandwidth == PSK31DemodSettings::PSK31DEMOD_MIN_RF_BANDWIDTH)
        && (restoredEdgeSettings.m_reverseAPIPort == 1)
        && (restoredEdgeSettings.m_udpPort == 65535)
        && (PSK31DemodSettings::validateRFBandwidth(
            std::numeric_limits<Real>::quiet_NaN()) == 100.0f);
    success = success && settingsValidationPassed;

    // Exercise every entry in the shared extended-ASCII Varicode table as one
    // continuous stream. This also checks character-boundary synchronization.
    for (int value = 0; value < 256; ++value)
    {
        unsigned bits;
        unsigned int bitCount;
        encoder.encode(QChar::fromLatin1(static_cast<char>(value)), bits, bitCount);

        for (unsigned int bit = 0; bit < bitCount; ++bit)
        {
            QChar character;
            if (decoder.decode((bits >> bit) & 1U, character)) {
                decoded.append(character.toLatin1() & 0xff);
            }
        }
    }

    QChar finalCharacter;
    if (decoder.decode(false, finalCharacter)) {
        decoded.append(finalCharacter.toLatin1() & 0xff);
    }

    success = success && (decoded.size() == 256);
    for (int i = 0; success && (i < decoded.size()); ++i) {
        success = decoded[i] == i;
    }

    decoder.reset();
    QChar character;
    for (int i = 0; i < 8; ++i) {
        success = success && !decoder.decode(false, character);
    }
    for (int i = 0; i < 13; ++i) {
        success = success && !decoder.decode(true, character);
    }
    success = success && !decoder.decode(false, character);
    success = success && !decoder.decode(false, character);

    unsigned bits;
    unsigned int bitCount;
    encoder.encode('A', bits, bitCount);
    bool gotA = false;
    for (unsigned int bit = 0; bit < bitCount; ++bit) {
        gotA = decoder.decode((bits >> bit) & 1U, character) || gotA;
    }
    gotA = decoder.decode(false, character) || gotA;
    success = success && gotA && (character == 'A');

    const QString message("CQ TEST\r\n");
    QString received;
    bool symbol = false;
    decoder.reset();
    decoder.decodeSymbol(symbol, character);

    for (QChar sourceCharacter : message)
    {
        encoder.encode(sourceCharacter, bits, bitCount);

        for (unsigned int bit = 0; bit < bitCount; ++bit)
        {
            if (((bits >> bit) & 1U) == 0U) {
                symbol = !symbol;
            }

            if (decoder.decodeSymbol(symbol, character)) {
                received.append(character);
            }
        }
    }

    symbol = !symbol;
    if (decoder.decodeSymbol(symbol, character)) {
        received.append(character);
    }
    success = success && (received == message);

    const QString payload("CQ CQ DE SDRANGEL 0123456789\r\n");
    const QVector<float> waveform = modulate(encodeBits(payload));
    TrialOptions nominal;
    nominal.snrDb100Hz = 20.0f;
    float measuredSNR;
    const bool nominalPassed = runTrial(waveform, nominal, &measuredSNR).contains(payload);
    const bool snrPassed = std::isfinite(measuredSNR) && (std::abs(measuredSNR - nominal.snrDb100Hz) < 5.0f);

    TrialOptions wideBandwidth = nominal;
    wideBandwidth.rfBandwidth = 200.0f;
    float wideBandwidthSNR;
    const bool wideBandwidthPassed = runTrial(waveform, wideBandwidth, &wideBandwidthSNR).contains(payload);
    const bool bandwidthNormalized = std::isfinite(wideBandwidthSNR)
        && (std::abs(wideBandwidthSNR - measuredSNR) < 1.0f);
    success = success && nominalPassed && snrPassed && wideBandwidthPassed && bandwidthNormalized;

    TrialOptions glitch;
    glitch.phaseGlitchStart = 250 * samplesPerSymbol + 2;
    glitch.phaseGlitchLength = 12;
    const bool glitchPassed = framePassed(waveform, payload, glitch);
    success = success && glitchPassed;
    qInfo() << "PSK31 regressions: nominal" << nominalPassed
            << "reported SNR" << measuredSNR << "valid" << snrPassed
            << "200 Hz SNR" << wideBandwidthSNR << "normalized" << bandwidthNormalized
            << "phase glitch" << glitchPassed
            << "settings validation" << settingsValidationPassed;

    const NoiseResult noise = runNoiseTrial(noiseDurationSeconds, noiseLevelDbFS, 6000);
    success = success && (noise.characters == 0);

    if (argsStr.contains("perf", Qt::CaseInsensitive)) {
        measurePerformance(waveform, payload);
    }
    else if (argsStr.contains("noise", Qt::CaseInsensitive)) {
        measureNoiseRejection();
    }
    else if (argsStr.contains("cpu", Qt::CaseInsensitive)) {
        measureCPUPerformance();
    }

    if (argsStr.contains("trace", Qt::CaseInsensitive))
    {
        for (int ppm : {-5000, 5000})
        {
            TrialOptions options;
            options.clockErrorPpm = ppm;
            const QString trace = runTrial(waveform, options);
            qInfo().noquote() << QString("clock %1 ppm: %2").arg(ppm, 6).arg(QString(trace.toLatin1().toHex()));
        }
    }

    if (success) {
        qInfo() << "MainBench::testPSK31: success";
    } else {
        qCritical() << "MainBench::testPSK31: failed";
    }
}
