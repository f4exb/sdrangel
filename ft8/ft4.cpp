///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026                                                           //
//                                                                               //
// Experimental FT4 decoder scaffold derived from FT8 decoder architecture.      //
///////////////////////////////////////////////////////////////////////////////////

#include <array>
#include <cmath>
#include <algorithm>
#include <random>

#include <QThread>
#include <QDebug>

#include "ft4.h"
#include "fft.h"
#include "osd.h"
#include "libldpc.h"
#include "util.h"
#include "arrays.h"

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

} // namespace

// a-priori probability of each of the 174 LDPC codeword
// bits being one. measured from reconstructed correct
// codewords, into ft8bits, then python bprob.py.
// from ft8-n4
const double FT4::apriori174[] = {
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

FT4::FT4(
    const std::vector<float> &samples,
    float min_hz,
    float max_hz,
    int start,
    int rate,
    const int hints1[],
    const int hints2[],
    double deadline,
    double final_deadline,
    CallbackInterface *cb,
    const std::vector<cdecode> &prevdecs,
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

void FT4::start_work()
{
    go(npasses_);
    emit finished();
}

// strength of costas block of signal with tone 0 at bi0,
// and symbol zero at si0.
float FT4::one_coarse_strength(const FFTEngine::ffts_t &bins, int bi0, int si0) const
{
    float sig = 0.0;
    float noise = 0.0;

    if (params.coarse_all >= 0)
    {
        for (int si = 0; si < 103; si++)
        {
            float mx;
            int mxi = -1;
            float sum = 0;

            for (int i = 0; i < 4; i++)
            {
                float x = std::abs(bins[si0 + si][bi0 + i]);
                sum += x;

                if (mxi < 0 || x > mx)
                {
                    mxi = i;
                    mx = x;
                }
            }

            int blockIndex = -1;
            int symbolInBlock = -1;

            if (si < 4)
            {
                blockIndex = 0;
                symbolInBlock = si;
            }
            else if (si >= 33 && si < 37)
            {
                blockIndex = 1;
                symbolInBlock = si - 33;
            }
            else if (si >= 66 && si < 70)
            {
                blockIndex = 2;
                symbolInBlock = si - 66;
            }
            else if (si >= 99)
            {
                blockIndex = 3;
                symbolInBlock = si - 99;
            }

            if (blockIndex >= 0)
            {
                int expectedTone = kFT4SyncTones[blockIndex][symbolInBlock];
                float x = std::abs(bins[si0 + si][bi0 + expectedTone]);
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
        // just sync symbols
        for (int blockIndex = 0; blockIndex < 4; blockIndex++)
        {
            int start = blockIndex * 33;

            for (int si = 0; si < 4; si++)
            {
                int expectedTone = kFT4SyncTones[blockIndex][si];

                for (int bi = 0; bi < 4; bi++)
                {
                    float x = std::abs(bins[si0 + start + si][bi0 + bi]);

                    if (bi == expectedTone) {
                        sig += x;
                    } else {
                        noise += x;
                    }
                }
            }
        }
    }

    if (params.coarse_strength_how == 0) {
        return sig - noise;
    } else if (params.coarse_strength_how == 1) {
        return sig - noise / 3;
    } else if (params.coarse_strength_how == 2) {
        return sig / (noise / 3);
    } else if (params.coarse_strength_how == 3) {
        return sig / (sig + (noise / 3));
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
int FT4::blocksize(int rate) const
{
    // FT4 symbol length is 576 at 12000 samples/second.
    int xblock = (576*rate) / 12000;
    int block = xblock;
    return block;
}



//
// look for potential psignals by searching FFT bins for Costas symbol
// blocks. returns a vector of candidate positions.
//
std::vector<Strength> FT4::coarse(const FFTEngine::ffts_t &bins, int si0, int si1)
{
    int block = blocksize(rate_);
    int nbins = bins[0].size();
    float bin_hz = rate_ / (float)block;
    int min_bin = min_hz_ / bin_hz;
    int max_bin = max_hz_ / bin_hz;
    std::vector<Strength> strengths;

    for (int bi = min_bin; bi < max_bin && bi + 4 <= nbins; bi++)
    {
        std::vector<Strength> sv;

        for (int si = si0; si < si1 && si + 103 <= (int)bins.size(); si++)
        {
            float s = one_coarse_strength(bins, bi, si);
            Strength st;
            st.strength_ = s;
            st.hz_ = bi * bin_hz;
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
std::vector<float> FT4::reduce_rate(
    const std::vector<float> &a,
    float hz0,
    float hz1,
    int arate,
    int brate,
    float &delta_hz
)
{
    // the pass band is hz0..hz1
    // stop bands are 0..hz00 and hz11..nyquist.
    float hz00;
    float hz11;
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

void FT4::go(int npasses)
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

    // can we reduce the sample rate?
    int nrate = -1;
    for (int xrate = 100; xrate < rate_; xrate += 100)
    {
        if (xrate < rate_ && (params.oddrate || (rate_ % xrate) == 0))
        {
            if (((max_hz_ - min_hz_) + 93.6 + 2 * params.go_extra) < params.nyquist * (xrate / 2))
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
            min_hz_ - 11.7 - params.go_extra,
            max_hz_ + 93.6 - 11.7 + params.go_extra,
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

        float ratio = nrate / (float)rate_;
        rate_ = nrate;
        start_ = round(start_ * ratio);
    }

    int block = blocksize(rate_);

    // start_ is the sample number of 0.5 seconds, the nominal start time.

    // make sure there's at least tplus*rate_ samples after the end.
    if (start_ + params.tplus * rate_ + 103 * block + block > samples_.size())
    {
        int need = start_ + params.tplus * rate_ + 103 * block - samples_.size();

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
            // reconstruct correct 103 symbols from LDPC output.
            std::vector<int> re103 = recode(d.bits);

            // fine up hz/off again now that we have more samples
            float best_hz = (d.hz0 + d.hz1) / 2.0;
            float best_off = d.off; // seconds
            search_both_known(
                samples_,
                rate_,
                re103,
                best_hz,
                best_off,
                best_hz,
                best_off
            );

            // subtract from nsamples_.
            subtract(re103, best_hz, best_hz, best_off);
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

        // just do this once, reuse for every fractional fft_shift
        // and down_v7_f() to 200 sps.
        std::vector<std::complex<float>> bins = fftEngine_->one_fft(
            samples_, 0, samples_.size());

        for (int hz_frac_i = 0; hz_frac_i < params.coarse_hz_n; hz_frac_i++)
        {
            // shift down by hz_frac
            float hz_frac = hz_frac_i * ((rate_ / (float)block) / params.coarse_hz_n);
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
// what's the strength of the FT4 sync blocks of
// the signal starting at hz and off?
//
float FT4::one_strength(const std::vector<float> &samples200, float hz, int off)
{
    int bin0 = round(hz / 20.833);
    float sig = 0;
    float noise = 0;

    // FT4 has 4 sync blocks of 4 symbols each at positions 0-3, 33-36, 66-69, 99-102
    for (int blockIndex = 0; blockIndex < 4; blockIndex++)
    {
        int start = blockIndex * 33; // Sync blocks start at 0, 33, 66, 99

        for (int si = 0; si < 4; si++)
        {
            auto fft = fftEngine_->one_fft(samples200, off + (si + start) * 32, 32);
            int expectedTone = kFT4SyncTones[blockIndex][si];

            for (int bi = 0; bi < 4; bi++)
            {
                float x = std::abs(fft[bin0 + bi]);

                if (bi == expectedTone) {
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
        return sig - noise / 3;
    } else if (params.strength_how == 2) {
        return sig / (noise / 3);
    } else if (params.strength_how == 3) {
        return sig / (sig + (noise / 3));
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
float FT4::one_strength_known(
    const std::vector<float> &samples,
    int rate,
    const std::vector<int> &syms,
    float hz,
    int off
)
{
    int block = blocksize(rate);
    float bin_hz = rate / (float)block;
    int bin0 = round(hz / bin_hz);
    float sig = 0;
    float noise = 0;
    float sum7 = 0;
    std::complex<float> prev = 0;

    for (int si = 0; si < 103; si += params.known_sparse)
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
            for (int bi = 0; bi < 4; bi++)
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
        return sig - noise / 3;
    } else if (params.known_strength_how == 2) {
        return sig / (noise / 3);
    } else if (params.known_strength_how == 3) {
        return sig / (sig + (noise / 3));
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

int FT4::search_time_fine(
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
    // shift in frequency to put hz at 46.8.
    // only shift the samples we need, both for speed,
    // and try to always shift down the same number of samples
    // to make it easier to cache fftw plans.
    //
    int len = (offN - off0) + 103 * 32 + 32;

    if (off0 + len > (int)samples200.size())
    {
        // len = samples200.size() - off0;
        // don't provoke random-length FFTs.
        return -1;
    }

    std::vector<float> downsamples200 = shift200(samples200, off0, len, hz);
    int best_off = -1;
    float best_sum = 0.0;

    for (int g = 0; g <= (offN - off0) && g + 103 * 32 <= len; g += gran)
    {
        float sum = one_strength(downsamples200, 46.8, g);

        if (sum > best_sum || best_off == -1)
        {
            best_off = g;
            best_sum = sum;
        }
    }

    str = best_sum;
    return off0 + best_off;
}

int FT4::search_time_fine_known(
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

    int block = blocksize(rate);
    float bin_hz = rate / (float)block;

    // nearest FFT bin center.
    float hz0 = round(hz / bin_hz) * bin_hz;
    // move hz to hz0, so it is centered in a symbol-sized bin.
    std::vector<float> downsamples = fft_shift_f(bins, rate, hz - hz0);
    int best_off = -1;
    float best_sum = 0.0;

    for (int g = off0; g <= offN; g += gran)
    {
        if (g >= 0 && g + 103 * block <= (int)downsamples.size())
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
// hz0 +/- hz_win in hz_inc increments. hz0 should be near 46.8.
// off0 +/- off_win in off_inc incremenents.
//
std::vector<Strength> FT4::search_both(
    const std::vector<float> &samples200,
    float hz0,
    int hz_n,
    float hz_win,
    int off0,
    int off_n,
    int off_win
)
{
    // assert(hz0 >= 46.8 - 20.833 / 2 && hz0 <= 46.8 + 20.833 / 2);

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

void FT4::search_both_known(
    const std::vector<float> &samples,
    int rate,
    const std::vector<int> &syms,
    float hz0,
    float off_secs0, // seconds
    float &hz_out,
    float &off_out
)
{
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
    float hz_start;
    float hz_inc;
    float hz_end;

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
std::vector<float> FT4::fft_shift(
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
std::vector<float> FT4::fft_shift_f(
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

// shift the frequency by a fraction of 20.833,
// to center hz on bin 2 (46.8 hz).
std::vector<float> FT4::shift200(
    const std::vector<float> &samples200,
    int off,
    int len,
    float hz
)
{
    if (std::abs(hz - 46.8) < 0.001 && off == 0 && len == (int)samples200.size()) {
        return samples200;
    } else {
        return fft_shift(samples200, off, len, 666.7, hz - 46.8);
    }
}

// returns a mini-FFT of 103 4-tone symbols.
// returns a mini-FFT of 103 4-tone symbols.
FFTEngine::ffts_t FT4::extract(const std::vector<float> &samples625, float, int off)
{

    FFTEngine::ffts_t bins3 = fftEngine_->ffts(samples625, off, 32);
    FFTEngine::ffts_t m103(103);

    for (int si = 0; si < 103; si++)
    {
        m103[si].resize(4);

        if (si < (int)bins3.size())
        {
            for (int bi = 0; bi < 4; bi++)
            {
                auto x = bins3[si][2 + bi];
                m103[si][bi] = x;
            }
        }
        else
        {
            for (int bi = 0; bi < 4; bi++) {
                m103[si][bi] = 0;
            }
        }
    }

    return m103;
}

//
// m103 is a 103x4 array of complex.
//
FFTEngine::ffts_t FT4::un_gray_code_c(const FFTEngine::ffts_t &m103) const
{
    FFTEngine::ffts_t m103a(103);
    // FT4 Gray code mapping for 4-FSK: {0, 1, 3, 2}
    int map[] = {0, 1, 3, 2};

    for (int si = 0; si < 103; si++)
    {
        m103a[si].resize(4);

        for (int bi = 0; bi < 4; bi++) {
            m103a[si][map[bi]] = m103[si][bi];
        }
    }

    return m103a;
}

//
// m103 is a 103x4 array of float.
//
std::vector<std::vector<float>> FT4::un_gray_code_r(const std::vector<std::vector<float>> &m103) const
{
    std::vector<std::vector<float>> m103a(103);
    // FT4 Gray code mapping for 4-FSK: {0, 1, 3, 2}
    int map[] = {0, 1, 3, 2};

    for (int si = 0; si < 103; si++)
    {
        m103a[si].resize(4);

        for (int bi = 0; bi < 4; bi++) {
            m103a[si][map[bi]] = m103[si][bi];
        }
    }

    return m103a;
}

//
// normalize levels by windowed median.
// this helps, but why?
//
std::vector<std::vector<float>> FT4::convert_to_snr(const std::vector<std::vector<float>> &m103) const
{
    if (params.snr_how < 0 || params.snr_win < 0) {
        return m103;
    }

    //
    // for each symbol time, what's its "noise" level?
    //
    std::vector<float> mm(103);

    for (int si = 0; si < 103; si++)
    {
        std::vector<float> v(4);
        float sum = 0.0;

        for (int bi = 0; bi < 4; bi++)
        {
            float x = m103[si][bi];
            v[bi] = x;
            sum += x;
        }

        if (params.snr_how != 1) {
            std::sort(v.begin(), v.end());
        }

        if (params.snr_how == 0) {
            // median (average of 2 middle values for 4 tones)
            mm[si] = (v[1] + v[2]) / 2;
        } else if (params.snr_how == 1) {
            mm[si] = sum / 4;
        } else if (params.snr_how == 2) {
            // all but strongest tone.
            mm[si] = (v[0] + v[1] + v[2]) / 3;
        } else if (params.snr_how == 3) {
            mm[si] = v[0]; // weakest tone
        } else if (params.snr_how == 4) {
            mm[si] = v[3]; // strongest tone
        } else if (params.snr_how == 5) {
            mm[si] = v[2]; // second-strongest tone
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

    std::vector<std::vector<float>> n103(103);

    for (int si = 0; si < 103; si++)
    {
        float sum = 0;

        for (int dd = si - params.snr_win; dd <= si + params.snr_win; dd++)
        {
            int wi = dd - (si - params.snr_win);

            if (dd >= 0 && dd < 103) {
                sum += mm[dd] * winwin[wi];
            } else if (dd < 0) {
                sum += mm[0] * winwin[wi];
            } else {
                sum += mm[102] * winwin[wi];
            }
        }

        n103[si].resize(4);

        for (int bi = 0; bi < 4; bi++) {
            n103[si][bi] = m103[si][bi] / sum;
        }
    }

    return n103;
}

//
// normalize levels by windowed median.
// this helps, but why?
//
std::vector<std::vector<std::complex<float>>> FT4::c_convert_to_snr(
    const std::vector<std::vector<std::complex<float>>> &m103
) const
{
    if (params.snr_how < 0 || params.snr_win < 0) {
        return m103;
    }

    //
    // for each symbol time, what's its "noise" level?
    //
    std::vector<float> mm(103);

    for (int si = 0; si < 103; si++)
    {
        std::vector<float> v(4);
        float sum = 0.0;

        for (int bi = 0; bi < 4; bi++)
        {
            float x = std::abs(m103[si][bi]);
            v[bi] = x;
            sum += x;
        }

        if (params.snr_how != 1) {
            std::sort(v.begin(), v.end());
        }

        if (params.snr_how == 0) {
            // median
            mm[si] = (v[1] + v[2]) / 2;
        } else if (params.snr_how == 1) {
            mm[si] = sum / 4;
        } else if (params.snr_how == 2) {
            // all but strongest tone.
            mm[si] = (v[0] + v[1] + v[2]) / 3;
        } else if (params.snr_how == 3) {
            mm[si] = v[0]; // weakest tone
        } else if (params.snr_how == 4) {
            mm[si] = v[3]; // strongest tone
        } else if (params.snr_how == 5) {
            mm[si] = v[2]; // second-strongest tone
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

    std::vector<std::vector<std::complex<float>>> n103(103);

    for (int si = 0; si < 103; si++)
    {
        float sum = 0;

        for (int dd = si - params.snr_win; dd <= si + params.snr_win; dd++)
        {
            int wi = dd - (si - params.snr_win);

            if (dd >= 0 && dd < 103) {
                sum += mm[dd] * winwin[wi];
            } else if (dd < 0) {
                sum += mm[0] * winwin[wi];
            } else {
                sum += mm[102] * winwin[wi];
            }
        }

        n103[si].resize(4);

        for (int bi = 0; bi < 4; bi++) {
            n103[si][bi] = m103[si][bi] / sum;
        }
    }

    return n103;
}

//
// statistics to decide soft probabilities,
// to drive LDPC decoder.
// distribution of strongest tones, and
// distribution of noise.
//
void FT4::make_stats(
    const std::vector<std::vector<float>> &m103,
    Stats &bests,
    Stats &all
)
{
    for (int si = 0; si < 103; si++)
    {
        // FT4 sync blocks at positions: 0-3, 33-36, 66-69, 99-102
        int blockIndex = -1;
        int symbolInBlock = -1;

        if (si < 4)
        {
            blockIndex = 0;
            symbolInBlock = si;
        }
        else if (si >= 33 && si < 37)
        {
            blockIndex = 1;
            symbolInBlock = si - 33;
        }
        else if (si >= 66 && si < 70)
        {
            blockIndex = 2;
            symbolInBlock = si - 66;
        }
        else if (si >= 99)
        {
            blockIndex = 3;
            symbolInBlock = si - 99;
        }

        if (blockIndex >= 0)
        {
            // FT4 sync symbol
            int expectedTone = kFT4SyncTones[blockIndex][symbolInBlock];

            for (int bi = 0; bi < 4; bi++)
            {
                float x = m103[si][bi];
                all.add(x);

                if (bi == expectedTone) {
                    bests.add(x);
                }
            }
        }
        else
        {
            // Data symbol - find strongest tone
            float mx = 0;

            for (int bi = 0; bi < 4; bi++)
            {
                float x = m103[si][bi];

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
// convert 103x4 complex FFT bins to magnitudes.
//
// exploits local phase coherence by decreasing magnitudes of bins
// whose phase is far from the phases of nearby strongest tones.
//
// relies on each tone being reasonably well centered in its FFT bin
// (in time and frequency) so that each tone completes an integer
// number of cycles and thus preserves phase from one symbol to the
// next.
//
std::vector<std::vector<float>> FT4::soft_c2m(const FFTEngine::ffts_t &c103) const
{
    std::vector<std::vector<float>> m103(103);
    std::vector<float> raw_phases(103); // of strongest tone in each symbol time

    for (int si = 0; si < 103; si++)
    {
        m103[si].resize(4);
        int mxi = -1;
        float mx;
        float mx_phase;

        for (int bi = 0; bi < 4; bi++)
        {
            float x = std::abs(c103[si][bi]);
            m103[si][bi] = x;

            if (mxi < 0 || x > mx)
            {
                mxi = bi;
                mx = x;
                mx_phase = std::arg(c103[si][bi]); // -pi .. pi
            }
        }

        raw_phases[si] = mx_phase;
    }

    if (params.soft_phase_win <= 0) {
        return m103;
    }

    // phase around each symbol.
    std::vector<float> phases(103);

    // for each symbol time, median of nearby phases
    for (int si = 0; si < 103; si++)
    {
        std::vector<float> v;

        for (int si1 = si - params.soft_phase_win; si1 <= si + params.soft_phase_win; si1++)
        {
            if (si1 >= 0 && si1 < 103)
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
    for (int si = 0; si < 103; si++)
    {
        for (int bi = 0; bi < 4; bi++)
        {
            float mag = std::abs(c103[si][bi]);
            float angle = std::arg(c103[si][bi]);
            float d = angle - phases[si];
            float factor = 0.1;

            if (d < M_PI / 2 && d > -M_PI / 2) {
                factor = cos(d);
            }

            m103[si][bi] = factor * mag;
        }
    }

    return m103;
}

//
// c103 is 103x4 complex tones, before un-gray-coding.
//
void FT4::soft_decode(const FFTEngine::ffts_t &c103, float ll174[])
{
    std::vector<std::vector<float>> m103(103);
    // m103 = absolute values of c103.
    // still pre-un-gray-coding so we know which
    // are the correct sync tones.
    m103 = soft_c2m(c103);
    m103 = convert_to_snr(m103);
    // statistics to decide soft probabilities.
    // distribution of strongest tones, and
    // distribution of noise.
    Stats bests(params.problt_how_sig, params.log_tail, params.log_rate);
    Stats all(params.problt_how_noise, params.log_tail, params.log_rate);
    make_stats(m103, bests, all);
    m103 = un_gray_code_r(m103);
    int lli = 0;

    // tone numbers that make each bit zero or one (for 4-FSK, 2 bits per symbol).
    int zeroi[2][2];
    int onei[2][2];

    for (int biti = 0; biti < 2; biti++)
    {
        if (biti == 0)
        {
            // high bit (bit 1)
            zeroi[0][0] = 0;
            zeroi[1][0] = 1;
            onei[0][0] = 2;
            onei[1][0] = 3;
        }

        if (biti == 1)
        {
            // low bit (bit 0)
            zeroi[0][1] = 0;
            zeroi[1][1] = 2;
            onei[0][1] = 1;
            onei[1][1] = 3;
        }
    }

    for (int i103 = 0; i103 < 103; i103++)
    {
        // FT4 sync blocks at positions: 0-3, 33-36, 66-69, 99-102
        if ((i103 < 4) || (i103 >= 33 && i103 < 37) || (i103 >= 66 && i103 < 70) || (i103 >= 99)) {
            // FT4 sync block, skip
            continue;
        }

        // for each of the two bits, look at the strongest tone
        // that would make it a zero, and the strongest tone that
        // would make it a one. use Bayes to decide which is more
        // likely, comparing each against the distribution of noise
        // and the distribution of strongest tones.
        // most-significant-bit first.

        for (int biti = 0; biti < 2; biti++)
        {
            // strongest tone that would make this bit be zero.
            int got_best_zero = 0;
            float best_zero = 0;

            for (int i = 0; i < 2; i++)
            {
                float x = m103[i103][zeroi[i][biti]];

                if (got_best_zero == 0 || x > best_zero)
                {
                    got_best_zero = 1;
                    best_zero = x;
                }
            }

            // strongest tone that would make this bit be one.
            int got_best_one = 0;
            float best_one = 0;

            for (int i = 0; i < 2; i++)
            {
                float x = m103[i103][onei[i][biti]];
                if (got_best_one == 0 || x > best_one)
                {
                    got_best_one = 1;
                    best_one = x;
                }
            }

            float ll = FT8::bayes(params, best_zero, best_one, lli, bests, all);
            ll174[lli++] = ll;
        }
    }
}

//
// c103 is 103x4 complex tones, before un-gray-coding.
//
void FT4::c_soft_decode(const FFTEngine::ffts_t &c103x, float ll174[])
{
    FFTEngine::ffts_t c103 = c_convert_to_snr(c103x);
    std::vector<std::complex<float>> maxes(103);

    for (int i = 0; i < 103; i++)
    {
        std::complex<float> m;

        // FT4 sync blocks at positions: 0-3, 33-36, 66-69, 99-102
        int blockIndex = -1;
        int symbolInBlock = -1;

        if (i < 4)
        {
            blockIndex = 0;
            symbolInBlock = i;
        }
        else if (i >= 33 && i < 37)
        {
            blockIndex = 1;
            symbolInBlock = i - 33;
        }
        else if (i >= 66 && i < 70)
        {
            blockIndex = 2;
            symbolInBlock = i - 66;
        }
        else if (i >= 99)
        {
            blockIndex = 3;
            symbolInBlock = i - 99;
        }

        if (blockIndex >= 0)
        {
            // FT4 sync symbol: use expected sync tone.
            int expectedTone = kFT4SyncTones[blockIndex][symbolInBlock];
            m = c103[i][expectedTone];
        }
        else
        {
            int got = 0;

            for (int j = 0; j < 4; j++)
            {
                if (got == 0 || std::abs(c103[i][j]) > std::abs(m))
                {
                    got = 1;
                    m = c103[i][j];
                }
            }
        }

        maxes[i] = m;
    }

    std::vector<std::vector<float>> m103(103);

    for (int i = 0; i < 103; i++)
    {
        m103[i].resize(4);

        for (int j = 0; j < 4; j++)
        {
            std::complex<float> c = c103[i][j];
            int n = 0;
            float sum = 0;

            for (int k = i - params.c_soft_win; k <= i + params.c_soft_win; k++)
            {
                if (k < 0 || k >= 103) {
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
                    // so set m103[i][j] to the distance from the
                    // phase/magnitude predicted by surrounding
                    // genuine-looking tones.
                    std::complex<float> c1 = maxes[k];
                    std::complex<float> d = c1 - c;
                    sum += std::abs(d);
                }

                n += 1;
            }

            m103[i][j] = 0 - (sum / n);
        }
    }

    // statistics to decide soft probabilities.
    // distribution of strongest tones, and
    // distribution of noise.
    Stats bests(params.problt_how_sig, params.log_tail, params.log_rate);
    Stats all(params.problt_how_noise, params.log_tail, params.log_rate);
    make_stats(m103, bests, all);
    m103 = un_gray_code_r(m103);
    int lli = 0;

    // tone numbers that make each bit zero or one (for 4-FSK, 2 bits per symbol).
    int zeroi[2][2];
    int onei[2][2];

    for (int biti = 0; biti < 2; biti++)
    {
        if (biti == 0)
        {
            // high bit (bit 1)
            zeroi[0][0] = 0;
            zeroi[1][0] = 1;
            onei[0][0] = 2;
            onei[1][0] = 3;
        }

        if (biti == 1)
        {
            // low bit (bit 0)
            zeroi[0][1] = 0;
            zeroi[1][1] = 2;
            onei[0][1] = 1;
            onei[1][1] = 3;
        }
    }

    for (int i103 = 0; i103 < 103; i103++)
    {
        // FT4 sync blocks at positions: 0-3, 33-36, 66-69, 99-102
        if ((i103 < 4) || (i103 >= 33 && i103 < 37) || (i103 >= 66 && i103 < 70) || (i103 >= 99)) {
            // FT4 sync block, skip
            continue;
        }

        // for each of the two bits, look at the strongest tone
        // that would make it a zero, and the strongest tone that
        // would make it a one. use Bayes to decide which is more
        // likely, comparing each against the distribution of noise
        // and the distribution of strongest tones.
        // most-significant-bit first.

        for (int biti = 0; biti < 2; biti++)
        {
            // strongest tone that would make this bit be zero.
            int got_best_zero = 0;
            float best_zero = 0;

            for (int i = 0; i < 2; i++)
            {
                float x = m103[i103][zeroi[i][biti]];

                if (got_best_zero == 0 || x > best_zero)
                {
                    got_best_zero = 1;
                    best_zero = x;
                }
            }

            // strongest tone that would make this bit be one.
            int got_best_one = 0;
            float best_one = 0;

            for (int i = 0; i < 2; i++)
            {
                float x = m103[i103][onei[i][biti]];

                if (got_best_one == 0 || x > best_one)
                {
                    got_best_one = 1;
                    best_one = x;
                }
            }

            float ll = FT8::bayes(params, best_zero, best_one, lli, bests, all);
            ll174[lli++] = ll;
        }
    }
}

//
// turn 103 symbol numbers into 174 bits.
// strip out the four FT4 sync blocks,
// leaving 87 symbol numbers.
// each represents two bits (4-FSK).
// (all post-un-gray-code).
// str is per-symbol strength; must be positive.
// each returned element is < 0 for 1, > 0 for zero,
// scaled by str.
//
std::vector<float> FT4::extract_bits(const std::vector<int> &syms, const std::vector<float>& str) const
{
    std::vector<float> bits;

    for (int si = 0; si < 103; si++)
    {
        // FT4 sync blocks at positions: 0-3, 33-36, 66-69, 99-102
        bool inSync = (si < 4)
                   || (si >= 33 && si < 37)
                   || (si >= 66 && si < 70)
                   || (si >= 99);

        if (inSync)
        {
            // sync block -- skip
        }
        else
        {
            // Extract 2 bits per symbol for 4-FSK
            bits.push_back((syms[si] & 2) == 0 ? str[si] : -str[si]);
            bits.push_back((syms[si] & 1) == 0 ? str[si] : -str[si]);
        }
    }

    return bits;
}

// decode successive pairs of symbols. exploits the likelihood
// that they have the same phase, by summing the complex
// correlations for each possible pair and using the max.
void FT4::soft_decode_pairs(
    const FFTEngine::ffts_t &m103x,
    float ll174[]
)
{
    FFTEngine::ffts_t m103 = c_convert_to_snr(m103x);

    struct BitInfo
    {
        float zero; // strongest correlation that makes it zero
        float one;  // and one
    };

    std::vector<BitInfo> bitinfo(103 * 2);

    for (int i = 0; i < (int)bitinfo.size(); i++)
    {
        bitinfo[i].zero = 0;
        bitinfo[i].one = 0;
    }

    Stats all(params.problt_how_noise, params.log_tail, params.log_rate);
    Stats bests(params.problt_how_sig, params.log_tail, params.log_rate);
    int map[] = {0, 1, 3, 2}; // un-gray-code

    for (int si = 0; si < 103; si += 2)
    {
        float mx = 0;
        float corrs[4 * 4];

        for (int s1 = 0; s1 < 4; s1++)
        {
            for (int s2 = 0; s2 < 4; s2++)
            {
                // sum up the correlations.
                std::complex<float> csum = m103[si][s1];

                if (si + 1 < 103) {
                    csum += m103[si + 1][s2];
                }

                float x = std::abs(csum);
                corrs[s1 * 4 + s2] = x;

                if (x > mx) {
                    mx = x;
                }

                all.add(x);
                // first symbol
                int i = map[s1];

                for (int bit = 0; bit < 2; bit++)
                {
                    int bitind = (si + 0) * 2 + (1 - bit);

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
                if (si + 1 < 103)
                {
                    i = map[s2];

                    for (int bit = 0; bit < 2; bit++)
                    {
                        int bitind = (si + 1) * 2 + (1 - bit);

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

        // FT4 sync blocks are [0..3], [33..36], [66..69], [99..102].
        // Since we process pairs, sync pairs are start/start+1 and start+2/start+3.
        if (si == 0 || si == 33 || si == 66 || si == 99) {
            bests.add(corrs[kFT4SyncTones[(si / 33)][0] * 4 + kFT4SyncTones[(si / 33)][1]]);
        } else if (si == 2 || si == 35 || si == 68 || si == 101) {
            bests.add(corrs[kFT4SyncTones[((si - 2) / 33)][2] * 4 + kFT4SyncTones[((si - 2) / 33)][3]]);
        } else {
            bests.add(mx);
        }
    }

    int lli = 0;

    for (int si = 0; si < 103; si++)
    {
        if ((si < 4) || (si >= 33 && si < 37) || (si >= 66 && si < 70) || (si >= 99)) {
            // FT4 sync
            continue;
        }

        for (int i = 0; i < 2; i++)
        {
            float best_zero = bitinfo[si * 2 + i].zero;
            float best_one = bitinfo[si * 2 + i].one;
            float ll = FT8::bayes(params, best_zero, best_one, lli, bests, all);
            ll174[lli++] = ll;
        }
    }
}

void FT4::soft_decode_triples(
    const FFTEngine::ffts_t &m103x,
    float ll174[]
)
{
    FFTEngine::ffts_t m103 = c_convert_to_snr(m103x);

    struct BitInfo
    {
        float zero; // strongest correlation that makes it zero
        float one;  // and one
    };

    std::vector<BitInfo> bitinfo(103 * 2);

    for (int i = 0; i < (int)bitinfo.size(); i++)
    {
        bitinfo[i].zero = 0;
        bitinfo[i].one = 0;
    }

    Stats all(params.problt_how_noise, params.log_tail, params.log_rate);
    Stats bests(params.problt_how_sig, params.log_tail, params.log_rate);

    int map[] = {0, 1, 3, 2}; // un-gray-code

    for (int si = 0; si < 103; si += 3)
    {
        float mx = 0;
        float corrs[4 * 4 * 4];

        for (int s1 = 0; s1 < 4; s1++)
        {
            for (int s2 = 0; s2 < 4; s2++)
            {
                for (int s3 = 0; s3 < 4; s3++)
                {
                    std::complex<float> csum = m103[si][s1];

                    if (si + 1 < 103) {
                        csum += m103[si + 1][s2];
                    }

                    if (si + 2 < 103) {
                        csum += m103[si + 2][s3];
                    }

                    float x = std::abs(csum);
                    corrs[s1 * 16 + s2 * 4 + s3] = x;

                    if (x > mx) {
                        mx = x;
                    }

                    all.add(x);
                    // first symbol
                    int i = map[s1];

                    for (int bit = 0; bit < 2; bit++)
                    {
                        int bitind = (si + 0) * 2 + (1 - bit);

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
                    if (si + 1 < 103)
                    {
                        i = map[s2];

                        for (int bit = 0; bit < 2; bit++)
                        {
                            int bitind = (si + 1) * 2 + (1 - bit);

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
                    if (si + 2 < 103)
                    {
                        i = map[s3];

                        for (int bit = 0; bit < 2; bit++)
                        {
                            int bitind = (si + 2) * 2 + (1 - bit);

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

        // FT4 sync blocks: [0..3], [33..36], [66..69], [99..102].
        // Use sync-only triple windows (start/start+1/start+2) when available.
        int blockIndex = -1;
        int symbolInBlock = -1;

        if (si == 0 || si == 33 || si == 66 || si == 99)
        {
            blockIndex = si / 33;
            symbolInBlock = 0;
        }

        if (blockIndex >= 0)
        {
            int t1 = kFT4SyncTones[blockIndex][symbolInBlock + 0];
            int t2 = kFT4SyncTones[blockIndex][symbolInBlock + 1];
            int t3 = kFT4SyncTones[blockIndex][symbolInBlock + 2];
            bests.add(corrs[t1 * 16 + t2 * 4 + t3]);
        }
        else
        {
            bests.add(mx);
        }
    }

    int lli = 0;

    for (int si = 0; si < 103; si++)
    {
        if ((si < 4) || (si >= 33 && si < 37) || (si >= 66 && si < 70) || (si >= 99)) {
            // FT4 sync
            continue;
        }

        for (int i = 0; i < 2; i++)
        {
            float best_zero = bitinfo[si * 2 + i].zero;
            float best_one = bitinfo[si * 2 + i].one;
            float ll = FT8::bayes(params, best_zero, best_one, lli, bests, all);
            ll174[lli++] = ll;
        }
    }
}

//
// given log likelihood for each bit, try LDPC and OSD decoders.
// on success, puts corrected 174 bits into a174[].
//
int FT4::decode(const float ll174[], int a174[], const FT8Params& _params, int use_osd, std::string &comment)
{
    int plain[174];  // will be 0/1 bits.
    int ldpc_ok = 0; // 83 will mean success.
    LDPC::ldpc_decode((float *)ll174, _params.ldpc_iters, plain, &ldpc_ok);
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
        } else {
            comment = "CRC fail";
        }
    }
    else
    {
        comment = "LDPC fail";
    }

    if (use_osd && _params.osd_depth >= 0 && ldpc_ok >= _params.osd_ldpc_thresh)
    {
        int oplain[91];
        int got_depth = -1;
        int osd_ok = OSD::osd_decode((float *)ll174, _params.osd_depth, oplain, &got_depth);

        if (osd_ok)
        {
            // reconstruct all 174.
            comment += "OSD-" + std::to_string(got_depth) + "-" + std::to_string(ldpc_ok);
            OSD::ldpc_encode(oplain, a174);

            if (OSD::check_crc(a174)) {
                return 1;
            } else {
                comment = "CRC fail after " + comment;
            }
        }
        else
        {
            comment = "OSD fail";
        }
    }

    return 0;
}

//
// encode a 77 bit message into a 174 bit payload
// adds the 14 bit CRC to obtain 91 bits
// apply (174, 91) generator mastrix to obtain the 83 parity bits
// append the 83 bits to the 91 bits message + crc to obbain the 174 bit payload
//
void FT4::encode(int a174[], const int s77[])
{
    int a91[91]; // msg + CRC
    std::fill(a91, a91 + 91, 0);
    std::copy(s77, s77+77, a91); // copy msg
    LDPC::ft8_crc(a91, 82, &a91[77]); // append CRC - to match OSD::check_crc
    std::copy(a91, a91+91, a174); // copy msg + CRC
    int sum;
    int n;
    int ni;
    int b;

    for (int i=0; i<83; i++)
    {
        sum = 0;

        for (int j=0; j<91; j++)
        {
            n = j/8;  // byte index in the generator matrix
            ni = j%8;  // bit index in the generator matrix byte LSB first
            b = (Arrays::Gm[i][n] >> (7-ni)) & 1; // bit in the generator matrix
            sum += a91[j] * b;
        }

        a174[91+i] = sum % 2; // sum modulo 2
    }
}

//
// bandpass filter some FFT bins.
// smooth transition from stop-band to pass-band,
// so that it's not a brick-wall filter, so that it
// doesn't ring.
//
std::vector<std::complex<float>> FT4::fbandpass(
    const std::vector<std::complex<float>> &bins0,
    float bin_hz,
    float low_outer,  // start of transition
    float low_inner,  // start of flat area
    float high_inner, // end of flat area
    float high_outer  // end of transition
) const
{
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
// move hz down to 46.8, filter+convert to 666.7 samples/second.
//
// like fft_shift(). one big FFT, move bins down and
// zero out those outside the band, then IFFT,
// then re-sample.
//
// XXX maybe merge w/ fft_shift() / shift200().
//
std::vector<float> FT4::down_v7(const std::vector<float> &samples, float hz)
{
    int len = samples.size();
    std::vector<std::complex<float>> bins = fftEngine_->one_fft(samples, 0, len);

    return down_v7_f(bins, len, hz);
}

std::vector<float> FT4::down_v7_f(const std::vector<std::complex<float>> &bins, int len, float hz)
{
    int nbins = bins.size();
    float bin_hz = rate_ / (float)len;
    int down = round((hz - 46.8) / bin_hz);
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

    // now filter to fit in 666.7 samples/second.

    float low_inner = 46.8 - params.shoulder200_extra;
    float low_outer = low_inner - params.shoulder200;

    if (low_outer < 0) {
        low_outer = 0;
    }

    float high_inner = 93.6*1.5 - 20.833 + params.shoulder200_extra;
    float high_outer = high_inner + params.shoulder200;

    if (high_outer > 333.35) {
        high_outer = 333.35;
    }

    bins1 = fbandpass(
        bins1,
        bin_hz,
        low_outer,
        low_inner,
        high_inner,
        high_outer
    );

    // convert back to time domain and down-sample to 666.7 samples/second.
    int blen = round(len * (666.7 / rate_));
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
 int FT4::one_merge(const std::vector<std::complex<float>> &bins, int len, float hz, int off)
{
    //
    // set up to search for best frequency and time offset.
    //

    //
    // move down to 46.8 hz and re-sample to 666.7 samples/second,
    // i.e. 32 samples/symbol.
    //
    std::vector<float> samples200 = down_v7_f(bins, len, hz);
    int off200 = round((off / (float)rate_) * 666.7);
    int ret = one_iter(samples200, off200, hz);
    return ret;
}

// return 2 if it decodes to a brand-new message.
// return 1 if it decodes but we've already seen it,
//   perhaps in a different pass.
// return 0 if we could not decode.
int FT4::one_iter(const std::vector<float> &samples200, int best_off, float hz_for_cb)
{
    if (params.do_second)
    {
        std::vector<Strength> strengths = search_both(
            samples200,
            46.8,
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
        int ret = one_iter1(samples200, best_off, 46.8, hz_for_cb, hz_for_cb);
        return ret;
    }

    return 0;
}

//
// estimate SNR, yielding numbers vaguely similar to WSJT-X.
// m103 is a 103x4 complex FFT output.
//
float FT4::guess_snr(const FFTEngine::ffts_t &m103) const
{
    float pnoises = 0;
    float psignals = 0;

    for (int si = 0; si < 103; si++)
    {
        if (si >= (int)m103.size() || m103[si].size() < 4) {
            continue;
        }

        std::vector<float> v(4);
        float sum = 0;

        for (int bi = 0; bi < 4; bi++)
        {
            v[bi] = std::abs(m103[si][bi]);
            sum += v[bi];
        }

        int blockIndex = -1;
        int symbolInBlock = -1;

        if (si < 4)
        {
            blockIndex = 0;
            symbolInBlock = si;
        }
        else if (si >= 33 && si < 37)
        {
            blockIndex = 1;
            symbolInBlock = si - 33;
        }
        else if (si >= 66 && si < 70)
        {
            blockIndex = 2;
            symbolInBlock = si - 66;
        }
        else if (si >= 99)
        {
            blockIndex = 3;
            symbolInBlock = si - 99;
        }

        if (blockIndex >= 0)
        {
            // FT4 sync symbols: use known sync tone.
            int expectedTone = kFT4SyncTones[blockIndex][symbolInBlock];
            psignals += v[expectedTone];
            pnoises += (sum - v[expectedTone]) / 3;
        }
        else
        {
            // Data symbols: strongest tone is likely signal.
            std::sort(v.begin(), v.end());
            psignals += v[3];
            pnoises += (v[0] + v[1] + v[2]) / 3;
        }
    }

    pnoises /= 103;
    psignals /= 103;

    if (pnoises < 1e-12f) {
        pnoises = 1e-12f;
    }

    pnoises *= pnoises; // square yields power
    psignals *= psignals;
    float raw = psignals / pnoises;
    raw -= 1; // turn (s+n)/n into s/n

    if (raw < 0.1) {
        raw = 0.1;
    }

    raw /= (2500.0 / 9.0); // 9.0 hz noise b/w -> 2500 hz b/w (FT4: 3.33x wider than FT8's 2.7 Hz)
    float snr = 10 * log10(raw);
    // empirically adjust to match WSJT-X's SNR numbers
    snr += 5;
    snr *= 1.2;
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
// hz0 is actual FFT bin number of m103[...][0].
//
// the output adj_hz is relative to the FFT bin center;
// a positive number means the real signal seems to be
// a bit higher in frequency that the bin center.
//
// adj_off is the amount to change the offset, in samples.
// should be subtracted from offset.
//
void FT4::fine(const FFTEngine::ffts_t &m103, int, float &adj_hz, float &adj_off) const
{
    adj_hz = 0.0;
    adj_off = 0.0;
    // tone number for each of the 103 symbols.
    int sym[103];
    float symval[103];
    float symphase[103];
    const int nSymbols = std::min(103, (int)m103.size());

    for (int i = 0; i < nSymbols; i++)
    {
        int blockIndex = -1;
        int symbolInBlock = -1;

        if (i < 4)
        {
            blockIndex = 0;
            symbolInBlock = i;
        }
        else if (i >= 33 && i < 37)
        {
            blockIndex = 1;
            symbolInBlock = i - 33;
        }
        else if (i >= 66 && i < 70)
        {
            blockIndex = 2;
            symbolInBlock = i - 66;
        }
        else if (i >= 99)
        {
            blockIndex = 3;
            symbolInBlock = i - 99;
        }

        if (blockIndex >= 0)
        {
            // FT4 sync symbol: use expected sync tone.
            sym[i] = kFT4SyncTones[blockIndex][symbolInBlock];
        }
        else
        {
            int mxj = -1;
            float mx = 0;

            for (int j = 0; j < 4; j++)
            {
                float x = std::abs(m103[i][j]);

                if (mxj < 0 || x > mx)
                {
                    mx = x;
                    mxj = j;
                }
            }

            sym[i] = mxj;
        }

        symphase[i] = std::arg(m103[i][sym[i]]);
        symval[i] = std::abs(m103[i][sym[i]]);
    }

    float sum = 0;
    float weight_sum = 0;

    for (int i = 0; i < nSymbols - 1; i++)
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

    if (weight_sum == 0.0f) {
        return;
    }

    float mean = sum / weight_sum;
    float err_rad = mean; // radians per symbol time
    float err_hz = (err_rad / (2 * M_PI)) / 0.048; // FT4 symbol time is 0.048s

    // if each symbol's phase is a bit more than we expect,
    // that means the real frequency is a bit higher
    // than we thought, so increase our estimate.
    adj_hz = err_hz;

    //
    // now think about offset error.
    //
    // the higher tones have many cycles per
    // symbol -- high tones have more cycles
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

    for (int i = 1; i < nSymbols; i++)
    {
        float ph0 = symphase[i - 1];
        float ph = symphase[i];
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
            adj_off -= params.fine_max_off;
        }
    }
}

//
// the signal is centered around 46.8 Hz in samples200.
//
// return 2 if it decodes to a brand-new message.
// return 1 if it decodes but we've already seen it,
//   perhaps in a different pass.
// return 0 if we could not decode.
//
int FT4::one_iter1(
    const std::vector<float> &samples200x,
    int best_off,
    float best_hz,
    float hz0_for_cb,
    float hz1_for_cb
)
{
    // put best_hz at the FT4 center frequency.
    std::vector<float> samples200 = shift200(
        samples200x,
        0,
        samples200x.size(),
        best_hz
    );

    // mini 103x4 FFT.
    FFTEngine::ffts_t m103 = extract(samples200, 46.8, best_off);

    // look at symbol-to-symbol phase change to try
    // to improve best_hz and best_off.
    if (params.do_fine_hz || params.do_fine_off)
    {
        float adj_hz = 0;
        float adj_off = 0;
        fine(m103, 4, adj_hz, adj_off);

        if (params.do_fine_hz == 0) {
            adj_hz = 0;
        }

        if (params.do_fine_off == 0) {
            adj_off = 0;
        }

        if (fabs(adj_hz) < 20.833 / 4 && fabs(adj_off) < 4)
        {
            best_hz += adj_hz;
            best_off += round(adj_off);

            if (best_off < 0) {
                best_off = 0;
            }

            samples200 = shift200(samples200x, 0, samples200x.size(), best_hz);
            m103 = extract(samples200, 46.8, best_off);
        }
    }

    float ll174[174];

    if (params.soft_ones)
    {
        if (params.soft_ones == 1) {
            soft_decode(m103, ll174);
        } else {
            c_soft_decode(m103, ll174);
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
            m103
        );

        if (ret) {
            return ret;
        }
    }

    if (params.soft_pairs)
    {
        float p174[174];
        soft_decode_pairs(m103, p174);
        int ret = try_decode(
            samples200,
            p174,
            best_hz,
            best_off,
            hz0_for_cb,
            hz1_for_cb,
            params.use_osd,
            "",
            m103
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
        soft_decode_triples(m103, p174);
        int ret = try_decode(
            samples200,
            p174,
            best_hz,
            best_off,
            hz0_for_cb,
            hz1_for_cb,
            params.use_osd,
            "",
            m103
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
                m103
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
                m103
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
// re103[] holds the error-corrected symbol numbers.
//
void FT4::subtract(
    const std::vector<int>& re103,
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

    if (bin0 + 4 > (int)bins[0].size()) {
        return;
    }

    if ((int)bins.size() < 103) {
        return;

    }

    std::vector<float> phases(103);
    std::vector<float> amps(103);

    for (int i = 0; i < 103; i++)
    {
        int sym = bin0 + re103[i];
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

    if (params.subtract_edge_symbols)
    {
        int sym = bin0 + re103[0];
        float phase = phases[0];
        float amp = amps[0];
        float hz = sym * bin_hz;
        float dtheta = 2 * M_PI / (rate_ / hz); // advance per sample

        for (int jj = 0; jj < ramp; jj++)
        {
            int iii = off0 - ramp + jj;

            if (iii < 0 || iii >= (int)moved.size()) {
                continue;
            }

            float theta = phase + (jj - ramp) * dtheta;
            float x = amp * cos(theta);
            x *= jj / (float)ramp;
            moved[iii] -= x;
        }
    }

    // initial ramp part of first symbol.
    {
        int sym = bin0 + re103[0];
        float phase = phases[0];
        float amp = amps[0];
        float hz = sym * bin_hz;
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

    for (int si = 0; si < 103; si++)
    {
        int sym = bin0 + re103[si];
        float phase = phases[si];
        float amp = amps[si];
        float hz = sym * bin_hz;
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

        if (si + 1 >= 103)
        {
            hz1 = hz;
            phase1 = phase;
        }
        else
        {
            int sym1 = bin0 + re103[si + 1];
            hz1 = sym1 * bin_hz;
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

        if (si == 103 - 1) {
            end = block;
        }

        for (int jj = block - ramp; jj < end; jj++)
        {
            int iii = off0 + block * si + jj;
            float x = amp * cos(theta);

            // trail off to zero at the very end.
            if (si == 103 - 1) {
                x *= 1.0 - ((jj - (block - ramp)) / (float)ramp);
            }

            moved[iii] -= x;
            theta += dtheta;
            dtheta += inc;
            theta += adj / (2.0 * ramp);
        }
    }

    if (params.subtract_edge_symbols)
    {
        int sym = bin0 + re103[102];
        float phase = phases[102];
        float amp = amps[102];
        float hz = sym * bin_hz;
        float dtheta = 2 * M_PI / (rate_ / hz); // advance per sample

        for (int jj = 0; jj < ramp; jj++)
        {
            int iii = off0 + block * 103 + jj;

            if (iii < 0 || iii >= (int)moved.size()) {
                continue;
            }

            float theta = phase + (block + jj) * dtheta;
            float x = amp * cos(theta);
            x *= 1.0 - (jj / (float)ramp);
            moved[iii] -= x;
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
int FT4::try_decode(
    const std::vector<float> &samples200,
    float ll174[174],
    float best_hz,
    int best_off_samples,
    float hz0_for_cb,
    float,
    int use_osd,
    const char *comment1,
    const FFTEngine::ffts_t &m103
)
{
    int a174[174];
    std::string comment(comment1);

    if (decode(ll174, a174, params, use_osd, comment))
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

        // reconstruct correct 103 symbols from LDPC output.
        std::vector<int> re103 = recode(a174);

        if (params.do_third == 1)
        {
            // fine-tune offset and hz for better subtraction.
            float best_off = best_off_samples / 666.7;
            search_both_known(
                samples200,
                666.7,
                re103,
                best_hz,
                best_off,
                best_hz,
                best_off
            );
            best_off_samples = round(best_off * 666.7);
        }

        // convert starting sample # from 666.7 samples/second back to rate_.
        // also hz.
        float best_off = best_off_samples / 666.7; // convert to seconds
        best_hz = hz0_for_cb + (best_hz - 46.8);

        if (params.do_third == 2)
        {
            // fine-tune offset and hz for better subtraction.
            search_both_known(
                samples_,
                rate_,
                re103,
                best_hz,
                best_off,
                best_hz,
                best_off
            );
        }

        float snr = guess_snr(m103);

        int a91[91];

        for (int i = 0; i < 91; i++) {
            a91[i] = a174[i];
        }

        for (int i = 0; i < 77; i++) {
            a91[i] ^= kFT4Rvec[i];
        }

        if (cb_)
        {
            cb_mu_.lock();
            int ret = cb_->hcb(
                a91,
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
                subtract(re103, best_hz, best_hz, best_off);
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
std::vector<int> FT4::recode(const int a174[]) const
{
    int i174 = 0;
    std::vector<int> out103;

    // FT4 uses 4 sync blocks of 4 symbols each (4-FSK)
    // Sync blocks at symbol positions: 0-3, 33-36, 66-69, 99-102
    // 87 data symbols carrying 174 bits (2 bits per symbol)

    for (int i103 = 0; i103 < 103; i103++)
    {
        // Check if this is a sync symbol position
        bool inSync = false;
        int blockIndex = -1;
        int symbolInBlock = -1;

        if (i103 >= 0 && i103 < 4)
        {
            inSync = true;
            blockIndex = 0;
            symbolInBlock = i103;
        }
        else if (i103 >= 33 && i103 < 37)
        {
            inSync = true;
            blockIndex = 1;
            symbolInBlock = i103 - 33;
        }
        else if (i103 >= 66 && i103 < 70)
        {
            inSync = true;
            blockIndex = 2;
            symbolInBlock = i103 - 66;
        }
        else if (i103 >= 99)
        {
            inSync = true;
            blockIndex = 3;
            symbolInBlock = i103 - 99;
        }

        if (inSync)
        {
            // Insert FT4 sync tone from kFT4SyncTones
            out103.push_back(kFT4SyncTones[blockIndex][symbolInBlock]);
        }
        else
        {
            // Data symbol: 2 bits encoded as 4-FSK with Gray coding
            int sym = (a174[i174 + 0] << 1) | (a174[i174 + 1] << 0);
            i174 += 2;
            // FT4 Gray code mapping for 4-FSK (Table 3 in FT4_FT8_QEX.pdf):
            // 00 -> 0, 01 -> 1, 11 -> 2, 10 -> 3
            int map[] = {0, 1, 3, 2};
            sym = map[sym];
            out103.push_back(sym);
        }
    }

    return out103;
}

FT4Decoder::~FT4Decoder()
{
    forceQuit(); // stop all remaining running threads if any

    for (auto& fftEngine : fftEngines) {
        delete fftEngine;
    }
}

//
// Launch decoding
//
void FT4Decoder::entry(
    const float xsamples[],
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
    const struct cdecode *xprevdecs
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

        auto *ft4 = new FT4(
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

        ft4->getParams() = getParams(); // transfer parameters
        int npasses = nprevdecs > 0 ? params.npasses_two : params.npasses_one;
        ft4->set_npasses(npasses);
        auto *th = new QThread();
        th->setObjectName(tr("ft4:%1:%2").arg(cb->get_name()).arg(i));
        threads.push_back(th);
        ft4->moveToThread(th);
        QObject::connect(th, &QThread::started, ft4, &FT4::start_work);
        QObject::connect(ft4, &FT4::finished, th, &QThread::quit, Qt::DirectConnection);
        QObject::connect(th, &QThread::finished, ft4, &QObject::deleteLater);
        QObject::connect(th, &QThread::finished, th, &QThread::deleteLater);
        th->start();
    }
}

void FT4Decoder::wait(double time_left)
{
    unsigned long thread_timeout = time_left * 1000;

    while (threads.size() != 0)
    {
        bool success = threads.front()->wait(thread_timeout);

        if (!success)
        {
            qDebug("FT4::FT4Decoder::wait: thread timed out");
            thread_timeout = 50; // only 50ms for the rest
        }

        threads.erase(threads.begin());
    }
}

void FT4Decoder::forceQuit()
{
    while (threads.size() != 0)
    {
        threads.front()->quit();
        threads.front()->wait();
        threads.erase(threads.begin());
    }
}

} // namespace FT8
