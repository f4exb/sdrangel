#ifndef LEANSDR_DSP_H
#define LEANSDR_DSP_H

#include <math.h>
#include "leansdr/framework.h"
#include "leansdr/math.h"

namespace leansdr
{

//////////////////////////////////////////////////////////////////////
// DSP blocks
//////////////////////////////////////////////////////////////////////

// [cconverter] converts complex streams between numric types,
// with optional ofsetting and rational scaling.
template<typename Tin, int Zin, typename Tout, int Zout, int Gn, int Gd>
struct cconverter: runnable
{
    cconverter(scheduler *sch, pipebuf<complex<Tin> > &_in,
            pipebuf<complex<Tout> > &_out) :
            runnable(sch, "cconverter"), in(_in), out(_out)
    {
    }

    void run()
    {
        unsigned long count = min(in.readable(), out.writable());
        complex<Tin> *pin = in.rd(), *pend = pin + count;
        complex<Tout> *pout = out.wr();
        for (; pin < pend; ++pin, ++pout)
        {
            pout->re = Zout + (pin->re - (Tin) Zin) * Gn / Gd;
            pout->im = Zout + (pin->im - (Tin) Zin) * Gn / Gd;
        }
        in.read(count);
        out.written(count);
    }

private:
    pipereader<complex<Tin> > in;
    pipewriter<complex<Tout> > out;
};

template<typename T>
struct cfft_engine
{
    const unsigned int n;

    cfft_engine(unsigned int _n) :
            n(_n), invsqrtn(1.0 / sqrt(n))
    {
        // Compute log2(n)
        logn = 0;
        for (int t = n; t > 1; t >>= 1)
            ++logn;
        // Bit reversal
        bitrev = new int[n];
        for (unsigned int i = 0; i < n; ++i)
        {
            bitrev[i] = 0;
            for (int b = 0; b < logn; ++b)
                bitrev[i] = (bitrev[i] << 1) | ((i >> b) & 1);
        }
        // Float constants
        omega = new complex<T> [n];
        omega_rev = new complex<T> [n];
        for (unsigned int i = 0; i < n; ++i)
        {
            float a = 2.0 * M_PI * i / n;
            omega_rev[i].re = (omega[i].re = cosf(a));
            omega_rev[i].im = -(omega[i].im = sinf(a));
        }
    }

    void inplace(complex<T> *data, bool reverse = false)
    {
        // Bit-reversal permutation
        for (unsigned int i = 0; i < n; ++i)
        {
            unsigned int r = bitrev[i];
            if (r < i)
            {
                complex<T> tmp = data[i];
                data[i] = data[r];
                data[r] = tmp;
            }
        }
        complex<T> *om = reverse ? omega_rev : omega;
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
                    complex<T> &w = om[k * dom];
                    complex<T> &dqk = data[q + k];
                    complex<T> x(w.re * dqk.re - w.im * dqk.im,
                            w.re * dqk.im + w.im * dqk.re);
                    data[q + k].re = data[p + k].re - x.re;
                    data[q + k].im = data[p + k].im - x.im;
                    data[p + k].re = data[p + k].re + x.re;
                    data[p + k].im = data[p + k].im + x.im;
                }
            }
        }
        if (reverse)
        {
            float invn = 1.0 / n;
            for (unsigned int i = 0; i < n; ++i)
            {
                data[i].re *= invn;
                data[i].im *= invn;
            }
        }
    }

private:
    int logn;
    int *bitrev;
    complex<T> *omega, *omega_rev;
    float invsqrtn;
};

template<typename T>
struct adder: runnable
{
    adder(scheduler *sch, pipebuf<T> &_in1, pipebuf<T> &_in2, pipebuf<T> &_out) :
            runnable(sch, "adder"),
            in1(_in1),
            in2(_in2),
            out(_out)
    {
    }

    void run()
    {
        int n = out.writable();
        if (in1.readable() < n)
            n = in1.readable();
        if (in2.readable() < n)
            n = in2.readable();
        T *pin1 = in1.rd(), *pin2 = in2.rd(), *pout = out.wr(), *pend = pout
                + n;
        while (pout < pend)
            *pout++ = *pin1++ + *pin2++;
        in1.read(n);
        in2.read(n);
        out.written(n);
    }

private:
    pipereader<T> in1, in2;
    pipewriter<T> out;
};

template<typename Tscale, typename Tin, typename Tout>
struct scaler: runnable
{
    Tscale scale;

    scaler(scheduler *sch, Tscale _scale, pipebuf<Tin> &_in, pipebuf<Tout> &_out) :
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
        for (; pin < pend; ++pin, ++pout)
            *pout = *pin * scale;
        in.read(count);
        out.written(count);
    }

private:
    pipereader<Tin> in;
    pipewriter<Tout> out;
};

// [awgb_c] generates complex white gaussian noise.

template<typename T>
struct wgn_c: runnable
{
    wgn_c(scheduler *sch, pipebuf<complex<T> > &_out) :
            runnable(sch, "awgn"),
            stddev(1.0),
            out(_out)
    {
    }

    void run()
    {
        int n = out.writable();
        complex<T> *pout = out.wr(), *pend = pout + n;
        while (pout < pend)
        {
            // TAOCP
            float x, y, r2;
            do
            {
                x = 2 * drand48() - 1;
                y = 2 * drand48() - 1;
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
    pipewriter<complex<T> > out;
};

template<typename T>
struct naive_lowpass: runnable
{
    naive_lowpass(scheduler *sch, pipebuf<T> &_in, pipebuf<T> &_out, int _w) :
            runnable(sch, "lowpass"),
            in(_in),
            out(_out),
            w(_w)
    {
    }

    void run()
    {
        if (in.readable() < w)
            return;
        unsigned long count = min(in.readable() - w, out.writable());
        T *pin = in.rd(), *pend = pin + count;
        T *pout = out.wr();
        float k = 1.0 / w;
        for (; pin < pend; ++pin, ++pout)
        {
            T x = 0.0;
            for (int i = 0; i < w; ++i)
                x = x + pin[i];
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

template<typename T, typename Tc>
struct fir_filter: runnable
{
    fir_filter(scheduler *sch, int _ncoeffs, Tc *_coeffs, pipebuf<T> &_in, pipebuf<T> &_out, unsigned int _decim = 1) :
            runnable(sch, "fir_filter"),
            freq_tap(NULL),
            tap_multiplier(1),
            freq_tol(0.1),
            ncoeffs(_ncoeffs),
            coeffs(_coeffs),
            in(_in),
            out(_out),
            decim(_decim)
    {
        shifted_coeffs = new T[ncoeffs];
        set_freq(0);
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
                    fprintf(stderr, "Shifting filter %f -> %f\n", current_freq,
                            new_freq);
                set_freq(new_freq);
            }
        }

        unsigned long count = min((in.readable() - ncoeffs) / decim,
                out.writable());
        T *pin = in.rd() + ncoeffs, *pend = pin + count * decim, *pout =
                out.wr();
        // TBD use coeffs when current_freq=0 (fewer mults if float)
        for (; pin < pend; pin += decim, ++pout)
        {
            T *pc = shifted_coeffs;
            T *pi = pin;
            T x = 0;
            for (unsigned int i = ncoeffs; i--; ++pc, --pi)
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
    unsigned int ncoeffs;
    Tc *coeffs;
    pipereader<T> in;
    pipewriter<T> out;
    unsigned int decim;
    T *shifted_coeffs;
    float current_freq;

    void set_freq(float f)
    {
        for (unsigned int i = 0; i < ncoeffs; ++i)
        {
            float a = 2 * M_PI * f * (i - (ncoeffs / 2.0f));
            float c = cosf(a), s = sinf(a);
            // TBD Support T=complex
            shifted_coeffs[i].re = coeffs[i] * c;
            shifted_coeffs[i].im = coeffs[i] * s;
        }
        current_freq = f;
    }
};
// fir_filter

// FIR FILTER WITH INTERPOLATION AND DECIMATION

template<typename T, typename Tc>
struct fir_resampler: runnable
{
    fir_resampler(scheduler *sch, int _ncoeffs, Tc *_coeffs, pipebuf<T> &_in, pipebuf<T> &_out, int _interp = 1, int _decim = 1) :
            runnable(sch, "fir_resampler"),
            ncoeffs(_ncoeffs),
            coeffs(_coeffs),
            interp(_interp),
            decim(_decim),
            in(_in),
            out(_out, interp),
            freq_tap(NULL),
            tap_multiplier(1),
            freq_tol(0.1)
    {
        if (decim != 1) {
            fail("fir_resampler::fir_resampler", "decim not implemented");  // TBD
            return;
        }
        shifted_coeffs = new T[ncoeffs];
        set_freq(0);
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
                    fprintf(stderr, "Shifting filter %f -> %f\n", current_freq,
                            new_freq);
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
            // TBD Support T=complex
            shifted_coeffs[i].re = coeffs[i] * c;
            shifted_coeffs[i].im = coeffs[i] * s;
        }
        current_freq = f;
    }
};
// fir_resampler

}// namespace

#endif  // LEANSDR_DSP_H
