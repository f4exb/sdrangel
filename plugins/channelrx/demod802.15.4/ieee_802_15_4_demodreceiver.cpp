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

#include "ieee_802_15_4_demodreceiver.h"

IEEE_802_15_4_DemodReceiver::IEEE_802_15_4_DemodReceiver() :
    m_bpsk(false),
    m_spc(4),
    m_chipsPerSymbol(32),
    m_numSymbols(16),
    m_P(128),
    m_window(4),
    m_threshold(0.1f),
    m_maxFrequencyOffset(200000.0f),
    m_maxOmega(0.0),
    m_pulseLength(0),
    m_coherentLane(true),
    m_nonCoherentLane(true),
    m_mask(0),
    m_n(0),
    m_crcScopeCount(0)
{
    m_stats = Statistics{0, 0, 0, 0, 0, 0};
    configure(m_phy, m_spc, m_maxFrequencyOffset, m_threshold);
}

void IEEE_802_15_4_DemodReceiver::configure(const IEEE_802_15_4_PHY& phy, int samplesPerChip, float maxFrequencyOffset, float threshold)
{
    m_phy = phy;
    m_bpsk = phy.m_modulation == IEEE_802_15_4_PHY::BPSK;
    m_spc = std::max(2, samplesPerChip);
    m_chipsPerSymbol = phy.getChipsPerSymbol();
    m_numSymbols = m_bpsk ? 1 : 16;
    m_P = m_chipsPerSymbol * m_spc;
    // BPSK has a 32 bit preamble, O-QPSK 8 symbols. The detector needs m_window + 1 symbols
    // of preamble to fill, and the longer the window, the lower the false alarm rate
    m_window = m_bpsk ? 16 : 6;
    m_verifyThreshold = m_chipsPerSymbol >= 32 ? m_verifyThreshold32 : m_verifyThreshold16;
    m_lostThreshold = m_lostThresholdFactor * std::sqrt(M_PI / 4.0) / std::sqrt((double) m_chipsPerSymbol);
    m_threshold = threshold;
    m_pulse = phy.getPulse(m_spc);
    m_pulseLength = (int) m_pulse.size();

    // Normalise pulse energy, so correlation values are comparable between PHYs
    double energy = 0.0;
    for (auto p : m_pulse) {
        energy += p * p;
    }
    for (auto& p : m_pulse) {
        p /= std::sqrt(energy);
    }

    m_chipSigns.resize(m_numSymbols);
    for (int s = 0; s < m_numSymbols; s++)
    {
        m_chipSigns[s].resize(m_chipsPerSymbol);
        for (int k = 0; k < m_chipsPerSymbol; k++) {
            m_chipSigns[s][k] = phy.getChip(s, k) ? 1.0f : -1.0f;
        }
    }

    m_chipValues.resize(m_chipsPerSymbol);

    // O-QPSK: even chips on I, odd chips on Q
    m_chipRotation.resize(m_chipsPerSymbol);
    for (int k = 0; k < m_chipsPerSymbol; k++) {
        m_chipRotation[k] = (m_bpsk || !(k & 1)) ? Cf(1.0f, 0.0f) : Cf(0.0f, -1.0f);
    }

    setMaxFrequencyOffset(maxFrequencyOffset);
    allocate();
    reset();
}

void IEEE_802_15_4_DemodReceiver::setMaxFrequencyOffset(float maxFrequencyOffset)
{
    m_maxFrequencyOffset = maxFrequencyOffset;
    m_maxOmega = 2.0 * M_PI * maxFrequencyOffset / (double) getSampleRate();
}

void IEEE_802_15_4_DemodReceiver::allocate()
{
    // Enough history for the detector window, the acquisition segment and the matched filter
    int64_t required = (2 * m_window + 6) * (int64_t) m_P + m_pulseLength + 64;
    int64_t size = 1;

    while (size < required) {
        size <<= 1;
    }

    m_x.assign(size, Cf(0.0f, 0.0f));
    m_xf.assign(size, Cf(0.0f, 0.0f));
    m_d.assign(size, Cf(0.0f, 0.0f));
    m_y.assign(size, Cf(0.0f, 0.0f));
    m_mask = size - 1;
}

void IEEE_802_15_4_DemodReceiver::reset()
{
    std::fill(m_x.begin(), m_x.end(), Cf(0.0f, 0.0f));
    std::fill(m_xf.begin(), m_xf.end(), Cf(0.0f, 0.0f));
    m_n = 0;
    m_R = Cd(0.0, 0.0);
    m_E1 = 0.0;
    m_E2 = 0.0;
    m_detectorPeakEnergy = 0.0;
    m_state = Search;
    m_waitUntil = 0;
    m_holdoffUntil = 0;
    m_frames.clear();
    m_scope = Scope{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    m_crcScopeCount = 0;
}

IEEE_802_15_4_DemodReceiver::Frame IEEE_802_15_4_DemodReceiver::popFrame()
{
    Frame frame = m_frames.front();
    m_frames.pop_front();
    return frame;
}

void IEEE_802_15_4_DemodReceiver::process(const Cf& x)
{
    m_x[m_n & m_mask] = x;

    // Detector input is averaged over a chip, so its noise bandwidth is close to the signal
    // bandwidth, whatever the number of samples per chip
    Cf xf(0.0f, 0.0f);
    for (int i = 0; i < m_spc; i++) {
        xf += m_x[(m_n - i) & m_mask];
    }
    m_xf[m_n & m_mask] = xf;

    updateDetector();
    m_n++;

    if (m_crcScopeCount > 0)
    {
        m_crcScopeCount--;
        if (m_crcScopeCount == 0) {
            m_scope.m_crc = 0.0f;
        }
    }

    if (m_state == Receive)
    {
        m_powerSum += std::norm(x);
        m_powerCount++;

        // If we haven't found the SFD yet, the acquisition may have been false (E.g. on a
        // run of zeros at the end of a BPSK frame, with the next frame following shortly
        // after), so allow a fresh detection, or one from a detector window that is entirely
        // after the acquisition, to restart acquisition
        if (m_scope.m_detectMetric < m_threshold / 2.0f) {
            m_detectorCleared = true;
        }
        bool newWindow = m_n - m_acquisitionN >= (int64_t) (m_window + 1) * m_P;
        if ((m_detectorCleared || newWindow) && (m_scope.m_detectMetric >= m_threshold) && searchingForSFD())
        {
            m_stats.m_detections++;
            m_state = Wait;
            m_waitUntil = m_n + (m_window + 2) * m_P;
            m_waitMax = m_scope.m_detectMetric;
            m_waitMaxN = m_n;
            m_waitR = m_R;
        }
        else
        {
            processReceive();
        }
    }
    else if (m_state == Wait)
    {
        // Acquire once the detector has peaked, which is when its window is inside the
        // preamble, so the acquisition segment that ends a symbol later is too
        if (m_scope.m_detectMetric > m_waitMax)
        {
            m_waitMax = m_scope.m_detectMetric;
            m_waitMaxN = m_n;
            m_waitR = m_R;
        }
        if ((m_n - m_waitMaxN >= m_P) || (m_n >= m_waitUntil))
        {
            if (!acquire())
            {
                m_state = Search;
                m_holdoffUntil = m_n + m_P;
            }
        }
    }
    else if ((m_n >= m_holdoffUntil) && (m_scope.m_detectMetric >= m_threshold))
    {
        // Wait for more of the preamble to arrive, so the acquisition segment is
        // inside the preamble even for a signal that has only just crossed the threshold
        m_stats.m_detections++;
        m_state = Wait;
        m_waitUntil = m_n + (m_window + 2) * m_P;
        m_waitMax = m_scope.m_detectMetric;
        m_waitMaxN = m_n;
        m_waitR = m_R;
    }

    m_scope.m_state = (float) m_state;
}

// Delay-and-correlate over one symbol period, summed over m_window symbols
void IEEE_802_15_4_DemodReceiver::updateDetector()
{
    int64_t windowSamples = (int64_t) m_window * m_P;

    if (m_n < windowSamples + m_P)
    {
        m_scope.m_detectMetric = 0.0f;
        if (m_n == windowSamples + m_P - 1) {
            recalculateDetector();
        }
        return;
    }

    // Periodically recalculate, to stop rounding errors accumulating
    if ((m_n & 0xffff) == 0)
    {
        recalculateDetector();
    }
    else
    {
        Cd x0(m_xf[m_n & m_mask]);
        Cd x1(m_xf[(m_n - m_P) & m_mask]);
        int64_t l = m_n - windowSamples;
        Cd l0(m_xf[l & m_mask]);
        Cd l1(m_xf[(l - m_P) & m_mask]);

        m_R += x0 * std::conj(x1) - l0 * std::conj(l1);
        m_E1 += std::norm(x0) - std::norm(l0);
        m_E2 += std::norm(x1) - std::norm(l1);

        // The running sums have rounding errors relative to the largest energy they've held.
        // If the energy has since fallen to near that level (E.g. a strong frame followed by
        // zero samples), the errors dominate and can give a large metric, so recalculate
        m_detectorPeakEnergy = std::max(m_detectorPeakEnergy, std::max(m_E1, m_E2));
        if ((m_E1 < m_detectorRelativeError * m_detectorPeakEnergy) || (m_E2 < m_detectorRelativeError * m_detectorPeakEnergy)) {
            recalculateDetector();
        }
    }

    double denom = m_E1 * m_E2;
    m_scope.m_detectMetric = ((m_E1 > 0.0) && (m_E2 > 0.0) && (denom > 1e-30)) ? (float) (std::norm(m_R) / denom) : 0.0f;
}

void IEEE_802_15_4_DemodReceiver::recalculateDetector()
{
    int64_t windowSamples = (int64_t) m_window * m_P;

    m_R = Cd(0.0, 0.0);
    m_E1 = 0.0;
    m_E2 = 0.0;

    for (int64_t i = m_n - windowSamples + 1; i <= m_n; i++)
    {
        Cd x0(m_xf[i & m_mask]);
        Cd x1(m_xf[(i - m_P) & m_mask]);
        m_R += x0 * std::conj(x1);
        m_E1 += std::norm(x0);
        m_E2 += std::norm(x1);
    }

    m_detectorPeakEnergy = std::max(m_E1, m_E2);
}

// Find frequency offset and symbol timing from the preamble
bool IEEE_802_15_4_DemodReceiver::acquire()
{
    const int64_t newest = m_n - 1;
    const int segLength = (m_window + 2) * m_P;
    const int64_t segStart = newest - segLength + 1;
    const int N = m_chipsPerSymbol;

    if (segStart - m_pulseLength < 0) {
        return false;
    }

    // Frequency, modulo the symbol rate, from the detector at its peak
    const double omegaFine = std::arg(m_waitR) / m_P;
    const double omegaStep = 2.0 * M_PI / m_P;
    int mMax = (int) std::ceil((m_maxOmega + omegaStep / 2.0) / omegaStep);

    std::vector<Cf> d(segLength + m_pulseLength - 1);
    std::vector<Cf> y(segLength);
    std::vector<Cf> bestY;
    std::vector<double> metric(m_P);
    std::vector<double> bestMetric;
    double bestValue = -1.0;
    int bestO = 0;
    double bestOmega = 0.0;

    for (int m = -mMax; m <= mMax; m++)
    {
        double omega = omegaFine + m * omegaStep;

        if ((std::abs(omega) > m_maxOmega + omegaStep / 2.0) && (m != 0)) {
            continue;
        }

        // De-rotate and matched filter the segment
        int64_t dStart = segStart - m_pulseLength + 1;
        for (int i = 0; i < (int) d.size(); i++)
        {
            double a = -omega * (double) (dStart + i - segStart);
            d[i] = m_x[(dStart + i) & m_mask] * Cf((float) std::cos(a), (float) std::sin(a));
        }
        for (int i = 0; i < segLength; i++)
        {
            Cf acc(0.0f, 0.0f);
            for (int j = 0; j < m_pulseLength; j++) {
                acc += m_pulse[j] * d[i + j];
            }
            y[i] = acc;
        }

        // Correlate against the preamble symbol at each timing offset,
        // combining symbols non-coherently
        double maxValue = -1.0;
        int maxO = 0;

        for (int o = 0; o < m_P; o++)
        {
            double sum = 0.0;

            for (int j = 0; j <= m_window; j++)
            {
                int idx = o + j * m_P;
                Cf c(0.0f, 0.0f);
                for (int k = 0; k < N; k++) {
                    c += m_chipSigns[0][k] * (y[idx + k * m_spc] * m_chipRotation[k]);
                }
                sum += std::abs(c);
            }

            metric[o] = sum;
            if (sum > maxValue)
            {
                maxValue = sum;
                maxO = o;
            }
        }

        if (maxValue > bestValue)
        {
            bestValue = maxValue;
            bestO = maxO;
            bestOmega = omega;
            bestY = y;
            bestMetric = metric;
        }
    }

    if (bestValue <= 0.0) {
        return false;
    }

    // Normalised correlation of each symbol at the best timing
    std::vector<Cd> C;
    std::vector<double> q;
    double qMax = 0.0;

    for (int j = 0; j <= m_window; j++)
    {
        int idx = bestO + j * m_P;
        Cf c(0.0f, 0.0f);
        double energy = 0.0;

        for (int k = 0; k < N; k++)
        {
            Cf u = bestY[idx + k * m_spc] * m_chipRotation[k];
            c += m_chipSigns[0][k] * u;
            energy += std::norm(u);
        }

        double qj = std::abs(c) / std::sqrt(N * energy + 1e-30);
        C.push_back(Cd(c));
        q.push_back(qj);
        qMax = std::max(qMax, qj);
    }

    // BPSK data also correlates with the preamble chips, as a 1 is just the inverse
    // sequence, so also check the differentially decoded bits are 0
    int good = 0;
    std::vector<bool> preamble(q.size());
    for (int j = 0; j < (int) q.size(); j++)
    {
        bool zero = !m_bpsk || ((j > 0) && ((C[j] * std::conj(C[j - 1])).real() > 0.0));
        good += (q[j] >= m_verifyThreshold) && zero ? 1 : 0;
        preamble[j] = (q[j] >= m_verifyThreshold * 0.8) && zero;
    }

    if (good < m_verifySymbols)
    {
        if (m_debug)
        {
            fprintf(stderr, "acquire failed: n %lld omega %f good %d q:", (long long) m_n, bestOmega, good);
            for (auto v : q) fprintf(stderr, " %.2f", v);
            fprintf(stderr, "\n");
        }
        return false;
    }

    // Start decoding at the start of the longest run of preamble symbols. Starting before
    // it, on noise, would use up the number of symbols the framer allows to find the SFD,
    // and the segment may end with symbols after the preamble that happen to correlate,
    // such as the zero bits in a BPSK SFD
    int j0 = 0;
    int bestRun = 0;
    for (int j = 0; j < (int) q.size(); )
    {
        if (preamble[j])
        {
            int k = j;
            while ((k < (int) q.size()) && preamble[k]) {
                k++;
            }
            if (k - j >= bestRun)
            {
                bestRun = k - j;
                j0 = j;
            }
            j = k;
        }
        else
        {
            j++;
        }
    }
    // For BPSK, preamble[] marks a symbol whose differential with the previous is 0, so
    // the previous symbol is part of the preamble too
    if (m_bpsk && (j0 > 0)) {
        j0--;
    }

    // Fractional timing from the shape of the correlation peak
    double mm = bestMetric[(bestO + m_P - 1) % m_P];
    double m0 = bestMetric[bestO];
    double mp = bestMetric[(bestO + 1) % m_P];
    double denom = mm - 2.0 * m0 + mp;
    double delta = (std::abs(denom) > 1e-30) ? 0.5 * (mm - mp) / denom : 0.0;
    delta = std::max(-0.5, std::min(0.5, delta));

    // Refine frequency from the phase change between consecutive preamble symbols
    Cd acc(0.0, 0.0);
    for (int j = j0 + 1; j < (int) C.size(); j++)
    {
        if ((q[j] >= m_verifyThreshold * 0.8) && (q[j - 1] >= m_verifyThreshold * 0.8)) {
            acc += C[j] * std::conj(C[j - 1]);
        }
    }
    if (std::abs(acc) > 0.0) {
        bestOmega += std::arg(acc) / m_P;
    }

    m_stats.m_acquisitions++;
    if (m_debug) {
        fprintf(stderr, "acquire: n %lld omega %f (%.0f Hz) o %d delta %.2f j0 %d qMax %.2f q:", (long long) m_n, bestOmega, bestOmega * getSampleRate() / (2.0 * M_PI), bestO, delta, j0, qMax);
        for (auto v : q) fprintf(stderr, " %.2f", v);
        fprintf(stderr, "\n");
    }
    startReceive(bestOmega, segStart, segStart + bestO + delta + j0 * m_P);

    return true;
}

void IEEE_802_15_4_DemodReceiver::startReceive(double omega, int64_t segStart, double tau)
{
    m_state = Receive;
    m_omega = omega;
    m_ncoRef = segStart;
    // Start a little before the segment, so early-late and the interpolator have history
    m_yNext = segStart - m_spc - 2;
    m_dNext = m_yNext - m_pulseLength + 1;
    m_rot = std::polar(1.0, -omega * (double) (m_dNext - m_ncoRef));
    m_rotStep = std::polar(1.0, -omega);
    m_tau = tau;
    m_phi = 0.0;
    m_dphi = 0.0;
    m_phaseInit = false;
    m_frameStart = segStart;
    m_powerSum = 0.0;
    m_powerCount = 0;
    m_qSum = 0.0;
    m_qCount = 0;
    m_ampSum = 0.0;
    m_ampSumSq = 0.0;
    m_ampCount = 0;
    m_recentQ.clear();
    m_detectorCleared = false;
    m_acquisitionN = m_n;

    m_framers[0].reset();
    m_framers[1].reset();
    m_prevECoh = -1;
    m_prevC = Cd(0.0, 0.0);

    for (int64_t i = segStart; i < m_n; i++) {
        m_powerSum += std::norm(m_x[i & m_mask]);
        m_powerCount++;
    }

    processReceive();
}

// Cubic (Catmull-Rom) interpolation of matched filter output
IEEE_802_15_4_DemodReceiver::Cf IEEE_802_15_4_DemodReceiver::yAt(double t) const
{
    int64_t i = (int64_t) std::floor(t);
    float mu = (float) (t - (double) i);
    Cf p0 = m_y[(i - 1) & m_mask];
    Cf p1 = m_y[i & m_mask];
    Cf p2 = m_y[(i + 1) & m_mask];
    Cf p3 = m_y[(i + 2) & m_mask];
    Cf a = -0.5f * p0 + 1.5f * p1 - 1.5f * p2 + 0.5f * p3;
    Cf b = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
    Cf c = -0.5f * p0 + 0.5f * p2;

    return ((a * mu + b) * mu + c) * mu + p1;
}

void IEEE_802_15_4_DemodReceiver::processReceive()
{
    // De-rotate and matched filter all samples received so far
    while (m_dNext < m_n)
    {
        m_d[m_dNext & m_mask] = m_x[m_dNext & m_mask] * Cf((float) m_rot.real(), (float) m_rot.imag());
        m_rot *= m_rotStep;
        if ((m_dNext & 0x3ff) == 0) {
            m_rot /= std::abs(m_rot);
        }
        m_dNext++;
    }

    while (m_yNext < m_dNext)
    {
        Cf acc(0.0f, 0.0f);
        int64_t start = m_yNext - m_pulseLength + 1;
        for (int j = 0; j < m_pulseLength; j++) {
            acc += m_pulse[j] * m_d[(start + j) & m_mask];
        }
        m_y[m_yNext & m_mask] = acc;
        m_yNext++;
    }

    // Decode all symbols for which we have samples, including early-late margin and interpolator
    while ((m_state == Receive) && (m_tau + (m_chipsPerSymbol - 1) * m_spc + m_spc / 2.0 + 3.0 < (double) m_yNext)) {
        decodeSymbol();
    }
}

IEEE_802_15_4_DemodReceiver::Cd IEEE_802_15_4_DemodReceiver::correlate(double tau, int symbol) const
{
    Cf c(0.0f, 0.0f);
    const std::vector<float>& signs = m_chipSigns[symbol];

    for (int k = 0; k < m_chipsPerSymbol; k++) {
        c += signs[k] * (yAt(tau + k * m_spc) * m_chipRotation[k]);
    }

    return Cd(c);
}

void IEEE_802_15_4_DemodReceiver::decodeSymbol()
{
    const int N = m_chipsPerSymbol;
    std::vector<Cf>& u = m_chipValues;
    double energy = 0.0;

    for (int k = 0; k < N; k++)
    {
        u[k] = yAt(m_tau + k * m_spc) * m_chipRotation[k];
        energy += std::norm(u[k]);
    }

    Cd rot;
    double q;
    double phaseError;
    int decided; // Symbol (O-QPSK) or chip sequence (BPSK) used for timing

    if (!m_bpsk)
    {
        Cd C[16];
        int bestNC = 0;
        double bestNCValue = -1.0;

        for (int s = 0; s < 16; s++)
        {
            Cf c(0.0f, 0.0f);
            const std::vector<float>& signs = m_chipSigns[s];
            for (int k = 0; k < N; k++) {
                c += signs[k] * u[k];
            }
            C[s] = Cd(c);
            double mag = std::abs(C[s]);
            if (mag > bestNCValue)
            {
                bestNCValue = mag;
                bestNC = s;
            }
        }

        if (!m_phaseInit)
        {
            m_phi = std::arg(C[bestNC]);
            m_phaseInit = true;
        }

        rot = std::polar(1.0, -m_phi);
        int bestCoh = 0;
        double bestCohValue = -1e30;

        for (int s = 0; s < 16; s++)
        {
            double v = (C[s] * rot).real();
            if (v > bestCohValue)
            {
                bestCohValue = v;
                bestCoh = s;
            }
        }

        Cd cr = C[bestCoh] * rot;
        phaseError = std::arg(cr);
        q = bestNCValue / std::sqrt(N * energy + 1e-30);
        decided = bestCoh;

        // SNR estimate from the in-phase component of the decided correlation. The
        // quadrature component also contains ISI between the I and Q chips, so isn't used
        m_ampSum += cr.real();
        m_ampSumSq += cr.real() * cr.real();
        m_ampCount++;

        frameSymbol(m_coherentLane ? bestCoh : bestNC, m_nonCoherentLane ? bestNC : bestCoh);
    }
    else
    {
        Cf c(0.0f, 0.0f);
        const std::vector<float>& signs = m_chipSigns[0];
        for (int k = 0; k < N; k++) {
            c += signs[k] * u[k];
        }
        Cd C(c);

        if (!m_phaseInit)
        {
            m_phi = std::arg(C);
            m_phaseInit = true;
            m_prevC = C;
        }

        rot = std::polar(1.0, -m_phi);
        Cd cr = C * rot;
        int eCoh = cr.real() < 0.0 ? 1 : 0;
        if (m_prevECoh < 0) {
            m_prevECoh = eCoh;
        }
        int bitCoh = eCoh ^ m_prevECoh;
        m_prevECoh = eCoh;
        int bitNC = (C * std::conj(m_prevC)).real() < 0.0 ? 1 : 0;
        m_prevC = C;

        if (eCoh) {
            cr = -cr;
        }
        phaseError = std::arg(cr);
        q = std::abs(C) / std::sqrt(N * energy + 1e-30);
        decided = 0;

        m_ampSum += cr.real();
        m_ampSumSq += cr.real() * cr.real();
        m_ampCount++;

        frameBits(m_coherentLane ? bitCoh : bitNC, m_nonCoherentLane ? bitNC : bitCoh);
    }

    if (m_debug) {
        fprintf(stderr, "sym tau %.2f q %.2f dec %d phi %.2f perr %.2f\n", m_tau, q, decided, m_phi, phaseError);
    }
    m_scope.m_correlation = (float) q;
    Cf chip = u[N - 1] * Cf((float) rot.real(), (float) rot.imag());
    m_scope.m_chip = std::sqrt(energy / N) > 0.0 ? (chip.real() / (float) std::sqrt(energy / N)) : 0.0f;
    m_qSum += q;
    m_qCount++;

    // Abandon frame quickly if signal lost, as an acknowledgement can follow a frame
    // after only 12 symbol periods
    const int lostSymbols = m_bpsk ? 8 : 4;
    m_recentQ.push_back(q);
    if ((int) m_recentQ.size() > lostSymbols) {
        m_recentQ.pop_front();
    }
    bool lost = false;
    if ((int) m_recentQ.size() == lostSymbols)
    {
        double sum = 0.0;
        for (auto v : m_recentQ) {
            sum += v;
        }
        lost = sum / lostSymbols < m_lostThreshold;
    }

    if (m_state != Receive) {
        return; // Frame finished
    }

    // Carrier phase loop
    m_dphi += m_phaseFreqGain * phaseError;
    m_phi += m_dphi + m_phaseGain * phaseError;
    m_phi = std::remainder(m_phi, 2.0 * M_PI);
    m_scope.m_phase = (float) (m_phi / M_PI);

    // Early-late timing
    double offset = m_spc / 2.0;
    double early = std::abs(correlate(m_tau - offset, decided));
    double late = std::abs(correlate(m_tau + offset, decided));
    double timingError = (early + late) > 0.0 ? (late - early) / (late + early) : 0.0;

    m_tau += m_P + m_timingGain * timingError * m_spc;

    if (lost) {
        abortFrame();
    }
}

void IEEE_802_15_4_DemodReceiver::frameSymbol(int symCoh, int symNC)
{
    // Least significant bit first
    for (int i = 0; (i < 4) && (m_state == Receive); i++) {
        frameBits((symCoh >> i) & 1, (symNC >> i) & 1);
    }
}

void IEEE_802_15_4_DemodReceiver::Framer::reset()
{
    m_state = FramerPreamble;
    m_preambleBits = 0;
    m_sfd = 0xffff;
    m_bitCount = 0;
    m_phr = 0;
    m_length = 0;
    m_bits.clear();
    m_psdu.clear();
    m_fcsOK = false;
    m_checked = false;
}

void IEEE_802_15_4_DemodReceiver::Framer::addBit(int bit)
{
    switch (m_state)
    {
    case FramerPreamble:
        // Look for 8 zero bits followed by SFD (0xa7), received LSB first
        m_sfd = (m_sfd >> 1) | (bit << 15);
        m_preambleBits++;
        if (m_sfd == 0xa700)
        {
            m_state = FramerPHR;
            m_bitCount = 0;
            m_phr = 0;
        }
        else if (m_preambleBits > 32 + 8 + 4)
        {
            m_state = FramerFailed;
        }
        break;

    case FramerPHR:
        m_phr |= bit << m_bitCount;
        m_bitCount++;
        if (m_bitCount == 8)
        {
            m_length = m_phr & 0x7f;
            if (lengthValid(m_length))
            {
                m_bits.clear();
                m_bits.reserve(m_length * 8);
                m_state = FramerPSDU;
            }
            else
            {
                m_state = FramerFailed;
            }
        }
        break;

    case FramerPSDU:
        m_bits.push_back(bit);
        if ((int) m_bits.size() >= m_length * 8)
        {
            m_psdu.assign(m_length, 0);
            for (int i = 0; i < m_length * 8; i++) {
                m_psdu[i / 8] |= m_bits[i] << (i % 8);
            }
            uint16_t fcs = IEEE_802_15_4_FrameDecoder::calcFCS(m_psdu.data(), m_length - 2);
            m_fcsOK = (m_psdu[m_length - 2] == (fcs & 0xff)) && (m_psdu[m_length - 1] == (fcs >> 8));
            m_state = FramerDone;
        }
        break;

    default:
        break;
    }
}

bool IEEE_802_15_4_DemodReceiver::searchingForSFD() const
{
    for (const auto& framer : m_framers)
    {
        if ((framer.m_state == FramerPHR) || (framer.m_state == FramerPSDU) || (framer.m_state == FramerDone)) {
            return false;
        }
    }

    return true;
}

void IEEE_802_15_4_DemodReceiver::frameBits(int bitCoh, int bitNC)
{
    m_framers[0].addBit(bitCoh);
    m_framers[1].addBit(bitNC);

    // Output as soon as either lane has a frame with a valid FCS
    for (int lane = 0; lane < 2; lane++)
    {
        Framer& framer = m_framers[lane];

        if ((framer.m_state == FramerDone) && !framer.m_checked)
        {
            framer.m_checked = true;

            if (framer.m_fcsOK)
            {
                finishFrame(lane);
                return;
            }
        }
    }

    // Neither lane has a valid frame, and neither will
    if (!m_framers[0].active() && !m_framers[1].active()) {
        abortFrame();
    }
}

void IEEE_802_15_4_DemodReceiver::finishFrame(int lane)
{
    Frame frame;
    const Framer& framer = m_framers[lane];

    frame.m_psdu = framer.m_psdu;
    frame.m_fcsOK = framer.m_fcsOK;
    frame.m_coherent = lane == 0;

    if (frame.m_fcsOK)
    {
        m_stats.m_framesOK++;
        if (frame.m_coherent) {
            m_stats.m_framesCoherent++;
        } else {
            m_stats.m_framesNonCoherent++;
        }
    }
    else
    {
        m_stats.m_framesBad++;
    }

    if (!frame.m_psdu.empty())
    {
        frame.m_frequencyOffset = (float) ((m_omega + m_dphi / m_P) * getSampleRate() / (2.0 * M_PI));
        frame.m_lqi = m_qCount > 0 ? (float) (m_qSum / m_qCount) : 0.0f;
        frame.m_power = m_powerCount > 0 ? (float) (m_powerSum / m_powerCount) : 0.0f;
        if (m_ampCount > 1)
        {
            double mean = m_ampSum / m_ampCount;
            double var = m_ampSumSq / m_ampCount - mean * mean;
            frame.m_snr = (var > 0.0) ? (float) (10.0 * std::log10(mean * mean / (2.0 * var))) : 99.0f;
        }
        else
        {
            frame.m_snr = 0.0f;
        }
        frame.m_startSample = m_frameStart;
        frame.m_endSample = m_n - 1;
        m_frames.push_back(frame);
        m_scope.m_crc = frame.m_fcsOK ? 1.0f : -1.0f;
        m_crcScopeCount = 4 * m_P;
    }

    endReceive();
}

// Stop receiving, outputting a complete frame from either lane if there is one,
// even though its FCS is invalid. The other lane may still be active if it decoded
// a different (longer) length, and has now lost the signal
void IEEE_802_15_4_DemodReceiver::abortFrame()
{
    if (m_framers[0].m_state == FramerDone) {
        finishFrame(0);
    } else if (m_framers[1].m_state == FramerDone) {
        finishFrame(1);
    } else {
        endReceive();
    }
}

void IEEE_802_15_4_DemodReceiver::endReceive()
{
    m_state = Search;
    // Allow detector window to clear of the frame just received
    m_holdoffUntil = m_n + m_P;
}
