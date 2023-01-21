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

#include <assert.h>
#include <QDebug>
#include "fft.h"
#include "util.h"

#define TIMING 0

namespace FT8 {

FFTEngine* FFTEngine::m_instance= nullptr;;

FFTEngine *FFTEngine::GetInstance()
{
    if (!m_instance) {
        m_instance = new FFTEngine();
    }

    return m_instance;
}

FFTEngine::Plan *FFTEngine::get_plan(int n, const char *why)
{
    // cache fftw plans in the parent process,
    // so they will already be there for fork()ed children.

    m_plansmu.lock();

    for (int i = 0; i < m_nplans; i++)
    {
        if (m_plans[i]->n_ == n && m_plans[i]->type_ == M_FFTW_TYPE
#if TIMING
            && strcmp(plans[i]->why_, why) == 0
#endif
        )
        {
            Plan *p = m_plans[i];
            p->uses_ += 1;
            m_plansmu.unlock();
            return p;
        }
    }

#if TIMING
    double t0 = now();
#endif

    // fftw_make_planner_thread_safe();
    m_plansmu2.lock();

    fftwf_set_timelimit(5);

    //
    // real -> complex
    //

    Plan *p = new Plan;

    p->n_ = n;
#if TIMING
    p->time_ = 0;
#endif
    p->uses_ = 1;
    p->why_ = why;
    p->r_ = (float *)fftwf_malloc(n * sizeof(float));
    assert(p->r_);
    p->c_ = (fftwf_complex *)fftwf_malloc(((n / 2) + 1) * sizeof(fftwf_complex));
    assert(p->c_);

    // FFTW_ESTIMATE
    // FFTW_MEASURE
    // FFTW_PATIENT
    // FFTW_EXHAUSTIVE
    int type = M_FFTW_TYPE;
    p->type_ = type;
    p->fwd_ = fftwf_plan_dft_r2c_1d(n, p->r_, p->c_, type);
    assert(p->fwd_);
    p->rev_ = fftwf_plan_dft_c2r_1d(n, p->c_, p->r_, type);
    assert(p->rev_);

    //
    // complex -> complex
    //
    p->cc1_ = (fftwf_complex *)fftwf_malloc(n * sizeof(fftwf_complex));
    assert(p->cc1_);
    p->cc2_ = (fftwf_complex *)fftwf_malloc(n * sizeof(fftwf_complex));
    assert(p->cc2_);
    p->cfwd_ = fftwf_plan_dft_1d(n, p->cc1_, p->cc2_, FFTW_FORWARD, type);
    assert(p->cfwd_);
    p->crev_ = fftwf_plan_dft_1d(n, p->cc2_, p->cc1_, FFTW_BACKWARD, type);
    assert(p->crev_);

    m_plansmu2.unlock();

    assert(m_nplans + 1 < 1000);

    m_plans[m_nplans] = p;
    m_nplans += 1;

#if TIMING
    if (0 && getpid() == plan_master_pid)
    {
        double t1 = now();
        fprintf(stderr, "miss pid=%d master=%d n=%d t=%.3f total=%d type=%d, %s\n",
                getpid(), plan_master_pid, n, t1 - t0, nplans, type, why);
    }
#endif

    m_plansmu.unlock();

    return p;
}

//
// do just one FFT on samples[i0..i0+block]
// real inputs, complex outputs.
// output has (block / 2) + 1 points.
//
std::vector<std::complex<float>> FFTEngine::one_fft(
    const std::vector<float> &samples,
    int i0,
    int block,
    const char *why,
    FFTEngine::Plan *p
)
{
    assert(i0 >= 0);
    assert(block > 1);

    int nsamples = samples.size();
    int nbins = (block / 2) + 1;

    if (p)
    {
        assert(p->n_ == block);
        p->uses_ += 1;
    }
    else
    {
        p = get_plan(block, why);
    }
    fftwf_plan m_plan = p->fwd_;

#if TIMING
    double t0 = now();
#endif

    assert((int)samples.size() - i0 >= block);

    int m_in_allocated = 0;
    float *m_in = (float *)samples.data() + i0;

    if ((((unsigned long long)m_in) % 16) != 0)
    {
        // m_in must be on a 16-byte boundary for FFTW.
        m_in = (float *)fftwf_malloc(sizeof(float) * p->n_);
        assert(m_in);
        m_in_allocated = 1;
        for (int i = 0; i < block; i++)
        {
            if (i0 + i < nsamples)
            {
                m_in[i] = samples[i0 + i];
            }
            else
            {
                m_in[i] = 0;
            }
        }
    }

    fftwf_complex *m_out = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * ((p->n_ / 2) + 1));
    assert(m_out);

    fftwf_execute_dft_r2c(m_plan, m_in, m_out);

    std::vector<std::complex<float>> out(nbins);

    for (int bi = 0; bi < nbins; bi++)
    {
        float re = m_out[bi][0];
        float im = m_out[bi][1];
        out[bi] = std::complex<float>(re, im);
    }

    if (m_in_allocated)
        fftwf_free(m_in);
    fftwf_free(m_out);

#if TIMING
    p->time_ += now() - t0;
#endif

    return out;
}

//
// do a full set of FFTs, one per symbol-time.
// bins[time][frequency]
//
FFTEngine::ffts_t FFTEngine::ffts(const std::vector<float> &samples, int i0, int block, const char *why)
{
    assert(i0 >= 0);
    assert(block > 1 && (block % 2) == 0);

    int nsamples = samples.size();
    int nbins = (block / 2) + 1;
    int nblocks = (nsamples - i0) / block;
    ffts_t bins(nblocks);
    for (int si = 0; si < nblocks; si++)
    {
        bins[si].resize(nbins);
    }

    Plan *p = get_plan(block, why);
    fftwf_plan m_plan = p->fwd_;

#if TIMING
    double t0 = now();
#endif

    // allocate our own b/c using p->m_in and p->m_out isn't thread-safe.
    float *m_in = (float *)fftwf_malloc(sizeof(float) * p->n_);
    fftwf_complex *m_out = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * ((p->n_ / 2) + 1));
    assert(m_in && m_out);

    // float *m_in = p->r_;
    // fftw_complex *m_out = p->c_;

    for (int si = 0; si < nblocks; si++)
    {
        int off = i0 + si * block;
        for (int i = 0; i < block; i++)
        {
            if (off + i < nsamples)
            {
                float x = samples[off + i];
                m_in[i] = x;
            }
            else
            {
                m_in[i] = 0;
            }
        }

        fftwf_execute_dft_r2c(m_plan, m_in, m_out);

        for (int bi = 0; bi < nbins; bi++)
        {
            float re = m_out[bi][0];
            float im = m_out[bi][1];
            std::complex<float> c(re, im);
            bins[si][bi] = c;
        }
    }

    fftwf_free(m_in);
    fftwf_free(m_out);

#if TIMING
    p->time_ += now() - t0;
#endif

    return bins;
}

//
// do just one FFT on samples[i0..i0+block]
// real inputs, complex outputs.
// output has block points.
//
std::vector<std::complex<float>> FFTEngine::one_fft_c(
    const std::vector<float> &samples,
    int i0,
    int block,
    const char *why
)
{
    assert(i0 >= 0);
    assert(block > 1);

    int nsamples = samples.size();

    Plan *p = get_plan(block, why);
    fftwf_plan m_plan = p->cfwd_;

#if TIMING
    double t0 = now();
#endif

    fftwf_complex *m_in = (fftwf_complex *)fftwf_malloc(block * sizeof(fftwf_complex));
    fftwf_complex *m_out = (fftwf_complex *)fftwf_malloc(block * sizeof(fftwf_complex));
    assert(m_in && m_out);

    for (int i = 0; i < block; i++)
    {
        if (i0 + i < nsamples)
        {
            m_in[i][0] = samples[i0 + i]; // real
        }
        else
        {
            m_in[i][0] = 0;
        }
        m_in[i][1] = 0; // imaginary
    }

    fftwf_execute_dft(m_plan, m_in, m_out);

    std::vector<std::complex<float>> out(block);

    float norm = 1.0 / sqrt(block);
    for (int bi = 0; bi < block; bi++)
    {
        float re = m_out[bi][0];
        float im = m_out[bi][1];
        std::complex<float> c(re, im);
        c *= norm;
        out[bi] = c;
    }

    fftwf_free(m_in);
    fftwf_free(m_out);

#if TIMING
    p->time_ += now() - t0;
#endif

    return out;
}

std::vector<std::complex<float>> FFTEngine::one_fft_cc(
    const std::vector<std::complex<float>> &samples,
    int i0,
    int block,
    const char *why
)
{
    assert(i0 >= 0);
    assert(block > 1);

    int nsamples = samples.size();

    Plan *p = get_plan(block, why);
    fftwf_plan m_plan = p->cfwd_;

#if TIMING
    double t0 = now();
#endif

    fftwf_complex *m_in = (fftwf_complex *)fftwf_malloc(block * sizeof(fftwf_complex));
    fftwf_complex *m_out = (fftwf_complex *)fftwf_malloc(block * sizeof(fftwf_complex));
    assert(m_in && m_out);

    for (int i = 0; i < block; i++)
    {
        if (i0 + i < nsamples)
        {
            m_in[i][0] = samples[i0 + i].real();
            m_in[i][1] = samples[i0 + i].imag();
        }
        else
        {
            m_in[i][0] = 0;
            m_in[i][1] = 0;
        }
    }

    fftwf_execute_dft(m_plan, m_in, m_out);

    std::vector<std::complex<float>> out(block);

    // float norm = 1.0 / sqrt(block);
    for (int bi = 0; bi < block; bi++)
    {
        float re = m_out[bi][0];
        float im = m_out[bi][1];
        std::complex<float> c(re, im);
        // c *= norm;
        out[bi] = c;
    }

    fftwf_free(m_in);
    fftwf_free(m_out);

#if TIMING
    p->time_ += now() - t0;
#endif

    return out;
}

std::vector<std::complex<float>> FFTEngine::one_ifft_cc(
    const std::vector<std::complex<float>> &bins,
    const char *why
)
{
    int block = bins.size();

    Plan *p = get_plan(block, why);
    fftwf_plan m_plan = p->crev_;

#if TIMING
    double t0 = now();
#endif

    fftwf_complex *m_in = (fftwf_complex *)fftwf_malloc(block * sizeof(fftwf_complex));
    fftwf_complex *m_out = (fftwf_complex *)fftwf_malloc(block * sizeof(fftwf_complex));
    assert(m_in && m_out);

    for (int bi = 0; bi < block; bi++)
    {
        float re = bins[bi].real();
        float im = bins[bi].imag();
        m_in[bi][0] = re;
        m_in[bi][1] = im;
    }

    fftwf_execute_dft(m_plan, m_in, m_out);

    std::vector<std::complex<float>> out(block);
    float norm = 1.0 / sqrt(block);
    for (int i = 0; i < block; i++)
    {
        float re = m_out[i][0];
        float im = m_out[i][1];
        std::complex<float> c(re, im);
        c *= norm;
        out[i] = c;
    }

    fftwf_free(m_in);
    fftwf_free(m_out);

#if TIMING
    p->time_ += now() - t0;
#endif

    return out;
}

std::vector<float> FFTEngine::one_ifft(const std::vector<std::complex<float>> &bins, const char *why)
{
    int nbins = bins.size();
    int block = (nbins - 1) * 2;

    Plan *p = get_plan(block, why);
    fftwf_plan m_plan = p->rev_;

#if TIMING
    double t0 = now();
#endif

    fftwf_complex *m_in = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * ((p->n_ / 2) + 1));
    float *m_out = (float *)fftwf_malloc(sizeof(float) * p->n_);

    for (int bi = 0; bi < nbins; bi++)
    {
        float re = bins[bi].real();
        float im = bins[bi].imag();
        m_in[bi][0] = re;
        m_in[bi][1] = im;
    }

    fftwf_execute_dft_c2r(m_plan, m_in, m_out);

    std::vector<float> out(block);
    for (int i = 0; i < block; i++)
    {
        out[i] = m_out[i];
    }

    fftwf_free(m_in);
    fftwf_free(m_out);

#if TIMING
    p->time_ += now() - t0;
#endif

    return out;
}

//
// return the analytic signal for signal x,
// just like scipy.signal.hilbert(), from which
// this code is copied.
//
// the return value is x + iy, where y is the hilbert transform of x.
//
std::vector<std::complex<float>> FFTEngine::analytic(const std::vector<float> &x, const char *why)
{
    ulong n = x.size();

    std::vector<std::complex<float>> y = one_fft_c(x, 0, n, why);
    assert(y.size() == n);

    // leave y[0] alone.
    // float the first (positive) half of the spectrum.
    // zero out the second (negative) half of the spectrum.
    // y[n/2] is the nyquist bucket if n is even; leave it alone.
    if ((n % 2) == 0)
    {
        for (ulong i = 1; i < n / 2; i++)
            y[i] *= 2;
        for (ulong i = n / 2 + 1; i < n; i++)
            y[i] = 0;
    }
    else
    {
        for (ulong i = 1; i < (n + 1) / 2; i++)
            y[i] *= 2;
        for (ulong i = (n + 1) / 2; i < n; i++)
            y[i] = 0;
    }

    std::vector<std::complex<float>> z = one_ifft_cc(y, why);

    return z;
}

//
// general-purpose shift x in frequency by hz.
// uses hilbert transform to avoid sidebands.
// but it does wrap around at 0 hz and the nyquist frequency.
//
// note analytic() does an FFT over the whole signal, which
// is expensive, and often re-used, but it turns out it
// isn't a big factor in overall run-time.
//
// like weakutil.py's freq_shift().
//
std::vector<float> FFTEngine::hilbert_shift(const std::vector<float> &x, float hz0, float hz1, int rate)
{
    // y = scipy.signal.hilbert(x)
    std::vector<std::complex<float>> y = analytic(x, "hilbert_shift");
    assert(y.size() == x.size());

    float dt = 1.0 / rate;
    int n = x.size();

    std::vector<float> ret(n);

    for (int i = 0; i < n; i++)
    {
        // complex "local oscillator" at hz.
        float hz = hz0 + (i / (float)n) * (hz1 - hz0);
        std::complex<float> lo = std::exp(std::complex<float>(0.0, 2 * M_PI * hz * dt * i));
        ret[i] = (lo * y[i]).real();
    }

    return ret;
}

void FFTEngine::fft_stats()
{
    for (int i = 0; i < m_nplans; i++)
    {
        Plan *p = m_plans[i];
        qDebug("FT8::FFTEngine::fft_stats: %-13s %6d %9d %6.3fn",
                p->why_,
                p->n_,
                p->uses_,
#if TIMING
                p->time_
#else
                0.0
#endif
        );
    }
}

} // namespace FT8
