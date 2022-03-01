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

#ifndef INCLUDE_RADIOSONDEDEMODSINK_H
#define INCLUDE_RADIOSONDEDEMODSINK_H

#include <QVector>

#include "dsp/channelsamplesink.h"
#include "dsp/phasediscri.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/firfilter.h"
#include "dsp/gaussian.h"
#include "util/movingaverage.h"
#include "util/doublebufferfifo.h"
#include "util/messagequeue.h"
#include "util/crc.h"

#include "radiosondedemodsettings.h"

// Length of preamble (40 bytes) and frame (std 320 bytes - extended 518)
#define RADIOSONDEDEMOD_MAX_BYTES  (40+518)

class ChannelAPI;
class RadiosondeDemod;
class ScopeVis;

class RadiosondeDemodSink : public ChannelSampleSink {
public:
    RadiosondeDemodSink(RadiosondeDemod *radiosondeDemod);
    ~RadiosondeDemodSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void setScopeSink(ScopeVis* scopeSink) { m_scopeSink = scopeSink; }
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const RadiosondeDemodSettings& settings, bool force = false);
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

    ScopeVis* m_scopeSink;    // Scope GUI to display baseband waveform
    RadiosondeDemod *m_radiosondeDemod;
    RadiosondeDemodSettings m_settings;
    ChannelAPI *m_channel;
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    int m_samplesPerSymbol;             // Number of samples per symbol

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

    Lowpass<Complex> m_lowpass;         // RF input filter
    PhaseDiscriminators m_phaseDiscri;  // FM demodulator
    Gaussian<Real> m_pulseShape;        // Pulse shaping filter
    Real *m_rxBuf;                      // Receive sample buffer, large enough for one max length messsage
    int m_rxBufLength;                  // Size in elements in m_rxBuf
    int m_rxBufIdx;                     // Index in to circular buffer
    int m_rxBufCnt;                     // Number of valid samples in buffer
    Real *m_train;                      // Training sequence to look for
    int m_correlationLength;

    unsigned char m_bytes[RADIOSONDEDEMOD_MAX_BYTES];
    crc16ccitt m_crc;

    QVector<qint16> m_demodBuffer;
    int m_demodBufferFill;

    SampleVector m_sampleBuffer;
    static const int m_sampleBufferSize = RadiosondeDemodSettings::RADIOSONDEDEMOD_CHANNEL_SAMPLE_RATE / 20;
    int m_sampleBufferIndex;

    static const uint8_t m_descramble[64];

    MessageQueue *getMessageQueueToChannel() { return m_messageQueueToChannel; }
    void processOneSample(Complex &ci);
    Real correlate(int idx) const;
    bool processFrame(int length, float corr, int sampleIdx);
    int reedSolomonErrorCorrection();
    bool checkCRCs(int length);
    void sampleToScope(Complex sample);
};

#endif // INCLUDE_RADIOSONDEDEMODSINK_H
