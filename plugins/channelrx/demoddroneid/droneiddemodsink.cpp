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

#include <algorithm>

#include <QDebug>

#include "device/deviceapi.h"
#include "channel/channelwebapiutils.h"

#include "droneiddemod.h"
#include "droneiddemodsink.h"
#include "droneiddemodworker.h"

DroneIDDemodSink::DroneIDDemodSink(DroneIDDemod *droneIDDemod) :
    m_droneIDDemod(droneIDDemod),
    m_channel(nullptr),
    m_worker(nullptr),
    m_channelSampleRate(DroneIDDemodSettings::DRONEIDDEMOD_CHANNEL_SAMPLE_RATE),
    m_channelFrequencyOffset(0),
    m_centerFrequency(0),
    m_interpolatorDistance(1.0f),
    m_interpolatorDistanceRemain(1.0f),
    m_resample(false),
    m_magsq(0.0),
    m_magsqSum(0.0),
    m_magsqPeak(0.0),
    m_magsqCount(0),
    m_burstsDetected(0),
    m_burstsDropped(0)
{
    applySettings(QStringList(), m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

DroneIDDemodSink::~DroneIDDemodSink()
{
}

void DroneIDDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    Complex ci;
    const bool shift = m_channelFrequencyOffset != 0;
    const Real scale = 1.0f / SDR_RX_SCALEF;

    for (SampleVector::const_iterator it = begin; it != end; ++it)
    {
        Complex c(it->real() * scale, it->imag() * scale);

        if (shift) {
            c *= m_nco.nextIQ();
        }

        if (!m_resample)
        {
            processOneSample(c);
        }
        else if (m_interpolatorDistance < 1.0f) // interpolate
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

void DroneIDDemodSink::processOneSample(Complex &ci)
{
    // Calculate average and peak levels for level meter
    Real magsq = std::norm(ci);
    m_magsqSum += magsq;
    if (magsq > m_magsqPeak) {
        m_magsqPeak = magsq;
    }
    m_magsqCount++;

    if (m_detector.process(ci))
    {
        m_burstsDetected++;

        if (m_worker)
        {
            // Count rather than log when dropped, as this can happen at a high rate
            if (m_worker->isQueueFull()) {
                m_burstsDropped++;
            } else {
                // Record frequency now, as device may be retuned (E.g. by scanning) before burst is decoded
                m_worker->queueBurst(m_detector.getBurst(), burstDateTime(), m_centerFrequency + m_settings.m_inputFrequencyOffset);
            }
        }
    }
}

QDateTime DroneIDDemodSink::burstDateTime()
{
    QDateTime dateTime = QDateTime::currentDateTime();

    if (m_settings.m_useFileTime)
    {
        QString hardwareId = m_droneIDDemod->getDeviceAPI()->getHardwareId();

        if ((hardwareId == "FileInput") || (hardwareId == "SigMFFileInput"))
        {
            QString dateTimeStr;
            int deviceIdx = m_droneIDDemod->getDeviceSetIndex();

            if (ChannelWebAPIUtils::getDeviceReportValue(deviceIdx, "absoluteTime", dateTimeStr)) {
                dateTime = QDateTime::fromString(dateTimeStr, Qt::ISODateWithMs);
            }
        }
    }

    return dateTime;
}

void DroneIDDemodSink::reset()
{
    m_detector.reset();
    m_interpolatorDistanceRemain = m_interpolatorDistance;
}

void DroneIDDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "DroneIDDemodSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((m_channelFrequencyOffset != channelFrequencyOffset) ||
        (m_channelSampleRate != channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((m_channelSampleRate != channelSampleRate) || force)
    {
        // Sample rate is 0 until device sample rate is known
        m_resample = (channelSampleRate > 0) && (channelSampleRate != DroneIDDemodSettings::DRONEIDDEMOD_CHANNEL_SAMPLE_RATE);

        if (m_resample)
        {
            double taps = std::max(m_interpolatorMinTaps, m_interpolatorTapsPerSampleRate * channelSampleRate);
            m_interpolator.create(m_interpolatorPhaseSteps, channelSampleRate, m_interpolatorCutoff, taps / m_interpolatorPhaseSteps);
            m_interpolatorDistance = (Real) channelSampleRate / (Real) DroneIDDemodSettings::DRONEIDDEMOD_CHANNEL_SAMPLE_RATE;
            m_interpolatorDistanceRemain = m_interpolatorDistance;
        }

        if ((channelSampleRate > 0) && (channelSampleRate < DroneIDDemodSettings::DRONEIDDEMOD_CHANNEL_SAMPLE_RATE)) {
            qWarning() << "DroneIDDemodSink::applyChannelSettings: Channel sample rate" << channelSampleRate << "is too low to receive DroneID";
        }

        m_detector.reset();
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void DroneIDDemodSink::applySettings(const QStringList& settingsKeys, const DroneIDDemodSettings& settings, bool force)
{
    qDebug() << "DroneIDDemodSink::applySettings:"
            << settings.getDebugString(settingsKeys, force)
            << " force: " << force;

    if (settingsKeys.contains("detectionThreshold") || force) {
        m_detector.setThreshold(settings.m_detectionThreshold);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}
