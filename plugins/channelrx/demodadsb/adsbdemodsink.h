///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_ADSBDEMODSINK_H
#define INCLUDE_ADSBDEMODSINK_H

#define BOOST_CHRONO_HEADER_ONLY
#include <boost/chrono/chrono.hpp>

#include "dsp/channelsamplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "util/movingaverage.h"

#include "adsbdemodsettings.h"
#include "adsbdemodstats.h"
#include "adsbdemodsinkworker.h"

class ADSBDemodSink : public ChannelSampleSink {
public:
    ADSBDemodSink();
    ~ADSBDemodSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

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

    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const ADSBDemodSettings& settings, bool force = false);
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_messageQueueToGUI = messageQueue; }
    void setMessageQueueToWorker(MessageQueue *messageQueue) { m_messageQueueToWorker = messageQueue; }
    void startWorker();
    void stopWorker();

private:
    friend ADSBDemodSinkWorker;

    struct MagSqLevelsStore
    {
        MagSqLevelsStore() :
            m_magsq(1e-12),
            m_magsqPeak(1e-12)
        {}
        double m_magsq;
        double m_magsqPeak;
    };

    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    ADSBDemodSettings m_settings;

    NCO m_nco;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;

    boost::chrono::steady_clock::time_point m_startPoint;
    double m_feedTime;                  //!< Time spent in feed()

    // Triple buffering for sharing sample data between two threads
    // Top area of each buffer is not used by writer, as it's used by the reader
    // for copying the last few samples of the previous buffer, so it can
    // be processed contiguously
    const int m_buffers = 3;
    const int m_bufferSize = 200000;
    Real *m_sampleBuffer[3];            //!< Each buffer is m_bufferSize samples
    QSemaphore m_bufferWrite[3];        //!< Sempahore to control write access to the buffers
    QSemaphore m_bufferRead[3];         //!< Sempahore to control read access from the buffers
    QDateTime m_bufferFirstSampleDateTime[3];  //!< Time for first sample in the buffer
    bool m_bufferDateTimeValid[3];
    ADSBDemodSinkWorker m_worker;       //!< Worker thread that does the actual demodulation
    int m_writeBuffer;                  //!< Which of the 3 buffers we're writing in to
    int m_writeIdx;                     //!< Index to to current write buffer

    // These values are derived from samplesPerBit
    int m_samplesPerFrame;              //!< Including preamble
    int m_samplesPerChip;

    double m_magsq; //!< displayed averaged value
    double m_magsqSum;
    double m_magsqPeak;
    int  m_magsqCount;
    MagSqLevelsStore m_magSqLevelStore;

    MessageQueue *m_messageQueueToGUI;
    MessageQueue *m_messageQueueToWorker;

    void init(int samplesPerBit);
    Real inline complexMagSq(Complex& ci)
    {
        double magsqRaw = ci.real()*ci.real() + ci.imag()*ci.imag();
        return (Real)(magsqRaw / (SDR_RX_SCALED*SDR_RX_SCALED));
    }
    void processOneSample(Real magsq);
    MessageQueue *getMessageQueueToGUI() { return m_messageQueueToGUI; }
    MessageQueue *getMessageQueueToWorker() { return m_messageQueueToWorker; }
};

#endif // INCLUDE_ADSBDEMODSINK_H
