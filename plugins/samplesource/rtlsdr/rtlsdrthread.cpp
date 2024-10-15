///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2014 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2015-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#include <errno.h>

#include "rtlsdrthread.h"
#include "rtlsdrinput.h"

#include "dsp/devicesamplesource.h"
#include "dsp/samplesinkfifo.h"

#define FCD_BLOCKSIZE 16384

RTLSDRThread::RTLSDRThread(rtlsdr_dev_t* dev, SampleSinkFifo* sampleFifo, ReplayBuffer<quint8> *replayBuffer, const RTLSDRSettings& settings, QObject* parent) :
	QThread(parent),
	m_running(false),
	m_dev(dev),
	m_convertBuffer(FCD_BLOCKSIZE),
	m_sampleFifo(sampleFifo),
	m_replayBuffer(replayBuffer)
{
    applySettings(settings, QStringList(), true);
    connect(&m_inputMessageQueue, &MessageQueue::messageEnqueued, this, &RTLSDRThread::handleInputMessages);
}

RTLSDRThread::~RTLSDRThread()
{
    qDebug() << "RTLSDRThread::~RTLSDRThread";
    if (m_running) {
        stopWork();
    }
}

void RTLSDRThread::startWork()
{
    connect(&m_inputMessageQueue, &MessageQueue::messageEnqueued, this, &RTLSDRThread::handleInputMessages);
    m_startWaitMutex.lock();
    start();
    while (!m_running) {
        m_startWaiter.wait(&m_startWaitMutex, 100);
    }
	m_startWaitMutex.unlock();
}

void RTLSDRThread::stopWork()
{
    if (m_running)
    {
        disconnect(&m_inputMessageQueue, &MessageQueue::messageEnqueued, this, &RTLSDRThread::handleInputMessages);
        m_running = false; // Cause run() to finish
#ifndef __EMSCRIPTEN__
        wait();
#endif
    }
}

void RTLSDRThread::run()
{
	int res;

	m_running = true;
	m_startWaiter.wakeAll();

    while (m_running)
    {
#ifndef __EMSCRIPTEN__
        if ((res = rtlsdr_read_async(m_dev, &RTLSDRThread::callbackHelper, this, 32, FCD_BLOCKSIZE)) < 0)
        {
            if (m_running) {
                qCritical("RTLSDRThread: async read error: %s", strerror(errno));
            }
            break;
        }
#else
        int len = 0;
        unsigned char buf[FCD_BLOCKSIZE];

        if ((res = rtlsdr_read_sync(m_dev, buf, sizeof(buf), &len)) < 0)
        {
            qCritical("RTLSDRThread: read error: %s", strerror(errno));
            break;
        }
        else
        {
            if (m_settings.m_iqOrder) {
                callbackIQ(buf, len);
            } else {
                callbackQI(buf, len);
            }
        }
#endif
    }

    m_running = false;
}

//  Decimate according to specified log2 (ex: log2=4 => decim=16)
//  Len is total samples (i.e. one I and Q pair will have len=2)
void RTLSDRThread::callbackIQ(const quint8* inBuf, qint32 len)
{
    SampleVector::iterator it = m_convertBuffer.begin();

    // Save data to replay buffer
    m_replayBuffer->lock();
    bool replayEnabled = m_replayBuffer->getSize() > 0;
    if (replayEnabled) {
        m_replayBuffer->write(inBuf, len);
    }

    const quint8* buf = inBuf;
    qint32 remaining = len;

    while (remaining > 0)
    {
        // Choose between live data or replayed data
        if (replayEnabled && m_replayBuffer->useReplay()) {
            len = m_replayBuffer->read(remaining, buf);
        } else {
            len = remaining;
        }
        remaining -= len;

        if (m_settings.m_log2Decim == 0)
        {
            m_decimatorsIQ.decimate1(&it, buf, len);
        }
        else
        {
            if (m_settings.m_fcPos == 0) // Infradyne
            {
                switch (m_settings.m_log2Decim)
                {
                case 1:
                    m_decimatorsIQ.decimate2_inf(&it, buf, len);
                    break;
                case 2:
                    m_decimatorsIQ.decimate4_inf(&it, buf, len);
                    break;
                case 3:
                    m_decimatorsIQ.decimate8_inf(&it, buf, len);
                    break;
                case 4:
                    m_decimatorsIQ.decimate16_inf(&it, buf, len);
                    break;
                case 5:
                    m_decimatorsIQ.decimate32_inf(&it, buf, len);
                    break;
                case 6:
                    m_decimatorsIQ.decimate64_inf(&it, buf, len);
                    break;
                default:
                    break;
                }
            }
            else if (m_settings.m_fcPos == 1) // Supradyne
            {
                switch (m_settings.m_log2Decim)
                {
                case 1:
                    m_decimatorsIQ.decimate2_sup(&it, buf, len);
                    break;
                case 2:
                    m_decimatorsIQ.decimate4_sup(&it, buf, len);
                    break;
                case 3:
                    m_decimatorsIQ.decimate8_sup(&it, buf, len);
                    break;
                case 4:
                    m_decimatorsIQ.decimate16_sup(&it, buf, len);
                    break;
                case 5:
                    m_decimatorsIQ.decimate32_sup(&it, buf, len);
                    break;
                case 6:
                    m_decimatorsIQ.decimate64_sup(&it, buf, len);
                    break;
                default:
                    break;
                }
            }
            else // Centered
            {
                switch (m_settings.m_log2Decim)
                {
                case 1:
                    m_decimatorsIQ.decimate2_cen(&it, buf, len);
                    break;
                case 2:
                    m_decimatorsIQ.decimate4_cen(&it, buf, len);
                    break;
                case 3:
                    m_decimatorsIQ.decimate8_cen(&it, buf, len);
                    break;
                case 4:
                    m_decimatorsIQ.decimate16_cen(&it, buf, len);
                    break;
                case 5:
                    m_decimatorsIQ.decimate32_cen(&it, buf, len);
                    break;
                case 6:
                    m_decimatorsIQ.decimate64_cen(&it, buf, len);
                    break;
                default:
                    break;
                }
            }
        }
    }

    m_replayBuffer->unlock();

    m_sampleFifo->write(m_convertBuffer.begin(), it);

#ifndef __EMSCRIPTEN__
    if (!m_running)
        rtlsdr_cancel_async(m_dev);
#endif
}

void RTLSDRThread::callbackQI(const quint8* inBuf, qint32 len)
{
    SampleVector::iterator it = m_convertBuffer.begin();

    // Save data to replay buffer
    m_replayBuffer->lock();
    bool replayEnabled = m_replayBuffer->getSize() > 0;
    if (replayEnabled) {
        m_replayBuffer->write(inBuf, len);
    }

    const quint8* buf = inBuf;
    qint32 remaining = len;

    while (remaining > 0)
    {
        // Choose between live data or replayed data
        if (replayEnabled && m_replayBuffer->useReplay()) {
            len = m_replayBuffer->read(remaining, buf);
        } else {
            len = remaining;
        }
        remaining -= len;

        if (m_settings.m_log2Decim == 0)
        {
            m_decimatorsQI.decimate1(&it, buf, len);
        }
        else
        {
            if (m_settings.m_fcPos == 0) // Infradyne
            {
                switch (m_settings.m_log2Decim)
                {
                case 1:
                    m_decimatorsQI.decimate2_inf(&it, buf, len);
                    break;
                case 2:
                    m_decimatorsQI.decimate4_inf(&it, buf, len);
                    break;
                case 3:
                    m_decimatorsQI.decimate8_inf(&it, buf, len);
                    break;
                case 4:
                    m_decimatorsQI.decimate16_inf(&it, buf, len);
                    break;
                case 5:
                    m_decimatorsQI.decimate32_inf(&it, buf, len);
                    break;
                case 6:
                    m_decimatorsQI.decimate64_inf(&it, buf, len);
                    break;
                default:
                    break;
                }
            }
            else if (m_settings.m_fcPos == 1) // Supradyne
            {
                switch (m_settings.m_log2Decim)
                {
                case 1:
                    m_decimatorsQI.decimate2_sup(&it, buf, len);
                    break;
                case 2:
                    m_decimatorsQI.decimate4_sup(&it, buf, len);
                    break;
                case 3:
                    m_decimatorsQI.decimate8_sup(&it, buf, len);
                    break;
                case 4:
                    m_decimatorsQI.decimate16_sup(&it, buf, len);
                    break;
                case 5:
                    m_decimatorsQI.decimate32_sup(&it, buf, len);
                    break;
                case 6:
                    m_decimatorsQI.decimate64_sup(&it, buf, len);
                    break;
                default:
                    break;
                }
            }
            else // Centered
            {
                switch (m_settings.m_log2Decim)
                {
                case 1:
                    m_decimatorsQI.decimate2_cen(&it, buf, len);
                    break;
                case 2:
                    m_decimatorsQI.decimate4_cen(&it, buf, len);
                    break;
                case 3:
                    m_decimatorsQI.decimate8_cen(&it, buf, len);
                    break;
                case 4:
                    m_decimatorsQI.decimate16_cen(&it, buf, len);
                    break;
                case 5:
                    m_decimatorsQI.decimate32_cen(&it, buf, len);
                    break;
                case 6:
                    m_decimatorsQI.decimate64_cen(&it, buf, len);
                    break;
                default:
                    break;
                }
            }
        }
    }

    m_replayBuffer->unlock();

    m_sampleFifo->write(m_convertBuffer.begin(), it);

#ifndef __EMSCRIPTEN__
    if (!m_running)
        rtlsdr_cancel_async(m_dev);
#endif
}

#ifndef __EMSCRIPTEN__
void RTLSDRThread::callbackHelper(unsigned char* buf, uint32_t len, void* ctx)
{
	RTLSDRThread* thread = (RTLSDRThread*) ctx;

    if (thread->m_settings.m_iqOrder) {
    	thread->callbackIQ(buf, len);
    } else {
        thread->callbackQI(buf, len);
    }
}
#endif

void RTLSDRThread::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

bool RTLSDRThread::handleMessage(const Message& cmd)
{
    if (RTLSDRInput::MsgConfigureRTLSDR::match(cmd))
    {
        auto& conf = (const RTLSDRInput::MsgConfigureRTLSDR&) cmd;

        applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());

        return true;
    }
    else
    {
        return false;
    }
}

bool RTLSDRThread::applySettings(const RTLSDRSettings& settings, const QStringList& settingsKeys, bool force)
{
    qDebug() << "RTLSDRThread::applySettings: force: " << force << settings.getDebugString(settingsKeys, force);

    if ((settingsKeys.contains("agc") && (settings.m_agc != m_settings.m_agc)) || force)
    {
        if (rtlsdr_set_agc_mode(m_dev, settings.m_agc ? 1 : 0) < 0) {
            qCritical("RTLSDRThread::applySettings: could not set AGC mode %s", settings.m_agc ? "on" : "off");
        } else {
            qDebug("RTLSDRThread::applySettings: AGC mode %s", settings.m_agc ? "on" : "off");
        }
    }

    if ((settingsKeys.contains("loPpmCorrection") && (settings.m_loPpmCorrection != m_settings.m_loPpmCorrection)) || force)
    {
        if (rtlsdr_set_freq_correction(m_dev, settings.m_loPpmCorrection) < 0) {
            qCritical("RTLSDRThread::applySettings: could not set LO ppm correction: %d", settings.m_loPpmCorrection);
        } else {
            qDebug("RTLSDRThread::applySettings: LO ppm correction set to: %d", settings.m_loPpmCorrection);
        }
    }

    if ((settingsKeys.contains("devSampleRate") && ((settings.m_devSampleRate) != m_settings.m_devSampleRate)) || force)
    {
        if (rtlsdr_set_sample_rate(m_dev, settings.m_devSampleRate) < 0) {
            qCritical("RTLSDRThread::applySettings: could not set sample rate: %d", settings.m_devSampleRate);
        } else {
            qDebug("RTLSDRThread::applySettings: sample rate set to %d", settings.m_devSampleRate);
        }
    }

    if ((settingsKeys.contains("log2Decim") && (settings.m_log2Decim != m_settings.m_log2Decim)) || force)
    {
        qDebug("RTLSDRThread::applySettings: log2decim set to %d", settings.m_log2Decim);
    }

    if (   (settingsKeys.contains("centerFrequency") && (settings.m_centerFrequency != m_settings.m_centerFrequency))
        || (settingsKeys.contains("fcPos") && (settings.m_fcPos != m_settings.m_fcPos))
        || (settingsKeys.contains("log2Decim") && (settings.m_log2Decim != m_settings.m_log2Decim))
        || (settingsKeys.contains("devSampleRate") && (settings.m_devSampleRate != m_settings.m_devSampleRate))
        || (settingsKeys.contains("transverterMode") && (settings.m_transverterMode != m_settings.m_transverterMode))
        || (settingsKeys.contains("transverterDeltaFrequency") && (settings.m_transverterDeltaFrequency != m_settings.m_transverterDeltaFrequency))
        || force)
    {
        qint64 deviceCenterFrequency = DeviceSampleSource::calculateDeviceCenterFrequency(
                settings.m_centerFrequency,
                settings.m_transverterDeltaFrequency,
                settings.m_log2Decim,
                (DeviceSampleSource::fcPos_t) settings.m_fcPos,
                settings.m_devSampleRate,
                DeviceSampleSource::FrequencyShiftScheme::FSHIFT_STD,
                settings.m_transverterMode);

        if (rtlsdr_set_center_freq(m_dev, deviceCenterFrequency) != 0) {
            qWarning("RTLSDRThread::applySettings: rtlsdr_set_center_freq(%lld) failed", deviceCenterFrequency);
        } else {
            qDebug("RTLSDRThread::applySettings: rtlsdr_set_center_freq(%lld)", deviceCenterFrequency);
        }
    }

    if ((settingsKeys.contains("noModMode") && (settings.m_noModMode != m_settings.m_noModMode)) || force)
    {
        qDebug() << "RTLSDRThread::applySettings: set noModMode to " << settings.m_noModMode;

        // Direct Modes: 0: off, 1: I, 2: Q, 3: NoMod.
        if (settings.m_noModMode) {
            rtlsdr_set_direct_sampling(m_dev, 3);
        } else {
            rtlsdr_set_direct_sampling(m_dev, 0);
        }
    }

    if ((settingsKeys.contains("rfBandwidth") && (settings.m_rfBandwidth != m_settings.m_rfBandwidth)) || force)
    {
        if (rtlsdr_set_tuner_bandwidth( m_dev, settings.m_rfBandwidth) != 0) {
            qCritical("RTLSDRThread::applySettings: could not set RF bandwidth to %u", settings.m_rfBandwidth);
        } else {
            qDebug() << "RTLSDRThread::applySettings: set RF bandwidth to " << settings.m_rfBandwidth;
        }
    }

    // Reapply offset_tuning setting if bandwidth is changed, otherwise frequency response of filter looks wrong on E4000
    if (   (settingsKeys.contains("offsetTuning") && (settings.m_offsetTuning != m_settings.m_offsetTuning))
        || (settingsKeys.contains("rfBandwidth") && (settings.m_rfBandwidth != m_settings.m_rfBandwidth))
        || force)
    {
        if (rtlsdr_set_offset_tuning(m_dev, settings.m_offsetTuning ? 0 : 1) != 0) {
            qCritical("RTLSDRThread::applySettings: could not set offset tuning to %s", settings.m_offsetTuning ? "on" : "off");
        } else {
            qDebug("RTLSDRThread::applySettings: offset tuning set to %s", settings.m_offsetTuning ? "on" : "off");
        }
    }

    if ((settingsKeys.contains("gain") && (settings.m_gain != m_settings.m_gain)) || force)
    {
        // Nooelec E4000 SDRs appear to require tuner_gain_mode to be reset to manual before
        // each call to set_tuner_gain, otherwise tuner AGC seems to be reenabled
        if (rtlsdr_set_tuner_gain_mode(m_dev, 1) < 0) {
            qCritical("RTLSDRThread::applySettings: error setting tuner gain mode to manual");
        }
        if (rtlsdr_set_tuner_gain(m_dev, settings.m_gain) != 0) {
            qCritical("RTLSDRThread::applySettings: rtlsdr_set_tuner_gain() failed");
        } else {
            qDebug("RTLSDRThread::applySettings: rtlsdr_set_tuner_gain() to %d", settings.m_gain);
        }
    }

    if ((settingsKeys.contains("biasTee") && (settings.m_biasTee != m_settings.m_biasTee)) || force)
    {
        if (rtlsdr_set_bias_tee(m_dev, settings.m_biasTee ? 1 : 0) != 0) {
            qCritical("RTLSDRThread::applySettings: rtlsdr_set_bias_tee() failed");
        } else {
            qDebug("RTLSDRThread::applySettings: rtlsdr_set_bias_tee() to %d", settings.m_biasTee ? 1 : 0);
        }
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    return true;
}
