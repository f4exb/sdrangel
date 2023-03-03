///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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
#include "util/doublebufferfifo.h"
#include "util/messagequeue.h"
#include "util/crc.h"

#include "packetdemodsettings.h"

#include <vector>
#include <iostream>
#include <fstream>

class ChannelAPI;
class PacketDemod;

class PacketDemodSink : public ChannelSampleSink {
public:
    PacketDemodSink(PacketDemod *packetDemod);
    ~PacketDemodSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const PacketDemodSettings& settings, bool force = false);
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
    Complex *m_f1;
    Complex *m_f0;
    Complex *m_corrBuf;
    int m_corrIdx;
    int m_corrCnt;

    Lowpass<Real> m_lowpassF1;
    Lowpass<Real> m_lowpassF0;

    int m_samplePrev;
    int m_syncCount;
    int m_symbolPrev;
    unsigned char m_bits;
    int m_bitCount;
    int m_onesCount;
    bool m_gotSOP;
    unsigned char m_bytes[512]; // Info field can be 256 bytes
    int m_byteCount;
    crc16x25 m_crc;

    QVector<qint16> m_demodBuffer;
    int m_demodBufferFill;

    void processOneSample(Complex &ci);
    MessageQueue *getMessageQueueToChannel() { return m_messageQueueToChannel; }
};

#endif // INCLUDE_PACKETDEMODSINK_H
