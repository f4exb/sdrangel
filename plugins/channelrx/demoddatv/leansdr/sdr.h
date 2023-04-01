// This file is part of LeanSDR Copyright (C) 2016-2019 <pabr@pabr.org>.
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

#ifndef LEANSDR_SDR_H
#define LEANSDR_SDR_H

#include <numeric>
#include <complex>

#include "leansdr/dsp.h"
#include "leansdr/math.h"

namespace leansdr
{

// Abbreviations for floating-point types

typedef float f32;

typedef std::complex<u8> cu8;
typedef std::complex<s8> cs8;
typedef std::complex<u16> cu16;
typedef std::complex<s16> cs16;
typedef std::complex<f32> cf32;

//////////////////////////////////////////////////////////////////////
// SDR blocks
//////////////////////////////////////////////////////////////////////

// AUTO-NOTCH FILTER

// Periodically detects the [nslots] strongest peaks with a FFT,
// removes them with a first-order filter.

template <typename T>
struct auto_notch : runnable
{
    int decimation;
    float k;

    auto_notch(
        scheduler *sch,
        pipebuf<std::complex<T>> &_in,
        pipebuf<std::complex<T>> &_out,
        int _nslots,
        T _agc_rms_setpoint
    ) :
        runnable(sch, "auto_notch"),
        decimation(1024 * 4096),
        k(0.002), // k(0.01)
        fft(4096),
        in(_in),
        out(_out, fft.size()),
        nslots(_nslots),
        phase(0),
        gain(1),
        agc_rms_setpoint(_agc_rms_setpoint)
    {
        __slots = new slot[nslots];

        for (int s = 0; s < nslots; ++s)
        {
            __slots[s].i = -1;
            __slots[s].expj = new std::complex<float>[fft.size()];
        }
    }

    ~auto_notch()
    {
        for (int s = 0; s < nslots; ++s) {
            delete[] __slots[s].expj;
        }

        delete[] __slots;
    }

    void run()
    {
        while (in.readable() >= fft.size() && out.writable() >= fft.size())
        {
            phase += fft.size();

            if (phase >= decimation)
            {
                phase -= decimation;
                detect();
            }

            process();
            in.read(fft.size());
            out.written(fft.size());
        }
    }

    void detect()
    {
        std::complex<T> *pin = in.rd();
        std::complex<float> *data = new std::complex<float>[fft.size()];
        float m0 = 0, m2 = 0;

        for (int i = 0; i < fft.size(); ++i)
        {
            data[i] = pin[i];
            m2 += (float) pin[i].real() * pin[i].real() + (float) pin[i].imag() * pin[i].imag();

            if (gen_abs(pin[i].real()) > m0) {
                m0 = gen_abs(pin[i].real());
            }

            if (gen_abs(pin[i].imag()) > m0) {
                m0 = gen_abs(pin[i].imag());
            }
        }

        if (agc_rms_setpoint && m2)
        {
            float rms = gen_sqrt(m2 / fft.size());

            if (sch->debug) {
                fprintf(stderr, "(pow %f max %f)", rms, m0);
            }

            float new_gain = agc_rms_setpoint / rms;
            gain = gain * 0.9 + new_gain * 0.1;
        }

        fft.inplace(data, true);
        float *amp = new float[fft.size()];

        for (int i = 0; i < fft.size(); ++i) {
            amp[i] = hypotf(data[i].real(), data[i].imag());
        }

        for (slot *s = __slots; s < __slots + nslots; ++s)
        {
            int iamax = 0;

            for (int i = 0; i < fft.size(); ++i)
            {
                if (amp[i] > amp[iamax]) {
                    iamax = i;
                }
            }

            if (iamax != s->i)
            {
                if (sch->debug) {
                    fprintf(stderr, "%s: slot %d new peak %d -> %d\n", name, (int)(s - __slots), s->i, iamax);
                }

                s->i = iamax;
                s->estim.real(0);
                s->estim.imag(0);
                s->estt = 0;

                for (int i = 0; i < fft.size(); ++i)
                {
                    float a = 2 * M_PI * s->i * i / fft.size();
                    s->expj[i].real(cosf(a));
                    s->expj[i].imag(sinf(a));
                }
            }

            amp[iamax] = 0;

            if (iamax - 1 >= 0) {
                amp[iamax - 1] = 0;
            }

            if (iamax + 1 < fft.size()) {
                amp[iamax + 1] = 0;
            }
        }

        delete[] amp;
        delete[] data;
    }

    void process()
    {
        std::complex<T> *pin = in.rd(), *pend = pin + fft.size(), *pout = out.wr();

        for (slot *s = __slots; s < __slots + nslots; ++s) {
            s->ej = s->expj;
        }

        for (; pin < pend; ++pin, ++pout)
        {
            std::complex<float> out = *pin;
            // TODO Optimize for nslots==1 ?

            for (slot *s = __slots; s < __slots + nslots; ++s->ej, ++s)
            {
                std::complex<float> bb(
                    pin->real() * s->ej->real() + pin->imag() * s->ej->imag(),
                    -pin->real() * s->ej->imag() + pin->imag() * s->ej->real()
                );
                s->estim.real(bb.real() * k + s->estim.real() * (1 - k));
                s->estim.imag(bb.imag() * k + s->estim.imag() * (1 - k));
                std::complex<float> sub(
                    s->estim.real() * s->ej->real() - s->estim.imag() * s->ej->imag(),
                    s->estim.real() * s->ej->imag() + s->estim.imag() * s->ej->real()
                );
                out.real(out.real() - sub.real());
                out.imag(out.imag() - sub.imag());
            }

            pout->real(gain * out.real());
            pout->imag(gain * out.imag());
        }
    }

  private:
    cfft_engine<float> fft;
    pipereader<std::complex<T>> in;
    pipewriter<std::complex<T>> out;
    int nslots;

    struct slot
    {
        int i;
        std::complex<float> estim;
        std::complex<float> *expj, *ej;
        int estt;
    };

    slot *__slots;
    int phase;
    float gain;
    T agc_rms_setpoint;
};

// SIGNAL STRENGTH ESTIMATOR

// Outputs RMS values.

template <typename T>
struct ss_estimator : runnable
{
    unsigned long window_size; // Samples per estimation
    unsigned long decimation;  // Output rate

    ss_estimator(
        scheduler *sch,
        pipebuf<std::complex<T>> &_in,
        pipebuf<T> &_out
    ) :
        runnable(sch, "SS estimator"),
        window_size(1024),
        decimation(1024),
        in(_in),
        out(_out),
        phase(0)
    {
    }

    void run()
    {
        while (in.readable() >= window_size && out.writable() >= 1)
        {
            phase += window_size;

            if (phase >= decimation)
            {
                phase -= decimation;
                std::complex<T> *p = in.rd(), *pend = p + window_size;
                float s = 0;

                for (; p < pend; ++p) {
                    s += (float)p->real() * p->real() + (float)p->imag() * p->imag();
                }

                out.write(sqrtf(s / window_size));
            }

            in.read(window_size);
        }
    }

  private:
    pipereader<std::complex<T>> in;
    pipewriter<T> out;
    unsigned long phase;
};

template <typename T>
struct ss_amp_estimator : runnable
{
    unsigned long window_size; // Samples per estimation
    unsigned long decimation;  // Output rate

    ss_amp_estimator(
        scheduler *sch,
        pipebuf<std::complex<T>> &_in,
        pipebuf<T> &_out_ss,
        pipebuf<T> &_out_ampmin,
        pipebuf<T> &_out_ampmax
    ) :
        runnable(sch, "SS estimator"),
        window_size(1024),
        decimation(1024),
        in(_in),
        out_ss(_out_ss),
        out_ampmin(_out_ampmin),
        out_ampmax(_out_ampmax),
        phase(0)
    {
    }

    void run()
    {
        while (in.readable() >= window_size && out_ss.writable() >= 1 && out_ampmin.writable() >= 1 && out_ampmax.writable() >= 1)
        {
            phase += window_size;

            if (phase >= decimation)
            {
                phase -= decimation;
                std::complex<T> *p = in.rd(), *pend = p + window_size;
                float s2 = 0;
                float amin = 1e38, amax = 0;

                for (; p < pend; ++p)
                {
                    float mag2 = (float)p->real() * p->real() + (float)p->imag() * p->imag();
                    s2 += mag2;
                    float mag = sqrtf(mag2);

                    if (mag < amin) {
                        amin = mag;
                    }

                    if (mag > amax) {
                        amax = mag;
                    }
                }

                out_ss.write(sqrtf(s2 / window_size));
                out_ampmin.write(amin);
                out_ampmax.write(amax);
            }

            in.read(window_size);
        }
    }

  private:
    pipereader<std::complex<T>> in;
    pipewriter<T> out_ss, out_ampmin, out_ampmax;
    unsigned long phase;
};

// AGC

template <typename T>
struct simple_agc : runnable
{
    float out_rms;   // Desired RMS output power
    float bw;        // Bandwidth
    float estimated; // Input power
    static const int chunk_size = 128;

    simple_agc(
        scheduler *sch,
        pipebuf<std::complex<T>> &_in,
        pipebuf<std::complex<T>> &_out
    ) :
        runnable(sch, "AGC"),
        out_rms(1),
        bw(0.001),
        estimated(0),
        in(_in),
        out(_out, chunk_size)
    {
    }

  private:
    pipereader<std::complex<T>> in;
    pipewriter<std::complex<T>> out;

    void run()
    {
        while (in.readable() >= chunk_size && out.writable() >= chunk_size)
        {
            std::complex<T> *pin = in.rd(), *pend = pin + chunk_size;
            float amp2 = 0;

            for (; pin < pend; ++pin) {
                amp2 += pin->real() * pin->real() + pin->imag() * pin->imag();
            }

            amp2 /= chunk_size;

            if (!estimated) {
                estimated = amp2;
            }

            estimated = estimated * (1 - bw) + amp2 * bw;
            float gain = estimated ? out_rms / sqrtf(estimated) : 0;
            pin = in.rd();
            std::complex<T> *pout = out.wr();
            float bwcomp = 1 - bw;

            for (; pin < pend; ++pin, ++pout)
            {
                pout->real() = pin->real() * gain;
                pout->imag() = pin->imag() * gain;
            }

            in.read(chunk_size);
            out.written(chunk_size);
        }
    }
};
// simple_agc

typedef uint16_t u_angle; //  [0,2PI[ in 65536 steps
typedef int16_t s_angle;  // [-PI,PI[ in 65536 steps

// GENERIC CONSTELLATION DECODING BY LOOK-UP TABLE.

// Metrics and phase errors are pre-computed on a RxR grid.
// R must be a power of 2.
// Up to 256 symbols.

struct softsymbol
{                 // TBD obsolete
    int16_t cost; // For Viterbi with TBM=int16_t
    uint8_t symbol;
};

// Target RMS amplitude for AGC
//const float cstln_amp = 73;  // Best for 32APSK 9/10
//const float cstln_amp = 90;  // Best for QPSK
//const float cstln_amp = 64;  // Best for BPSK
//const float cstln_amp = 75;  // Best for BPSK at 45°
const float cstln_amp = 75; // Trade-off

// A struct that temporarily holds all the info we precompute for the LUT.
struct full_ss
{
    uint8_t nearest;      // Index of nearest in constellation
    uint16_t dists2[256]; // Squared distances
    float p[8];           // 0..1 probability of bits being 1
};

// Options for soft-symbols.
// These functions are overloaded to keep cstln_lut<SOFTSYMB> generic:
//   to_softsymb(const full_ss *fss, SOFTSYMB *ss)
//   softsymb_harden(SOFTSYMB *ss) {
//   softsymb_to_dump(const SOFTSYMB &ss, int bit)     To grey 0..255
// For LUT initialization only.  Performance is not critical.

// Hard decision soft-symbols.
// Value is the symbol index, 0..255.
typedef uint8_t hard_ss;
void to_softsymb(const full_ss *fss, hard_ss *ss);
void softsymb_harden(hard_ss *ss);
uint8_t softsymb_to_dump(const hard_ss &ss, int bit);

// Euclidian QPSK soft-symbols.
// Additive metric suitable for Viterbi.
// Backward-compatible with simplified Viterbi (TBD remove)
struct eucl_ss
{
    static const int MAX_SYMBOLS = 4;
    uint16_t dists2[MAX_SYMBOLS];
    uint16_t discr2; // 2nd_nearest - nearest
    uint8_t nearest;
};

void to_softsymb(const full_ss *fss, eucl_ss *ss);
void softsymb_harden(eucl_ss *ss);
uint8_t softsymb_to_dump(const eucl_ss &ss, int bit);

// Log-Likelihood Ratios soft-symbols
typedef int8_t llr_t; // log(p(0)/p(1)), clip -127=1 +127=0

inline bool llr_harden(llr_t v)
{
    return v & 128;
}

struct llr_ss
{
    llr_t bits[8]; // Up to 8 bit considered independent
};

void to_softsymb(const full_ss *fss, llr_ss *ss);
void softsymb_harden(llr_ss *ss);
uint8_t softsymb_to_dump(const llr_ss &ss, int bit);

struct cstln_base
{
    enum predef
    {
        BPSK, // DVB-S2 (and DVB-S variant)
        QPSK, // DVB-S
        PSK8,
        APSK16,
        APSK32,  // DVB-S2
        APSK64E, // DVB-S2X
        QAM16,
        QAM64,
        QAM256, // For experimentation only
        COUNT
    };

    static const char *names[];
    float amp_max; // Max amplitude. 1 for PSK, 0 if not applicable.
    std::complex<int8_t> *symbols;
    int nsymbols;
    int nrotations;
};
// cstln_base

template <typename SOFTSYMB, int R>
struct cstln_lut : cstln_base
{
    cstln_lut(
        cstln_base::predef type,
        float mer = 10,
        float gamma1 = 0,
        float gamma2 = 0,
        float gamma3 = 0
    )
    {
        symbols = nullptr;

        switch (type)
        {
        case BPSK:
            amp_max = 1;
            nrotations = 2;
            nsymbols = 2;
            symbols = new std::complex<signed char>[nsymbols];
#if 0 // BPSK at 0°
            symbols[0] = polar(1, 2, 0);
            symbols[1] = polar(1, 2, 1);
            printf("cstln_lut::cstln_lut: BPSK at 0 degrees\n");
#else // BPSK at 45°
            symbols[0] = polar(1, 8, 1);
            symbols[1] = polar(1, 8, 5);
            printf("cstln_lut::cstln_lut: BPSK at 45 degrees\n");
#endif
            make_lut_from_symbols(mer);
            break;
        case QPSK:
            amp_max = 1;
            // EN 300 421, section 4.5 Baseband shaping and modulation
            // EN 302 307, section 5.4.1
            nrotations = 4;
            nsymbols = 4;
            symbols = new std::complex<signed char>[nsymbols];
            symbols[0] = polar(1, 4, 0.5);
            symbols[1] = polar(1, 4, 3.5);
            symbols[2] = polar(1, 4, 1.5);
            symbols[3] = polar(1, 4, 2.5);
            make_lut_from_symbols(mer);
            printf("cstln_lut::cstln_lut: QPSK\n");
            break;
        case PSK8:
            amp_max = 1;
            // EN 302 307, section 5.4.2
            nrotations = 8;
            nsymbols = 8;
            symbols = new std::complex<signed char>[nsymbols];
            symbols[0] = polar(1, 8, 1);
            symbols[1] = polar(1, 8, 0);
            symbols[2] = polar(1, 8, 4);
            symbols[3] = polar(1, 8, 5);
            symbols[4] = polar(1, 8, 2);
            symbols[5] = polar(1, 8, 7);
            symbols[6] = polar(1, 8, 3);
            symbols[7] = polar(1, 8, 6);
            make_lut_from_symbols(mer);
            printf("cstln_lut::cstln_lut: PSK8\n");
            break;
        case APSK16:
        {
            // Default gamma for non-DVB-S2 applications.
            if (gamma1 == 0)
                gamma1 = 2.57;
            // EN 302 307, section 5.4.3
            float r1 = sqrtf(4.0f / (1.0f + 3.0f * gamma1 * gamma1));
            float r2 = gamma1 * r1;
            amp_max = r2;
            nrotations = 4;
            nsymbols = 16;
            symbols = new std::complex<signed char>[nsymbols];
            symbols[0] = polar(r2, 12, 1.5);
            symbols[1] = polar(r2, 12, 10.5);
            symbols[2] = polar(r2, 12, 4.5);
            symbols[3] = polar(r2, 12, 7.5);
            symbols[4] = polar(r2, 12, 0.5);
            symbols[5] = polar(r2, 12, 11.5);
            symbols[6] = polar(r2, 12, 5.5);
            symbols[7] = polar(r2, 12, 6.5);
            symbols[8] = polar(r2, 12, 2.5);
            symbols[9] = polar(r2, 12, 9.5);
            symbols[10] = polar(r2, 12, 3.5);
            symbols[11] = polar(r2, 12, 8.5);
            symbols[12] = polar(r1, 4, 0.5);
            symbols[13] = polar(r1, 4, 3.5);
            symbols[14] = polar(r1, 4, 1.5);
            symbols[15] = polar(r1, 4, 2.5);
            make_lut_from_symbols(mer);
            printf("cstln_lut::cstln_lut: APSK16: gamma1=%f r1=%f r2=%f\n", gamma1, r1, r2);
            break;
        }
        case APSK32:
        {
            // Default gammas for non-DVB-S2 applications.
            if (gamma1 == 0)
                gamma1 = 2.53;
            if (gamma2 == 0)
                gamma2 = 4.30;
            // EN 302 307, section 5.4.3
            float r1 = sqrtf(
                8.0f / (1.0f + 3.0f * gamma1 * gamma1 + 4 * gamma2 * gamma2));
            float r2 = gamma1 * r1;
            float r3 = gamma2 * r1;
            amp_max = r3;
            nrotations = 4;
            nsymbols = 32;
            symbols = new std::complex<signed char>[nsymbols];
            symbols[0] = polar(r2, 12, 1.5);
            symbols[1] = polar(r2, 12, 2.5);
            symbols[2] = polar(r2, 12, 10.5);
            symbols[3] = polar(r2, 12, 9.5);
            symbols[4] = polar(r2, 12, 4.5);
            symbols[5] = polar(r2, 12, 3.5);
            symbols[6] = polar(r2, 12, 7.5);
            symbols[7] = polar(r2, 12, 8.5);
            symbols[8] = polar(r3, 16, 1);
            symbols[9] = polar(r3, 16, 3);
            symbols[10] = polar(r3, 16, 14);
            symbols[11] = polar(r3, 16, 12);
            symbols[12] = polar(r3, 16, 6);
            symbols[13] = polar(r3, 16, 4);
            symbols[14] = polar(r3, 16, 9);
            symbols[15] = polar(r3, 16, 11);
            symbols[16] = polar(r2, 12, 0.5);
            symbols[17] = polar(r1, 4, 0.5);
            symbols[18] = polar(r2, 12, 11.5);
            symbols[19] = polar(r1, 4, 3.5);
            symbols[20] = polar(r2, 12, 5.5);
            symbols[21] = polar(r1, 4, 1.5);
            symbols[22] = polar(r2, 12, 6.5);
            symbols[23] = polar(r1, 4, 2.5);
            symbols[24] = polar(r3, 16, 0);
            symbols[25] = polar(r3, 16, 2);
            symbols[26] = polar(r3, 16, 15);
            symbols[27] = polar(r3, 16, 13);
            symbols[28] = polar(r3, 16, 7);
            symbols[29] = polar(r3, 16, 5);
            symbols[30] = polar(r3, 16, 8);
            symbols[31] = polar(r3, 16, 10);
            make_lut_from_symbols(mer);
            printf("cstln_lut::cstln_lut: APSK32: gamma1=%f gamma2=%f, r1=%f r2=%f r3=%f\n", gamma1, gamma2, r1, r2, r3);
            break;
        }
        case APSK64E:
        {
            // Default gammas for non-DVB-S2 applications.
            if (gamma1 == 0)
                gamma1 = 2.4;
            if (gamma2 == 0)
                gamma2 = 4.3;
            if (gamma3 == 0)
                gamma3 = 7.0;
            // EN 302 307-2, section 5.4.5, Table 13e
            float r1 = sqrtf(
                64.0f / (4.0f + 12.0f * gamma1 * gamma1 + 20.0f * gamma2 * gamma2 + 28.0f * gamma3 * gamma3));
            float r2 = gamma1 * r1;
            float r3 = gamma2 * r1;
            float r4 = gamma3 * r1;
            amp_max = r4;
            nrotations = 4;
            nsymbols = 64;
            symbols = new std::complex<signed char>[nsymbols];
            polar2(0, r4, 1.0 / 4, 7.0 / 4, 3.0 / 4, 5.0 / 4);
            polar2(4, r4, 13.0 / 28, 43.0 / 28, 15.0 / 28, 41.0 / 28);
            polar2(8, r4, 1.0 / 28, 55.0 / 28, 27.0 / 28, 29.0 / 28);
            polar2(12, r1, 1.0 / 4, 7.0 / 4, 3.0 / 4, 5.0 / 4);
            polar2(16, r4, 9.0 / 28, 47.0 / 28, 19.0 / 28, 37.0 / 28);
            polar2(20, r4, 11.0 / 28, 45.0 / 28, 17.0 / 28, 39.0 / 28);
            polar2(24, r3, 1.0 / 20, 39.0 / 20, 19.0 / 20, 21.0 / 20);
            polar2(28, r2, 1.0 / 12, 23.0 / 12, 11.0 / 12, 13.0 / 12);
            polar2(32, r4, 5.0 / 28, 51.0 / 28, 23.0 / 28, 33.0 / 28);
            polar2(36, r3, 9.0 / 20, 31.0 / 20, 11.0 / 20, 29.0 / 20);
            polar2(40, r4, 3.0 / 28, 53.0 / 28, 25.0 / 28, 31.0 / 28);
            polar2(44, r2, 5.0 / 12, 19.0 / 12, 7.0 / 12, 17.0 / 12);
            polar2(48, r3, 1.0 / 4, 7.0 / 4, 3.0 / 4, 5.0 / 4);
            polar2(52, r3, 7.0 / 20, 33.0 / 20, 13.0 / 20, 27.0 / 20);
            polar2(56, r3, 3.0 / 20, 37.0 / 20, 17.0 / 20, 23.0 / 20);
            polar2(60, r2, 1.0 / 4, 7.0 / 4, 3.0 / 4, 5.0 / 4);
            make_lut_from_symbols(mer);
            printf("cstln_lut::cstln_lut: APSK64E: gamma1=%f gamma2=%f, gamm3=%f r1=%f r2=%f r3=%f r4=%f\n", gamma1, gamma2, gamma3, r1, r2, r3, r4);
            break;
        }
        case QAM16:
            amp_max = 0;
            make_qam(16, mer);
            break;
        case QAM64:
            amp_max = 1;
            make_qam(64, mer);
            break;
        case QAM256:
            amp_max = 1;
            make_qam(256, mer);
            break;
        default:
            fail("Constellation not implemented");
        }
    }

    ~cstln_lut()
    {
        if (symbols) {
            delete[] symbols;
        }
    }

    struct result
    {
        SOFTSYMB ss;
        s_angle phase_error;
        uint8_t symbol; // Nearest symbol, useful for C&T recovery
    };

    inline result *lookup(float I, float Q)
    {
        // Handling of overflows beyond the lookup table:
        // - For BPSK/QPSK/8PSK we only care about the phase,
        //   so the following is harmless and improves locking at low SNR.
        // - For amplitude modulations this is not appropriate.
        //   However, if there is enough noise to cause overflow,
        //   demodulation would probably fail anyway.
        //
        // Comment-out for better throughput at high SNR.
#if 1
        while (I < -128 || I > 127 || Q < -128 || Q > 127)
        {
            I *= 0.5;
            Q *= 0.5;
        }
#endif
        return &lut[(u8)(s8)I][(u8)(s8)Q];
    }

    inline result *lookup(int I, int Q)
    {
        // Ignore wrapping modulo 256
        return &lut[(u8)I][(u8)Q];
    }

  private:
    std::complex<signed char> polar(float r, int n, float i)
    {
        float a = i * 2 * M_PI / n;
        return std::complex<signed char>(
            r * cosf(a) * cstln_amp,
            r * sinf(a) * cstln_amp
        );
    }

    // Helper function for some constellation tables
    void polar2(int i, float r, float a0, float a1, float a2, float a3)
    {
        float a[] = {a0, a1, a2, a3};

        for (int j = 0; j < 4; ++j)
        {
            float phi = a[j] * M_PI;
            symbols[i + j] = std::complex<signed char>(
                r * cosf(phi) * cstln_amp,
                r * sinf(phi) * cstln_amp
            );
        }
    }

    void make_qam(int n, float mer)
    {
        nrotations = 4;
        nsymbols = n;
        symbols = new std::complex<signed char>[nsymbols];
        int m = sqrtl(n);
        float scale;

        { // Average power in first quadrant with unit grid
            int q = m / 2;
            float avgpower = 2 * (q * 0.25 + (q - 1) * q / 2.0 + (q - 1) * q * (2 * q - 1) / 6.0) / q;
            scale = 1.0 / sqrtf(avgpower);
        }
        // Arbitrary mapping

        int s = 0;

        for (int x = 0; x < m; ++x)
        {
            for (int y = 0; y < m; ++y)
            {
                float I = x - (float)(m - 1) / 2;
                float Q = y - (float)(m - 1) / 2;
                symbols[s].real(I * scale * cstln_amp);
                symbols[s].imag(Q * scale * cstln_amp);
                ++s;
            }
        }

        make_lut_from_symbols(mer);
    }

    result lut[R][R];

    void make_lut_from_symbols(float mer)
    {
        // Note: Excessively low values of MER will break 16APSK and 32APSK.
        float sigma = cstln_amp * pow(10.0, (-mer / 20));

        // Precomputed values.
        // Shared scope so that we don't have to reset dists2[nsymbols..] to -1.
        struct full_ss fss;

        for (int s = 0; s < 256; ++s) {
            fss.dists2[s] = -1;
        }

        for (int I = -R / 2; I < R / 2; ++I)
        {
            for (int Q = -R / 2; Q < R / 2; ++Q)
            {
                // Nearest symbol
                fss.nearest = 0;
                fss.dists2[0] = 65535;
                // Conditional probabilities:
                // Sum likelyhoods from all candidate symbols.
                //
                // P(TX[b]==B | RX==IQ) =
                //   sum(S=0..nsymbols-1, P(TX[b]==B | RX==IQ && TXs==S))
                //
                // P(TX[b] == B | RX==IQ && TX==S) =
                //   P(TX[b]==B && RX==IQ && TX==S) / P(RX==IQ && TX==S)
                float probs[8][2];
                memset(probs, 0, sizeof(probs));

                for (int s = 0; s < nsymbols; ++s)
                {
                    float d2 = ((I - symbols[s].real()) * (I - symbols[s].real()) + (Q - symbols[s].imag()) * (Q - symbols[s].imag()));

                    if (d2 < fss.dists2[fss.nearest]) {
                        fss.nearest = s;
                    }

                    fss.dists2[s] = d2;
                    float p = expf(-d2 / (2 * sigma * sigma)) / (sqrtf(2 * M_PI) * sigma);

                    for (int bit = 0; bit < 8; ++bit) {
                        probs[bit][(s >> bit) & 1] += p;
                    }
                }

                // Normalize
                for (int b = 0; b < 8; ++b)
                {
                    float p = probs[b][1] / (probs[b][0] + probs[b][1]);

                    // Avoid trouble when sigma is unrealistically low.
                    if (!isnormal(p)) {
                        p = 0;
                    }

                    fss.p[b] = p;
                }

                result *pr = &lut[I & (R - 1)][Q & (R - 1)];
                to_softsymb(&fss, &pr->ss);
                // Always record nearest symbol and phase error for C&T.
                pr->symbol = fss.nearest;
                float ph_symbol = atan2f(
                    symbols[pr->symbol].imag(),
                    symbols[pr->symbol].real()
                );
                float ph_err = atan2f(Q, I) - ph_symbol;
                pr->phase_error = (int32_t)(ph_err * 65536 / (2 * M_PI)); // Mod 65536
            }
        }
    }

  public:
    void dump(FILE *f)
    {
        int bps = log2(nsymbols);
        fprintf(f, "P5\n%d %d\n255\n", R, R * (bps + 1));

        for (int bit = 0; bit < bps + 1; ++bit)
        {
            for (int Q = R / 2 - 1; Q >= -R / 2; --Q)
            {
                for (int I = -R / 2; I < R / 2; ++I)
                {
                    result *pr = &lut[I & (R - 1)][Q & (R - 1)];
                    uint8_t v;

                    if (bit < bps) {
                        v = softsymb_to_dump(pr->ss, bit);
                    } else {
                        v = 128 + pr->phase_error / 64;
                    }

                    // Highlight the constellation symbols.
                    for (int s = 0; s < nsymbols; ++s)
                    {
                        if (symbols[s].real() == I && symbols[s].imag() == Q) {
                            v ^= 128;
                        }
                    }

                    fputc(v, f);
                }
            }
        }
    }

    // Convert soft metric to Hamming distance
    void harden()
    {
        for (int i = 0; i < R; ++i)
        {
            for (int q = 0; q < R; ++q) {
                softsymb_harden(&lut[i][q].ss);
            }
        }
    }

    int m_typeCode;
    int m_rateCode;
    bool m_setByModcod;
};
// cstln_lut

// SAMPLER INTERFACE FOR CSTLN_RECEIVER

template <typename T>
struct sampler_interface
{
    virtual ~sampler_interface() {
    }

    virtual std::complex<T> interp(const std::complex<T> *pin, float mu, float phase) = 0;

    virtual void update_freq(float freqw, int weight = 0)
    {
        (void) freqw;
        (void) weight;
    } // 65536 = 1 Hz

    virtual int readahead() = 0;
};

// NEAREST-SAMPLE SAMPLER FOR CSTLN_RECEIVER
// Suitable for bandpass-filtered, oversampled signals only

template <typename T>
struct nearest_sampler : sampler_interface<T>
{
    int readahead() {
        return 0;
    }

    std::complex<T> interp(const std::complex<T> *pin, float mu, float phase)
    {
        (void) mu;
        return pin[0] * trig.expi(-phase);
    }

  private:
    trig16 trig;
};
// nearest_sampler

// LINEAR SAMPLER FOR CSTLN_RECEIVER

template <typename T>
struct linear_sampler : sampler_interface<T>
{
    int readahead() {
        return 1;
    }

    std::complex<T> interp(const std::complex<T> *pin, float mu, float phase)
    {
        // Derotate pin[0] and pin[1]
        std::complex<T> s0 = pin[0] * trig.expi(-phase);
        std::complex<T> s1 = pin[1] * trig.expi(-(phase + freqw));
        // Interpolate linearly
        return s0 * (1 - mu) + s1 * mu;
    }

    void update_freq(float _freqw, int weight = 0)
    {
        (void) weight;
        freqw = _freqw;
    }

  private:
    trig16 trig;
    float freqw;
};
// linear_sampler

// FIR SAMPLER FOR CSTLN_RECEIVER

template <typename T, typename Tc>
struct fir_sampler : sampler_interface<T>
{
    fir_sampler(int _ncoeffs, Tc *_coeffs, int _subsampling = 1) :
        ncoeffs(_ncoeffs),
        coeffs(_coeffs),
        subsampling(_subsampling),
        update_freq_phase(0)
    {
        shifted_coeffs = new std::complex<T>[ncoeffs];
        do_update_freq(0); // In case application never calls update_freq()
    }

    virtual ~fir_sampler()
    {
        delete[] shifted_coeffs;
    }

    int readahead() {
        return ncoeffs - 1;
    }

    std::complex<T> interp(const std::complex<T> *pin, float mu, float phase)
    {
        // Apply FIR filter with subsampling
        std::complex<T> acc(0, 0);
        std::complex<T> *pc = shifted_coeffs + (int)((1 - mu) * subsampling);
        std::complex<T> *pcend = shifted_coeffs + ncoeffs;

        if (subsampling == 1)
        {
            // Special case for heavily oversampled signals,
            // where filtering is expensive.
            // gcc-4.9.2 can vectorize this form with NEON on ARM.
            while (pc < pcend) {
                acc += (*pc++) * (*pin++);
            }
        }
        else
        {
            // Not vectorized because the coefficients are not
            // guaranteed to be contiguous in memory.
            for (; pc < pcend; pc += subsampling, ++pin) {
                acc += (*pc) * (*pin);
            }
        }

        // Derotate
        return trig.expi(-phase) * acc;
    }

    void update_freq(float freqw, int weight = 0)
    {
        if (!weight) {
            update_freq_phase = 0;  // Force refresh.
        }
        // Throttling: Update one coeff per 16 processed samples,
        // to keep the overhead of freq tracking below about 10%.
        update_freq_phase -= weight;

        if (update_freq_phase <= 0)
        {
            update_freq_phase = ncoeffs * 16;
            do_update_freq(freqw);
        }
    }

  private:
    void do_update_freq(float freqw)
    {
        float f = freqw / subsampling;

        for (int i = 0; i < ncoeffs; ++i) {
            shifted_coeffs[i] = trig.expi(-f * (i - ncoeffs / 2)) * coeffs[i];
        }
    }

    trig16 trig;
    int ncoeffs;
    Tc *coeffs;
    int subsampling;
    cf32 *shifted_coeffs;
    int update_freq_phase;
};
// fir_sampler

// CONSTELLATION RECEIVER

// Linear interpolation: good enough for 1.2 samples/symbol,
// but higher oversampling is recommended.

template <typename T, typename SOFTSYMB>
struct cstln_receiver : runnable
{
    sampler_interface<T> *sampler;
    cstln_lut<SOFTSYMB, 256> *cstln;
    unsigned long meas_decimation;     // Measurement rate
    float omega, min_omega, max_omega; // Samples per symbol
    float freqw, min_freqw, max_freqw; // Freq offs (65536 = 1 Hz)
    float pll_adjustment;
    bool allow_drift; // Follow carrier beyond safe limits
    static const unsigned int chunk_size = 128;
    float kest;

    cstln_receiver(
        scheduler *sch,
        sampler_interface<T> *_sampler,
        pipebuf<std::complex<T>> &_in,
        pipebuf<SOFTSYMB> &_out,
        pipebuf<float> *_freq_out = nullptr,
        pipebuf<float> *_ss_out = nullptr,
        pipebuf<float> *_mer_out = nullptr,
        pipebuf<cf32> *_cstln_out = nullptr
    ) :
        runnable(sch, "Constellation receiver"),
        sampler(_sampler),
        cstln(nullptr),
        meas_decimation(1048576),
        pll_adjustment(1.0),
        allow_drift(false),
        kest(0.01),
        in(_in),
        out(_out, chunk_size),
        est_insp(cstln_amp * cstln_amp),
        agc_gain(1),
        mu(0),
        phase(0),
        est_sp(0),
        est_ep(0),
        meas_count(0)
    {
        set_omega(1);
        set_freq(0);
        freq_out = _freq_out ? new pipewriter<float>(*_freq_out) : nullptr;
        ss_out = _ss_out ? new pipewriter<float>(*_ss_out) : nullptr;
        mer_out = _mer_out ? new pipewriter<float>(*_mer_out) : nullptr;
        cstln_out = _cstln_out ? new pipewriter<cf32>(*_cstln_out) : nullptr;

        for (int i = 0; i < 3; i++)
        {
            hist[i].p = 0;
            hist[i].c = 0;
        }
    }

    ~cstln_receiver()
    {
        if (freq_out) {
            delete freq_out;
        }
        if (ss_out) {
            delete ss_out;
        }
        if (mer_out) {
            delete mer_out;
        }
        if (cstln_out) {
            delete cstln_out;
        }
    }

    void set_omega(float _omega, float tol = 10e-6)
    {
        omega = _omega;
        min_omega = omega * (1 - tol);
        max_omega = omega * (1 + tol);
        update_freq_limits();
    }

    void set_freq(float freq)
    {
        freqw = freq * 65536;
        update_freq_limits();
        refresh_freq_tap();
    }

    void set_allow_drift(bool d)
    {
        allow_drift = d;
    }

    void update_freq_limits()
    {
        // Prevent PLL from crossing +-SR/n/2 and locking at +-SR/n.
        int n = 4;

        if (cstln)
        {
            switch (cstln->nsymbols)
            {
            case 2:
                n = 2;
                break; // BPSK
            case 4:
                n = 4;
                break; // QPSK
            case 8:
                n = 8;
                break; // 8PSK
            case 16:
                n = 12;
                break; // 16APSK
            case 32:
                n = 16;
                break; // 32APSK
            default:
                n = 4;
                break;
            }
        }

        min_freqw = freqw - 65536 / max_omega / n / 2;
        max_freqw = freqw + 65536 / max_omega / n / 2;
    }

    void run()
    {
        if (!cstln) {
            fail("constellation not set");
        }

        // Magic constants that work with the qa recordings.
        float freq_alpha = 0.04;
        float freq_beta = 0.0012 / omega * pll_adjustment;
        float gain_mu = 0.02 / (cstln_amp * cstln_amp) * 2;
        int max_meas = chunk_size / meas_decimation + 1;

        // Large margin on output_size because mu adjustments
        // can lead to more than chunk_size/min_omega symbols.
        while (in.readable() >= chunk_size + sampler->readahead() &&
            out.writable() >= chunk_size &&
            (!freq_out || freq_out->writable() >= max_meas) &&
            (!ss_out || ss_out->writable() >= max_meas) &&
            (!mer_out || mer_out->writable() >= max_meas) &&
            (!cstln_out || cstln_out->writable() >= max_meas))
        {
            sampler->update_freq(freqw, chunk_size);

            std::complex<T> *pin = in.rd(), *pin0 = pin, *pend = pin + chunk_size;
            SOFTSYMB *pout = out.wr(), *pout0 = pout;

            // These are scoped outside the loop for SS and MER estimation.
            std::complex<float> sg{0.0f, 0.0f}; // Symbol before AGC;
            std::complex<float> s;  // For MER estimation and constellation viewer
            std::complex<signed char> *cstln_point = nullptr;

            while (pin < pend)
            {
                // Here mu is the time of the next symbol counted from 0 at pin.
                if (mu < 1)
                {
                    // Here 0<=mu<1 is the fractional time of the next symbol
                    // between pin and pin+1.
                    sg = sampler->interp(pin, mu, phase + mu * freqw);
                    s = sg * agc_gain;

                    // Constellation look-up
                    typename cstln_lut<SOFTSYMB, 256>::result *cr =
                        cstln->lookup(s.real(), s.imag());
                    *pout = cr->ss;
                    ++pout;

                    // PLL
                    phase += cr->phase_error * freq_alpha;
                    freqw += cr->phase_error * freq_beta;

                    // Modified Mueller and Müller
                    // mu[k]=real((c[k]-c[k-2])*conj(p[k-1])-(p[k]-p[k-2])*conj(c[k-1]))
                    //      =dot(c[k]-c[k-2],p[k-1]) - dot(p[k]-p[k-2],c[k-1])
                    // p = received signals
                    // c = decisions (constellation points)
                    hist[2] = hist[1];
                    hist[1] = hist[0];
                    hist[0].p.real(s.real());
                    hist[0].p.imag(s.imag());
                    cstln_point = &cstln->symbols[cr->symbol];
                    hist[0].c.real(cstln_point->real());
                    hist[0].c.imag(cstln_point->imag());
                    float muerr = ((hist[0].p.real() - hist[2].p.real()) * hist[1].c.real() + (hist[0].p.imag() - hist[2].p.imag()) * hist[1].c.imag())
                        - ((hist[0].c.real() - hist[2].c.real()) * hist[1].p.real() + (hist[0].c.imag() - hist[2].c.imag()) * hist[1].p.imag());
                    float mucorr = muerr * gain_mu;
                    const float max_mucorr = 0.1;

                    // TBD Optimize out statically
                    if (mucorr < -max_mucorr) {
                        mucorr = -max_mucorr;
                    }

                    if (mucorr > max_mucorr) {
                        mucorr = max_mucorr;
                    }

                    mu += mucorr;
                    mu += omega; // Next symbol time;
                }                // mu<1

                // Next sample
                ++pin;
                --mu;
                phase += freqw;
            } // chunk_size

            in.read(pin - pin0);
            out.written(pout - pout0);

            // Normalize phase so that it never exceeds 32 bits.
            // Max freqw is 2^31/65536/chunk_size = 256 Hz
            // (this may happen with leandvb --drift --decim).
            phase = fmodf(phase, 65536); // Rounding direction irrelevant

            if (cstln_point)
            {
                // Output the last interpolated PSK symbol, max once per chunk_size
                if (cstln_out) {
                    cstln_out->write(s);
                }

                // AGC
                // For APSK we must do AGC on the symbols, not the whole signal.
                // TODO Use a better estimator at low SNR.
                float insp = sg.real() * sg.real() + sg.imag() * sg.imag();
                est_insp = insp * kest + est_insp * (1 - kest);

                if (est_insp) {
                    agc_gain = cstln_amp / gen_sqrt(est_insp);
                }

                // SS and MER
                std::complex<float> ev(
                    s.real() - cstln_point->real(),
                    s.imag() - cstln_point->imag()
                );
                float sig_power, ev_power;

                if (cstln->nsymbols == 2)
                {
                    // Special case for BPSK: Ignore quadrature component of noise.
                    // TBD Projection on I axis assumes BPSK at 45°
                    float sig_real = (cstln_point->real() + cstln_point->imag()) * 0.707;
                    float ev_real = (ev.real() + ev.imag()) * 0.707;
                    sig_power = sig_real * sig_real;
                    ev_power = ev_real * ev_real;
                }
                else
                {
                    sig_power = (int)cstln_point->real() * cstln_point->real() + (int)cstln_point->imag() * cstln_point->imag();
                    ev_power = ev.real() * ev.real() + ev.imag() * ev.imag();
                }

                est_sp = sig_power * kest + est_sp * (1 - kest);
                est_ep = ev_power * kest + est_ep * (1 - kest);
            }

            // This is best done periodically ouside the inner loop,
            // but will cause non-deterministic output.

            if (!allow_drift)
            {
                if (freqw < min_freqw || freqw > max_freqw) {
                    freqw = (max_freqw + min_freqw) / 2;
                }
            }

            // Output measurements

            refresh_freq_tap();

            meas_count += pin - pin0;

            while (meas_count >= meas_decimation)
            {
                meas_count -= meas_decimation;

                if (freq_out) {
                    freq_out->write(freq_tap);
                }

                if (ss_out) {
                    ss_out->write(sqrtf(est_insp));
                }

                if (mer_out) {
                    mer_out->write(est_ep ? 10 * log10f(est_sp / est_ep) : 0);
                }
            }

        } // Work to do
    }

    float freq_tap;

    void refresh_freq_tap()
    {
        freq_tap = freqw / 65536;
    }

  private:
    struct
    {
        std::complex<float> p; // Received symbol
        std::complex<float> c; // Matched constellation point
    } hist[3];

    pipereader<std::complex<T>> in;
    pipewriter<SOFTSYMB> out;
    float est_insp, agc_gain;
    float mu;    // PSK time expressed in clock ticks
    float phase; // 65536=2pi
    // Signal estimation
    float est_sp; // Estimated RMS signal power
    float est_ep; // Estimated RMS error vector power
    unsigned long meas_count;
    pipewriter<float> *freq_out, *ss_out, *mer_out;
    pipewriter<cf32> *cstln_out;
};

// FAST QPSK RECEIVER

// Optimized for u8 input, no AGC, uses phase information only.
// Outputs hard symbols.

template <typename T>
struct fast_qpsk_receiver : runnable
{
    typedef u8 hardsymbol;
    unsigned long meas_decimation;           // Measurement rate
    float omega, min_omega, max_omega;       // Samples per symbol
    signed long freqw, min_freqw, max_freqw; // Freq offs (angle per sample)
    float pll_adjustment;
    bool allow_drift; // Follow carrier beyond safe limits
    static const unsigned int chunk_size = 128;

    fast_qpsk_receiver(
        scheduler *sch,
        pipebuf<std::complex<T>> &_in,
        pipebuf<hardsymbol> &_out,
        pipebuf<float> *_freq_out = nullptr,
        pipebuf<std::complex<T>> *_cstln_out = nullptr
    ) :
        runnable(sch, "Fast QPSK receiver"),
        meas_decimation(1048576),
        pll_adjustment(1.0),
        allow_drift(false),
        in(_in),
        out(_out, chunk_size),
        mu(0),
        phase(0),
        meas_count(0)
    {
        set_omega(1);
        set_freq(0);
        freq_out = _freq_out ? new pipewriter<float>(*_freq_out) : nullptr;
        cstln_out = _cstln_out ? new pipewriter<std::complex<T>>(*_cstln_out) : nullptr;
        memset(hist, 0, sizeof(hist));
        init_lookup_tables();
    }

    ~fast_qpsk_receiver()
    {
        if (freq_out) {
            delete freq_out;
        }
        if (cstln_out) {
            delete cstln_out;
        }
    }

    void set_omega(float _omega, float tol = 10e-6)
    {
        omega = _omega;
        min_omega = omega * (1 - tol);
        max_omega = omega * (1 + tol);
        update_freq_limits();
    }

    void set_freq(float freq)
    {
        freqw = freq * 65536;
        update_freq_limits();
    }

    void update_freq_limits()
    {
        // Prevent PLL from locking at +-symbolrate/4.
        // TODO The +-SR/8 limit is suitable for QPSK only.
        min_freqw = freqw - 65536 / max_omega / 8;
        max_freqw = freqw + 65536 / max_omega / 8;
    }

    static const int RLUT_BITS = 8;
    static const int RLUT_ANGLES = 1 << RLUT_BITS;

    void run()
    {
        // Magic constants that work with the qa recordings.
        signed long freq_alpha = 0.04 * 65536;
        signed long freq_beta = 0.0012 * 256 * 65536 / omega * pll_adjustment;

        if (!freq_beta) {
            fail("Excessive oversampling");
        }

        float gain_mu = 0.02 / (cstln_amp * cstln_amp) * 2;

        int max_meas = chunk_size / meas_decimation + 1;
        // Largin margin on output_size because mu adjustments
        // can lead to more than chunk_size/min_omega symbols.

        while (in.readable() >= chunk_size + 1 && // +1 for interpolation
               out.writable() >= chunk_size && (!freq_out || freq_out->writable() >= max_meas) && (!cstln_out || cstln_out->writable() >= max_meas))
        {
            std::complex<T> *pin = in.rd(), *pin0 = pin, *pend = pin + chunk_size;
            hardsymbol *pout = out.wr(), *pout0 = pout;

            cu8 s;
            u_angle symbol_arg = 0; // Exported for constellation viewer

            while (pin < pend)
            {
                // Here mu is the time of the next symbol counted from 0 at pin.
                if (mu < 1)
                {
                    // Here 0<=mu<1 is the fractional time of the next symbol
                    // between pin and pin+1.

                    // Derotate and interpolate
#if 0   /* Phase only (does not work)
           Careful with the float/signed/unsigned casts */
                    u_angle a0 = fast_arg(pin[0]) - phase;
                    u_angle a1 = fast_arg(pin[1]) - (phase+freqw);
                    s_angle da = a1 - a0;
                    symbol_arg = a0 + (s_angle)(da*mu);
                    s = arg_to_symbol(symbol_arg);
#elif 1 // Linear by lookup-table. 1.2M on bench3bishs
                    polar *p0 = &lut_polar[pin[0].real()][pin[0].imag()];
                    u_angle a0 = (u_angle)(p0->a - phase) >> (16 - RLUT_BITS);
                    cu8 *p0r = &lut_rect[a0][p0->r >> 1];
                    polar *p1 = &lut_polar[pin[1].real()][pin[1].imag()];
                    u_angle a1 = (u_angle)(p1->a - (phase + freqw)) >> (16 - RLUT_BITS);
                    cu8 *p1r = &lut_rect[a1][p1->r >> 1];
                    s.real((int)(p0r->real() + (p1r->real() - p0r->real()) * mu));
                    s.imag((int)(p0r->imag() + (p1r->imag() - p0r->imag()) * mu));
                    symbol_arg = fast_arg(s);
#else   // Linear floating-point, for reference
                    float a0 = -(int)phase * M_PI / 32768;
                    float cosa0 = cosf(a0), sina0 = sinf(a0);
                    std::complex<float>
                        p0r(((float)pin[0].real() - 128) * cosa0 - ((float)pin[0].imag() - 128) * sina0,
                            ((float)pin[0].real() - 128) * sina0 + ((float)pin[0].imag() - 128) * cosa0);
                    float a1 = -(int)(phase + freqw) * M_PI / 32768;
                    float cosa1 = cosf(a1), sina1 = sinf(a1);
                    std::complex<float>
                        p1r(((float)pin[1].real() - 128) * cosa1 - ((float)pin[1].imag() - 128) * sina1,
                            ((float)pin[1].real() - 128) * sina1 + ((float)pin[1].imag() - 128) * cosa1);
                    s.real() = (int)(128 + p0r.real() + (p1r.real() - p0r.real()) * mu);
                    s.imag() = (int)(128 + p0r.imag() + (p1r.imag() - p0r.imag()) * mu);
                    symbol_arg = fast_arg(s);
#endif

                    int quadrant = symbol_arg >> 14;
                    static unsigned char quadrant_to_symbol[4] = {0, 2, 3, 1};
                    *pout = quadrant_to_symbol[quadrant];
                    ++pout;

                    // PLL
                    s_angle phase_error = (s_angle)(symbol_arg & 16383) - 8192;
                    phase += (phase_error * freq_alpha + 32768) >> 16;
                    freqw += (phase_error * freq_beta + 32768 * 256) >> 24;

                    // Modified Mueller and Müller
                    // mu[k]=real((c[k]-c[k-2])*conj(p[k-1])-(p[k]-p[k-2])*conj(c[k-1]))
                    //      =dot(c[k]-c[k-2],p[k-1]) - dot(p[k]-p[k-2],c[k-1])
                    // p = received signals
                    // c = decisions (constellation points)
                    hist[2] = hist[1];
                    hist[1] = hist[0];
#define HIST_FLOAT 0
#if HIST_FLOAT
                    hist[0].p.real() = (float)s.real() - 128;
                    hist[0].p.imag() = (float)s.imag() - 128;

                    cu8 cp = arg_to_symbol((symbol_arg & 49152) + 8192);
                    hist[0].c.real() = (float)cp.real() - 128;
                    hist[0].c.imag() = (float)cp.imag() - 128;

                    float muerr =
                        ((hist[0].p.real() - hist[2].p.real()) * hist[1].c.real() +
                         (hist[0].p.imag() - hist[2].p.imag()) * hist[1].c.imag()) -
                        ((hist[0].c.real() - hist[2].c.real()) * hist[1].p.real() +
                         (hist[0].c.imag() - hist[2].c.imag()) * hist[1].p.imag());
#else
                    hist[0].p = s;
                    hist[0].c = arg_to_symbol((symbol_arg & 49152) + 8192);

                    int muerr =
                        ((signed char)(hist[0].p.real() - hist[2].p.real()) * ((int)hist[1].c.real() - 128) + (signed char)(hist[0].p.imag() - hist[2].p.imag()) * ((int)hist[1].c.imag() - 128)) - ((signed char)(hist[0].c.real() - hist[2].c.real()) * ((int)hist[1].p.real() - 128) + (signed char)(hist[0].c.imag() - hist[2].c.imag()) * ((int)hist[1].p.imag() - 128));
#endif
                    float mucorr = muerr * gain_mu;
                    const float max_mucorr = 0.1;

                    // TBD Optimize out statically
                    if (mucorr < -max_mucorr) {
                        mucorr = -max_mucorr;
                    }

                    if (mucorr > max_mucorr) {
                        mucorr = max_mucorr;
                    }

                    mu += mucorr;
                    mu += omega; // Next symbol time;
                }                // mu<1

                // Next sample
                ++pin;
                --mu;
                phase += freqw;
            } // chunk_size

            in.read(pin - pin0);
            out.written(pout - pout0);

            if (symbol_arg && cstln_out) {
                // Output the last interpolated PSK symbol, max once per chunk_size
                cstln_out->write(s);
            }

            // This is best done periodically ouside the inner loop,
            // but will cause non-deterministic output.

            if (!allow_drift)
            {
                if (freqw < min_freqw || freqw > max_freqw) {
                    freqw = (max_freqw + min_freqw) / 2;
                }
            }

            // Output measurements

            meas_count += pin - pin0;

            while (meas_count >= meas_decimation)
            {
                meas_count -= meas_decimation;

                if (freq_out) {
                    freq_out->write((float)freqw / 65536);
                }
            }

        } // Work to do
    }

  private:
    struct polar
    {
        u_angle a;
        unsigned char r;
    } lut_polar[256][256];

    u_angle fast_arg(const cu8 &c)
    {
        // TBD read cu8 as u16 index, same endianness as in init()
        return lut_polar[c.real()][c.imag()].a;
    }

    cu8 lut_rect[RLUT_ANGLES][256];
    cu8 lut_sincos[65536];

    cu8 arg_to_symbol(u_angle a)
    {
        return lut_sincos[a];
    }

    void init_lookup_tables()
    {
        for (int i = 0; i < 256; ++i)
        {
            for (int q = 0; q < 256; ++q)
            {
                // Don't cast float to unsigned directly
                lut_polar[i][q].a = (s_angle)(atan2f(q - 128, i - 128) * 65536 / (2 * M_PI));
                lut_polar[i][q].r = (int)hypotf(i - 128, q - 128);
            }
        }

        for (unsigned long a = 0; a < 65536; ++a)
        {
            float f = 2 * M_PI * a / 65536;
            lut_sincos[a].real(128 + cstln_amp * cosf(f));
            lut_sincos[a].imag(128 + cstln_amp * sinf(f));
        }

        for (int a = 0; a < RLUT_ANGLES; ++a)
        {
            for (int r = 0; r < 256; ++r)
            {
                lut_rect[a][r].real((int)(128 + r * cos(2 * M_PI * a / RLUT_ANGLES)));
                lut_rect[a][r].imag((int)(128 + r * sin(2 * M_PI * a / RLUT_ANGLES)));
            }
        }
    }

    struct
    {
#if HIST_FLOAT
        std::complex<float> p; // Received symbol
        std::complex<float> c; // Matched constellation point
#else
        cu8 p; // Received symbol
        cu8 c; // Matched constellation point
#endif
    } hist[3];

    pipereader<cu8> in;
    pipewriter<hardsymbol> out;
    float mu; // PSK time expressed in clock ticks. TBD fixed point.
    u_angle phase;
    unsigned long meas_count;
    pipewriter<float> *freq_out, *mer_out;
    pipewriter<cu8> *cstln_out;
};
// fast_qpsk_receiver

// CONSTELLATION TRANSMITTER

// Maps symbols to I/Q points.

template <typename Tout, int Zout>
struct cstln_transmitter : runnable
{
    cstln_lut<hard_ss, 256> *cstln;

    cstln_transmitter(
        scheduler *sch,
        pipebuf<u8> &_in,
        pipebuf<std::complex<Tout>> &_out
    ) :
        runnable(sch, "cstln_transmitter"),
        in(_in),
        out(_out),
        cstln(nullptr)
    {
    }

    void run()
    {
        if (!cstln) {
            fail("constellation not set");
        }

        int count = min(in.readable(), out.writable());
        u8 *pin = in.rd(), *pend = pin + count;
        std::complex<Tout> *pout = out.wr();

        for (; pin < pend; ++pin, ++pout)
        {
            std::complex<signed char> *cp = &cstln->symbols[*pin];
            pout->real(Zout + cp->real());
            pout->imag(Zout + cp->imag());
        }

        in.read(count);
        out.written(count);
    }

  private:
    pipereader<u8> in;
    pipewriter<std::complex<Tout>> out;
};
// cstln_transmitter

// FREQUENCY SHIFTER

// Resolution is sample_freq/65536.

template <typename T>
struct rotator : runnable
{
    rotator(
        scheduler *sch,
        pipebuf<std::complex<T>> &_in,
        pipebuf<std::complex<T>> &_out,
        float freq
    ) :
        runnable(sch, "rotator"),
        in(_in),
        out(_out),
        index(0)
    {
        int ifreq = freq * 65536;

        if (sch->debug) {
            fprintf(stderr, "Rotate: req=%f real=%f\n", freq, ifreq / 65536.0);
        }

        for (int i = 0; i < 65536; ++i)
        {
            lut_cos[i] = cosf(2 * M_PI * i * ifreq / 65536);
            lut_sin[i] = sinf(2 * M_PI * i * ifreq / 65536);
        }
    }

    void run()
    {
        unsigned long count = min(in.readable(), out.writable());
        std::complex<T> *pin = in.rd(), *pend = pin + count;
        std::complex<T> *pout = out.wr();

        for (; pin < pend; ++pin, ++pout, ++index)
        {
            float c = lut_cos[index];
            float s = lut_sin[index];
            pout->real(pin->real() * c - pin->imag() * s);
            pout->imag(pin->real() * s + pin->imag() * c);
        }

        in.read(count);
        out.written(count);
    }

  private:
    pipereader<std::complex<T>> in;
    pipewriter<std::complex<T>> out;
    float lut_cos[65536];
    float lut_sin[65536];
    unsigned short index; // Current phase
};
// rotator

// SPECTRUM-BASED CNR ESTIMATOR

// Assumes that the spectrum is as follows:
//
//  ---|--noise---|-roll-off-|---carrier+noise----|-roll-off-|---noise--|---
//     |  (bw/2)  |   (bw)   |       (bw/2)       |   (bw)   |  (bw/2)  |
//
// Maximum roll-off 0.5

template <typename T>
struct cnr_fft : runnable
{
    cnr_fft(
        scheduler *sch,
        pipebuf<std::complex<T>> &_in,
        pipebuf<float> &_out,
        float _bandwidth,
        int nfft = 4096
    ) :
        runnable(sch, "cnr_fft"),
        bandwidth(_bandwidth),
        freq_tap(nullptr),
        tap_multiplier(1),
        decimation(1048576),
        kavg(0.1),
        in(_in),
        out(_out),
        fft(nfft < 128 ? 128 : nfft > 4096 ? 4096 : nfft),
        avgpower(nullptr),
        sorted(nullptr),
        data(nullptr),
        power(nullptr),
        phase(0),
        cslots_ratio(0.2),
        nslots_shift_ratio(0.65),
        nslots_ratio(0.1)
    {
        fprintf(stderr, "cnr_fft::cnr_fft: bw: %f FFT: %d\n", bandwidth, fft.size());

        if (bandwidth != 0.5) {
            fail("cnr_fft::cnr_fft: CNR estimator requires Fsampling = 2x Fsymbol");
        }
    }

    ~cnr_fft()
    {
        if (avgpower) {
            delete[] avgpower;
        }
        if (sorted) {
            delete[] sorted;
        }
        if (data) {
            delete[] data;
        }
        if (power) {
            delete[] power;
        }
    }

    void run()
    {
        while (in.readable() >= fft.size() && out.writable() >= 1)
        {
            phase += fft.size();

            if (phase >= decimation)
            {
                phase -= decimation;
                do_cnr();
            }

            in.read(fft.size());
        }
    }

    float bandwidth;
    float *freq_tap, tap_multiplier;
    int decimation;
    float kavg;

  private:
    void do_cnr()
    {
        if (!sorted) {
            sorted = new T[fft.size()];
        }
        if (!data) {
            data = new std::complex<T>[fft.size()];
        }
        if (!power) {
            power = new T[fft.size()];
        }

        float center_freq = freq_tap ? *freq_tap * tap_multiplier : 0;
        int icf = floor(center_freq * fft.size() + 0.5);
        memcpy(data, in.rd(), fft.size() * sizeof(data[0]));
        fft.inplace(data, true);

        for (int i = 0; i < fft.size(); ++i)
            power[i] = data[i].real() * data[i].real() + data[i].imag() * data[i].imag();

        if (!avgpower)
        {
            // Initialize with first spectrum
            avgpower = new T[fft.size()];
            memcpy(avgpower, power, fft.size() * sizeof(avgpower[0]));
        }

        // Accumulate and low-pass filter (exponential averaging)
        for (int i = 0; i < fft.size(); ++i) {
            avgpower[i] = avgpower[i] * (1 - kavg) + power[i] * kavg;
        }

#define LEANDVB_SDR_CNR_METHOD 2
#if LEANDVB_SDR_CNR_METHOD == 0
        int bwslots = (bandwidth / 4) * fft.size();

        if (!bwslots) {
            return;
        }

        // Measure carrier+noise in center band
        float c2plusn2 = avgslots(icf-bwslots, icf+bwslots);
        // Measure noise left and right of roll-off zones
        float n2 = ( avgslots(icf-bwslots*4, icf-bwslots*3) +
		    avgslots(icf+bwslots*3, icf+bwslots*4) ) / 2;
#elif LEANDVB_SDR_CNR_METHOD == 1
        int cbwslots = bandwidth * cslots_ratio * fft.size();
        int nstart = bandwidth * nslots_shift_ratio * fft.size();
        int nstop = nstart + bandwidth * nslots_ratio * fft.size();

        if (!cbwslots || !nstart || !nstop) {
            return;
        }

        // Measure carrier+noise in center band
        float c2plusn2 = avgslots(icf - cbwslots, icf + cbwslots);
        // Measure noise left and right of roll-off zones
        float n2 = (avgslots(icf - nstop, icf - nstart) +
            avgslots(icf + nstart, icf + nstop)) / 2;
#elif LEANDVB_SDR_CNR_METHOD == 2
        int bw = bandwidth * 0.6 * fft.size();
        float c2plusn2 = 0;
        float n2 = 0;
        minmax(icf - bw, icf + bw, n2, c2plusn2);
#endif
        float c2 = c2plusn2 - n2;
        float cnr = (c2 > 0 && n2 > 0) ? 10 * log10f(c2 / n2) : -50;
        out.write(cnr);
    }

    float avgslots(int i0, int i1)
    { // i0 <= i1
        T s = 0;

        for (int i = i0; i <= i1; ++i)
        {
            int j = i < 0 ? fft.size() + i : i;
            s += avgpower[j < 0 ? 0 : j >= fft.size() ? fft.size()-1 : j];
        }

        return s / (i1 - i0 + 1);
    }

    void minmax(int i0, int i1, float& min, float&max)
    {
        int l = 0;

        for (int i = i0; i <= i1 && l < fft.size(); ++i, ++l)
        {
            int j = i < 0 ? fft.size() + i : i;
            sorted[l] = avgpower[j < 0 ? 0 : j >= fft.size() ? fft.size()-1 : j];
        }

        std::sort(sorted, &sorted[l]);
        int m = l/5;
        min = std::accumulate<T*>(&sorted[0], &sorted[m], (T) 0) / (m+1);
        max = std::accumulate<T*>(&sorted[l-m], &sorted[l], (T) 0) / (m+1);

        // fprintf(stderr, "l: %d m: %d min: %f max: %f\n", l, m, min, max);
    }

    pipereader<std::complex<T>> in;
    pipewriter<float> out;
    cfft_engine<T> fft;
    T *avgpower;
    T *sorted;
    std::complex<T> *data;
    T *power;
    int phase;
    float cslots_ratio;
    float nslots_shift_ratio;
    float nslots_ratio;
};
// cnr_fft

template <typename T, int NFFT>
struct spectrum : runnable
{
    int decimation;
    float kavg;
    int decim;

    spectrum(
        scheduler *sch,
        pipebuf<std::complex<T>> &_in,
        pipebuf<float[NFFT]> &_out
    ) :
        runnable(sch, "spectrum"),
        decimation(1048576),
        kavg(0.1),
        decim(1), in(_in),
        out(_out),
        fft(NFFT),
        avgpower(nullptr),
        phase(0)
    {
    }

    ~spectrum()
    {
        if (avgpower) {
            delete avgpower;
        }
    }

    void run()
    {
        while (in.readable() >= fft.n * decim && out.writable() >= 1)
        {
            phase += fft.n * decim;

            if (phase >= decimation)
            {
                phase -= decimation;
                do_spectrum();
            }

            in.read(fft.n * decim);
        }
    }

  private:
    void do_spectrum()
    {
        std::complex<T> data[fft.n];

        if (decim == 1)
        {
            memcpy(data, in.rd(), fft.n * sizeof(data[0]));
        }
        else
        {
            std::complex<T> *pin = in.rd();

            for (int i = 0; i < fft.n; ++i, pin += decim) {
                data[i] = *pin;
            }
        }

        fft.inplace(data, true);
        float power[NFFT];

        for (int i = 0; i < fft.n; ++i) {
            power[i] = (float)data[i].real() * data[i].real() + (float)data[i].imag() * data[i].imag();
        }

        if (!avgpower)
        {
            // Initialize with first spectrum
            avgpower = new float[fft.n];
            memcpy(avgpower, power, fft.n * sizeof(avgpower[0]));
        }

        // Accumulate and low-pass filter
        for (int i = 0; i < fft.n; ++i)
            avgpower[i] = avgpower[i] * (1 - kavg) + power[i] * kavg;

        // Reuse power[]
        for (int i = 0; i < fft.n / 2; ++i)
        {
            power[i] = 10 * log10f(avgpower[NFFT / 2 + i]);
            power[NFFT / 2 + i] = 10 * log10f(avgpower[i]);
        }

        memcpy(out.wr(), power, sizeof(power[0]) * NFFT);
        out.written(1);
    }

    pipereader<std::complex<T>> in;
    pipewriter<float[NFFT]> out;
    cfft_engine<T> fft;
    T *avgpower;
    int phase;
};
// spectrum

} // namespace leansdr

#endif // LEANSDR_SDR_H
