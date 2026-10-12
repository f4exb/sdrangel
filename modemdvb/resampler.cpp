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

#include "cpufeatures.h"
#include "resampler.h"
#include "resamplerkernel.h"

#if defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(_M_ARM64)
#include <arm_neon.h>
#define RESAMPLER_NEON
#elif defined(USE_SSE2) || defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#define RESAMPLER_SSE2
#endif

namespace dvbt {

const double Resampler::m_pi = 3.141592653589793;
const double Resampler::m_attenuation = 70.0;
const double Resampler::m_kaiserBeta = 0.1102 * (m_attenuation - 8.7);

double Resampler::besselI0(double x)
{
    double sum = 1.0, term = 1.0;

    for (int k = 1; k < 50; k++)
    {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;

        if (term < sum * 1e-12) {
            break;
        }
    }

    return sum;
}

// The filter for one output sample: a dot product of the input with a phase of the filter, 2 complex taps at a time
struct DotBase
{
#if defined(RESAMPLER_SSE2)
    static void dot(const float *x, const float *h, int taps, float *out)
    {
        __m128 acc = _mm_setzero_ps();

        for (int n = 0; n < 2 * taps; n += 4) {
            acc = _mm_add_ps(acc, _mm_mul_ps(_mm_loadu_ps(x + n), _mm_loadu_ps(h + n)));
        }

        // acc holds re, im of even taps and re, im of odd taps
        acc = _mm_add_ps(acc, _mm_movehl_ps(acc, acc));
        _mm_storel_pi(reinterpret_cast<__m64 *>(out), acc);
    }
#elif defined(RESAMPLER_NEON)
    static void dot(const float *x, const float *h, int taps, float *out)
    {
        float32x4_t acc = vdupq_n_f32(0.0f);

        for (int n = 0; n < 2 * taps; n += 4) {
            acc = vmlaq_f32(acc, vld1q_f32(x + n), vld1q_f32(h + n));
        }

        // acc holds re, im of even taps and re, im of odd taps
        vst1_f32(out, vadd_f32(vget_low_f32(acc), vget_high_f32(acc)));
    }
#else
    static void dot(const float *x, const float *h, int taps, float *out)
    {
        float re = 0.0f, im = 0.0f;

        for (int n = 0; n < 2 * taps; n += 2)
        {
            re += x[n] * h[n];
            im += x[n + 1] * h[n + 1];
        }

        out[0] = re;
        out[1] = im;
    }
#endif
};

Resampler::Resampler() :
    m_taps(0),
    m_avx2(modemdvb::cpuHasAVX2()),
    m_step(1.0),
    m_nominalStep(1.0),
    m_correction(1.0),
    m_time(0.0)
{
}

void Resampler::configure(double inputRate, double outputRate, double passband)
{
    // Content above the stopband would alias onto the passband, at whichever rate is lower
    double stopband = std::min(inputRate, outputRate) - passband;
    double cutoff = (passband + stopband) / 2.0 / inputRate;        // Cycles per input sample
    double transition = std::max(0.02, (stopband - passband) / inputRate);
    int length = (int) std::ceil((m_attenuation - 7.95) / (14.36 * transition)); // Kaiser estimate, in input samples

    m_taps = std::max(4, std::min(64, length + 1));
    m_taps = (m_taps + 3) & ~3; // A multiple of 4, for SIMD
    m_filter.assign(m_phases * m_taps, 0.0f);
    // Phase p delays by p / m_phases of an input sample. Tap n multiplies input sample i0 - m_taps/2 + 1 + n
    for (int p = 0; p < m_phases; p++)
    {
        double frac = (double) p / m_phases;
        double sum = 0.0;

        for (int n = 0; n < m_taps; n++)
        {
            double x = (n - m_taps / 2 + 1) - frac; // Distance from the output time, in input samples
            double arg = 2.0 * x / m_taps;
            double window = std::abs(arg) < 1.0 ? besselI0(m_kaiserBeta * std::sqrt(1.0 - arg * arg)) / besselI0(m_kaiserBeta) : 0.0;
            double y = 2.0 * m_pi * cutoff * x;
            double sinc = std::abs(y) < 1e-12 ? 1.0 : std::sin(y) / y;
            double h = 2.0 * cutoff * sinc * window;
            m_filter[p * m_taps + n] = (float) h;
            sum += h;
        }

        for (int n = 0; n < m_taps; n++) {
            m_filter[p * m_taps + n] /= (float) sum; // Unity gain at DC for every phase
        }
    }

    m_filterPairs.resize(2 * m_filter.size());

    for (size_t i = 0; i < m_filter.size(); i++)
    {
        m_filterPairs[2 * i] = m_filter[i];
        m_filterPairs[2 * i + 1] = m_filter[i];
    }

    m_nominalStep = inputRate / outputRate;
    m_step = m_nominalStep * m_correction;
    reset();
}

void Resampler::setRatioCorrection(double correction)
{
    // Clock offsets are a few ppm, so anything far beyond is an error, which mustn't stop the resampler working
    if (!std::isfinite(correction)) {
        correction = 1.0;
    }

    m_correction = std::max(0.99, std::min(1.01, correction));
    m_step = m_nominalStep * m_correction;
}

void Resampler::reset()
{
    m_history.assign(m_taps, Cf(0.0f, 0.0f));
    m_time = m_taps / 2 - 1;
}

void Resampler::prime(const std::vector<Cf>& previous)
{
    reset();
    int n = std::min((int) previous.size(), m_taps);
    std::copy(previous.end() - n, previous.end(), m_history.end() - n);
    m_time = m_taps; // The first new sample
}

void Resampler::process(const Cf *in, int nbIn, std::vector<Cf>& out)
{
    const size_t n = out.size();
    const int maxOut = maxOutputs(nbIn);
    out.resize(n + maxOut);
    const int made = process(in, nbIn, out.data() + n, maxOut);
    out.resize(n + made);
}

int Resampler::maxOutputs(int nbIn) const
{
    return resampleMaxOutputs((int) m_history.size() + nbIn, m_time, m_step);
}

void makePolyphaseFilter(int taps, int phases, const std::function<double(double)>& impulse, std::vector<float>& pairs)
{
    pairs.assign(2 * (size_t) phases * taps, 0.0f);
    std::vector<double> h(taps);

    // As configure(): phase p delays by p / phases of an input sample, tap n multiplies input sample i0 - taps/2 + 1 + n
    for (int p = 0; p < phases; p++)
    {
        const double frac = (double) p / phases;
        double sum = 0.0;

        for (int n = 0; n < taps; n++)
        {
            h[n] = impulse((n - taps / 2 + 1) - frac);
            sum += h[n];
        }

        for (int n = 0; n < taps; n++)
        {
            const float v = (float) (sum != 0.0 ? h[n] / sum : h[n]);
            pairs[2 * ((size_t) p * taps + n)] = v;
            pairs[2 * ((size_t) p * taps + n) + 1] = v;
        }
    }
}

int resampleFilter(const std::complex<float> *samples, int offset, int available, const float *pairs, int taps,
    int phases, double& time, double step, std::complex<float> *out, int maxOut)
{
#if defined(MODEMDVB_X86)
    static const bool avx2 = modemdvb::cpuHasAVX2();

    if (avx2) {
        return resampleAVX2(samples, offset, available, pairs, taps, phases, time, step, out, maxOut);
    }
#endif

    return resampleRun<DotBase>(samples, offset, available, pairs, taps, phases, time, step, out, maxOut);
}

// Input sample j (from m_history[0], which m_time is from) at samples[j - offset], to available
int Resampler::run(const Cf *samples, int offset, int available, Cf *out, int maxOut)
{
#if defined(MODEMDVB_X86)
    if (m_avx2) {
        return resampleAVX2(samples, offset, available, m_filterPairs.data(), m_taps, m_phases, m_time, m_step, out, maxOut);
    }
#endif

    return resampleRun<DotBase>(samples, offset, available, m_filterPairs.data(), m_taps, m_phases, m_time, m_step, out, maxOut);
}

// The input follows the history. The outputs whose taps include the history are made from a copy of it with the start
// of the input, and the rest from the input itself, so it isn't copied. Either way, m_time is from m_history[0], so
// the outputs are the same
int Resampler::process(const Cf *in, int nbIn, Cf *out, int maxOut)
{
    const int h = (int) m_history.size();
    const int half = m_taps / 2 - 1;
    const int available = h + nbIn;
    // Those that stop for want of the rest of the head have their first tap past the history
    const int head = std::min(nbIn, m_taps);
    m_staging.assign(m_history.begin(), m_history.end());
    m_staging.insert(m_staging.end(), in, in + head);
    int made = run(m_staging.data(), 0, h + head, out, maxOut);

    if (head < nbIn) {
        made += run(in, h, available, out + made, maxOut - made);
    }

    // Keep what is still needed
    int consumed = std::max(0, (int) m_time - half);
    consumed = std::min(consumed, available);

    if (consumed >= h)
    {
        m_history.assign(in + (consumed - h), in + nbIn);
    }
    else
    {
        m_history.erase(m_history.begin(), m_history.begin() + consumed);
        m_history.insert(m_history.end(), in, in + nbIn);
    }

    m_time -= consumed;
    return made;
}

} // namespace dvbt
