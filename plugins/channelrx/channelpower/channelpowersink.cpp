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

#include <QDebug>

#include "channelpowersink.h"

ChannelPowerSink::ChannelPowerSink() :
        m_channelSampleRate(10000),
        m_channelFrequencyOffset(0),
        m_lowpassFFT(nullptr),
        m_lowpassBufferIdx(0)
{
    resetMagLevels();

    applySettings(m_settings, QStringList(), true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

ChannelPowerSink::~ChannelPowerSink()
{
    delete m_lowpassFFT;
}

void ChannelPowerSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    QMutexLocker mutexLocker(&m_mutex);

    for (SampleVector::const_iterator it = begin; it != end; ++it)
    {
        Complex c(it->real(), it->imag());
        c *= m_nco.nextIQ();

        processOneSample(c);
    }
}

void ChannelPowerSink::processOneSample(Complex &ci)
{
    // Low pass filter to desired channel bandwidth
    fftfilt::cmplx *filtered;
    int nOut = m_lowpassFFT->runFilt(ci, &filtered);
    if (nOut > 0)
    {
        memcpy(m_lowpassBuffer, filtered, nOut * sizeof(Complex));
        m_lowpassBufferIdx = 0;
    }
    Complex c = m_lowpassBuffer[m_lowpassBufferIdx++];

    // Calculate power
    Real re = c.real() / SDR_RX_SCALEF;
    Real im = c.imag() / SDR_RX_SCALEF;
    Real magsq = re*re + im*im;

    // Although computationally more expensive to take the square root here,
    // it possibly reduces problems of accumulating numbers
    // that may differ significantly in magnitude, for long averages
    double mag = sqrt((double)magsq);

    m_magSum += mag;
    if (mag > m_pulseThresholdLinear)
    {
        m_magPulseSum += mag;
        m_magPulseCount++;
        if (m_magPulseCount >= m_averageCnt)
        {
            m_magPulseAvg = m_magPulseSum / m_magPulseCount;
            m_magPulseSum = 0.0;
            m_magPulseCount = 0;
        }
    }

    if (mag > m_magMaxPeak) {
        m_magMaxPeak = mag;
    }
    if (mag < m_magMinPeak) {
        m_magMinPeak = mag;
    }

    m_magCount++;
    if (m_magCount >= m_averageCnt)
    {
        m_magAvg = m_magSum / m_magCount;
        m_magSum = 0.0;
        m_magCount = 0;
    }
}

void ChannelPowerSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "ChannelPowerSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((m_channelFrequencyOffset != channelFrequencyOffset) ||
        (m_channelSampleRate != channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((m_channelSampleRate != channelSampleRate) || force)
    {
        delete m_lowpassFFT;
        m_lowpassFFT = new fftfilt(0, m_settings.m_rfBandwidth / 2.0f / m_channelSampleRate, m_lowpassLen);
        m_lowpassBufferIdx = 0;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;

    m_averageCnt = (int)((m_settings.m_averagePeriodUS * (qint64)m_channelSampleRate / 1e6));
}

void ChannelPowerSink::applySettings(const ChannelPowerSettings& settings, const QStringList& settingsKeys, bool force)
{
    qDebug() << "ChannelPowerSink::applySettings:" << " force: " << force << settings.getDebugString(settingsKeys, force);

    if ((settingsKeys.contains("rfBandwidth") && (settings.m_rfBandwidth != m_settings.m_rfBandwidth)) || force)
    {
        delete m_lowpassFFT;
        m_lowpassFFT = new fftfilt(0, settings.m_rfBandwidth / 2.0f / m_channelSampleRate, m_lowpassLen);
        m_lowpassBufferIdx = 0;
    }

    if (settingsKeys.contains("averagePeriodUS") || force) {
        m_averageCnt = (int)((settings.m_averagePeriodUS * (qint64)m_channelSampleRate / 1e6));
    }

    if (settingsKeys.contains("pulseThreshold") || force) {
        m_pulseThresholdLinear = std::pow(10.0, settings.m_pulseThreshold / 20.0);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}
