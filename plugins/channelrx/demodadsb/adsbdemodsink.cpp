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

#include <QTime>
#include <QDebug>

#include "util/profiler.h"

#include "adsbdemodsink.h"
#include "adsbdemodsinkworker.h"
#include "adsbdemod.h"
#include "adsb.h"

ADSBDemodSink::ADSBDemodSink() :
    m_channelSampleRate(6000000),
    m_channelFrequencyOffset(0),
    m_sampleBuffer{nullptr, nullptr, nullptr},
    m_worker(this),
    m_writeBuffer(0),
    m_writeIdx(0),
    m_magsq(0.0f),
    m_magsqSum(0.0f),
    m_magsqPeak(0.0f),
    m_magsqCount(0),
    m_messageQueueToGUI(nullptr)
{
    applySettings(m_settings, QStringList(), true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
    for (int i = 0; i < m_buffers; i++)
        m_bufferWrite[i].release(1);
    m_bufferWrite[m_writeBuffer].acquire();
}

ADSBDemodSink::~ADSBDemodSink()
{
    for (int i = 0; i < m_buffers; i++)
        delete[] m_sampleBuffer[i];
}

void ADSBDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    // Start timing how long we are in this function
    PROFILER_START();

    // Optimise for common case, where no resampling or frequency offset
    if ((m_interpolatorDistance == 1.0f) && (m_channelFrequencyOffset == 0))
    {
        for (SampleVector::const_iterator it = begin; it != end; ++it)
        {
            /*
            // SampleVector is vector of qint32 or qint16
            // Use integer mul to save one FP conversion and it has lower latency
            qint64 r = (qint64)it->real();
            qint64 i = (qint64)it->imag();
            qint64 magsqRaw = r*r + i*i;
            Real magsq = (Real)((double)magsqRaw / (SDR_RX_SCALED*SDR_RX_SCALED));
            processOneSample(magsq);
            */
            Complex c(it->real(), it->imag());
            Real magsq = complexMagSq(c);
            processOneSample(magsq);
        }
    }
    else if (m_interpolatorDistance == 1.0f) // just apply offset
    {
        for (SampleVector::const_iterator it = begin; it != end; ++it)
        {
            Complex c(it->real(), it->imag());
            c *= m_nco.nextIQ();
            processOneSample(complexMagSq(c));
        }
    }
    else if (m_interpolatorDistance < 1.0f) // interpolate
    {
        for (SampleVector::const_iterator it = begin; it != end; ++it)
        {
            Complex c(it->real(), it->imag());
            Complex ci;
            c *= m_nco.nextIQ();
            while (!m_interpolator.interpolate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(complexMagSq(ci));
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
    }
    else // decimate
    {
        for (SampleVector::const_iterator it = begin; it != end; ++it)
        {
            Complex c(it->real(), it->imag());
            Complex ci;
            c *= m_nco.nextIQ();
            if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(complexMagSq(ci));
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
    }

    // Calculate number of seconds in this function
    PROFILER_STOP("ADSB feed");
}

void ADSBDemodSink::processOneSample(Real magsq)
{
    m_magsqSum += magsq;
    if (magsq > m_magsqPeak)
        m_magsqPeak = magsq;
    m_magsqCount++;
    m_sampleBuffer[m_writeBuffer][m_writeIdx] = magsq;
    m_writeIdx++;
    if (!m_bufferDateTimeValid[m_writeBuffer])
    {
        QDateTime dateTime = QDateTime::currentDateTime();
        if (m_minFirstSampleDateTime.isValid() && (dateTime < m_minFirstSampleDateTime)) {
            dateTime = m_minFirstSampleDateTime;
        }
        m_bufferFirstSampleDateTime[m_writeBuffer] = dateTime;
        m_bufferDateTimeValid[m_writeBuffer] = true;

        // Make sure timestamps from different buffers are in order, even if we receive samples faster than real time
        const qint64 samplesPerSecondMSec = ADS_B_BITS_PER_SECOND * m_settings.m_samplesPerBit / 1000;
        const qint64 offsetMSec = m_bufferSize / samplesPerSecondMSec;
        m_minFirstSampleDateTime = dateTime.addMSecs(offsetMSec);
    }
    if (m_writeIdx >= m_bufferSize)
    {
        m_bufferRead[m_writeBuffer].release();

        m_writeBuffer++;
        if (m_writeBuffer >= m_buffers)
            m_writeBuffer = 0;

        if (m_worker.isRunning())
            m_bufferWrite[m_writeBuffer].acquire();

        m_writeIdx = m_samplesPerFrame - 1; // Leave space for copying samples from previous buffer

        m_bufferDateTimeValid[m_writeBuffer] = false;
    }
}

void ADSBDemodSink::startWorker()
{
    qDebug() << "ADSBDemodSink::startWorker";
    if (!m_worker.isRunning())
        m_worker.start();
}

void ADSBDemodSink::stopWorker()
{
    if (m_worker.isRunning())
    {
        qDebug() << "ADSBDemodSink::stopWorker: Stopping worker";
        m_worker.requestInterruption();
        // Worker may be blocked waiting for a buffer
        for (int i = 0; i < m_buffers; i++)
        {
            if (m_bufferRead[i].available() == 0)
                m_bufferRead[i].release(1);
        }
        m_worker.wait();
        // If this is called from ADSBDemod, we need to also
        // make sure baseband sink thread isn't blocked in processOneSample
        for (int i = 0; i < m_buffers; i++)
        {
            if (m_bufferWrite[i].available() == 0)
                m_bufferWrite[i].release(1);
        }
        qDebug() << "ADSBDemodSink::stopWorker: Worker stopped";
    }
}

void ADSBDemodSink::init(int samplesPerBit)
{
    bool restart = m_worker.isRunning();
    if (restart)
    {
        // Stop worker as we're going to delete the buffers
        stopWorker();
    }
    // Reset state of semaphores
    for (int i = 0; i < m_buffers; i++)
    {
         m_bufferWrite[i].acquire(m_bufferWrite[i].available());
         m_bufferWrite[i].release(1);
         m_bufferRead[i].acquire(m_bufferRead[i].available());
    }
    m_writeBuffer = 0;
    m_bufferWrite[m_writeBuffer].acquire();

    for (int i = 0; i < m_buffers; i++)
    {
        if (m_sampleBuffer[i])
            delete[] m_sampleBuffer[i];
    }

    m_samplesPerFrame = samplesPerBit*(ADS_B_PREAMBLE_BITS+ADS_B_ES_BITS);
    m_samplesPerChip = samplesPerBit/ADS_B_CHIPS_PER_BIT;
    m_writeIdx = m_samplesPerFrame - 1; // Leave space for copying samples from previous buffer
    m_bufferDateTimeValid[m_writeBuffer] = false;

    for (int i = 0; i < m_buffers; i++)
        m_sampleBuffer[i] = new Real[m_bufferSize];

    if (restart)
        startWorker();
}

void ADSBDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "ADSBDemodSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if (channelSampleRate == 0) {
        return;
    }

    if ((channelFrequencyOffset != m_channelFrequencyOffset) ||
        (channelSampleRate != m_channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((channelSampleRate != m_channelSampleRate) || force)
    {
        m_interpolator.create(m_settings.m_interpolatorPhaseSteps, channelSampleRate, m_settings.m_rfBandwidth / 2.2,  m_settings.m_interpolatorTapsPerPhase);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) channelSampleRate / (Real) (ADS_B_BITS_PER_SECOND * m_settings.m_samplesPerBit);
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void ADSBDemodSink::applySettings(const ADSBDemodSettings& settings, const QStringList& settingsKeys, bool force)
{
    qDebug() << "ADSBDemodSink::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_correlationThreshold: " << settings.m_correlationThreshold
            << " m_demodModeS: " << settings.m_demodModeS
            << " m_samplesPerBit: " << settings.m_samplesPerBit
            << " force: " << force;

    if ((settingsKeys.contains("rfBandwidth") && (settings.m_rfBandwidth != m_settings.m_rfBandwidth))
        || (settingsKeys.contains("samplesPerBit") && (settings.m_samplesPerBit != m_settings.m_samplesPerBit))
        || (settingsKeys.contains("interpolatorPhaseSteps") && (settings.m_interpolatorPhaseSteps != m_settings.m_interpolatorPhaseSteps))
        || (settingsKeys.contains("interpolatorTapsPerPhase") && (settings.m_interpolatorTapsPerPhase != m_settings.m_interpolatorTapsPerPhase))
        || force)
    {
        m_interpolator.create(m_settings.m_interpolatorPhaseSteps, m_channelSampleRate, settings.m_rfBandwidth / 2.2,  m_settings.m_interpolatorTapsPerPhase);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) (ADS_B_BITS_PER_SECOND * settings.m_samplesPerBit);
    }

    if ((settingsKeys.contains("samplesPerBit") && (settings.m_samplesPerBit != m_settings.m_samplesPerBit)) || force)
    {
        init(settings.m_samplesPerBit);
    }

    // Forward to worker
    ADSBDemodSinkWorker::MsgConfigureADSBDemodSinkWorker *msg = ADSBDemodSinkWorker::MsgConfigureADSBDemodSinkWorker::create(
            settings, settingsKeys, force);
    m_worker.getInputMessageQueue()->push(msg);

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

void ADSBDemodSink::resetStats()
{
    ADSBDemod::MsgResetStats* msg = ADSBDemod::MsgResetStats::create();
    m_worker.getInputMessageQueue()->push(msg);
}
