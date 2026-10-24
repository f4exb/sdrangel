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

#ifndef INCLUDE_ACARSIRIDIUMSCANNER_H
#define INCLUDE_ACARSIRIDIUMSCANNER_H

#include <QByteArray>
#include <QList>

#include <cmath>
#include <cstdlib>
#include <complex>
#include <deque>
#include <vector>

#include "acarsiridium.h"
#include "acarsiridiumframe.h"
#include "acarsiridiumchannelizer.h"

// Watches the whole Iridium band at once.
//
// Iridium assigns a duplex channel when a call is set up, out of about 240 spaced
// 41.67 kHz across roughly 1619 to 1626 MHz, so a receiver watching one channel sees a
// few percent of what is on the air - measured on a four minute recording of the whole
// band, the busiest single channel carried 4% of the short burst data frames in it. This
// splits the band with the polyphase channelizer and runs a burst receiver on every
// output, which is what makes scanning practical without the user adding a demodulator
// per channel.
//
// The channelizer spacing is 31.25 kHz rather than the 41.67 kHz of the Iridium grid.
// Lining the two up would need a channel count divisible by three, and a transform of
// that size is far more work than a power of two; nothing needs them aligned, because a
// burst is found by searching its channel rather than by being assumed to sit in the
// middle of one. The cost is that neighbouring channels overlap, so a burst is often
// found twice, and the duplicates have to go - a frame decoded twice would otherwise
// break the short burst data reassembly, whose fragments have to arrive in counter order.

//! The channelizer spacing, which with four times oversampling gives the 125 kHz the
//! receiver runs at. The input rate is this times the number of channels
#define ACARSIRIDIUM_SCAN_SPACING           31250

class AcarsIridiumScanner
{
public:
    typedef std::complex<double> Cd;

    struct Detection
    {
        AcarsIridiumFrame m_frame;
        double m_frequency = 0.0;       //!< Hz from the centre of the band
        double m_snrDb = 0.0;
        qint64 m_sampleIndex = 0;       //!< Where the burst started, in input samples
        int m_channel = 0;
        //!< A short burst data message, once a run of frames has been put back together.
        //!< Reported whether or not it turns out to hold ACARS, because the step that
        //!< reads an ACARS block out of one is the only part of the chain never tested
        //!< against real traffic - if it ever wrongly refuses a message, that has to be
        //!< visible rather than silently dropped
        QByteArray m_message;
        QByteArray m_acars;             //!< A complete ACARS block, when one came out
    };

    AcarsIridiumScanner() :
        m_channels(0),
        m_decimation(0),
        m_inputRate(0.0),
        m_holdSamples(0),
        m_sameTimeSamples(0),
        m_sampleIndex(0)
    {
    }

    //! channels is the number of channelizer outputs, which fixes the input rate at
    //! channels * ACARSIRIDIUM_SCAN_SPACING. It must be a power of two
    void configure(int channels)
    {
        m_channels = channels;
        m_inputRate = (double) channels * ACARSIRIDIUM_SCAN_SPACING;
        m_holdSamples = (qint64) (HoldSeconds * m_inputRate);
        m_sameTimeSamples = (qint64) (SameTimeSeconds * m_inputRate);
        m_channelizer.configure(channels);
        m_reassembler.setBandWidth(m_inputRate);
        m_decimation = m_channelizer.decimation();
        m_receivers.clear();
        m_receivers.resize(channels);
        m_pending.clear();
        m_detections.clear();
        m_sampleIndex = 0;
        m_burstCount = 0;
        m_duplicateCount = 0;
        m_frameCount = 0;
        m_idaCount = 0;
        m_messageCount = 0;
        m_acarsCount = 0;
    }

    int channels() const { return m_channels; }
    double inputRate() const { return m_inputRate; }

    //! Feeds one input sample at inputRate
    void processSample(const Cd& sample)
    {
        m_sampleIndex++;

        if (!m_channelizer.processSample(sample)) {
            return;
        }

        const std::vector<Cd>& channelSamples = m_channelizer.channelSamples();

        for (int c = 0; c < m_channels; c++)
        {
            if (!m_receivers[c].processSample(channelSamples[c])) {
                continue;
            }

            while (m_receivers[c].hasBurst())
            {
                AcarsIridiumReceiver::Burst burst = m_receivers[c].popBurst();

                if (burst.m_direction == AcarsIridiumReceiver::DirectionUnknown) {
                    continue;
                }

                Candidate candidate;
                candidate.m_burst = burst;
                candidate.m_channel = c;
                candidate.m_frequency = m_channelizer.channelOffset(c, m_inputRate)
                                      + burst.m_frequencyOffset;
                // The receiver counts in its own channel samples
                candidate.m_sampleIndex = burst.m_sampleIndex * m_decimation;
                candidate.m_detectedAt = m_sampleIndex;
                m_pending.push_back(candidate);
                m_burstCount++;
            }
        }

        flush(false);
    }

    //! Call when the input ends, so anything still held for duplicate checking is emitted
    void finish()
    {
        flush(true);
    }

    bool hasDetection() const { return !m_detections.empty(); }

    Detection popDetection()
    {
        Detection detection = m_detections.front();
        m_detections.pop_front();

        return detection;
    }

    int burstCount() const { return m_burstCount; }
    int duplicateCount() const { return m_duplicateCount; }
    int frameCount() const { return m_frameCount; }
    int idaCount() const { return m_idaCount; }
    int messageCount() const { return m_messageCount; }
    int acarsCount() const { return m_acarsCount; }

private:

    struct Candidate
    {
        AcarsIridiumReceiver::Burst m_burst;
        int m_channel = 0;
        double m_frequency = 0.0;
        qint64 m_sampleIndex = 0;       //!< Where the burst started
        qint64 m_detectedAt = 0;        //!< When it finished and was handed over
    };

    // How long a burst is held before being decoded, so that the same burst found in a
    // neighbouring channel has arrived too and the better of the two can be taken.
    // Measured from when the burst was handed over, not from when it started: a burst
    // runs to 20 ms, so by the time it ends its start is long past and holding against
    // that would expire immediately and let every duplicate through.
    //
    // A duration rather than a count of input samples, because the input rate spans eight
    // to one across the bandwidth settings: as a fixed count this was 8 ms at 32 channels
    // but 1 ms at 256, which is the wrong way round - the widest setting has the most
    // overlapping neighbours and so needs the hold most
    static constexpr double HoldSeconds = 0.008;

    // Two detections are the same burst when they start at about the same time and are at
    // about the same frequency. Bursts on one channel are a time slot apart, roughly
    // 9 ms, so this cannot merge two genuine transmissions; the channel spacing is
    // 31.25 kHz, so the frequency tolerance is well inside it
    static constexpr double SameTimeSeconds = 0.004;
    static constexpr double SameFrequencyHz = 12000.0;

    void flush(bool all)
    {
        while (!m_pending.empty())
        {
            if (!all && ((m_sampleIndex - m_pending.front().m_detectedAt) < m_holdSamples)) {
                break;
            }

            // Take the oldest, then find every other pending detection that is the same
            // burst heard in an overlapping channel, and keep whichever has the best
            // signal to noise - that is the one whose channel the burst sat nearest the
            // middle of
            Candidate best = m_pending.front();
            m_pending.pop_front();

            for (size_t i = 0; i < m_pending.size(); )
            {
                const Candidate& other = m_pending[i];

                // The band is circular, so the channels either side of the fold at its
                // edge are neighbours and a burst in both has to be recognised as one
                if ((std::llabs(other.m_sampleIndex - best.m_sampleIndex) < m_sameTimeSamples)
                 && (AcarsIridiumReassembler::separation(other.m_frequency, best.m_frequency, m_inputRate) < SameFrequencyHz))
                {
                    if (other.m_burst.m_snrDb > best.m_burst.m_snrDb) {
                        best = other;
                    }

                    m_pending.erase(m_pending.begin() + i);
                    m_duplicateCount++;
                }
                else
                {
                    i++;
                }
            }

            decode(best);
        }
    }

    void decode(const Candidate& candidate)
    {
        Detection detection;

        if (!m_decoder.decode(candidate.m_burst.m_frameBits, detection.m_frame)) {
            return;
        }

        detection.m_frequency = candidate.m_frequency;
        detection.m_snrDb = candidate.m_burst.m_snrDb;
        detection.m_sampleIndex = candidate.m_sampleIndex;
        detection.m_channel = candidate.m_channel;
        m_frameCount++;

        if (detection.m_frame.m_type == AcarsIridiumFrame::TypeIDA)
        {
            m_idaCount++;

            const qint64 timeMs = (qint64) ((1000.0 * (double) candidate.m_sampleIndex) / m_inputRate);
            QByteArray message;

            // Short burst data is reassembled per transmitter, and the reassembler tells
            // them apart by frequency, so it is given the absolute frequency in the band
            // rather than the offset within a channel
            if (m_reassembler.addFrame(detection.m_frame, timeMs, candidate.m_frequency, message))
            {
                QByteArray block;

                detection.m_message = message;
                m_messageCount++;

                if (AcarsIridiumReassembler::acarsBlock(message, detection.m_frame.m_uplink, block))
                {
                    detection.m_acars = block;
                    m_acarsCount++;
                }
            }
        }

        m_detections.push_back(detection);
    }

    int m_channels;
    int m_decimation;
    double m_inputRate;
    qint64 m_holdSamples;
    qint64 m_sameTimeSamples;
    qint64 m_sampleIndex;

    AcarsIridiumChannelizer m_channelizer;
    std::vector<AcarsIridiumReceiver> m_receivers;
    AcarsIridiumFrameDecoder m_decoder;
    AcarsIridiumReassembler m_reassembler;

    std::deque<Candidate> m_pending;
    std::deque<Detection> m_detections;

    int m_burstCount = 0;
    int m_duplicateCount = 0;
    int m_frameCount = 0;
    int m_idaCount = 0;
    int m_messageCount = 0;
    int m_acarsCount = 0;
};

#endif // INCLUDE_ACARSIRIDIUMSCANNER_H
