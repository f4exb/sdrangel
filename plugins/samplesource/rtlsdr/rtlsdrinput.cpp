///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
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
#include <string.h>
#include <errno.h>
#include "rtlsdrinput.h"

#include "device/devicesourceapi.h"

#include "rtlsdrthread.h"
#include "rtlsdrgui.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/filerecord.h"

MESSAGE_CLASS_DEFINITION(RTLSDRInput::MsgConfigureRTLSDR, Message)
MESSAGE_CLASS_DEFINITION(RTLSDRInput::MsgReportRTLSDR, Message)
MESSAGE_CLASS_DEFINITION(RTLSDRInput::MsgFileRecord, Message)

const quint64 RTLSDRInput::frequencyLowRangeMin = 1000UL;
const quint64 RTLSDRInput::frequencyLowRangeMax = 275000UL;
const quint64 RTLSDRInput::frequencyHighRangeMin = 24000UL;
const quint64 RTLSDRInput::frequencyHighRangeMax = 1900000UL;
const int RTLSDRInput::sampleRateLowRangeMin = 230000U;
const int RTLSDRInput::sampleRateLowRangeMax = 300000U;
const int RTLSDRInput::sampleRateHighRangeMin = 950000U;
const int RTLSDRInput::sampleRateHighRangeMax = 2400000U;

RTLSDRInput::RTLSDRInput(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_dev(0),
	m_rtlSDRThread(0),
	m_deviceDescription(),
	m_running(false)
{
    openDevice();

    char recFileNameCStr[30];
    sprintf(recFileNameCStr, "test_%d.sdriq", m_deviceAPI->getDeviceUID());
    m_fileSink = new FileRecord(std::string(recFileNameCStr));
    m_deviceAPI->addSink(m_fileSink);
}

RTLSDRInput::~RTLSDRInput()
{
    if (m_running) stop();
    m_deviceAPI->removeSink(m_fileSink);
    delete m_fileSink;
    closeDevice();
}

void RTLSDRInput::destroy()
{
    delete this;
}

bool RTLSDRInput::openDevice()
{
    if (m_dev != 0)
    {
        closeDevice();
    }

    char vendor[256];
    char product[256];
    char serial[256];
    int res;
    int numberOfGains;

    if (!m_sampleFifo.setSize(96000 * 4))
    {
        qCritical("RTLSDRInput::openDevice: Could not allocate SampleFifo");
        return false;
    }

    int device;

    if ((device = rtlsdr_get_index_by_serial(qPrintable(m_deviceAPI->getSampleSourceSerial()))) < 0)
    {
        qCritical("RTLSDRInput::openDevice: could not get RTLSDR serial number");
        return false;
    }

    if ((res = rtlsdr_open(&m_dev, device)) < 0)
    {
        qCritical("RTLSDRInput::openDevice: could not open RTLSDR #%d: %s", device, strerror(errno));
        return false;
    }

    vendor[0] = '\0';
    product[0] = '\0';
    serial[0] = '\0';

    if ((res = rtlsdr_get_usb_strings(m_dev, vendor, product, serial)) < 0)
    {
        qCritical("RTLSDRInput::openDevice: error accessing USB device");
        stop();
        return false;
    }

    qInfo("RTLSDRInput::openDevice: open: %s %s, SN: %s", vendor, product, serial);
    m_deviceDescription = QString("%1 (SN %2)").arg(product).arg(serial);

    if ((res = rtlsdr_set_sample_rate(m_dev, 1152000)) < 0)
    {
        qCritical("RTLSDRInput::openDevice: could not set sample rate: 1024k S/s");
        stop();
        return false;
    }

    if ((res = rtlsdr_set_tuner_gain_mode(m_dev, 1)) < 0)
    {
        qCritical("RTLSDRInput::openDevice: error setting tuner gain mode");
        stop();
        return false;
    }

    if ((res = rtlsdr_set_agc_mode(m_dev, 0)) < 0)
    {
        qCritical("RTLSDRInput::openDevice: error setting agc mode");
        stop();
        return false;
    }

    numberOfGains = rtlsdr_get_tuner_gains(m_dev, NULL);

    if (numberOfGains < 0)
    {
        qCritical("RTLSDRInput::openDevice: error getting number of gain values supported");
        stop();
        return false;
    }

    m_gains.resize(numberOfGains);

    if (rtlsdr_get_tuner_gains(m_dev, &m_gains[0]) < 0)
    {
        qCritical("RTLSDRInput::openDevice: error getting gain values");
        stop();
        return false;
    }
    else
    {
        qDebug() << "RTLSDRInput::openDevice: " << m_gains.size() << "gains";
    }

    if ((res = rtlsdr_reset_buffer(m_dev)) < 0)
    {
        qCritical("RTLSDRInput::openDevice: could not reset USB EP buffers: %s", strerror(errno));
        stop();
        return false;
    }

    return true;
}

bool RTLSDRInput::start()
{
	QMutexLocker mutexLocker(&m_mutex);

	if (!m_dev) {
	    return false;
	}

    if (m_running) stop();

	if ((m_rtlSDRThread = new RTLSDRThread(m_dev, &m_sampleFifo)) == NULL)
	{
		qFatal("RTLSDRInput::start: out of memory");
		stop();
		return false;
	}

	m_rtlSDRThread->setSamplerate(m_settings.m_devSampleRate);
	m_rtlSDRThread->setLog2Decimation(m_settings.m_log2Decim);
	m_rtlSDRThread->setFcPos((int) m_settings.m_fcPos);

	m_rtlSDRThread->startWork();

	mutexLocker.unlock();

	applySettings(m_settings, true);
	m_running = true;

	return true;
}

void RTLSDRInput::closeDevice()
{
    if (m_dev != 0)
    {
        rtlsdr_close(m_dev);
        m_dev = 0;
    }

    m_deviceDescription.clear();
}

void RTLSDRInput::stop()
{
	QMutexLocker mutexLocker(&m_mutex);

	if (m_rtlSDRThread != 0)
	{
		m_rtlSDRThread->stopWork();
		delete m_rtlSDRThread;
		m_rtlSDRThread = 0;
	}

	m_running = false;
}

const QString& RTLSDRInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int RTLSDRInput::getSampleRate() const
{
	int rate = m_settings.m_devSampleRate;
	return (rate / (1<<m_settings.m_log2Decim));
}

quint64 RTLSDRInput::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

bool RTLSDRInput::handleMessage(const Message& message)
{
    if (MsgConfigureRTLSDR::match(message))
    {
        MsgConfigureRTLSDR& conf = (MsgConfigureRTLSDR&) message;
        qDebug() << "RTLSDRInput::handleMessage: MsgConfigureRTLSDR";

        bool success = applySettings(conf.getSettings(), conf.getForce());

        if (!success)
        {
            qDebug("RTLSDRInput::handleMessage: config error");
        }

        return true;
    }
    else if (MsgFileRecord::match(message))
    {
        MsgFileRecord& conf = (MsgFileRecord&) message;
        qDebug() << "RTLSDRInput::handleMessage: MsgFileRecord: " << conf.getStartStop();

        if (conf.getStartStop()) {
            m_fileSink->startRecording();
        } else {
            m_fileSink->stopRecording();
        }

        return true;
    }
    else
    {
        return false;
    }
}

bool RTLSDRInput::applySettings(const RTLSDRSettings& settings, bool force)
{
    bool forwardChange = false;

    if ((m_settings.m_agc != settings.m_agc) || force)
    {
        if (rtlsdr_set_agc_mode(m_dev, settings.m_agc ? 1 : 0) < 0)
        {
            qCritical("could not set AGC mode %s", settings.m_agc ? "on" : "off");
        }
        else
        {
            m_settings.m_agc = settings.m_agc;
        }
    }

    if ((m_settings.m_gain != settings.m_gain) || force)
    {
        m_settings.m_gain = settings.m_gain;

        if(m_dev != 0)
        {
            if(rtlsdr_set_tuner_gain(m_dev, m_settings.m_gain) != 0)
            {
                qDebug("rtlsdr_set_tuner_gain() failed");
            }
        }
    }

    if ((m_settings.m_dcBlock != settings.m_dcBlock) || force)
    {
        m_settings.m_dcBlock = settings.m_dcBlock;
        m_deviceAPI->configureCorrections(m_settings.m_dcBlock, m_settings.m_iqImbalance);
    }

    if ((m_settings.m_iqImbalance != settings.m_iqImbalance) || force)
    {
        m_settings.m_iqImbalance = settings.m_iqImbalance;
        m_deviceAPI->configureCorrections(m_settings.m_dcBlock, m_settings.m_iqImbalance);
    }

    if ((m_settings.m_loPpmCorrection != settings.m_loPpmCorrection) || force)
    {
        if (m_dev != 0)
        {
            if (rtlsdr_set_freq_correction(m_dev, settings.m_loPpmCorrection) < 0)
            {
                qCritical("RTLSDRInput::applySettings: could not set LO ppm correction: %d", settings.m_loPpmCorrection);
            }
            else
            {
                m_settings.m_loPpmCorrection = settings.m_loPpmCorrection;
                qDebug("RTLSDRInput::applySettings: LO ppm correction set to: %d", settings.m_loPpmCorrection);
            }
        }
    }

    if ((m_settings.m_devSampleRate != settings.m_devSampleRate) || force)
    {
        m_settings.m_devSampleRate = settings.m_devSampleRate;
        forwardChange = true;

        if(m_dev != 0)
        {
            if( rtlsdr_set_sample_rate(m_dev, settings.m_devSampleRate) < 0)
            {
                qCritical("RTLSDRInput::applySettings: could not set sample rate: %d", settings.m_devSampleRate);
            }
            else
            {
                if (m_rtlSDRThread) m_rtlSDRThread->setSamplerate(settings.m_devSampleRate);
                qDebug("RTLSDRInput::applySettings: sample rate set to %d", m_settings.m_devSampleRate);
            }
        }
    }

    if ((m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        m_settings.m_log2Decim = settings.m_log2Decim;
        forwardChange = true;

        if (m_rtlSDRThread != 0)
        {
            m_rtlSDRThread->setLog2Decimation(settings.m_log2Decim);
        }
    }

    if (force || (m_settings.m_centerFrequency != settings.m_centerFrequency)
            || (m_settings.m_fcPos != settings.m_fcPos)
            || (m_settings.m_transverterMode != settings.m_transverterMode)
            || (m_settings.m_transverterDeltaFrequency != settings.m_transverterDeltaFrequency))
    {
        m_settings.m_centerFrequency = settings.m_centerFrequency;
        m_settings.m_transverterMode = settings.m_transverterMode;
        m_settings.m_transverterDeltaFrequency = settings.m_transverterDeltaFrequency;
        qint64 deviceCenterFrequency = m_settings.m_centerFrequency;
        deviceCenterFrequency -= m_settings.m_transverterMode ? m_settings.m_transverterDeltaFrequency : 0;
        deviceCenterFrequency = deviceCenterFrequency < 0 ? 0 : deviceCenterFrequency;
        qint64 f_img = deviceCenterFrequency;
        quint32 devSampleRate = m_settings.m_devSampleRate;

        forwardChange = true;

        if ((m_settings.m_log2Decim == 0) || (settings.m_fcPos == RTLSDRSettings::FC_POS_CENTER))
        {
            f_img = deviceCenterFrequency;
        }
        else
        {
            if (settings.m_fcPos == RTLSDRSettings::FC_POS_INFRA)
            {
                deviceCenterFrequency += (devSampleRate / 4);
                f_img = deviceCenterFrequency + devSampleRate/2;
            }
            else if (settings.m_fcPos == RTLSDRSettings::FC_POS_SUPRA)
            {
                deviceCenterFrequency -= (devSampleRate / 4);
                f_img = deviceCenterFrequency - devSampleRate/2;
            }
        }

        if (m_dev != 0)
        {
            if (rtlsdr_set_center_freq( m_dev, deviceCenterFrequency ) != 0)
            {
                qDebug("rtlsdr_set_center_freq(%lld) failed", deviceCenterFrequency);
            }
            else
            {
                qDebug() << "RTLSDRInput::applySettings: center freq: " << m_settings.m_centerFrequency << " Hz"
                        << " device center freq: " << deviceCenterFrequency << " Hz"
                        << " device sample rate: " << devSampleRate << "S/s"
                        << " Actual sample rate: " << devSampleRate/(1<<m_settings.m_log2Decim) << "S/s"
                        << " img: " << f_img << "Hz";
            }
        }
    }

    if ((m_settings.m_fcPos != settings.m_fcPos) || force)
    {
        m_settings.m_fcPos = settings.m_fcPos;

        if (m_rtlSDRThread != 0)
        {
            m_rtlSDRThread->setFcPos((int) m_settings.m_fcPos);
            qDebug() << "RTLSDRInput: set fc pos (enum) to " << (int) m_settings.m_fcPos;
        }
    }

    if ((m_settings.m_noModMode != settings.m_noModMode) || force)
    {
        m_settings.m_noModMode = settings.m_noModMode;

        // Direct Modes: 0: off, 1: I, 2: Q, 3: NoMod.
        if (m_settings.m_noModMode) {
            set_ds_mode(3);
        } else {
            set_ds_mode(0);
        }
    }

    if (forwardChange)
    {
        int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2Decim);
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
        m_fileSink->handleMessage(*notif); // forward to file sink
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    return true;
}

void RTLSDRInput::setMessageQueueToGUI(MessageQueue *queue)
{
    qDebug("RTLSDRInput::setMessageQueueToGUI: %p", queue);
    DeviceSampleSource::setMessageQueueToGUI(queue);

    if (queue) {
        MsgReportRTLSDR *message = MsgReportRTLSDR::create(m_gains);
        queue->push(message);
    }
}

void RTLSDRInput::set_ds_mode(int on)
{
	rtlsdr_set_direct_sampling(m_dev, on);
}

