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

#ifndef INCLUDE_ACARSIRIDIUMDETECTOR_H
#define INCLUDE_ACARSIRIDIUMDETECTOR_H

#include <QtGlobal>

#include <cmath>
#include <complex>
#include <vector>

#if defined(USE_SSE4_1)
#include <smmintrin.h>
#endif

// Burst detection for a whole bank of channels at once.
//
// Every channel runs the same half dozen arithmetic operations on each of its samples -
// power, a smoothing step, a noise floor step and two comparisons - and at the widest
// setting that is thirty two million of them a second, because the channelizer produces
// four samples per channel for every input sample and there are two hundred and fifty six
// channels. Held one receiver object at a time, that work is slow for a reason that is
// not arithmetic: each block touches a couple of cache lines in each of 256 objects that
// are tens of kilobytes apart, so the whole thing runs at the speed of the cache rather
// than of the adds.
//
// Holding the state as one array per quantity instead puts a block's worth of each in a
// handful of cache lines, and makes the arithmetic something a vector unit can do two
// channels at a time. The samples a burst is made of are not here - those stay with the
// receiver, which is what knows how to demodulate them.
//
// The order of operations within a channel is exactly the order the single channel path
// used, because the two have to agree: the scanner's output is checked against recordings
// frame for frame, and a different rounding order would be a different set of frames.

class AcarsIridiumDetectorBank
{
public:
    typedef std::complex<double> Cd;

    enum Event
    {
        Started,        //!< A burst began on this channel in this block
        Ended           //!< It ended, and the receiver should decode what it has
    };

    struct Change
    {
        int m_channel;
        Event m_event;
    };

    AcarsIridiumDetectorBank() :
        m_channels(0),
        m_startThreshold(8.0),
        m_endThreshold(3.0),
        m_startSamples(8),
        m_endSamples(16),
        m_preTriggerLength(16),
        m_preTriggerWrite(0),
        m_preTriggerCount(0),
        m_sampleIndex(0)
    {
    }

    //! The thresholds are ratios against the tracked noise floor, and the counts are how
    //! many samples either side of one is needed before a burst starts or ends
    void configure(int channels, double startThreshold, double endThreshold,
                   int startSamples, int endSamples, int preTriggerLength)
    {
        m_channels = channels;
        m_startThreshold = startThreshold;
        m_endThreshold = endThreshold;
        m_startSamples = startSamples;
        m_endSamples = endSamples;
        m_preTriggerLength = preTriggerLength;

        reset();
    }

    void reset()
    {
        m_smoothed.assign(m_channels, 0.0);
        m_noiseFloor.assign(m_channels, 0.0);
        m_noiseValid.assign(m_channels, 0);
        m_inBurst.assign(m_channels, 0);
        m_aboveCount.assign(m_channels, 0);
        m_belowCount.assign(m_channels, 0);

        // One ring for the whole bank, holding whole blocks rather than a ring per
        // channel, and laid out block first so that a block is written as one run.
        //
        // The other way round - all of a channel's history together - is the obvious
        // layout and is much worse: a block then writes one sample into each channel's
        // own stretch, a cache line apart, so at 256 channels it touches 256 lines over
        // 64 kB and falls out of L1. That showed up as the detector costing 80% more per
        // sample at 256 channels than at 128, while every other stage stayed flat.
        // Reading one channel's history is strided instead, but that only happens when a
        // burst starts, which is thousands of blocks apart
        m_preTrigger.assign((size_t) m_channels * m_preTriggerLength, Cd(0.0, 0.0));
        m_preTriggerWrite = 0;
        m_preTriggerCount = 0;
        m_sampleIndex = 0;
        m_burstChannels.clear();
    }

    //! The index the next block will have, which is what a burst start is measured from
    qint64 sampleIndex() const { return m_sampleIndex; }
    int preTriggerCount() const { return m_preTriggerCount; }

    //! The channels currently receiving a burst, so the caller does not have to look
    const std::vector<int>& burstChannels() const { return m_burstChannels; }

    bool inBurst(int channel) const { return m_inBurst[channel] != 0; }

    //! The tracked noise floor for a channel, which is what a burst's signal to noise is
    //! measured against
    double noiseFloor(int channel) const { return m_noiseFloor[channel]; }



    //! Ends a burst the caller has given up on, without decoding it - a channel that has
    //! been transmitting continuously for longer than any burst can be. The noise floor
    //! is invalidated so it re-converges rather than staying at whatever it was
    void abandonBurst(int channel)
    {
        m_inBurst[channel] = 0;
        m_aboveCount[channel] = 0;
        m_noiseValid[channel] = 0;
        removeBurstChannel(channel);
    }

    //! Feeds one sample per channel.
    //!
    //! append(channel, sample) is called for every channel that was already receiving
    //! when the block arrived, and returns true if that channel has now taken more
    //! samples than any burst can be - it is given the sample before the end test, which
    //! is the order the single channel path used, so the sample a burst ends on is part
    //! of it. A channel that starts in this block is not appended to: its samples so far
    //! are the pre-trigger history.
    //!
    //! begin(channel, burstStart) is called when a burst starts and returns the buffer to
    //! put that history in, which is filled here rather than afterwards: this block's
    //! sample goes into the ring at the end, over the oldest one, so by the time the
    //! caller had a chance to read it the window would be short of where it started
    template <typename Append, typename Begin>
    void processBlock(const Cd *samples, std::vector<Change>& changes, Append append, Begin begin)
    {
        power(samples);
        continueBursts(samples, changes, append);
        idleAndHistory(samples, changes, begin);

        m_sampleIndex++;
    }

private:

    //! Power and the smoothing step, for every channel. Nothing here depends on whether a
    //! channel is in a burst, so it is one straight run over two arrays
    void power(const Cd *samples)
    {
        const double *in = (const double *) samples;
        double *smoothed = m_smoothed.data();
        int c = 0;

#if defined(USE_SSE4_1)
        const __m128d alpha = _mm_set1_pd(0.25);

        for (; c + 2 <= m_channels; c += 2)
        {
            // Two complex samples are four doubles: multiply by themselves and add the
            // halves across to get the two norms
            const __m128d a = _mm_loadu_pd(in + (2 * c));
            const __m128d b = _mm_loadu_pd(in + (2 * c) + 2);
            const __m128d aa = _mm_mul_pd(a, a);
            const __m128d bb = _mm_mul_pd(b, b);
            const __m128d p = _mm_hadd_pd(aa, bb);

            const __m128d s = _mm_loadu_pd(smoothed + c);

            _mm_storeu_pd(smoothed + c,
                          _mm_add_pd(s, _mm_mul_pd(_mm_sub_pd(p, s), alpha)));
        }
#endif

        for (; c < m_channels; c++)
        {
            const double p = std::norm(samples[c]);

            smoothed[c] += (p - smoothed[c]) * 0.25;
        }
    }

    //! The noise floor, the start test and the history ring, in one pass.
    //!
    //! One pass rather than three: the arithmetic here is only a handful of operations
    //! per channel, so walking the arrays is most of what it costs, and splitting it up
    //! was measurably worse than the single fused loop it replaced. Within a channel the
    //! order is the one the single channel path used - the idle test, then the start,
    //! then the ring write, so that a burst's history is taken before this block's
    //! sample lands on the oldest slot
    template <typename Begin>
    void idleAndHistory(const Cd *samples, std::vector<Change>& changes, Begin begin)
    {
        const double *smoothed = m_smoothed.data();
        double *noise = m_noiseFloor.data();
        quint8 *valid = m_noiseValid.data();
        const quint8 *inBurst = m_inBurst.data();
        qint32 *above = m_aboveCount.data();
        Cd *ring = m_preTrigger.data();
        const int length = m_preTriggerLength;
        const size_t write = (size_t) m_preTriggerWrite;

        for (int c = 0; c < m_channels; c++)
        {
            if (!inBurst[c])
            {
                if (!valid[c])
                {
                    noise[c] = smoothed[c];
                    valid[c] = 1;
                }
                else
                {
                    noise[c] += (smoothed[c] - noise[c]) * 0.0001;
                }

                if (smoothed[c] > (noise[c] * m_startThreshold))
                {
                    above[c]++;

                    if (above[c] >= m_startSamples) {
                        startBurst(c, changes, begin);
                    }
                }
                else
                {
                    above[c] = 0;
                }
            }

            ring[((size_t) write * m_channels) + c] = samples[c];
        }

        m_preTriggerWrite++;

        if (m_preTriggerWrite == length) {
            m_preTriggerWrite = 0;
        }

        if (m_preTriggerCount < length) {
            m_preTriggerCount++;
        }
    }

    //! The channels that were already receiving: take this block's sample, then test
    //! whether the burst has ended. Walked backwards so that removing one does not
    //! disturb the ones still to be looked at, and only over the channels that were
    //! receiving before this block - on a real band a handful out of hundreds
    template <typename Append>
    void continueBursts(const Cd *samples, std::vector<Change>& changes, Append append)
    {
        const double *smoothed = m_smoothed.data();
        const double *noise = m_noiseFloor.data();

        for (int i = (int) m_burstChannels.size() - 1; i >= 0; i--)
        {
            const int c = m_burstChannels[i];

            if (append(c, samples[c]))
            {
                // Longer than any burst can be, so something is transmitting continuously
                m_inBurst[c] = 0;
                m_aboveCount[c] = 0;
                m_noiseValid[c] = 0;
                m_burstChannels.erase(m_burstChannels.begin() + i);
                continue;
            }

            if (smoothed[c] < (noise[c] * m_endThreshold))
            {
                m_belowCount[c]++;

                if (m_belowCount[c] >= m_endSamples)
                {
                    m_inBurst[c] = 0;
                    m_aboveCount[c] = 0;
                    m_burstChannels.erase(m_burstChannels.begin() + i);
                    changes.push_back({ c, Ended });
                }
            }
            else
            {
                m_belowCount[c] = 0;
            }
        }
    }

    //! Channels whose start counter has reached the threshold. The counter only advances
    //! for an idle channel, so one that reaches it here cannot already be receiving, and
    //! the end test above has already run without it
    template <typename Begin>
    void startBurst(int c, std::vector<Change>& changes, Begin begin)
    {
        m_inBurst[c] = 1;
        m_belowCount[c] = 0;
        m_burstChannels.push_back(c);
        changes.push_back({ c, Started });

        // The samples from before the detector fired are part of the burst, and the
        // preamble they hold is what the frequency estimate needs. Oldest first, which
        // with a ring is wherever the next write would go
        std::vector<Cd>& out = begin(c, m_sampleIndex - (qint64) m_preTriggerCount);

        for (int i = 0; i < m_preTriggerCount; i++)
        {
            const int slot = (m_preTriggerWrite + m_preTriggerLength - m_preTriggerCount + i)
                           % m_preTriggerLength;

            out.push_back(m_preTrigger[((size_t) slot * m_channels) + c]);
        }
    }

    void removeBurstChannel(int channel)
    {
        for (size_t i = 0; i < m_burstChannels.size(); i++)
        {
            if (m_burstChannels[i] == channel)
            {
                m_burstChannels.erase(m_burstChannels.begin() + i);
                return;
            }
        }
    }

    int m_channels;
    double m_startThreshold;
    double m_endThreshold;
    int m_startSamples;
    int m_endSamples;
    int m_preTriggerLength;

    std::vector<double> m_smoothed;
    std::vector<double> m_noiseFloor;
    std::vector<quint8> m_noiseValid;
    std::vector<quint8> m_inBurst;
    std::vector<qint32> m_aboveCount;
    std::vector<qint32> m_belowCount;
    std::vector<int> m_burstChannels;

    std::vector<Cd> m_preTrigger;
    int m_preTriggerWrite;
    int m_preTriggerCount;
    qint64 m_sampleIndex;
};

#endif // INCLUDE_ACARSIRIDIUMDETECTOR_H
