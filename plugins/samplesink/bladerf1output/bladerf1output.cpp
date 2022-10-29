///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#include <string.h>
#include <errno.h>

#include <QDebug>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"

#include "dsp/dspcommands.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspdevicesinkengine.h"
#include "dsp/dspengine.h"
#include "device/deviceapi.h"
#include "bladerf1/devicebladerf1shared.h"
#include "bladerf1outputthread.h"
#include "bladerf1output.h"

MESSAGE_CLASS_DEFINITION(Bladerf1Output::MsgConfigureBladerf1, Message)
MESSAGE_CLASS_DEFINITION(Bladerf1Output::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(Bladerf1Output::MsgReportBladerf1, Message)

Bladerf1Output::Bladerf1Output(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_dev(nullptr),
	m_bladerfThread(nullptr),
	m_deviceDescription("BladeRFOutput"),
	m_running(false)
{
    m_sampleSourceFifo.resize(SampleSourceFifo::getSizePolicy(m_settings.m_devSampleRate));
    openDevice();
    m_deviceAPI->setNbSinkStreams(1);
    m_deviceAPI->setBuddySharedPtr(&m_sharedParams);
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &Bladerf1Output::networkManagerFinished
    );
}

Bladerf1Output::~Bladerf1Output()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &Bladerf1Output::networkManagerFinished
    );
    delete m_networkManager;

    if (m_running) {
        stop();
    }

    closeDevice();
    m_deviceAPI->setBuddySharedPtr(0);
}

void Bladerf1Output::destroy()
{
    delete this;
}

bool Bladerf1Output::openDevice()
{
    if (m_dev != 0) {
        closeDevice();
    }

    int res;

    m_sampleSourceFifo.resize(SampleSourceFifo::getSizePolicy(m_settings.m_devSampleRate));

    if (m_deviceAPI->getSourceBuddies().size() > 0)
    {
        DeviceAPI *sourceBuddy = m_deviceAPI->getSourceBuddies()[0];
        DeviceBladeRF1Params *buddySharedParams = (DeviceBladeRF1Params *) sourceBuddy->getBuddySharedPtr();

        if (buddySharedParams == 0)
        {
            qCritical("BladerfOutput::start: could not get shared parameters from buddy");
            return false;
        }

        if (buddySharedParams->m_dev == 0) // device is not opened by buddy
        {
            qCritical("BladerfOutput::start: could not get BladeRF handle from buddy");
            return false;
        }

        m_sharedParams = *(buddySharedParams); // copy parameters from buddy
        m_dev = m_sharedParams.m_dev;          // get BladeRF handle
    }
    else
    {
        if (!DeviceBladeRF1::open_bladerf(&m_dev, qPrintable(m_deviceAPI->getSamplingDeviceSerial())))
        {
            qCritical("BladerfOutput::start: could not open BladeRF %s", qPrintable(m_deviceAPI->getSamplingDeviceSerial()));
            return false;
        }

        m_sharedParams.m_dev = m_dev;
    }

    // TODO: adjust USB transfer data according to sample rate
    if ((res = bladerf_sync_config(m_dev, BLADERF_TX_X1, BLADERF_FORMAT_SC16_Q11, 64, 8192, 32, 10000)) < 0)
    {
        qCritical("BladerfOutput::start: bladerf_sync_config with return code %d", res);
        return false;
    }

    if ((res = bladerf_enable_module(m_dev, BLADERF_MODULE_TX, true)) < 0)
    {
        qCritical("BladerfOutput::start: bladerf_enable_module with return code %d", res);
        return false;
    }

    return true;
}

void Bladerf1Output::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

bool Bladerf1Output::start()
{
//	QMutexLocker mutexLocker(&m_mutex);

    if (!m_dev) {
        return false;
    }

    if (m_running) stop();

    m_bladerfThread = new Bladerf1OutputThread(m_dev, &m_sampleSourceFifo);

//	mutexLocker.unlock();
	applySettings(m_settings, QList<QString>(), true);

	m_bladerfThread->setLog2Interpolation(m_settings.m_log2Interp);

    m_bladerfThread->startWork();

	qDebug("BladerfOutput::start: started");
    m_running = true;

    return true;
}

void Bladerf1Output::closeDevice()
{
    int res;

    if (m_dev == 0) { // was never open
        return;
    }

    if ((res = bladerf_enable_module(m_dev, BLADERF_MODULE_TX, false)) < 0)
    {
        qCritical("BladerfOutput::closeDevice: bladerf_enable_module with return code %d", res);
    }

    if (m_deviceAPI->getSourceBuddies().size() == 0)
    {
        qDebug("BladerfOutput::closeDevice: closing device since Rx side is not open");

        if (m_dev != 0) // close BladeRF
        {
            bladerf_close(m_dev);
        }
    }

    m_sharedParams.m_dev = 0;
    m_dev = 0;
}

void Bladerf1Output::stop()
{
//	QMutexLocker mutexLocker(&m_mutex);
    if (m_bladerfThread != 0)
	{
		m_bladerfThread->stopWork();
		delete m_bladerfThread;
		m_bladerfThread = 0;
	}

    m_running = false;
}

QByteArray Bladerf1Output::serialize() const
{
    return m_settings.serialize();
}

bool Bladerf1Output::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureBladerf1* message = MsgConfigureBladerf1::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureBladerf1* messageToGUI = MsgConfigureBladerf1::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& Bladerf1Output::getDeviceDescription() const
{
	return m_deviceDescription;
}

int Bladerf1Output::getSampleRate() const
{
	int rate = m_settings.m_devSampleRate;
	return (rate / (1<<m_settings.m_log2Interp));
}

quint64 Bladerf1Output::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void Bladerf1Output::setCenterFrequency(qint64 centerFrequency)
{
    BladeRF1OutputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureBladerf1* message = MsgConfigureBladerf1::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureBladerf1* messageToGUI = MsgConfigureBladerf1::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool Bladerf1Output::handleMessage(const Message& message)
{
	if (MsgConfigureBladerf1::match(message))
	{
		MsgConfigureBladerf1& conf = (MsgConfigureBladerf1&) message;
		qDebug() << "BladerfOutput::handleMessage: MsgConfigureBladerf";

		if (!applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce())) {
			qDebug("BladeRF config error");
		}

		return true;
	}
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "BladerfOutput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initDeviceEngine()) {
                m_deviceAPI->startDeviceEngine();
            }
        }
        else
        {
            m_deviceAPI->stopDeviceEngine();
        }

        if (m_settings.m_useReverseAPI) {
            webapiReverseSendStartStop(cmd.getStartStop());
        }

        return true;
    }
	else
	{
		return false;
	}
}

bool Bladerf1Output::applySettings(const BladeRF1OutputSettings& settings, const QList<QString>& settingsKeys, bool force)
{
	qDebug() << "BladerfOutput::applySettings: force:" << force << settings.getDebugString(settingsKeys, force);
	bool forwardChange    = false;
    bool suspendOwnThread = false;
    bool threadWasRunning = false;
//	QMutexLocker mutexLocker(&m_mutex);

    if (settingsKeys.contains("devSampleRate") ||
        settingsKeys.contains("log2Interp") || force)
    {
        suspendOwnThread = true;
    }

    if (suspendOwnThread)
    {
        if (m_bladerfThread)
        {
            if (m_bladerfThread->isRunning())
            {
                m_bladerfThread->stopWork();
                threadWasRunning = true;
            }
        }
    }

	if (settingsKeys.contains("devSampleRate") ||
        settingsKeys.contains("log2Interp") || force)
	{
#if defined(_MSC_VER)
        unsigned int fifoRate = (unsigned int) settings.m_devSampleRate / (1<<settings.m_log2Interp);
        fifoRate = fifoRate < 48000U ? 48000U : fifoRate;
#else
        unsigned int fifoRate = std::max(
            (unsigned int) settings.m_devSampleRate / (1<<settings.m_log2Interp),
            DeviceBladeRF1Shared::m_sampleFifoMinRate);
#endif
        m_sampleSourceFifo.resize(SampleSourceFifo::getSizePolicy(fifoRate));
	}

    if ((m_settings.m_devSampleRate != settings.m_devSampleRate) || force)
    {
        forwardChange = true;

        if (m_dev != 0)
        {
            unsigned int actualSamplerate;

            if (bladerf_set_sample_rate(m_dev, BLADERF_MODULE_TX, settings.m_devSampleRate, &actualSamplerate) < 0) {
                qCritical("BladerfOutput::applySettings: could not set sample rate: %d", settings.m_devSampleRate);
            } else {
                qDebug() << "BladerfOutput::applySettings: bladerf_set_sample_rate(BLADERF_MODULE_TX) actual sample rate is " << actualSamplerate;
            }
        }
    }

    if (settingsKeys.contains("log2Interp") || force)
    {
        forwardChange = true;

        if (m_bladerfThread != 0)
        {
            m_bladerfThread->setLog2Interpolation(settings.m_log2Interp);
            qDebug() << "BladerfOutput::applySettings: set interpolation to " << (1<<settings.m_log2Interp);
        }
    }

	if (settingsKeys.contains("vga1") || force)
	{
		if (m_dev != 0)
		{
			if (bladerf_set_txvga1(m_dev, settings.m_vga1) != 0) {
				qDebug("BladerfOutput::applySettings: bladerf_set_txvga1() failed");
			} else {
				qDebug() << "BladerfOutput::applySettings: VGA1 gain set to " << settings.m_vga1;
			}
		}
	}

	if (settingsKeys.contains("vga2") || force)
	{
		if(m_dev != 0)
		{
			if (bladerf_set_txvga2(m_dev, settings.m_vga2) != 0) {
				qDebug("BladerfOutput::applySettings:bladerf_set_rxvga2() failed");
			} else {
				qDebug() << "BladerfOutput::applySettings: VGA2 gain set to " << settings.m_vga2;
			}
		}
	}

	if (settingsKeys.contains("xb200") || force)
	{
		if (m_dev != 0)
		{
            bool changeSettings;

            if (m_deviceAPI->getSourceBuddies().size() > 0)
            {
                DeviceAPI *buddy = m_deviceAPI->getSourceBuddies()[0];

                if (buddy->getDeviceSourceEngine()->state() == DSPDeviceSourceEngine::StRunning) { // Tx side running
                    changeSettings = false;
                } else {
                    changeSettings = true;
                }
            }
            else // No Rx open
            {
                changeSettings = true;
            }

            if (changeSettings)
            {
                if (settings.m_xb200)
                {
                    if (bladerf_expansion_attach(m_dev, BLADERF_XB_200) != 0) {
                        qDebug("BladerfOutput::applySettings: bladerf_expansion_attach(xb200) failed");
                    } else {
                        qDebug() << "BladerfOutput::applySettings: Attach XB200";
                    }
                }
                else
                {
                    if (bladerf_expansion_attach(m_dev, BLADERF_XB_NONE) != 0) {
                        qDebug("BladerfOutput::applySettings: bladerf_expansion_attach(none) failed");
                    } else {
                        qDebug() << "BladerfOutput::applySettings: Detach XB200";
                    }
                }

                m_sharedParams.m_xb200Attached = settings.m_xb200;
            }
        }
	}

	if (settingsKeys.contains("xb200Path") || force)
	{
		if (m_dev != 0)
		{
			if (bladerf_xb200_set_path(m_dev, BLADERF_MODULE_TX, settings.m_xb200Path) != 0) {
				qDebug("BladerfOutput::applySettings: bladerf_xb200_set_path(BLADERF_MODULE_TX) failed");
			} else {
				qDebug() << "BladerfOutput::applySettings: set xb200 path to " << settings.m_xb200Path;
			}
		}
	}

	if (settingsKeys.contains("xb200Filter") || force)
	{
		if (m_dev != 0)
		{
			if (bladerf_xb200_set_filterbank(m_dev, BLADERF_MODULE_TX, settings.m_xb200Filter) != 0) {
				qDebug("BladerfOutput::applySettings: bladerf_xb200_set_filterbank(BLADERF_MODULE_TX) failed");
			} else {
				qDebug() << "BladerfOutput::applySettings: set xb200 filter to " << settings.m_xb200Filter;
			}
		}
	}

	if (settingsKeys.contains("bandwidth") || force)
	{
		if (m_dev != 0)
		{
			unsigned int actualBandwidth;

			if (bladerf_set_bandwidth(m_dev, BLADERF_MODULE_TX, settings.m_bandwidth, &actualBandwidth) < 0) {
				qCritical("BladerfOutput::applySettings: could not set bandwidth: %d", settings.m_bandwidth);
			} else {
				qDebug() << "BladerfOutput::applySettings: bladerf_set_bandwidth(BLADERF_MODULE_TX) actual bandwidth is " << actualBandwidth;
			}
		}
	}

	if (settingsKeys.contains("centerFrequency"))
    {
		forwardChange = true;

        if (m_dev != 0)
        {
            if (bladerf_set_frequency( m_dev, BLADERF_MODULE_TX, settings.m_centerFrequency ) != 0) {
                qDebug("BladerfOutput::applySettings: bladerf_set_frequency(%lld) failed", settings.m_centerFrequency);
            }
        }
	}

    if (threadWasRunning)
    {
        m_bladerfThread->startWork();
    }

    if (settingsKeys.contains("useReverseAPI"))
    {
        bool fullUpdate = (settingsKeys.contains("useReverseAPI") && settings.m_useReverseAPI) ||
            settingsKeys.contains("reverseAPIAddress") ||
            settingsKeys.contains("reverseAPIPort") ||
            settingsKeys.contains("reverseAPIDeviceIndex");
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

	if (forwardChange)
	{
		int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2Interp);
		DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
		m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
	}

	return true;
}

int Bladerf1Output::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setBladeRf1OutputSettings(new SWGSDRangel::SWGBladeRF1OutputSettings());
    response.getBladeRf1OutputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

void Bladerf1Output::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const BladeRF1OutputSettings& settings)
{
    response.getBladeRf1OutputSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getBladeRf1OutputSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getBladeRf1OutputSettings()->setVga1(settings.m_vga1);
    response.getBladeRf1OutputSettings()->setVga2(settings.m_vga2);
    response.getBladeRf1OutputSettings()->setBandwidth(settings.m_bandwidth);
    response.getBladeRf1OutputSettings()->setLog2Interp(settings.m_log2Interp);
    response.getBladeRf1OutputSettings()->setXb200(settings.m_xb200 ? 1 : 0);
    response.getBladeRf1OutputSettings()->setXb200Path((int) settings.m_xb200Path);
    response.getBladeRf1OutputSettings()->setXb200Filter((int) settings.m_xb200Filter);

    response.getBladeRf1OutputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getBladeRf1OutputSettings()->getReverseApiAddress()) {
        *response.getBladeRf1OutputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getBladeRf1OutputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getBladeRf1OutputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getBladeRf1OutputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

int Bladerf1Output::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    BladeRF1OutputSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureBladerf1 *msg = MsgConfigureBladerf1::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureBladerf1 *msgToGUI = MsgConfigureBladerf1::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void Bladerf1Output::webapiUpdateDeviceSettings(
    BladeRF1OutputSettings& settings,
    const QStringList& deviceSettingsKeys,
    SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getBladeRf1OutputSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getBladeRf1OutputSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("vga1")) {
        settings.m_vga1 = response.getBladeRf1OutputSettings()->getVga1();
    }
    if (deviceSettingsKeys.contains("vga2")) {
        settings.m_vga2 = response.getBladeRf1OutputSettings()->getVga2();
    }
    if (deviceSettingsKeys.contains("bandwidth")) {
        settings.m_bandwidth = response.getBladeRf1OutputSettings()->getBandwidth();
    }
    if (deviceSettingsKeys.contains("log2Interp")) {
        settings.m_log2Interp = response.getBladeRf1OutputSettings()->getLog2Interp();
    }
    if (deviceSettingsKeys.contains("xb200")) {
        settings.m_xb200 = response.getBladeRf1OutputSettings()->getXb200() == 0 ? 0 : 1;
    }
    if (deviceSettingsKeys.contains("xb200Path")) {
        settings.m_xb200Path = static_cast<bladerf_xb200_path>(response.getBladeRf1OutputSettings()->getXb200Path());
    }
    if (deviceSettingsKeys.contains("xb200Filter")) {
        settings.m_xb200Filter = static_cast<bladerf_xb200_filter>(response.getBladeRf1OutputSettings()->getXb200Filter());
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getBladeRf1OutputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getBladeRf1OutputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getBladeRf1OutputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getBladeRf1OutputSettings()->getReverseApiDeviceIndex();
    }
}

int Bladerf1Output::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int Bladerf1Output::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    MsgStartStop *message = MsgStartStop::create(run);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgStartStop *messagetoGui = MsgStartStop::create(run);
        m_guiMessageQueue->push(messagetoGui);
    }

    return 200;
}

void Bladerf1Output::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const BladeRF1OutputSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(1); // single Tx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("BladeRF1"));
    swgDeviceSettings->setBladeRf1OutputSettings(new SWGSDRangel::SWGBladeRF1OutputSettings());
    SWGSDRangel::SWGBladeRF1OutputSettings *swgBladeRF1OutputSettings = swgDeviceSettings->getBladeRf1OutputSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        swgBladeRF1OutputSettings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("devSampleRate") || force) {
        swgBladeRF1OutputSettings->setDevSampleRate(settings.m_devSampleRate);
    }
    if (deviceSettingsKeys.contains("vga1") || force) {
        swgBladeRF1OutputSettings->setVga1(settings.m_vga1);
    }
    if (deviceSettingsKeys.contains("vga2") || force) {
        swgBladeRF1OutputSettings->setVga2(settings.m_vga2);
    }
    if (deviceSettingsKeys.contains("bandwidth") || force) {
        swgBladeRF1OutputSettings->setBandwidth(settings.m_bandwidth);
    }
    if (deviceSettingsKeys.contains("log2Interp") || force) {
        swgBladeRF1OutputSettings->setLog2Interp(settings.m_log2Interp);
    }
    if (deviceSettingsKeys.contains("xb200") || force) {
        swgBladeRF1OutputSettings->setXb200(settings.m_xb200 ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("xb200Path") || force) {
        swgBladeRF1OutputSettings->setXb200Path((int) settings.m_xb200Path);
    }
    if (deviceSettingsKeys.contains("xb200Filter") || force) {
        swgBladeRF1OutputSettings->setXb200Filter((int) settings.m_xb200Filter);
    }

    QString deviceSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(deviceSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgDeviceSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);

    delete swgDeviceSettings;
}

void Bladerf1Output::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(1); // single Tx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("BladeRF1"));

    QString deviceSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/run")
            .arg(m_settings.m_reverseAPIAddress)
            .arg(m_settings.m_reverseAPIPort)
            .arg(m_settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(deviceSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgDeviceSettings->asJson().toUtf8());
    buffer->seek(0);
    QNetworkReply *reply;

    if (start) {
        reply = m_networkManager->sendCustomRequest(m_networkRequest, "POST", buffer);
    } else {
        reply = m_networkManager->sendCustomRequest(m_networkRequest, "DELETE", buffer);
    }

    buffer->setParent(reply);
    delete swgDeviceSettings;
}

void Bladerf1Output::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "Bladerf1Output::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("Bladerf1Output::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
