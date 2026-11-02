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

//! The transform the bank runs on each block, so that a faster one can be put behind it.
//!
//! The built-in radix 2 is what keeps the channelizer self contained, which is what lets
//! the offline harnesses in test/ build against Qt alone. It is also the single largest
//! thing the bank spends its time on - measured against FFTW on the same 256 point
//! transform it is over five times slower - so the plugin, which has sdrbase, installs
//! SDRangel's FFT engine instead
class AcarsIridiumTransform
{
public:
    virtual ~AcarsIridiumTransform() {}

    //! Always the inverse transform, unscaled, which is what the bank wants
    virtual void configure(int size) = 0;
    virtual void transform(std::vector<std::complex<double>>& data) = 0;
};

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

    //! Puts a different transform behind the bank. Must be called before configure, and
    //! the caller keeps ownership. Passing nullptr goes back to the built-in one
    void setTransform(AcarsIridiumTransform *transform)
    {
        m_transform = transform ? transform : &m_builtIn;
    }

    //! channels must be a power of two and a multiple of Oversample. The input rate has
    //! to be channels * spacing, and for Iridium the spacing wanted is 31.25 kHz, so
    //! 8 MS/s gives 256 channels, 4 MS/s gives 128, and so on.
    //!
    //! The taps per branch set what the filtering costs: the work is the input rate times
    //! the oversampling times this, and nothing else - the channel count cancels out. It
    //! was twelve, which left the prototype far better than it needs to be. At eight it
    //! still has 70 dB beyond the output Nyquist, which is what folds back, and reaches
    //! 60 dB by 1.28 channel spacings against the two that are available. The cost of
    //! going lower is not aliasing but a softer channel edge, which lets more of a burst
    //! into its neighbours: measured on two recordings, eight finds slightly more bursts
    //! than twelve and rather more of them are duplicates of each other
    void configure(int channels, int tapsPerBranch = 8)
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

        m_transform->configure(channels);
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

        // A conditional wrap, not a modulo: m_length is a member, so the compiler cannot
        // turn the division into a multiply, and this runs once per input sample - eight
        // million times a second at the widest setting
        if (++m_write == m_length) {
            m_write = 0;
        }
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
    //! One chunk of channels, all taps, with the sums held in registers.
    //!
    //! The other way round - one tap across every channel, which is what this did - has
    //! to read and write the output array once per tap, so a complex multiply and add
    //! costs three loads and a store. Holding a chunk's sums across all the taps leaves
    //! two loads and nothing else, and the output is written once.
    //!
    //! Every bound here is the constant, never a variable count, so that the loops unroll
    //! and the accumulators become registers. Written with a runtime chunk length instead
    //! this was 30% SLOWER than the version it replaced, because the array of sums went
    //! on the stack and the loads and stores came straight back
    void accumulateChunk(int p, int newest)
    {
#if defined(USE_SSE4_1)
        __m128d acc[ChunkChannels];

        for (int i = 0; i < ChunkChannels; i++) {
            acc[i] = _mm_setzero_pd();
        }

        for (int q = 0; q < m_tapsPerBranch; q++)
        {
            // Branch p reads the sample this far back, and within the chunk the index
            // walks backwards one per channel
            int index = newest - (q * m_channels) - p;

            while (index < 0) {
                index += m_length;
            }

            const double *tap = &m_proto[(q * m_channels) + p];

            if (index >= (ChunkChannels - 1))
            {
                // The whole chunk is there without the delay line wrapping round
                const Cd *in = &m_delay[index];

                for (int i = 0; i < ChunkChannels; i++)
                {
                    const __m128d a = _mm_loadu_pd((const double *) (in - i));

                    acc[i] = _mm_add_pd(acc[i], _mm_mul_pd(a, _mm_set1_pd(tap[i])));
                }
            }
            else
            {
                // At most one chunk per tap straddles the end of the delay line
                for (int i = 0; i < ChunkChannels; i++)
                {
                    int j = index - i;

                    if (j < 0) {
                        j += m_length;
                    }

                    const __m128d a = _mm_loadu_pd((const double *) &m_delay[j]);

                    acc[i] = _mm_add_pd(acc[i], _mm_mul_pd(a, _mm_set1_pd(tap[i])));
                }
            }
        }

        for (int i = 0; i < ChunkChannels; i++) {
            _mm_storeu_pd((double *) &m_out[p + i], acc[i]);
        }
#else
        Cd acc[ChunkChannels];

        for (int i = 0; i < ChunkChannels; i++) {
            acc[i] = Cd(0.0, 0.0);
        }

        for (int q = 0; q < m_tapsPerBranch; q++)
        {
            int index = newest - (q * m_channels) - p;

            while (index < 0) {
                index += m_length;
            }

            const double *tap = &m_proto[(q * m_channels) + p];

            for (int i = 0; i < ChunkChannels; i++)
            {
                int j = index - i;

                if (j < 0) {
                    j += m_length;
                }

                const Cd& sample = m_delay[j];

                acc[i] += Cd(sample.real() * tap[i], sample.imag() * tap[i]);
            }
        }

        for (int i = 0; i < ChunkChannels; i++) {
            m_out[p + i] = acc[i];
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
        // A chunk of channels at a time, with every tap applied before moving on. Taking
        // one tap across all the channels instead walks all three arrays one element at
        // a time, which looks better and was what this did, but it has to read and write
        // the output once per tap. Doing it this way the chunk's sums never leave the
        // registers and the output is written once, which is also why nothing needs
        // zeroing first. What must not be done is a whole branch at a time, which walks
        // the delay line and the prototype in strides of a whole channel count - at 256
        // channels a 4 kB step per access, which no prefetcher will follow
        // The channel count is a power of two and never below the chunk, so every chunk
        // is full - which is what lets the loops inside run to a constant bound
        for (int p = 0; p < m_channels; p += ChunkChannels)
        {
            accumulateChunk(p, newest);
        }

        m_transform->transform(m_out);

        // The phase for this block, which is where the oversampling is paid for
        const int base = m_block * m_channels;

        for (int c = 0; c < m_channels; c++) {
            m_out[c] *= m_rotation[base + c];
        }

        m_block = (m_block + 1) % Oversample;
    }

    //! Channels whose sums are carried in registers together. Eight complex doubles is
    //! eight of the sixteen vector registers, leaving room for the loads and the taps
    static const int ChunkChannels = 8;

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

    //! The transform the bank actually runs, and the one it falls back on
    class BuiltInTransform : public AcarsIridiumTransform
    {
    public:
        void configure(int size) override { m_fft.configure(size); }
        void transform(std::vector<Cd>& data) override { m_fft.transform(data, true); }
    private:
        AcarsIridiumFft m_fft;
    };

    BuiltInTransform m_builtIn;
    AcarsIridiumTransform *m_transform = &m_builtIn;
};

#endif // INCLUDE_ACARSIRIDIUMCHANNELIZER_H
