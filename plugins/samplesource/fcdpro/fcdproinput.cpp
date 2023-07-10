///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2018 Edouard Griffiths, F4EXB                              //
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
#include "dsp/dspengine.h"
#include "device/deviceapi.h"

#include "fcdproinput.h"
#include "fcdprothread.h"
#include "fcdtraits.h"
#include "fcdproconst.h"

MESSAGE_CLASS_DEFINITION(FCDProInput::MsgConfigureFCDPro, Message)
MESSAGE_CLASS_DEFINITION(FCDProInput::MsgStartStop, Message)

FCDProInput::FCDProInput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_dev(nullptr),
	m_settings(),
	m_FCDThread(nullptr),
	m_deviceDescription(fcd_traits<Pro>::displayedName),
	m_running(false)
{
    m_sampleFifo.setLabel(m_deviceDescription);
    m_fcdFIFO.setSize(20*fcd_traits<Pro>::convBufSize);
    openDevice();
    m_deviceAPI->setNbSourceStreams(1);
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &FCDProInput::networkManagerFinished
    );
}

FCDProInput::~FCDProInput()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &FCDProInput::networkManagerFinished
    );
    delete m_networkManager;

    if (m_running) {
        stop();
    }

    closeDevice();
}

void FCDProInput::destroy()
{
    delete this;
}

bool FCDProInput::openDevice()
{
    if (m_dev != 0) {
        closeDevice();
    }

    int device = m_deviceAPI->getSamplingDeviceSequence();
    qDebug() << "FCDProInput::openDevice with device #" << device;
    m_dev = fcdOpen(fcd_traits<Pro>::vendorId, fcd_traits<Pro>::productId, device);

    if (m_dev == 0)
    {
        qCritical("FCDProInput::start: could not open FCD");
        return false;
    }

    if (!openFCDAudio(fcd_traits<Pro>::qtDeviceName))
    {
        qCritical("FCDProInput::start: could not open FCD audio source");
        return false;
    }
    else
    {
        qDebug("FCDProInput::start: FCD audio source opened");
    }

    return true;
}

void FCDProInput::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

bool FCDProInput::start()
{
	qDebug() << "FCDProInput::start";

//	QMutexLocker mutexLocker(&m_mutex);

    if (!m_dev) {
        return false;
    }

    if (m_running) stop();

	/* Apply settings before streaming to avoid bus contention;
	 * there is very little spare bandwidth on a full speed USB device.
	 * Failure is harmless if no device is found
	 * ... This is rubbish...*/

	//applySettings(m_settings, true);

	if(!m_sampleFifo.setSize(96000*4))
	{
		qCritical("Could not allocate SampleFifo");
		return false;
	}

	m_FCDThread = new FCDProThread(&m_sampleFifo, &m_fcdFIFO);
    m_FCDThread->setLog2Decimation(m_settings.m_log2Decim);
    m_FCDThread->setFcPos(m_settings.m_fcPos);
    m_FCDThread->setIQOrder(m_settings.m_iqOrder);
	m_FCDThread->startWork();

//	mutexLocker.unlock();
	applySettings(m_settings, QList<QString>(), true);

	qDebug("FCDProInput::started");
	m_running = true;

	return true;
}

void FCDProInput::closeDevice()
{
    if (m_dev == 0) { // was never open
        return;
    }

    fcdClose(m_dev);
    m_dev = 0;

	closeFCDAudio();
}

bool FCDProInput::openFCDAudio(const char* cardname)
{
    AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
    const QList<AudioDeviceInfo>& audioList = audioDeviceManager->getInputDevices();

    for (const auto &itAudio : audioList)
    {
        if (itAudio.deviceName().contains(QString(cardname)))
        {
            int fcdDeviceIndex = audioDeviceManager->getInputDeviceIndex(itAudio.deviceName());
            AudioDeviceManager::InputDeviceInfo fcdDeviceInfo;
            audioDeviceManager->getInputDeviceInfo(itAudio.deviceName(), fcdDeviceInfo);
            fcdDeviceInfo.sampleRate = fcd_traits<Pro>::sampleRate;
            audioDeviceManager->setInputDeviceInfo(fcdDeviceIndex, fcdDeviceInfo);
            audioDeviceManager->addAudioSource(&m_fcdFIFO, getInputMessageQueue(), fcdDeviceIndex);
            qDebug("FCDProPlusInput::openFCDAudio: %s index %d",
                    itAudio.deviceName().toStdString().c_str(), fcdDeviceIndex);
            return true;
        }
    }

    qCritical("FCDProInput::openFCDAudio: device with name %s not found", cardname);
    return false;
}

void FCDProInput::closeFCDAudio()
{
    AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
    audioDeviceManager->removeAudioSource(&m_fcdFIFO);
}

void FCDProInput::stop()
{
//	QMutexLocker mutexLocker(&m_mutex);

	if (m_FCDThread)
	{
		m_FCDThread->stopWork();
		// wait for thread to quit ?
		delete m_FCDThread;
		m_FCDThread = nullptr;
	}

	m_running = false;
}

QByteArray FCDProInput::serialize() const
{
    return m_settings.serialize();
}

bool FCDProInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureFCDPro* message = MsgConfigureFCDPro::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureFCDPro* messageToGUI = MsgConfigureFCDPro::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& FCDProInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int FCDProInput::getSampleRate() const
{
	return fcd_traits<Pro>::sampleRate/(1<<m_settings.m_log2Decim);
}

quint64 FCDProInput::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void FCDProInput::setCenterFrequency(qint64 centerFrequency)
{
    FCDProSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureFCDPro* message = MsgConfigureFCDPro::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureFCDPro* messageToGUI = MsgConfigureFCDPro::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool FCDProInput::handleMessage(const Message& message)
{
	if(MsgConfigureFCDPro::match(message))
	{
		qDebug() << "FCDProInput::handleMessage: MsgConfigureFCD";
		MsgConfigureFCDPro& conf = (MsgConfigureFCDPro&) message;
		applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());
		return true;
	}
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "FCDProInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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

void FCDProInput::applySettings(const FCDProSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "FCDProInput::applySettings: force: " << force << settings.getDebugString(settingsKeys, force);
	bool forwardChange = false;

	if (force || settingsKeys.contains("centerFrequency")
        || settingsKeys.contains("LOppmTenths")
        || settingsKeys.contains("fcPos")
        || settingsKeys.contains("log2Decim")
        || settingsKeys.contains("transverterMode")
        || settingsKeys.contains("transverterDeltaFrequency"))
	{
        qint64 deviceCenterFrequency = DeviceSampleSource::calculateDeviceCenterFrequency(
                settings.m_centerFrequency,
                settings.m_transverterDeltaFrequency,
                settings.m_log2Decim,
                (DeviceSampleSource::fcPos_t) settings.m_fcPos,
                fcd_traits<Pro>::sampleRate,
                DeviceSampleSource::FrequencyShiftScheme::FSHIFT_STD,
                settings.m_transverterMode);

        if (m_dev != 0) {
            set_center_freq((double) deviceCenterFrequency);
        }

        qDebug() << "FCDProInput::applySettings: center freq: " << settings.m_centerFrequency << " Hz"
                << " device center freq: " << deviceCenterFrequency << " Hz";

		forwardChange = (m_settings.m_centerFrequency != settings.m_centerFrequency) || force;

		m_settings.m_centerFrequency = settings.m_centerFrequency;
	}

	if (settingsKeys.contains("log2Decim") || force)
	{
		forwardChange = true;

		if (m_FCDThread)
		{
		    m_FCDThread->setLog2Decimation(settings.m_log2Decim);
			qDebug() << "FCDProInput::applySettings: set decimation to " << (1<<settings.m_log2Decim);
		}
	}

    if (settingsKeys.contains("fcPos") || force)
    {
        if (m_FCDThread) {
            m_FCDThread->setFcPos((int) settings.m_fcPos);
        }

        qDebug() << "FCDProInput::applySettings: set fc pos (enum) to " << (int) settings.m_fcPos;
    }

    if (settingsKeys.contains("iqOrder") || force)
    {
        if (m_FCDThread) {
            m_FCDThread->setIQOrder((int) settings.m_iqOrder);
        }

        qDebug() << "FCDProInput::applySettings: set IQ order to %s" << (settings.m_iqOrder ? "IQ" : "QI");
    }

	if (settingsKeys.contains("lnaGainIndex") || force)
	{
		if (m_dev != 0) {
			set_lnaGain(settings.m_lnaGainIndex);
		}
	}

	if (settingsKeys.contains("rfFilterIndex") || force)
	{
		if (m_dev != 0) {
			set_rfFilter(settings.m_rfFilterIndex);
		}
	}

	if (settingsKeys.contains("lnaEnhanceIndex") || force)
	{
		if (m_dev != 0) {
			set_lnaEnhance(settings.m_lnaEnhanceIndex);
		}
	}

	if (settingsKeys.contains("bandIndex") || force)
	{
		if (m_dev != 0) {
			set_band(settings.m_bandIndex);
		}
	}

	if (settingsKeys.contains("mixerGainIndex") || force)
	{
		if (m_dev != 0) {
			set_mixerGain(settings.m_mixerGainIndex);
		}
	}

	if (settingsKeys.contains("mixerFilterIndex") || force)
	{
		if (m_dev != 0) {
			set_mixerFilter(settings.m_mixerFilterIndex);
		}
	}

	if (settingsKeys.contains("biasCurrentIndex") || force)
	{
		if (m_dev != 0) {
			set_biasCurrent(settings.m_biasCurrentIndex);
		}
	}

	if (settingsKeys.contains("modeIndex") || force)
	{
		if (m_dev != 0) {
			set_mode(settings.m_modeIndex);
		}
	}

	if (settingsKeys.contains("gain1Index") || force)
	{
		if (m_dev != 0) {
			set_gain1(settings.m_gain1Index);
		}
	}

	if (settingsKeys.contains("rcFilterIndex") || force)
	{
		if (m_dev != 0) {
			set_rcFilter(settings.m_rcFilterIndex);
		}
	}

	if (settingsKeys.contains("gain2Index") || force)
	{
		if (m_dev != 0) {
			set_gain2(settings.m_gain2Index);
		}
	}

	if (settingsKeys.contains("gain3Index") || force)
	{
		if (m_dev != 0) {
			set_gain3(settings.m_gain3Index);
		}
	}

	if (settingsKeys.contains("gain4Index") || force)
	{
		if (m_dev != 0) {
			set_gain4(settings.m_gain4Index);
		}
	}

	if (settingsKeys.contains("ifFilterIndex") || force)
	{
		if (m_dev != 0) {
			set_ifFilter(settings.m_ifFilterIndex);
		}
	}

	if (settingsKeys.contains("gain5Index") || force)
	{
		if (m_dev != 0) {
			set_gain5(settings.m_gain5Index);
		}
	}

	if (settingsKeys.contains("gain6Index") || force)
	{
		if (m_dev != 0) {
			set_gain6(settings.m_gain6Index);
		}
	}

	if (settingsKeys.contains("dcBlock") || force)
	{
		m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
	}

	if (settingsKeys.contains("iqCorrection") || force)
	{
		m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
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
		DSPSignalNotification *notif = new DSPSignalNotification(fcd_traits<Pro>::sampleRate/(1<<m_settings.m_log2Decim), m_settings.m_centerFrequency);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }
}

void FCDProInput::set_center_freq(double freq)
{
	freq += freq*(((double) m_settings.m_LOppmTenths)/10000000.0);

	if (fcdAppSetFreq(m_dev, freq) == FCD_MODE_NONE)
	{
		qDebug("No FCD HID found for frquency change");
	}

}

void FCDProInput::set_bias_t(bool on)
{
    (void) on;
	//quint8 cmd = on ? 1 : 0;

	// TODO: use FCD Pro controls
	//fcdAppSetParam(m_dev, FCD_CMD_APP_SET_BIAS_TEE, &cmd, 1);
}

void FCDProInput::set_lnaGain(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_lna_gain_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::lna_gains[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_LNA_GAIN, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_lnaGain: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_rfFilter(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_rf_filter_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::rf_filters[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_RF_FILTER, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_rfFilter: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_lnaEnhance(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_lna_enhance_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::lna_enhances[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_LNA_ENHANCE, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_lnaEnhance: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_band(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_band_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::bands[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_BAND, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_band: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_mixerGain(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_mixer_gain_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::mixer_gains[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_MIXER_GAIN, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_mixerGain: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_mixerFilter(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_mixer_filter_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::mixer_filters[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_MIXER_FILTER, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_mixerFilter: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_biasCurrent(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_bias_current_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::bias_currents[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_BIAS_CURRENT, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_biasCurrent: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_mode(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_if_gain_mode_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::if_gain_modes[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_IF_GAIN_MODE, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_mode: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_gain1(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_if_gain1_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::if_gains1[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_IF_GAIN1, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_gain1: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_rcFilter(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_if_rc_filter_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::if_rc_filters[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_IF_RC_FILTER, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_rcFilter: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_gain2(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_if_gain2_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::if_gains2[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_IF_GAIN2, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_gain2: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_gain3(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_if_gain3_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::if_gains3[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_IF_GAIN3, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_gain3: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_gain4(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_if_gain4_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::if_gains4[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_IF_GAIN4, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_gain4: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_ifFilter(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_if_filter_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::if_filters[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_IF_FILTER, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_ifFilter: failed to set at " << cmd_value;
	}

}

void FCDProInput::set_gain5(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_if_gain5_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::if_gains5[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_IF_GAIN5, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_gain5: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_gain6(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_if_gain6_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::if_gains6[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_IF_GAIN6, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_gain6: failed to set at " << cmd_value;
	}
}

int FCDProInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int FCDProInput::webapiRun(
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

int FCDProInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setFcdProSettings(new SWGSDRangel::SWGFCDProSettings());
    response.getFcdProSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int FCDProInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    FCDProSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureFCDPro *msg = MsgConfigureFCDPro::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureFCDPro *msgToGUI = MsgConfigureFCDPro::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void FCDProInput::webapiUpdateDeviceSettings(
        FCDProSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getFcdProSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("LOppmTenths")) {
        settings.m_LOppmTenths = response.getFcdProSettings()->getLOppmTenths();
    }
    if (deviceSettingsKeys.contains("lnaGainIndex")) {
        settings.m_lnaGainIndex = response.getFcdProSettings()->getLnaGainIndex();
    }
    if (deviceSettingsKeys.contains("rfFilterIndex")) {
        settings.m_rfFilterIndex = response.getFcdProSettings()->getRfFilterIndex();
    }
    if (deviceSettingsKeys.contains("lnaEnhanceIndex")) {
        settings.m_lnaEnhanceIndex = response.getFcdProSettings()->getLnaEnhanceIndex();
    }
    if (deviceSettingsKeys.contains("bandIndex")) {
        settings.m_bandIndex = response.getFcdProSettings()->getBandIndex();
    }
    if (deviceSettingsKeys.contains("mixerGainIndex")) {
        settings.m_mixerGainIndex = response.getFcdProSettings()->getMixerGainIndex();
    }
    if (deviceSettingsKeys.contains("mixerFilterIndex")) {
        settings.m_mixerFilterIndex = response.getFcdProSettings()->getMixerFilterIndex();
    }
    if (deviceSettingsKeys.contains("biasCurrentIndex")) {
        settings.m_biasCurrentIndex = response.getFcdProSettings()->getBiasCurrentIndex();
    }
    if (deviceSettingsKeys.contains("modeIndex")) {
        settings.m_modeIndex = response.getFcdProSettings()->getModeIndex();
    }
    if (deviceSettingsKeys.contains("gain1Index")) {
        settings.m_gain1Index = response.getFcdProSettings()->getGain1Index();
    }
    if (deviceSettingsKeys.contains("gain2Index")) {
        settings.m_gain2Index = response.getFcdProSettings()->getGain2Index();
    }
    if (deviceSettingsKeys.contains("gain3Index")) {
        settings.m_gain3Index = response.getFcdProSettings()->getGain3Index();
    }
    if (deviceSettingsKeys.contains("gain4Index")) {
        settings.m_gain4Index = response.getFcdProSettings()->getGain4Index();
    }
    if (deviceSettingsKeys.contains("gain5Index")) {
        settings.m_gain5Index = response.getFcdProSettings()->getGain5Index();
    }
    if (deviceSettingsKeys.contains("gain6Index")) {
        settings.m_gain6Index = response.getFcdProSettings()->getGain6Index();
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getFcdProSettings()->getLog2Decim();
    }
    if (deviceSettingsKeys.contains("iqOrder")) {
        settings.m_iqOrder = response.getFcdProSettings()->getIqOrder() != 0;
    }
    if (deviceSettingsKeys.contains("fcPos")) {
        settings.m_fcPos = (FCDProSettings::fcPos_t) response.getFcdProSettings()->getFcPos();
    }
    if (deviceSettingsKeys.contains("rcFilterIndex")) {
        settings.m_rcFilterIndex = response.getFcdProSettings()->getRcFilterIndex();
    }
    if (deviceSettingsKeys.contains("ifFilterIndex")) {
        settings.m_ifFilterIndex = response.getFcdProSettings()->getIfFilterIndex();
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getFcdProSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getFcdProSettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency")) {
        settings.m_transverterDeltaFrequency = response.getFcdProSettings()->getTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("transverterMode")) {
        settings.m_transverterMode = response.getFcdProSettings()->getTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getFcdProSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getFcdProSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getFcdProSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getFcdProSettings()->getReverseApiDeviceIndex();
    }
}

void FCDProInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const FCDProSettings& settings)
{
    response.getFcdProSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getFcdProSettings()->setLOppmTenths(settings.m_LOppmTenths);
    response.getFcdProSettings()->setLnaGainIndex(settings.m_lnaGainIndex);
    response.getFcdProSettings()->setRfFilterIndex(settings.m_rfFilterIndex);
    response.getFcdProSettings()->setLnaEnhanceIndex(settings.m_lnaEnhanceIndex);
    response.getFcdProSettings()->setBandIndex(settings.m_bandIndex);
    response.getFcdProSettings()->setMixerGainIndex(settings.m_mixerGainIndex);
    response.getFcdProSettings()->setMixerFilterIndex(settings.m_mixerFilterIndex);
    response.getFcdProSettings()->setBiasCurrentIndex(settings.m_biasCurrentIndex);
    response.getFcdProSettings()->setModeIndex(settings.m_modeIndex);
    response.getFcdProSettings()->setGain1Index(settings.m_gain1Index);
    response.getFcdProSettings()->setGain2Index(settings.m_gain2Index);
    response.getFcdProSettings()->setGain3Index(settings.m_gain3Index);
    response.getFcdProSettings()->setGain4Index(settings.m_gain4Index);
    response.getFcdProSettings()->setGain5Index(settings.m_gain5Index);
    response.getFcdProSettings()->setGain6Index(settings.m_gain6Index);
    response.getFcdProSettings()->setLog2Decim(settings.m_log2Decim);
    response.getFcdProSettings()->setIqOrder(settings.m_iqOrder ? 1 : 0);
    response.getFcdProSettings()->setFcPos((int) settings.m_fcPos);
    response.getFcdProSettings()->setRcFilterIndex(settings.m_rcFilterIndex);
    response.getFcdProSettings()->setIfFilterIndex(settings.m_ifFilterIndex);
    response.getFcdProSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getFcdProSettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    response.getFcdProSettings()->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    response.getFcdProSettings()->setTransverterMode(settings.m_transverterMode ? 1 : 0);

    response.getFcdProSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getFcdProSettings()->getReverseApiAddress()) {
        *response.getFcdProSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getFcdProSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getFcdProSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getFcdProSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

void FCDProInput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const FCDProSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("FCDPro"));
    swgDeviceSettings->setFcdProSettings(new SWGSDRangel::SWGFCDProSettings());
    SWGSDRangel::SWGFCDProSettings *swgFCDProSettings = swgDeviceSettings->getFcdProSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        swgFCDProSettings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("LOppmTenths") || force) {
        swgFCDProSettings->setLOppmTenths(settings.m_LOppmTenths);
    }
    if (deviceSettingsKeys.contains("lnaGainIndex") || force) {
        swgFCDProSettings->setLnaGainIndex(settings.m_lnaGainIndex);
    }
    if (deviceSettingsKeys.contains("rfFilterIndex") || force) {
        swgFCDProSettings->setRfFilterIndex(settings.m_rfFilterIndex);
    }
    if (deviceSettingsKeys.contains("lnaEnhanceIndex") || force) {
        swgFCDProSettings->setLnaEnhanceIndex(settings.m_lnaEnhanceIndex);
    }
    if (deviceSettingsKeys.contains("bandIndex") || force) {
        swgFCDProSettings->setBandIndex(settings.m_bandIndex);
    }
    if (deviceSettingsKeys.contains("mixerGainIndex") || force) {
        swgFCDProSettings->setMixerGainIndex(settings.m_mixerGainIndex);
    }
    if (deviceSettingsKeys.contains("mixerFilterIndex") || force) {
        swgFCDProSettings->setMixerFilterIndex(settings.m_mixerFilterIndex);
    }
    if (deviceSettingsKeys.contains("biasCurrentIndex") || force) {
        swgFCDProSettings->setBiasCurrentIndex(settings.m_biasCurrentIndex);
    }
    if (deviceSettingsKeys.contains("modeIndex") || force) {
        swgFCDProSettings->setModeIndex(settings.m_modeIndex);
    }
    if (deviceSettingsKeys.contains("gain1Index") || force) {
        swgFCDProSettings->setGain1Index(settings.m_gain1Index);
    }
    if (deviceSettingsKeys.contains("gain2Index") || force) {
        swgFCDProSettings->setGain2Index(settings.m_gain2Index);
    }
    if (deviceSettingsKeys.contains("gain3Index") || force) {
        swgFCDProSettings->setGain3Index(settings.m_gain3Index);
    }
    if (deviceSettingsKeys.contains("gain4Index") || force) {
        swgFCDProSettings->setGain4Index(settings.m_gain4Index);
    }
    if (deviceSettingsKeys.contains("gain5Index") || force) {
        swgFCDProSettings->setGain5Index(settings.m_gain5Index);
    }
    if (deviceSettingsKeys.contains("gain6Index") || force) {
        swgFCDProSettings->setGain6Index(settings.m_gain6Index);
    }
    if (deviceSettingsKeys.contains("log2Decim") || force) {
        swgFCDProSettings->setLog2Decim(settings.m_log2Decim);
    }
    if (deviceSettingsKeys.contains("iqOrder") || force) {
        swgFCDProSettings->setIqOrder(settings.m_iqOrder ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("fcPos") || force) {
        swgFCDProSettings->setFcPos(settings.m_fcPos);
    }
    if (deviceSettingsKeys.contains("rcFilterIndex") || force) {
        swgFCDProSettings->setRcFilterIndex(settings.m_rcFilterIndex);
    }
    if (deviceSettingsKeys.contains("ifFilterIndex") || force) {
        swgFCDProSettings->setIfFilterIndex(settings.m_ifFilterIndex);
    }
    if (deviceSettingsKeys.contains("dcBlock") || force) {
        swgFCDProSettings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("iqCorrection") || force) {
        swgFCDProSettings->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency") || force) {
        swgFCDProSettings->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("transverterMode") || force) {
        swgFCDProSettings->setTransverterMode(settings.m_transverterMode ? 1 : 0);
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

void FCDProInput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("FCDPro"));

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

void FCDProInput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "FCDProInput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("FCDProInput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
