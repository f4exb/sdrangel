///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_CHANNELPOWERSINK_H
#define INCLUDE_CHANNELPOWERSINK_H

#include <QMutex>

#include "dsp/channelsamplesink.h"
#include "dsp/nco.h"
#include "dsp/fftfilt.h"

#include "channelpowersettings.h"

class ChannelAPI;
class ChannelPower;

class ChannelPowerSink : public ChannelSampleSink {
public:
    ChannelPowerSink();
    ~ChannelPowerSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const ChannelPowerSettings& settings, const QStringList& settingsKeys, bool force = false);
    void setChannel(ChannelAPI *channel) { m_channel = channel; }

    void getMagLevels(double& avg, double& pulseAvg, double &maxPeak, double &minPeak)
    {
        QMutexLocker mutexLocker(&m_mutex);
        avg = m_magAvg;
        pulseAvg = m_magPulseAvg;
        maxPeak = m_magMaxPeak;
        minPeak = m_magMinPeak;
    }

    void resetMagLevels()
    {
        QMutexLocker mutexLocker(&m_mutex);
        m_magSum = 0.0;
        m_magCount = 0;
        m_magAvg = std::numeric_limits<double>::quiet_NaN();
        m_magPulseSum = 0.0;
        m_magPulseCount = 0;
        m_magPulseAvg = std::numeric_limits<double>::quiet_NaN();
        m_magMinPeak = std::numeric_limits<double>::max();
        m_magMaxPeak = -std::numeric_limits<double>::max();
    }

private:

    ChannelPowerSettings m_settings;
    ChannelAPI *m_channel;
    int m_channelSampleRate;
    int m_channelFrequencyOffset;

    NCO m_nco;

    fftfilt *m_lowpassFFT;
    static constexpr int m_lowpassLen = 2048;
    Complex m_lowpassBuffer[m_lowpassLen];
    int m_lowpassBufferIdx;

    double m_magSum;
    double m_magCount;
    double m_magAvg;
    double m_magPulseSum;
    double m_magPulseCount;
    double m_magPulseAvg;
    double m_magMaxPeak;
    double m_magMinPeak;

    int m_averageCnt;
    double m_pulseThresholdLinear;

    QMutex m_mutex;

    void processOneSample(Complex &ci);
};

#endif // INCLUDE_CHANNELPOWERSINK_H

