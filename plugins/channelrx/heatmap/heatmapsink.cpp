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

#include <complex.h>

#include "dsp/dspengine.h"
#include "dsp/datafifo.h"
#include "dsp/scopevis.h"
#include "util/db.h"
#include "util/stepfunctions.h"
#include "maincore.h"

#include "heatmap.h"
#include "heatmapsink.h"

HeatMapSink::HeatMapSink(HeatMap *heatMap) :
        m_scopeSink(nullptr),
        m_heatMap(heatMap),
        m_channelSampleRate(10000),
        m_channelFrequencyOffset(0),
        m_magsq(0.0),
        m_magsqSum(0.0),
        m_magsqPeak(0.0),
        m_magsqCount(0),
        m_messageQueueToChannel(nullptr),
        m_sampleBufferSize(1000),
        m_sampleBufferIndex(0)
{
    resetMagLevels();
    m_sampleBuffer.resize(m_sampleBufferSize);

    applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

HeatMapSink::~HeatMapSink()
{
}

void HeatMapSink::sampleToScope(Complex sample)
{
    if (m_scopeSink)
    {
        Real r = std::real(sample) * SDR_RX_SCALEF;
        Real i = std::imag(sample) * SDR_RX_SCALEF;
        m_sampleBuffer[m_sampleBufferIndex++] = Sample(r, i);

        if (m_sampleBufferIndex >= m_sampleBufferSize)
        {
            std::vector<SampleVector::const_iterator> vbegin;
            vbegin.push_back(m_sampleBuffer.begin());
            m_scopeSink->feed(vbegin, m_sampleBufferSize);
            m_sampleBufferIndex = 0;
        }
    }
}

void HeatMapSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    QMutexLocker mutexLocker(&m_mutex); // Is this too coarse, depending on size of sample vector?
    Complex ci;

    for (SampleVector::const_iterator it = begin; it != end; ++it)
    {
        Complex c(it->real(), it->imag());
        c *= m_nco.nextIQ();

        if (m_interpolatorDistance < 1.0f) // interpolate
        {
            while (!m_interpolator.interpolate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
        else // decimate
        {
            if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
    }
}

void HeatMapSink::processOneSample(Complex &ci)
{
    Real re = ci.real() / SDR_RX_SCALEF;
    Real im = ci.imag() / SDR_RX_SCALEF;
    Real magsq = re*re + im*im;
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();
    m_magsqSum += magsq;
    if (magsq > m_magsqPeak) {
        m_magsqPeak = magsq;
    }
    m_magsqCount++;

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

    // Sample to feed to scope (so we can see power at channel sample rate)
    Complex scopeSample;
    scopeSample.real(ci.real() / SDR_RX_SCALEF);
    scopeSample.imag(ci.imag() / SDR_RX_SCALEF);
    sampleToScope(scopeSample);
}

void HeatMapSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "HeatMapSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((m_channelFrequencyOffset != channelFrequencyOffset) ||
        (m_channelSampleRate != channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((m_channelSampleRate != channelSampleRate) || force)
    {
        m_interpolator.create(16, channelSampleRate, m_settings.m_rfBandwidth / 2.2);
        m_interpolatorDistance = (Real) channelSampleRate / (Real) m_settings.m_sampleRate;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void HeatMapSink::applySettings(const HeatMapSettings& settings, bool force)
{
    qDebug() << "HeatMapSink::applySettings:"
            << " sampleRate: " << settings.m_sampleRate
            << " averagePeriodUS: " << settings.m_averagePeriodUS
            << " pulseThreshold: " << settings.m_pulseThreshold
            << " force: " << force;

    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth)
        || (settings.m_averagePeriodUS != m_settings.m_averagePeriodUS)
        || (settings.m_sampleRate != m_settings.m_sampleRate)
        || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_rfBandwidth / 2.2);
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) settings.m_sampleRate;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    if ((settings.m_averagePeriodUS != m_settings.m_averagePeriodUS)
        || (settings.m_sampleRate != m_settings.m_sampleRate)
        || force)
    {
        m_averageCnt = (int)((settings.m_averagePeriodUS * settings.m_sampleRate / 1e6));
        // For low sample rates, we want a small buffer, so scope update isn't too slow
        if (settings.m_sampleRate < 100) {
            m_sampleBufferSize = 1;
        } else if (settings.m_sampleRate <= 100) {
            m_sampleBufferSize = 10;
        } else if (settings.m_sampleRate <= 10000) {
            m_sampleBufferSize = 100;
        } else {
            m_sampleBufferSize = 1000;
        }
        qDebug() << "m_averageCnt" << m_averageCnt;
        qDebug() << "m_sampleBufferSize" << m_sampleBufferSize;
        m_sampleBuffer.resize(m_sampleBufferSize);
        if (m_sampleBufferIndex >= m_sampleBufferSize) {
            m_sampleBufferIndex = 0;
        }
    }

    if ((settings.m_pulseThreshold != m_settings.m_pulseThreshold) || force)
    {
        m_pulseThresholdLinear = std::pow(10.0, settings.m_pulseThreshold / 20.0);
        qDebug() << "m_pulseThresholdLinear" << m_pulseThresholdLinear;
    }

    m_settings = settings;
}

