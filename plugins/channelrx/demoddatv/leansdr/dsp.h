// This file is part of LeanSDR Copyright (C) 2016-2018 <pabr@pabr.org>.
// See the toplevel README for more information.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef LEANSDR_DSP_H
#define LEANSDR_DSP_H

#include "leansdr/framework.h"
#include "leansdr/math.h"
#include <math.h>

namespace leansdr
{

//////////////////////////////////////////////////////////////////////
// DSP blocks
//////////////////////////////////////////////////////////////////////

// [cconverter] converts std::complex streams between numric types,
// with optional ofsetting and rational scaling.
template <typename Tin, int Zin, typename Tout, int Zout, int Gn, int Gd>
struct cconverter : runnable
{
    cconverter(
        scheduler *sch,
        pipebuf<std::complex<Tin>> &_in,
        pipebuf<std::complex<Tout>> &_out
    ) :
        runnable(sch, "cconverter"),
        in(_in),
        out(_out)
    {
    }

    void run()
    {
        unsigned long count = min(in.readable(), out.writable());
        std::complex<Tin> *pin = in.rd(), *pend = pin + count;
        std::complex<Tout> *pout = out.wr();

        for (; pin < pend; ++pin, ++pout)
        {
            pout->re = Zout + (pin->re - (Tin)Zin) * Gn / Gd;
            pout->im = Zout + (pin->im - (Tin)Zin) * Gn / Gd;
        }

        in.read(count);
        out.written(count);
    }

  private:
    pipereader<std::complex<Tin>> in;
    pipewriter<std::complex<Tout>> out;
};

template <typename T>
struct cfft_engine
{
    cfft_engine(int _n) :
        bitrev(nullptr),
        omega(nullptr),
        omega_rev(nullptr)
    {
        init(_n);
    }

    ~cfft_engine() {
        release();
    }

    int size() {
        return n;
    }

    void init(int _n)
    {
        release();
        n = _n;
        invsqrtn = 1.0 / sqrt(n);

        // Compute log2(n)
        logn = 0;
        for (int t = n; t > 1; t >>= 1) {
            ++logn;
        }

        // Bit reversal
        bitrev = new int[n];

        for (int i = 0; i < n; ++i)
        {
            bitrev[i] = 0;

            for (int b = 0; b < logn; ++b) {
                bitrev[i] = (bitrev[i] << 1) | ((i >> b) & 1);
            }
        }

        // Float constants
        omega = new std::complex<T>[n];
        omega_rev = new std::complex<T>[n];

        for (int i = 0; i < n; ++i)
        {
            float a = 2.0 * M_PI * i / n;
            omega_rev[i].real(cosf(a));
            omega[i].real(cosf(a));
            omega_rev[i].imag(sinf(a));
            omega[i].imag(sinf(a));
            // omega_rev[i].re = (omega[i].re = cosf(a));
            // omega_rev[i].im = -(omega[i].im = sinf(a));
        }
    }

    void inplace(std::complex<T> *data, bool reverse = false)
    {
        // Bit-reversal permutation
        for (int i = 0; i < n; ++i)
        {
            int r = bitrev[i];

            if ((r < 0) || (r >= n)) // corruption: init again and exit
            {
                fprintf(stderr, "cfft_engine::inplace: corruption detected\n");
                init(n);
                return;
            }

            if (r < i)
            {
                std::complex<T> tmp = data[i];
                data[i] = data[r];
                data[r] = tmp;
            }
        }

        std::complex<T> *om = reverse ? omega_rev : omega;
        // Danielson-Lanczos
        for (int i = 0; i < logn; ++i)
        {
            int hbs = 1 << i;
            int dom = 1 << (logn - 1 - i);

            for (int j = 0; j < dom; ++j)
            {
                int p = j * hbs * 2, q = p + hbs;

                for (int k = 0; k < hbs; ++k)
                {
                    std::complex<T> &w = om[k * dom];
                    std::complex<T> &dqk = data[q + k];
                    std::complex<T> x(w.real() * dqk.real() - w.imag() * dqk.imag(),
                                 w.real() * dqk.imag() + w.imag() * dqk.real());
                    data[q + k].real(data[p + k].real() - x.real());
                    data[q + k].imag(data[p + k].imag() - x.imag());
                    data[p + k].real(data[p + k].real() + x.real());
                    data[p + k].imag(data[p + k].imag() + x.imag());
                }
            }
        }

        if (reverse)
        {
            float invn = 1.0 / n;

            for (int i = 0; i < n; ++i) {
                data[i] *= invn;
            }
        }
    }

  private:
    void release()
    {
        if (bitrev) {
            delete[] bitrev;
        }
        if (omega) {
            delete[] omega;
        }
        if (omega_rev) {
            delete[] omega_rev;
        }
    }

    int *bitrev;
    std::complex<T> *omega;
    std::complex<T> *omega_rev;
    int n;
    float invsqrtn;
    int logn;
};

template <typename T>
struct adder : runnable
{
    adder(
        scheduler *sch,
        pipebuf<T> &_in1,
        pipebuf<T> &_in2,
        pipebuf<T> &_out
    ) :
        runnable(sch, "adder"),
        in1(_in1),
        in2(_in2),
        out(_out)
    {
    }

    void run()
    {
        int n = out.writable();

        if (in1.readable() < n) {
            n = in1.readable();
        }

        if (in2.readable() < n) {
            n = in2.readable();
        }

        T *pin1 = in1.rd(), *pin2 = in2.rd(), *pout = out.wr(), *pend = pout + n;

        while (pout < pend) {
            *pout++ = *pin1++ + *pin2++;
        }

        in1.read(n);
        in2.read(n);
        out.written(n);
    }

  private:
    pipereader<T> in1, in2;
    pipewriter<T> out;
};

template <typename Tscale, typename Tin, typename Tout>
struct scaler : runnable
{
    Tscale scale;

    scaler(
        scheduler *sch,
        Tscale _scale,
        pipebuf<Tin> &_in,
        pipebuf<Tout> &_out
    ) :
        runnable(sch, "scaler"),
        scale(_scale),
        in(_in),
        out(_out)
    {
    }

    void run()
    {
        unsigned long count = min(in.readable(), out.writable());
        Tin *pin = in.rd(), *pend = pin + count;
        Tout *pout = out.wr();

        for (; pin < pend; ++pin, ++pout) {
            *pout = *pin * scale;
        }

        in.read(count);
        out.written(count);
    }

  private:
    pipereader<Tin> in;
    pipewriter<Tout> out;
};

// [awgb_c] generates std::complex white gaussian noise.

template <typename T>
struct wgn_c : runnable
{
    wgn_c(
        scheduler *sch,
        pipebuf<std::complex<T>>
        &_out
    ) :
        runnable(sch, "awgn"),
        stddev(1.0),
        out(_out)
    {
    }

    void run()
    {
        int n = out.writable();
        std::complex<T> *pout = out.wr(), *pend = pout + n;

        while (pout < pend)
        {
            // TAOCP
            float x, y, r2;

            do
            {
                x = 2 * rand_compat() - 1;
                y = 2 * rand_compat() - 1;
                r2 = x * x + y * y;
            } while (r2 == 0 || r2 >= 1);

            float k = sqrtf(-logf(r2) / r2) * stddev;
            pout->re = k * x;
            pout->im = k * y;
            ++pout;
        }

        out.written(n);
    }
    float stddev;

  private:
    pipewriter<std::complex<T>> out;
};

template <typename T>
struct naive_lowpass : runnable
{
    naive_lowpass(
        scheduler *sch,
        pipebuf<T> &_in,
        pipebuf<T> &_out,
        int _w
    ) :
        runnable(sch, "lowpass"),
        in(_in),
        out(_out),
        w(_w)
    {
    }

    void run()
    {
        if (in.readable() < w) {
            return;
        }

        unsigned long count = min(in.readable() - w, out.writable());
        T *pin = in.rd(), *pend = pin + count;
        T *pout = out.wr();
        float k = 1.0 / w;

        for (; pin < pend; ++pin, ++pout)
        {
            T x = 0.0;

            for (int i = 0; i < w; ++i) {
                x = x + pin[i];
            }

            *pout = x * k;
        }

        in.read(count);
        out.written(count);
    }

  private:
    pipereader<T> in;
    pipewriter<T> out;
    int w;
};

template <typename T, typename Tc>
struct fir_filter : runnable
{
    fir_filter(
        scheduler *sch,
        int _ncoeffs,
        Tc *_coeffs,
        pipebuf<T> &_in,
        pipebuf<T> &_out,
        unsigned int _decim = 1
    ) :
        runnable(sch, "fir_filter"),
        ncoeffs(_ncoeffs),
        coeffs(_coeffs),
        in(_in),
        out(_out),
        decim(_decim),
        freq_tap(nullptr),
        tap_multiplier(1),
        freq_tol(0.1)
    {
        shifted_coeffs = new T[ncoeffs];
        set_freq(0);
    }

    ~fir_filter() {
        delete[] shifted_coeffs;
    }

    void run()
    {
        if (in.readable() < ncoeffs)
            return;

        if (freq_tap)
        {
            float new_freq = *freq_tap * tap_multiplier;
            if (fabs(current_freq - new_freq) > freq_tol)
            {
                if (sch->verbose)
                    fprintf(stderr, "Shifting filter %f -> %f\n",
                            current_freq, new_freq);
                set_freq(new_freq);
            }
        }

        long count = min((in.readable() - ncoeffs) / decim,
                                  out.writable());
        T *pin = in.rd() + ncoeffs, *pend = pin + count * decim, *pout = out.wr();
        // TBD use coeffs when current_freq=0 (fewer mults if float)
        for (; pin < pend; pin += decim, ++pout)
        {
            T *pc = shifted_coeffs;
            T *pi = pin;
            T x = 0;
            for (int i = ncoeffs; i--; ++pc, --pi)
                x = x + (*pc) * (*pi);
            *pout = x;
        }
        in.read(count * decim);
        out.written(count);
    }

  public:
    float *freq_tap;
    float tap_multiplier;
    float freq_tol;

  private:
    int ncoeffs;
    Tc *coeffs;
    pipereader<T> in;
    pipewriter<T> out;
    int decim;
    T *shifted_coeffs;
    float current_freq;

    void set_freq(float f)
    {
        for (int i = 0; i < ncoeffs; ++i)
        {
            float a = 2 * M_PI * f * (i - ncoeffs / 2.0);
            float c = cosf(a), s = sinf(a);
            // TBD Support T=std::complex
            shifted_coeffs[i].real(coeffs[i] * c);
            shifted_coeffs[i].imag(coeffs[i] * s);
        }
        current_freq = f;
    }
}; // fir_filter

// FIR FILTER WITH INTERPOLATION AND DECIMATION

template <typename T, typename Tc>
struct fir_resampler : runnable
{
    fir_resampler(
        scheduler *sch,
        int _ncoeffs,
        Tc *_coeffs,
        pipebuf<T> &_in,
        pipebuf<T> &_out,
        int _interp = 1,
        int _decim = 1
    ) :
        runnable(sch, "fir_resampler"),
        ncoeffs(_ncoeffs),
        coeffs(_coeffs),
        interp(_interp),
        decim(_decim),
        in(_in),
        out(_out, interp),
        freq_tap(nullptr),
        tap_multiplier(1),
        freq_tol(0.1)
    {
        if (decim != 1)
            fail("fir_resampler: decim not implemented"); // TBD
        shifted_coeffs = new T[ncoeffs];
        set_freq(0);
    }

    ~fir_resampler() {
        delete[] shifted_coeffs;
    }

    void run()
    {
        if (in.readable() < ncoeffs)
            return;

        if (freq_tap)
        {
            float new_freq = *freq_tap * tap_multiplier;
            if (fabs(current_freq - new_freq) > freq_tol)
            {
                if (sch->verbose)
                    fprintf(stderr, "Shifting filter %f -> %f\n",
                            current_freq, new_freq);
                set_freq(new_freq);
            }
        }

        if (in.readable() * interp < ncoeffs)
            return;
        unsigned long count = min((in.readable() * interp - ncoeffs) / interp,
                                  out.writable() / interp);
        int latency = (ncoeffs + interp) / interp;
        T *pin = in.rd() + latency, *pend = pin + count, *pout = out.wr();
        // TBD use coeffs when current_freq=0 (fewer mults if float)
        for (; pin < pend; ++pin)
        {
            for (int i = 0; i < interp; ++i, ++pout)
            {
                T *pi = pin;
                T *pc = shifted_coeffs + i, *pcend = shifted_coeffs + ncoeffs;
                T x = 0;
                for (; pc < pcend; pc += interp, --pi)
                    x = x + (*pc) * (*pi);
                *pout = x;
            }
        }
        in.read(count);
        out.written(count * interp);
    }

  public:
    float *freq_tap;
    float tap_multiplier;
    float freq_tol;

  private:
    unsigned int ncoeffs;
    Tc *coeffs;
    int interp, decim;
    pipereader<T> in;
    pipewriter<T> out;
    T *shifted_coeffs;
    float current_freq;

    void set_freq(float f)
    {
        for (int i = 0; i < ncoeffs; ++i)
        {
            float a = 2 * M_PI * f * i;
            float c = cosf(a), s = sinf(a);
            // TBD Support T=std::complex
            shifted_coeffs[i].re = coeffs[i] * c;
            shifted_coeffs[i].im = coeffs[i] * s;
        }
        current_freq = f;
    }
}; // fir_resampler

} // namespace leansdr

#endif // LEANSDR_DSP_H
