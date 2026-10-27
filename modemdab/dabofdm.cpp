///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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
#include <cstdio>
#include <cstdlib>
#include <numeric>

#include "dabprofile.h"
#include "dabofdm.h"

namespace modemdab
{

namespace
{

constexpr double Pi = 3.14159265358979323846;
constexpr int Margin = ModeI::FFTSize;          // Samples kept either side of a frame
constexpr int NullBlock = 64;                   // Power averaging block for null detection
constexpr int NullWindowBlocks = 40;            // 2560 samples, inside the 2656 sample null
constexpr int TimingMargin = 24;                // Samples of guard kept before the first path
constexpr int MaxTrackingStep = 32;             // Larger timing steps need confirming
constexpr float MinAcquisitionQuality = 3.0f;   // Integer offset correlation peak to mean
constexpr float MinImpulseQualityDB = 9.0f;     // Impulse response peak to mean, to lock
constexpr float MinTrackingQualityDB = 6.0f;    // and to keep lock
constexpr int MaxBadFrames = 8;
// A frame whose impulse response quality is this far below the locked average has lost the
// signal's timing: samples were lost before the input, so the frames are not where expected.
// A few such frames in a row are enough to search again.
constexpr float CollapseDB = 12.0f;
constexpr int MaxCollapsedFrames = 3;
// Band filter: overlap-save with a 4096 point FFT and a 511 tap Kaiser windowed lowpass
constexpr int FilterFFTSize = 4096;
constexpr int FilterTaps = 511;
constexpr int FilterStep = FilterFFTSize - FilterTaps + 1;
constexpr double FilterCutoffHz = 785000.0;

bool debugEnabled()
{
    static const bool enabled = std::getenv("MODEMDAB_DEBUG") != nullptr;
    return enabled;
}

inline int binForCarrier(int k)
{
    return (k + ModeI::FFTSize) % ModeI::FFTSize;
}

// Index of carrier k (-768..768, k != 0) among the 1536 active carriers
inline int carrierIndex(int k)
{
    return k < 0 ? k + 768 : k + 767;
}

} // anonymous namespace

DABFrontEnd::DABFrontEnd() :
    m_state(State::Searching),
    m_searchRange(35),
    m_bufferStart(0),
    m_frameStart(0),
    m_frameStartFraction(0.0),
    m_frequency(0.0),
    m_clockErrorPPM(0.0f),
    m_driftPerFrame(0.0f),
    m_haveTiming(false),
    m_lastFineTiming(0.0),
    m_integerOffsetVotes(0),
    m_lastIntegerOffset(0),
    m_badFrames(0),
    m_pendingShift(0),
    m_acquisitionAttempts(0),
    m_locks(0),
    m_fft(ModeI::FFTSize, false),
    m_ifft(ModeI::FFTSize, true),
    m_bandFilter(true),
    m_dcRemoval(true),
    m_impulseAverage(0.0f),
    m_haveImpulseAverage(false),
    m_lastNullDepth(0.0f),
    m_haveLastNullDepth(false),
    m_dc(0.0, 0.0),
    m_detection(Detection::Differential),
    m_exportCarriers(false),
    m_filterFFT(FilterFFTSize, false),
    m_filterIFFT(FilterFFTSize, true),
    m_haveCarrierPower(false),
    m_haveCarrierNoise(false),
    m_noiseFrames(0)
{
    // Kaiser windowed sinc (beta 7.86 for about 80 dB of stop band attenuation)
    std::vector<std::complex<float>> taps(FilterFFTSize, 0.0f);
    const double fc = FilterCutoffHz / ModeI::SampleRate;
    const double beta = 7.86;
    auto bessel0 = [](double x) {
        double sum = 1.0;
        double term = 1.0;
        for (int k = 1; k < 30; k++)
        {
            term *= (x / (2.0 * k)) * (x / (2.0 * k));
            sum += term;
        }
        return sum;
    };
    const double centre = (FilterTaps - 1) / 2.0;
    for (int n = 0; n < FilterTaps; n++)
    {
        const double t = n - centre;
        const double sinc = t == 0.0 ? 2.0 * fc : std::sin(2.0 * Pi * fc * t) / (Pi * t);
        const double r = t / centre;
        const double window = bessel0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / bessel0(beta);
        taps[n] = static_cast<float>(sinc * window / FilterFFTSize);
    }
    m_filterResponse.resize(FilterFFTSize);
    m_filterFFT.transform(taps.data(), m_filterResponse.data());
    m_filterInput.assign(FilterTaps - 1, 0.0f);
    m_filterWork.resize(FilterFFTSize);

    m_fftIn.resize(ModeI::FFTSize);
    m_symbols.resize(ModeI::SymbolsPerFrame, std::vector<std::complex<float>>(ModeI::FFTSize));
    m_differential.resize((ModeI::SymbolsPerFrame - 1) * ModeI::Carriers);
    m_softCarrier.resize(ModeI::Carriers);
    for (int i = 0; i < ModeI::Carriers; i++) {
        m_softCarrier[i] = carrierIndex(DABTables::frequencyInterleaving()[i]);
    }
    m_carrierNoise.resize(ModeI::Carriers, 1.0f);
    m_lastNullPower.resize(ModeI::Carriers, 0.0f);
    m_carrierPower.resize(ModeI::Carriers, 0.0f);
    m_frame.ficSoft.resize(ModeI::FICSymbols * ModeI::BitsPerSymbol);
    m_frame.mscSoft.resize(ModeI::MSCSymbols * ModeI::BitsPerSymbol);
    m_frame.nullSpectrum.resize(ModeI::FFTSize);
}

void DABFrontEnd::setSearchRange(int rangeHz)
{
    m_searchRange = std::max(1, std::min(rangeHz / 1000, 300));
}

void DABFrontEnd::setBandFilter(bool enabled)
{
    m_bandFilter = enabled;
}

void DABFrontEnd::filter(const std::complex<float> *samples, std::size_t count)
{
    DABProfileScope profile(DABProfile::BandFilter);
    m_filterInput.insert(m_filterInput.end(), samples, samples + count);
    std::vector<std::complex<float>> spectrum(FilterFFTSize);
    std::size_t position = 0;

    // Each block of the FFT size gives FilterStep outputs free of circular wrap
    while (m_filterInput.size() - position >= static_cast<std::size_t>(FilterFFTSize))
    {
        m_filterFFT.transform(&m_filterInput[position], spectrum.data());
        for (int i = 0; i < FilterFFTSize; i++) {
            spectrum[i] *= m_filterResponse[i];
        }
        m_filterIFFT.transform(spectrum.data(), m_filterWork.data());
        m_buffer.insert(m_buffer.end(), m_filterWork.begin() + (FilterTaps - 1), m_filterWork.end());
        position += FilterStep;
    }
    m_filterInput.erase(m_filterInput.begin(), m_filterInput.begin() + position);
}

void DABFrontEnd::reset()
{
    m_bufferStart += static_cast<int64_t>(m_buffer.size());
    m_buffer.clear();
    m_acquisitionAttempts = 0;
    m_locks = 0;
    m_frequency = 0.0;
    m_clockErrorPPM = 0.0f;
    m_driftPerFrame = 0.0f;
    reacquire();
}

void DABFrontEnd::reacquire()
{
    m_state = State::Searching;
    m_haveTiming = false;
    m_haveCarrierNoise = false;
    m_noiseFrames = 0;
    m_haveCarrierPower = false;
    m_haveLastNullDepth = false;
    m_haveImpulseAverage = false;
    std::fill(m_lastNullPower.begin(), m_lastNullPower.end(), 0.0f);
    m_frameStartFraction = 0.0;
    m_integerOffsetVotes = 0;
    m_badFrames = 0;
    m_pendingShift = 0;
}

void DABFrontEnd::process(const std::complex<float> *samples, std::size_t count)
{
    if (m_dcRemoval && (count > 0))
    {
        // The mean of each block, smoothed with a time constant of about 0.1 s; the
        // ensemble's own contribution averages out far below the carriers
        std::complex<double> sum = 0.0;
        for (std::size_t i = 0; i < count; i++) {
            sum += std::complex<double>(samples[i]);
        }
        const double alpha = std::min(1.0, static_cast<double>(count) / (0.1 * ModeI::SampleRate));
        m_dc += alpha * (sum / static_cast<double>(count) - m_dc);
        const std::complex<float> dc(m_dc);
        m_dcRemoved.resize(count);
        for (std::size_t i = 0; i < count; i++) {
            m_dcRemoved[i] = samples[i] - dc;
        }
        samples = m_dcRemoved.data();
    }

    if (m_bandFilter) {
        filter(samples, count);
    } else {
        m_buffer.insert(m_buffer.end(), samples, samples + count);
    }

    while (true)
    {
        const int64_t bufferEnd = m_bufferStart + static_cast<int64_t>(m_buffer.size());

        if (m_state == State::Searching)
        {
            const int64_t needed = 2 * ModeI::FrameSize + 3 * ModeI::NullSize + 2 * Margin;
            if (static_cast<int64_t>(m_buffer.size()) < needed) {
                break;
            }
            m_acquisitionAttempts++;
            if (!acquire()) {
                discard(m_bufferStart + ModeI::FrameSize / 2, true);
            }
        }
        else
        {
            if (m_frameStart - Margin < m_bufferStart)
            {
                // Timing moved before the buffered input: search again further on, as the
                // same input would give the same timing
                if (debugEnabled()) {
                    std::fprintf(stderr, "track: timing moved before the input, reacquire\n");
                }
                reacquire();
                discard(m_bufferStart + ModeI::FrameSize / 2, true);
                continue;
            }
            if (m_frameStart + ModeI::FrameSize + Margin > bufferEnd) {
                break;
            }
            processFrame();
            discard(m_frameStart - 2 * Margin);
        }
    }
}

void DABFrontEnd::discard(int64_t upTo, bool now)
{
    int64_t count = upTo - m_bufferStart;

    if (count <= 0) {
        return;
    }
    count = std::min(count, static_cast<int64_t>(m_buffer.size()));
    // Otherwise erase in large steps only, to limit copying
    if (!now && (count < ModeI::FrameSize) && (count < static_cast<int64_t>(m_buffer.size()))) {
        return;
    }
    m_buffer.erase(m_buffer.begin(), m_buffer.begin() + count);
    m_bufferStart += count;
}

void DABFrontEnd::mix(int64_t start, int length, double frequency, std::vector<std::complex<float>>& out) const
{
    DABProfileScope profile(DABProfile::Mixing);
    const double increment = -2.0 * Pi * frequency / ModeI::SampleRate;
    const std::complex<float> *in = &m_buffer[start - m_bufferStart];

    // The rotation within a block of 1024 samples is tabulated, and the block's starting
    // rotation computed exactly, so rounding errors do not accumulate
    constexpr int Block = 1024;
    float tableRe[Block];
    float tableIm[Block];
    for (int j = 0; j < Block; j++)
    {
        tableRe[j] = static_cast<float>(std::cos(increment * j));
        tableIm[j] = static_cast<float>(std::sin(increment * j));
    }

    out.resize(length);
    const float *src = reinterpret_cast<const float *>(in);
    float *dst = reinterpret_cast<float *>(out.data());

    for (int i = 0; i < length; i += Block)
    {
        const float baseRe = static_cast<float>(std::cos(increment * i));
        const float baseIm = static_cast<float>(std::sin(increment * i));
        const int count = std::min(Block, length - i);
        const float *x = &src[2 * i];
        float *y = &dst[2 * i];

        for (int j = 0; j < count; j++)
        {
            const float rotationRe = baseRe * tableRe[j] - baseIm * tableIm[j];
            const float rotationIm = baseRe * tableIm[j] + baseIm * tableRe[j];
            const float re = x[2 * j];
            const float im = x[2 * j + 1];
            y[2 * j] = re * rotationRe - im * rotationIm;
            y[2 * j + 1] = re * rotationIm + im * rotationRe;
        }
    }
}

std::complex<float> DABFrontEnd::cyclicPrefixCorrelation(const std::complex<float> *frame, int windowOffset) const
{
    DABProfileScope profile(DABProfile::Mixing);
    // The guard interval repeats the end of each symbol, 2048 samples later
    std::complex<double> sum = 0.0;

    for (int l = 1; l <= ModeI::SymbolsPerFrame; l++)
    {
        const int start = ModeI::NullSize + (l - 1) * ModeI::SymbolSize + windowOffset;
        for (int n = 16; n < ModeI::GuardSize - 16; n++) {
            sum += std::complex<double>(frame[start + n] * std::conj(frame[start + n + ModeI::FFTSize]));
        }
    }
    return std::complex<float>(static_cast<float>(sum.real()), static_cast<float>(sum.imag()));
}

void DABFrontEnd::symbolFFT(const std::complex<float> *window, std::vector<std::complex<float>>& out)
{
    DABProfileScope profile(DABProfile::SymbolFFT);
    out.resize(ModeI::FFTSize);
    m_fft.transform(window, out.data());
}

int DABFrontEnd::integerFrequencyOffset(const std::vector<std::complex<float>>& prs, int range, float& quality) const
{
    // Correlate the phase differences of adjacent carriers with those of the phase reference.
    // A timing error rotates every difference equally, so only the magnitude is compared.
    const auto& reference = DABTables::phaseReference();
    std::vector<std::complex<float>> normalized(ModeI::FFTSize);

    for (int i = 0; i < ModeI::FFTSize; i++)
    {
        float magnitude = std::abs(prs[i]);
        normalized[i] = magnitude > 0.0f ? prs[i] / magnitude : 0.0f;
    }

    std::vector<float> correlation(2 * range + 1);
    int best = 0;

    for (int s = -range; s <= range; s++)
    {
        std::complex<float> sum = 0.0f;

        for (int k = -ModeI::Carriers / 2 + 1; k <= ModeI::Carriers / 2; k++)
        {
            if ((k == 0) || (k == 1)) {
                continue;
            }
            const std::complex<float> rx = normalized[binForCarrier(k + s)] * std::conj(normalized[binForCarrier(k - 1 + s)]);
            const std::complex<float> ref = reference[k + 768] * std::conj(reference[k - 1 + 768]);
            sum += rx * std::conj(ref);
        }
        correlation[s + range] = std::abs(sum);
        if (correlation[s + range] > correlation[best]) {
            best = s + range;
        }
    }

    float others = 0.0f;
    for (int i = 0; i < 2 * range + 1; i++)
    {
        if (i != best) {
            others += correlation[i];
        }
    }
    others /= std::max(1, 2 * range);
    quality = others > 0.0f ? correlation[best] / others : 0.0f;
    return best - range;
}

int DABFrontEnd::timingFromImpulseResponse(const std::vector<std::complex<float>>& prs, float& qualityDB, double& fineTiming)
{
    const auto& reference = DABTables::phaseReference();
    std::vector<std::complex<float>> h(ModeI::FFTSize, 0.0f);
    std::vector<std::complex<float>> impulse(ModeI::FFTSize);
    std::complex<float> slope = 0.0f;
    std::complex<float> previous = 0.0f;

    for (int k = -ModeI::Carriers / 2; k <= ModeI::Carriers / 2; k++)
    {
        if (k == 0)
        {
            previous = 0.0f;
            continue;
        }
        const std::complex<float> v = prs[binForCarrier(k)] * std::conj(reference[k + 768]);
        h[binForCarrier(k)] = v;
        slope += v * std::conj(previous);
        previous = v;
    }

    // Mean delay, used to track the sample clock
    fineTiming = -std::arg(slope) * ModeI::FFTSize / (2.0 * Pi);

    m_ifft.transform(h.data(), impulse.data());

    std::vector<float> power(2 * ModeI::FFTSize + 1);
    float peak = 0.0f;
    double total = 0.0;

    for (int n = 0; n < ModeI::FFTSize; n++)
    {
        const float p = std::norm(impulse[n]);
        power[n] = p;
        peak = std::max(peak, p);
        total += p;
    }
    const double mean = total / ModeI::FFTSize;
    qualityDB = (mean > 0.0) ? static_cast<float>(10.0 * std::log10(peak / mean)) : 0.0f;

    // Energy inside the guard interval for each window position: paths at n in [d, d + guard]
    // are received without inter-symbol interference when the window moves by d.
    std::vector<double> prefix(2 * ModeI::FFTSize + 1, 0.0);
    for (int i = 0; i < 2 * ModeI::FFTSize; i++) {
        prefix[i + 1] = prefix[i] + power[i % ModeI::FFTSize];
    }

    const int length = ModeI::GuardSize + 1;
    std::vector<double> energy(ModeI::FFTSize);
    int bestIndex = 0;

    // Index i stands for a shift of i - FFTSize / 2
    for (int i = 0; i < ModeI::FFTSize; i++)
    {
        const int shift = i - ModeI::FFTSize / 2;
        const int start = (shift + ModeI::FFTSize) % ModeI::FFTSize;
        energy[i] = prefix[start + length] - prefix[start];
        if (energy[i] > energy[bestIndex]) {
            bestIndex = i;
        }
    }

    // The window can move anywhere on the plateau around the best position without losing
    // energy: put it as late as possible, less a margin, so pre-echoes are also covered.
    const double threshold = 0.98 * energy[bestIndex];
    int last = bestIndex;
    int first = bestIndex;

    while ((last + 1 < ModeI::FFTSize) && (energy[last + 1] >= threshold)) {
        last++;
    }
    while ((first > 0) && (energy[first - 1] >= threshold)) {
        first--;
    }

    const int margin = std::min(TimingMargin, (last - first) / 2);
    return (last - margin) - ModeI::FFTSize / 2;
}

bool DABFrontEnd::acquire()
{
    DABProfileScope profile(DABProfile::Acquisition);
    // Find the null symbol: the lowest power window in a frame's worth of input
    const int searchStart = Margin;
    const int searchBlocks = (ModeI::FrameSize + ModeI::NullSize) / NullBlock;
    const int totalBlocks = searchBlocks + NullWindowBlocks;
    std::vector<double> blockPower(totalBlocks);

    for (int b = 0; b < totalBlocks; b++)
    {
        double sum = 0.0;
        const std::complex<float> *p = &m_buffer[searchStart + b * NullBlock];
        for (int i = 0; i < NullBlock; i++) {
            sum += std::norm(p[i]);
        }
        blockPower[b] = sum;
    }

    double window = 0.0;
    for (int b = 0; b < NullWindowBlocks; b++) {
        window += blockPower[b];
    }

    double minimum = window;
    int minimumBlock = 0;

    for (int b = 1; b < searchBlocks; b++)
    {
        window += blockPower[b + NullWindowBlocks - 1] - blockPower[b - 1];
        if (window < minimum)
        {
            minimum = window;
            minimumBlock = b;
        }
    }

    const double mean = std::accumulate(blockPower.begin(), blockPower.begin() + ModeI::FrameSize / NullBlock, 0.0)
        / (ModeI::FrameSize / NullBlock);

    if (debugEnabled()) {
        std::fprintf(stderr, "acquire: null ratio %.3f at block %d\n", mean > 0.0 ? minimum / (NullWindowBlocks * mean) : 0.0, minimumBlock);
    }
    if ((mean <= 0.0) || (minimum / (NullWindowBlocks * mean) > 0.5)) {
        return false;
    }

    const int64_t nullStart = m_bufferStart + searchStart + static_cast<int64_t>(minimumBlock) * NullBlock;

    // Fractional frequency offset from the cyclic prefix (modulo the 1 kHz carrier spacing)
    const std::complex<float> *frame = &m_buffer[nullStart - m_bufferStart];
    const std::complex<float> cp = cyclicPrefixCorrelation(frame, 0);
    double frequency = -std::arg(cp) / (2.0 * Pi) * (ModeI::SampleRate / ModeI::FFTSize);

    // Integer offset from the phase reference symbol
    std::vector<std::complex<float>> prs;
    const int prsWindow = Margin + ModeI::NullSize + ModeI::GuardSize;

    mix(nullStart - Margin, ModeI::FrameSize + 2 * Margin, frequency, m_mixed);
    symbolFFT(&m_mixed[prsWindow], prs);

    float quality;
    const int offset = integerFrequencyOffset(prs, m_searchRange, quality);
    if (debugEnabled()) {
        std::fprintf(stderr, "acquire: fraction %.1f Hz, integer offset %d quality %.2f\n", frequency, offset, quality);
    }
    if (quality < MinAcquisitionQuality) {
        return false;
    }
    frequency += offset * (ModeI::SampleRate / ModeI::FFTSize);

    mix(nullStart - Margin, ModeI::FrameSize + 2 * Margin, frequency, m_mixed);
    symbolFFT(&m_mixed[prsWindow], prs);

    float impulseQuality;
    double fineTiming;
    const int shift = timingFromImpulseResponse(prs, impulseQuality, fineTiming);
    if (debugEnabled()) {
        std::fprintf(stderr, "acquire: timing shift %d impulse quality %.1f dB\n", shift, impulseQuality);
    }
    if (impulseQuality < MinImpulseQualityDB) {
        return false;
    }
    // A null at the start of the input, whose frame starts before it: take the next one
    if (nullStart + shift - Margin < m_bufferStart) {
        return false;
    }

    m_frequency = frequency;
    m_frameStart = nullStart + shift;
    m_frameStartFraction = 0.0;
    m_state = State::Locked;
    m_locks++;
    m_haveTiming = false;
    m_haveCarrierNoise = false;
    m_noiseFrames = 0;
    m_haveCarrierPower = false;
    m_haveLastNullDepth = false;
    m_haveImpulseAverage = false;
    std::fill(m_lastNullPower.begin(), m_lastNullPower.end(), 0.0f);
    m_badFrames = 0;
    m_integerOffsetVotes = 0;
    return true;
}

bool DABFrontEnd::processFrame()
{
    const int64_t frameStart = m_frameStart;
    const int frameLength = ModeI::FrameSize + 2 * Margin;

    // Track the fractional offset from the cyclic prefix of the raw input: its phase is the
    // total offset modulo 1 kHz, so only the integer part of the current estimate is kept.
    const std::complex<float> cp = cyclicPrefixCorrelation(&m_buffer[frameStart - m_bufferStart], 0);
    const double spacing = ModeI::SampleRate / ModeI::FFTSize;
    const double fraction = -std::arg(cp) / (2.0 * Pi) * spacing;
    double frequency = std::round((m_frequency - fraction) / spacing) * spacing + fraction;

    mix(frameStart - Margin, frameLength, frequency, m_mixed);

    std::vector<std::complex<float>> prs;
    const int prsWindow = Margin + ModeI::NullSize + ModeI::GuardSize;
    symbolFFT(&m_mixed[prsWindow], prs);

    // An integer carrier slip is only accepted when seen in consecutive frames
    float offsetQuality;
    const int offset = integerFrequencyOffset(prs, 2, offsetQuality);
    if ((offset != 0) && (offsetQuality >= MinAcquisitionQuality))
    {
        if (offset == m_lastIntegerOffset) {
            m_integerOffsetVotes++;
        } else {
            m_integerOffsetVotes = 1;
        }
        m_lastIntegerOffset = offset;
        if (m_integerOffsetVotes >= 3)
        {
            frequency += offset * spacing;
            m_integerOffsetVotes = 0;
            mix(frameStart - Margin, frameLength, frequency, m_mixed);
            symbolFFT(&m_mixed[prsWindow], prs);
        }
    }
    else
    {
        m_integerOffsetVotes = 0;
    }

    float impulseQuality;
    double fineTiming;
    int shift = timingFromImpulseResponse(prs, impulseQuality, fineTiming);

    const bool collapsed = m_haveImpulseAverage && (impulseQuality < m_impulseAverage - CollapseDB);
    if ((impulseQuality < MinTrackingQualityDB) || collapsed)
    {
        // The frequency and timing estimates of a bad frame are not used
        m_badFrames++;
        if (debugEnabled()) {
            std::fprintf(stderr, "track: bad impulse response %d (quality %.1f dB, average %.1f dB, shift %d)\n", m_badFrames,
                impulseQuality, m_impulseAverage, shift);
        }
        if (m_badFrames >= (collapsed ? MaxCollapsedFrames : MaxBadFrames)) {
            if (debugEnabled()) {
                std::fprintf(stderr, "track: reacquire after %d bad frames\n", m_badFrames);
            }
            reacquire();
        } else {
            m_frameStart += ModeI::FrameSize;
        }
        return false;
    }
    m_badFrames = 0;
    m_frequency = frequency;
    m_impulseAverage = m_haveImpulseAverage ? 0.9f * m_impulseAverage + 0.1f * impulseQuality : impulseQuality;
    m_haveImpulseAverage = true;

    // Large steps need to be seen twice, so that one noisy impulse response cannot move the timing
    if (std::abs(shift) > MaxTrackingStep)
    {
        if ((m_pendingShift != 0) && (std::abs(shift - m_pendingShift) <= 16))
        {
            m_pendingShift = 0;
        }
        else
        {
            m_pendingShift = shift;
            shift = 0;
        }
    }
    else
    {
        m_pendingShift = 0;
    }

    // Sample clock: the drift of the mean channel delay from frame to frame
    const double timing = static_cast<double>(frameStart) + fineTiming;
    if (m_haveTiming)
    {
        const double drift = timing - m_lastFineTiming - ModeI::FrameSize;
        if (std::abs(drift) < 40.0)
        {
            m_driftPerFrame = 0.9f * m_driftPerFrame + 0.1f * static_cast<float>(drift);
            m_clockErrorPPM = m_driftPerFrame / ModeI::FrameSize * 1.0e6f;
        }
    }
    m_lastFineTiming = timing;
    m_haveTiming = true;

    if (shift != 0) {
        symbolFFT(&m_mixed[prsWindow + shift], prs);
    }

    // FFT of every symbol, correcting the timing drift within the frame
    for (int l = 1; l <= ModeI::SymbolsPerFrame; l++)
    {
        const int window = Margin + shift + ModeI::NullSize + (l - 1) * ModeI::SymbolSize + ModeI::GuardSize;
        std::vector<std::complex<float>>& symbol = m_symbols[l - 1];

        if (l == 1) {
            symbol = prs;
        } else {
            symbolFFT(&m_mixed[window], symbol);
        }

        const float tau = m_driftPerFrame * static_cast<float>((l - 1) * ModeI::SymbolSize) / ModeI::FrameSize;
        if (std::abs(tau) > 0.001f)
        {
            // A phase slope across the carriers, by recurrence from the lowest carrier
            const double step = 2.0 * Pi * tau / ModeI::FFTSize;
            const std::complex<float> increment(static_cast<float>(std::cos(step)), static_cast<float>(std::sin(step)));
            std::complex<float> rotation(static_cast<float>(std::cos(step * (-ModeI::Carriers / 2))),
                static_cast<float>(std::sin(step * (-ModeI::Carriers / 2))));
            for (int k = -ModeI::Carriers / 2; k <= ModeI::Carriers / 2; k++)
            {
                symbol[binForCarrier(k)] *= rotation;
                rotation *= increment;
            }
        }
    }

    // Null symbol: spectrum for TII, and the noise of each carrier
    const int nullWindow = Margin + shift + (ModeI::NullSize - ModeI::FFTSize) / 2;
    std::vector<std::complex<float>> nullFFT;
    symbolFFT(&m_mixed[nullWindow], nullFFT);
    for (int i = 0; i < ModeI::FFTSize; i++) {
        m_frame.nullSpectrum[i] = std::norm(nullFFT[i]);
    }
    estimateNoise(nullFFT);

    demodulate();
    carrierToNoise();

    double nullPower = 0.0;
    for (int n = 64; n < ModeI::NullSize - 64; n++) {
        nullPower += std::norm(m_mixed[Margin + shift + n]);
    }
    nullPower /= (ModeI::NullSize - 128);
    double symbolPower = 0.0;
    for (int n = ModeI::NullSize; n < ModeI::FrameSize; n += 7) {
        symbolPower += std::norm(m_mixed[Margin + shift + n]);
    }
    symbolPower /= (ModeI::FrameSize - ModeI::NullSize) / 7;

    m_frame.startSample = frameStart + shift;
    m_frame.frequencyOffsetHz = static_cast<float>(m_frequency);
    m_frame.clockErrorPPM = m_clockErrorPPM;
    // TII carriers are in the null of alternate frames only: the lower of two frames is the
    // null without them
    const float nullDepth = (nullPower > 0.0 && symbolPower > 0.0) ? static_cast<float>(10.0 * std::log10(nullPower / symbolPower)) : 0.0f;
    m_frame.nullDepthDB = m_haveLastNullDepth ? std::min(nullDepth, m_lastNullDepth) : nullDepth;
    m_lastNullDepth = nullDepth;
    m_haveLastNullDepth = true;
    m_frame.prsQualityDB = impulseQuality;
    m_frame.timingAdjustment = shift;

    // Next frame, including the measured clock drift
    m_frameStartFraction += m_driftPerFrame;
    const int64_t whole = static_cast<int64_t>(std::floor(m_frameStartFraction));
    m_frameStartFraction -= static_cast<double>(whole);
    m_frameStart = frameStart + shift + ModeI::FrameSize + whole;

    if (m_frameCallback) {
        m_frameCallback(m_frame);
    }
    return true;
}

void DABFrontEnd::estimateNoise(const std::vector<std::complex<float>>& nullSymbol)
{
    DABProfileScope profile(DABProfile::Demodulation);
    // TII carriers are on in alternate frames only: the lower power of two consecutive
    // frames leaves the noise and interference
    std::vector<float> power(ModeI::Carriers);

    for (int k = -ModeI::Carriers / 2; k <= ModeI::Carriers / 2; k++)
    {
        if (k == 0) {
            continue;
        }
        const int c = carrierIndex(k);
        const float p = std::norm(nullSymbol[binForCarrier(k)]);
        power[c] = std::min(p, m_lastNullPower[c]);
        m_lastNullPower[c] = p;
    }

    // Smooth across neighbouring carriers, then over frames
    for (int c = 0; c < ModeI::Carriers; c++)
    {
        const float sum = power[std::max(0, c - 1)] + power[c] + power[std::min(ModeI::Carriers - 1, c + 1)];
        const float smoothed = sum / 3.0f;
        if (m_haveCarrierNoise) {
            m_carrierNoise[c] = 0.9f * m_carrierNoise[c] + 0.1f * smoothed;
        } else {
            m_carrierNoise[c] = smoothed;
        }
    }
    // The first frame has no previous null to compare with
    static const int warmUp = 2;
    if (m_noiseFrames < warmUp) {
        m_noiseFrames++;
    }
    m_haveCarrierNoise = m_noiseFrames >= warmUp;
}

void DABFrontEnd::carrierToNoise()
{
    // Each carrier's mean power over the frame (signal and noise), smoothed over frames
    for (int c = 0; c < ModeI::Carriers; c++)
    {
        const int bin = c < ModeI::Carriers / 2 ? c + ModeI::FFTSize - ModeI::Carriers / 2 : c - ModeI::Carriers / 2 + 1;
        float power = 0.0f;
        for (int l = 0; l < ModeI::SymbolsPerFrame; l++) {
            power += std::norm(m_symbols[l][bin]);
        }
        power /= ModeI::SymbolsPerFrame;
        m_carrierPower[c] = m_haveCarrierPower ? 0.9f * m_carrierPower[c] + 0.1f * power : power;
    }
    m_haveCarrierPower = true;

    // The noise estimate is the lower of two nulls (to remove TII), which halves noise on
    // average, so it is doubled here
    m_frame.carrierCN.resize(ModeI::Carriers);
    for (int c = 0; c < ModeI::Carriers; c++)
    {
        const float n = 2.0f * std::max(m_carrierNoise[c], 1.0e-30f);
        const float s = std::max(m_carrierPower[c] - n, 1.0e-3f * n);
        m_frame.carrierCN[c] = 10.0f * std::log10(s / n);
    }
    // The median carrier: a strong interferer on a few carriers, which the decoder copes
    // with, shows in the carriers' C/N rather than dominating this
    if (m_haveCarrierNoise)
    {
        std::vector<float> sorted = m_frame.carrierCN;
        std::nth_element(sorted.begin(), sorted.begin() + ModeI::Carriers / 2, sorted.end());
        m_frame.cnDB = sorted[ModeI::Carriers / 2];
    }
    else
    {
        m_frame.cnDB = 0.0f;
    }
}

void DABFrontEnd::demodulate()
{
    DABProfileScope profile(DABProfile::Demodulation);
    const int differentialSymbols = ModeI::SymbolsPerFrame - 1;
    std::vector<std::complex<float>>& differential = m_differential;
    std::vector<float> sumAbs(ModeI::Carriers, 0.0f);
    std::vector<float> sumAbsSquared(ModeI::Carriers, 0.0f);
    // Carriers -768..-1 are bins 1280..2047 and carriers 1..768 bins 1..768
    const int firstBin[2] = {ModeI::FFTSize - ModeI::Carriers / 2, 1};

    for (int l = 2; l <= ModeI::SymbolsPerFrame; l++)
    {
        const float *current = reinterpret_cast<const float *>(m_symbols[l - 1].data());
        const float *previous = reinterpret_cast<const float *>(m_symbols[l - 2].data());
        float *d = reinterpret_cast<float *>(&differential[(l - 2) * ModeI::Carriers]);

        for (int half = 0; half < 2; half++)
        {
            const float *x = &current[2 * firstBin[half]];
            const float *y = &previous[2 * firstBin[half]];
            float *out = &d[2 * half * (ModeI::Carriers / 2)];
            float *sa = &sumAbs[half * (ModeI::Carriers / 2)];
            float *sq = &sumAbsSquared[half * (ModeI::Carriers / 2)];

            for (int i = 0; i < ModeI::Carriers / 2; i++)
            {
                // x * conj(y)
                const float re = x[2 * i] * y[2 * i] + x[2 * i + 1] * y[2 * i + 1];
                const float im = x[2 * i + 1] * y[2 * i] - x[2 * i] * y[2 * i + 1];
                out[2 * i] = re;
                out[2 * i + 1] = im;
                const float ar = std::abs(re);
                const float ai = std::abs(im);
                sa[i] += ar + ai;
                sq[i] += ar * ar + ai * ai;
            }
        }
    }

    // MER: each component's mean amplitude and its variance about it
    const double n = 2.0 * differentialSymbols;
    double signal = 0.0;
    double noise = 0.0;

    for (int c = 0; c < ModeI::Carriers; c++)
    {
        const double mu = sumAbs[c] / n;
        signal += mu * mu;
        noise += std::max(sumAbsSquared[c] / n - mu * mu, 1.0e-30);
    }
    m_frame.merDB = noise > 0.0 ? static_cast<float>(10.0 * std::log10(signal / noise)) : 0.0f;
    // The differential product has twice the noise of a symbol
    m_frame.snrDB = m_frame.merDB + 3.01f;

    // The real part of the differential product is the optimum metric when the noise is the
    // same on every carrier (its amplitude already follows the channel). Carriers with more
    // noise, from interference, are weighted down in proportion.
    std::vector<float> weight(ModeI::Carriers, 1.0f);
    if (m_haveCarrierNoise)
    {
        std::vector<float> sorted = m_carrierNoise;
        std::nth_element(sorted.begin(), sorted.begin() + ModeI::Carriers / 2, sorted.end());
        const float reference = sorted[ModeI::Carriers / 2];
        for (int c = 0; c < ModeI::Carriers; c++) {
            weight[c] = std::max(1.0e-3f, std::min(2.0f, reference / std::max(m_carrierNoise[c], 1.0e-30f)));
        }
    }

    // Carriers in carrier index order, for three-symbol detection and the coherent pass
    auto carrier = [this](int l, int c) -> const std::complex<float>& {
        return m_symbols[l - 1][c < ModeI::Carriers / 2 ? c + ModeI::FFTSize - ModeI::Carriers / 2 : c - ModeI::Carriers / 2 + 1];
    };

    if (m_exportCarriers)
    {
        m_frame.carriers.resize(ModeI::SymbolsPerFrame * ModeI::Carriers);
        for (int l = 1; l <= ModeI::SymbolsPerFrame; l++)
        {
            for (int c = 0; c < ModeI::Carriers; c++) {
                m_frame.carriers[(l - 1) * ModeI::Carriers + c] = carrier(l, c);
            }
        }
        m_frame.weights = weight;
    }
    else
    {
        m_frame.carriers.clear();
        m_frame.weights.clear();
    }

    const std::complex<float> *source = differential.data();
    if (m_detection == Detection::ThreeSymbol)
    {
        // Noncoherent detection over symbols l-2, l-1 and l: the metric of hypotheses
        // y(l-1) = q(a), y(l) = q(c) is |r(l-2) + r(l-1) conj(q(a)) + r(l) conj(q(a) q(c))|^2.
        // Each bit's soft value is the best metric with the bit 0 less the best with it 1,
        // scaled so that the two symbol case gives Re/Im of the differential product.
        // q(a) = exp(j (pi/4 + a pi/2)), so q(a) q(c) is a multiple of j: conj of it rotates
        // r(l) by -90 degrees (a + c + 1) times.
        m_threeSymbol.resize(differential.size());
        const float a = 1.0f / std::sqrt(2.0f);
        const float scale = 1.0f / (2.0f * std::sqrt(2.0f));
        // Symbol 2 has only the phase reference before it
        std::copy(differential.begin(), differential.begin() + ModeI::Carriers, m_threeSymbol.begin());

        for (int l = 3; l <= ModeI::SymbolsPerFrame; l++)
        {
            std::complex<float> *out = &m_threeSymbol[(l - 2) * ModeI::Carriers];
            for (int c = 0; c < ModeI::Carriers; c++)
            {
                const std::complex<float> A = carrier(l - 2, c);
                const std::complex<float> B = carrier(l - 1, c);
                const std::complex<float> C = carrier(l, c);
                // B conj(q(a))
                const std::complex<float> Bq[4] = {
                    std::complex<float>(a * (B.real() + B.imag()), a * (B.imag() - B.real())),
                    std::complex<float>(a * (-B.real() + B.imag()), a * (-B.imag() - B.real())),
                    std::complex<float>(a * (-B.real() - B.imag()), a * (-B.imag() + B.real())),
                    std::complex<float>(a * (B.real() - B.imag()), a * (B.imag() + B.real()))
                };
                // C (-j)^k
                const std::complex<float> Cr[4] = {
                    C,
                    std::complex<float>(C.imag(), -C.real()),
                    -C,
                    std::complex<float>(-C.imag(), C.real())
                };
                float best[4] = {-1.0e30f, -1.0e30f, -1.0e30f, -1.0e30f};    // By c
                for (int h = 0; h < 4; h++)
                {
                    const std::complex<float> AB = A + Bq[h];
                    for (int q = 0; q < 4; q++) {
                        best[q] = std::max(best[q], std::norm(AB + Cr[(h + q + 1) & 3]));
                    }
                }
                // Real part positive for c = 0, 3; imaginary part positive for c = 0, 1
                out[c] = std::complex<float>(
                    scale * (std::max(best[0], best[3]) - std::max(best[1], best[2])),
                    scale * (std::max(best[0], best[1]) - std::max(best[2], best[3])));
            }
        }
        source = m_threeSymbol.data();
    }

    // Soft bits, in the order of the QPSK symbols before frequency interleaving
    double sumSoft = 0.0;
    for (int l = 2; l <= ModeI::SymbolsPerFrame; l++)
    {
        const std::complex<float> *d = &source[(l - 2) * ModeI::Carriers];
        float *out = (l <= 1 + ModeI::FICSymbols)
            ? &m_frame.ficSoft[(l - 2) * ModeI::BitsPerSymbol]
            : &m_frame.mscSoft[(l - 2 - ModeI::FICSymbols) * ModeI::BitsPerSymbol];
        float symbolSum = 0.0f;

        for (int i = 0; i < ModeI::Carriers; i++)
        {
            const int c = m_softCarrier[i];
            const float re = d[c].real() * weight[c];
            const float im = d[c].imag() * weight[c];
            out[i] = re;
            out[i + ModeI::Carriers] = im;
            symbolSum += std::abs(re) + std::abs(im);
        }
        sumSoft += symbolSum;
    }

    // Limit outliers (impulsive interference) to a few times the mean magnitude
    const float limit = static_cast<float>(8.0 * sumSoft / (2.0 * differentialSymbols * ModeI::Carriers));
    if (limit > 0.0f)
    {
        for (float& v : m_frame.ficSoft) {
            v = std::max(-limit, std::min(limit, v));
        }
        for (float& v : m_frame.mscSoft) {
            v = std::max(-limit, std::min(limit, v));
        }
    }
}

} // namespace modemdab
