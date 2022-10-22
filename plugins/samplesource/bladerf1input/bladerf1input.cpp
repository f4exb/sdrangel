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

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspdevicesinkengine.h"
#include "dsp/dspengine.h"
#include "device/deviceapi.h"

#include "bladerf1input.h"
#include "bladerf1inputthread.h"

MESSAGE_CLASS_DEFINITION(Bladerf1Input::MsgConfigureBladerf1, Message)
MESSAGE_CLASS_DEFINITION(Bladerf1Input::MsgStartStop, Message)

Bladerf1Input::Bladerf1Input(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_dev(0),
	m_bladerfThread(nullptr),
	m_deviceDescription("BladeRFInput"),
	m_running(false)
{
    m_sampleFifo.setLabel(m_deviceDescription);
    openDevice();
    m_deviceAPI->setNbSourceStreams(1);
    m_deviceAPI->setBuddySharedPtr(&m_sharedParams);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &Bladerf1Input::networkManagerFinished
    );
}

Bladerf1Input::~Bladerf1Input()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &Bladerf1Input::networkManagerFinished
    );
    delete m_networkManager;

    if (m_running) {
        stop();
    }

    closeDevice();
    m_deviceAPI->setBuddySharedPtr(0);
}

void Bladerf1Input::destroy()
{
    delete this;
}

bool Bladerf1Input::openDevice()
{
    if (m_dev != 0)
    {
        closeDevice();
    }

    int res;

    if (!m_sampleFifo.setSize(96000 * 4))
    {
        qCritical("BladerfInput::openDevice: could not allocate SampleFifo");
        return false;
    }

    if (m_deviceAPI->getSinkBuddies().size() > 0)
    {
        DeviceAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
        DeviceBladeRF1Params *buddySharedParams = (DeviceBladeRF1Params *) sinkBuddy->getBuddySharedPtr();

        if (buddySharedParams == 0)
        {
            qCritical("BladerfInput::openDevice: could not get shared parameters from buddy");
            return false;
        }

        if (buddySharedParams->m_dev == 0) // device is not opened by buddy
        {
            qCritical("BladerfInput::openDevice: could not get BladeRF handle from buddy");
            return false;
        }

        m_sharedParams = *(buddySharedParams); // copy parameters from buddy
        m_dev = m_sharedParams.m_dev;          // get BladeRF handle
    }
    else
    {
        if (!DeviceBladeRF1::open_bladerf(&m_dev, qPrintable(m_deviceAPI->getSamplingDeviceSerial())))
        {
            qCritical("BladerfInput::start: could not open BladeRF %s", qPrintable(m_deviceAPI->getSamplingDeviceSerial()));
            return false;
        }

        m_sharedParams.m_dev = m_dev;
    }

    // TODO: adjust USB transfer data according to sample rate
    if ((res = bladerf_sync_config(m_dev, BLADERF_RX_X1, BLADERF_FORMAT_SC16_Q11, 64, 8192, 32, 10000)) < 0)
    {
        qCritical("BladerfInput::start: bladerf_sync_config with return code %d", res);
        return false;
    }

    if ((res = bladerf_enable_module(m_dev, BLADERF_MODULE_RX, true)) < 0)
    {
        qCritical("BladerfInput::start: bladerf_enable_module with return code %d", res);
        return false;
    }

    return true;
}

void Bladerf1Input::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

bool Bladerf1Input::start()
{
//	QMutexLocker mutexLocker(&m_mutex);

    if (!m_dev)
    {
        qDebug("BladerfInput::start: no device handle");
        return false;
    }

    if (m_running) stop();

	m_bladerfThread = new Bladerf1InputThread(m_dev, &m_sampleFifo);
	m_bladerfThread->setLog2Decimation(m_settings.m_log2Decim);
	m_bladerfThread->setFcPos((int) m_settings.m_fcPos);
    m_bladerfThread->setIQOrder(m_settings.m_iqOrder);

	m_bladerfThread->startWork();

//	mutexLocker.unlock();
	applySettings(m_settings, QList<QString>(), true);

	qDebug("BladerfInput::startInput: started");
	m_running = true;

	return true;
}

void Bladerf1Input::closeDevice()
{
    int res;

    if (m_dev == 0) { // was never open
        return;
    }

    if ((res = bladerf_enable_module(m_dev, BLADERF_MODULE_RX, false)) < 0)
    {
        qCritical("BladerfInput::stop: bladerf_enable_module with return code %d", res);
    }

    if (m_deviceAPI->getSinkBuddies().size() == 0)
    {
        qDebug("BladerfInput::closeDevice: closing device since Tx side is not open");

        if(m_dev != 0) // close BladeRF
        {
            bladerf_close(m_dev);
        }
    }

    m_sharedParams.m_dev = 0;
    m_dev = 0;
}

void Bladerf1Input::stop()
{
//	QMutexLocker mutexLocker(&m_mutex);

	if(m_bladerfThread)
	{
		m_bladerfThread->stopWork();
		delete m_bladerfThread;
		m_bladerfThread = nullptr;
	}

	m_running = false;
}

QByteArray Bladerf1Input::serialize() const
{
    return m_settings.serialize();
}

bool Bladerf1Input::deserialize(const QByteArray& data)
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

const QString& Bladerf1Input::getDeviceDescription() const
{
	return m_deviceDescription;
}

int Bladerf1Input::getSampleRate() const
{
	int rate = m_settings.m_devSampleRate;
	return (rate / (1<<m_settings.m_log2Decim));
}

quint64 Bladerf1Input::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void Bladerf1Input::setCenterFrequency(qint64 centerFrequency)
{
    BladeRF1InputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureBladerf1* message = MsgConfigureBladerf1::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureBladerf1* messageToGUI = MsgConfigureBladerf1::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool Bladerf1Input::handleMessage(const Message& message)
{
	if (MsgConfigureBladerf1::match(message))
	{
		MsgConfigureBladerf1& conf = (MsgConfigureBladerf1&) message;
		qDebug() << "Bladerf1Input::handleMessage: MsgConfigureBladerf1";

		if (!applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce())) {
			qDebug("BladeRF config error");
		}

		return true;
	}
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "BladerfInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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

bool Bladerf1Input::applySettings(const BladeRF1InputSettings& settings, const QList<QString>& settingsKeys, bool force)
{
	bool forwardChange = false;
//	QMutexLocker mutexLocker(&m_mutex);

	qDebug() << "BladerfInput::applySettings: force: " << force << settings.getDebugString(settingsKeys, force);

	if ((settingsKeys.contains("dcBlock")) ||
	    settingsKeys.contains("iqCorrection") || force)
	{
		m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
	}

	if (settingsKeys.contains("lnaGain") || force)
	{
		if (m_dev != 0)
		{
			if(bladerf_set_lna_gain(m_dev, getLnaGain(settings.m_lnaGain)) != 0) {
				qDebug("BladerfInput::applySettings: bladerf_set_lna_gain() failed");
			} else {
				qDebug() << "BladerfInput::applySettings: LNA gain set to " << getLnaGain(settings.m_lnaGain);
			}
		}
	}

	if (settingsKeys.contains("vga1") || force)
	{
		if (m_dev != 0)
		{
			if(bladerf_set_rxvga1(m_dev, settings.m_vga1) != 0) {
				qDebug("BladerfInput::applySettings: bladerf_set_rxvga1() failed");
			} else {
				qDebug() << "BladerfInput::applySettings: VGA1 gain set to " << settings.m_vga1;
			}
		}
	}

	if (settingsKeys.contains("vga2") || force)
	{
		if(m_dev != 0)
		{
			if(bladerf_set_rxvga2(m_dev, settings.m_vga2) != 0) {
				qDebug("BladerfInput::applySettings: bladerf_set_rxvga2() failed");
			} else {
				qDebug() << "BladerfInput::applySettings: VGA2 gain set to " << settings.m_vga2;
			}
		}
	}

	if (settingsKeys.contains("xb200") || force)
	{
		if (m_dev != 0)
		{
		    bool changeSettings;

		    if (m_deviceAPI->getSinkBuddies().size() > 0)
		    {
		        DeviceAPI *buddy = m_deviceAPI->getSinkBuddies()[0];

		        if (buddy->getDeviceSinkEngine()->state() == DSPDeviceSinkEngine::StRunning) { // Tx side running
		            changeSettings = false;
		        } else {
		            changeSettings = true;
		        }
		    }
		    else // No Tx open
		    {
                changeSettings = true;
		    }

		    if (changeSettings)
		    {
	            if (settings.m_xb200)
	            {
	                if (bladerf_expansion_attach(m_dev, BLADERF_XB_200) != 0) {
	                    qDebug("BladerfInput::applySettings: bladerf_expansion_attach(xb200) failed");
	                } else {
	                    qDebug() << "BladerfInput::applySettings: Attach XB200";
	                }
	            }
	            else
	            {
	                if (bladerf_expansion_attach(m_dev, BLADERF_XB_NONE) != 0) {
	                    qDebug("BladerfInput::applySettings: bladerf_expansion_attach(none) failed");
	                } else {
	                    qDebug() << "BladerfInput::applySettings: Detach XB200";
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
			if(bladerf_xb200_set_path(m_dev, BLADERF_MODULE_RX, settings.m_xb200Path) != 0) {
				qDebug("BladerfInput::applySettings: bladerf_xb200_set_path(BLADERF_MODULE_RX) failed");
			} else {
				qDebug() << "BladerfInput::applySettings: set xb200 path to " << settings.m_xb200Path;
			}
		}
	}

	if (settingsKeys.contains("xb200Filter") || force)
	{
		if (m_dev != 0)
		{
			if(bladerf_xb200_set_filterbank(m_dev, BLADERF_MODULE_RX, settings.m_xb200Filter) != 0) {
				qDebug("BladerfInput::applySettings: bladerf_xb200_set_filterbank(BLADERF_MODULE_RX) failed");
			} else {
				qDebug() << "BladerfInput::applySettings: set xb200 filter to " << settings.m_xb200Filter;
			}
		}
	}

	if (settingsKeys.contains("devSampleRate") || force)
	{
		forwardChange = true;

		if (m_dev != 0)
		{
			unsigned int actualSamplerate;

			if (bladerf_set_sample_rate(m_dev, BLADERF_MODULE_RX, settings.m_devSampleRate, &actualSamplerate) < 0) {
				qCritical("BladerfInput::applySettings: could not set sample rate: %d", settings.m_devSampleRate);
			} else {
				qDebug() << "BladerfInput::applySettings: bladerf_set_sample_rate(BLADERF_MODULE_RX) actual sample rate is " << actualSamplerate;
			}
		}
	}

	if (settingsKeys.contains("bandwidth") || force)
	{
		if(m_dev != 0)
		{
			unsigned int actualBandwidth;

			if( bladerf_set_bandwidth(m_dev, BLADERF_MODULE_RX, settings.m_bandwidth, &actualBandwidth) < 0) {
				qCritical("BladerfInput::applySettings: could not set bandwidth: %d", settings.m_bandwidth);
			} else {
				qDebug() << "BladerfInput::applySettings: bladerf_set_bandwidth(BLADERF_MODULE_RX) actual bandwidth is " << actualBandwidth;
			}
		}
	}

	if (settingsKeys.contains("fcPos") || force)
	{
		if (m_bladerfThread)
		{
			m_bladerfThread->setFcPos((int) settings.m_fcPos);
			qDebug() << "BladerfInput::applySettings: set fc pos (enum) to " << (int) settings.m_fcPos;
		}
	}

    if (settingsKeys.contains("log2Decim") || force)
    {
        forwardChange = true;

        if (m_bladerfThread)
        {
            m_bladerfThread->setLog2Decimation(settings.m_log2Decim);
            qDebug() << "BladerfInput::applySettings: set decimation to " << (1<<settings.m_log2Decim);
        }
    }

    if (settingsKeys.contains("iqOrder") || force)
    {
        if (m_bladerfThread) {
            m_bladerfThread->setIQOrder(settings.m_iqOrder);
        }
    }

    if (settingsKeys.contains("centerFrequency")
        || settingsKeys.contains("log2Decim")
        || settingsKeys.contains("fcPos")
        || settingsKeys.contains("devSampleRate") || force)
    {
        qint64 deviceCenterFrequency = DeviceSampleSource::calculateDeviceCenterFrequency(
                settings.m_centerFrequency,
                0,
                settings.m_log2Decim,
                (DeviceSampleSource::fcPos_t) settings.m_fcPos,
                settings.m_devSampleRate,
                DeviceSampleSource::FrequencyShiftScheme::FSHIFT_STD,
                false);

        forwardChange = true;

        if (m_dev != 0)
        {
            if (bladerf_set_frequency( m_dev, BLADERF_MODULE_RX, deviceCenterFrequency ) != 0) {
                qWarning("BladerfInput::applySettings: bladerf_set_frequency(%lld) failed", settings.m_centerFrequency);
            } else {
                qDebug("BladerfInput::applySettings: bladerf_set_frequency(%lld)", settings.m_centerFrequency);
            }
        }
    }

	if (forwardChange)
	{
		int sampleRate = settings.m_devSampleRate/(1<<settings.m_log2Decim);
		DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, settings.m_centerFrequency);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
	}

    if (settings.m_useReverseAPI)
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

	return true;
}

bladerf_lna_gain Bladerf1Input::getLnaGain(int lnaGain)
{
	if (lnaGain == 2) {
		return BLADERF_LNA_GAIN_MAX;
	} else if (lnaGain == 1) {
		return BLADERF_LNA_GAIN_MID;
	} else {
		return BLADERF_LNA_GAIN_BYPASS;
	}
}

int Bladerf1Input::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setBladeRf1InputSettings(new SWGSDRangel::SWGBladeRF1InputSettings());
    response.getBladeRf1InputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

void Bladerf1Input::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const BladeRF1InputSettings& settings)
{
    response.getBladeRf1InputSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getBladeRf1InputSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getBladeRf1InputSettings()->setLnaGain(settings.m_lnaGain);
    response.getBladeRf1InputSettings()->setVga1(settings.m_vga1);
    response.getBladeRf1InputSettings()->setVga2(settings.m_vga2);
    response.getBladeRf1InputSettings()->setBandwidth(settings.m_bandwidth);
    response.getBladeRf1InputSettings()->setLog2Decim(settings.m_log2Decim);
    response.getBladeRf1InputSettings()->setIqOrder(settings.m_iqOrder ? 1 : 0);
    response.getBladeRf1InputSettings()->setFcPos((int) settings.m_fcPos);
    response.getBladeRf1InputSettings()->setXb200(settings.m_xb200 ? 1 : 0);
    response.getBladeRf1InputSettings()->setXb200Path((int) settings.m_xb200Path);
    response.getBladeRf1InputSettings()->setXb200Filter((int) settings.m_xb200Filter);
    response.getBladeRf1InputSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getBladeRf1InputSettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);

    response.getBladeRf1InputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getBladeRf1InputSettings()->getReverseApiAddress()) {
        *response.getBladeRf1InputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getBladeRf1InputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getBladeRf1InputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getBladeRf1InputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

int Bladerf1Input::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    BladeRF1InputSettings settings = m_settings;
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

void Bladerf1Input::webapiUpdateDeviceSettings(
        BladeRF1InputSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getBladeRf1InputSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getBladeRf1InputSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("lnaGain")) {
        settings.m_lnaGain = response.getBladeRf1InputSettings()->getLnaGain();
    }
    if (deviceSettingsKeys.contains("vga1")) {
        settings.m_vga1 = response.getBladeRf1InputSettings()->getVga1();
    }
    if (deviceSettingsKeys.contains("vga2")) {
        settings.m_vga2 = response.getBladeRf1InputSettings()->getVga2();
    }
    if (deviceSettingsKeys.contains("bandwidth")) {
        settings.m_bandwidth = response.getBladeRf1InputSettings()->getBandwidth();
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getBladeRf1InputSettings()->getLog2Decim();
    }
    if (deviceSettingsKeys.contains("iqOrder")) {
        settings.m_iqOrder = response.getBladeRf1InputSettings()->getIqOrder() != 0;
    }
    if (deviceSettingsKeys.contains("fcPos")) {
        settings.m_fcPos = static_cast<BladeRF1InputSettings::fcPos_t>(response.getBladeRf1InputSettings()->getFcPos());
    }
    if (deviceSettingsKeys.contains("xb200")) {
        settings.m_xb200 = response.getBladeRf1InputSettings()->getXb200() == 0 ? 0 : 1;
    }
    if (deviceSettingsKeys.contains("xb200Path")) {
        settings.m_xb200Path = static_cast<bladerf_xb200_path>(response.getBladeRf1InputSettings()->getXb200Path());
    }
    if (deviceSettingsKeys.contains("xb200Filter")) {
        settings.m_xb200Filter = static_cast<bladerf_xb200_filter>(response.getBladeRf1InputSettings()->getXb200Filter());
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getBladeRf1InputSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getBladeRf1InputSettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getBladeRf1InputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getBladeRf1InputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getBladeRf1InputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getBladeRf1InputSettings()->getReverseApiDeviceIndex();
    }
}

int Bladerf1Input::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int Bladerf1Input::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
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

void Bladerf1Input::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const BladeRF1InputSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("BladeRF1"));
    swgDeviceSettings->setBladeRf1InputSettings(new SWGSDRangel::SWGBladeRF1InputSettings());
    SWGSDRangel::SWGBladeRF1InputSettings *swgBladeRF1Settings = swgDeviceSettings->getBladeRf1InputSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        swgBladeRF1Settings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("devSampleRate") || force) {
        swgBladeRF1Settings->setDevSampleRate(settings.m_devSampleRate);
    }
    if (deviceSettingsKeys.contains("lnaGain") || force) {
        swgBladeRF1Settings->setLnaGain(settings.m_lnaGain);
    }
    if (deviceSettingsKeys.contains("vga1") || force) {
        swgBladeRF1Settings->setVga1(settings.m_vga1);
    }
    if (deviceSettingsKeys.contains("vga2") || force) {
        swgBladeRF1Settings->setVga1(settings.m_vga2);
    }
    if (deviceSettingsKeys.contains("bandwidth") || force) {
        swgBladeRF1Settings->setBandwidth(settings.m_bandwidth);
    }
    if (deviceSettingsKeys.contains("log2Decim") || force) {
        swgBladeRF1Settings->setLog2Decim(settings.m_log2Decim);
    }
    if (deviceSettingsKeys.contains("iqOrder") || force) {
        swgBladeRF1Settings->setIqOrder(settings.m_iqOrder ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("fcPos") || force) {
        swgBladeRF1Settings->setFcPos((int) settings.m_fcPos);
    }
    if (deviceSettingsKeys.contains("xb200") || force) {
        swgBladeRF1Settings->setXb200(settings.m_xb200 ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("xb200Path") || force) {
        swgBladeRF1Settings->setXb200Path((int) settings.m_xb200Path);
    }
    if (deviceSettingsKeys.contains("xb200Filter") || force) {
        swgBladeRF1Settings->setXb200Filter((int) settings.m_xb200Filter);
    }
    if (deviceSettingsKeys.contains("dcBlock") || force) {
        swgBladeRF1Settings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("iqCorrection") || force) {
        swgBladeRF1Settings->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
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

void Bladerf1Input::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
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

void Bladerf1Input::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "Bladerf1Input::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("Bladerf1Input::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
