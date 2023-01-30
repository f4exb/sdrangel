///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// This is the code from ft8mon: https://github.com/rtmrtmrtmrtm/ft8mon          //
// written by Robert Morris, AB1HL                                               //
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

//
// An FT8 receiver in C++.
//
// Many ideas and protocol details borrowed from Franke
// and Taylor's WSJT-X code.
//
// Robert Morris, AB1HL
//

#include <stdio.h>
// #include <assert.h>
#include <math.h>
#include <complex>
#include <fftw3.h>
#include <algorithm>
#include <complex>
#include <random>
#include <functional>
#include <map>

#include <QThread>

#include "util.h"
#include "ft8.h"
#include "libldpc.h"
#include "osd.h"

namespace FT8 {

//
// return a Hamming window of length n.
//
std::vector<float> hamming(int n)
{
    std::vector<float> h(n);

    for (int k = 0; k < n; k++) {
        h[k] = 0.54 - 0.46 * cos(2 * M_PI * k / (n - 1.0));
    }

    return h;
}

//
// blackman window
//
std::vector<float> blackman(int n)
{
    std::vector<float> h(n);

    for (int k = 0; k < n; k++) {
        h[k] = 0.42 - 0.5 * cos(2 * M_PI * k / n) + 0.08 * cos(4 * M_PI * k / n);
    }

    return h;
}

//
// symmetric blackman window
//
std::vector<float> sym_blackman(int n)
{
    std::vector<float> h(n);

    for (int k = 0; k < (n / 2) + 1; k++) {
        h[k] = 0.42 - 0.5 * cos(2 * M_PI * k / n) + 0.08 * cos(4 * M_PI * k / n);
    }

    for (int k = n - 1; k >= (n / 2) + 1; --k) {
        h[k] = h[(n - 1) - k];
    }

    return h;
}

//
// blackman-harris window
//
std::vector<float> blackmanharris(int n)
{
    float a0 = 0.35875;
    float a1 = 0.48829;
    float a2 = 0.14128;
    float a3 = 0.01168;
    std::vector<float> h(n);

    for (int k = 0; k < n; k++)
    {
        // symmetric
        h[k] = a0 - a1 * cos(2 * M_PI * k / (n - 1)) + a2 * cos(4 * M_PI * k / (n - 1)) - a3 * cos(6 * M_PI * k / (n - 1));
        // periodic
        // h[k] =
        //  a0
        //  - a1 * cos(2 * M_PI * k / n)
        //  + a2 * cos(4 * M_PI * k / n)
        //  - a3 * cos(6 * M_PI * k / n);
    }

    return h;
}

Stats::Stats(int how, float log_tail, float log_rate) :
    sum_(0),
    finalized_(false),
    how_(how),
    log_tail_(log_tail),
    log_rate_(log_rate)
{}

void Stats::add(float x)
{
    a_.push_back(x);
    sum_ += x;
    finalized_ = false;
}

void Stats::finalize()
{
    finalized_ = true;

    int n = a_.size();
    mean_ = sum_ / n;
    float var = 0;
    float bsum = 0;

    for (int i = 0; i < n; i++)
    {
        float y = a_[i] - mean_;
        var += y * y;
        bsum += fabs(y);
    }

    var /= n;
    stddev_ = sqrt(var);
    b_ = bsum / n;

    // prepare for binary search to find where values lie
    // in the distribution.
    if (how_ != 0 && how_ != 5) {
        std::sort(a_.begin(), a_.end());
    }
}

float Stats::mean()
{
    if (!finalized_) {
        finalize();
    }

    return mean_;
}

float Stats::stddev()
{
    if (!finalized_) {
        finalize();
    }

    return stddev_;
}

// fraction of distribution that's less than x.
// assumes normal distribution.
// this is PHI(x), or the CDF at x,
// or the integral from -infinity
// to x of the PDF.
float Stats::gaussian_problt(float x)
{
    float SDs = (x - mean()) / stddev();
    float frac = 0.5 * (1.0 + erf(SDs / sqrt(2.0)));
    return frac;
}

// https://en.wikipedia.org/wiki/Laplace_distribution
// m and b from page 116 of Mark Owen's Practical Signal Processing.
float Stats::laplace_problt(float x)
{
    float m = mean();
    float cdf;

    if (x < m) {
        cdf = 0.5 * exp((x - m) / b_);
    } else {
        cdf = 1.0 - 0.5 * exp(-(x - m) / b_);
    }

    return cdf;
}

// look into the actual distribution.
float Stats::problt(float x)
{
    if (!finalized_) {
        finalize();
    }

    if (how_ == 0) {
        return gaussian_problt(x);
    }

    if (how_ == 5) {
        return laplace_problt(x);
    }

    // binary search.
    auto it = std::lower_bound(a_.begin(), a_.end(), x);
    int i = it - a_.begin();
    int n = a_.size();

    if (how_ == 1)
    {
        // index into the distribution.
        // works poorly for values that are off the ends
        // of the distribution, since those are all
        // mapped to 0.0 or 1.0, regardless of magnitude.
        return i / (float)n;
    }

    if (how_ == 2)
    {
        // use a kind of logistic regression for
        // values near the edges of the distribution.
        if (i < log_tail_ * n)
        {
            float x0 = a_[(int)(log_tail_ * n)];
            float y = 1.0 / (1.0 + exp(-log_rate_ * (x - x0)));
            // y is 0..0.5
            y /= 5;
            return y;
        }
        else if (i > (1 - log_tail_) * n)
        {
            float x0 = a_[(int)((1 - log_tail_) * n)];
            float y = 1.0 / (1.0 + exp(-log_rate_ * (x - x0)));
            // y is 0.5..1
            // we want (1-log_tail)..1
            y -= 0.5;
            y *= 2;
            y *= log_tail_;
            y += (1 - log_tail_);
            return y;
        }
        else
        {
            return i / (float)n;
        }
    }

    if (how_ == 3)
    {
        // gaussian for values near the edge of the distribution.
        if (i < log_tail_ * n) {
            return gaussian_problt(x);
        } else if (i > (1 - log_tail_) * n) {
            return gaussian_problt(x);
        } else {
            return i / (float)n;
        }
    }

    if (how_ == 4)
    {
        // gaussian for values outside the distribution.
        if (x < a_[0] || x > a_.back()) {
            return gaussian_problt(x);
        } else {
            return i / (float)n;
        }
    }

    return 0;
}

// a-priori probability of each of the 174 LDPC codeword
// bits being one. measured from reconstructed correct
// codewords, into ft8bits, then python bprob.py.
// from ft8-n4
const double FT8::apriori174[] = {
  0.47, 0.32, 0.29, 0.37, 0.52, 0.36, 0.40, 0.42, 0.42, 0.53, 0.44,
  0.44, 0.39, 0.46, 0.39, 0.38, 0.42, 0.43, 0.45, 0.51, 0.42, 0.48,
  0.31, 0.45, 0.47, 0.53, 0.59, 0.41, 0.03, 0.50, 0.30, 0.26, 0.40,
  0.65, 0.34, 0.49, 0.46, 0.49, 0.69, 0.40, 0.45, 0.45, 0.60, 0.46,
  0.43, 0.49, 0.56, 0.45, 0.55, 0.51, 0.46, 0.37, 0.55, 0.52, 0.56,
  0.55, 0.50, 0.01, 0.19, 0.70, 0.88, 0.75, 0.75, 0.74, 0.73, 0.18,
  0.71, 0.35, 0.60, 0.58, 0.36, 0.60, 0.38, 0.50, 0.02, 0.01, 0.98,
  0.48, 0.49, 0.54, 0.50, 0.49, 0.53, 0.50, 0.49, 0.49, 0.51, 0.51,
  0.51, 0.47, 0.50, 0.53, 0.51, 0.46, 0.51, 0.51, 0.48, 0.51, 0.52,
  0.50, 0.52, 0.51, 0.50, 0.49, 0.53, 0.52, 0.50, 0.46, 0.47, 0.48,
  0.52, 0.50, 0.49, 0.51, 0.49, 0.49, 0.50, 0.50, 0.50, 0.50, 0.51,
  0.50, 0.49, 0.49, 0.55, 0.49, 0.51, 0.48, 0.55, 0.49, 0.48, 0.50,
  0.51, 0.50, 0.51, 0.50, 0.51, 0.53, 0.49, 0.54, 0.50, 0.48, 0.49,
  0.46, 0.51, 0.51, 0.52, 0.49, 0.51, 0.49, 0.51, 0.50, 0.49, 0.50,
  0.50, 0.47, 0.49, 0.52, 0.49, 0.51, 0.49, 0.48, 0.52, 0.48, 0.49,
  0.47, 0.50, 0.48, 0.50, 0.49, 0.51, 0.51, 0.51, 0.49,
};

FT8::FT8(
    const std::vector<float> &samples,
    float min_hz,
    float max_hz,
    int start,
    int rate,
    int hints1[],
    int hints2[],
    double deadline,
    double final_deadline,
    CallbackInterface *cb,
    std::vector<cdecode> prevdecs,
    FFTEngine *fftEngine
)
{
    samples_ = samples;
    min_hz_ = min_hz;
    max_hz_ = max_hz;
    prevdecs_ = prevdecs;
    start_ = start;
    rate_ = rate;
    deadline_ = deadline;
    final_deadline_ = final_deadline;
    cb_ = cb;
    down_hz_ = 0;

    for (int i = 0; hints1[i]; i++) {
        hints1_.push_back(hints1[i]);
    }

    for (int i = 0; hints2[i]; i++) {
        hints2_.push_back(hints2[i]);
    }

    hack_size_ = -1;
    hack_data_ = nullptr;
    hack_off_ = -1;
    hack_len_ = -1;
    fftEngine_ = fftEngine;
    npasses_ = 1;
}

FT8::~FT8()
{
}

void FT8::start_work()
{
    go(npasses_);
    emit finished();
}

// strength of costas block of signal with tone 0 at bi0,
// and symbol zero at si0.
float FT8::one_coarse_strength(const FFTEngine::ffts_t &bins, int bi0, int si0)
{
    int costas[] = {3, 1, 4, 0, 6, 5, 2};

    // assert(si0 >= 0 && si0 + 72 + 8 <= (int)bins.size());
    // assert(bi0 >= 0 && bi0 + 8 <= (int)bins[0].size());

    float sig = 0.0;
    float noise = 0.0;

    if (params.coarse_all >= 0)
    {
        for (int si = 0; si < 79; si++)
        {
            float mx;
            int mxi = -1;
            float sum = 0;

            for (int i = 0; i < 8; i++)
            {
                float x = std::abs(bins[si0 + si][bi0 + i]);
                sum += x;

                if (mxi < 0 || x > mx)
                {
                    mxi = i;
                    mx = x;
                }
            }

            if (si >= 0 && si < 7)
            {
                float x = std::abs(bins[si0 + si][bi0 + costas[si - 0]]);
                sig += x;
                noise += sum - x;
            }
            else if (si >= 36 && si < 36 + 7)
            {
                float x = std::abs(bins[si0 + si][bi0 + costas[si - 36]]);
                sig += x;
                noise += sum - x;
            }
            else if (si >= 72 && si < 72 + 7)
            {
                float x = std::abs(bins[si0 + si][bi0 + costas[si - 72]]);
                sig += x;
                noise += sum - x;
            }
            else
            {
                sig += params.coarse_all * mx;
                noise += params.coarse_all * (sum - mx);
            }
        }
    }
    else
    {
        // coarse_all = -1
        // just costas symbols
        for (int si = 0; si < 7; si++)
        {
            for (int bi = 0; bi < 8; bi++)
            {
                float x = 0;
                x += std::abs(bins[si0 + si][bi0 + bi]);
                x += std::abs(bins[si0 + 36 + si][bi0 + bi]);
                x += std::abs(bins[si0 + 72 + si][bi0 + bi]);

                if (bi == costas[si]) {
                    sig += x;
                } else {
                    noise += x;
                }
            }
        }
    }

    if (params.coarse_strength_how == 0) {
        return sig - noise;
    } else if (params.coarse_strength_how == 1) {
        return sig - noise / 7;
    } else if (params.coarse_strength_how == 2) {
        return sig / (noise / 7);
    } else if (params.coarse_strength_how == 3) {
        return sig / (sig + (noise / 7));
    } else if (params.coarse_strength_how == 4) {
        return sig;
    } else if (params.coarse_strength_how == 5) {
        return sig / (sig + noise);
    } else if (params.coarse_strength_how == 6) {
        // this is it.
        return sig / noise;
    } else {
        return 0;
    }
}

// return symbol length in samples at the given rate.
// insist on integer symbol lengths so that we can
// use whole FFT bins.
int FT8::blocksize(int rate)
{
    // FT8 symbol length is 1920 at 12000 samples/second.
    int xblock = (1920*rate) / 12000;
    int block = xblock;
    return block;
}



//
// look for potential psignals by searching FFT bins for Costas symbol
// blocks. returns a vector of candidate positions.
//
std::vector<Strength> FT8::coarse(const FFTEngine::ffts_t &bins, int si0, int si1)
{
    int block = blocksize(rate_);
    int nbins = bins[0].size();
    float bin_hz = rate_ / (float)block;
    int min_bin = min_hz_ / bin_hz;
    int max_bin = max_hz_ / bin_hz;
    std::vector<Strength> strengths;

    for (int bi = min_bin; bi < max_bin && bi + 8 <= nbins; bi++)
    {
        std::vector<Strength> sv;

        for (int si = si0; si < si1 && si + 79 < (int)bins.size(); si++)
        {
            float s = one_coarse_strength(bins, bi, si);
            Strength st;
            st.strength_ = s;
            st.hz_ = bi * 6.25;
            st.off_ = si * block;
            sv.push_back(st);
        }

        if (sv.size() < 1) {
            break;
        }

        // save best ncoarse offsets, but require that they be separated
        // by at least one symbol time.

        std::sort(
            sv.begin(),
            sv.end(),
            [](const Strength &a, const Strength &b) -> bool {
                return a.strength_ > b.strength_;
            }
        );

        strengths.push_back(sv[0]);
        int nn = 1;

        for (int i = 1; nn < params.ncoarse && i < (int)sv.size(); i++)
        {
            if (std::abs(sv[i].off_ - sv[0].off_) > params.ncoarse_blocks * block)
            {
                strengths.push_back(sv[i]);
                nn++;
            }
        }
    }

    return strengths;
}

//
// reduce the sample rate from arate to brate.
// center hz0..hz1 in the new nyquist range.
// but first filter to that range.
// sets delta_hz to hz moved down.
//
std::vector<float> FT8::reduce_rate(
    const std::vector<float> &a,
    float hz0,
    float hz1,
    int arate,
    int brate,
    float &delta_hz
)
{
    // assert(brate < arate);
    // assert(hz1 - hz0 <= brate / 2);

    // the pass band is hz0..hz1
    // stop bands are 0..hz00 and hz11..nyquist.
    float hz00, hz11;
    hz0 = std::max(0.0f, hz0 - params.reduce_extra);
    hz1 = std::min(arate / 2.0f, hz1 + params.reduce_extra);

    if (params.reduce_shoulder > 0)
    {
        hz00 = hz0 - params.reduce_shoulder;
        hz11 = hz1 + params.reduce_shoulder;
    }
    else
    {
        float mid = (hz0 + hz1) / 2;
        hz00 = mid - (brate * params.reduce_factor);
        hz00 = std::min(hz00, hz0);
        hz11 = mid + (brate * params.reduce_factor);
        hz11 = std::max(hz11, hz1);
    }

    int alen = a.size();
    std::vector<std::complex<float>> bins1 = fftEngine_->one_fft(a, 0, alen);
    int nbins1 = bins1.size();
    float bin_hz = arate / (float)alen;

    if (params.reduce_how == 2)
    {
        // band-pass filter the FFT output.
        bins1 = fbandpass(
            bins1,
            bin_hz,
            hz00,
            hz0,
            hz1,
            hz11
        );
    }

    if (params.reduce_how == 3)
    {
        for (int i = 0; i < nbins1; i++)
        {
            if (i < (hz0 / bin_hz)) {
                bins1[i] = 0;
            } else if (i > (hz1 / bin_hz)) {
                bins1[i] = 0;
            }
        }
    }

    // shift down.
    int omid = ((hz0 + hz1) / 2) / bin_hz;
    int nmid = (brate / 4.0) / bin_hz;
    int delta = omid - nmid; // amount to move down
    // assert(delta < nbins1);
    int blen = round(alen * (brate / (float)arate));
    std::vector<std::complex<float>> bbins(blen / 2 + 1);

    for (int i = 0; i < (int)bbins.size(); i++)
    {
        if (delta > 0) {
            bbins[i] = bins1[i + delta];
        } else {
            bbins[i] = bins1[i];
        }
    }

    // use ifft to reduce the rate.
    std::vector<float> vvv = fftEngine_->one_ifft(bbins);
    delta_hz = delta * bin_hz;

    return vvv;
}

void FT8::go(int npasses)
{
    if (0)
    {
        fprintf(stderr, "go: %.0f .. %.0f, %.0f, rate=%d\n",
                min_hz_, max_hz_, max_hz_ - min_hz_, rate_);
    }

    // trim to make samples_ a good size for FFTW.
    int nice_sizes[] = {18000, 18225, 36000, 36450,
                        54000, 54675, 72000, 72900,
                        144000, 145800, 216000, 218700,
                        0};
    int nice = -1;

    for (int i = 0; nice_sizes[i]; i++)
    {
        int sz = nice_sizes[i];

        if (fabs(samples_.size() - sz) < 0.05 * samples_.size())
        {
            nice = sz;
            break;
        }
    }

    if (nice != -1) {
        samples_.resize(nice);
    }

    // assert(min_hz_ >= 0 && max_hz_ + 50 <= rate_ / 2);

    // can we reduce the sample rate?
    int nrate = -1;
    for (int xrate = 100; xrate < rate_; xrate += 100)
    {
        if (xrate < rate_ && (params.oddrate || (rate_ % xrate) == 0))
        {
            if (((max_hz_ - min_hz_) + 50 + 2 * params.go_extra) < params.nyquist * (xrate / 2))
            {
                nrate = xrate;
                break;
            }
        }
    }

    if (params.do_reduce && nrate > 0 && nrate < rate_ * 0.75)
    {
        // filter and reduce the sample rate from rate_ to nrate.

        double t0 = now();
        int osize = samples_.size();

        float delta_hz; // how much it moved down
        samples_ = reduce_rate(
            samples_,
            min_hz_ - 3.1 - params.go_extra,
            max_hz_ + 50 - 3.1 + params.go_extra,
            rate_,
            nrate,
            delta_hz
        );

        double t1 = now();

        if (t1 - t0 > 0.1)
        {
            fprintf(stderr, "reduce oops, size %d -> %d, rate %d -> %d, took %.2f\n",
                    osize,
                    (int)samples_.size(),
                    rate_,
                    nrate,
                    t1 - t0);
        }

        if (0)
        {
            fprintf(stderr, "%.0f..%.0f, range %.0f, rate %d -> %d, delta hz %.0f, %.6f sec\n",
                    min_hz_, max_hz_,
                    max_hz_ - min_hz_,
                    rate_, nrate, delta_hz, t1 - t0);
        }

        if (delta_hz > 0)
        {
            down_hz_ = delta_hz; // to adjust hz for Python.
            min_hz_ -= down_hz_;
            max_hz_ -= down_hz_;

            for (int i = 0; i < (int)prevdecs_.size(); i++)
            {
                prevdecs_[i].hz0 -= delta_hz;
                prevdecs_[i].hz1 -= delta_hz;
            }
        }

        // assert(max_hz_ + 50 < nrate / 2);
        // assert(min_hz_ >= 0);

        float ratio = nrate / (float)rate_;
        rate_ = nrate;
        start_ = round(start_ * ratio);
    }

    int block = blocksize(rate_);

    // start_ is the sample number of 0.5 seconds, the nominal start time.

    // make sure there's at least tplus*rate_ samples after the end.
    if (start_ + params.tplus * rate_ + 79 * block + block > samples_.size())
    {
        int need = start_ + params.tplus * rate_ + 79 * block - samples_.size();

        // round up to a whole second, to ease fft plan caching.
        if ((need % rate_) != 0) {
            need += rate_ - (need % rate_);
        }

        std::default_random_engine generator;
        std::uniform_int_distribution<int> distribution(0, samples_.size() - 1);
        auto rnd = std::bind(distribution, generator);
        std::vector<float> v(need);

        for (int i = 0; i < need; i++)
        {
            // v[i] = 0;
            v[i] = samples_[rnd()];
        }

        samples_.insert(samples_.end(), v.begin(), v.end());
    }

    int si0 = (start_ - params.tminus * rate_) / block;

    if (si0 < 0) {
        si0 = 0;
    }

    int si1 = (start_ + params.tplus * rate_) / block;

    // a copy from which to subtract.
    nsamples_ = samples_;
    int any = 0;

    for (int i = 0; i < (int)prevdecs_.size(); i++)
    {
        auto d = prevdecs_[i];

        if (d.hz0 >= min_hz_ && d.hz0 <= max_hz_)
        {
            // reconstruct correct 79 symbols from LDPC output.
            std::vector<int> re79 = recode(d.bits);

            // fine up hz/off again now that we have more samples
            float best_hz = (d.hz0 + d.hz1) / 2.0;
            float best_off = d.off; // seconds
            search_both_known(
                samples_,
                rate_,
                re79,
                best_hz,
                best_off,
                best_hz,
                best_off
            );

            // subtract from nsamples_.
            subtract(re79, best_hz, best_hz, best_off);
            any += 1;
        }
    }

    if (any) {
        samples_ = nsamples_;
    }

    for (pass_ = 0; pass_ < npasses; pass_++)
    {
        double total_remaining = deadline_ - now();
        double remaining = total_remaining / (npasses - pass_);

        if (pass_ == 0) {
            remaining *= params.pass0_frac;
        }

        double deadline = now() + remaining;
        int new_decodes = 0;
        samples_ = nsamples_;

        std::vector<Strength> order;

        //
        // search coarsely for Costas blocks.
        // in fractions of bins in off and hz.
        //

        // just do this once, re-use for every fractional fft_shift
        // and down_v7_f() to 200 sps.
        std::vector<std::complex<float>> bins = fftEngine_->one_fft(
            samples_, 0, samples_.size());

        for (int hz_frac_i = 0; hz_frac_i < params.coarse_hz_n; hz_frac_i++)
        {
            // shift down by hz_frac
            float hz_frac = hz_frac_i * (6.25 / params.coarse_hz_n);
            std::vector<float> samples1;

            if (hz_frac_i == 0) {
                samples1 = samples_;
            } else {
                samples1 = fft_shift_f(bins, rate_, hz_frac);
            }

            for (int off_frac_i = 0; off_frac_i < params.coarse_off_n; off_frac_i++)
            {
                int off_frac = off_frac_i * (block / params.coarse_off_n);
                FFTEngine::ffts_t bins = fftEngine_->ffts(samples1, off_frac, block);
                std::vector<Strength> oo = coarse(bins, si0, si1);

                for (int i = 0; i < (int)oo.size(); i++)
                {
                    oo[i].hz_ += hz_frac;
                    oo[i].off_ += off_frac;
                }

                order.insert(order.end(), oo.begin(), oo.end());
            }
        }

        //
        // sort strongest-first.
        //
        std::sort(
            order.begin(),
            order.end(),
            [](const Strength &a, const Strength &b) -> bool {
                return a.strength_ > b.strength_;
            }
        );

        char already[2000]; // XXX

        for (int i = 0; i < (int)(sizeof(already) / sizeof(already[0])); i++) {
            already[i] = 0;
        }

        for (int ii = 0; ii < (int)order.size(); ii++)
        {
            double tt = now();

            if (ii > 0 &&
                tt > deadline &&
                (tt > deadline_ || new_decodes >= params.pass_threshold) &&
                (pass_ < npasses - 1 || tt > final_deadline_)
            ) {
                break;
            }

            float hz = order[ii].hz_;

            if (already[(int)round(hz / params.already_hz)]) {
                continue;
            }

            int off = order[ii].off_;
            int ret = one_merge(bins, samples_.size(), hz, off);

            if (ret)
            {
                if (ret == 2) {
                    new_decodes++;
                }

                already[(int)round(hz / params.already_hz)] = 1;
            }
        }
    } // pass
}

//
// what's the strength of the Costas sync blocks of
// the signal starting at hz and off?
//
float FT8::one_strength(const std::vector<float> &samples200, float hz, int off)
{
    int bin0 = round(hz / 6.25);
    int costas[] = {3, 1, 4, 0, 6, 5, 2};
    int starts[] = {0, 36, 72};
    float sig = 0;
    float noise = 0;

    for (int which = 0; which < 3; which++)
    {
        int start = starts[which];

        for (int si = 0; si < 7; si++)
        {
            auto fft = fftEngine_->one_fft(samples200, off + (si + start) * 32, 32);
            for (int bi = 0; bi < 8; bi++)
            {
                float x = std::abs(fft[bin0 + bi]);

                if (bi == costas[si]) {
                    sig += x;
                } else {
                    noise += x;
                }
            }
        }
    }

    if (params.strength_how == 0) {
        return sig - noise;
    } else if (params.strength_how == 1) {
        return sig - noise / 7;
    } else if (params.strength_how == 2) {
        return sig / (noise / 7);
    } else if (params.strength_how == 3) {
        return sig / (sig + (noise / 7));
    } else if (params.strength_how == 4) {
        return sig;
    } else if (params.strength_how == 5) {
        return sig / (sig + noise);
    } else if (params.strength_how == 6) {
        return sig / noise;
    } else {
        return 0;
    }
}

//
// given a complete known signal's symbols in syms,
// how strong is it? used to look for the best
// offset and frequency at which to subtract a
// decoded signal.
//
float FT8::one_strength_known(
    const std::vector<float> &samples,
    int rate,
    const std::vector<int> &syms,
    float hz,
    int off
)
{
    int block = blocksize(rate);
    // assert(syms.size() == 79);
    int bin0 = round(hz / 6.25);
    float sig = 0;
    float noise = 0;
    float sum7 = 0;
    std::complex<float> prev = 0;

    for (int si = 0; si < 79; si += params.known_sparse)
    {
        auto fft = fftEngine_->one_fft(samples, off + si * block, block);

        if (params.known_strength_how == 7)
        {
            std::complex<float> c = fft[bin0 + syms[si]];

            if (si > 0) {
                sum7 += std::abs(c - prev);
            }

            prev = c;
        }
        else
        {
            for (int bi = 0; bi < 8; bi++)
            {
                float x = std::abs(fft[bin0 + bi]);

                if (bi == syms[si]) {
                    sig += x;
                } else {
                    noise += x;
                }
            }
        }
    }

    if (params.known_strength_how == 0) {
        return sig - noise;
    } else if (params.known_strength_how == 1) {
        return sig - noise / 7;
    } else if (params.known_strength_how == 2) {
        return sig / (noise / 7);
    } else if (params.known_strength_how == 3) {
        return sig / (sig + (noise / 7));
    } else if (params.known_strength_how == 4) {
        return sig;
    } else if (params.known_strength_how == 5) {
        return sig / (sig + noise);
    } else if (params.known_strength_how == 6) {
        return sig / noise;
    } else if (params.known_strength_how == 7) {
        return -sum7;
    } else {
        return 0;
    }
}

int FT8::search_time_fine(
    const std::vector<float> &samples200,
    int off0,
    int offN,
    float hz,
    int gran,
    float &str
)
{
    if (off0 < 0) {
        off0 = 0;
    }

    //
    // shift in frequency to put hz at 25.
    // only shift the samples we need, both for speed,
    // and try to always shift down the same number of samples
    // to make it easier to cache fftw plans.
    //
    int len = (offN - off0) + 79 * 32 + 32;

    if (off0 + len > (int)samples200.size())
    {
        // len = samples200.size() - off0;
        // don't provoke random-length FFTs.
        return -1;
    }

    std::vector<float> downsamples200 = shift200(samples200, off0, len, hz);
    int best_off = -1;
    float best_sum = 0.0;

    for (int g = 0; g <= (offN - off0) && g + 79 * 32 <= len; g += gran)
    {
        float sum = one_strength(downsamples200, 25, g);

        if (sum > best_sum || best_off == -1)
        {
            best_off = g;
            best_sum = sum;
        }
    }

    str = best_sum;
    // assert(best_off >= 0);
    return off0 + best_off;
}

int FT8::search_time_fine_known(
    const std::vector<std::complex<float>> &bins,
    int rate,
    const std::vector<int> &syms,
    int off0,
    int offN,
    float hz,
    int gran,
    float &str
)
{
    if (off0 < 0) {
        off0 = 0;
    }

    // nearest FFT bin center.
    float hz0 = round(hz / 6.25) * 6.25;
    // move hz to hz0, so it is centered in a symbol-sized bin.
    std::vector<float> downsamples = fft_shift_f(bins, rate, hz - hz0);
    int best_off = -1;
    int block = blocksize(rate);
    float best_sum = 0.0;

    for (int g = off0; g <= offN; g += gran)
    {
        if (g >= 0 && g + 79 * block <= (int)downsamples.size())
        {
            float sum = one_strength_known(downsamples, rate, syms, hz0, g);

            if (sum > best_sum || best_off == -1)
            {
                best_off = g;
                best_sum = sum;
            }
        }
    }

    if (best_off < 0) {
        return -1;
    }

    str = best_sum;
    return best_off;
}

//
// search for costas blocks in an MxN time/frequency grid.
// hz0 +/- hz_win in hz_inc increments. hz0 should be near 25.
// off0 +/- off_win in off_inc incremenents.
//
std::vector<Strength> FT8::search_both(
    const std::vector<float> &samples200,
    float hz0,
    int hz_n,
    float hz_win,
    int off0,
    int off_n,
    int off_win
)
{
    // assert(hz0 >= 25 - 6.25 / 2 && hz0 <= 25 + 6.25 / 2);

    std::vector<Strength> strengths;

    float hz_inc = 2 * hz_win / hz_n;
    int off_inc = round(2 * off_win / (float)off_n);

    if (off_inc < 1) {
        off_inc = 1;
    }

    for (float hz = hz0 - hz_win; hz <= hz0 + hz_win + 0.01; hz += hz_inc)
    {
        float str = 0;
        int off = search_time_fine(
            samples200,
            off0 - off_win,
            off0 + off_win, hz,
            off_inc,
            str
        );

        if (off >= 0)
        {
            Strength st;
            st.hz_ = hz;
            st.off_ = off;
            st.strength_ = str;
            strengths.push_back(st);
        }
    }

    return strengths;
}

void FT8::search_both_known(
    const std::vector<float> &samples,
    int rate,
    const std::vector<int> &syms,
    float hz0,
    float off_secs0, // seconds
    float &hz_out,
    float &off_out
)
{
    // assert(hz0 >= 0 && hz0 + 50 < rate / 2);

    int off0 = round(off_secs0 * (float)rate);
    int off_win = params.third_off_win * blocksize(rate_);

    if (off_win < 1) {
        off_win = 1;
    }

    int off_inc = trunc((2.0 * off_win) / (params.third_off_n - 1.0));

    if (off_inc < 1) {
        off_inc = 1;
    }

    int got_best = 0;
    float best_hz = 0;
    int best_off = 0;
    float best_strength = 0;
    std::vector<std::complex<float>> bins = fftEngine_->one_fft(samples, 0, samples.size());
    float hz_start, hz_inc, hz_end;

    if (params.third_hz_n > 1)
    {
        hz_inc = (2.0 * params.third_hz_win) / (params.third_hz_n - 1.0);
        hz_start = hz0 - params.third_hz_win;
        hz_end = hz0 + params.third_hz_win;
    }
    else
    {
        hz_inc = 1;
        hz_start = hz0;
        hz_end = hz0;
    }

    for (float hz = hz_start; hz <= hz_end + 0.0001; hz += hz_inc)
    {
        float strength = 0;
        int off = search_time_fine_known(
            bins,
            rate,
            syms,
            off0 - off_win,
            off0 + off_win,
            hz,
            off_inc,
            strength
        );

        if (off >= 0 && (got_best == 0 || strength > best_strength))
        {
            got_best = 1;
            best_hz = hz;
            best_off = off;
            best_strength = strength;
        }
    }

    if (got_best)
    {
        hz_out = best_hz;
        off_out = best_off / (float)rate;
    }
}

//
// shift frequency by shifting the bins of one giant FFT.
// so no problem with phase mismatch &c at block boundaries.
// surprisingly fast at 200 samples/second.
// shifts *down* by hz.
//
std::vector<float> FT8::fft_shift(
    const std::vector<float> &samples,
    int off,
    int len,
    int rate,
    float hz
)
{
    std::vector<std::complex<float>> bins;
    // horrible hack to avoid repeated FFTs on the same input.
    hack_mu_.lock();

    if ((int)samples.size() == hack_size_ && samples.data() == hack_data_ &&
        off == hack_off_ && len == hack_len_ &&
        samples[0] == hack_0_ && samples[1] == hack_1_)
    {
        bins = hack_bins_;
    }
    else
    {
        bins = fftEngine_->one_fft(samples, off, len);
        hack_bins_ = bins;
        hack_size_ = samples.size();
        hack_off_ = off;
        hack_len_ = len;
        hack_0_ = samples[0];
        hack_1_ = samples[1];
        hack_data_ = samples.data();
    }

    hack_mu_.unlock();
    return fft_shift_f(bins, rate, hz);
}

//
// shift down by hz.
//
std::vector<float> FT8::fft_shift_f(
    const std::vector<std::complex<float>> &bins,
    int rate,
    float hz
)
{
    int nbins = bins.size();
    int len = (nbins - 1) * 2;
    float bin_hz = rate / (float)len;
    int down = round(hz / bin_hz);
    std::vector<std::complex<float>> bins1(nbins);

    for (int i = 0; i < nbins; i++)
    {
        int j = i + down;

        if (j >= 0 && j < nbins) {
            bins1[i] = bins[j];
        } else {
            bins1[i] = 0;
        }
    }

    std::vector<float> out = fftEngine_->one_ifft(bins1);
    return out;
}

// shift the frequency by a fraction of 6.25,
// to center hz on bin 4 (25 hz).
std::vector<float> FT8::shift200(
    const std::vector<float> &samples200,
    int off,
    int len,
    float hz
)
{
    if (std::abs(hz - 25) < 0.001 && off == 0 && len == (int)samples200.size()) {
        return samples200;
    } else {
        return fft_shift(samples200, off, len, 200, hz - 25.0);
    }
    // return hilbert_shift(samples200, hz - 25.0, hz - 25.0, 200);
}

// returns a mini-FFT of 79 8-tone symbols.
FFTEngine::ffts_t FT8::extract(const std::vector<float> &samples200, float, int off)
{

    FFTEngine::ffts_t bins3 = fftEngine_->ffts(samples200, off, 32);
    FFTEngine::ffts_t m79(79);

    for (int si = 0; si < 79; si++)
    {
        m79[si].resize(8);

        if (si < (int)bins3.size())
        {
            for (int bi = 0; bi < 8; bi++)
            {
                auto x = bins3[si][4 + bi];
                m79[si][bi] = x;
            }
        }
        else
        {
            for (int bi = 0; bi < 8; bi++) {
                m79[si][bi] = 0;
            }
        }
    }

    return m79;
}

//
// m79 is a 79x8 array of complex.
//
FFTEngine::ffts_t FT8::un_gray_code_c(const FFTEngine::ffts_t &m79)
{
    FFTEngine::ffts_t m79a(79);
    int map[] = {0, 1, 3, 2, 6, 4, 5, 7};

    for (int si = 0; si < 79; si++)
    {
        m79a[si].resize(8);

        for (int bi = 0; bi < 8; bi++) {
            m79a[si][map[bi]] = m79[si][bi];
        }
    }

    return m79a;
}

//
// m79 is a 79x8 array of float.
//
std::vector<std::vector<float>> FT8::un_gray_code_r(const std::vector<std::vector<float>> &m79)
{
    std::vector<std::vector<float>> m79a(79);
    int map[] = {0, 1, 3, 2, 6, 4, 5, 7};

    for (int si = 0; si < 79; si++)
    {
        m79a[si].resize(8);

        for (int bi = 0; bi < 8; bi++) {
            m79a[si][map[bi]] = m79[si][bi];
        }
    }

    return m79a;
}

//
// normalize levels by windowed median.
// this helps, but why?
//
std::vector<std::vector<float>> FT8::convert_to_snr(const std::vector<std::vector<float>> &m79)
{
    if (params.snr_how < 0 || params.snr_win < 0) {
        return m79;
    }

    //
    // for each symbol time, what's its "noise" level?
    //
    std::vector<float> mm(79);

    for (int si = 0; si < 79; si++)
    {
        std::vector<float> v(8);
        float sum = 0.0;

        for (int bi = 0; bi < 8; bi++)
        {
            float x = m79[si][bi];
            v[bi] = x;
            sum += x;
        }

        if (params.snr_how != 1) {
            std::sort(v.begin(), v.end());
        }

        if (params.snr_how == 0) {
            // median
            mm[si] = (v[3] + v[4]) / 2;
        } else if (params.snr_how == 1) {
            mm[si] = sum / 8;
        } else if (params.snr_how == 2) {
            // all but strongest tone.
            mm[si] = (v[0] + v[1] + v[2] + v[3] + v[4] + v[5] + v[6]) / 7;
        } else if (params.snr_how == 3) {
            mm[si] = v[0]; // weakest tone
        } else if (params.snr_how == 4) {
            mm[si] = v[7]; // strongest tone
        } else if (params.snr_how == 5) {
            mm[si] = v[6]; // second-strongest tone
        } else {
            mm[si] = 1.0;
        }
    }

    // we're going to take a windowed average.
    std::vector<float> winwin;

    if (params.snr_win > 0) {
        winwin = blackman(2 * params.snr_win + 1);
    } else {
        winwin.push_back(1.0);
    }

    std::vector<std::vector<float>> n79(79);

    for (int si = 0; si < 79; si++)
    {
        float sum = 0;

        for (int dd = si - params.snr_win; dd <= si + params.snr_win; dd++)
        {
            int wi = dd - (si - params.snr_win);

            if (dd >= 0 && dd < 79) {
                sum += mm[dd] * winwin[wi];
            } else if (dd < 0) {
                sum += mm[0] * winwin[wi];
            } else {
                sum += mm[78] * winwin[wi];
            }
        }

        n79[si].resize(8);

        for (int bi = 0; bi < 8; bi++) {
            n79[si][bi] = m79[si][bi] / sum;
        }
    }

    return n79;
}

//
// normalize levels by windowed median.
// this helps, but why?
//
std::vector<std::vector<std::complex<float>>> FT8::c_convert_to_snr(
    const std::vector<std::vector<std::complex<float>>> &m79
)
{
    if (params.snr_how < 0 || params.snr_win < 0) {
        return m79;
    }

    //
    // for each symbol time, what's its "noise" level?
    //
    std::vector<float> mm(79);

    for (int si = 0; si < 79; si++)
    {
        std::vector<float> v(8);
        float sum = 0.0;

        for (int bi = 0; bi < 8; bi++)
        {
            float x = std::abs(m79[si][bi]);
            v[bi] = x;
            sum += x;
        }

        if (params.snr_how != 1) {
            std::sort(v.begin(), v.end());
        }

        if (params.snr_how == 0) {
            // median
            mm[si] = (v[3] + v[4]) / 2;
        } else if (params.snr_how == 1) {
            mm[si] = sum / 8;
        } else if (params.snr_how == 2) {
            // all but strongest tone.
            mm[si] = (v[0] + v[1] + v[2] + v[3] + v[4] + v[5] + v[6]) / 7;
        } else if (params.snr_how == 3) {
            mm[si] = v[0]; // weakest tone
        } else if (params.snr_how == 4) {
            mm[si] = v[7]; // strongest tone
        } else if (params.snr_how == 5) {
            mm[si] = v[6]; // second-strongest tone
        } else {
            mm[si] = 1.0;
        }
    }

    // we're going to take a windowed average.
    std::vector<float> winwin;

    if (params.snr_win > 0) {
        winwin = blackman(2 * params.snr_win + 1);
    } else {
        winwin.push_back(1.0);
    }

    std::vector<std::vector<std::complex<float>>> n79(79);

    for (int si = 0; si < 79; si++)
    {
        float sum = 0;

        for (int dd = si - params.snr_win; dd <= si + params.snr_win; dd++)
        {
            int wi = dd - (si - params.snr_win);

            if (dd >= 0 && dd < 79) {
                sum += mm[dd] * winwin[wi];
            } else if (dd < 0) {
                sum += mm[0] * winwin[wi];
            } else {
                sum += mm[78] * winwin[wi];
            }
        }

        n79[si].resize(8);

        for (int bi = 0; bi < 8; bi++) {
            n79[si][bi] = m79[si][bi] / sum;
        }
    }

    return n79;
}

//
// statistics to decide soft probabilities,
// to drive LDPC decoder.
// distribution of strongest tones, and
// distribution of noise.
//
void FT8::make_stats(
    const std::vector<std::vector<float>> &m79,
    Stats &bests,
    Stats &all
)
{
    int costas[] = {3, 1, 4, 0, 6, 5, 2};

    for (int si = 0; si < 79; si++)
    {
        if (si < 7 || (si >= 36 && si < 36 + 7) || si >= 72)
        {
            // Costas.
            int ci;

            if (si >= 72) {
                ci = si - 72;
            } else if (si >= 36) {
                ci = si - 36;
            } else {
                ci = si;
            }

            for (int bi = 0; bi < 8; bi++)
            {
                float x = m79[si][bi];
                all.add(x);

                if (bi == costas[ci]) {
                    bests.add(x);
                }
            }
        }
        else
        {
            float mx = 0;

            for (int bi = 0; bi < 8; bi++)
            {
                float x = m79[si][bi];

                if (x > mx) {
                    mx = x;
                }

                all.add(x);
            }

            bests.add(mx);
        }
    }
}

//
// convert 79x8 complex FFT bins to magnitudes.
//
// exploits local phase coherence by decreasing magnitudes of bins
// whose phase is far from the phases of nearby strongest tones.
//
// relies on each tone being reasonably well centered in its FFT bin
// (in time and frequency) so that each tone completes an integer
// number of cycles and thus preserves phase from one symbol to the
// next.
//
std::vector<std::vector<float>> FT8::soft_c2m(const FFTEngine::ffts_t &c79)
{
    std::vector<std::vector<float>> m79(79);
    std::vector<float> raw_phases(79); // of strongest tone in each symbol time

    for (int si = 0; si < 79; si++)
    {
        m79[si].resize(8);
        int mxi = -1;
        float mx;
        float mx_phase;

        for (int bi = 0; bi < 8; bi++)
        {
            float x = std::abs(c79[si][bi]);
            m79[si][bi] = x;

            if (mxi < 0 || x > mx)
            {
                mxi = bi;
                mx = x;
                mx_phase = std::arg(c79[si][bi]); // -pi .. pi
            }
        }

        raw_phases[si] = mx_phase;
    }

    if (params.soft_phase_win <= 0) {
        return m79;
    }

    // phase around each symbol.
    std::vector<float> phases(79);

    // for each symbol time, median of nearby phases
    for (int si = 0; si < 79; si++)
    {
        std::vector<float> v;

        for (int si1 = si - params.soft_phase_win; si1 <= si + params.soft_phase_win; si1++)
        {
            if (si1 >= 0 && si1 < 79)
            {
                float x = raw_phases[si1];
                v.push_back(x);
            }
        }

        // choose the phase that has the lowest total distance to other
        // phases. like median but avoids -pi..pi wrap-around.
        int n = v.size();
        int best = -1;
        float best_score = 0;
        for (int i = 0; i < n; i++)

        {
            float score = 0;

            for (int j = 0; j < n; j++)
            {
                if (i == j) {
                    continue;
                }

                float d = fabs(v[i] - v[j]);

                if (d > M_PI) {
                    d = 2 * M_PI - d;
                }

                score += d;
            }

            if (best == -1 || score < best_score)
            {
                best = i;
                best_score = score;
            }
        }

        phases[si] = v[best];
    }

    // project each tone against the median phase around that symbol time.
    for (int si = 0; si < 79; si++)
    {
        for (int bi = 0; bi < 8; bi++)
        {
            float mag = std::abs(c79[si][bi]);
            float angle = std::arg(c79[si][bi]);
            float d = angle - phases[si];
            float factor = 0.1;

            if (d < M_PI / 2 && d > -M_PI / 2) {
                factor = cos(d);
            }

            m79[si][bi] = factor * mag;
        }
    }

    return m79;
}

//
// guess the probability that a bit is zero vs one,
// based on strengths of strongest tones that would
// give it those values. for soft LDPC decoding.
//
// returns log-likelihood, zero is positive, one is negative.
//
float FT8::bayes(
    float best_zero,
    float best_one,
    int lli,
    Stats &bests,
    Stats &all
)
{
    float maxlog = 4.97;
    float ll = 0;
    float pzero = 0.5;
    float pone = 0.5;

    if (params.use_apriori)
    {
        pzero = 1.0 - apriori174[lli];
        pone = apriori174[lli];
    }

    //
    // Bayes combining rule normalization from:
    // http://cs.wellesley.edu/~anderson/writing/naive-bayes.pdf
    //
    // a = P(zero)P(e0|zero)P(e1|zero)
    // b = P(one)P(e0|one)P(e1|one)
    // p = a / (a + b)
    //
    // also see Mark Owen's book Practical Signal Processing,
    // Chapter 6.
    //

    // zero
    float a = pzero * bests.problt(best_zero) * (1.0 - all.problt(best_one));

    if (params.bayes_how == 1) {
        a *= all.problt(all.mean() + (best_zero - best_one));
    }

    // one
    float b = pone * bests.problt(best_one) * (1.0 - all.problt(best_zero));

    if (params.bayes_how == 1) {
        b *= all.problt(all.mean() + (best_one - best_zero));
    }

    float p;

    if (a + b == 0) {
        p = 0.5;
    } else {
        p = a / (a + b);
    }

    if (1 - p == 0.0) {
        ll = maxlog;
    } else {
        ll = log(p / (1 - p));
    }

    if (ll > maxlog) {
        ll = maxlog;
    }

    if (ll < -maxlog) {
        ll = -maxlog;
    }

    return ll;
}

//
// c79 is 79x8 complex tones, before un-gray-coding.
//
void FT8::soft_decode(const FFTEngine::ffts_t &c79, float ll174[])
{
    std::vector<std::vector<float>> m79(79);
    // m79 = absolute values of c79.
    // still pre-un-gray-coding so we know which
    // are the correct Costas tones.
    m79 = soft_c2m(c79);
    m79 = convert_to_snr(m79);
    // statistics to decide soft probabilities.
    // distribution of strongest tones, and
    // distribution of noise.
    Stats bests(params.problt_how_sig, params.log_tail, params.log_rate);
    Stats all(params.problt_how_noise, params.log_tail, params.log_rate);
    make_stats(m79, bests, all);
    m79 = un_gray_code_r(m79);
    int lli = 0;

    for (int i79 = 0; i79 < 79; i79++)
    {
        if (i79 < 7 || (i79 >= 36 && i79 < 36 + 7) || i79 >= 72) {
            // Costas, skip
            continue;
        }

        // for each of the three bits, look at the strongest tone
        // that would make it a zero, and the strongest tone that
        // would make it a one. use Bayes to decide which is more
        // likely, comparing each against the distribution of noise
        // and the distribution of strongest tones.
        // most-significant-bit first.

        for (int biti = 0; biti < 3; biti++)
        {
            // tone numbers that make this bit zero or one.
            int zeroi[4];
            int onei[4];

            if (biti == 0)
            {
                // high bit
                zeroi[0] = 0;
                zeroi[1] = 1;
                zeroi[2] = 2;
                zeroi[3] = 3;
                onei[0] = 4;
                onei[1] = 5;
                onei[2] = 6;
                onei[3] = 7;
            }

            if (biti == 1)
            {
                // middle bit
                zeroi[0] = 0;
                zeroi[1] = 1;
                zeroi[2] = 4;
                zeroi[3] = 5;
                onei[0] = 2;
                onei[1] = 3;
                onei[2] = 6;
                onei[3] = 7;
            }

            if (biti == 2)
            {
                // low bit
                zeroi[0] = 0;
                zeroi[1] = 2;
                zeroi[2] = 4;
                zeroi[3] = 6;
                onei[0] = 1;
                onei[1] = 3;
                onei[2] = 5;
                onei[3] = 7;
            }

            // strongest tone that would make this bit be zero.
            int got_best_zero = 0;
            float best_zero = 0;

            for (int i = 0; i < 4; i++)
            {
                float x = m79[i79][zeroi[i]];

                if (got_best_zero == 0 || x > best_zero)
                {
                    got_best_zero = 1;
                    best_zero = x;
                }
            }

            // strongest tone that would make this bit be one.
            int got_best_one = 0;
            float best_one = 0;

            for (int i = 0; i < 4; i++)
            {
                float x = m79[i79][onei[i]];
                if (got_best_one == 0 || x > best_one)
                {
                    got_best_one = 1;
                    best_one = x;
                }
            }

            float ll = bayes(best_zero, best_one, lli, bests, all);
            ll174[lli++] = ll;
        }
    }
    // assert(lli == 174);
}

//
// c79 is 79x8 complex tones, before un-gray-coding.
//
void FT8::c_soft_decode(const FFTEngine::ffts_t &c79x, float ll174[])
{
    FFTEngine::ffts_t c79 = c_convert_to_snr(c79x);
    int costas[] = {3, 1, 4, 0, 6, 5, 2};
    std::complex<float> maxes[79];

    for (int i = 0; i < 79; i++)
    {
        std::complex<float> m;

        if (i < 7)
        {
            // Costas.
            m = c79[i][costas[i]];
        }
        else if (i >= 36 && i < 36 + 7)
        {
            // Costas.
            m = c79[i][costas[i - 36]];
        }
        else if (i >= 72)
        {
            // Costas.
            m = c79[i][costas[i - 72]];
        }
        else
        {
            int got = 0;

            for (int j = 0; j < 8; j++)
            {
                if (got == 0 || std::abs(c79[i][j]) > std::abs(m))
                {
                    got = 1;
                    m = c79[i][j];
                }
            }
        }

        maxes[i] = m;
    }

    std::vector<std::vector<float>> m79(79);

    for (int i = 0; i < 79; i++)
    {
        m79[i].resize(8);

        for (int j = 0; j < 8; j++)
        {
            std::complex<float> c = c79[i][j];
            int n = 0;
            float sum = 0;

            for (int k = i - params.c_soft_win; k <= i + params.c_soft_win; k++)
            {
                if (k < 0 || k >= 79) {
                    continue;
                }

                if (k == i)
                {
                    sum -= params.c_soft_weight * std::abs(c);
                }
                else
                {
                    // we're expecting all genuine tones to have
                    // about the same phase and magnitude.
                    // so set m79[i][j] to the distance from the
                    // phase/magnitude predicted by surrounding
                    // genuine-looking tones.
                    std::complex<float> c1 = maxes[k];
                    std::complex<float> d = c1 - c;
                    sum += std::abs(d);
                }

                n += 1;
            }

            m79[i][j] = 0 - (sum / n);
        }
    }

    // statistics to decide soft probabilities.
    // distribution of strongest tones, and
    // distribution of noise.
    Stats bests(params.problt_how_sig, params.log_tail, params.log_rate);
    Stats all(params.problt_how_noise, params.log_tail, params.log_rate);
    make_stats(m79, bests, all);
    m79 = un_gray_code_r(m79);
    int lli = 0;

    for (int i79 = 0; i79 < 79; i79++)
    {
        if (i79 < 7 || (i79 >= 36 && i79 < 36 + 7) || i79 >= 72) {
            // Costas, skip
            continue;
        }

        // for each of the three bits, look at the strongest tone
        // that would make it a zero, and the strongest tone that
        // would make it a one. use Bayes to decide which is more
        // likely, comparing each against the distribution of noise
        // and the distribution of strongest tones.
        // most-significant-bit first.

        for (int biti = 0; biti < 3; biti++)
        {
            // tone numbers that make this bit zero or one.
            int zeroi[4];
            int onei[4];

            if (biti == 0)
            {
                // high bit
                zeroi[0] = 0;
                zeroi[1] = 1;
                zeroi[2] = 2;
                zeroi[3] = 3;
                onei[0] = 4;
                onei[1] = 5;
                onei[2] = 6;
                onei[3] = 7;
            }

            if (biti == 1)
            {
                // middle bit
                zeroi[0] = 0;
                zeroi[1] = 1;
                zeroi[2] = 4;
                zeroi[3] = 5;
                onei[0] = 2;
                onei[1] = 3;
                onei[2] = 6;
                onei[3] = 7;
            }

            if (biti == 2)
            {
                // low bit
                zeroi[0] = 0;
                zeroi[1] = 2;
                zeroi[2] = 4;
                zeroi[3] = 6;
                onei[0] = 1;
                onei[1] = 3;
                onei[2] = 5;
                onei[3] = 7;
            }

            // strongest tone that would make this bit be zero.
            int got_best_zero = 0;
            float best_zero = 0;

            for (int i = 0; i < 4; i++)
            {
                float x = m79[i79][zeroi[i]];

                if (got_best_zero == 0 || x > best_zero)
                {
                    got_best_zero = 1;
                    best_zero = x;
                }
            }

            // strongest tone that would make this bit be one.
            int got_best_one = 0;
            float best_one = 0;

            for (int i = 0; i < 4; i++)
            {
                float x = m79[i79][onei[i]];

                if (got_best_one == 0 || x > best_one)
                {
                    got_best_one = 1;
                    best_one = x;
                }
            }

            float ll = bayes(best_zero, best_one, lli, bests, all);
            ll174[lli++] = ll;
        }
    }
    // assert(lli == 174);
}

//
// turn 79 symbol numbers into 174 bits.
// strip out the three Costas sync blocks,
// leaving 58 symbol numbers.
// each represents three bits.
// (all post-un-gray-code).
// str is per-symbol strength; must be positive.
// each returned element is < 0 for 1, > 0 for zero,
// scaled by str.
//
std::vector<float> FT8::extract_bits(const std::vector<int> &syms, const std::vector<float> str)
{
    // assert(syms.size() == 79);
    // assert(str.size() == 79);

    std::vector<float> bits;

    for (int si = 0; si < 79; si++)
    {
        if (si < 7 || (si >= 36 && si < 36 + 7) || si >= 72)
        {
            // costas -- skip
        }
        else
        {
            bits.push_back((syms[si] & 4) == 0 ? str[si] : -str[si]);
            bits.push_back((syms[si] & 2) == 0 ? str[si] : -str[si]);
            bits.push_back((syms[si] & 1) == 0 ? str[si] : -str[si]);
        }
    }

    return bits;
}

// decode successive pairs of symbols. exploits the likelyhood
// that they have the same phase, by summing the complex
// correlations for each possible pair and using the max.
void FT8::soft_decode_pairs(
    const FFTEngine::ffts_t &m79x,
    float ll174[]
)
{
    FFTEngine::ffts_t m79 = c_convert_to_snr(m79x);

    struct BitInfo
    {
        float zero; // strongest correlation that makes it zero
        float one;  // and one
    };

    std::vector<BitInfo> bitinfo(79 * 3);

    for (int i = 0; i < (int)bitinfo.size(); i++)
    {
        bitinfo[i].zero = 0;
        bitinfo[i].one = 0;
    }

    Stats all(params.problt_how_noise, params.log_tail, params.log_rate);
    Stats bests(params.problt_how_sig, params.log_tail, params.log_rate);
    int map[] = {0, 1, 3, 2, 6, 4, 5, 7}; // un-gray-code

    for (int si = 0; si < 79; si += 2)
    {
        float mx = 0;
        float corrs[8 * 8];

        for (int s1 = 0; s1 < 8; s1++)
        {
            for (int s2 = 0; s2 < 8; s2++)
            {
                // sum up the correlations.
                std::complex<float> csum = m79[si][s1];

                if (si + 1 < 79) {
                    csum += m79[si + 1][s2];
                }

                float x = std::abs(csum);
                corrs[s1 * 8 + s2] = x;

                if (x > mx) {
                    mx = x;
                }

                all.add(x);
                // first symbol
                int i = map[s1];

                for (int bit = 0; bit < 3; bit++)
                {
                    int bitind = (si + 0) * 3 + (2 - bit);

                    if ((i & (1 << bit)))
                    {
                        // symbol i would make this bit a one.
                        if (x > bitinfo[bitind].one) {
                            bitinfo[bitind].one = x;
                        }
                    }
                    else
                    {
                        // symbol i would make this bit a zero.
                        if (x > bitinfo[bitind].zero) {
                            bitinfo[bitind].zero = x;
                        }
                    }
                }

                // second symbol
                if (si + 1 < 79)
                {
                    i = map[s2];

                    for (int bit = 0; bit < 3; bit++)
                    {
                        int bitind = (si + 1) * 3 + (2 - bit);

                        if ((i & (1 << bit)))
                        {
                            // symbol i would make this bit a one.
                            if (x > bitinfo[bitind].one) {
                                bitinfo[bitind].one = x;
                            }
                        }
                        else
                        {
                            // symbol i would make this bit a zero.
                            if (x > bitinfo[bitind].zero) {
                                bitinfo[bitind].zero = x;
                            }
                        }
                    }
                }
            }
        }

        if (si == 0 || si == 36 || si == 72) {
            bests.add(corrs[3 * 8 + 1]);
        } else if (si == 2 || si == 38 || si == 74) {
            bests.add(corrs[4 * 8 + 0]);
        } else if (si == 4 || si == 40 || si == 76) {
            bests.add(corrs[6 * 8 + 5]);
        } else {
            bests.add(mx);
        }
    }

    int lli = 0;

    for (int si = 0; si < 79; si++)
    {
        if (si < 7 || (si >= 36 && si < 36 + 7) || si >= 72) {
            // costas
            continue;
        }

        for (int i = 0; i < 3; i++)
        {
            float best_zero = bitinfo[si * 3 + i].zero;
            float best_one = bitinfo[si * 3 + i].one;
            // ll174[lli++] = best_zero > best_one ? 4.99 : -4.99;
            float ll = bayes(best_zero, best_one, lli, bests, all);
            ll174[lli++] = ll;
        }
    }
    // assert(lli == 174);
}

void FT8::soft_decode_triples(
    const FFTEngine::ffts_t &m79x,
    float ll174[]
)
{
    FFTEngine::ffts_t m79 = c_convert_to_snr(m79x);

    struct BitInfo
    {
        float zero; // strongest correlation that makes it zero
        float one;  // and one
    };

    std::vector<BitInfo> bitinfo(79 * 3);

    for (int i = 0; i < (int)bitinfo.size(); i++)
    {
        bitinfo[i].zero = 0;
        bitinfo[i].one = 0;
    }

    Stats all(params.problt_how_noise, params.log_tail, params.log_rate);
    Stats bests(params.problt_how_sig, params.log_tail, params.log_rate);

    int map[] = {0, 1, 3, 2, 6, 4, 5, 7}; // un-gray-code

    for (int si = 0; si < 79; si += 3)
    {
        float mx = 0;
        float corrs[8 * 8 * 8];

        for (int s1 = 0; s1 < 8; s1++)
        {
            for (int s2 = 0; s2 < 8; s2++)
            {
                for (int s3 = 0; s3 < 8; s3++)
                {
                    std::complex<float> csum = m79[si][s1];

                    if (si + 1 < 79) {
                        csum += m79[si + 1][s2];
                    }

                    if (si + 2 < 79) {
                        csum += m79[si + 2][s3];
                    }

                    float x = std::abs(csum);
                    corrs[s1 * 64 + s2 * 8 + s3] = x;

                    if (x > mx) {
                        mx = x;
                    }

                    all.add(x);
                    // first symbol
                    int i = map[s1];

                    for (int bit = 0; bit < 3; bit++)
                    {
                        int bitind = (si + 0) * 3 + (2 - bit);

                        if ((i & (1 << bit)))
                        {
                            // symbol i would make this bit a one.
                            if (x > bitinfo[bitind].one) {
                                bitinfo[bitind].one = x;
                            }
                        }
                        else
                        {
                            // symbol i would make this bit a zero.
                            if (x > bitinfo[bitind].zero) {
                                bitinfo[bitind].zero = x;
                            }
                        }
                    }

                    // second symbol
                    if (si + 1 < 79)
                    {
                        i = map[s2];

                        for (int bit = 0; bit < 3; bit++)
                        {
                            int bitind = (si + 1) * 3 + (2 - bit);

                            if ((i & (1 << bit)))
                            {
                                // symbol i would make this bit a one.
                                if (x > bitinfo[bitind].one) {
                                    bitinfo[bitind].one = x;
                                }
                            }
                            else
                            {
                                // symbol i would make this bit a zero.
                                if (x > bitinfo[bitind].zero) {
                                    bitinfo[bitind].zero = x;
                                }
                            }
                        }
                    }

                    // third symbol
                    if (si + 2 < 79)
                    {
                        i = map[s3];

                        for (int bit = 0; bit < 3; bit++)
                        {
                            int bitind = (si + 2) * 3 + (2 - bit);

                            if ((i & (1 << bit)))
                            {
                                // symbol i would make this bit a one.
                                if (x > bitinfo[bitind].one) {
                                    bitinfo[bitind].one = x;
                                }
                            }
                            else
                            {
                                // symbol i would make this bit a zero.
                                if (x > bitinfo[bitind].zero) {
                                    bitinfo[bitind].zero = x;
                                }
                            }
                        }
                    }
                }
            }
        }

        // costas: 3, 1, 4, 0, 6, 5, 2
        if (si == 0 || si == 36 || si == 72) {
            bests.add(corrs[3 * 64 + 1 * 8 + 4]);
        } else if (si == 3 || si == 39 || si == 75) {
            bests.add(corrs[0 * 64 + 6 * 8 + 5]);
        } else {
            bests.add(mx);
        }
    }

    int lli = 0;

    for (int si = 0; si < 79; si++)
    {
        if (si < 7 || (si >= 36 && si < 36 + 7) || si >= 72) {
            // costas
            continue;
        }

        for (int i = 0; i < 3; i++)
        {
            float best_zero = bitinfo[si * 3 + i].zero;
            float best_one = bitinfo[si * 3 + i].one;
            // ll174[lli++] = best_zero > best_one ? 4.99 : -4.99;
            float ll = bayes(best_zero, best_one, lli, bests, all);
            ll174[lli++] = ll;
        }
    }
    // assert(lli == 174);
}

//
// given log likelyhood for each bit, try LDPC and OSD decoders.
// on success, puts corrected 174 bits into a174[].
//
int FT8::decode(const float ll174[], int a174[], int use_osd, std::string &comment)
{
    void ldpc_decode(float llcodeword[], int iters, int plain[], int *ok);
    void ldpc_decode_log(float codeword[], int iters, int plain[], int *ok);
    int plain[174];  // will be 0/1 bits.
    int ldpc_ok = 0; // 83 will mean success.
    ldpc_decode((float *)ll174, params.ldpc_iters, plain, &ldpc_ok);
    int ok_thresh = 83; // 83 is perfect

    if (ldpc_ok >= ok_thresh)
    {
        // plain[] is 91 systematic data bits, 83 parity bits.
        for (int i = 0; i < 174; i++) {
            a174[i] = plain[i];
        }

        if (OSD::check_crc(a174)) {
            // success!
            return 1;
        }
    }

    if (use_osd && params.osd_depth >= 0 && ldpc_ok >= params.osd_ldpc_thresh)
    {
        int oplain[91];
        int got_depth = -1;
        int osd_ok = OSD::osd_decode((float *)ll174, params.osd_depth, oplain, &got_depth);

        if (osd_ok)
        {
            // reconstruct all 174.
            comment += "OSD-" + std::to_string(got_depth) + "-" + std::to_string(ldpc_ok);
            OSD::ldpc_encode(oplain, a174);
            return 1;
        }
    }

    return 0;
}

//
// bandpass filter some FFT bins.
// smooth transition from stop-band to pass-band,
// so that it's not a brick-wall filter, so that it
// doesn't ring.
//
std::vector<std::complex<float>> FT8::fbandpass(
    const std::vector<std::complex<float>> &bins0,
    float bin_hz,
    float low_outer,  // start of transition
    float low_inner,  // start of flat area
    float high_inner, // end of flat area
    float high_outer  // end of transition
)
{
    // assert(low_outer <= low_inner);
    // assert(low_inner <= high_inner);
    // assert(high_inner <= high_outer);

    int nbins = bins0.size();
    std::vector<std::complex<float>> bins1(nbins);

    for (int i = 0; i < nbins; i++)
    {
        float ihz = i * bin_hz;
        // cos(x)+flat+cos(x) taper
        float factor;
        if (ihz <= low_outer || ihz >= high_outer)
        {
            factor = 0;
        }
        else if (ihz >= low_outer && ihz < low_inner)
        {
            // rising shoulder
#if 1
            factor = (ihz - low_outer) / (low_inner - low_outer); // 0 .. 1
#else
            float theta = (ihz - low_outer) / (low_inner - low_outer);    // 0 .. 1
            theta -= 1;                                                    // -1 .. 0
            theta *= 3.14159;                                              // -pi .. 0
            factor = cos(theta);                                           // -1 .. 1
            factor = (factor + 1) / 2;                                     // 0 .. 1
#endif
        }
        else if (ihz > high_inner && ihz <= high_outer)
        {
            // falling shoulder
#if 1
            factor = (high_outer - ihz) / (high_outer - high_inner); // 1 .. 0
#else
            float theta = (high_outer - ihz) / (high_outer - high_inner); // 1 .. 0
            theta = 1.0 - theta;                                           // 0 .. 1
            theta *= 3.14159;                                              // 0 .. pi
            factor = cos(theta);                                           // 1 .. -1
            factor = (factor + 1) / 2;                                     // 1 .. 0
#endif
        }
        else
        {
            factor = 1.0;
        }

        bins1[i] = bins0[i] * factor;
    }

    return bins1;
}

//
// move hz down to 25, filter+convert to 200 samples/second.
//
// like fft_shift(). one big FFT, move bins down and
// zero out those outside the band, then IFFT,
// then re-sample.
//
// XXX maybe merge w/ fft_shift() / shift200().
//
std::vector<float> FT8::down_v7(const std::vector<float> &samples, float hz)
{
    int len = samples.size();
    std::vector<std::complex<float>> bins = fftEngine_->one_fft(samples, 0, len);

    return down_v7_f(bins, len, hz);
}

std::vector<float> FT8::down_v7_f(const std::vector<std::complex<float>> &bins, int len, float hz)
{
    int nbins = bins.size();
    float bin_hz = rate_ / (float)len;
    int down = round((hz - 25) / bin_hz);
    std::vector<std::complex<float>> bins1(nbins);

    for (int i = 0; i < nbins; i++)
    {
        int j = i + down;

        if (j >= 0 && j < nbins) {
            bins1[i] = bins[j];
        } else {
            bins1[i] = 0;
        }
    }

    // now filter to fit in 200 samples/second.

    float low_inner = 25.0 - params.shoulder200_extra;
    float low_outer = low_inner - params.shoulder200;

    if (low_outer < 0) {
        low_outer = 0;
    }

    float high_inner = 75 - 6.25 + params.shoulder200_extra;
    float high_outer = high_inner + params.shoulder200;

    if (high_outer > 100) {
        high_outer = 100;
    }

    bins1 = fbandpass(
        bins1,
        bin_hz,
        low_outer,
        low_inner,
        high_inner,
        high_outer
    );

    // convert back to time domain and down-sample to 200 samples/second.
    int blen = round(len * (200.0 / rate_));
    std::vector<std::complex<float>> bbins(blen / 2 + 1);

    for (int i = 0; i < (int)bbins.size(); i++) {
        bbins[i] = bins1[i];
    }

    std::vector<float> out = fftEngine_->one_ifft(bbins);
    return out;
}

//
// putative start of signal is at hz and symbol si0.
//
// return 2 if it decodes to a brand-new message.
// return 1 if it decodes but we've already seen it,
//   perhaps in a different pass.
// return 0 if we could not decode.
//
// XXX merge with one_iter().
//
int FT8::one_merge(const std::vector<std::complex<float>> &bins, int len, float hz, int off)
{
    //
    // set up to search for best frequency and time offset.
    //

    //
    // move down to 25 hz and re-sample to 200 samples/second,
    // i.e. 32 samples/symbol.
    //
    std::vector<float> samples200 = down_v7_f(bins, len, hz);
    int off200 = round((off / (float)rate_) * 200.0);
    int ret = one_iter(samples200, off200, hz);
    return ret;
}

// return 2 if it decodes to a brand-new message.
// return 1 if it decodes but we've already seen it,
//   perhaps in a different pass.
// return 0 if we could not decode.
int FT8::one_iter(const std::vector<float> &samples200, int best_off, float hz_for_cb)
{
    if (params.do_second)
    {
        std::vector<Strength> strengths = search_both(
            samples200,
            25,
            params.second_hz_n,
            params.second_hz_win,
            best_off,
            params.second_off_n,
            params.second_off_win * 32
        );
        //
        // sort strongest-first.
        //
        std::sort(
            strengths.begin(),
            strengths.end(),
            [](const Strength &a, const Strength &b) -> bool {
                return a.strength_ > b.strength_;
            }
        );

        for (int i = 0; i < (int)strengths.size() && i < params.second_count; i++)
        {
            float hz = strengths[i].hz_;
            int off = strengths[i].off_;
            int ret = one_iter1(samples200, off, hz, hz_for_cb, hz_for_cb);

            if (ret > 0) {
                return ret;
            }
        }
    }
    else
    {
        int ret = one_iter1(samples200, best_off, 25, hz_for_cb, hz_for_cb);
        return ret;
    }

    return 0;
}

//
// estimate SNR, yielding numbers vaguely similar to WSJT-X.
// m79 is a 79x8 complex FFT output.
//
float FT8::guess_snr(const FFTEngine::ffts_t &m79)
{
    int costas[] = {3, 1, 4, 0, 6, 5, 2};
    float pnoises = 0;
    float psignals = 0;

    for (int i = 0; i < 7; i++)
    {
        psignals += std::abs(m79[i][costas[i]]);
        psignals += std::abs(m79[36 + i][costas[i]]);
        psignals += std::abs(m79[72 + i][costas[i]]);
        pnoises += std::abs(m79[i][(costas[i] + 4) % 8]);
        pnoises += std::abs(m79[36 + i][(costas[i] + 4) % 8]);
        pnoises += std::abs(m79[72 + i][(costas[i] + 4) % 8]);
    }

    for (int i = 0; i < 79; i++)
    {
        if (i < 7 || (i >= 36 && i < 36 + 7) || (i >= 72 && i < 72 + 7)) {
            continue;
        }

        std::vector<float> v(8);

        for (int j = 0; j < 8; j++) {
            v[j] = std::abs(m79[i][j]);
        }

        std::sort(v.begin(), v.end());
        psignals += v[7]; // strongest tone, probably the signal
        pnoises += (v[2] + v[3] + v[4]) / 3;
    }

    pnoises /= 79;
    psignals /= 79;
    pnoises *= pnoises; // square yields power
    psignals *= psignals;
    float raw = psignals / pnoises;
    raw -= 1; // turn (s+n)/n into s/n

    if (raw < 0.1) {
        raw = 0.1;
    }

    raw /= (2500.0 / 2.7); // 2.7 hz noise b/w -> 2500 hz b/w
    float snr = 10 * log10(raw);
    snr += 5;
    snr *= 1.4;
    return snr;
}

//
// compare phases of successive symbols to guess whether
// the starting offset is a little too high or low.
// we expect each symbol to have the same phase.
// an error in causes the phase to advance at a steady rate.
// so if hz is wrong, we expect the phase to advance
// or retard at a steady pace.
// an error in offset causes each symbol to start at
// a phase that depends on the symbol's frequency;
// a particular offset error causes a phase error
// that depends on frequency.
// hz0 is actual FFT bin number of m79[...][0] (always 4).
//
// the output adj_hz is relative to the FFT bin center;
// a positive number means the real signal seems to be
// a bit higher in frequency that the bin center.
//
// adj_off is the amount to change the offset, in samples.
// should be subtracted from offset.
//
void FT8::fine(const FFTEngine::ffts_t &m79, int, float &adj_hz, float &adj_off)
{
    adj_hz = 0.0;
    adj_off = 0.0;
    // tone number for each of the 79 symbols.
    int sym[79];
    float symval[79];
    float symphase[79];
    int costas[] = {3, 1, 4, 0, 6, 5, 2};

    for (int i = 0; i < 79; i++)
    {
        if (i < 7)
        {
            sym[i] = costas[i];
        }
        else if (i >= 36 && i < 36 + 7)
        {
            sym[i] = costas[i - 36];
        }
        else if (i >= 72)
        {
            sym[i] = costas[i - 72];
        }
        else
        {
            int mxj = -1;
            float mx = 0;

            for (int j = 0; j < 8; j++)
            {
                float x = std::abs(m79[i][j]);

                if (mxj < 0 || x > mx)
                {
                    mx = x;
                    mxj = j;
                }
            }

            sym[i] = mxj;
        }

        symphase[i] = std::arg(m79[i][sym[i]]);
        symval[i] = std::abs(m79[i][sym[i]]);
    }

    float sum = 0;
    float weight_sum = 0;

    for (int i = 0; i < 79 - 1; i++)
    {
        float d = symphase[i + 1] - symphase[i];

        while (d > M_PI) {
            d -= 2 * M_PI;
        }

        while (d < -M_PI) {
            d += 2 * M_PI;
        }

        float w = symval[i];
        sum += d * w;
        weight_sum += w;
    }

    float mean = sum / weight_sum;
    float err_rad = mean; // radians per symbol time
    float err_hz = (err_rad / (2 * M_PI)) / 0.16; // cycles per symbol time

    // if each symbol's phase is a bit more than we expect,
    // that means the real frequency is a bit higher
    // than we thought, so increase our estimate.
    adj_hz = err_hz;

    //
    // now think about offset error.
    //
    // the higher tones have many cycles per
    // symbol -- e.g. tone 7 has 11 cycles
    // in each symbol. a one- or two-sample
    // offset error at such a high tone will
    // change the phase by pi or more,
    // which makes the phase-to-samples
    // conversion ambiguous. so only try
    // to distinguish early-ontime-late,
    // not the amount.
    //
    int nearly = 0;
    int nlate = 0;
    float early = 0.0;
    float late = 0.0;

    for (int i = 1; i < 79; i++)
    {
        float ph0 = std::arg(m79[i - 1][sym[i - 1]]);
        float ph = std::arg(m79[i][sym[i]]);
        float d = ph - ph0;
        d -= err_rad; // correct for hz error.

        while (d > M_PI) {
            d -= 2 * M_PI;
        }

        while (d < -M_PI) {
            d += 2 * M_PI;
        }

        // if off is correct, each symbol will have the same phase (modulo
        // the above hz correction), since each FFT bin holds an integer
        // number of cycles.

        // if off is too small, the phase is altered by the trailing part
        // of the previous symbol. if the previous tone was lower,
        // the phase won't have advanced as much as expected, and
        // this symbol's phase will be lower than the previous phase.
        // if the previous tone was higher, the phase will be more
        // advanced than expected. thus off too small leads to
        // a phase difference that's the reverse of the tone difference.

        // if off is too high, then the FFT started a little way into
        // this symbol, which causes the phase to be advanced a bit.
        // of course the previous symbol's phase was also advanced
        // too much. if this tone is higher than the previous symbol,
        // its phase will be more advanced than the previous. if
        // less, less.

        // the point: if successive phases and tone differences
        // are positively correlated, off is too high. if negatively,
        // too low.

        // fine_max_tone:
        // if late, ignore if a high tone, since ambiguous.
        // if early, ignore if prev is a high tone.

        if (sym[i] > sym[i - 1])
        {
            if (d > 0 && sym[i] <= params.fine_max_tone)
            {
                nlate++;
                late += d / std::abs(sym[i] - sym[i - 1]);
            }

            if (d < 0 && sym[i - 1] <= params.fine_max_tone)
            {
                nearly++;
                early += fabs(d) / std::abs(sym[i] - sym[i - 1]);
            }
        }
        else if (sym[i] < sym[i - 1])
        {
            if (d > 0 && sym[i - 1] <= params.fine_max_tone)
            {
                nearly++;
                early += d / std::abs(sym[i] - sym[i - 1]);
            }

            if (d < 0 && sym[i] <= params.fine_max_tone)
            {
                nlate++;
                late += fabs(d) / std::abs(sym[i] - sym[i - 1]);
            }
        }
    }

    if (nearly > 0) {
        early /= nearly;
    }

    if (nlate > 0) {
        late /= nlate;
    }

    // qDebug("early %d %.1f, late %d %.1f", nearly, early, nlate, late);

    // assumes 32 samples/symbol.
    if (nearly > 2 * nlate)
    {
        adj_off = round(32 * early / params.fine_thresh);

        if (adj_off > params.fine_max_off) {
            adj_off = params.fine_max_off;
        }
    }
    else if (nlate > 2 * nearly)
    {
        adj_off = 0 - round(32 * late / params.fine_thresh);

        if (fabs(adj_off) > params.fine_max_off) {
            adj_off =- params.fine_max_off;
        }
    }
}

//
// the signal is at roughly 25 hz in samples200.
//
// return 2 if it decodes to a brand-new message.
// return 1 if it decodes but we've already seen it,
//   perhaps in a different pass.
// return 0 if we could not decode.
//
int FT8::one_iter1(
    const std::vector<float> &samples200x,
    int best_off,
    float best_hz,
    float hz0_for_cb,
    float hz1_for_cb
)
{
    // put best_hz in the middle of bin 4, at 25.0.
    std::vector<float> samples200 = shift200(
        samples200x,
        0,
        samples200x.size(),
        best_hz
    );

    // mini 79x8 FFT.
    FFTEngine::ffts_t m79 = extract(samples200, 25, best_off);

    // look at symbol-to-symbol phase change to try
    // to improve best_hz and best_off.
    if (params.do_fine_hz || params.do_fine_off)
    {
        float adj_hz = 0;
        float adj_off = 0;
        fine(m79, 4, adj_hz, adj_off);

        if (params.do_fine_hz == 0) {
            adj_hz = 0;
        }

        if (params.do_fine_off == 0) {
            adj_off = 0;
        }

        if (fabs(adj_hz) < 6.25 / 4 && fabs(adj_off) < 4)
        {
            best_hz += adj_hz;
            best_off += round(adj_off);

            if (best_off < 0) {
                best_off = 0;
            }

            samples200 = shift200(samples200x, 0, samples200x.size(), best_hz);
            m79 = extract(samples200, 25, best_off);
        }
    }

    float ll174[174];

    if (params.soft_ones)
    {
        if (params.soft_ones == 1) {
            soft_decode(m79, ll174);
        } else {
            c_soft_decode(m79, ll174);
        }

        int ret = try_decode(
            samples200,
            ll174,
            best_hz,
            best_off,
            hz0_for_cb,
            hz1_for_cb,
            params.use_osd,
            "",
             m79
        );

        if (ret) {
            return ret;
        }
    }

    if (params.soft_pairs)
    {
        float p174[174];
        soft_decode_pairs(m79, p174);
        int ret = try_decode(
            samples200,
            p174,
            best_hz,
            best_off,
            hz0_for_cb,
            hz1_for_cb,
            params.use_osd,
            "",
            m79
        );

        if (ret) {
            return ret;
        }

        if (params.soft_ones == 0) {
            std::copy(p174, p174 + 174, ll174);
        }
    }

    if (params.soft_triples)
    {
        float p174[174];
        soft_decode_triples(m79, p174);
        int ret = try_decode(
            samples200,
            p174,
            best_hz,
            best_off,
            hz0_for_cb,
            hz1_for_cb,
            params.use_osd,
            "",
            m79
        );

        if (ret) {
            return ret;
        }
    }

    if (params.use_hints)
    {
        for (int hi = 0; hi < (int)hints1_.size(); hi++)
        {
            int h = hints1_[hi]; // 28-bit number, goes in ll174 0..28

            if (params.use_hints == 2 && h != 2)
            {
                // just CQ
                continue;
            }

            float n174[174];

            for (int i = 0; i < 174; i++)
            {
                if (i < 28)
                {
                    int bit = h & (1 << 27);

                    if (bit) {
                        n174[i] = -4.97;
                    } else {
                        n174[i] = 4.97;
                    }

                    h <<= 1;
                }
                else
                {
                    n174[i] = ll174[i];
                }
            }

            int ret = try_decode(
                samples200,
                n174,
                best_hz,
                best_off,
                hz0_for_cb,
                hz1_for_cb,
                0,
                "hint1",
                m79
            );

            if (ret) {
                return ret;
            }
        }
    }

    if (params.use_hints == 1)
    {
        for (int hi = 0; hi < (int)hints2_.size(); hi++)
        {
            int h = hints2_[hi]; // 28-bit number, goes in ll174 29:29+28
            float n174[174];

            for (int i = 0; i < 174; i++)
            {
                if (i >= 29 && i < 29 + 28)
                {
                    int bit = h & (1 << 27);

                    if (bit) {
                        n174[i] = -4.97;
                    } else {
                        n174[i] = 4.97;
                    }

                    h <<= 1;
                }
                else
                {
                    n174[i] = ll174[i];
                }
            }

            int ret = try_decode(
                samples200,
                n174,
                best_hz,
                best_off,
                hz0_for_cb,
                hz1_for_cb,
                0,
                "hint2",
                m79
            );

            if (ret) {
                return ret;
            }
        }
    }

    return 0;
}

//
// subtract a corrected decoded signal from nsamples_,
// perhaps revealing a weaker signal underneath,
// to be decoded in a subsequent pass.
//
// re79[] holds the error-corrected symbol numbers.
//
void FT8::subtract(
    const std::vector<int> re79,
    float hz0,
    float hz1,
    float off_sec
)
{
    int block = blocksize(rate_);
    float bin_hz = rate_ / (float)block;
    int off0 = off_sec * rate_;
    float mhz = (hz0 + hz1) / 2.0;
    int bin0 = round(mhz / bin_hz);
    // move nsamples so that signal is centered in bin0.
    float diff0 = (bin0 * bin_hz) - hz0;
    float diff1 = (bin0 * bin_hz) - hz1;
    std::vector<float> moved = fftEngine_->hilbert_shift(nsamples_, diff0, diff1, rate_);
    FFTEngine::ffts_t bins = fftEngine_->ffts(moved, off0, block);

    if (bin0 + 8 > (int)bins[0].size()) {
        return;
    }

    if ((int)bins.size() < 79) {
        return;

    }

    std::vector<float> phases(79);
    std::vector<float> amps(79);

    for (int i = 0; i < 79; i++)
    {
        int sym = bin0 + re79[i];
        std::complex<float> c = bins[i][sym];
        phases[i] = std::arg(c);
        // FFT multiplies magnitudes by number of bins,
        // or half the number of samples.
        amps[i] = std::abs(c) / (block / 2.0);
    }

    int ramp = round(block * params.subtract_ramp);

    if (ramp < 1) {
        ramp = 1;
    }

    // initial ramp part of first symbol.
    {
        int sym = bin0 + re79[0];
        float phase = phases[0];
        float amp = amps[0];
        float hz = 6.25 * sym;
        float dtheta = 2 * M_PI / (rate_ / hz); // advance per sample

        for (int jj = 0; jj < ramp; jj++)
        {
            float theta = phase + jj * dtheta;
            float x = amp * cos(theta);
            x *= jj / (float)ramp;
            int iii = off0 + block * 0 + jj;
            moved[iii] -= x;
        }
    }

    for (int si = 0; si < 79; si++)
    {
        int sym = bin0 + re79[si];
        float phase = phases[si];
        float amp = amps[si];
        float hz = 6.25 * sym;
        float dtheta = 2 * M_PI / (rate_ / hz); // advance per sample

        // we've already done the first ramp for this symbol.
        // now for the steady part between ramps.
        for (int jj = ramp; jj < block - ramp; jj++)
        {
            float theta = phase + jj * dtheta;
            float x = amp * cos(theta);
            int iii = off0 + block * si + jj;
            moved[iii] -= x;
        }

        // now the two ramps, from us to the next symbol.
        // we need to smoothly change the frequency,
        // approximating wsjt-x's gaussian frequency shift,
        // and also end up matching the next symbol's phase,
        // which is often different from this symbol due
        // to inaccuracies in hz or offset.

        // at start of this symbol's off-ramp.
        float theta = phase + (block - ramp) * dtheta;
        float hz1;
        float phase1;

        if (si + 1 >= 79)
        {
            hz1 = hz;
            phase1 = phase;
        }
        else
        {
            int sym1 = bin0 + re79[si + 1];
            hz1 = 6.25 * sym1;
            phase1 = phases[si + 1];
        }

        float dtheta1 = 2 * M_PI / (rate_ / hz1);
        // add this to dtheta for each sample, to gradually
        // change the frequency.
        float inc = (dtheta1 - dtheta) / (2.0 * ramp);
        // after we've applied all those inc's, what will the
        // phase be at the end of the next symbol's initial ramp,
        // if we don't do anything to correct it?
        float actual = theta + dtheta * 2.0 * ramp + inc * 4.0 * ramp * ramp / 2.0;
        // what phase does the next symbol want to be at when
        // its on-ramp finishes?
        float target = phase1 + dtheta1 * ramp;

        // ???
        while (fabs(target - actual) > M_PI)
        {
            if (target < actual) {
                target += (2 * M_PI) - 1e-3; // plus epsilonn to break possible infinite loop
            } else {
                target -= (2 * M_PI) + 1e-3; // plus epsilonn to break possible infinite loop
            }
        }

        // adj is to be spread evenly over the off-ramp and on-ramp samples.
        float adj = target - actual;
        int end = block + ramp;

        if (si == 79 - 1) {
            end = block;
        }

        for (int jj = block - ramp; jj < end; jj++)
        {
            int iii = off0 + block * si + jj;
            float x = amp * cos(theta);

            // trail off to zero at the very end.
            if (si == 79 - 1) {
                x *= 1.0 - ((jj - (block - ramp)) / (float)ramp);
            }

            moved[iii] -= x;
            theta += dtheta;
            dtheta += inc;
            theta += adj / (2.0 * ramp);
        }
    }

    nsamples_ = fftEngine_->hilbert_shift(moved, -diff0, -diff1, rate_);
}

//
// decode, give to callback, and subtract.
//
// return 2 if it decodes to a brand-new message.
// return 1 if it decodes but we've already seen it,
//   perhaps in a different pass.
// return 0 if we could not decode.
//
int FT8::try_decode(
    const std::vector<float> &samples200,
    float ll174[174],
    float best_hz,
    int best_off_samples,
    float hz0_for_cb,
    float,
    int use_osd,
    const char *comment1,
    const FFTEngine::ffts_t &m79
)
{
    int a174[174];
    std::string comment(comment1);

    if (decode(ll174, a174, use_osd, comment))
    {
        // a174 is corrected 91 bits of plain message plus 83 bits of LDPC parity.
        // how many of the corrected 174 bits match the received signal in ll174?
        int correct_bits = 0;

        for (int i = 0; i < 174; i++)
        {
            if (ll174[i] < 0 && a174[i] == 1) {
                correct_bits += 1;
            } else if (ll174[i] > 0 && a174[i] == 0) {
                correct_bits += 1;
            }
        }

        // reconstruct correct 79 symbols from LDPC output.
        std::vector<int> re79 = recode(a174);

        if (params.do_third == 1)
        {
            // fine-tune offset and hz for better subtraction.
            float best_off = best_off_samples / 200.0;
            search_both_known(
                samples200,
                200,
                re79,
                best_hz,
                best_off,
                best_hz,
                best_off
            );
            best_off_samples = round(best_off * 200.0);
        }

        // convert starting sample # from 200 samples/second back to rate_.
        // also hz.
        float best_off = best_off_samples / 200.0; // convert to seconds
        best_hz = hz0_for_cb + (best_hz - 25.0);

        if (params.do_third == 2)
        {
            // fine-tune offset and hz for better subtraction.
            search_both_known(
                samples_,
                rate_,
                re79,
                best_hz,
                best_off,
                best_hz,
                best_off
            );
        }

        float snr = guess_snr(m79);

        if (cb_)
        {
            cb_mu_.lock();
            int ret = cb_->hcb(
                a174,
                best_hz + down_hz_,
                best_off,
                comment.c_str(),
                snr,
                pass_,
                correct_bits
            );

            cb_mu_.unlock();

            if (ret == 2)
            {
                // a new decode. subtract it from nsamples_.
                subtract(re79, best_hz, best_hz, best_off);
            }

            return ret;
        }

        return 1;
    }
    else
    {
        return 0;
    }
}

//
// given 174 bits corrected by LDPC, work
// backwards to the symbols that must have
// been sent.
// used to help ensure that subtraction subtracts
// at the right place.
//
std::vector<int> FT8::recode(int a174[])
{
    int i174 = 0;
    int costas[] = {3, 1, 4, 0, 6, 5, 2};
    std::vector<int> out79;

    for (int i79 = 0; i79 < 79; i79++)
    {
        if (i79 < 7)
        {
            out79.push_back(costas[i79]);
        }
        else if (i79 >= 36 && i79 < 36 + 7)
        {
            out79.push_back(costas[i79 - 36]);
        }
        else if (i79 >= 72)
        {
            out79.push_back(costas[i79 - 72]);
        }
        else
        {
            int sym = (a174[i174 + 0] << 2) | (a174[i174 + 1] << 1) | (a174[i174 + 2] << 0);
            i174 += 3;
            // gray code
            int map[] = {0, 1, 3, 2, 5, 6, 4, 7};
            sym = map[sym];
            out79.push_back(sym);
        }
    }
    // assert(out79.size() == 79);
    // assert(i174 == 174);
    return out79;
}

FT8Decoder::~FT8Decoder()
{
    forceQuit(); // stop all remaining running threads if any

    for (auto& fftEngine : fftEngines) {
        delete fftEngine;
    }
}

//
// Launch decoding
//
void FT8Decoder::entry(
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
    double t0 = now();
    double deadline = t0 + time_left;
    double final_deadline = t0 + total_time_left;
    // decodes from previous runs, for subtraction.
    std::vector<cdecode> prevdecs;

    for (int i = 0; i < nprevdecs; i++) {
        prevdecs.push_back(xprevdecs[i]);
    }

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

    float per = (max_hz - min_hz) / params.nthreads;

    for (int i = 0; i < params.nthreads; i++)
    {
        float hz0 = min_hz + i * per;

        if (i > 0 || params.overlap_edges) {
            hz0 -= params.overlap;
        }

        float hz1 = min_hz + (i + 1) * per;

        if (i != params.nthreads - 1 || params.overlap_edges) {
            hz1 += params.overlap;
        }

        hz0 = std::max(hz0, 0.0f);
        hz1 = std::min(hz1, (rate / 2.0f) - 50);

        if (i == (int) fftEngines.size()) {
            fftEngines.push_back(new FFTEngine());
        }

        FT8 *ft8 = new FT8(
            samples,
            hz0,
            hz1,
            start,
            rate,
            hints1,
            hints2,
            deadline,
            final_deadline,
            cb,
            prevdecs,
            fftEngines[i]
        );

        ft8->getParams() = getParams(); // transfer parameters
        int npasses = nprevdecs > 0 ? params.npasses_two : params.npasses_one;
        ft8->set_npasses(npasses);
        QThread *th = new QThread();
        th->setObjectName(tr("ft8:%1:%2").arg(cb->get_name()).arg(i));
        threads.push_back(th);
        // std::thread *th = new std::thread([ft8, npasses] () { ft8->go(npasses); });
        // thv.push_back(std::pair<FT8*, std::thread*>(ft8, th));
        ft8->moveToThread(th);
        QObject::connect(th, &QThread::started, ft8, &FT8::start_work);
        QObject::connect(ft8, &FT8::finished, th, &QThread::quit, Qt::DirectConnection);
        QObject::connect(th, &QThread::finished, ft8, &QObject::deleteLater);
        QObject::connect(th, &QThread::finished, th, &QThread::deleteLater);
        th->start();
    }
}

void FT8Decoder::wait(double time_left)
{
    unsigned long thread_timeout = time_left * 1000;

    while (threads.size() != 0)
    {
        bool success = threads.front()->wait(thread_timeout);

        if (!success)
        {
            qDebug("FT8::FT8Decoder::wait: thread timed out");
            thread_timeout = 50; // only 50ms for the rest
        }

        threads.erase(threads.begin());
    }
}

void FT8Decoder::forceQuit()
{
    while (threads.size() != 0)
    {
        threads.front()->quit();
        threads.front()->wait();
        threads.erase(threads.begin());
    }
}

} // namespace FT8
