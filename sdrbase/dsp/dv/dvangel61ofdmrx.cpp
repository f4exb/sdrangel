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

// The HF receiver (ANGEL61.md section 5, plan item C4), ported from the design studies'
// simulator (sdrbase/dsp/dv/tests/angel61_hf.cpp: acqCore, Learner, refreshDelay, estimateFrame and
// runTracking, as A5 and A6 measured them), at the studies' 16 kS/s.

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <algorithm>
#include <cmath>
#include <cstring>

#include "dvangel61ofdm.h"

#ifdef ANGEL61_RX_PROFILE
#include <chrono>
#include <cstdio>
double g_rxProfile[8] = { 0 };
const char *g_rxProfileNames[8] = { "demodulate", "learn", "estimate", "decode", "delay design", "fixed design", "acquire", "decimate" };
struct RxTimer
{
    int slot;
    std::chrono::steady_clock::time_point t0;
    explicit RxTimer(int s) : slot(s), t0(std::chrono::steady_clock::now()) {}
    ~RxTimer() { g_rxProfile[slot] += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(); }
};
#define RX_TIME(slot) RxTimer rxTimer##slot(slot)
#else
#define RX_TIME(slot)
#endif

namespace {

typedef std::complex<float> cf;
typedef std::complex<double> cd;

const double TwoPi = 2.0 * M_PI;
const double Fs = 16000.0;              // the receiver's rate: the input decimated by 3
const int Decimation = 3;
const int N = 320;                      // 20 ms
const int Ncp = 64;                     // 4 ms
const int Nsym = N + Ncp;
const int Wola = 16;                    // 1 ms
const int L = 10;
const int FL = L * Nsym;
const int ExtraCarriers = 2;            // carriers beyond each edge the acquisition looks at
const int Df = 8;                       // pilot spacing on the diagonal (2.2)
const double AcqFirstFrameFill = 0.5;   // acquisition waits while its window's first frame is less filled than this
const int HeaderCells = DVAngel61::HeaderCells;
const int ReacquireAfter = 25;          // frames without an accepted header (A5)
const int ConfirmWithin = 4;            // frames an acquisition has to have a header accepted, else it is dropped
const int WeakHold = 8;                 // the hold (for ReacquireAfter) while one header only is accepted since an acquisition
const double InitialSigma = 5.0;        // the learner's Doppler before it has measured one, Hz
const int PullFrames = 12;              // frames after an acquisition the frequency loop takes up to 2 Hz a step
const int CpSpan = 160;                 // the tracking's prefix correlation: samples either side of the timing
const double CpBias = 6.5;              // the prefix correlation's extent's middle against the profile's centre, one path (measured: the WOLA ramps lengthen it on one side)
const double CpHalfWidth = 46.0;        // one path's prefix correlation above 30%: this either side (measured, AWGN)
const int CpGuard = 16;
const int SeedWidth = 8;                // samples either side of a path the guard finds that the learning is seeded over
const double SeedShare = 0.2;           // the power the seed gives it                 // samples its extent's middle may differ from the profile's before it takes over
const int DelayRefresh = 4;             // frames between re-solving the delay design (A6)
const double IciMinSigma = 1.5;         // Hz: the learnt Doppler from which a failed payload has its inter-carrier interference cancelled
const int IciCarriers = 2;              // the carriers either side whose leakage is cancelled
const int IciPasses = 3;                // cancellations and decodes at most
const double Sbl2DAbove = 3.0;          // Hz, the learned 2 sigma above which SBL is two-dimensional
const int Sbl2DIterations = 4;
const double DesignMargin = 1.3, DesignOffset = 0.05;   // the design's Doppler: margin x learned sigma + offset
const int CellIterations = 2;
//     // updateCells: the share of each tap's power moved to each neighbour a frame           // EM iterations a frame of the learning from the known cells
const int FloorAround = 4;              // samples about a learned tap the design's floor covers (cells' learning)
const double CellTapWidth = 1.0;        // samples: a learned tap's delay uncertainty in the design (cells' learning)
const float QpskA = 0.70710678f, QamA = 0.31622777f;

// ---- layout at 16 kS/s ------------------------------------------------------------------------

struct RxLayout
{
    DVAngel61::Profile profile;
    int Nc = 0, cells = 0, rep = 2;
    double df = 50.0;
    std::vector<int> kind;              // 0 payload, 1 pilot, 2 header (the studies' convention)
    std::vector<int> pilots, headers, payload;
    std::vector<cf> pilotValue;
    std::vector<cf> E;                  // e^{j 2 pi f t}, carriers -ExtraCarriers .. Nc + ExtraCarriers - 1, t from -Ncp over N + 2 Ncp
    int T = N + 2 * Ncp;

    double carrierHz(int k) const { return (k - (Nc - 1) / 2.0) * df; }
    const cf *table(int k) const { return &E[(size_t) (k + ExtraCarriers) * T]; }

    explicit RxLayout(DVAngel61::Profile p) : profile(p)
    {
        const DVAngel61::Layout& l = DVAngel61::layout(p);
        Nc = l.m_carriers;
        cells = l.m_cells;
        rep = l.m_headerCopies;
        kind.assign(cells, 0);
        pilotValue.assign(cells, cf(0, 0));
        for (int c = 0; c < cells; c++)
        {
            if (l.m_kind[c] == DVAngel61::CellPilot)
            {
                kind[c] = 1;
                pilots.push_back(c);
                pilotValue[c] = l.m_known[c];
            }
            else if (l.m_kind[c] == DVAngel61::CellHeader)
            {
                kind[c] = 2;
            }
        }
        headers = l.m_headerCells;
        payload = l.m_payloadCells;
        E.resize((size_t) (Nc + 2 * ExtraCarriers) * T);
        for (int k = -ExtraCarriers; k < Nc + ExtraCarriers; k++)
        {
            const double f = carrierHz(k);
            for (int j = 0; j < T; j++) {
                E[(size_t) (k + ExtraCarriers) * T + j] = cf(std::polar(1.0, TwoPi * f * (j - Ncp) / Fs));
            }
        }
    }
};

cf dotConj(const cf *a, const cf *b, int n)
{
    const float *x = reinterpret_cast<const float *>(a);
    const float *y = reinterpret_cast<const float *>(b);
    float re = 0.0f, im = 0.0f;
    for (int m = 0; m < n; m++)
    {
        re += x[2 * m] * y[2 * m] + x[2 * m + 1] * y[2 * m + 1];
        im += x[2 * m + 1] * y[2 * m] - x[2 * m] * y[2 * m + 1];
    }
    return cf(re, im);
}

//! Cells k0 .. k1 - 1 of a symbol whose window starts woff after its useful start, at the sent scale
void demodulateSymbol(const RxLayout& lay, const cf *usefulStart, int woff, int k0, int k1, cf *cells)
{
    const float scale = std::sqrt((float) lay.Nc) / N;
    for (int k = k0; k < k1; k++) {
        cells[k - k0] = dotConj(usefulStart + woff, lay.table(k) + Ncp + woff, N) * scale;
    }
}

// ---- correlation tables and Wiener fits (A6) ---------------------------------------------------

struct CorrTab
{
    int Nc = 0;
    std::vector<cd> t;
    cd at(int dl, int dk) const { return t[(size_t) (dl + L - 1) * (2 * Nc - 1) + (dk + Nc - 1)]; }
    cd cell(int nc, int c1, int c2) const { return at(c1 / nc - c2 / nc, c1 % nc - c2 % nc); }
};

//! A delay tap: delay and power, Doppler (sigma, Hz) and delay uncertainty (width, seconds: uniform)
struct Tap { double tau, power, sigma, width; };

CorrTab tabTaps(const RxLayout& lay, const std::vector<Tap>& taps)
{
    CorrTab tb;
    tb.Nc = lay.Nc;
    tb.t.assign((size_t) (2 * L - 1) * (2 * lay.Nc - 1), cd(0, 0));
    std::vector<cd> rf(2 * lay.Nc - 1);
    for (const Tap& tp : taps)
    {
        if (tp.power <= 0.0) {
            continue;
        }
        for (int dk = -(lay.Nc - 1); dk <= lay.Nc - 1; dk++)
        {
            const double x = M_PI * dk * lay.df * tp.width;
            const double spread = std::fabs(x) < 1e-9 ? 1.0 : std::sin(x) / x;
            rf[dk + lay.Nc - 1] = std::polar(tp.power * spread, -TwoPi * dk * lay.df * tp.tau);
        }
        for (int dl = -(L - 1); dl <= L - 1; dl++)
        {
            const double dt = (double) dl * Nsym / Fs;
            const double tf = std::exp(-2.0 * M_PI * M_PI * tp.sigma * tp.sigma * dt * dt);
            cd *row = &tb.t[(size_t) (dl + L - 1) * (2 * lay.Nc - 1)];
            for (int i = 0; i < 2 * lay.Nc - 1; i++) {
                row[i] += tf * rf[i];
            }
        }
    }
    return tb;
}

//! A uniform delay profile over [t0, t1] seconds of total power `power`, Doppler 2 sigma = 2 sigma
CorrTab tabUniform(const RxLayout& lay, double t0, double t1, double sigma, double power)
{
    CorrTab tb;
    tb.Nc = lay.Nc;
    tb.t.assign((size_t) (2 * L - 1) * (2 * lay.Nc - 1), cd(0, 0));
    const double span = t1 - t0;
    for (int dl = -(L - 1); dl <= L - 1; dl++)
    {
        const double dt = (double) dl * Nsym / Fs;
        const double tf = std::exp(-2.0 * M_PI * M_PI * sigma * sigma * dt * dt);
        for (int dk = -(lay.Nc - 1); dk <= lay.Nc - 1; dk++)
        {
            const double f = dk * lay.df, x = f * span;
            const double s = std::fabs(x) < 1e-12 ? 1.0 : std::sin(M_PI * x) / (M_PI * x);
            tb.t[(size_t) (dl + L - 1) * (2 * lay.Nc - 1) + dk + lay.Nc - 1] = std::polar(power * tf * s, -M_PI * f * (t0 + t1));
        }
    }
    return tb;
}

struct Fit
{
    int n = 0;
    std::vector<cf> w;                  // w[t n + i]
    std::vector<float> mse;
};

/**
 * Wiener weights of `targets` from `anchors` under the correlation R and noise nv (the studies'
 * fitTable, the same arithmetic reorganised for speed): the anchors' covariance factored in double
 * precision (Cholesky, then the factor's inverse row by row), then for every target y = L^-1 b and
 * x = L^-H y, both row by row over contiguous single-precision arrays, w = conj(x), and the MSE
 * R(c, c) - Re(b^H x).
 */
Fit fitTable(const RxLayout& lay, const std::vector<int>& anchors, const std::vector<int>& targets, const CorrTab& R, double nv)
{
    const int n = (int) anchors.size(), nc = lay.Nc;
    std::vector<int> al(n), ak(n);
    for (int i = 0; i < n; i++)
    {
        al[i] = anchors[i] / nc;
        ak[i] = anchors[i] % nc;
    }
    std::vector<cd> Lc((size_t) n * n, cd(0, 0));
    for (int i = 0; i < n; i++)
    {
        cd *Li = &Lc[(size_t) i * n];
        for (int j = 0; j <= i; j++)
        {
            const cd *Lj = &Lc[(size_t) j * n];
            cd s = R.at(al[i] - al[j], ak[i] - ak[j]) + (i == j ? cd(nv + 1e-9, 0) : cd(0, 0));
            for (int t = 0; t < j; t++) {
                s -= Li[t] * std::conj(Lj[t]);
            }
            if (i == j) {
                Li[i] = cd(std::sqrt(std::max(1e-12, s.real())), 0.0);
            } else {
                Li[j] = s / Lj[j].real();
            }
        }
    }
    // the inverse of the lower factor, row by row: row i = (e_i - sum_{t < i} L[i][t] row t) / L[i][i]
    std::vector<cd> Inv((size_t) n * n, cd(0, 0));
    for (int i = 0; i < n; i++)
    {
        cd *row = &Inv[(size_t) i * n];
        row[i] = cd(1.0, 0.0);
        const cd *Li = &Lc[(size_t) i * n];
        for (int t = 0; t < i; t++)
        {
            const cd a = Li[t];
            const cd *rt = &Inv[(size_t) t * n];
            for (int j = 0; j <= t; j++) {
                row[j] -= a * rt[j];
            }
        }
        const double d = 1.0 / Li[i].real();
        for (int j = 0; j <= i; j++) {
            row[j] *= d;
        }
    }
    std::vector<float> ire((size_t) n * n), iim((size_t) n * n);
    for (size_t e = 0; e < Inv.size(); e++)
    {
        ire[e] = (float) Inv[e].real();
        iim[e] = (float) Inv[e].imag();
    }

    Fit f;
    f.n = n;
    f.w.assign((size_t) targets.size() * n, cf(0, 0));
    f.mse.assign(targets.size(), 0.0f);
    std::vector<float> bre(n), bim(n), yre(n), yim(n), xre(n), xim(n);
    for (size_t ti = 0; ti < targets.size(); ti++)
    {
        const int c = targets[ti], cl = c / nc, ck = c % nc;
        for (int i = 0; i < n; i++)
        {
            const cd b = R.at(al[i] - cl, ak[i] - ck);
            bre[i] = (float) b.real();
            bim[i] = (float) b.imag();
        }
        // y = Inv b
        for (int i = 0; i < n; i++)
        {
            const float *rr = &ire[(size_t) i * n], *ri = &iim[(size_t) i * n];
            float sr = 0.0f, si = 0.0f;
            for (int j = 0; j <= i; j++)
            {
                sr += rr[j] * bre[j] - ri[j] * bim[j];
                si += rr[j] * bim[j] + ri[j] * bre[j];
            }
            yre[i] = sr;
            yim[i] = si;
        }
        // x = Inv^H y: x[j] += conj(Inv[i][j]) y[i] over the rows
        std::fill(xre.begin(), xre.end(), 0.0f);
        std::fill(xim.begin(), xim.end(), 0.0f);
        for (int i = 0; i < n; i++)
        {
            const float *rr = &ire[(size_t) i * n], *ri = &iim[(size_t) i * n];
            const float yr = yre[i], yi = yim[i];
            for (int j = 0; j <= i; j++)
            {
                xre[j] += rr[j] * yr + ri[j] * yi;
                xim[j] += rr[j] * yi - ri[j] * yr;
            }
        }
        double er = 0.0;
        cf *w = &f.w[ti * n];
        for (int i = 0; i < n; i++)
        {
            w[i] = cf(xre[i], -xim[i]);
            er += (double) bre[i] * xre[i] + (double) bim[i] * xim[i];
        }
        f.mse[ti] = (float) std::max(0.0, R.at(0, 0).real() - er);
    }
    return f;
}

#ifdef ANGEL61_RX_PROFILE
Fit fitTableRef(const RxLayout& lay, const std::vector<int>& anchors, const std::vector<int>& targets, const CorrTab& R, double nv)
{
    const int n = (int) anchors.size();
    std::vector<cd> Lc((size_t) n * n, cd(0, 0));
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j <= i; j++)
        {
            cd s = R.cell(lay.Nc, anchors[i], anchors[j]) + (i == j ? cd(nv + 1e-9, 0) : cd(0, 0));
            for (int t = 0; t < j; t++) {
                s -= Lc[(size_t) i * n + t] * std::conj(Lc[(size_t) j * n + t]);
            }
            if (i == j) {
                Lc[(size_t) i * n + i] = cd(std::sqrt(std::max(1e-12, s.real())), 0.0);
            } else {
                Lc[(size_t) i * n + j] = s / Lc[(size_t) j * n + j].real();
            }
        }
    }
    Fit f;
    f.n = n;
    f.w.assign((size_t) targets.size() * n, cf(0, 0));
    f.mse.assign(targets.size(), 0.0f);
    std::vector<cd> b(n), y(n), x(n);
    for (size_t ti = 0; ti < targets.size(); ti++)
    {
        const int c = targets[ti];
        for (int i = 0; i < n; i++) {
            b[i] = R.cell(lay.Nc, anchors[i], c);
        }
        for (int i = 0; i < n; i++)
        {
            cd acc = b[i];
            const cd *Li = &Lc[(size_t) i * n];
            for (int t = 0; t < i; t++) {
                acc -= Li[t] * y[t];
            }
            y[i] = acc / Li[i].real();
        }
        for (int i = n - 1; i >= 0; i--)
        {
            cd acc = y[i];
            for (int t = i + 1; t < n; t++) {
                acc -= std::conj(Lc[(size_t) t * n + i]) * x[t];
            }
            x[i] = acc / Lc[(size_t) i * n + i].real();
        }
        cd e(0, 0);
        for (int i = 0; i < n; i++)
        {
            f.w[ti * n + i] = cf(std::conj(x[i]));
            e += std::conj(b[i]) * x[i];
        }
        f.mse[ti] = (float) std::max(0.0, R.cell(lay.Nc, c, c).real() - e.real());
    }
    return f;
}

#endif

//! Weights that are finite and bounded, and finite errors: else the design is refused (section 5)
bool sane(const Fit& f)
{
    for (const cf& w : f.w)
    {
        if (!std::isfinite(w.real()) || !std::isfinite(w.imag()) || (std::abs(w) > 100.0f)) {
            return false;
        }
    }
    for (float m : f.mse)
    {
        if (!std::isfinite(m)) {
            return false;
        }
    }
    return true;
}

// ---- what the receiver learns of the channel from the pilots (A6) ------------------------------

struct Learner
{
    const RxLayout& lay;
    static const int Half = 80;         // +-5 ms on a one-sample grid
    std::vector<cf> ex;
    std::vector<cd> phase;              // (2 Nc - 1) x (2 Half + 1): phaseOf
    std::vector<double> win;
    std::vector<double> pdp;
    double power = 1.0;
    bool havePdp = false;
    std::vector<cd> suv, fsuv;
    std::vector<double> suu, svv, nn, fnn;
    std::vector<cf> prev;
    bool havePrev = false;
    std::vector<double> pdpSlow;        // updateCells: the profile over about 20 frames (5 s), for the timing
    std::vector<double> ruu, rvv;       // updateCells: the pairs' powers with their noise (the phase's precision)
    bool cellsResidual = false;         // residual() from updateCells' pairs
    std::vector<cf> prevG;              // updateCells: the last frame's known cells
    std::vector<float> prevN;
    bool havePrevG = false;
    double sigma = 1.0;
    int frames = 0;

    explicit Learner(const RxLayout& l) : lay(l)
    {
        ex.resize((size_t) (2 * Half + 1) * lay.Nc);
        for (int m = -Half; m <= Half; m++) {
            for (int k = 0; k < lay.Nc; k++) {
                ex[(size_t) (m + Half) * lay.Nc + k] = cf(std::polar(1.0, TwoPi * lay.carrierHz(k) * m / Fs));
            }
        }
        win.resize(lay.Nc);
        for (int k = 0; k < lay.Nc; k++) {
            win[k] = 0.5 - 0.5 * std::cos(TwoPi * (k + 0.5) / lay.Nc);
        }
        phase.resize((size_t) (2 * lay.Nc - 1) * (2 * Half + 1));
        for (int dk = -(lay.Nc - 1); dk <= lay.Nc - 1; dk++) {
            for (int m = 0; m <= 2 * Half; m++) {
                phase[(size_t) (dk + lay.Nc - 1) * (2 * Half + 1) + m] = std::polar(1.0, -TwoPi * dk * lay.df * (m - Half) / Fs);
            }
        }
        reset();
    }

    void reset()
    {
        pdp.assign(2 * Half + 1, 0.0);
        power = 1.0;
        havePdp = false;
        const int lags = 2 * L + 1;
        suv.assign(lags, cd(0, 0)); suu.assign(lags, 0); svv.assign(lags, 0); nn.assign(lags, 0);
        fsuv.assign(lags, cd(0, 0)); fnn.assign(lags, 0);
        prev.assign(lay.cells, cf(0, 0));
        havePrev = false;
        pdpSlow.clear();
        ruu.assign(lags, 0);
        rvv.assign(lags, 0);
        cellsResidual = false;
        prevG.assign(lay.cells, cf(0, 0));
        prevN.assign(lay.cells, -1.0f);
        havePrevG = false;
        // wide until measured: the first frames' design for 1 Hz lost 10 Hz channels' headers until
        // the learning had their spread (results section 44)
        sigma = InitialSigma;
        frames = 0;
    }

    void update(const cf *Y, double nv)
    {
        const int Nc = lay.Nc;
        std::vector<cf> obs(lay.cells, cf(0, 0));
        std::vector<cd> hk(Nc, cd(0, 0));
        std::vector<int> nk(Nc, 0);
        for (int c : lay.pilots)
        {
            obs[c] = Y[c] * std::conj(lay.pilotValue[c]);
            hk[c % Nc] += cd(obs[c]);
            nk[c % Nc]++;
        }
        double pw = 0;
        int used = 0;
        for (int k = 0; k < Nc; k++)
        {
            if (!nk[k]) {
                continue;
            }
            hk[k] /= nk[k];
            pw += std::norm(hk[k]) - nv / nk[k];
            used++;
        }
        pw = std::max(1e-4, pw / std::max(1, used));
#ifdef ANGEL61_RX_PROFILE
        const std::vector<double> before = pdp;
        const bool hadPdp = havePdp;
#endif
        const bool twoD = (frames > 0) && (2.0 * sigma > Sbl2DAbove);
        if (twoD) {
            for (int it = 0; it < Sbl2DIterations; it++) {
                sbl2D(obs, nv, pw);
            }
        } else {
            sblStep(hk, nk, nv, pw);
        }
#ifdef ANGEL61_RX_PROFILE
        static int checks[2] = { 0, 0 };
        if (checks[twoD] < 3)
        {
            checks[twoD]++;
            const std::vector<double> mine = pdp;
            pdp = before;
            havePdp = hadPdp;
            if (twoD) {
                for (int it = 0; it < Sbl2DIterations; it++) {
                    sbl2DRef(obs, nv, pw);
                }
            } else {
                sblStepRef(hk, nk, nv, pw);
            }
            double diff = 0, mx = 0;
            for (size_t m = 0; m < pdp.size(); m++) { diff = std::max(diff, std::fabs(pdp[m] - mine[m])); mx = std::max(mx, pdp[m]); }
            printf("SBL check (%s): largest tap %.3g, largest difference %.3g\n", twoD ? "2D" : "1D", mx, diff);
            pdp = mine;
        }
#endif
        power = havePdp ? 0.5 * power + 0.5 * pw : pw;
        havePdp = true;

        // Doppler: same-carrier pilot pairs within the frame and against the last one
        for (size_t i = 0; i < suv.size(); i++)
        {
            suv[i] *= 0.9; suu[i] *= 0.9; svv[i] *= 0.9; nn[i] *= 0.9;
            fsuv[i] = 0; fnn[i] = 0;
        }
        auto pair = [&](const cf& u, const cf& v, int lag) {
            if ((lag <= 0) || (lag >= (int) suv.size())) {
                return;
            }
            const cd p = cd(u) * std::conj(cd(v));
            suv[lag] += p; suu[lag] += std::norm(u); svv[lag] += std::norm(v); nn[lag] += 1;
            fsuv[lag] += p; fnn[lag] += 1;
        };
        for (int k = 0; k < Nc; k++)
        {
            for (int l2 = 0; l2 < L; l2++)
            {
                if (lay.kind[l2 * Nc + k] != 1) {
                    continue;
                }
                for (int l1 = 0; l1 < l2; l1++) {
                    if (lay.kind[l1 * Nc + k] == 1) {
                        pair(obs[l1 * Nc + k], obs[l2 * Nc + k], l2 - l1);
                    }
                }
                if (havePrev) {
                    for (int l1 = 0; l1 < L; l1++) {
                        if (lay.kind[l1 * Nc + k] == 1) {
                            pair(prev[l1 * Nc + k], obs[l2 * Nc + k], L - l1 + l2);
                        }
                    }
                }
            }
        }
        prev = obs;
        havePrev = true;

        // the spread: the longest lag whose noise-corrected correlation is between 0.3 and 0.97
        const double Ts = (double) Nsym / Fs;
        int best = -1, first = -1, last = -1;
        std::vector<double> rho(suv.size(), 0.0);
        for (int lag = 1; lag < (int) suv.size(); lag++)
        {
            if (nn[lag] < 8) {
                continue;
            }
            const double a = std::max(1e-9 * nn[lag], suu[lag] - nn[lag] * nv), b = std::max(1e-9 * nn[lag], svv[lag] - nn[lag] * nv);
            rho[lag] = std::min(0.999, std::abs(suv[lag]) / std::sqrt(a * b));
            if (first < 0) {
                first = lag;
            }
            last = lag;
            if ((rho[lag] >= 0.3) && (rho[lag] <= 0.97)) {
                best = lag;
            }
        }
        if (best > 0) {
            sigma = std::sqrt(-std::log(rho[best])) / (M_PI * std::sqrt(2.0) * best * Ts);
        } else if ((last > 0) && (rho[first] > 0.97)) {
            sigma = std::sqrt(-std::log(std::max(0.97, rho[last]))) / (M_PI * std::sqrt(2.0) * last * Ts);
        } else if (first > 0) {
            sigma = std::sqrt(-std::log(std::max(0.02, rho[first]))) / (M_PI * std::sqrt(2.0) * first * Ts);
        }
        frames++;
    }

    /**
     * Learning from every cell the frame is known in (plan C4's fix for 10 Hz of Doppler spread): the
     * pilots, and the header's cells once the header is accepted. g: the channel at a cell (Y x* /
     * |x|^2), nvc its noise (negative where the cell is not known).
     *
     * The tap powers by snapshot sparse Bayesian learning: each symbol's known cells are one snapshot
     * of the frequency response, h_k = sum_m a_m e^{-j 2 pi f_k tau_m}, the snapshots sharing the
     * powers P_m and nothing else, so that no time model enters. Each step is MacKay's fixed point
     * (the posterior mean's power over one less the posterior variance's share), not EM's (the mean
     * power plus the variance): EM grows a tap at the floor by 1 + P (|d|^2 - q) a step, a few
     * percent a frame with the few known cells a symbol has, and a path that was in a fade when the
     * learning started (NVIS's second, at 1 Hz), or that the sample clock moves onto the next tap,
     * stayed unlearned for 100 frames (NVIS lost frames at any SNR; 20 ppm lost 4 dB at 3 ms and
     * 10 Hz, 40 ppm the lock). MacKay's grows it by |d|^2 / q. update's learning from the pilots alone needs a time
     * model to tell the true taps from the aliases of pilots 8 carriers apart (sbl2D), and at 10 Hz the
     * Doppler it learns for it is wrong; the header's cells, spread over time and frequency, break the
     * aliasing without one.
     *
     * The Doppler from same-carrier pairs of known cells at every lag, within the frame and against
     * the last one, noise removed per cell: a weighted least-squares fit of the Gaussian correlation,
     * -ln rho = 2 pi^2 sigma^2 (lag Ts)^2, over the lags from 1 up to the first whose correlation is
     * below 0.2 (a near-zero correlation's magnitude is biased up, 0.1 to 0.35 at 10 Hz, and at long
     * lags it would dominate the fit), weighted by pairs x rho^2. update's rule, the longest lag whose
     * correlation is within 0.3 to 0.97, can take such a lag's chance 0.3 and design for 0.7 Hz.
     *
     * Measured in the design study (angel61_hf.cpp, estimator ddh, genie timing): within 0.1 to 0.3 dB
     * of the matched statistics at 10 Hz, where the pilots' learning was 2.3 dB behind on HF-NB tier 0
     * and never reached 10% on the other tiers.
     */
    void updateCells(const std::vector<cf>& g, const std::vector<float>& nvc)
    {
        const int Nc = lay.Nc, M = 2 * Half + 1, D = 2 * Nc - 1;
        double pw = 0.0;
        int used = 0;
        for (int c = 0; c < lay.cells; c++)
        {
            if (nvc[c] >= 0.0f)
            {
                pw += std::norm(g[c]) - nvc[c];
                used++;
            }
        }
        pw = std::max(1e-4, pw / std::max(1, used));
        if (!havePdp)
        {
            // flat over the whole window, not the prefix alone: a path beyond the prefix (NVIS's 7 ms,
            // 6 ms at 10 Hz) would start from nothing, and the learning settle on its alias 40 samples
            // (2.5 ms, the pilots' 8 carriers) nearer
            pdp.assign(M, pw / M);
        }
        const double floorTap = 2e-4 * pw;
        std::vector<int> ks;
        std::vector<cd> C, Ci, v, Gd(D), Nd(D);
        for (int it = 0; it < CellIterations; it++)
        {
            // C_ij = G(k_i - k_j) + noise, G(dk) = sum_m P_m phase(dk, m)
            for (int dk = -(Nc - 1); dk <= Nc - 1; dk++)
            {
                cd acc(0, 0);
                for (int m = 0; m < M; m++) {
                    if (pdp[m] > 0) {
                        acc += pdp[m] * phaseOf(dk, m);
                    }
                }
                Gd[dk + Nc - 1] = acc;
            }
            std::vector<double> Mu(M, 0.0), Sg(M, 0.0);
            int snaps = 0;
            for (int l = 0; l < L; l++)
            {
                ks.clear();
                for (int k = 0; k < Nc; k++) {
                    if (nvc[l * Nc + k] >= 0.0f) {
                        ks.push_back(k);
                    }
                }
                const int n = (int) ks.size();
                if (n < 2) {
                    continue;
                }
                C.assign((size_t) n * n, cd(0, 0));
                for (int i = 0; i < n; i++)
                {
                    for (int j = 0; j <= i; j++) {
                        C[(size_t) i * n + j] = Gd[ks[i] - ks[j] + Nc - 1];
                    }
                    C[(size_t) i * n + i] += nvc[l * Nc + ks[i]];
                }
                hermitianInverse(C, n, Ci);
                // v = C^-1 g; N(dk) = sum over pairs with k_i - k_j = dk of Ci_ij
                v.assign(n, cd(0, 0));
                std::fill(Nd.begin(), Nd.end(), cd(0, 0));
                for (int i = 0; i < n; i++)
                {
                    const cd *row = &Ci[(size_t) i * n];
                    cd acc(0, 0);
                    for (int j = 0; j < n; j++)
                    {
                        acc += row[j] * cd(g[l * Nc + ks[j]]);
                        Nd[ks[i] - ks[j] + Nc - 1] += row[j];
                    }
                    v[i] = acc;
                }
                for (int m = 0; m < M; m++)
                {
                    // d = a^H C^-1 g and q = a^H C^-1 a, a_i = conj(e_m[k_i])
                    const cf *e = &ex[(size_t) m * Nc];
                    cd d(0, 0), q(0, 0);
                    for (int i = 0; i < n; i++) {
                        d += cd(e[ks[i]]) * v[i];
                    }
                    for (int dk = -(Nc - 1); dk <= Nc - 1; dk++) {
                        q += Nd[dk + Nc - 1] * std::conj(phaseOf(dk, m));
                    }
                    const double Pm = pdp[m];
                    Mu[m] += std::norm(Pm * d);
                    Sg[m] += std::max(0.0, Pm - Pm * Pm * q.real());
                }
                snaps++;
            }
            if (!snaps) {
                break;
            }
            for (int m = 0; m < M; m++)
            {
                // MacKay's fixed point, the mean |mu|^2 over 1 - Sigma / P (the growth |d|^2 / q, at
                // most 100 an iteration)
                const double keep = std::max(1e-3, 1.0 - (Sg[m] / snaps) / pdp[m]);
                const double Pm = std::min(100.0 * pdp[m], (Mu[m] / snaps) / keep);
                pdp[m] = std::max(floorTap, (havePdp || (it > 0)) ? 0.5 * pdp[m] + 0.5 * Pm : Pm);
            }
        }
        power = havePdp ? 0.5 * power + 0.5 * pw : pw;
        havePdp = true;
        // The long run, for the timing (extent): at fast fading a path's power swings frame to frame,
        // and the latest profile's extent then collapses onto the stronger path and the timing chases
        // it back and forth. About 20 frames: the mean of the frames so far until there are 20
        if (pdpSlow.size() != pdp.size()) {
            pdpSlow = pdp;
        } else {
            const double w = std::max(0.05, 1.0 / (frames + 1));
            for (size_t m = 0; m < pdp.size(); m++) {
                pdpSlow[m] = (1.0 - w) * pdpSlow[m] + w * pdp[m];
            }
        }

        // the Doppler and the residual frequency: same-carrier pairs of known cells
        for (size_t i = 0; i < suv.size(); i++)
        {
            suv[i] *= 0.9; suu[i] *= 0.9; svv[i] *= 0.9; nn[i] *= 0.9; ruu[i] *= 0.9; rvv[i] *= 0.9;
            fsuv[i] = 0; fnn[i] = 0;
        }
        auto pair = [&](const cf& u, float nu, const cf& w, float nw, int lag) {
            if ((lag <= 0) || (lag >= (int) suv.size())) {
                return;
            }
            const cd p = cd(u) * std::conj(cd(w));
            suv[lag] += p; suu[lag] += std::norm(u) - nu; svv[lag] += std::norm(w) - nw; nn[lag] += 1;
            ruu[lag] += std::norm(u); rvv[lag] += std::norm(w);
            fsuv[lag] += p; fnn[lag] += 1;
        };
        cellsResidual = true;
        for (int k = 0; k < Nc; k++)
        {
            for (int l2 = 0; l2 < L; l2++)
            {
                const int c2 = l2 * Nc + k;
                if (nvc[c2] < 0.0f) {
                    continue;
                }
                for (int l1 = 0; l1 < l2; l1++)
                {
                    const int c1 = l1 * Nc + k;
                    if (nvc[c1] >= 0.0f) {
                        pair(g[c1], nvc[c1], g[c2], nvc[c2], l2 - l1);
                    }
                }
                if (havePrevG)
                {
                    for (int l1 = 0; l1 < L; l1++)
                    {
                        const int c1 = l1 * Nc + k;
                        if (prevN[c1] >= 0.0f) {
                            pair(prevG[c1], prevN[c1], g[c2], nvc[c2], L - l1 + l2);
                        }
                    }
                }
            }
        }
        prevG = g;
        prevN = nvc;
        havePrevG = true;

        const double Ts = (double) Nsym / Fs;
        double num = 0.0, den = 0.0, rhoLongest = 1.0;
        int longest = -1;
        for (int lag = 1; lag < (int) suv.size(); lag++)
        {
            if (nn[lag] < 32) {
                continue;
            }
            const double a = std::max(1e-9 * nn[lag], suu[lag]), b = std::max(1e-9 * nn[lag], svv[lag]);
            const double r = std::min(0.999, std::abs(suv[lag]) / std::sqrt(a * b));
            longest = lag;
            rhoLongest = r;
            if (r < 0.2) {
                break;
            }
            if (r > 0.97) {
                continue;
            }
            const double x = 2.0 * M_PI * M_PI * (lag * Ts) * (lag * Ts), y = -std::log(r), w = nn[lag] * r * r;
            num += w * x * y;
            den += w * x * x;
        }
        if (den > 0.0) {
            sigma = std::sqrt(num / den);
        } else if ((longest > 0) && (rhoLongest < 0.2)) {
            // The shortest lag with pairs enough is decorrelated already: fast, and that lag bounds it
            // from below. Before a header is accepted the pairs are the pilots', on the diagonal, 8
            // symbols apart, which 10 Hz of spread has decorrelated; taking this for the slow case
            // below gave a sigma of a fraction of a hertz, and the design built on it failed the
            // headers that would have given the short lags (results section 44)
            sigma = std::max(sigma, std::sqrt(-std::log(0.2) / (2.0 * M_PI * M_PI * (longest * Ts) * (longest * Ts))));
        } else if (longest > 0) {
            // every lag above 0.97: slow; the longest lag bounds it
            sigma = std::sqrt(-std::log(std::max(0.97, rhoLongest)) / (2.0 * M_PI * M_PI * (longest * Ts) * (longest * Ts)));
        }
        frames++;
    }

    /**
     * The phase of carrier difference dk at the delay of tap m: e^{-j 2 pi dk df (m - Half) / Fs}.
     * Every sum over pairs of pilots in the two SBL steps depends on the pair only through its
     * carrier difference and on the tap only through this phase, so each is grouped by carrier
     * difference once and then evaluated for every tap: 2 Nc - 1 terms a tap rather than n^2.
     */
    cd phaseOf(int dk, int m) const { return phase[(size_t) (dk + lay.Nc - 1) * (2 * Half + 1) + m]; }

    //! C^-1 of the Hermitian positive definite C (lower triangle given), in full
    static void hermitianInverse(std::vector<cd>& C, int n, std::vector<cd>& Ci)
    {
        for (int i = 0; i < n; i++)
        {
            cd *Li = &C[(size_t) i * n];
            for (int j = 0; j <= i; j++)
            {
                const cd *Lj = &C[(size_t) j * n];
                cd sum = Li[j];
                for (int t = 0; t < j; t++) {
                    sum -= Li[t] * std::conj(Lj[t]);
                }
                if (i == j) {
                    Li[i] = cd(std::sqrt(std::max(1e-12, sum.real())), 0);
                } else {
                    Li[j] = sum / Lj[j].real();
                }
            }
        }
        std::vector<cd> Inv((size_t) n * n, cd(0, 0));
        for (int i = 0; i < n; i++)
        {
            cd *row = &Inv[(size_t) i * n];
            row[i] = cd(1.0, 0.0);
            const cd *Li = &C[(size_t) i * n];
            for (int t = 0; t < i; t++)
            {
                const cd a = Li[t];
                const cd *rt = &Inv[(size_t) t * n];
                for (int j = 0; j <= t; j++) {
                    row[j] -= a * rt[j];
                }
            }
            const double d = 1.0 / Li[i].real();
            for (int j = 0; j <= i; j++) {
                row[j] *= d;
            }
        }
        // C^-1 = Inv^H Inv, row by row of Inv
        Ci.assign((size_t) n * n, cd(0, 0));
        for (int t = 0; t < n; t++)
        {
            const cd *rt = &Inv[(size_t) t * n];
            for (int i = 0; i <= t; i++)
            {
                const cd ci = std::conj(rt[i]);
                cd *out = &Ci[(size_t) i * n];
                for (int j = 0; j <= t; j++) {
                    out[j] += ci * rt[j];
                }
            }
        }
    }

    //! One EM step of sparse Bayesian learning of the tap powers from each carrier's mean pilot
    //! (the studies' sblStep, grouped by carrier difference)
    void sblStep(const std::vector<cd>& hk, const std::vector<int>& nk, double nv, double pw)
    {
        const int Nc = lay.Nc, M = 2 * Half + 1, D = 2 * Nc - 1;
        std::vector<int> ks;
        for (int k = 0; k < Nc; k++) {
            if (nk[k]) {
                ks.push_back(k);
            }
        }
        const int n = (int) ks.size();
        if (!havePdp)
        {
            pdp.assign(M, 0.0);
            const int span = Ncp / 2;
            for (int m = 0; m < M; m++) {
                if (std::abs(m - Half) <= span) {
                    pdp[m] = pw / (2 * span + 1);
                }
            }
        }
        const double floorTap = 2e-4 * pw;
        // C_ij = G(k_i - k_j) + nv / n_k, G(dk) = sum_m P_m phase(dk, m)
        std::vector<cd> G(D, cd(0, 0));
        for (int dk = -(Nc - 1); dk <= Nc - 1; dk++)
        {
            cd acc(0, 0);
            for (int m = 0; m < M; m++) {
                if (pdp[m] > 0) {
                    acc += pdp[m] * phaseOf(dk, m);
                }
            }
            G[dk + Nc - 1] = acc;
        }
        std::vector<cd> C((size_t) n * n, cd(0, 0));
        for (int i = 0; i < n; i++)
        {
            for (int j = 0; j <= i; j++) {
                C[(size_t) i * n + j] = G[ks[i] - ks[j] + Nc - 1];
            }
            C[(size_t) i * n + i] += nv / nk[ks[i]];
        }
        std::vector<cd> Ci;
        hermitianInverse(C, n, Ci);
        // v = C^-1 h; N(dk) = sum over pairs with k_i - k_j = dk of Ci_ij
        std::vector<cd> v(n, cd(0, 0)), Nd(D, cd(0, 0));
        for (int i = 0; i < n; i++)
        {
            const cd *row = &Ci[(size_t) i * n];
            cd acc(0, 0);
            for (int j = 0; j < n; j++)
            {
                acc += row[j] * hk[ks[j]];
                Nd[ks[i] - ks[j] + Nc - 1] += row[j];
            }
            v[i] = acc;
        }
        std::vector<double> P(M);
        for (int m = 0; m < M; m++)
        {
            // d = a^H C^-1 h and q = a^H C^-1 a, a_i = conj(e_m[k_i])
            const cf *e = &ex[(size_t) m * Nc];
            cd d(0, 0), q(0, 0);
            for (int i = 0; i < n; i++) {
                d += cd(e[ks[i]]) * v[i];
            }
            for (int dk = -(Nc - 1); dk <= Nc - 1; dk++) {
                q += Nd[dk + Nc - 1] * std::conj(phaseOf(dk, m));
            }
            const double Pm = pdp[m];
            P[m] = std::norm(Pm * d) + std::max(0.0, Pm - Pm * Pm * q.real());
        }
        for (int m = 0; m < M; m++) {
            pdp[m] = std::max(floorTap, havePdp ? 0.5 * pdp[m] + 0.5 * P[m] : P[m]);
        }
    }

    //! The EM step over the whole frame's pilots with each tap a process in time (the studies' sbl2D,
    //! grouped by carrier difference)
    void sbl2D(const std::vector<cf>& obs, double nv, double pw)
    {
        const int Nc = lay.Nc, M = 2 * Half + 1, D = 2 * Nc - 1;
        const std::vector<int>& pc = lay.pilots;
        const int n = (int) pc.size();
        const double Ts = (double) Nsym / Fs;
        std::vector<double> tau(L);
        for (int dl = 0; dl < L; dl++) {
            tau[dl] = std::exp(-2.0 * M_PI * M_PI * sigma * sigma * (dl * Ts) * (dl * Ts));
        }
        auto T = [&](int i, int j) { return tau[std::abs(pc[i] / Nc - pc[j] / Nc)]; };
        double trT2 = 0;
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                trT2 += T(i, j) * T(i, j);
            }
        }
        const double rank = (double) n * n / trT2;
        // the even taps' powers, their odd neighbours' halves added
        std::vector<double> Pe(M, 0.0);
        for (int m = 0; m < M; m++) {
            if (!((m - Half) & 1)) {
                Pe[m] = pdp[m] + (m > 0 ? 0.5 * pdp[m - 1] : 0.0) + (m + 1 < M ? 0.5 * pdp[m + 1] : 0.0);
            }
        }
        std::vector<cd> G(D, cd(0, 0));
        for (int dk = -(Nc - 1); dk <= Nc - 1; dk++)
        {
            cd acc(0, 0);
            for (int m = 0; m < M; m++) {
                if ((pdp[m] > 0) && !((m - Half) & 1)) {
                    acc += Pe[m] * phaseOf(dk, m);
                }
            }
            G[dk + Nc - 1] = acc;
        }
        // C_ij = T_ij G(k_i - k_j) + nv
        std::vector<cd> C((size_t) n * n, cd(0, 0));
        for (int i = 0; i < n; i++)
        {
            for (int j = 0; j <= i; j++) {
                C[(size_t) i * n + j] = T(i, j) * G[pc[i] % Nc - pc[j] % Nc + Nc - 1];
            }
            C[(size_t) i * n + i] += nv;
        }
        std::vector<cd> Ci;
        hermitianInverse(C, n, Ci);
        // v = C^-1 y; Q(dk) = sum conj(v_i) v_j T_ij and N(dk) = sum Ci_ij T_ij over pairs with k_i - k_j = dk
        std::vector<cd> v(n, cd(0, 0));
        for (int i = 0; i < n; i++)
        {
            const cd *row = &Ci[(size_t) i * n];
            cd acc(0, 0);
            for (int j = 0; j < n; j++) {
                acc += row[j] * cd(obs[pc[j]]);
            }
            v[i] = acc;
        }
        std::vector<cd> Qd(D, cd(0, 0)), Nd(D, cd(0, 0));
        for (int i = 0; i < n; i++)
        {
            const cd *row = &Ci[(size_t) i * n];
            const cd cvi = std::conj(v[i]);
            for (int j = 0; j < n; j++)
            {
                const double t = T(i, j);
                const int dk = pc[i] % Nc - pc[j] % Nc + Nc - 1;
                Qd[dk] += cvi * v[j] * t;
                Nd[dk] += row[j] * t;
            }
        }
        std::vector<double> P(M, 0.0);
        for (int m = 0; m < M; m++)
        {
            if (((m - Half) & 1) || (Pe[m] <= 0)) {
                continue;
            }
            // quad = sum conj(v_i) v_j T_ij e^{-j 2 pi (f_i - f_j) d / Fs}; tr = sum Ci_ij T_ij e^{+j 2 pi (f_i - f_j) d / Fs}
            double quad = 0, tr = 0;
            for (int dk = -(Nc - 1); dk <= Nc - 1; dk++)
            {
                const cd ph = phaseOf(dk, m);
                quad += (Qd[dk + Nc - 1] * ph).real();
                tr += (Nd[dk + Nc - 1] * std::conj(ph)).real();
            }
            const double Pm = Pe[m];
            P[m] = std::max(0.0, Pm + Pm * Pm / rank * (quad - tr));
        }
        std::vector<double> Q(M, 0.0);
        for (int m = 0; m < M; m++)
        {
            if ((m - Half) & 1) {
                continue;
            }
            Q[m] += 0.5 * P[m];
            if (m > 0) {
                Q[m - 1] += 0.25 * P[m];
            }
            if (m + 1 < M) {
                Q[m + 1] += 0.25 * P[m];
            }
        }
        const double floorTap = 2e-4 * pw;
        for (int m = 0; m < M; m++) {
            pdp[m] = std::max(floorTap, havePdp ? 0.5 * pdp[m] + 0.5 * Q[m] : Q[m]);
        }
    }

#ifdef ANGEL61_RX_PROFILE
    //! One EM step of sparse Bayesian learning of the tap powers from each carrier's mean pilot
    void sblStepRef(const std::vector<cd>& hk, const std::vector<int>& nk, double nv, double pw)
    {
        const int Nc = lay.Nc, M = 2 * Half + 1;
        std::vector<int> ks;
        for (int k = 0; k < Nc; k++) {
            if (nk[k]) {
                ks.push_back(k);
            }
        }
        const int n = (int) ks.size();
        if (!havePdp)
        {
            pdp.assign(M, 0.0);
            const int span = Ncp / 2;
            for (int m = 0; m < M; m++) {
                if (std::abs(m - Half) <= span) {
                    pdp[m] = pw / (2 * span + 1);
                }
            }
        }
        const double floorTap = 2e-4 * pw;
        std::vector<cd> C((size_t) n * n, cd(0, 0));
        for (int m = 0; m < M; m++)
        {
            if (pdp[m] <= 0) {
                continue;
            }
            const cf *e = &ex[(size_t) m * Nc];
            for (int i = 0; i < n; i++)
            {
                const cd ei = std::conj(cd(e[ks[i]])) * pdp[m];
                for (int j = 0; j <= i; j++) {
                    C[(size_t) i * n + j] += ei * cd(e[ks[j]]);
                }
            }
        }
        for (int i = 0; i < n; i++) {
            C[(size_t) i * n + i] += nv / nk[ks[i]];
        }
        for (int i = 0; i < n; i++)
        {
            for (int j = 0; j <= i; j++)
            {
                cd s = C[(size_t) i * n + j];
                for (int t = 0; t < j; t++) {
                    s -= C[(size_t) i * n + t] * std::conj(C[(size_t) j * n + t]);
                }
                if (i == j) {
                    C[(size_t) i * n + i] = cd(std::sqrt(std::max(1e-12, s.real())), 0);
                } else {
                    C[(size_t) i * n + j] = s / C[(size_t) j * n + j].real();
                }
            }
        }
        auto fwd = [&](std::vector<cd>& v) {
            for (int i = 0; i < n; i++)
            {
                cd acc = v[i];
                for (int t = 0; t < i; t++) {
                    acc -= C[(size_t) i * n + t] * v[t];
                }
                v[i] = acc / C[(size_t) i * n + i].real();
            }
        };
        std::vector<cd> u(n), w(n);
        for (int i = 0; i < n; i++) {
            u[i] = hk[ks[i]];
        }
        fwd(u);
        std::vector<double> P(M);
        for (int m = 0; m < M; m++)
        {
            const cf *e = &ex[(size_t) m * Nc];
            for (int i = 0; i < n; i++) {
                w[i] = std::conj(cd(e[ks[i]]));
            }
            fwd(w);
            double q = 0;
            cd d(0, 0);
            for (int i = 0; i < n; i++)
            {
                q += std::norm(w[i]);
                d += std::conj(w[i]) * u[i];
            }
            const double Pm = pdp[m];
            P[m] = std::norm(Pm * d) + std::max(0.0, Pm - Pm * Pm * q);
        }
        for (int m = 0; m < M; m++) {
            pdp[m] = std::max(floorTap, havePdp ? 0.5 * pdp[m] + 0.5 * P[m] : P[m]);
        }
    }

    //! The EM step over the whole frame's pilots with each tap a process in time (two-dimensional SBL)
    void sbl2DRef(const std::vector<cf>& obs, double nv, double pw)
    {
        const int Nc = lay.Nc, M = 2 * Half + 1;
        const std::vector<int>& pc = lay.pilots;
        const int n = (int) pc.size();
        const double Ts = (double) Nsym / Fs;
        std::vector<double> T((size_t) n * n);
        double trT = 0, trT2 = 0;
        for (int i = 0; i < n; i++)
        {
            for (int j = 0; j < n; j++)
            {
                const double dt = (pc[i] / Nc - pc[j] / Nc) * Ts;
                const double t = std::exp(-2.0 * M_PI * M_PI * sigma * sigma * dt * dt);
                T[(size_t) i * n + j] = t;
                trT2 += t * t;
            }
            trT += 1.0;
        }
        const double rank = trT * trT / trT2;
        std::vector<cd> C((size_t) n * n, cd(0, 0));
        for (int m = 0; m < M; m++)
        {
            if ((pdp[m] <= 0) || ((m - Half) & 1)) {
                continue;
            }
            const double pm = pdp[m] + (m > 0 ? 0.5 * pdp[m - 1] : 0.0) + (m + 1 < M ? 0.5 * pdp[m + 1] : 0.0);
            const cf *e = &ex[(size_t) m * Nc];
            for (int i = 0; i < n; i++)
            {
                const cd ai = std::conj(cd(e[pc[i] % Nc])) * pm;
                for (int j = 0; j <= i; j++) {
                    C[(size_t) i * n + j] += ai * cd(e[pc[j] % Nc]) * T[(size_t) i * n + j];
                }
            }
        }
        for (int i = 0; i < n; i++) {
            C[(size_t) i * n + i] += nv;
        }
        std::vector<cd> Lc((size_t) n * n, cd(0, 0));
        for (int i = 0; i < n; i++)
        {
            for (int j = 0; j <= i; j++)
            {
                cd sum = C[(size_t) i * n + j];
                for (int t = 0; t < j; t++) {
                    sum -= Lc[(size_t) i * n + t] * std::conj(Lc[(size_t) j * n + t]);
                }
                if (i == j) {
                    Lc[(size_t) i * n + i] = cd(std::sqrt(std::max(1e-12, sum.real())), 0);
                } else {
                    Lc[(size_t) i * n + j] = sum / Lc[(size_t) j * n + j].real();
                }
            }
        }
        std::vector<cd> Li((size_t) n * n, cd(0, 0));
        for (int j = 0; j < n; j++)
        {
            Li[(size_t) j * n + j] = cd(1.0 / Lc[(size_t) j * n + j].real(), 0);
            for (int i = j + 1; i < n; i++)
            {
                cd acc(0, 0);
                for (int t = j; t < i; t++) {
                    acc -= Lc[(size_t) i * n + t] * Li[(size_t) t * n + j];
                }
                Li[(size_t) i * n + j] = acc / Lc[(size_t) i * n + i].real();
            }
        }
        std::vector<cd> Ci((size_t) n * n, cd(0, 0));
        for (int i = 0; i < n; i++)
        {
            for (int j = 0; j <= i; j++)
            {
                cd acc(0, 0);
                for (int t = i; t < n; t++) {
                    acc += std::conj(Li[(size_t) t * n + i]) * Li[(size_t) t * n + j];
                }
                Ci[(size_t) i * n + j] = acc;
                Ci[(size_t) j * n + i] = std::conj(acc);
            }
        }
        std::vector<cd> v(n, cd(0, 0)), z(n), a(n);
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                v[i] += Ci[(size_t) i * n + j] * cd(obs[pc[j]]);
            }
        }
        std::vector<double> P(M, 0.0);
        for (int m = 0; m < M; m++)
        {
            if ((m - Half) & 1) {
                continue;
            }
            const double Pm = pdp[m] + (m > 0 ? 0.5 * pdp[m - 1] : 0.0) + (m + 1 < M ? 0.5 * pdp[m + 1] : 0.0);
            if (Pm <= 0) {
                continue;
            }
            const cf *e = &ex[(size_t) m * Nc];
            for (int i = 0; i < n; i++)
            {
                a[i] = std::conj(cd(e[pc[i] % Nc]));
                z[i] = std::conj(a[i]) * v[i];
            }
            double quad = 0, tr = 0;
            for (int i = 0; i < n; i++)
            {
                cd qi(0, 0), ti(0, 0);
                for (int j = 0; j < n; j++)
                {
                    const double t = T[(size_t) i * n + j];
                    qi += t * z[j];
                    ti += Ci[(size_t) i * n + j] * a[j] * t;
                }
                quad += (std::conj(z[i]) * qi).real();
                tr += (std::conj(a[i]) * ti).real();
            }
            P[m] = std::max(0.0, Pm + Pm * Pm / rank * (quad - tr));
        }
        std::vector<double> Q(M, 0.0);
        for (int m = 0; m < M; m++)
        {
            if ((m - Half) & 1) {
                continue;
            }
            Q[m] += 0.5 * P[m];
            if (m > 0) {
                Q[m - 1] += 0.25 * P[m];
            }
            if (m + 1 < M) {
                Q[m + 1] += 0.25 * P[m];
            }
        }
        const double floorTap = 2e-4 * pw;
        for (int m = 0; m < M; m++) {
            pdp[m] = std::max(floorTap, havePdp ? 0.5 * pdp[m] + 0.5 * Q[m] : Q[m]);
        }
    }

#endif

    /**
     * A lag's frequency, its phase taken about the shorter lags' estimate `f`: a lag aliases beyond
     * +-1 / (2 lag Ts), +-0.9 Hz at lag 23, and at 1 Hz of spread the cells' rule reaches past lag 20.
     * Read as it stood, a 4.7 Hz error after an acquisition wrapped from lag 5 and the long lags, the
     * heaviest, held the estimate near zero: the loop never pulled in (results section 44)
     */
    static double unwrapped(const cd& s, int lag, double Ts, double f)
    {
        const double about = TwoPi * f * lag * Ts;
        return f - std::arg(s * std::polar(1.0, about)) / (TwoPi * lag * Ts);
    }

    //! The residual frequency in this frame's pairs, in Hz
    bool residual(double& f) const
    {
        if (cellsResidual) {
            return residualCells(f);
        }
        const double Ts = (double) Nsym / Fs;
        double num = 0, den = 0;
        for (int lag = 1; (lag <= 10) && (lag < (int) fsuv.size()); lag++)
        {
            if ((fnn[lag] < 4) || (std::abs(fsuv[lag]) <= 0)) {
                continue;
            }
            const double w = std::abs(fsuv[lag]) * lag * lag;
            num += w * unwrapped(fsuv[lag], lag, Ts, den > 0 ? num / den : 0.0);
            den += w;
        }
        if (den <= 0) {
            return false;
        }
        f = num / den;
        return true;
    }

    /**
     * The residual frequency from updateCells' pairs: the lags from 1 up to the first whose
     * correlation is below 0.2, each lag's phase weighted by its precision, pairs x rho^2 / (1 -
     * rho^2) x lag^2, rho with the noise in (the phase's variance goes as (1 - rho^2) / (n rho^2), the
     * frequency's as that over lag^2). The pilots' rule weights every lag to 10 by its sum's
     * magnitude x lag^2: at 10 Hz of spread the lags past 3 carry no correlation, their phases are
     * noise and lag 10 aliases beyond +-2 Hz, and the carrier wandered 2.7 to 3.3 Hz RMS (0.24 on the
     * poor channel).
     */
    bool residualCells(double& f) const
    {
        const double Ts = (double) Nsym / Fs;
        double num = 0.0, den = 0.0;
        for (int lag = 1; lag < (int) fsuv.size(); lag++)
        {
            if ((nn[lag] < 32) || (fnn[lag] < 4)) {
                continue;
            }
            const double r = std::min(0.999, std::abs(suv[lag]) / std::sqrt(std::max(1e-12, ruu[lag] * rvv[lag])));
            if (r < 0.2) {
                break;
            }
            if (std::abs(fsuv[lag]) <= 0) {
                continue;
            }
            const double w = fnn[lag] * r * r / (1.0 - r * r) * lag * lag;
            num += w * unwrapped(fsuv[lag], lag, Ts, den > 0 ? num / den : 0.0);
            den += w;
        }
        if (den <= 0) {
            return false;
        }
        f = num / den;
        return true;
    }

    /**
     * A path the prefix correlation shows at about `pos` samples and the profile lacks: taps within
     * `width` of it raised to a share of the power, in the profile and its long run, for the
     * learning to confirm or prune (MacKay's update settles it in a frame or two)
     */
    void seed(double pos, int width)
    {
        const int c = (int) std::lround(pos) + Half;
        const double v = SeedShare * power / (2 * width + 1);
        for (int m = std::max(0, c - width); m <= std::min(2 * Half, c + width); m++)
        {
            pdp[m] = std::max(pdp[m], v);
            if (!pdpSlow.empty()) {
                pdpSlow[m] = std::max(pdpSlow[m], v);
            }
        }
    }

    //! Move the profile after a timing step of `samples`
    void shift(int samples)
    {
        auto moved = [samples](const std::vector<double>& in) {
            std::vector<double> p(in.size(), 0.0);
            for (int m = 0; m < (int) in.size(); m++)
            {
                const int s = m - samples;
                if ((s >= 0) && (s < (int) in.size())) {
                    p[s] = in[m];
                }
            }
            for (double& v : p) {
                v = std::max(v, 1e-6);
            }
            return p;
        };
        pdp = moved(pdp);
        if (!pdpSlow.empty()) {
            pdpSlow = moved(pdpSlow);
        }
        for (int c = 0; c < lay.cells; c++)
        {
            const cf rot = cf(std::polar(1.0, TwoPi * lay.carrierHz(c % lay.Nc) * samples / Fs));
            prev[c] *= rot;
            prevG[c] *= rot;
        }
    }

    //! The profile's extent above a tenth of its peak, samples from the reference: the long run's
    //! when the cells' learning keeps one
    void extent(int& lo, int& hi) const
    {
        const std::vector<double>& pr = pdpSlow.empty() ? pdp : pdpSlow;
        double mx = 0;
        for (double v : pr) {
            mx = std::max(mx, v);
        }
        lo = 0;
        hi = 0;
        if (mx <= 0) {
            return;
        }
        lo = Half;
        hi = -Half;
        for (int m = 0; m < (int) pr.size(); m++)
        {
            if (pr[m] >= 0.1 * mx)
            {
                lo = std::min(lo, m - Half);
                hi = std::max(hi, m - Half);
            }
        }
    }
};

// ---- acquisition (A2, A5) ----------------------------------------------------------------------

struct AcqEst { int64_t start; int p; double f; double signal; double total; double coherence; double fill; };

/** Symbol timing and the frequency within a carrier from the cyclic prefix, then the frame phase and
 *  whole carriers from the pilot diagonal, then the fine frequency from same-carrier pilot pairs.
 *  y[o ..] must hold obsFrames frames and a symbol. signal and total: the prefix correlation's power
 *  and the samples' mean power (the SNR at acquisition). */
AcqEst acqCore(const RxLayout& lay, const cf *y, size_t ylen, size_t o, int obsFrames)
{
    const int Nc = lay.Nc;
    const int symbols = obsFrames * L;
    // The prefix's correlation at every phase: sum over the symbols of sum_{j < Ncp} y[n + j] y*[n + j + N],
    // n = o + phi + i Nsym, through a running sum of y[n] y*[n + N] (the same samples as a dot product per
    // phase and symbol, 64 times fewer products: acquisition runs every half frame while searching)
    const size_t span = (size_t) (symbols - 1) * Nsym + Ncp;
    std::vector<cd> run(span + 1);
    run[0] = cd(0, 0);
    for (size_t n = 0; n < span; n++) {
        run[n + 1] = run[n] + cd(y[o + n] * std::conj(y[o + n + N]));
    }
    std::vector<cd> gam(Nsym, cd(0, 0));
    for (int phi = 0; phi < Nsym; phi++)
    {
        cd acc(0, 0);
        for (int i = 0; i < symbols - 1; i++)
        {
            const size_t n = (size_t) phi + (size_t) i * Nsym;
            acc += run[n + Ncp] - run[n];
        }
        gam[phi] = acc;
    }
    int best = 0;
    double bestV = -1;
    for (int phi = 0; phi < Nsym; phi++)
    {
        double v = 0;
        for (int t = -Ncp / 2; t <= Ncp / 2; t++) {
            v += std::abs(gam[(phi + t + Nsym) % Nsym]);
        }
        if (v > bestV)
        {
            bestV = v;
            best = phi;
        }
    }
    // the carriers sit on odd multiples of half the spacing, so the prefix is the negated end of the symbol
    const double fFrac = -std::arg(-gam[best]) / (TwoPi * N / Fs);
    // the prefix correlation of the first frame against the last's: under 1 when the signal starts
    // within the window (a key-up after noise); 1 for a window of one frame
    cd first(0, 0), last(0, 0);
    for (int i = 0; i < symbols - 1; i++)
    {
        const size_t n = (size_t) best + (size_t) i * Nsym;
        if (i < L) {
            first += run[n + Ncp] - run[n];
        } else if (i >= symbols - L) {
            last += run[n + Ncp] - run[n];
        }
    }
    const double fill = obsFrames < 2 ? 1.0 : std::abs(last) > 0.0 ? (std::abs(first) / L) / (std::abs(last) / (L - 1)) : 0.0;
    double total = 0;
    for (int i = 0; i < symbols * Nsym; i++) {
        total += std::norm(y[o + i]);
    }
    total /= symbols * Nsym;
    const double signal = std::abs(gam[best]) / ((double) (symbols - 1) * Ncp);
    // The timing: the middle of the correlation's extent above 30% of its peak (each path's a
    // triangle 64 samples either side of it), within the learning's window either side of the
    // strongest part. The strongest part alone centres on one path, and NVIS's second, 7 ms (112
    // samples) on, is then beyond the learning's +-5 ms and never learned: 68% of frames were lost
    // at any SNR. Detection, the frame phase, the frequency, fill and signal stay the strongest
    // part's: the diagonal's metric taken at the centred timing too gave noise a second place to
    // pass (HF-WB: 7 acquisitions in 10 h of noise against 2)
    int centre = 0;
    {
        const int T = 2 * Learner::Half;
        double mx = 0.0;
        for (int t = -T; t <= T; t++) {
            mx = std::max(mx, std::abs(gam[(best + t + 2 * Nsym) % Nsym]));
        }
        int lo = 0, hi = 0;
        for (int t = -T; t <= T; t++)
        {
            if (std::abs(gam[(best + t + 2 * Nsym) % Nsym]) >= 0.3 * mx)
            {
                lo = std::min(lo, t);
                hi = std::max(hi, t);
            }
        }
        centre = std::max(-(Learner::Half - Ncp / 2), std::min(Learner::Half - Ncp / 2, (int) std::lround((lo + hi) / 2.0 - CpBias)));
    }
    int64_t start = (int64_t) o + best;
    if (start - (int64_t) o > Nsym / 2) {
        start -= Nsym;
    }
    if (start < Nsym / 2) {
        start += Nsym;
    }

    std::vector<cf> r(ylen);
    {
        const size_t i0 = (size_t) std::max<int64_t>(0, start - Nsym);
        const size_t i1 = std::min(ylen, (size_t) (start + (int64_t) (symbols + 1) * Nsym));
        for (size_t i = i0; i < i1; i++) {
            r[i] = y[i] * cf(std::polar(1.0, -TwoPi * fFrac * (double) i / Fs));
        }
    }
    const int KE = Nc + 2 * ExtraCarriers;
    std::vector<cf> Yv((size_t) symbols * KE);
    const int woff = -Ncp / 2;
    for (int i = 0; i < symbols; i++)
    {
        const size_t n0 = (size_t) start + (size_t) i * Nsym + Ncp;
        demodulateSymbol(lay, &r[n0], woff, -ExtraCarriers, Nc + ExtraCarriers, &Yv[(size_t) i * KE]);
    }
    auto Yat = [&](int i, int k) { return Yv[(size_t) i * KE + k + ExtraCarriers]; };

    double bestM = -1;
    int bestP = 0, bestB = 0, bestPairs = 0;
    cd bestD(0, 0), bestU(0, 0);
    for (int b = -ExtraCarriers; b <= ExtraCarriers; b++)
    {
        for (int p = 0; p < L; p++)
        {
            cd D(0, 0), U(0, 0);
            int pairs = 0;
            for (int i = 0; i + 1 < symbols; i++)
            {
                const int l = (i + p) % L, l2 = (i + 1 + p) % L;
                for (int k = 0; k + 1 < Nc; k++)
                {
                    if ((lay.kind[l * Nc + k] != 1) || (lay.kind[l2 * Nc + k + 1] != 1)) {
                        continue;
                    }
                    const cf u = Yat(i, k + b) * std::conj(lay.pilotValue[l * Nc + k]);
                    const cf v = Yat(i + 1, k + 1 + b) * std::conj(lay.pilotValue[l2 * Nc + k + 1]);
                    const cd uv = cd(u * std::conj(v));
                    D += uv;
                    U += uv / (std::abs(uv) + 1e-30);   // each pair a unit phasor: the detection metric
                    pairs++;
                }
            }
            const double metric = pairs ? std::abs(D) / std::sqrt((double) pairs) : 0;
            if (metric > bestM)
            {
                bestM = metric;
                bestP = p;
                bestB = b;
                bestD = D;
                bestU = U;
                bestPairs = pairs;
            }
        }
    }
    const double f2 = fFrac + bestB * lay.df - std::arg(bestD * std::polar(1.0, TwoPi * bestB * lay.df * Nsym / Fs)) / (TwoPi * Nsym / Fs);

    double num = 0.0, den = 0.0;
    for (int lag = 1; (lag <= Df) && (lag < symbols); lag++)
    {
        cd D3(0, 0);
        for (int i = 0; i + lag < symbols; i++)
        {
            const int l = (i + bestP) % L, l2 = (i + lag + bestP) % L;
            for (int k = 0; k < Nc; k++)
            {
                if ((lay.kind[l * Nc + k] != 1) || (lay.kind[l2 * Nc + k] != 1)) {
                    continue;
                }
                const cf u = Yat(i, k + bestB) * std::conj(lay.pilotValue[l * Nc + k]);
                const cf v = Yat(i + lag, k + bestB) * std::conj(lay.pilotValue[l2 * Nc + k]);
                D3 += cd(u * std::conj(v));
            }
        }
        if (std::abs(D3) <= 0.0) {
            continue;
        }
        D3 *= std::polar(1.0, TwoPi * (f2 - fFrac) * lag * Nsym / Fs);
        const double w = std::abs(D3) * lag * lag;
        num += w * (-std::arg(D3) / (TwoPi * lag * Nsym / Fs));
        den += w;
    }
    const double f3 = f2 + (den > 0.0 ? num / den : 0.0);
    // Detection: the pairs' unit phasors' mean. A signal's pairs agree in phase; noise's do not
    // (about 1 / sqrt(pairs)), and a steady carrier, which makes the few pairs beside it agree
    // strongly, moves it by only their share (weighting the pairs by their amplitude, it passed
    // 0.95 on a carrier 30 dB over the noise)
    return AcqEst{ start + centre, bestP, f3, signal, total, bestPairs ? std::abs(bestU) / bestPairs : 0.0, fill };
}

// ---- likelihoods, in DVAngel61's bit order ----------------------------------------------------

inline float lse(float a, float b) { return std::max(a, b) + std::log1p(std::exp(-std::fabs(a - b))); }

//! LLRs (positive for 0) of an equalised cell z with complex noise variance v: QPSK bits 0 and 1 the
//! signs of I and Q; 16-QAM bits 0 and 1 the signs, 2 and 3 the magnitudes (1 the inner level)
void cellLLR(cf z, float v, int bps, float *llr)
{
    v = std::max(v, 1e-9f);
    if (bps == 2)
    {
        llr[0] = 2.0f * 1.41421356f * z.real() / v;
        llr[1] = 2.0f * 1.41421356f * z.imag() / v;
        return;
    }
    for (int dim = 0; dim < 2; dim++)
    {
        const float x = dim == 0 ? z.real() : z.imag();
        const float m1p = -(x - QamA) * (x - QamA) / v, m3p = -(x - 3 * QamA) * (x - 3 * QamA) / v;
        const float m1n = -(x + QamA) * (x + QamA) / v, m3n = -(x + 3 * QamA) * (x + 3 * QamA) / v;
        llr[dim] = lse(m1p, m3p) - lse(m1n, m3n);
        llr[2 + dim] = lse(m3p, m3n) - lse(m1p, m1n);
    }
}

// ---- decimation 48 -> 16 kS/s ------------------------------------------------------------------

struct Decimator
{
    std::vector<float> h;
    std::vector<cf> hist;
    int phase = 0;
    size_t pos = 0;

    Decimator()
    {
        // Blackman-windowed sinc, cutoff 4 kHz at 48 kS/s, unit DC gain: the HF-WB band (+-2.8 kHz)
        // passes flat, and what folds from beyond 12 kHz lands outside it
        const int taps = 73, mid = taps / 2;
        const double fc = 4000.0 / 48000.0;
        h.resize(taps);
        double sum = 0;
        for (int i = 0; i < taps; i++)
        {
            const double x = i - mid;
            const double s = x == 0 ? 2 * fc : std::sin(TwoPi * fc * x) / (M_PI * x);
            const double w = 0.42 - 0.5 * std::cos(TwoPi * i / (taps - 1)) + 0.08 * std::cos(2 * TwoPi * i / (taps - 1));
            h[i] = (float) (s * w);
            sum += s * w;
        }
        for (float& v : h) {
            v = (float) (v / sum);
        }
        reset();
    }
    void reset()
    {
        hist.assign(2 * h.size(), cf(0, 0));
        pos = 0;
        phase = 0;
    }
    static int delay() { return 36; }     // input samples
    void process(const cf *x, int n, std::vector<cf>& out)
    {
        const size_t taps = h.size();
        for (int i = 0; i < n; i++)
        {
            hist[pos] = hist[pos + taps] = x[i];
            pos = (pos + 1) % taps;
            if (++phase == Decimation)
            {
                phase = 0;
                cf acc(0, 0);
                const cf *hp = &hist[pos];
                for (size_t t = 0; t < taps; t++) {
                    acc += h[taps - 1 - t] * hp[t];
                }
                out.push_back(acc);
            }
        }
    }
};

// ---- the channel filter at 16 kS/s --------------------------------------------------------------

/**
 * The profile's channel: flat to 1325 Hz (HF-NB) or 2850 Hz (HF-WB), past the edge carriers' main
 * lobes with a 50 Hz carrier offset, and 60 dB down from 1450 or 3250 Hz, where the next channel's
 * carriers start with HF-NB channels 2.7 kHz apart and HF-WB 6 kHz: a Kaiser-windowed sinc, linear
 * phase (465 and 147 taps). The decimator alone passed to 4 kHz, so an adjacent channel's OFDM, with
 * the same 4 ms prefix, reached the prefix correlations (acquisition locked to its timing, the guard
 * took it for a path) and leaked into the edge carriers: HF-NB lost 29% of its frames beside a signal
 * of its own level and every frame beside one 20 dB up. Stopping at 1550 Hz left the next channel's
 * first carriers in the transition: 38% lost 30 dB under a neighbour with no spill of its own; 80 dB
 * rather than 60 mattered only beyond 50 dB, where a real neighbour's own spill (31 dB down in HF-NB's
 * next channel) is already the limit (results section 45)
 */
struct ChannelFilter
{
    std::vector<float> h;
    std::vector<cf> hist;
    size_t pos = 0;

    static double besselI0(double x)
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 50; k++)
        {
            term *= (x / (2.0 * k)) * (x / (2.0 * k));
            sum += term;
            if (term < 1e-12 * sum) {
                break;
            }
        }
        return sum;
    }

    explicit ChannelFilter(DVAngel61::Profile profile)
    {
        const bool wide = profile == DVAngel61::ProfileHFWB;
        const double pass = wide ? 2850.0 : 1325.0, stop = wide ? 3250.0 : 1450.0;
        const double atten = 60.0, beta = 0.1102 * (atten - 8.7);
        int taps = (int) std::ceil((atten - 8.0) / (2.285 * TwoPi * (stop - pass) / Fs)) + 1;
        taps |= 1;
        const int mid = taps / 2;
        const double fc = (pass + stop) / 2.0 / Fs;
        h.resize(taps);
        double sum = 0.0;
        for (int i = 0; i < taps; i++)
        {
            const double x = i - mid;
            const double sinc = x == 0 ? 2.0 * fc : std::sin(TwoPi * fc * x) / (M_PI * x);
            const double t = (double) x / mid;
            const double w = besselI0(beta * std::sqrt(std::max(0.0, 1.0 - t * t))) / besselI0(beta);
            h[i] = (float) (sinc * w);
            sum += sinc * w;
        }
        for (float& v : h) {
            v = (float) (v / sum);
        }
        reset();
    }
    void reset()
    {
        hist.assign(2 * h.size(), cf(0, 0));
        pos = 0;
    }
    int delay() const { return (int) h.size() / 2; }     // samples at 16 kS/s
    //! In place; symmetric taps, the pairs folded
    void process(cf *x, int n)
    {
        const size_t taps = h.size(), mid = taps / 2;
        for (int i = 0; i < n; i++)
        {
            hist[pos] = hist[pos + taps] = x[i];
            pos = (pos + 1) % taps;
            const cf *hp = &hist[pos];         // oldest first
            float re = h[mid] * hp[mid].real(), im = h[mid] * hp[mid].imag();
            for (size_t t = 0; t < mid; t++)
            {
                const cf a = hp[t] + hp[taps - 1 - t];
                re += h[t] * a.real();
                im += h[t] * a.imag();
            }
            x[i] = cf(re, im);
        }
    }
};

} // namespace

// ---- the receiver ---------------------------------------------------------------------------------

struct DVAngel61OFDM::Receiver::Impl
{
    DVAngel61::Profile profile;
    RxLayout lay;
    Decimator dec;
    ChannelFilter chf;
    std::vector<cf> r;                  // the decimated, gain-scaled input: sample rBase + i
    int64_t rBase = 0;
    float gain = 1.0f;                  // input scale: the channel's power about 1
    bool locked = false;
    int64_t q = 0;                      // the useful start of the next frame's symbol 0 (delay-centred)
    double fhat = 0.0, phiA = 0.0;
    int sinceAcquire = 0;               // frames since the last acquisition
    std::vector<cf> headerEst, payloadEst, headerRef;   // the last estimate's channel at the header's and payload's cells, the header sent
    bool iciPass = false;               // estimate() on a frame with its interference cancelled

    /**
     * Inter-carrier interference (results section 45): at 10 Hz of Doppler spread the channel moves
     * within a symbol's 20 ms and each carrier leaks into its neighbours, a floor near an SINR of 15
     * dB. With the channel frozen within each symbol high-latitude HF-WB 9.0k at 22 dB lost 0.02% of
     * its frames against 11%. First order: a carrier's channel changing by S over the window leaks
     * c_d S X into the carrier d below it, c_d = 1 / (N (e^{j 2 pi d / N} - 1)) (about 1 / (j 2 pi d)).
     * S from the estimates at the previous and next symbols (one side at the frame's ends), X the
     * cells sent: the pilots, the header, the payload re-encoded from the decoder's codeword, at
     * their gain alpha. The two carriers either side: more added the slopes' errors (four: 2.2%
     * lost against 1.5; eight: 3.7%). Returns Y less the leakage.
     */
    std::vector<cf> cancelIci(const std::vector<cf>& Y, const std::vector<cf>& payloadX, double alpha) const
    {
        const int Nc = lay.Nc;
        std::vector<cf> H(lay.cells), X(lay.cells);
        for (int c : lay.pilots)
        {
            H[c] = Y[c] * std::conj(lay.pilotValue[c]);
            X[c] = lay.pilotValue[c];
        }
        for (size_t h = 0; h < lay.headers.size(); h++)
        {
            H[lay.headers[h]] = headerEst[h];
            X[lay.headers[h]] = headerRef[h] * (float) alpha;
        }
        for (size_t p = 0; p < lay.payload.size(); p++)
        {
            H[lay.payload[p]] = payloadEst[p];
            X[lay.payload[p]] = payloadX[p] * (float) alpha;
        }
        // each cell's slope over the window times what it sent
        std::vector<cf> SX(lay.cells);
        for (int l = 0; l < L; l++)
        {
            const int la = l > 0 ? l - 1 : l, lb = l < L - 1 ? l + 1 : l;
            const float g = (float) ((double) N / Nsym / (lb - la));
            for (int k = 0; k < Nc; k++) {
                SX[l * Nc + k] = (H[lb * Nc + k] - H[la * Nc + k]) * g * X[l * Nc + k];
            }
        }
        cf c[2 * IciCarriers + 1];
        for (int d = -IciCarriers; d <= IciCarriers; d++) {
            c[d + IciCarriers] = d == 0 ? cf(0, 0) : cf(1.0 / ((double) N * (std::polar(1.0, TwoPi * d / N) - 1.0)));
        }
        std::vector<cf> out(Y);
        for (int l = 0; l < L; l++)
        {
            for (int k = 0; k < Nc; k++)
            {
                cf acc(0, 0);
                for (int d = -IciCarriers; d <= IciCarriers; d++)
                {
                    const int j = k + d;
                    if ((d != 0) && (j >= 0) && (j < Nc)) {
                        acc += c[d + IciCarriers] * SX[l * Nc + j];
                    }
                }
                out[l * Nc + k] -= acc;
            }
        }
        return out;
    }
    int64_t nA = 0;
    int badRun = 0;
    bool confirmed = false;             // a header accepted since the last acquisition
    int unconfirmed = 0;                // frames since the last acquisition while not confirmed
    int acceptedSince = 0;              // headers accepted since the last acquisition
    bool lastSeen = false;              // an accepted header's last-frame flag since the last confirmed frame
    int acquisitions = 0;
    int64_t acquireFrom = 0;
    Learner ln;
    // estimator
    double nvEff = 0.1;                 // noise and interference a cell, measured
    // the transmitter's peak-reduction error (setDistortionModel): per constellation (QPSK, 16-QAM)
    int dmodel = 15;                    // all of it (setDistortionModel)
    bool cellLearning = true;           // the learner takes every known cell (setCellLearning), else the pilots alone
    bool scopeEnabled = false;          // setScopeEnabled
    std::vector<cf> scope;              // equalised payload cells for the constellation display
    // the detection threshold (setDetectionThreshold): just over the metric's largest on 16 000
    // windows of noise alone, and of noise with a carrier 10 to 50 dB over it (HF-NB 0.354 / 0.375,
    // HF-WB 0.240 / 0.228); tier 0's windows at its thresholds are over it but for 0.1% on the poor
    // channel (angel61_hf_results.txt section 33)
    double detectThreshold = 0.0;
    std::vector<DVAngel61OFDM::Receiver::AcquisitionAttempt> attempts;
    int refusedWindows = 0;
    double n0 = 0.1;                    // the channel's noise a cell, measured on the header without the distortion
    double alphaEst[2] = { 1.0, 1.0 };  // the data cells' gain against the pilots, measured on the headers
    bool alphaSeen[2] = { false, false };  // measured since the acquisition
    int lastC = 0;                      // the constellation of the last accepted frame
    double noiseLLR() const { return (dmodel & DVAngel61OFDM::Receiver::DistortionVariance) ? n0 : nvEff; }
    double noiseDesign() const { return (dmodel & DVAngel61OFDM::Receiver::DistortionDesignN0) ? n0 : nvEff; }
    double alphaOf(int c) const { return (dmodel & DVAngel61OFDM::Receiver::DistortionAlpha) ? alphaEst[c] : 1.0; }
    /**
     * The uncorrelated error (d against alpha x), header and payload cells. Clipping's error goes with
     * its shrink: after the reference peak reduction kd is 0.19 to 0.25 (1 - alpha) on header cells and
     * 0.23 to 0.30 (1 - alpha) on payload cells in both constellations and profiles, so it follows the
     * measured gain, and a transmitter without peak reduction (alpha 1) has none. Without the measured
     * gain, the reference transmitter's
     */
    double kdHeader(int c) const
    {
        if (dmodel & DVAngel61OFDM::Receiver::DistortionAlpha) {
            return 0.22 * std::max(0.0, 1.0 - alphaEst[c]);
        }
        static const double t[2][2] = { { 0.033, 0.0093 }, { 0.036, 0.0102 } };
        return t[profile == DVAngel61::ProfileHFWB ? 1 : 0][c];
    }
    double kdPayload(int c) const
    {
        if (dmodel & DVAngel61OFDM::Receiver::DistortionAlpha) {
            return 0.26 * std::max(0.0, 1.0 - alphaEst[c]);
        }
        static const double t[2][2] = { { 0.038, 0.0103 }, { 0.041, 0.0115 } };
        return t[profile == DVAngel61::ProfileHFWB ? 1 : 0][c];
    }
    static double alphaPrior(int c) { return c ? 0.95 : 0.86; }
    double nvDesign = -1.0;             // the fixed design's noise
    std::vector<int> anchors1, anchors2;
    Fit r1, r2, r3;                     // fixed (4 ms, 2 Hz): pilots -> header, pilots + header -> payload, pilots -> payload
    Fit d1, d2, d3;
    bool dReady = false;
    CorrTab robTab, delayTab;           // the designs' correlations, for the pilot-only payload fits on demand
    bool r3Ready = false, d3Ready = false;
    int since = 0;
    double designSigma = 0.0;          // the learnt Doppler the delay design was solved for
    std::vector<double> cpProf;         // the prefix correlation's magnitude about the timing (+-CpSpan), over about 8 frames
    int cpFrames = 0;
    int refused = 0;
    // frames
    int nextCounter = -1;
    // the data group being received
    bool groupActive = false;
    int groupType = 0;
    int groupLast = -1;                 // index of the last frame of the group seen
    DVAngel61::Header groupAny;
    int groupFourthLast = -1;
    std::vector<float> groupLLR;

    explicit Impl(DVAngel61::Profile p) : profile(p), lay(p), chf(p), ln(lay)
    {
        detectThreshold = p == DVAngel61::ProfileHFWB ? 0.25 : 0.38;
        anchors1 = lay.pilots;
        anchors2 = lay.pilots;
        anchors2.insert(anchors2.end(), lay.headers.begin(), lay.headers.end());
    }

    void reset()
    {
        dec.reset();
        chf.reset();
        r.clear();
        rBase = 0;
        gain = 1.0f;
        locked = false;
        badRun = 0;
        acquireFrom = 0;
        ln.reset();
        cpProf.assign(2 * CpSpan + 1, 0.0);
        cpFrames = 0;
        nvEff = 0.1;
        nvDesign = -1.0;
        dReady = false;
        since = 0;
        nextCounter = -1;
        groupActive = false;
    }

    void buildFixed(double nv)
    {
        RX_TIME(5);
        // the fixed design: any delay within the prefix, 2 Hz of Doppler spread (2 sigma)
        CorrTab rob = tabUniform(lay, -Ncp / 2.0 / Fs, Ncp / 2.0 / Fs, 1.0, 1.0);
        std::vector<int> payload = lay.payload;
        r1 = fitTable(lay, anchors1, lay.headers, rob, nv);
        r2 = fitTable(lay, anchors2, payload, rob, nv);
        robTab = rob;
        r3Ready = false;
        nvDesign = nv;
    }

    void refreshDelay()
    {
        RX_TIME(4);
        // The learned profile's taps above 3% of its peak, and the learned Doppler with a margin.
        // The pilots' learning: a floor of 2% of the power over the prefix, for a path a sample or
        // two from where the profile has it (without any floor the poor channel's tier 0 floored at
        // 3.6% lost). The cells' learning: each tap's delay uncertain by CellTapWidth (a delay
        // uniform over the width multiplies its frequency correlation by sinc(dk df width)), and
        // only a token floor (1e-3) within FloorAround samples of its taps. A path's delay falls
        // between samples (the sample clock, a real channel), and two one-sample taps model it to
        // -17 dB at HF-WB's band edge (-40 at HF-NB's): the 2% floor near the taps had covered for
        // it. With the width and no floor HF-WB 9.0k at 3 ms and 10 Hz reached 10% at 22.5 dB (12.4%
        // lost at 23.2 before), NVIS's HF-WB 6.0k 23.4 (24.7); a 0.5% floor as well changed
        // nothing (+-0.2 dB)
        double mx = 0;
        for (double v : ln.pdp) {
            mx = std::max(mx, v);
        }
        const double sigma = std::min(15.0, std::max(0.05, DesignMargin * ln.sigma + DesignOffset));
        designSigma = ln.sigma;
        const double floorTotal = (cellLearning ? 0.0 : 0.02 * ln.power) + 1e-3;
        const double width = cellLearning ? CellTapWidth / Fs : 0.0;
        const int span = Ncp / 2;
        const int M = (int) ln.pdp.size();
        std::vector<char> floored(M, 0);
        for (int m = 0; m < M; m++)
        {
            if (!cellLearning)
            {
                floored[m] = std::abs(m - Learner::Half) <= span;
            }
            else if (ln.pdp[m] >= 0.03 * mx)
            {
                for (int j = std::max(0, m - FloorAround); j <= std::min(M - 1, m + FloorAround); j++) {
                    floored[j] = 1;
                }
            }
        }
        const int flooredTaps = (int) std::count(floored.begin(), floored.end(), 1);
        std::vector<Tap> taps;
        for (int m = 0; m < M; m++)
        {
            const int d = m - Learner::Half;
            double p = ln.pdp[m] >= 0.03 * mx ? ln.pdp[m] : 0.0;
            if (floored[m]) {
                p += floorTotal / std::max(1, flooredTaps);
            }
            if (p > 0) {
                taps.push_back({ d / Fs, p, sigma, width });
            }
        }
        CorrTab t = tabTaps(lay, taps);
        d1 = fitTable(lay, anchors1, lay.headers, t, nvDesign);
        d2 = fitTable(lay, anchors2, lay.payload, t, nvDesign);
        delayTab = t;
        d3Ready = false;
        dReady = sane(d1) && sane(d2);
#ifdef ANGEL61_RX_PROFILE
        static bool checked = false;
        if (!checked)
        {
            // the reorganised fit against the studies' own, once
            checked = true;
            Fit ref = fitTableRef(lay, anchors2, lay.payload, t, nvDesign);
            float dw = 0.0f, dm = 0.0f, mw = 0.0f;
            for (size_t e = 0; e < ref.w.size(); e++) { dw = std::max(dw, std::abs(ref.w[e] - d2.w[e])); mw = std::max(mw, std::abs(ref.w[e])); }
            for (size_t e = 0; e < ref.mse.size(); e++) dm = std::max(dm, std::fabs(ref.mse[e] - d2.mse[e]));
            printf("fit check: largest weight %.3g, largest weight difference %.3g, largest MSE difference %.3g\n", mw, dw, dm);
        }
#endif
        if (!dReady) {
            refused++;
        }
        since = 0;
    }

    void ensure(int64_t end, bool& enough) const
    {
        enough = rBase + (int64_t) r.size() >= end;
    }

    const cf *at(int64_t n) const { return &r[(size_t) (n - rBase)]; }

    //! Demodulate the frame whose symbol 0 has its delay-centred useful start at q, derotated
    void demodFrame(std::vector<cf>& Y) const
    {
        std::vector<cf> buf(N);
        const float scale = std::sqrt((float) lay.Nc) / N;
        const int woff = -Ncp / 2 + Wola / 2;
        Y.resize(lay.cells);
        for (int l = 0; l < L; l++)
        {
            const int64_t w = q + (int64_t) l * Nsym + woff;
            for (int m = 0; m < N; m++)
            {
                const double ph = phiA + TwoPi * fhat * (double) (w + m - nA) / Fs;
                buf[m] = *at(w + m) * cf(std::polar(1.0, -std::fmod(ph, TwoPi)));
            }
            for (int k = 0; k < lay.Nc; k++) {
                Y[(size_t) l * lay.Nc + k] = dotConj(buf.data(), lay.table(k) + Ncp + woff, N) * scale;
            }
        }
    }

    //! Move the prefix correlation after a timing step of `samples`, as Learner::shift
    void cpShift(int samples)
    {
        std::vector<double> p(cpProf.size(), 0.0);
        for (int m = 0; m < (int) cpProf.size(); m++)
        {
            const int t = m - samples;
            if ((t >= 0) && (t < (int) cpProf.size())) {
                p[(size_t) t] = cpProf[(size_t) m];
            }
        }
        cpProf.swap(p);
    }

    /**
     * The guard: the outermost paths the prefix correlation shows (its extent less a path's own
     * half-width either side). One more than CpGuard beyond the profile's extent [lo, hi] is a path
     * the learning has not seen: c becomes the middle of the two extents' union (true), to be
     * followed up to CpGuard a frame (an acquisition on one of NVIS's paths, the other 112 samples
     * on, is centred in 4 frames rather than 19), and the learning is seeded there: without it the
     * profile, one path only, pulled the timing back while the other faded, and learnt it 20 frames
     * on. The guard only widens: at 1 Hz one path fades for seconds, the correlation's extent
     * shrinks onto the other, and following its middle threw away a path the profile still had
     * (runs of 10 to 60 frames lost on NVIS's 16-QAM tier).
     */
    bool guardTiming(int lo, int hi, double& c)
    {
        int clo, chi;
        if (!cpExtent(clo, chi)) {
            return false;
        }
        const double plo = clo - CpBias + CpHalfWidth, phi = chi - CpBias - CpHalfWidth;
        const bool below = plo < lo - CpGuard, above = phi > hi + CpGuard;
        if (!below && !above) {
            return false;
        }
        c = (std::min((double) lo, plo) + std::max((double) hi, phi)) / 2.0;
        if (cellLearning)
        {
            if (below) {
                ln.seed(plo, SeedWidth);
            }
            if (above) {
                ln.seed(phi, SeedWidth);
            }
        }
        return true;
    }

    /**
     * Step the timing by adj samples: the next frame's start, the profiles, and the delay design
     * moved with them (shiftDesign: exact, and a pass over the weights where solving it again cost
     * about 17 ms on HF-WB; with the timing rounded a single-path channel steps a sample most frames)
     */
    void stepTiming(int adj, int64_t& qNext)
    {
        if (adj == 0) {
            return;
        }
        qNext += adj;
        ln.shift(adj);
        cpShift(adj);
        if (dReady) {
            shiftDesign(adj);
        } else {
            refreshDelay();
        }
    }

    /**
     * The delay design after a timing step of `samples`, without solving it again: the profile's
     * taps move by -samples, the prior's correlation between carriers ka and kt turns by
     * e^{j 2 pi (kt - ka) df samples / Fs}, and so do the Wiener weights (w' = D_t w D_a^H), exactly;
     * the MSEs are unchanged (against a design solved again on the moved profile: weights within
     * 1e-4). The pilot-only payload fit is left to be rebuilt on demand
     */
    void shiftDesign(int samples)
    {
        const int Nc = lay.Nc;
        const double step = TwoPi * lay.df * samples / Fs;
        std::vector<cf> rot(2 * Nc - 1);
        for (int dk = -(Nc - 1); dk <= Nc - 1; dk++) {
            rot[dk + Nc - 1] = cf(std::polar(1.0, step * dk));
        }
        auto shiftFit = [&](Fit& f, const std::vector<int>& anchors, const std::vector<int>& targets) {
            if (f.w.size() != targets.size() * (size_t) f.n) {
                return;
            }
            for (size_t t = 0; t < targets.size(); t++)
            {
                const int kt = targets[t] % Nc;
                cf *w = &f.w[t * f.n];
                for (int i = 0; i < f.n; i++) {
                    w[i] *= rot[kt - anchors[i] % Nc + Nc - 1];
                }
            }
        };
        shiftFit(d1, anchors1, lay.headers);
        shiftFit(d2, anchors2, lay.payload);
        if (delayTab.Nc == Nc)
        {
            for (size_t r = 0; r < delayTab.t.size(); r += 2 * Nc - 1) {
                for (int i = 0; i < 2 * Nc - 1; i++) {
                    delayTab.t[r + i] *= cd(rot[i]);
                }
            }
        }
        d3Ready = false;
    }

    /**
     * The prefix correlation about the timing, over the frame's symbols, into the long run: the
     * guard on the learning's window. A path the learning has never seen (beyond its +-5 ms of the
     * timing: NVIS's second path, 7 ms on, when acquisition's window had it in a fade) cannot
     * move the profile's extent, and the timing would stay on the one path for good. Its prefix is
     * in the correlation whatever the learning knows. t = 0 at the delay-centred start (the
     * acquisition's reference), each symbol's correlation summed coherently (the residual carrier
     * turns each by the same phase), the frames' magnitudes averaged.
     */
    void cpUpdate()
    {
        const int W = 2 * CpSpan + 1;
        std::vector<cd> run((size_t) W + Ncp);
        std::vector<cd> sum((size_t) W, cd(0, 0));
        for (int l = 0; l < L; l++)
        {
            const int64_t b = q - Ncp + (int64_t) l * Nsym - CpSpan;
            if ((b < rBase) || (b + W + Ncp + N > rBase + (int64_t) r.size())) {
                continue;                               // the first frame after an acquisition early in r
            }
            run[0] = cd(0, 0);
            for (int n = 0; n + 1 < W + Ncp; n++) {
                run[(size_t) n + 1] = run[(size_t) n] + cd(*at(b + n) * std::conj(*at(b + n + N)));
            }
            for (int t = 0; t < W; t++) {
                sum[(size_t) t] += run[(size_t) t + Ncp] - run[(size_t) t];
            }
        }
        const double w = std::max(0.125, 1.0 / (cpFrames + 1));
        for (int t = 0; t < W; t++) {
            cpProf[(size_t) t] = (1.0 - w) * cpProf[(size_t) t] + w * std::abs(sum[(size_t) t]);
        }
        cpFrames++;
    }

    //! The prefix correlation's extent above 30% of its peak: false until it has 4 frames
    bool cpExtent(int& lo, int& hi) const
    {
        if (cpFrames < 4) {
            return false;
        }
        double mx = 0.0;
        for (double v : cpProf) {
            mx = std::max(mx, v);
        }
        lo = CpSpan;
        hi = -CpSpan;
        for (int t = 0; t < (int) cpProf.size(); t++)
        {
            if (cpProf[(size_t) t] >= 0.3 * mx)
            {
                lo = std::min(lo, t - CpSpan);
                hi = std::max(hi, t - CpSpan);
            }
        }
        return mx > 0.0;
    }

    //! Acquire on the two frames from `from`; false when the window is below the detection threshold
    bool acquire(int64_t from)
    {
        RX_TIME(6);
        AcqEst a = acqCore(lay, r.data(), r.size(), (size_t) (from + Nsym - rBase), 2);
        // A window the signal starts in, its first frame under half filled, is left for the next one
        // half a frame on: that one still holds the transmission's first frame, and its estimates come
        // from at least three quarters of signal rather than a few symbols' (whose fine frequency could
        // be one of lag 8's aliases, 5.2 Hz out, and the lock then never decoded)
        const bool detected = (a.coherence >= detectThreshold) && (a.fill >= AcqFirstFrameFill);
        if (attempts.size() < 4096)
        {
            DVAngel61OFDM::Receiver::AcquisitionAttempt at;
            at.m_coherence = (float) a.coherence;
            at.m_prefix = (float) (a.signal / std::max(1e-12, a.total));
            at.m_locked = detected;
            at.m_sample = from * Decimation;
            attempts.push_back(at);
        }
        if (!detected)
        {
            refusedWindows++;
            return false;
        }
        const int64_t s0 = a.start + rBase;
        // the first frame starting within the frames acquisition looked at: they are still in r
        int i = 0;
        while ((i + a.p) % L != 0) {
            i++;
        }
        q = s0 + (int64_t) i * Nsym + Ncp;
        fhat = a.f;
        phiA = 0.0;
        nA = 0;
        // gain: the signal's power (the prefix correlation) to 1; the noise a cell from the rest
        const double signal = std::max(1e-12, a.signal);
        const double noise = std::max(signal * 1e-3, a.total - signal);
        gain *= (float) (1.0 / std::sqrt(signal));
        for (cf& v : r) {
            v *= (float) (1.0 / std::sqrt(signal));
        }
        nvEff = noise / signal * lay.Nc / N;
        n0 = nvEff;
        lastC = 0;
        for (int c = 0; c < 2; c++)
        {
            alphaEst[c] = (dmodel & DVAngel61OFDM::Receiver::DistortionAlpha) ? alphaPrior(c) : 1.0;
            alphaSeen[c] = false;
        }
        buildFixed(noiseDesign());
        ln.reset();
        cpProf.assign(2 * CpSpan + 1, 0.0);
        cpFrames = 0;
        dReady = false;
        since = 0;
        badRun = 0;
        confirmed = false;
        unconfirmed = 0;
        acceptedSince = 0;
        lastSeen = false;
        nextCounter = -1;
        groupActive = false;
        locked = true;
        acquisitions++;
        sinceAcquire = 0;
        return true;
    }

    //! Header LLRs through the current design, the decided header, and the payload LLRs through the pilots and (when accepted) the header
    void estimate(const std::vector<cf>& Y, bool& accepted, DVAngel61::Header& header, int bpsIfLost, std::vector<float>& llr, int& bps)
    {
        const Fit& f1 = dReady ? d1 : r1;
        std::vector<cf> obs(anchors1.size());
        for (size_t i = 0; i < obs.size(); i++) {
            obs[i] = Y[anchors1[i]] * std::conj(lay.pilotValue[anchors1[i]]);
        }
        float hllr[DVAngel61::HeaderCodedBits];
        std::fill(hllr, hllr + DVAngel61::HeaderCodedBits, 0.0f);
        std::vector<cf> hEst(lay.headers.size());
        for (size_t h = 0; h < lay.headers.size(); h++)
        {
            cf est(0, 0);
            const cf *w = &f1.w[h * f1.n];
            for (int i = 0; i < f1.n; i++) {
                est += w[i] * obs[i];
            }
            if (std::norm(est) < 1e-12f) {
                est = cf(1e-6f, 0);
            }
            hEst[h] = est;
            float two[2];
            if (dmodel)
            {
                // the frame's constellation is not known yet: the last frame's
                const double a = alphaOf(lastC);
                const double kd = (dmodel & DVAngel61OFDM::Receiver::DistortionVariance) ? kdHeader(lastC) : 0.0;
                const double v = noiseLLR() / (a * a * std::norm(est)) + f1.mse[h] / std::norm(est) + kd;
                cellLLR(Y[lay.headers[h]] / (est * (float) a), (float) v, 2, two);
            }
            else
            {
                cellLLR(Y[lay.headers[h]] / est, (float) (nvEff + f1.mse[h]) / std::norm(est), 2, two);
            }
            hllr[2 * (h % HeaderCells)] += two[0];
            hllr[2 * (h % HeaderCells) + 1] += two[1];
        }
        accepted = DVAngel61::decodeHeader(hllr, header) && (DVAngel61::profileOf(header.m_type) == profile)
            && DVAngel61::isKnownType(header.m_type);
        std::vector<cf> ref(lay.headers.size());
        headerEst = hEst;
        payloadEst.assign(lay.payload.size(), cf(0, 0));
        if (accepted)
        {
            uint8_t bits[DVAngel61::HeaderCodedBits];
            cf symbols[HeaderCells];
            DVAngel61::encodeHeader(header, bits);
            DVAngel61::mapCells(DVAngel61::QPSK, bits, HeaderCells, symbols);
            headerRef.resize(lay.headers.size());
            for (size_t h = 0; h < lay.headers.size(); h++) {
                headerRef[h] = symbols[h % HeaderCells];
            }
            double acc = 0;
            for (size_t h = 0; h < lay.headers.size(); h++)
            {
                ref[h] = symbols[h % HeaderCells];
                acc += std::norm(Y[lay.headers[h]] - hEst[h] * ref[h]) - f1.mse[h];
            }
            // noise and interference measured on the decoded header, smoothed; at most 30 dB below the channel
            const double meas = std::max(1e-3 * ln.power, acc / lay.headers.size());
            nvEff = 0.7 * nvEff + 0.3 * meas;

            // the distortion model: the data cells' gain from the header, then the noise without the
            // gain's error and the reference transmitter's uncorrelated error
            const int c = DVAngel61::constellationOf(header.m_type) == DVAngel61::QAM16 ? 1 : 0;
            double num = 0.0, den = 0.0;
            for (size_t h = 0; h < lay.headers.size(); h++)
            {
                num += (double) (std::conj(hEst[h] * ref[h]) * Y[lay.headers[h]]).real();
                den += std::norm(hEst[h]);
            }
            if ((dmodel & DVAngel61OFDM::Receiver::DistortionAlpha) && (den > 0.0))
            {
                // the first header of a constellation sets it (the transmitter may have no peak reduction)
                const double am = std::max(0.5, std::min(1.2, num / den));
                alphaEst[c] = alphaSeen[c] ? 0.8 * alphaEst[c] + 0.2 * am : am;
                alphaSeen[c] = true;
            }
            const double a = alphaOf(c);
            double acc0 = 0.0;
            for (size_t h = 0; h < lay.headers.size(); h++) {
                acc0 += std::norm(Y[lay.headers[h]] - hEst[h] * ref[h] * (float) a) - f1.mse[h];
            }
            const double meanH2 = den / lay.headers.size();
            const double kd = (dmodel & (DVAngel61OFDM::Receiver::DistortionVariance | DVAngel61OFDM::Receiver::DistortionDesignN0
                | DVAngel61OFDM::Receiver::DistortionSnr)) ? kdHeader(c) : 0.0;
            const double meas0 = std::max(1e-3 * ln.power, acc0 / lay.headers.size() - kd * a * a * meanH2);
            n0 = 0.7 * n0 + 0.3 * meas0;
            lastC = c;
        }
        bps = accepted ? DVAngel61::bitsPerCell(DVAngel61::constellationOf(header.m_type)) : bpsIfLost;
        if (bps == 0) {
            return;
        }
        // the pilot-only payload fit is only for a data group's frame whose header was lost: solved on demand
        if (!accepted && dReady && !d3Ready)
        {
            d3 = fitTable(lay, anchors1, lay.payload, delayTab, nvDesign);
            d3Ready = true;
            if (!sane(d3))
            {
                // refused as the design is: the fixed one's pilot-only fit instead
                refused++;
                d3 = fitTable(lay, anchors1, lay.payload, robTab, nvDesign);
            }
        }
        if (!accepted && !dReady && !r3Ready)
        {
            r3 = fitTable(lay, anchors1, lay.payload, robTab, nvDesign);
            r3Ready = true;
        }
        const Fit& f2 = accepted ? (dReady ? d2 : r2) : (dReady ? d3 : r3);
        const int cP = bps == 4 ? 1 : 0;
        const double aP = alphaOf(cP);
        const double kdP = (dmodel & DVAngel61OFDM::Receiver::DistortionVariance) ? kdPayload(cP) : 0.0;
        std::vector<cf> obs2(f2.n);
        for (int i = 0; i < f2.n; i++)
        {
            const int c = accepted ? anchors2[i] : anchors1[i];
            const bool pilot = lay.kind[c] == 1;
            const cf x = pilot ? lay.pilotValue[c] : ref[i - lay.pilots.size()];
            // a header anchor arrives at the data cells' gain: to the pilots' scale
            obs2[i] = Y[c] * std::conj(x) / (pilot ? 1.0f : (float) aP);
        }
        llr.assign(lay.payload.size() * bps, 0.0f);
        for (size_t p = 0; p < lay.payload.size(); p++)
        {
            cf est(0, 0);
            const cf *w = &f2.w[p * f2.n];
            for (int i = 0; i < f2.n; i++) {
                est += w[i] * obs2[i];
            }
            if (std::norm(est) < 1e-12f) {
                est = cf(1e-6f, 0);
            }
            payloadEst[p] = est;
            const cf z = dmodel ? Y[lay.payload[p]] / (est * (float) aP) : Y[lay.payload[p]] / est;
            if (dmodel)
            {
                const double v = noiseLLR() / (aP * aP * std::norm(est)) + f2.mse[p] / std::norm(est) + kdP;
                cellLLR(z, (float) v, bps, &llr[p * bps]);
            }
            else
            {
                cellLLR(z, (float) (nvEff + f2.mse[p]) / std::norm(est), bps, &llr[p * bps]);
            }
            if (accepted && scopeEnabled && !iciPass && (scope.size() < (size_t) DVAngel61OFDM::Receiver::ScopeSymbols)) {
                scope.push_back(z);
            }
        }
    }

    //! The learner on the frame's known cells: the pilots, and the header's when it was accepted
    void learnCells(const std::vector<cf>& Y, bool accepted, const DVAngel61::Header& header)
    {
        std::vector<cf> g(lay.cells, cf(0, 0));
        std::vector<float> nvc(lay.cells, -1.0f);
        const float nv = (float) noiseDesign();
        for (int c : lay.pilots)
        {
            const cf x = lay.pilotValue[c];
            g[c] = Y[c] * std::conj(x) / std::norm(x);
            nvc[c] = nv / std::norm(x);
        }
        if (accepted)
        {
            // the header's cells arrive at the data cells' gain (peak reduction): to the pilots' scale
            uint8_t bits[DVAngel61::HeaderCodedBits];
            cf symbols[HeaderCells];
            DVAngel61::encodeHeader(header, bits);
            DVAngel61::mapCells(DVAngel61::QPSK, bits, HeaderCells, symbols);
            const float a = (float) alphaOf(DVAngel61::constellationOf(header.m_type) == DVAngel61::QAM16 ? 1 : 0);
            for (size_t h = 0; h < lay.headers.size(); h++)
            {
                const int c = lay.headers[h];
                const cf x = symbols[h % HeaderCells] * a;
                g[c] = Y[c] * std::conj(x) / std::norm(x);
                nvc[c] = nv / std::norm(x);
            }
        }
        ln.updateCells(g, nvc);
    }

    float snrDb() const
    {
        // SNR3k: the channel's power over the noise in 3 kHz (a cell's noise is the sample noise x Nc / N)
        if (dmodel & DVAngel61OFDM::Receiver::DistortionSnr)
        {
            // the channel's noise alone, against the frame's mean power: the pilots at the channel's,
            // the data cells at alpha^2 (1 + kd) of it
            const double a = alphaEst[lastC];
            const double data = (double) (lay.headers.size() + lay.payload.size());
            const double rho = (lay.pilots.size() + data * a * a * (1.0 + kdPayload(lastC))) / (lay.pilots.size() + data);
            const double noiseSample = n0 * N / lay.Nc;
            return (float) (10.0 * std::log10(std::max(1e-9, ln.power * rho) / (noiseSample * 3000.0 / Fs)));
        }
        const double noiseSample = nvEff * N / lay.Nc;
        return (float) (10.0 * std::log10(std::max(1e-9, ln.power) / (noiseSample * 3000.0 / Fs)));
    }

    void decodeGroup(Frame& fr)
    {
        if (!groupActive) {
            return;
        }
        std::vector<uint8_t> block(DVAngel61::infoBits(groupType));
        DVAngel61::Header headers[DVAngel61::GroupFrames];
        int it = -1;
        if (DVAngel61::decodeGroup(groupLLR.data(), groupAny, groupFourthLast, block.data(), headers, &it))
        {
            fr.m_payloadOk = true;
            fr.m_group = true;
            fr.m_block = block;
            std::copy(headers, headers + DVAngel61::GroupFrames, fr.m_groupHeaders);
        }
        fr.m_iterations = it;
        groupActive = false;
    }

    void frame(std::vector<Frame>& frames)
    {
        std::vector<cf> Y;
        {
            RX_TIME(0);
            demodFrame(Y);
        }
        if (!cellLearning)
        {
            RX_TIME(1);
            ln.update(Y.data(), noiseDesign());
        }

        const int expected = nextCounter;
        const bool inGroup = groupActive && (expected >= 0) && (DVAngel61::groupIndex(expected) > groupLast);
        const int bpsIfLost = inGroup ? DVAngel61::bitsPerCell(DVAngel61::constellationOf(groupType)) : 0;
        bool accepted = false;
        DVAngel61::Header header;
        std::vector<float> llr;
        int bps = 0;
        {
            RX_TIME(2);
            estimate(Y, accepted, header, bpsIfLost, llr, bps);
        }
        if (cellLearning)
        {
            RX_TIME(1);
            learnCells(Y, accepted, header);
        }
        cpUpdate();
        RX_TIME(3);

        Frame fr;
        fr.m_headerOk = accepted;
        fr.m_header = header;
        fr.m_sample = (q - Ncp - chf.delay()) * Decimation - Decimator::delay();

        if (accepted)
        {
            nextCounter = (header.m_counter + 1) & 0x3F;
            if (DVAngel61::isGroupData(header.m_type))
            {
                const int gi = DVAngel61::groupIndex(header.m_counter);
                if (groupActive && ((gi <= groupLast) || (header.m_type != groupType))) {
                    decodeGroup(fr);                          // a new group: the last one ends here
                }
                if (!groupActive)
                {
                    groupActive = true;
                    groupType = header.m_type;
                    groupLLR.assign(DVAngel61::codedBits(groupType), 0.0f);
                    groupFourthLast = -1;
                }
                groupAny = header;
                groupLast = gi;
                std::copy(llr.begin(), llr.end(), groupLLR.begin() + (size_t) gi * llr.size());
                if (gi == DVAngel61::GroupFrames - 1) {
                    groupFourthLast = header.m_lastFrame ? 1 : 0;
                }
            }
            else
            {
                if (groupActive) {
                    decodeGroup(fr);
                }
                std::vector<uint8_t> block(DVAngel61::infoBits(header.m_type));
                std::vector<uint8_t> codeword(DVAngel61::codedBits(header.m_type));
                int it = -1;
                fr.m_payloadOk = DVAngel61::decodePayload(llr.data(), header, block.data(), &it, codeword.data());
                // At 10 Hz a failed payload again with the interference its decode implies cancelled
                // (cancelIci), the frame estimated again from the cleaned cells: high-latitude HF-WB 9.0k
                // at 22 dB lost 1.5% of its frames against 11%, HF-WB 6.0k at 14 dB 7.4 against 12.6,
                // low-latitude HF-WB 6.0k at 24 dB 5.5 against 14.1 (results section 45)
                for (int pass = 0; (pass < IciPasses) && !fr.m_payloadOk && (ln.sigma >= IciMinSigma); pass++)
                {
                    std::vector<cf> px(lay.payload.size());
                    DVAngel61::payloadCells(codeword.data(), header.m_counter, header.m_type, px.data());
                    const std::vector<cf> Yc = cancelIci(Y, px, alphaOf(bps == 4 ? 1 : 0));
                    // its noise and the distortion model's gains measured on it, then put back: the
                    // next frames' are the channel's
                    const double kept[4] = { nvEff, n0, alphaEst[0], alphaEst[1] };
                    const int keptC = lastC;
                    bool acceptedC = false;
                    DVAngel61::Header headerC;
                    std::vector<float> llrC;
                    int bpsC = 0;
                    iciPass = true;
                    estimate(Yc, acceptedC, headerC, 0, llrC, bpsC);
                    iciPass = false;
                    nvEff = kept[0];
                    n0 = kept[1];
                    alphaEst[0] = kept[2];
                    alphaEst[1] = kept[3];
                    lastC = keptC;
                    if (!acceptedC || !(headerC == header) || (bpsC != bps)) {
                        break;
                    }
                    fr.m_payloadOk = DVAngel61::decodePayload(llrC.data(), header, block.data(), &it, codeword.data());
                }
                fr.m_iterations = it;
                if (fr.m_payloadOk)
                {
                    fr.m_block = block;
                    // The channel's bit errors the code corrected: the first decisions against the codeword
                    std::vector<float> coded(codeword.size());
                    DVAngel61::positionsToCodeword(llr.data(), header.m_counter, DVAngel61::codeOf(header.m_type), coded.data());
                    fr.m_codedBits = (int) coded.size();
                    for (size_t i = 0; i < coded.size(); i++) {
                        fr.m_channelErrors += (coded[i] < 0.0f) != (codeword[i] != 0);   // an LLR is positive for a 0
                    }
                }
            }
        }
        else
        {
            if (expected >= 0) {
                nextCounter = (expected + 1) & 0x3F;
            }
            if (inGroup)
            {
                const int gi = DVAngel61::groupIndex(expected);
                std::copy(llr.begin(), llr.end(), groupLLR.begin() + (size_t) gi * llr.size());
                groupLast = gi;
            }
        }

        if (groupActive && (groupLast == DVAngel61::GroupFrames - 1)) {
            decodeGroup(fr);                                  // the group's fourth frame has passed
        }

        // the designs follow the measured noise (rebuilt when it has moved 2 dB); the delay design is
        // re-solved every 4 frames (refused when degenerate: the fixed one stands)
        if (std::fabs(10.0 * std::log10(noiseDesign() / nvDesign)) > 2.0)
        {
            buildFixed(noiseDesign());
            since = DelayRefresh;
        }
        // (and at once when the learnt Doppler has moved by half again from the design's: a design for 5
        // Hz on a 1 Hz channel, or the reverse, waited up to 4 frames, and the noise its fit misjudged was
        // measured, raising the design's noise in turn: results section 44)
        // Both taken at 0.5 Hz or more: below it the Doppler barely moves the design over a frame, and
        // on AWGN the learnt Doppler wanders by more than half again about a few hundredths of a hertz,
        // which re-solved HF-WB's design most frames (10.3 ms a frame against 4: results section 45)
        const bool sigmaMoved = (designSigma > 0.0)
            && (std::fabs(std::log(std::max(0.5, ln.sigma) / std::max(0.5, designSigma))) > std::log(1.5));
        if (!dReady || (++since >= DelayRefresh) || sigmaMoved) {
            refreshDelay();
        }

        // tracking on accepted frames; after 25 without one, acquire again. An acquisition with no
        // header accepted in its first frames was on noise, or on a transmission's first partial
        // frame: acquire again at once rather than holding a false lock for 25 frames. With one
        // header only, after 8: the wide first design passes a header on an acquisition 4 to 5 Hz
        // off at 1 Hz of spread, and none after (NVIS held such locks 25 frames: results section 44)
        int64_t qNext = q + FL;
        if (accepted)
        {
            badRun = 0;
            confirmed = true;
            acceptedSince++;
            if (fr.m_payloadOk) {
                lastSeen = false;
            }
            double df;
            if (ln.residual(df))
            {
                const int64_t nB = qNext - Ncp / 2;
                phiA += TwoPi * fhat * (double) (nB - nA) / Fs;
                nA = nB;
                // up to 2 Hz a measurement in the first frames: an acquisition's error of some hertz
                // pulled in at 0.25 Hz a frame (results section 44)
                const double clip = (sinceAcquire < PullFrames) ? 2.0 : 0.5;
                fhat += 0.5 * std::max(-clip, std::min(clip, df));
            }
            // The extent's middle. The cells' learning: rounded, half a sample's error corrected at
            // once (a 3 ms spread leaves no slack: with a whole sample's dead band HF-WB 9.0k at 3 ms
            // and 10 Hz lost 12.4% at 23.2 dB, now 10.9%); the pilots': half the middle beyond 2
            // samples; up to 3 samples a frame. The guard (guardTiming) may take over. The next
            // frame on the moved profile: the delay design re-solved now
            int lo, hi;
            ln.extent(lo, hi);
            double c = (lo + hi) / 2.0;
            double maxStep = 3.0;
            if (guardTiming(lo, hi, c)) {
                maxStep = CpGuard;
            }
            const double want = cellLearning ? c : ((std::fabs(c) < 2.0) ? 0.0 : 0.5 * c);
            stepTiming((int) std::lround(std::max(-maxStep, std::min(maxStep, want))), qNext);
        }
        else if ((++badRun >= ((acceptedSince < 2) ? WeakHold : ReacquireAfter)) || (!confirmed && (++unconfirmed >= ConfirmWithin)))
        {
            locked = false;
            acquireFrom = qNext - Ncp;
        }
        else if (confirmed)
        {
            // The guard needs no header: a step to a path the learning has not seen can leave the
            // headers failing until the learning has it, and tracking on accepted frames alone then
            // held the timing (NVIS: 16 frames lost after one)
            int lo, hi;
            ln.extent(lo, hi);
            double c;
            if (guardTiming(lo, hi, c)) {
                stepTiming((int) std::lround(std::max((double) -CpGuard, std::min((double) CpGuard, c))), qNext);
            }
        }

        // The transmission's last frame (2.8): the next transmission has its own timing and frequency,
        // so acquire afresh after it rather than hold this lock for 25 frames. Confirmed (the CRC-12
        // covers the flag): at once. A flag on a header whose payload failed is not acted on, but
        // the hold is shortened to ConfirmWithin frames
        const bool lastConfirmed = fr.m_payloadOk && (fr.m_group ? fr.m_groupHeaders[DVAngel61::GroupFrames - 1].m_lastFrame : header.m_lastFrame);
        if (lastConfirmed)
        {
            locked = false;
            acquireFrom = qNext - Ncp;
        }
        else if (accepted && header.m_lastFrame)
        {
            lastSeen = true;
        }
        if (lastSeen && !accepted && locked && (badRun >= ConfirmWithin))
        {
            locked = false;
            acquireFrom = qNext - Ncp;
        }
        q = qNext;

        fr.m_snrDb = snrDb();
        fr.m_frequencyHz = (float) fhat;
        frames.push_back(fr);
        sinceAcquire++;
    }

    void process(const cf *samples, int n, std::vector<Frame>& frames)
    {
        const size_t before = r.size();
        {
            RX_TIME(7);
            dec.process(samples, n, r);
            chf.process(r.data() + before, (int) (r.size() - before));
        }
        for (size_t i = before; i < r.size(); i++) {
            r[i] *= gain;
        }

        for (;;)
        {
            const int64_t end = rBase + (int64_t) r.size();
            if (!locked)
            {
                if (acquireFrom < rBase) {
                    acquireFrom = rBase;
                }
                if (end < acquireFrom + 2LL * FL + 3LL * Nsym + 64) {
                    break;
                }
                if (!acquire(acquireFrom)) {
                    acquireFrom += FL / 2;      // not a signal: half a frame on
                }
                continue;
            }
            if (end < q + FL + Nsym + 64) {
                break;
            }
            frame(frames);
        }

        // keep what the next acquisition or frame can need (the next frame can start beyond the
        // samples received so far: acquisition looks up to three frames ahead)
        const int64_t keep = (locked ? q : acquireFrom) - 2 * Nsym;
        if (keep - rBase > 8 * FL)
        {
            const size_t cut = std::min(r.size(), (size_t) (keep - rBase));
            r.erase(r.begin(), r.begin() + cut);
            rBase += cut;
        }
    }
};

DVAngel61OFDM::Receiver::Receiver(DVAngel61::Profile profile) :
    m_impl(new Impl(profile))
{
}

DVAngel61OFDM::Receiver::~Receiver()
{
    delete m_impl;
}

void DVAngel61OFDM::Receiver::reset()
{
    m_impl->reset();
}

void DVAngel61OFDM::Receiver::process(const std::complex<float> *samples, int n, std::vector<Frame>& frames)
{
    m_impl->process(samples, n, frames);
}

bool DVAngel61OFDM::Receiver::locked() const
{
    return m_impl->locked;
}

void DVAngel61OFDM::Receiver::setDistortionModel(int flags)
{
    m_impl->dmodel = flags;
}

void DVAngel61OFDM::Receiver::setCellLearning(bool enable)
{
    m_impl->cellLearning = enable;
}

void DVAngel61OFDM::Receiver::setScopeEnabled(bool enable)
{
    m_impl->scopeEnabled = enable;
    if (!enable) {
        m_impl->scope.clear();
    }
}

std::vector<std::complex<float>> DVAngel61OFDM::Receiver::takeScopeSymbols()
{
    std::vector<std::complex<float>> symbols;
    symbols.swap(m_impl->scope);
    return symbols;
}

void DVAngel61OFDM::Receiver::setDetectionThreshold(float coherence)
{
    m_impl->detectThreshold = coherence;
}

std::vector<DVAngel61OFDM::Receiver::AcquisitionAttempt> DVAngel61OFDM::Receiver::takeAcquisitionAttempts()
{
    std::vector<AcquisitionAttempt> a;
    a.swap(m_impl->attempts);
    return a;
}

int DVAngel61OFDM::Receiver::acquisitionsRefused() const
{
    return m_impl->refusedWindows;
}

int DVAngel61OFDM::Receiver::acquisitions() const
{
    return m_impl->acquisitions;
}

int DVAngel61OFDM::Receiver::designsRefused() const
{
    return m_impl->refused;
}
