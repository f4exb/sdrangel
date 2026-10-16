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
#include <array>
#include <cmath>

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && (_M_IX86_FP >= 2))
#define MODEMDAB_COHERENT_SSE2 1
#include <emmintrin.h>
#endif

#include "dabcoherent.h"
#include "dabprofile.h"

namespace modemdab
{

namespace
{

constexpr int MSCBits = ModeI::MSCSymbols * ModeI::BitsPerSymbol;
constexpr int FICBits = ModeI::FICSymbols * ModeI::BitsPerSymbol;
constexpr std::size_t MaximumFrames = 8;
// Candidate memories of the references: from fast fading to a static channel
const float feedbacks[7] = {0.0f, 0.25f, 0.5f, 0.75f, 0.9f, 0.97f, 0.99f};
// Carrier spacing of the slope estimate: long enough to average, short enough not to wrap
constexpr int SlopeSpan = 128;

// Four floats, one per carrier: the recursions run on four carriers at once
#ifdef MODEMDAB_COHERENT_SSE2
struct F4
{
    __m128 v;
    F4() : v(_mm_setzero_ps()) {}
    F4(__m128 x) : v(x) {}
    explicit F4(float x) : v(_mm_set1_ps(x)) {}
    static F4 load(const float *p) { return F4(_mm_loadu_ps(p)); }
    void store(float *p) const { _mm_storeu_ps(p, v); }
};
inline F4 operator+(F4 a, F4 b) { return F4(_mm_add_ps(a.v, b.v)); }
inline F4 operator-(F4 a, F4 b) { return F4(_mm_sub_ps(a.v, b.v)); }
inline F4 operator*(F4 a, F4 b) { return F4(_mm_mul_ps(a.v, b.v)); }
inline F4 operator/(F4 a, F4 b) { return F4(_mm_div_ps(a.v, b.v)); }
// 1 where a > b, else 0
inline F4 greater(F4 a, F4 b) { return F4(_mm_and_ps(_mm_cmpgt_ps(a.v, b.v), _mm_set1_ps(1.0f))); }
#else
struct F4
{
    float v[4];
    F4() : v{0.0f, 0.0f, 0.0f, 0.0f} {}
    explicit F4(float x) : v{x, x, x, x} {}
    static F4 load(const float *p) { F4 f; for (int i = 0; i < 4; i++) { f.v[i] = p[i]; } return f; }
    void store(float *p) const { for (int i = 0; i < 4; i++) { p[i] = v[i]; } }
};
inline F4 operator+(F4 a, F4 b) { for (int i = 0; i < 4; i++) { a.v[i] += b.v[i]; } return a; }
inline F4 operator-(F4 a, F4 b) { for (int i = 0; i < 4; i++) { a.v[i] -= b.v[i]; } return a; }
inline F4 operator*(F4 a, F4 b) { for (int i = 0; i < 4; i++) { a.v[i] *= b.v[i]; } return a; }
inline F4 operator/(F4 a, F4 b) { for (int i = 0; i < 4; i++) { a.v[i] /= b.v[i]; } return a; }
inline F4 greater(F4 a, F4 b) { for (int i = 0; i < 4; i++) { a.v[i] = a.v[i] > b.v[i] ? 1.0f : 0.0f; } return a; }
#endif

// Complex values of four carriers
struct C4
{
    F4 re;
    F4 im;
};
inline C4 operator+(const C4& a, const C4& b) { return {a.re + b.re, a.im + b.im}; }
inline C4 operator*(const C4& a, const C4& b) { return {a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re}; }
inline C4 operator*(F4 a, const C4& b) { return {a * b.re, a * b.im}; }
inline C4 conjugate(const C4& a) { return {a.re, F4(0.0f) - a.im}; }

// Four consecutive complex values, split into real and imaginary parts, and back
inline C4 loadComplex(const std::complex<float> *p)
{
    const float *f = reinterpret_cast<const float *>(p);
#ifdef MODEMDAB_COHERENT_SSE2
    const __m128 lo = _mm_loadu_ps(f);
    const __m128 hi = _mm_loadu_ps(f + 4);
    return {F4(_mm_shuffle_ps(lo, hi, _MM_SHUFFLE(2, 0, 2, 0))), F4(_mm_shuffle_ps(lo, hi, _MM_SHUFFLE(3, 1, 3, 1)))};
#else
    C4 c;
    for (int i = 0; i < 4; i++)
    {
        c.re.v[i] = f[2 * i];
        c.im.v[i] = f[2 * i + 1];
    }
    return c;
#endif
}

inline void storeComplex(const C4& c, std::complex<float> *p)
{
    float *f = reinterpret_cast<float *>(p);
#ifdef MODEMDAB_COHERENT_SSE2
    _mm_storeu_ps(f, _mm_unpacklo_ps(c.re.v, c.im.v));
    _mm_storeu_ps(f + 4, _mm_unpackhi_ps(c.re.v, c.im.v));
#else
    for (int i = 0; i < 4; i++)
    {
        f[2 * i] = c.re.v[i];
        f[2 * i + 1] = c.im.v[i];
    }
#endif
}

inline int carrierIndex(int k)
{
    return k < 0 ? k + 768 : k + 767;
}

} // anonymous namespace

DABCoherentDecoder::DABCoherentDecoder() :
    m_cif(0),
    m_lastCIFCount(-1),
    m_decodedUpTo(-1000000),
    m_knownFraction(0.0f),
    m_feedback(0.9f),
    m_reliabilityThreshold(DefaultReliabilityThreshold),
    m_drift(0.0f),
    m_driftSlope(0.0f)
{
    m_softCarrier.resize(ModeI::Carriers);
    for (int i = 0; i < ModeI::Carriers; i++) {
        m_softCarrier[i] = carrierIndex(DABTables::frequencyInterleaving()[i]);
    }
    m_known.resize(FICBits + MSCBits);
}

void DABCoherentDecoder::setReliabilityThreshold(float threshold)
{
    m_reliabilityThreshold = threshold;
    for (auto& s : m_subchannels) {
        s->decoder.setReliabilityLimit(threshold);
    }
}

void DABCoherentDecoder::reset()
{
    m_frames.clear();
    for (auto& s : m_subchannels)
    {
        s->decoder.configure(s->config);
        s->codewords.clear();
    }
    m_cif = 0;
    m_lastCIFCount = -1;
    m_decodedUpTo = -1000000;
}

void DABCoherentDecoder::setSubchannels(const std::vector<DABSubchannel>& subchannels)
{
    bool same = subchannels.size() == m_subchannels.size();
    for (std::size_t i = 0; same && (i < subchannels.size()); i++) {
        same = subchannels[i] == m_subchannels[i]->config;
    }
    if (same) {
        return;
    }

    m_subchannels.clear();
    for (const auto& config : subchannels)
    {
        std::unique_ptr<Subchannel> s(new Subchannel());
        s->config = config;
        s->profile = config.profile();
        s->decoder.configure(config);
        s->decoder.setReliabilityLimit(m_reliabilityThreshold);
        s->decoder.setCountErrors(false);
        if (!s->profile.parts.empty()) {
            m_subchannels.push_back(std::move(s));
        }
    }
    reset();
}

void DABCoherentDecoder::push(const DABFrame& frame, int cifCount, const std::array<std::vector<uint8_t>, 4>& ficCoded,
    const std::array<bool, 4>& ficValid)
{
    if ((frame.carriers.size() != static_cast<std::size_t>(ModeI::SymbolsPerFrame * ModeI::Carriers))
        || (frame.mscSoft.size() != static_cast<std::size_t>(MSCBits))) {
        return;
    }

    // Frames must follow each other: after a gap, start again
    if ((m_lastCIFCount >= 0) && (((cifCount - m_lastCIFCount + ModeI::CIFCountModulus) % ModeI::CIFCountModulus) != ModeI::CIFsPerFrame)) {
        reset();
    }
    m_lastCIFCount = cifCount;

    Frame f;
    f.cif = m_cif;
    f.cifCount = cifCount;
    f.carriers = frame.carriers;
    f.weights = frame.weights;
    f.mscSoft = frame.mscSoft;
    f.fic = ficCoded;
    f.ficValid = ficValid;
    m_frames.push_back(std::move(f));
    while (m_frames.size() > MaximumFrames) {
        m_frames.pop_front();
    }

    // First pass: decode every sub-channel and re-encode what it gives (the decoders count
    // their own time)
    {
        std::vector<uint8_t> logicalFrame;

        for (int i = 0; i < ModeI::CIFsPerFrame; i++)
        {
            const int64_t cif = m_cif + i;
            bool allDecoded = true;

            for (auto& s : m_subchannels)
            {
                if (s->decoder.push(&frame.mscSoft[i * ModeI::CIFBits], logicalFrame))
                {
                    DABProfileScope profile(DABProfile::MSC);
                    const int64_t logical = cif - 15;
                    DABConvolutional::encodeKnown(s->decoder.lastBits(), s->decoder.lastReliable(), s->profile, s->codewords[logical]);
                    while (!s->codewords.empty() && (s->codewords.begin()->first < logical - 24)) {
                        s->codewords.erase(s->codewords.begin());
                    }
                }
                else
                {
                    allDecoded = false;
                }
            }
            if (allDecoded) {
                m_decodedUpTo = cif - 15;
            }
        }
    }
    m_cif += ModeI::CIFsPerFrame;
}

bool DABCoherentDecoder::pop(Output& output)
{
    while (!m_frames.empty())
    {
        Frame& front = m_frames.front();
        // Too old to be completed (decoding started after it)
        if (m_subchannels.empty() || (front.cif + ModeI::CIFsPerFrame - 1 <= m_decodedUpTo))
        {
            demodulate(front, output);
            m_frames.pop_front();
            return true;
        }
        return false;
    }
    return false;
}

void DABCoherentDecoder::demodulate(Frame& frame, Output& output)
{
    DABProfileScope profile(DABProfile::Demodulation);
    const int symbols = ModeI::SymbolsPerFrame;
    const int carriers = ModeI::Carriers;

    // The coded bits of the frame, as transmitted: the FIC, then four CIFs
    std::fill(m_known.begin(), m_known.end(), static_cast<int8_t>(-1));
    for (int b = 0; b < ModeI::CIFsPerFrame; b++)
    {
        if (frame.ficValid[b] && (frame.fic[b].size() == static_cast<std::size_t>(ModeI::FICBlockBits)))
        {
            for (int i = 0; i < ModeI::FICBlockBits; i++) {
                m_known[b * ModeI::FICBlockBits + i] = static_cast<int8_t>(frame.fic[b][i]);
            }
        }
    }

    // Capacity units no sub-channel uses carry nothing defined, so stay unknown
    for (int i = 0; i < ModeI::CIFsPerFrame; i++)
    {
        int8_t *cifBits = &m_known[FICBits + i * ModeI::CIFBits];
        const int64_t cif = frame.cif + i;

        for (const auto& s : m_subchannels)
        {
            // Bit j of this CIF is bit j of the codeword sent timeInterleavingDelay(j) CIFs ago
            const std::vector<int8_t> *codewords[16];
            for (int d = 0; d < 16; d++)
            {
                auto it = s->codewords.find(cif - d);
                codewords[d] = it != s->codewords.end() ? &it->second : nullptr;
            }
            const int start = s->config.startCU * ModeI::CUBits;
            const int size = s->config.sizeCU * ModeI::CUBits;
            for (int j = 0; j < size; j++)
            {
                const std::vector<int8_t> *codeword = codewords[DABTables::timeInterleavingDelay(j)];
                cifBits[start + j] = (codeword && (j < static_cast<int>(codeword->size()))) ? (*codeword)[j] : static_cast<int8_t>(-1);
            }
        }
    }

    // The known QPSK symbols y(l) (l = 2..76), zero where unknown, and k = 1 where known. All
    // arrays are as the frame's carriers: index (l - 1) * 1536 + carrier.
    const float a = 1.0f / std::sqrt(2.0f);
    const std::size_t size = static_cast<std::size_t>(symbols) * carriers;
    m_y.assign(size, 0.0f);
    m_k.assign(size, 0.0f);
    m_decision.resize(size);
    m_gain.resize(size);
    int knownCount = 0;
    for (int l = 2; l <= symbols; l++)
    {
        const int8_t *bits = &m_known[(l - 2) * ModeI::BitsPerSymbol];
        std::complex<float> *y = &m_y[(l - 1) * carriers];
        float *k = &m_k[(l - 1) * carriers];
        for (int n = 0; n < carriers; n++)
        {
            const int8_t p0 = bits[n];
            const int8_t p1 = bits[n + carriers];
            if ((p0 >= 0) && (p1 >= 0))
            {
                const int c = m_softCarrier[n];
                y[c] = std::complex<float>(p0 ? -a : a, p1 ? -a : a);
                k[c] = 1.0f;
                knownCount++;
            }
        }
    }
    m_knownFraction = static_cast<float>(knownCount) / ((symbols - 1) * carriers);

    const std::complex<float> *r = frame.carriers.data();
    const std::complex<float> *y = m_y.data();
    const float *known = m_k.data();

    // The phase drift from one symbol to the next: a residual frequency offset turns every
    // carrier, a sampling clock offset turns them in proportion to their frequency. Either
    // biases a reference with a long memory, so it is measured from the known symbols,
    // r(l) conj(r(l-1) y(l)), and taken out of the references.
    m_drifts.resize(carriers);
    {
        std::vector<std::complex<float>> d(carriers, 0.0f);
        for (int i = 1; i < symbols; i++)
        {
            const std::complex<float> *ri = &r[i * carriers];
            const std::complex<float> *yi = &y[i * carriers];
            for (int c = 0; c < carriers; c++) {
                d[c] += ri[c] * std::conj(ri[c - carriers] * yi[c]);
            }
        }
        std::complex<double> common = 0.0;
        for (int c = 0; c < carriers; c++) {
            common += std::complex<double>(d[c]);
        }
        // The ramp from the change over SlopeSpan carriers (on one side of DC)
        std::complex<double> step = 0.0;
        for (int c = SlopeSpan; c < carriers; c++)
        {
            if ((c < carriers / 2) || (c - SlopeSpan >= carriers / 2)) {
                step += std::complex<double>(d[c] * std::conj(d[c - SlopeSpan]));
            }
        }
        const double theta0 = std::arg(common);
        const double theta1 = std::abs(step) > 0.0 ? std::arg(step) / SlopeSpan : 0.0;
        for (int c = 0; c < carriers; c++)
        {
            const int kc = c < carriers / 2 ? c - carriers / 2 : c - carriers / 2 + 1;
            m_drifts[c] = std::polar(1.0f, static_cast<float>(theta0 + theta1 * kc));
        }
        m_drift = static_cast<float>(theta0);
        m_driftSlope = static_cast<float>(theta1);
    }
    const std::complex<float> *drift = m_drifts.data();

    // Choose the memory of the references: the one whose prediction of each known symbol
    // from the symbols before (normalized by the sum of its weights) has the least mean square
    // error, weighted by the noise weights of the carriers (on every eighth carrier)
    {
        double bestError = -1.0;
        for (float alpha : feedbacks)
        {
            double error = 0.0;
            for (int c = 0; c < carriers; c += 8)
            {
                std::complex<float> P = r[c];
                float A = 1.0f;
                float carrierError = 0.0f;
                for (int i = 1; i < symbols; i++)
                {
                    const int index = i * carriers + c;
                    if (known[index] != 0.0f)
                    {
                        const std::complex<float> prediction = P * y[index] * drift[c];
                        carrierError += std::norm(r[index] - prediction * (1.0f / A));
                        P = r[index] + alpha * prediction;
                        A = 1.0f + alpha * A;
                    }
                    else
                    {
                        P = r[index];
                        A = 1.0f;
                    }
                }
                error += carrierError * frame.weights[c];
            }
            if ((bestError < 0.0) || (error < bestError))
            {
                bestError = error;
                m_feedback = alpha;
            }
        }
    }
    const float alpha = m_feedback;
    const float alpha2 = alpha * alpha;

    // For each carrier: the reference from before, P(l) ~ h z(l-1), the one from after,
    // F(l) ~ h z(l), and the decision variable D = (r(l) + alpha F(l)) conj(P(l)). The
    // weights of the received symbols in a reference are 1, alpha, alpha^2... and the soft
    // value is scaled by the signal to noise ratio of D:
    //   A = sum of the weights, E = sum of their squares,
    //   gain = 2 As Ap / (As^2 Ep + Ap^2 Es), which is 1 for differential detection.
    // D's signal to noise ratio is proportional to q = (As Ap)^2 / (As^2 Ep + Ap^2 Es): 1/2
    // for differential detection, and more as more neighbours are known. Where q is no
    // better than differential detection, the pass before's soft bits (three-symbol) are
    // kept (gain 0 marks them). In weak signals, where few decoded bits are reliable, that
    // keeps coherent detection from being worse than three-symbol.
    // An unknown symbol has y = 0 and k = 0, which restarts the references. Carriers are
    // taken four at a time; the recursions' arrays are by symbol index i = l - 1.
    const F4 a1(alpha);
    const F4 a2(alpha2);
    const F4 one(1.0f);
    const F4 two(2.0f);
    const F4 differentialQuality(0.55f);
    std::array<C4, ModeI::SymbolsPerFrame> forward, backward;
    std::array<F4, ModeI::SymbolsPerFrame> forwardA, forwardE, backwardA, backwardE;

    for (int c0 = 0; c0 < carriers; c0 += 4)
    {
        const C4 w = loadComplex(&drift[c0]);
        const C4 wConj = conjugate(w);
        auto R = [&](int i) { return loadComplex(&r[i * carriers + c0]); };
        auto Y = [&](int i) { return loadComplex(&y[i * carriers + c0]); };
        auto K = [&](int i) { return F4::load(&known[i * carriers + c0]); };

        // forward[i] estimates h z(i-1), for i = 1..75
        forward[1] = R(0);
        forwardA[1] = one;
        forwardE[1] = one;
        for (int i = 1; i < symbols - 1; i++)
        {
            const F4 ki = K(i);
            forward[i + 1] = R(i) + a1 * (forward[i] * Y(i) * w);
            forwardA[i + 1] = one + a1 * ki * forwardA[i];
            forwardE[i + 1] = one + a2 * ki * forwardE[i];
        }

        // backward[i] estimates h z(i) from the symbols after i
        backward[symbols - 1] = C4{};
        backwardA[symbols - 1] = F4(0.0f);
        backwardE[symbols - 1] = F4(0.0f);
        for (int i = symbols - 2; i >= 1; i--)
        {
            const F4 ki = K(i + 1);
            backward[i] = conjugate(Y(i + 1)) * wConj * (R(i + 1) + a1 * backward[i + 1]);
            backwardA[i] = ki * (one + a1 * backwardA[i + 1]);
            backwardE[i] = ki * (one + a2 * backwardE[i + 1]);
        }

        // Only the MSC symbols are wanted
        for (int i = 1 + ModeI::FICSymbols; i < symbols; i++)
        {
            const C4 S = R(i) + a1 * backward[i];
            const F4 As = one + a1 * backwardA[i];
            const F4 Es = one + a2 * backwardE[i];
            const F4 Ap = forwardA[i];
            const F4 Ep = forwardE[i];
            storeComplex(S * conjugate(forward[i] * w), &m_decision[i * carriers + c0]);
            const F4 AsAp = As * Ap;
            const F4 denominator = As * As * Ep + Ap * Ap * Es;
            const F4 better = greater(AsAp * AsAp / denominator, differentialQuality);
            (better * two * AsAp / denominator).store(&m_gain[i * carriers + c0]);
        }
    }

    // The output: this pass's soft bits, with the frame for another pass
    output.cifCount = frame.cifCount;
    output.ficCoded = frame.fic;
    output.ficValid = frame.ficValid;
    output.frame.carriers = std::move(frame.carriers);
    output.frame.weights = frame.weights;
    std::vector<float>& mscSoft = output.frame.mscSoft;
    mscSoft.resize(MSCBits);

    // Soft bits of the MSC symbols, before frequency interleaving, with the noise weights
    double sumSoft = 0.0;
    for (int l = 1 + ModeI::FICSymbols + 1; l <= symbols; l++)
    {
        float *out = &mscSoft[(l - 2 - ModeI::FICSymbols) * ModeI::BitsPerSymbol];
        const float *before = &frame.mscSoft[(l - 2 - ModeI::FICSymbols) * ModeI::BitsPerSymbol];
        for (int n = 0; n < carriers; n++)
        {
            const int c = m_softCarrier[n];
            const int index = (l - 1) * carriers + c;
            const float g = m_gain[index] * frame.weights[c];
            if (g > 0.0f)
            {
                out[n] = m_decision[index].real() * g;
                out[n + carriers] = m_decision[index].imag() * g;
            }
            else
            {
                out[n] = before[n];
                out[n + carriers] = before[n + carriers];
            }
            sumSoft += std::abs(out[n]) + std::abs(out[n + carriers]);
        }
    }
    const float limit = static_cast<float>(8.0 * sumSoft / MSCBits);
    for (float& v : mscSoft) {
        v = std::max(-limit, std::min(limit, v));
    }
}

} // namespace modemdab
