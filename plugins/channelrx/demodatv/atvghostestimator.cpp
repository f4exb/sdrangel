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

#include "atvghostestimator.h"

namespace {

typedef std::complex<double> Cd;

// Signal delayed by a fractional number of samples: v[k - delay], or v[0] before the start
Cd delayed(const std::vector<Cd>& v, int k, double delay)
{
    double x = k - delay;

    if (x <= 0.0) {
        return v[0];
    }

    int i = (int) x;
    double f = x - i;
    return i + 1 < (int) v.size() ? v[i] + f * (v[i + 1] - v[i]) : v[i];
}

// y[k] = a[k] - g y[k - delay], as the detector cancels
std::vector<Cd> cancel(const std::vector<Cd>& a, double delay, Cd gain)
{
    std::vector<Cd> y(a);

    for (int k = (int) std::ceil(delay) + 1; k < (int) a.size(); k++) {
        y[k] = a[k] - gain * delayed(y, k, delay);
    }

    return y;
}

}

// Keeps the trial cancellations stable
std::complex<double> ATVGhostEstimator::limitGain(std::complex<double> gain)
{
    return std::abs(gain) > m_maxGain ? gain * (m_maxGain / std::abs(gain)) : gain;
}

ATVGhostEstimator::ATVGhostEstimator()
{
    configure(10000000.0f, 640, 4.2e6f);
}

void ATVGhostEstimator::configure(float sampleRate, int samplesPerLine, float videoBandwidth)
{
    auto us = [sampleRate](double t) { return (int) std::round(t * 1e-6 * sampleRate); };
    double ringing = 1.25e6 / videoBandwidth; // us, for the edges' own response to settle

    m_sampleRate = sampleRate;
    m_samplesPerLine = samplesPerLine;
    m_pre = us(8.0);        // the echo of what is up to 4 us before the tip, and history for its cancellation
    m_post = us(9.0);
    m_tipStart = us(std::max(0.5, ringing));
    m_tipMid = us(2.3);
    m_tipEnd = us(4.2);
    m_backPorchStart = us(5.3);
    m_backPorchEnd = us(8.5);
    m_frontPorchStart = -us(1.2);
    m_frontPorchEnd = -us(0.3);
    m_maxDelay = 4.0e-6 * sampleRate;
    // Below 2 MHz, the edges ring for so long that tuning errors look like echoes
    m_minDelay = videoBandwidth < 2.0e6f ? m_maxDelay : std::max(1.0, 1e-6 * sampleRate * std::max(1.0, 2.0 * ringing));
    m_delayStep = 0.1e-6 * sampleRate;
    m_sharpness = 0.5e-6 * sampleRate;
    m_line.assign(m_pre + m_post, 0.0f);
    m_next.assign(m_pre, 0.0f);
    reset();
}

void ATVGhostEstimator::reset()
{
    m_sum.assign(m_pre + m_post, 0.0);
    m_lines = 0;
    m_lastOffset = 0;
    m_misses = 0;
    m_delay = (float) m_minDelay;
    m_gain = 0.0f;
}

bool ATVGhostEstimator::feed(int sampleOffset, std::complex<float> z)
{
    if (sampleOffset < m_lastOffset) {
        std::copy(m_next.begin(), m_next.end(), m_line.begin()); // a new line has started
    }

    m_lastOffset = sampleOffset;
    int k = sampleOffset - (m_samplesPerLine - m_pre);

    if ((k >= 0) && (k < m_pre))
    {
        m_next[k] = z;
    }
    else if ((sampleOffset >= 0) && (sampleOffset < m_post))
    {
        m_line[m_pre + sampleOffset] = z;

        if (sampleOffset == m_post - 1) {
            return endLine();
        }
    }

    return false;
}

bool ATVGhostEstimator::endLine()
{
    auto mean = [this](int start, int end) {
        double sum = 0.0;
        for (int i = m_pre + start; i < m_pre + end; i++) {
            sum += m_line[i].real();
        }
        return sum / (end - start);
    };

    // Only ordinary line sync pulses (not the vertical interval's): sync over the whole tip,
    // blanking either side. The echo can raise the second half of the tip
    double tipA = mean(m_tipStart, m_tipMid);
    double tipB = mean(m_tipMid, m_tipEnd);
    double backPorch = mean(m_backPorchStart, m_backPorchEnd);
    double frontPorch = mean(m_frontPorchStart, m_frontPorchEnd);
    double step = tipA - backPorch;

    if ((step > 0.1 * tipA) && (tipB - backPorch > 0.5 * step) && (tipA - frontPorch > 0.5 * step))
    {
        for (int i = 0; i < (int) m_line.size(); i++) {
            m_sum[i] += Cd(m_line[i].real(), m_line[i].imag());
        }

        m_lines++;
    }

    if (m_lines < m_linesPerEstimate) {
        return false;
    }

    estimate();
    std::fill(m_sum.begin(), m_sum.end(), 0.0);
    m_lines = 0;
    return true;
}

void ATVGhostEstimator::estimate()
{
    if (m_minDelay + m_sharpness > m_maxDelay) {
        return; // the video bandwidth is too narrow to tell an echo from the edges' ringing
    }

    std::vector<Cd> a(m_sum.size());

    for (int i = 0; i < (int) a.size(); i++) {
        a[i] = m_sum[i] / (double) m_lines;
    }

    const int windows[2][2] = {
        {m_pre + m_tipStart, m_pre + m_tipEnd},
        {m_pre + m_backPorchStart, m_pre + m_backPorchEnd}
    };

    // Power of the in-phase signal's variation about its mean, within each window
    auto flatness = [&windows](const std::vector<Cd>& y) {
        double power = 0.0;
        for (const auto& w : windows)
        {
            double mean = 0.0;
            for (int k = w[0]; k < w[1]; k++) {
                mean += y[k].real();
            }
            mean /= w[1] - w[0];
            for (int k = w[0]; k < w[1]; k++) {
                power += (y[k].real() - mean) * (y[k].real() - mean);
            }
        }
        return power;
    };

    // Least squares gain of an echo at the given delay in y (the in-phase part of y - g y[k - delay] flattest)
    auto fitGain = [&windows](const std::vector<Cd>& y, double delay) {
        double uu = 0.0, uv = 0.0, vv = 0.0, ut = 0.0, vt = 0.0;
        for (const auto& w : windows)
        {
            int n = w[1] - w[0];
            double mt = 0.0, mu = 0.0, mv = 0.0;
            for (int k = w[0]; k < w[1]; k++)
            {
                Cd d = delayed(y, k, delay);
                mt += y[k].real();
                mu += d.real();
                mv -= d.imag();
            }
            mt /= n; mu /= n; mv /= n;
            for (int k = w[0]; k < w[1]; k++)
            {
                Cd d = delayed(y, k, delay);
                double t = y[k].real() - mt;
                double u = d.real() - mu;
                double v = -d.imag() - mv;
                uu += u * u; uv += u * v; vv += v * v; ut += u * t; vt += v * t;
            }
        }
        double det = uu * vv - uv * uv;
        return det > 0.0 ? Cd((vv * ut - uv * vt) / det, (uu * vt - uv * ut) / det) : Cd(0.0);
    };

    // Gain of an echo at this delay, refined on the cancelled signal, and how flat that leaves it
    auto fitEcho = [&](double delay, Cd& gain) {
        gain = limitGain(fitGain(a, delay));
        for (int i = 0; i < 3; i++) {
            gain = limitGain(gain + fitGain(cancel(a, delay, gain), delay));
        }
        return flatness(cancel(a, delay, gain));
    };

    double original = flatness(a);

    if (original <= 0.0) {
        return;
    }

    // Search in steps of 0.1 us, then refine about the best
    std::vector<double> delays, flat;

    for (double delay = m_minDelay; delay <= m_maxDelay; delay += m_delayStep)
    {
        delays.push_back(delay);
        flat.push_back(flatness(cancel(a, delay, limitGain(fitGain(a, delay)))) / original);
    }

    // While an echo is being cancelled, look for it near where it was, and keep it unless it has
    // clearly gone, so the cancellation does not come and go as the picture changes
    bool tracking = m_gain != 0.0f;
    int side = (int) std::round(m_sharpness / m_delayStep);
    int first = 0, last = (int) flat.size() - 1;

    if (tracking)
    {
        int centre = (int) std::round((m_delay - m_minDelay) / m_delayStep);
        first = std::max(first, centre - side);
        last = std::min(last, centre + side);
    }

    int best = (int) (std::min_element(flat.begin() + first, flat.begin() + last + 1) - flat.begin());

    // An echo flattens the sync region at its delay, and clearly less so either side
    double worse = 1.0;

    if (best - side >= 0) {
        worse = std::min(worse, flat[best - side]);
    }
    if (best + side < (int) flat.size()) {
        worse = std::min(worse, flat[best + side]);
    }

    Cd gain = 0.0;
    double delay = delays[best];
    double sharpness = tracking ? m_keepSharpness : m_minSharpness;

    if ((flat[best] < m_minImprovement) && (worse - flat[best] > sharpness * (1.0 - flat[best])))
    {
        double f[3];
        for (int i = 0; i < 3; i++) {
            f[i] = fitEcho(delay + 0.5 * m_delayStep * (i - 1), gain);
        }

        double curvature = f[0] - 2.0 * f[1] + f[2];
        double offset = curvature > 0.0 ? 0.25 * (f[0] - f[2]) / curvature : 0.0;
        delay = std::max(m_minDelay, std::min(m_maxDelay, delay + m_delayStep * std::max(-0.5, std::min(0.5, offset))));

        if (fitEcho(delay, gain) >= m_minImprovement * original) {
            gain = 0.0;
        }
    }

    std::complex<float> g((float) gain.real(), (float) gain.imag());

    if (g == 0.0f)
    {
        // No echo: after several estimates without it, let it fade
        if (++m_misses >= m_maxMisses)
        {
            m_gain *= 1.0f - m_smoothing;

            if (std::abs(m_gain) < 0.02f) {
                m_gain = 0.0f;
            }
        }
    }
    else if (!tracking)
    {
        m_misses = 0;
        m_delay = (float) delay;
        m_gain = m_smoothing * g;
    }
    else
    {
        m_misses = 0;
        m_delay += m_smoothing * ((float) delay - m_delay);
        m_gain += m_smoothing * (g - m_gain);
    }
}
