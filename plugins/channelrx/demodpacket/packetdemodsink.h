///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2021 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020-2021, 2023 Jon Beniston, M7RCE <jon@beniston.com>          //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
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

#ifndef INCLUDE_PACKETDEMODSINK_H
#define INCLUDE_PACKETDEMODSINK_H

#include <QVector>

#include "dsp/channelsamplesink.h"
#include "dsp/phasediscri.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/firfilter.h"
#include "util/movingaverage.h"
#include "util/messagequeue.h"

#include "packetdemodsettings.h"
#include "packetdemodframer.h"
#include "packetdemodtonecorrelator.h"
#include "packetdemodcore.h"

// Post correlation filter length. The envelope being filtered changes at the baud rate, so
// this only has to be long enough to smooth it; 301 taps per tone is 602 multiply
// accumulates per sample and measures no better than 63.
#define PACKETDEMOD_LOWPASS_TAPS 63

#include <vector>
#include <deque>
#include <utility>

// The HDLC state machine, Chase decoding and the frame plausibility check now live in
// PacketDemodFramer, shared verbatim with the offline test harness.
typedef PacketDemodFramer::State PacketDemodDeframer;

class ChannelAPI;
class PacketDemod;


class PacketDemodSink : public ChannelSampleSink {
public:
    PacketDemodSink(PacketDemod *packetDemod);
    ~PacketDemodSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const QStringList& settingsKeys, const PacketDemodSettings& settings, bool force = false);
    void setMessageQueueToChannel(MessageQueue *messageQueue) { m_messageQueueToChannel = messageQueue; }
    void setChannel(ChannelAPI *channel) { m_channel = channel; }

    double getMagSq() const { return m_magsq; }

    void getMagSqLevels(double& avg, double& peak, int& nbSamples)
    {
        if (m_magsqCount > 0)
        {
            m_magsq = m_magsqSum / m_magsqCount;
            m_magSqLevelStore.m_magsq = m_magsq;
            m_magSqLevelStore.m_magsqPeak = m_magsqPeak;
        }

        avg = m_magSqLevelStore.m_magsq;
        peak = m_magSqLevelStore.m_magsqPeak;
        nbSamples = m_magsqCount == 0 ? 1 : m_magsqCount;

        m_magsqSum = 0.0f;
        m_magsqPeak = 0.0f;
        m_magsqCount = 0;
    }

private:
    struct MagSqLevelsStore
    {
        MagSqLevelsStore() :
            m_magsq(1e-12),
            m_magsqPeak(1e-12)
        {}
        double m_magsq;
        double m_magsqPeak;
    };

    PacketDemod *m_packetDemod;
    PacketDemodSettings m_settings;
    ChannelAPI *m_channel;
    int m_channelSampleRate;
    int m_channelFrequencyOffset;

    NCO m_nco;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;

    double m_magsq;
    double m_magsqSum;
    double m_magsqPeak;
    int  m_magsqCount;
    MagSqLevelsStore m_magSqLevelStore;

    MessageQueue *m_messageQueueToChannel;

    MovingAverageUtil<Real, double, 16> m_movingAverage;

    PhaseDiscriminators m_phaseDiscri;
    int m_correlationLength;
    PacketDemodToneCorrelator m_correlator;

    Lowpass<Real> m_lowpassF1;
    Lowpass<Real> m_lowpassF0;

    int m_samplePrev;
    int m_syncCount;
    PacketDemodFramer m_framer;

    // Chase decoding: the slicer discards how confident each symbol decision was, so keep
    // it, and on a CRC failure retry with the weakest decisions inverted
    PacketDemodDeframer m_deframer;

    // The detector, estimator and replay live in PacketDemodCore, shared verbatim with
    // the offline test harness rather than mirrored by it.
    PacketDemodCore m_core;
    std::deque<std::pair<QByteArray, quint64>> m_recent; // Recently reported, to deduplicate

    // Demod analyzer trace - the sink's business, not the detector's
    QVector<qint16> m_demodBuffer;
    int m_demodBufferFill;

    void processOneSample(Complex &ci);


    bool sendPacket(const QByteArray& packet, quint64 stamp);  // false if a duplicate
    MessageQueue *getMessageQueueToChannel() { return m_messageQueueToChannel; }
};

#endif // INCLUDE_PACKETDEMODSINK_H
