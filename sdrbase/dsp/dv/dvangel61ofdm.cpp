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

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <algorithm>
#include <cmath>

#include "dvangel61ofdm.h"

typedef std::complex<float> cf;
typedef std::complex<double> cd;

static const double TwoPi = 2.0 * M_PI;

float DVAngel61OFDM::outputGain(DVAngel61::Constellation c)
{
    return std::pow(10.0f, (-1.0f - referencePaprDB(c)) / 20.0f) / std::sqrt(referenceMeanPower(c));
}

namespace {

//! An FFT size's bit reversal and twiddles (exp(-2 pi i k / n), k < n / 2), worked out once
struct FftPlan
{
    std::vector<int> m_reverse;
    std::vector<double> m_cos, m_sin;
};

const FftPlan& fftPlan(int n)
{
    // the modulator's sizes are 512 and 1024; each thread keeps its own (no lock on the DSP path)
    thread_local std::vector<std::pair<int, FftPlan>> plans;

    for (const auto& p : plans) {
        if (p.first == n) {
            return p.second;
        }
    }

    FftPlan plan;
    plan.m_reverse.resize(n);

    for (int i = 0, j = 0; i < n; i++)
    {
        plan.m_reverse[i] = j;
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
    }

    plan.m_cos.resize(n / 2);
    plan.m_sin.resize(n / 2);

    for (int k = 0; k < n / 2; k++)
    {
        plan.m_cos[k] = std::cos(TwoPi * k / n);
        plan.m_sin[k] = -std::sin(TwoPi * k / n);
    }

    plans.emplace_back(n, std::move(plan));
    return plans.back().second;
}

}

//! In-place radix-2 FFT, unscaled both ways (the peak reducer's, as study A8's). The twiddles come from
//! a table rather than a recurrence along each block, and the butterflies are written out in doubles
//! (std::complex<double>'s operators were not inlined): up to 32 of these an OFDM symbol
static void fft(std::vector<cd>& a, bool inverse)
{
    const int n = (int) a.size();
    const FftPlan& plan = fftPlan(n);
    double *x = reinterpret_cast<double *>(a.data());   // interleaved re, im

    for (int i = 0; i < n; i++)
    {
        const int j = plan.m_reverse[i];
        if (i < j) {
            std::swap(a[i], a[j]);
        }
    }

    const double sign = inverse ? -1.0 : 1.0;

    for (int len = 2; len <= n; len <<= 1)
    {
        const int half = len / 2, step = n / len;

        for (int i = 0; i < n; i += len)
        {
            for (int j = 0; j < half; j++)
            {
                const double wr = plan.m_cos[j * step], wi = sign * plan.m_sin[j * step];
                double *u = x + 2 * (i + j), *v = x + 2 * (i + j + half);
                const double vr = v[0] * wr - v[1] * wi, vi = v[0] * wi + v[1] * wr;
                const double ur = u[0], ui = u[1];
                u[0] = ur + vr;
                u[1] = ui + vi;
                v[0] = ur - vr;
                v[1] = ui - vi;
            }
        }
    }
}

// ---- Modulator ------------------------------------------------------------------------------

DVAngel61OFDM::Modulator::Modulator(DVAngel61::Profile profile) :
    m_profile(profile),
    m_nc(carriers(profile)),
    m_width(carriers(profile) + 2),
    m_peakReduction(true)
{
    const int len = SymbolSamples + TaperSamples;
    const double scale = 1.0 / std::sqrt((double) m_nc);
    m_tones.resize((size_t) m_width * len);

    for (int i = 0; i < m_width; i++)
    {
        const double f = carrierHz(m_nc, i - 1);
        for (int j = 0; j < len; j++)
        {
            const double t = (double) (j - PrefixSamples) / SampleRate;
            m_tones[(size_t) i * len + j] = cf(std::polar(scale, TwoPi * f * t));
        }
    }

    m_window.assign(len, 1.0f);
    for (int j = 0; j < TaperSamples; j++)
    {
        const float r = 0.5f * (1.0f - std::cos((float) M_PI * (j + 0.5f) / TaperSamples));
        m_window[j] = r;
        m_window[SymbolSamples + j] = 1.0f - r;
    }

    m_fftSize = m_nc > 64 ? 1024 : 512;
    m_sent.assign((size_t) FrameSymbols * m_width, cf(0, 0));
    reset();
}

void DVAngel61OFDM::Modulator::reset()
{
    m_tail.assign(TaperSamples, cf(0, 0));
}

/**
 * A8's TRe1HYB on one symbol's cells (Nc + 2, edge carriers included): 16 passes of clipping the
 * symbol to 3 dB (QPSK) or 5 dB (16-QAM) above its nominal rms and taking the clipping error's
 * spectrum C back onto the cells. Pilots: nothing. The edge carriers: 2 C, their amplitude held to 0.5
 * of a data cell. Header cells: C. Payload: an outer coordinate (every QPSK one, a 16-QAM +-3 one)
 * takes the outward part of C at 1.5 and the inward part at 1, held within 2x its nominal value
 * outwards; an inner 16-QAM coordinate takes C.
 */
void DVAngel61OFDM::Modulator::reduceSymbol(cf *cells, const cf *nominal, const signed char *kind, bool qam16)
{
    const int L = m_fftSize, nc = m_nc;
    const double cr = qam16 ? 5.0 : 3.0;
    const int passes = 16;
    const float mu = 1.5f, beta = 1.0f, aceMax = 2.0f, muEdge = 2.0f, edgeMax = 0.5f;
    const double amp = std::sqrt((double) nc) * std::pow(10.0, cr / 20.0), amp2 = amp * amp;
    auto bin = [&](int i) { const int g = (i - 1) - nc / 2; return ((g % L) + L) % L; };
    m_fft.resize(L);

    for (int pass = 0; pass < passes; pass++)
    {
        std::fill(m_fft.begin(), m_fft.end(), cd(0, 0));
        for (int i = 0; i < m_width; i++) {
            m_fft[bin(i)] = cd(cells[i]);
        }
        fft(m_fft, true);
        bool clipped = false;

        for (int n = 0; n < L; n++)
        {
            const double m2 = std::norm(m_fft[n]);
            if (m2 > amp2)
            {
                m_fft[n] *= amp / std::sqrt(m2) - 1.0;
                clipped = true;
            }
            else
            {
                m_fft[n] = 0.0;
            }
        }

        if (!clipped) {
            break;
        }

        fft(m_fft, false);

        for (int i = 0; i < m_width; i++)
        {
            const signed char k = kind[i];
            if (k == DVAngel61::CellPilot) {
                continue;
            }

            const cf C = cf(m_fft[bin(i)] / (double) L);

            if (k < 0)
            {
                // an edge carrier
                cf v = cells[i] + muEdge * C;
                const float m = std::abs(v);
                if (m > edgeMax) {
                    v *= edgeMax / m;
                }
                cells[i] = v;
            }
            else if (k == DVAngel61::CellHeader)
            {
                cells[i] += beta * C;
            }
            else
            {
                float cur[2] = { cells[i].real(), cells[i].imag() };
                const float x0[2] = { nominal[i].real(), nominal[i].imag() }, c[2] = { C.real(), C.imag() };

                for (int d = 0; d < 2; d++)
                {
                    const bool outer = !qam16 || std::fabs(x0[d]) > 0.6f;
                    if (!outer)
                    {
                        cur[d] += beta * c[d];
                        continue;
                    }
                    const float s = x0[d] > 0 ? 1.0f : -1.0f;
                    const float v = cur[d] + (s * c[d] > 0 ? mu : beta) * c[d];
                    const float mag = std::min(s * v, std::fabs(x0[d]) * aceMax);
                    cur[d] = s * mag;
                }

                cells[i] = cf(cur[0], cur[1]);
            }
        }
    }
}

void DVAngel61OFDM::Modulator::synthesizeSymbol(const cf *cells, cf *out)
{
    const int len = SymbolSamples + TaperSamples;
    std::fill(out, out + len, cf(0, 0));
    float *o = reinterpret_cast<float *>(out);

    for (int i = 0; i < m_width; i++)
    {
        if (cells[i] == cf(0, 0)) {
            continue;
        }
        const float xr = cells[i].real(), xi = cells[i].imag();
        const float *e = reinterpret_cast<const float *>(&m_tones[(size_t) i * len]);
        for (int m = 0; m < len; m++)
        {
            const float er = e[2 * m], ei = e[2 * m + 1];
            o[2 * m] += xr * er - xi * ei;
            o[2 * m + 1] += xr * ei + xi * er;
        }
    }

    for (int m = 0; m < len; m++) {
        out[m] *= m_window[m];
    }
}

void DVAngel61OFDM::Modulator::modulateFrame(const cf *cells, DVAngel61::Constellation payload, std::vector<cf>& out)
{
    const DVAngel61::Layout& l = DVAngel61::layout(m_profile);
    const bool qam16 = payload == DVAngel61::QAM16;
    std::vector<cf> sym(m_width), nominal(m_width), samples(SymbolSamples + TaperSamples);
    std::vector<signed char> kind(m_width);

    for (int s = 0; s < FrameSymbols; s++)
    {
        sym[0] = sym[m_width - 1] = cf(0, 0);
        kind[0] = kind[m_width - 1] = -1;

        for (int k = 0; k < m_nc; k++)
        {
            sym[k + 1] = cells[s * m_nc + k];
            kind[k + 1] = l.m_kind[s * m_nc + k];
        }

        if (m_peakReduction)
        {
            nominal = sym;
            reduceSymbol(sym.data(), nominal.data(), kind.data(), qam16);
        }

        std::copy(sym.begin(), sym.end(), m_sent.begin() + (size_t) s * m_width);
        synthesizeSymbol(sym.data(), samples.data());

        for (int m = 0; m < TaperSamples; m++) {
            samples[m] += m_tail[m];
        }

        out.insert(out.end(), samples.begin(), samples.begin() + SymbolSamples);
        std::copy(samples.begin() + SymbolSamples, samples.end(), m_tail.begin());
    }
}

void DVAngel61OFDM::Modulator::flush(std::vector<cf>& out)
{
    out.insert(out.end(), m_tail.begin(), m_tail.end());
    reset();
}

// ---- Demodulation ---------------------------------------------------------------------------

namespace {

//! e^{-j 2 pi f_k t} x sqrt(Nc) / N for t over the prefix and the useful part (from the prefix's start)
struct DemodTables
{
    int nc = 0;
    std::vector<cf> e;

    explicit DemodTables(DVAngel61::Profile profile)
    {
        nc = DVAngel61OFDM::carriers(profile);
        const int len = DVAngel61OFDM::SymbolSamples;
        const double scale = std::sqrt((double) nc) / DVAngel61OFDM::UsefulSamples;
        e.resize((size_t) nc * len);
        for (int k = 0; k < nc; k++)
        {
            const double f = DVAngel61OFDM::carrierHz(nc, k);
            for (int j = 0; j < len; j++)
            {
                const double t = (double) (j - DVAngel61OFDM::PrefixSamples) / DVAngel61OFDM::SampleRate;
                e[(size_t) k * len + j] = cf(std::polar(scale, -TwoPi * f * t));
            }
        }
    }
};

const DemodTables& demodTables(DVAngel61::Profile profile)
{
    static const DemodTables nb(DVAngel61::ProfileHFNB), wb(DVAngel61::ProfileHFWB);
    return profile == DVAngel61::ProfileHFWB ? wb : nb;
}

} // namespace

void DVAngel61OFDM::demodulateSymbol(DVAngel61::Profile profile, const cf *y, int woff, cf *cells)
{
    const DemodTables& t = demodTables(profile);
    const int len = SymbolSamples;
    const float *x = reinterpret_cast<const float *>(y);

    for (int k = 0; k < t.nc; k++)
    {
        const float *e = reinterpret_cast<const float *>(&t.e[(size_t) k * len + PrefixSamples + woff]);
        float re = 0.0f, im = 0.0f;
        for (int m = 0; m < UsefulSamples; m++)
        {
            re += x[2 * m] * e[2 * m] - x[2 * m + 1] * e[2 * m + 1];
            im += x[2 * m] * e[2 * m + 1] + x[2 * m + 1] * e[2 * m];
        }
        cells[k] = cf(re, im);
    }
}
