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

#ifndef INCLUDE_ACARSIRIDIUMCHANNELIZER_H
#define INCLUDE_ACARSIRIDIUMCHANNELIZER_H

#include <complex>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <vector>

#if defined(USE_SSE4_1)
#include <smmintrin.h>
#endif

#include "acarsiridiumfft.h"

// Oversampled polyphase channelizer, so one Iridium receiver can watch the whole band.
//
// Iridium hops: a duplex channel is assigned when a call is set up, and over 240 channels
// spaced 41.67 kHz across roughly 1619 to 1626 MHz. Watching one 125 kHz channel sees
// only a few percent of the traffic - measured on a four minute recording, the busiest
// single channel carried 4% of the short burst data frames in the band. Covering it all
// with one demodulator per channel would mean dozens of them, so the band is split here
// instead, in one pass, and a receiver run on every output.
//
// A plain filter bank decimates by the number of channels, which puts the output rate
// equal to the channel spacing and leaves no room for a signal that does not sit exactly
// in the middle of its channel. This one decimates by a quarter of that, so each output
// runs at four times the channel spacing:
//
//     channels M, decimate by D = M / 4, so the output rate is 4 * inputRate / M
//
// The channel spacing is then inputRate / M and the output rate four times it. At
// 31.25 kHz spacing and 125 kHz out, an Iridium burst - about 35 kHz wide, and up to
// 37 kHz off its nominal frequency once Doppler has had its way - is always wholly
// inside at least one output. It is often inside two, so the scanner above this has to
// throw the duplicates away.
//
// Oversampling by a whole number of channels is what keeps this simple: decimating by
// M / 4 rather than M leaves a phase step between output blocks of a quarter turn per
// channel per block, which is a power of -j and so costs nothing to apply.

class AcarsIridiumChannelizer
{
public:
    typedef std::complex<double> Cd;

    //! How much each channel is oversampled: the output rate is this many times the
    //! channel spacing. Four leaves room for a burst anywhere in its channel
    static const int Oversample = 4;

    AcarsIridiumChannelizer() :
        m_channels(0),
        m_decimation(0),
        m_tapsPerBranch(0),
        m_length(0),
        m_write(0),
        m_count(0),
        m_block(0)
    {
    }

    //! channels must be a power of two and a multiple of Oversample. The input rate has
    //! to be channels * spacing, and for Iridium the spacing wanted is 31.25 kHz, so
    //! 8 MS/s gives 256 channels, 4 MS/s gives 128, and so on
    void configure(int channels, int tapsPerBranch = 12)
    {
        m_channels = channels;
        m_decimation = channels / Oversample;
        m_tapsPerBranch = tapsPerBranch;
        m_length = channels * tapsPerBranch;

        designPrototype();

        m_delay.assign(m_length, Cd(0.0, 0.0));
        m_out.assign(m_channels, Cd(0.0, 0.0));
        m_write = 0;
        m_count = 0;
        m_block = 0;

        m_fft.configure(channels);
        buildRotations();
    }

    int channels() const { return m_channels; }
    int decimation() const { return m_decimation; }

    //! Channel spacing, which is also how far apart the channel centres below are
    double spacing(double inputRate) const { return inputRate / (double) m_channels; }

    //! Output sample rate of every channel
    double channelRate(double inputRate) const { return inputRate / (double) m_decimation; }

    //! Where channel c sits relative to the centre of the band, in Hz. The upper half of
    //! the channels are the negative frequencies, as they are in any transform
    double channelOffset(int c, double inputRate) const
    {
        const double step = spacing(inputRate);

        return (c < (m_channels / 2)) ? (c * step) : ((c - m_channels) * step);
    }

    //! Feeds one input sample. Returns true when a block of channel samples is ready,
    //! one per channel, in channelSamples()
    bool processSample(const Cd& sample)
    {
        m_delay[m_write] = sample;
        m_write = (m_write + 1) % m_length;
        m_count++;

        if (m_count < m_decimation) {
            return false;
        }

        m_count = 0;
        computeBlock();

        return true;
    }

    const std::vector<Cd>& channelSamples() const { return m_out; }

    //! The prototype lowpass the bank is built from. Exposed so a test can filter a
    //! channel directly with the same taps and check the bank against it
    const std::vector<double>& prototype() const { return m_proto; }

private:

    //! Prototype lowpass, Kaiser windowed sinc.
    //!
    //! The cutoff is a little wider than the channel spacing, so that a burst sitting
    //! between two channel centres is still passed whole by one of them. That makes
    //! neighbouring channels overlap, which is why the same burst can be found twice.
    //! The stopband has to be inside the output Nyquist - two channel spacings away with
    //! this much oversampling - or what is rejected would fold back in
    void designPrototype()
    {
        const int taps = m_length;
        const double cutoff = 1.05 / (double) m_channels;    // Cycles per input sample
        const double beta = 0.1102 * (60.0 - 8.7);           // Kaiser, for 60 dB stopband

        m_proto.assign(taps, 0.0);
        double sum = 0.0;

        for (int n = 0; n < taps; n++)
        {
            const double x = (double) n - ((double) (taps - 1) / 2.0);
            const double sinc = (std::fabs(x) < 1e-12)
                              ? (2.0 * cutoff)
                              : (std::sin(2.0 * M_PI * cutoff * x) / (M_PI * x));
            const double t = ((2.0 * (double) n) / (double) (taps - 1)) - 1.0;
            const double window = besselI0(beta * std::sqrt(std::max(0.0, 1.0 - (t * t)))) / besselI0(beta);

            m_proto[n] = sinc * window;
            sum += m_proto[n];
        }

        // Unit gain at DC, so a channel output has the same amplitude as its input
        for (int n = 0; n < taps; n++) {
            m_proto[n] *= (double) m_channels / sum;
        }
    }

    static double besselI0(double x)
    {
        double sum = 1.0;
        double term = 1.0;

        for (int k = 1; k < 40; k++)
        {
            term *= (x * x) / (4.0 * (double) k * (double) k);
            sum += term;

            if (term < (1e-16 * sum)) {
                break;
            }
        }

        return sum;
    }

    void buildRotations()
    {
        // The phase each channel output carries, as a table indexed by the block counter
        // and the channel.
        //
        // Decimating by M / Oversample rather than by M leaves a step of
        // exp(-j2.pi.c.r / Oversample) on channel c at block r, which repeats every
        // Oversample blocks. On top of that the first block is emitted once D samples
        // have arrived, so it corresponds to input sample D - 1 rather than 0, and that
        // offset puts a further constant exp(-j2.pi.c.(D - 1) / M) on each channel.
        // Differential demodulation would not notice a constant phase, but folding it in
        // costs nothing and makes a channel output exactly what mixing that channel down
        // and filtering it would have given - which is what the test checks against
        m_rotation.assign(Oversample * m_channels, Cd(0.0, 0.0));

        for (int r = 0; r < Oversample; r++)
        {
            for (int c = 0; c < m_channels; c++)
            {
                const double angle = -2.0 * M_PI * (double) c
                                   * (((double) (r * m_decimation)) + (double) (m_decimation - 1))
                                   / (double) m_channels;

                m_rotation[(r * m_channels) + c] = Cd(std::cos(angle), std::sin(angle));
            }
        }
    }

    //! out[i] += in[-i] * h[i], for a run of i. The input walks backwards, because branch
    //! p of the bank takes the sample p further back down the delay line, and the run has
    //! already been cut so that neither side crosses the end of the buffer.
    //!
    //! A complex double is exactly one 128 bit register, so a vector unit does the
    //! multiply and the add on the real and imaginary parts together rather than one
    //! after the other. Two runs are kept going at once because each is a chain of
    //! dependent adds, and one chain on its own leaves the pipeline waiting on itself
    static void accumulate(Cd *out, const Cd *in, const double *h, int run)
    {
#if defined(USE_SSE4_1)
        int i = 0;

        for (; (i + 1) < run; i += 2)
        {
            const __m128d a0 = _mm_loadu_pd((const double *) (in - i));
            const __m128d a1 = _mm_loadu_pd((const double *) (in - i - 1));
            const __m128d o0 = _mm_loadu_pd((double *) (out + i));
            const __m128d o1 = _mm_loadu_pd((double *) (out + i + 1));

            _mm_storeu_pd((double *) (out + i),
                          _mm_add_pd(o0, _mm_mul_pd(a0, _mm_set1_pd(h[i]))));
            _mm_storeu_pd((double *) (out + i + 1),
                          _mm_add_pd(o1, _mm_mul_pd(a1, _mm_set1_pd(h[i + 1]))));
        }

        for (; i < run; i++)
        {
            const __m128d a = _mm_loadu_pd((const double *) (in - i));
            const __m128d o = _mm_loadu_pd((double *) (out + i));

            _mm_storeu_pd((double *) (out + i),
                          _mm_add_pd(o, _mm_mul_pd(a, _mm_set1_pd(h[i]))));
        }
#else
        for (int i = 0; i < run; i++)
        {
            const Cd& sample = in[-i];
            const double tap = h[i];

            out[i] += Cd(sample.real() * tap, sample.imag() * tap);
        }
#endif
    }

    void computeBlock()
    {
        const int newest = (m_write - 1 + m_length) % m_length;

        // Each branch filters every Mth sample of the delay line, which is what makes one
        // prototype filter serve as the whole bank. Written straight into the output,
        // because the transform below is in place and a copy of the whole bank every
        // block is megabytes a second for nothing.
        //
        // The loop runs tap first and branch second, which looks the wrong way round -
        // the branch accumulator cannot stay in a register, and the output is read and
        // written once per tap instead of once per block. It is much faster anyway,
        // because branch first walks both the delay line and the prototype in strides of
        // a whole channel count: at 256 channels that is a 4 kB step per access, which no
        // prefetcher will follow. This way all three arrays are walked one element at a
        // time, which also makes the inner loop something a vector unit can do
        std::fill(m_out.begin(), m_out.end(), Cd(0.0, 0.0));

        for (int q = 0; q < m_tapsPerBranch; q++)
        {
            // Branch p reads the sample this far back, so p ascending walks backwards
            // down the delay line from here
            int index = newest - (q * m_channels);

            while (index < 0) {
                index += m_length;
            }

            const double *tap = &m_proto[q * m_channels];
            int p = 0;

            while (p < m_channels)
            {
                // As far as can be gone before the delay line wraps round to its end
                const int run = std::min(m_channels - p, index + 1);

                accumulate(&m_out[p], &m_delay[index], &tap[p], run);

                p += run;
                index -= run;

                if (index < 0) {
                    index += m_length;
                }
            }
        }

        m_fft.transform(m_out, true);

        // The phase for this block, which is where the oversampling is paid for
        const int base = m_block * m_channels;

        for (int c = 0; c < m_channels; c++) {
            m_out[c] *= m_rotation[base + c];
        }

        m_block = (m_block + 1) % Oversample;
    }

    int m_channels;
    int m_decimation;
    int m_tapsPerBranch;
    int m_length;
    int m_write;
    int m_count;
    int m_block;

    std::vector<double> m_proto;
    std::vector<Cd> m_delay;
    std::vector<Cd> m_out;
    std::vector<Cd> m_rotation;
    AcarsIridiumFft m_fft;
};

#endif // INCLUDE_ACARSIRIDIUMCHANNELIZER_H
