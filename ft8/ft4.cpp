///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026                                                           //
//                                                                               //
// Experimental FT4 decoder scaffold derived from FT8 decoder architecture.      //
///////////////////////////////////////////////////////////////////////////////////

#include <array>
#include <cmath>
#include <algorithm>

#include <QThread>

#include "ft4.h"
#include "libldpc.h"
#include "osd.h"

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
    int start;
    float tone0;
    float sync;
    float noise;
    float score;
};

class FT4Worker : public QObject
{
public:
    FT4Worker(
        const std::vector<float>& samples,
        float minHz,
        float maxHz,
        int start,
        int rate,
        CallbackInterface *cb,
        const FT4Params& params
    ) :
        m_samples(samples),
        m_minHz(minHz),
        m_maxHz(maxHz),
        m_start(start),
        m_rate(rate),
        m_cb(cb),
        m_params(params)
    {}

    void start_work()
    {
        decode();
    }

private:
    float goertzelPower(int sampleStart, int sampleCount, float frequency) const
    {
        const float omega = 2.0f * static_cast<float>(M_PI) * frequency / m_rate;
        const float coeff = 2.0f * std::cos(omega);
        float q0 = 0.0f;
        float q1 = 0.0f;
        float q2 = 0.0f;

        for (int i = 0; i < sampleCount; i++)
        {
            q0 = coeff * q1 - q2 + m_samples[sampleStart + i];
            q2 = q1;
            q1 = q0;
        }

        return q1 * q1 + q2 * q2 - coeff * q1 * q2;
    }

    float symbolTonePower(int start, int symbolIndex, float tone0, int tone) const
    {
        const int symbolStart = start + symbolIndex * kFT4SymbolSamples;
        const float toneSpacing = static_cast<float>(m_rate) / static_cast<float>(kFT4SymbolSamples);
        const float frequency = tone0 + tone * toneSpacing;
        return goertzelPower(symbolStart, kFT4SymbolSamples, frequency);
    }

    void collectSyncMetrics(int start, float tone0, float& sync, float& noise) const
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
                    const float power = symbolTonePower(start, symbolIndex, tone0, tone);

                    if (tone == expectedTone) {
                        sync += power;
                    } else {
                        noise += power;
                    }
                }
            }
        }
    }

    void buildBitMetrics(int start, float tone0, std::array<float, 174>& bitMetrics) const
    {
        int bitIndex = 0;

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
                tonePower[tone] = symbolTonePower(start, symbolIndex, tone0, tone);
            }

            const float b0Zero = std::max(tonePower[0], tonePower[1]);
            const float b0One = std::max(tonePower[2], tonePower[3]);
            const float b1Zero = std::max(tonePower[0], tonePower[3]);
            const float b1One = std::max(tonePower[1], tonePower[2]);

            bitMetrics[bitIndex++] = b0Zero - b0One;
            bitMetrics[bitIndex++] = b1Zero - b1One;
        }
    }

    void decode()
    {
        if (m_samples.size() < kFT4FrameSamples) {
            return;
        }

        const int maxStart = static_cast<int>(m_samples.size()) - kFT4FrameSamples;
        const float toneSpacing = static_cast<float>(m_rate) / static_cast<float>(kFT4SymbolSamples);
        const float maxTone0 = m_maxHz - 3.0f * toneSpacing;

        if (maxTone0 <= m_minHz) {
            return;
        }

        std::vector<int> startCandidates;

        auto addStartCandidate = [&](int start)
        {
            start = std::max(0, std::min(start, maxStart));

            if (std::find(startCandidates.begin(), startCandidates.end(), start) == startCandidates.end()) {
                startCandidates.push_back(start);
            }
        };

        for (int start = 0; start <= std::min(maxStart, 14000); start += kFT4SymbolSamples) {
            addStartCandidate(start);
        }

        const int nominalStart = maxStart / 2;
        addStartCandidate(std::max(0, nominalStart - 2 * kFT4SymbolSamples));
        addStartCandidate(std::max(0, nominalStart - kFT4SymbolSamples));
        addStartCandidate(nominalStart);
        addStartCandidate(std::min(maxStart, nominalStart + kFT4SymbolSamples));
        addStartCandidate(std::min(maxStart, nominalStart + 2 * kFT4SymbolSamples));
        addStartCandidate(maxStart);

        std::vector<FT4Candidate> candidates;

        for (int start : startCandidates)
        {
            for (float tone0 = m_minHz; tone0 <= maxTone0; tone0 += toneSpacing)
            {
                float sync = 0.0f;
                float noise = 0.0f;
                collectSyncMetrics(start, tone0, sync, noise);
                const float score = sync - 1.25f * (noise / 3.0f);
                candidates.push_back(FT4Candidate{start, tone0, sync, noise, score});
            }
        }

        if (candidates.empty()) {
            return;
        }

        std::sort(candidates.begin(), candidates.end(), [](const FT4Candidate& lhs, const FT4Candidate& rhs) {
            return lhs.score > rhs.score;
        });

        const int maxDecodeCandidates = std::min(m_params.max_candidates, static_cast<int>(candidates.size()));
        const int ldpcThreshold = std::max(48, m_params.osd_ldpc_thresh - 18);

        for (int candidateIndex = 0; candidateIndex < maxDecodeCandidates; candidateIndex++)
        {
            const FT4Candidate& candidate = candidates[candidateIndex];
            std::array<float, 174> llr;
            buildBitMetrics(candidate.start, candidate.tone0, llr);

            int plain[174];
            int ldpcOk = 0;
            LDPC::ldpc_decode(llr.data(), m_params.ldpc_iters, plain, &ldpcOk);

            if (ldpcOk < ldpcThreshold) {
                continue;
            }

            int a174[174];

            for (int i = 0; i < 174; i++) {
                a174[i] = plain[i];
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
            const float off = static_cast<float>(m_start) / m_rate + static_cast<float>(candidate.start) / m_rate;
            m_cb->hcb(a91, candidate.tone0, off, "FT4-EXP", snr, 0, ldpcOk);
        }
    }

    std::vector<float> m_samples;
    float m_minHz;
    float m_maxHz;
    int m_start;
    int m_rate;
    CallbackInterface *m_cb;
    FT4Params m_params;
};

} // namespace

FT4Decoder::~FT4Decoder()
{
    forceQuit();
}

void FT4Decoder::entry(
    float xsamples[],
    int nsamples,
    int start,
    int rate,
    float min_hz,
    float max_hz,
    int hints1[],
    int hints2[],
    double time_left,
    double total_time_left,
    CallbackInterface *cb,
    int nprevdecs,
    struct cdecode *xprevdecs
)
{
    (void) hints1;
    (void) hints2;
    (void) time_left;
    (void) total_time_left;
    (void) nprevdecs;
    (void) xprevdecs;

    std::vector<float> samples(nsamples);

    for (int i = 0; i < nsamples; i++) {
        samples[i] = xsamples[i];
    }

    if (min_hz < 0) {
        min_hz = 0;
    }

    if (max_hz > rate / 2) {
        max_hz = rate / 2;
    }

    const float per = (max_hz - min_hz) / params.nthreads;

    for (int i = 0; i < params.nthreads; i++)
    {
        float hz0 = min_hz + i * per;
        float hz1 = min_hz + (i + 1) * per;
        hz0 = std::max(hz0, 0.0f);
        hz1 = std::min(hz1, (rate / 2.0f) - 50.0f);

        FT4Worker *ft4 = new FT4Worker(samples, hz0, hz1, start, rate, cb, params);
        QThread *th = new QThread();
        th->setObjectName(QString("ft4:%1:%2").arg(cb->get_name()).arg(i));
        threads.push_back(th);
        ft4->moveToThread(th);
        QObject::connect(th, &QThread::started, ft4, [ft4, th]() {
            ft4->start_work();
            th->quit();
        });
        QObject::connect(th, &QThread::finished, ft4, &QObject::deleteLater);
        QObject::connect(th, &QThread::finished, th, &QObject::deleteLater);
        th->start();
    }
}

void FT4Decoder::wait(double time_left)
{
    unsigned long thread_timeout = time_left * 1000;

    while (!threads.empty())
    {
        bool success = threads.front()->wait(thread_timeout);

        if (!success)
        {
            qDebug("FT8::FT4Decoder::wait: thread timed out");
            thread_timeout = 50;
        }

        threads.erase(threads.begin());
    }
}

void FT4Decoder::forceQuit()
{
    while (!threads.empty())
    {
        threads.front()->quit();
        threads.front()->wait();
        threads.erase(threads.begin());
    }
}

} // namespace FT8
