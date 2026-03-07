///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
//                                                                               //
// This is the code from ft8mon: https://github.com/rtmrtmrtmrtm/ft8mon          //
// reformatted and adapted to Qt and SDRangel context                            //
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
#include "libldpc.h"
#include "osd.h"
#include "ft4worker.h"

namespace FT8 {

namespace {

constexpr int kFT4SymbolSamples = 576;
constexpr int kFT4TotalSymbols = 103;
constexpr int kFT4FrameSamples = kFT4TotalSymbols * kFT4SymbolSamples;

const std::array<std::array<int, 4>, 4> kFT4SyncTones = {{
    {{0, 1, 3, 2}},
    {{1, 0, 2, 3}},
    {{2, 3, 1, 0}},
    {{3, 2, 0, 1}}
}};

const std::array<int, 77> kFT4Rvec = {{
    0,1,0,0,1,0,1,0,0,1,0,1,1,1,1,0,1,0,0,0,1,0,0,1,1,0,1,1,0,
    1,0,0,1,0,1,1,0,0,0,0,1,0,0,0,1,0,1,0,0,1,1,1,1,0,0,1,0,1,
    0,1,0,1,0,1,1,0,1,1,1,1,1,0,0,0,1,0,1
}};

struct FT4Candidate
{
    int frameIndex;
    int start;
    int toneBin;
    float sync;
    float noise;
    float score;
};

}


FT4Worker::FT4Worker(
    const std::vector<float>& samples,
    float minHz,
    float maxHz,
    int start,
    int rate,
    CallbackInterface *cb,
    const FT4ParamsLight& params
) :
    m_samples(samples),
    m_minHz(minHz),
    m_maxHz(maxHz),
    m_start(start),
    m_rate(rate),
    m_cb(cb),
    m_params(params)
{}

void FT4Worker::start_work()
{
    decode();
}

float FT4Worker::binPowerInterpolated(const FFTEngine::ffts_t& bins, int symbolIndex, float toneBin)
{
    const int binCount = bins[symbolIndex].size();

    if (binCount <= 0) {
        return 0.0f;
    }

    if (toneBin <= 0.0f) {
        return std::abs(bins[symbolIndex][0]);
    }

    if (toneBin >= static_cast<float>(binCount - 1)) {
        return std::abs(bins[symbolIndex][binCount - 1]);
    }

    const int lowerBin = static_cast<int>(std::floor(toneBin));
    const int upperBin = lowerBin + 1;
    const float frac = toneBin - lowerBin;
    const float p0 = std::abs(bins[symbolIndex][lowerBin]);
    const float p1 = std::abs(bins[symbolIndex][upperBin]);
    return p0 + frac * (p1 - p0);
}

void FT4Worker::collectSyncMetrics(const FFTEngine::ffts_t& bins, float toneBin, float& sync, float& noise) const
{
    sync = 0.0f;
    noise = 0.0f;

    for (int block = 0; block < 4; block++)
    {
        const int symbolOffset = block * 33;

        for (int index = 0; index < 4; index++)
        {
            const int symbolIndex = symbolOffset + index;
            const int expectedTone = kFT4SyncTones[block][index];

            for (int tone = 0; tone < 4; tone++)
            {
                const float power = binPowerInterpolated(bins, symbolIndex, toneBin + tone);

                if (tone == expectedTone) {
                    sync += power;
                } else {
                    noise += power;
                }
            }
        }
    }
}

float FT4Worker::refineToneBin(const FFTEngine::ffts_t& bins, int toneBin) const
{
    auto scoreAt = [&](float candidateToneBin) {
        float sync = 0.0f;
        float noise = 0.0f;
        collectSyncMetrics(bins, candidateToneBin, sync, noise);
        return sync - 0.9f * (noise / 3.0f);
    };

    const float left = scoreAt(toneBin - 1.0f);
    const float center = scoreAt(toneBin);
    const float right = scoreAt(toneBin + 1.0f);
    const float denom = left - 2.0f * center + right;
    float frac = 0.0f;

    if (std::fabs(denom) > 1.0e-12f) {
        frac = 0.5f * (left - right) / denom;
    }

    if (frac > 0.5f) {
        frac = 0.5f;
    } else if (frac < -0.5f) {
        frac = -0.5f;
    }

    return toneBin + frac;
}

void FT4Worker::buildBitMetrics(const FFTEngine::ffts_t& bins, float toneBin, std::array<float, 174>& bitMetrics) const
{
    int bitIndex = 0;
    float llrAbsSum = 0.0f;

    for (int symbolIndex = 0; symbolIndex < kFT4TotalSymbols; symbolIndex++)
    {
        const bool inSync = (symbolIndex < 4)
                            || (symbolIndex >= 33 && symbolIndex < 37)
                            || (symbolIndex >= 66 && symbolIndex < 70)
                            || (symbolIndex >= 99);

        if (inSync) {
            continue;
        }

        std::array<float, 4> tonePower;

        for (int tone = 0; tone < 4; tone++) {
            tonePower[tone] = binPowerInterpolated(bins, symbolIndex, toneBin + tone);
        }

        const float epsilon = 1.0e-6f;
        const float b0Zero = std::max(tonePower[0], tonePower[1]) + epsilon;
        const float b0One = std::max(tonePower[2], tonePower[3]) + epsilon;
        const float b1Zero = std::max(tonePower[0], tonePower[3]) + epsilon;
        const float b1One = std::max(tonePower[1], tonePower[2]) + epsilon;

        const float llr0 = std::log(b0Zero / b0One);
        const float llr1 = std::log(b1Zero / b1One);

        bitMetrics[bitIndex++] = llr0;
        bitMetrics[bitIndex++] = llr1;
        llrAbsSum += std::fabs(llr0) + std::fabs(llr1);
    }

    if (bitIndex <= 0) {
        return;
    }

    const float meanAbs = llrAbsSum / bitIndex;

    if (meanAbs < 1.0e-5f) {
        return;
    }

    const float targetMean = 3.0f;
    const float globalScale = targetMean / meanAbs;

    for (int i = 0; i < bitIndex; i++) {
        float scaled = bitMetrics[i] * globalScale;

        if (scaled > 8.0f) {
            scaled = 8.0f;
        } else if (scaled < -8.0f) {
            scaled = -8.0f;
        }

        bitMetrics[i] = scaled;
    }
}

FFTEngine::ffts_t FT4Worker::makeFrameBins(int start)
{
    return m_fftEngine.ffts(m_samples, start, kFT4SymbolSamples);
}

void FT4Worker::decode()
{
    if (m_samples.size() < kFT4FrameSamples) {
        return;
    }

    const int maxStart = static_cast<int>(m_samples.size()) - kFT4FrameSamples;
    const float binSpacing = static_cast<float>(m_rate) / static_cast<float>(kFT4SymbolSamples);

    if (m_maxHz <= m_minHz) {
        return;
    }

    std::vector<int> coarseStarts;
    std::vector<int> startCandidates;

    auto addCoarseStart = [&](int start)
    {
        start = std::max(0, std::min(start, maxStart));

        if (std::find(coarseStarts.begin(), coarseStarts.end(), start) == coarseStarts.end()) {
            coarseStarts.push_back(start);
        }
    };

    for (int start = 0; start <= std::min(maxStart, 14000); start += kFT4SymbolSamples) {
        addCoarseStart(start);
    }

    const int nominalStart = maxStart / 2;
    addCoarseStart(nominalStart - 2 * kFT4SymbolSamples);
    addCoarseStart(nominalStart - kFT4SymbolSamples);
    addCoarseStart(nominalStart);
    addCoarseStart(nominalStart + kFT4SymbolSamples);
    addCoarseStart(nominalStart + 2 * kFT4SymbolSamples);
    addCoarseStart(maxStart);

    const int eighthSymbol = kFT4SymbolSamples / 8;
    const std::array<int, 9> fineOffsets = {{
        -4 * eighthSymbol,
        -3 * eighthSymbol,
        -2 * eighthSymbol,
        -eighthSymbol,
        0,
        eighthSymbol,
        2 * eighthSymbol,
        3 * eighthSymbol,
        4 * eighthSymbol
    }};

    auto addStartCandidate = [&](int start)
    {
        start = std::max(0, std::min(start, maxStart));

        if (std::find(startCandidates.begin(), startCandidates.end(), start) == startCandidates.end()) {
            startCandidates.push_back(start);
        }
    };

    for (int coarseStart : coarseStarts)
    {
        for (int offset : fineOffsets) {
            addStartCandidate(coarseStart + offset);
        }
    }

    struct FT4Frame
    {
        int start;
        FFTEngine::ffts_t bins;
    };

    std::vector<FT4Frame> frames;
    frames.reserve(startCandidates.size());

    for (int start : startCandidates)
    {
        FFTEngine::ffts_t bins = makeFrameBins(start);

        if (bins.size() < kFT4TotalSymbols) {
            continue;
        }

        if (bins[0].size() < 8) {
            continue;
        }

        frames.push_back(FT4Frame{start, std::move(bins)});
    }

    if (frames.empty()) {
        return;
    }

    const int minToneBin = std::max(0, static_cast<int>(std::ceil(m_minHz / binSpacing)));
    const int maxToneBinByHz = static_cast<int>(std::floor(m_maxHz / binSpacing)) - 3;

    if (maxToneBinByHz < minToneBin) {
        return;
    }

    std::vector<FT4Candidate> candidates;

    for (int frameIndex = 0; frameIndex < static_cast<int>(frames.size()); frameIndex++)
    {
        const FT4Frame& frame = frames[frameIndex];
        const int maxToneBin = std::min(maxToneBinByHz, static_cast<int>(frame.bins[0].size()) - 5);

        for (int toneBin = minToneBin; toneBin <= maxToneBin; toneBin++)
        {
            float sync = 0.0f;
            float noise = 0.0f;
            collectSyncMetrics(frame.bins, toneBin, sync, noise);

            if (sync < 1.8f) {
                continue;
            }

            const float score = sync - 1.0f * (noise / 3.0f);
            candidates.push_back(FT4Candidate{frameIndex, frame.start, toneBin, sync, noise, score});
        }
    }

    if (candidates.empty()) {
        return;
    }

    std::sort(candidates.begin(), candidates.end(), [](const FT4Candidate& lhs, const FT4Candidate& rhs) {
        return lhs.score > rhs.score;
    });

    const int maxDecodeCandidates = std::min(200, static_cast<int>(candidates.size()));
    const int ldpcThreshold = std::max(25, m_params.osd_ldpc_thresh - 50);

    for (int candidateIndex = 0; candidateIndex < maxDecodeCandidates; candidateIndex++)
    {
        const FT4Candidate& candidate = candidates[candidateIndex];
        const FT4Frame& frame = frames[candidate.frameIndex];
        const float refinedToneBin = refineToneBin(frame.bins, candidate.toneBin);
        std::array<float, 174> llr;
        buildBitMetrics(frame.bins, refinedToneBin, llr);

        int plain[174];
        int ldpcOk = 0;
        int correct_bits = 0;
        LDPC::ldpc_decode(llr.data(), m_params.ldpc_iters, plain, &ldpcOk);

        if (ldpcOk < ldpcThreshold)
        {
            continue;
        }

        int a174[174];

        for (int i = 0; i < 174; i++)
        {
            a174[i] = plain[i];

            if (llr.data()[i] < 0 && a174[i] == 1) {
                correct_bits += 1;
            } else if (llr.data()[i] > 0 && a174[i] == 0) {
                correct_bits += 1;
            }
        }

        bool crcOk = OSD::check_crc(a174);

        if (!crcOk && m_params.use_osd)
        {
            int oplain[91];
            int depth = -1;
            std::array<float, 174> llrCopy = llr;

            if (OSD::osd_decode(llrCopy.data(), m_params.osd_depth, oplain, &depth))
            {
                OSD::ldpc_encode(oplain, a174);
                crcOk = OSD::check_crc(a174);
            }
        }

        if (!crcOk) {
            continue;
        }

        int a91[91];

        for (int i = 0; i < 91; i++) {
            a91[i] = a174[i];
        }

        for (int i = 0; i < 77; i++) {
            a91[i] ^= kFT4Rvec[i];
        }

        const float snrLinear = (candidate.sync + 1.0e-9f) / ((candidate.noise / 3.0f) + 1.0e-9f);
        const float snr = 10.0f * std::log10(snrLinear) - 12.0f;
                    const float tone0 = refinedToneBin * binSpacing;

        if (snr < -26.0f) {
            continue;
        }

        const float off = static_cast<float>(m_start) / m_rate + static_cast<float>(candidate.start) / m_rate;
        m_cb->hcb(a91, tone0, off, "FT4-EXP", snr, 0, correct_bits);
    }
}


} // namespace FT8
