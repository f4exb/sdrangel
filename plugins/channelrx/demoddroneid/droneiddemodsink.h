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

#ifndef INCLUDE_DRONEIDDEMODSINK_H
#define INCLUDE_DRONEIDDEMODSINK_H

#include <atomic>

#include "dsp/channelsamplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"

#include "droneiddemodsettings.h"
#include "droneiddetector.h"

class ChannelAPI;
class DroneIDDemod;
class DroneIDDemodWorker;

class DroneIDDemodSink : public ChannelSampleSink {
public:
    DroneIDDemodSink(DroneIDDemod *droneIDDemod);
    ~DroneIDDemodSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const QStringList& settingsKeys, const DroneIDDemodSettings& settings, bool force = false);
    void setWorker(DroneIDDemodWorker *worker) { m_worker = worker; }
    void reset(); //!< Clear detector state, when restarting
    void setChannel(ChannelAPI *channel) { m_channel = channel; }
    void setCenterFrequency(qint64 centerFrequency) { m_centerFrequency = centerFrequency; } //!< Device center frequency

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

    // Number of bursts detected, and dropped because decoder couldn't keep up
    int getBurstsDetected() const { return m_burstsDetected; }
    int getBurstsDropped() const { return m_burstsDropped; }

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

    // Signal occupies +/-4.5MHz. When resampling from a channel rate of less than 2*15.36MS/s, only
    // frequencies above 15.36-4.5MHz alias on to the signal, so a short filter with a wide transition
    // band can be used. Length is scaled with sample rate to keep transition width constant.
    static constexpr Real m_interpolatorCutoff = 5.5e6f;
    static constexpr int m_interpolatorPhaseSteps = 16;
    static constexpr double m_interpolatorTapsPerSampleRate = 24.0 / 20.0e6;   // 24 taps at 20MS/s
    static constexpr double m_interpolatorMinTaps = 8.0;

    DroneIDDemod *m_droneIDDemod;
    DroneIDDemodSettings m_settings;
    ChannelAPI *m_channel;
    DroneIDDemodWorker *m_worker;
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    qint64 m_centerFrequency;

    NCO m_nco;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
    bool m_resample;

    double m_magsq;
    double m_magsqSum;
    double m_magsqPeak;
    int m_magsqCount;
    MagSqLevelsStore m_magSqLevelStore;

    DroneIDDetector m_detector;
    std::atomic<int> m_burstsDetected;
    std::atomic<int> m_burstsDropped;

    void processOneSample(Complex &ci);
    QDateTime burstDateTime();
};

#endif // INCLUDE_DRONEIDDEMODSINK_H
