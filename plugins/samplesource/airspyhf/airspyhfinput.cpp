///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include <string.h>
#include <errno.h>
#include <QDebug>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGAirspyHFReport.h"

#include <device/devicesourceapi.h>
#include <dsp/filerecord.h>
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"

#include "airspyhfinput.h"

#include "airspyhfplugin.h"
#include "airspyhfsettings.h"
#include "airspyhfthread.h"

MESSAGE_CLASS_DEFINITION(AirspyHFInput::MsgConfigureAirspyHF, Message)
MESSAGE_CLASS_DEFINITION(AirspyHFInput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(AirspyHFInput::MsgFileRecord, Message)

const qint64 AirspyHFInput::loLowLimitFreqHF   =      9000L;
const qint64 AirspyHFInput::loHighLimitFreqHF  =  31000000L;
const qint64 AirspyHFInput::loLowLimitFreqVHF  =  60000000L;
const qint64 AirspyHFInput::loHighLimitFreqVHF = 260000000L;

AirspyHFInput::AirspyHFInput(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_dev(0),
	m_airspyHFThread(0),
	m_deviceDescription("AirspyHF"),
	m_running(false)
{
    openDevice();
    m_fileSink = new FileRecord(QString("test_%1.sdriq").arg(m_deviceAPI->getDeviceUID()));
    m_deviceAPI->addSink(m_fileSink);
}

AirspyHFInput::~AirspyHFInput()
{
    if (m_running) { stop(); }
    m_deviceAPI->removeSink(m_fileSink);
    delete m_fileSink;
    closeDevice();
}

void AirspyHFInput::destroy()
{
    delete this;
}

bool AirspyHFInput::openDevice()
{
    if (m_dev != 0)
    {
        closeDevice();
    }

    airspyhf_error rc;

    if (!m_sampleFifo.setSize(1<<19))
    {
        qCritical("AirspyHFInput::openDevice: could not allocate SampleFifo");
        return false;
    }

    if ((m_dev = open_airspyhf_from_serial(m_deviceAPI->getSampleSourceSerial())) == 0)
    {
        qCritical("AirspyHFInput::openDevice: could not open Airspy HF with serial %s", qPrintable(m_deviceAPI->getSampleSourceSerial()));
        m_dev = 0;
        return false;
    }
    else
    {
        qDebug("AirspyHFInput::openDevice: opened Airspy HF with serial %s", qPrintable(m_deviceAPI->getSampleSourceSerial()));
    }

    uint32_t nbSampleRates;
    uint32_t *sampleRates;

    rc = (airspyhf_error) airspyhf_get_samplerates(m_dev, &nbSampleRates, 0);

    if (rc == AIRSPYHF_SUCCESS)
    {
        qDebug("AirspyHFInput::openDevice: %d sample rates for Airspy HF", nbSampleRates);
    }
    else
    {
        qCritical("AirspyHFInput::openDevice: could not obtain the number of Airspy HF sample rates");
        closeDevice();
        return false;
    }

    sampleRates = new uint32_t[nbSampleRates];

    rc = (airspyhf_error) airspyhf_get_samplerates(m_dev, sampleRates, nbSampleRates);

    if (rc == AIRSPYHF_SUCCESS)
    {
        qDebug("AirspyHFInput::openDevice: obtained Airspy HF sample rates");
    }
    else
    {
        qCritical("AirspyHFInput::openDevice: could not obtain Airspy HF sample rates");
        closeDevice();
        return false;
    }

    m_sampleRates.clear();

    for (unsigned int i = 0; i < nbSampleRates; i++)
    {
        m_sampleRates.push_back(sampleRates[i]);
        qDebug("AirspyHFInput::openDevice: sampleRates[%d] = %u Hz", i, sampleRates[i]);
    }

    delete[] sampleRates;

    return true;
}

void AirspyHFInput::init()
{
    applySettings(m_settings, true);
}

bool AirspyHFInput::start()
{
	QMutexLocker mutexLocker(&m_mutex);

    if (!m_dev) {
        return false;
    }

    if (m_running) { stop(); }

	m_airspyHFThread = new AirspyHFThread(m_dev, &m_sampleFifo);
	int sampleRateIndex = m_settings.m_devSampleRateIndex;

    if (m_settings.m_devSampleRateIndex >= m_sampleRates.size()) {
        sampleRateIndex = m_sampleRates.size() - 1;
    }

    if (sampleRateIndex >= 0) {
        m_airspyHFThread->setSamplerate(m_sampleRates[sampleRateIndex]);
    }

	m_airspyHFThread->setLog2Decimation(m_settings.m_log2Decim);
	m_airspyHFThread->startWork();

	mutexLocker.unlock();

	applySettings(m_settings, true);

	qDebug("AirspyHFInput::startInput: started");
	m_running = true;

	return true;
}

void AirspyHFInput::closeDevice()
{
    if (m_dev != 0)
    {
        airspyhf_stop(m_dev);
        airspyhf_close(m_dev);
        m_dev = 0;
    }

    m_deviceDescription.clear();
}

void AirspyHFInput::stop()
{
	qDebug("AirspyHFInput::stop");
	QMutexLocker mutexLocker(&m_mutex);

	if (m_airspyHFThread != 0)
	{
	    m_airspyHFThread->stopWork();
		delete m_airspyHFThread;
		m_airspyHFThread = 0;
	}

	m_running = false;
}

QByteArray AirspyHFInput::serialize() const
{
    return m_settings.serialize();
}

bool AirspyHFInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureAirspyHF* message = MsgConfigureAirspyHF::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureAirspyHF* messageToGUI = MsgConfigureAirspyHF::create(m_settings, true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& AirspyHFInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int AirspyHFInput::getSampleRate() const
{
    int sampleRateIndex = m_settings.m_devSampleRateIndex;

    if (m_settings.m_devSampleRateIndex >= m_sampleRates.size()) {
        sampleRateIndex = m_sampleRates.size() - 1;
    }

    if (sampleRateIndex >= 0)
    {
        int rate = m_sampleRates[sampleRateIndex];
        return (rate / (1<<m_settings.m_log2Decim));
    }
    else
    {
        return 0;
    }
}

quint64 AirspyHFInput::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void AirspyHFInput::setCenterFrequency(qint64 centerFrequency)
{
    AirspyHFSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureAirspyHF* message = MsgConfigureAirspyHF::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureAirspyHF* messageToGUI = MsgConfigureAirspyHF::create(settings, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool AirspyHFInput::handleMessage(const Message& message)
{
	if (MsgConfigureAirspyHF::match(message))
	{
	    MsgConfigureAirspyHF& conf = (MsgConfigureAirspyHF&) message;
		qDebug() << "MsgConfigureAirspyHF::handleMessage: MsgConfigureAirspyHF";

		bool success = applySettings(conf.getSettings(), conf.getForce());

		if (!success)
		{
			qDebug("MsgConfigureAirspyHF::handleMessage: AirspyHF config error");
		}

		return true;
	}
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "AirspyHFInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initAcquisition())
            {
                m_deviceAPI->startAcquisition();
            }
        }
        else
        {
            m_deviceAPI->stopAcquisition();
        }

        return true;
    }
    else if (MsgFileRecord::match(message))
    {
        MsgFileRecord& conf = (MsgFileRecord&) message;
        qDebug() << "AirspyHFInput::handleMessage: MsgFileRecord: " << conf.getStartStop();

        if (conf.getStartStop())
        {
            if (m_settings.m_fileRecordName.size() != 0) {
                m_fileSink->setFileName(m_settings.m_fileRecordName);
            } else {
                m_fileSink->genUniqueFileName(m_deviceAPI->getDeviceUID());
            }

            m_fileSink->startRecording();
        }
        else
        {
            m_fileSink->stopRecording();
        }

        return true;
    }
	else
	{
		return false;
	}
}

void AirspyHFInput::setDeviceCenterFrequency(quint64 freq_hz, const AirspyHFSettings& settings)
{
    switch(settings.m_bandIndex)
    {
    case 1:
        freq_hz = freq_hz < loLowLimitFreqVHF ? loLowLimitFreqVHF : freq_hz > loHighLimitFreqVHF ? loHighLimitFreqVHF : freq_hz;
        break;
    case 0:
    default:
        freq_hz = freq_hz < loLowLimitFreqHF ? loLowLimitFreqHF : freq_hz > loHighLimitFreqHF ? loHighLimitFreqHF : freq_hz;
        break;
    }

	airspyhf_error rc = (airspyhf_error) airspyhf_set_freq(m_dev, static_cast<uint32_t>(freq_hz));

	if (rc == AIRSPYHF_SUCCESS) {
		qDebug("AirspyHFInput::setDeviceCenterFrequency: frequency set to %llu Hz", freq_hz);
	} else {
		qWarning("AirspyHFInput::setDeviceCenterFrequency: could not frequency to %llu Hz", freq_hz);
	}
}

bool AirspyHFInput::applySettings(const AirspyHFSettings& settings, bool force)
{
	QMutexLocker mutexLocker(&m_mutex);

	bool forwardChange = false;
	airspyhf_error rc;
	int sampleRateIndex = settings.m_devSampleRateIndex;

	qDebug() << "AirspyHFInput::applySettings";

	if ((m_settings.m_devSampleRateIndex != settings.m_devSampleRateIndex) || force)
	{
		forwardChange = true;

		if (settings.m_devSampleRateIndex >= m_sampleRates.size()) {
		    sampleRateIndex = m_sampleRates.size() - 1;
		}

		if ((m_dev != 0) && (sampleRateIndex >= 0))
		{
			rc = (airspyhf_error) airspyhf_set_samplerate(m_dev, sampleRateIndex);

			if (rc != AIRSPYHF_SUCCESS)
			{
				qCritical("AirspyHFInput::applySettings: could not set sample rate index %u (%d S/s)", sampleRateIndex, m_sampleRates[sampleRateIndex]);
			}
			else if (m_airspyHFThread != 0)
			{
				qDebug("AirspyHFInput::applySettings: sample rate set to index: %u (%d S/s)", sampleRateIndex, m_sampleRates[sampleRateIndex]);
				m_airspyHFThread->setSamplerate(m_sampleRates[sampleRateIndex]);
			}
		}
	}

	if ((m_settings.m_log2Decim != settings.m_log2Decim) || force)
	{
		forwardChange = true;

		if (m_airspyHFThread != 0)
		{
		    m_airspyHFThread->setLog2Decimation(settings.m_log2Decim);
			qDebug() << "AirspyInput: set decimation to " << (1<<settings.m_log2Decim);
		}
	}

	if ((m_settings.m_LOppmTenths != settings.m_LOppmTenths) || force)
	{
	    if (m_dev != 0)
	    {
	        rc = (airspyhf_error) airspyhf_set_calibration(m_dev, settings.m_LOppmTenths * 100);

	        if (rc != AIRSPYHF_SUCCESS)
            {
                qCritical("AirspyHFInput::applySettings: could not set LO ppm correction to %f", settings.m_LOppmTenths / 10.0f);
            }
            else if (m_airspyHFThread != 0)
            {
                qDebug("AirspyHFInput::applySettings: LO ppm correction set to %f", settings.m_LOppmTenths / 10.0f);
            }
	    }
	}

	if (force || (m_settings.m_centerFrequency != settings.m_centerFrequency)
	        || (m_settings.m_transverterMode != settings.m_transverterMode)
	        || (m_settings.m_transverterDeltaFrequency != settings.m_transverterDeltaFrequency))
	{
        qint64 deviceCenterFrequency = settings.m_centerFrequency;
        deviceCenterFrequency -= settings.m_transverterMode ? settings.m_transverterDeltaFrequency : 0;
        deviceCenterFrequency = deviceCenterFrequency < 0 ? 0 : deviceCenterFrequency;
        qint64 f_img = deviceCenterFrequency;

		if ((m_dev != 0) && (sampleRateIndex >= 0))
		{
            quint32 devSampleRate = m_sampleRates[sampleRateIndex];
			setDeviceCenterFrequency(deviceCenterFrequency, settings);

			qDebug() << "AirspyHFInput::applySettings: center freq: " << settings.m_centerFrequency << " Hz"
					<< " device center freq: " << deviceCenterFrequency << " Hz"
					<< " device sample rate: " << devSampleRate << "Hz"
					<< " Actual sample rate: " << devSampleRate/(1<<m_settings.m_log2Decim) << "Hz"
					<< " img: " << f_img << "Hz";
		}

		forwardChange = true;
	}

	if (forwardChange && (sampleRateIndex >= 0))
	{
		int sampleRate = m_sampleRates[sampleRateIndex]/(1<<settings.m_log2Decim);
		DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, settings.m_centerFrequency);
        m_fileSink->handleMessage(*notif); // forward to file sink
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
	}

	m_settings = settings;
	return true;
}

airspyhf_device_t *AirspyHFInput::open_airspyhf_from_serial(const QString& serialStr)
{
    airspyhf_device_t *devinfo;
    bool ok;
    airspyhf_error rc;

    uint64_t serial = serialStr.toULongLong(&ok, 16);

    if (!ok)
    {
        qCritical("AirspyHFInput::open_airspyhf_from_serial: invalid serial %s", qPrintable(serialStr));
        return 0;
    }
    else
    {
        rc = (airspyhf_error) airspyhf_open_sn(&devinfo, serial);

        if (rc == AIRSPYHF_SUCCESS) {
            return devinfo;
        } else {
            return 0;
        }
    }
}

int AirspyHFInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage __attribute__((unused)))
{
    response.setAirspyHfSettings(new SWGSDRangel::SWGAirspyHFSettings());
    response.getAirspyHfSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int AirspyHFInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage __attribute__((unused)))
{
    AirspyHFSettings settings = m_settings;

    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getAirspyHfSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("devSampleRateIndex")) {
        settings.m_devSampleRateIndex = response.getAirspyHfSettings()->getDevSampleRateIndex();
    }
    if (deviceSettingsKeys.contains("LOppmTenths")) {
        settings.m_LOppmTenths = response.getAirspyHfSettings()->getLOppmTenths();
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getAirspyHfSettings()->getLog2Decim();
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency")) {
        settings.m_transverterDeltaFrequency = response.getAirspyHfSettings()->getTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("transverterMode")) {
        settings.m_transverterMode = response.getAirspyHfSettings()->getTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("bandIndex")) {
        settings.m_bandIndex = response.getAirspyHfSettings()->getBandIndex() != 0;
    }
    if (deviceSettingsKeys.contains("fileRecordName")) {
        settings.m_fileRecordName = *response.getAirspyHfSettings()->getFileRecordName();
    }

    MsgConfigureAirspyHF *msg = MsgConfigureAirspyHF::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAirspyHF *msgToGUI = MsgConfigureAirspyHF::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void AirspyHFInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const AirspyHFSettings& settings)
{
    response.getAirspyHfSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getAirspyHfSettings()->setDevSampleRateIndex(settings.m_devSampleRateIndex);
    response.getAirspyHfSettings()->setLOppmTenths(settings.m_LOppmTenths);
    response.getAirspyHfSettings()->setLog2Decim(settings.m_log2Decim);
    response.getAirspyHfSettings()->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    response.getAirspyHfSettings()->setTransverterMode(settings.m_transverterMode ? 1 : 0);
    response.getAirspyHfSettings()->setBandIndex(settings.m_bandIndex ? 1 : 0);

    if (response.getAirspyHfSettings()->getFileRecordName()) {
        *response.getAirspyHfSettings()->getFileRecordName() = settings.m_fileRecordName;
    } else {
        response.getAirspyHfSettings()->setFileRecordName(new QString(settings.m_fileRecordName));
    }
}

void AirspyHFInput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    response.getAirspyHfReport()->setSampleRates(new QList<SWGSDRangel::SWGSampleRate*>);

    for (std::vector<uint32_t>::const_iterator it = getSampleRates().begin(); it != getSampleRates().end(); ++it)
    {
        response.getAirspyHfReport()->getSampleRates()->append(new SWGSDRangel::SWGSampleRate);
        response.getAirspyHfReport()->getSampleRates()->back()->setRate(*it);
    }
}

int AirspyHFInput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage __attribute__((unused)))
{
    response.setAirspyHfReport(new SWGSDRangel::SWGAirspyHFReport());
    response.getAirspyHfReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

int AirspyHFInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int AirspyHFInput::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    MsgStartStop *message = MsgStartStop::create(run);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgStartStop *msgToGUI = MsgStartStop::create(run);
        m_guiMessageQueue->push(msgToGUI);
    }

    return 200;
}

