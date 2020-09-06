///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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
#include <QNetworkAccessManager>
#include <QBuffer>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGMetisMISOSettings.h"

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/dspdevicemimoengine.h"
#include "dsp/devicesamplesource.h"
#include "metis/devicemetis.h"

#include "metismisoudphandler.h"
#include "metismiso.h"

MESSAGE_CLASS_DEFINITION(MetisMISO::MsgConfigureMetisMISO, Message)
MESSAGE_CLASS_DEFINITION(MetisMISO::MsgStartStop, Message)


MetisMISO::MetisMISO(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
    m_udpHandler(&m_sampleMIFifo, &m_sampleMOFifo, deviceAPI),
	m_deviceDescription("MetisMISO"),
	m_running(false),
	m_masterTimer(deviceAPI->getMasterTimer())
{
    m_mimoType = MIMOHalfSynchronous;
    m_sampleMIFifo.init(MetisMISOSettings::m_maxReceivers, 96000 * 4);
    m_sampleMOFifo.init(1, 96000 * 4);
    m_deviceAPI->setNbSourceStreams(MetisMISOSettings::m_maxReceivers);
    m_deviceAPI->setNbSinkStreams(1);
    int deviceSequence = m_deviceAPI->getSamplingDeviceSequence();
    const DeviceMetisScan::DeviceScan *deviceScan = DeviceMetis::instance().getDeviceScanAt(deviceSequence);
    m_udpHandler.setMetisAddress(deviceScan->m_address, deviceScan->m_port);
    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

MetisMISO::~MetisMISO()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;

    if (m_running) {
        stopRx();
    }
}

void MetisMISO::destroy()
{
    delete this;
}

void MetisMISO::init()
{
    applySettings(m_settings, true);
}

bool MetisMISO::startRx()
{
    qDebug("MetisMISO::startRx");
	QMutexLocker mutexLocker(&m_mutex);

    if (!m_running) {
        startMetis();
    }

	mutexLocker.unlock();

	applySettings(m_settings, true);
	m_running = true;

	return true;
}

bool MetisMISO::startTx()
{
    qDebug("MetisMISO::startTx");
	QMutexLocker mutexLocker(&m_mutex);

    if (!m_running) {
        startMetis();
    }

	mutexLocker.unlock();

	applySettings(m_settings, true);
	m_running = true;

	return true;
}

void MetisMISO::stopRx()
{
    qDebug("MetisMISO::stopRx");
	QMutexLocker mutexLocker(&m_mutex);

    if (m_running) {
        stopMetis();
    }

	m_running = false;
}

void MetisMISO::stopTx()
{
    qDebug("MetisMISO::stopTx");
	QMutexLocker mutexLocker(&m_mutex);

    if (m_running) {
        stopMetis();
    }

	m_running = false;
}

void MetisMISO::startMetis()
{
    MetisMISOUDPHandler::MsgStartStop *message = MetisMISOUDPHandler::MsgStartStop::create(true);
    m_udpHandler.getInputMessageQueue()->push(message);
}

void MetisMISO::stopMetis()
{
    MetisMISOUDPHandler::MsgStartStop *message = MetisMISOUDPHandler::MsgStartStop::create(false);
    m_udpHandler.getInputMessageQueue()->push(message);
}

QByteArray MetisMISO::serialize() const
{
    return m_settings.serialize();
}

bool MetisMISO::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureMetisMISO* message = MsgConfigureMetisMISO::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureMetisMISO* messageToGUI = MsgConfigureMetisMISO::create(m_settings, true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& MetisMISO::getDeviceDescription() const
{
	return m_deviceDescription;
}

int MetisMISO::getSourceSampleRate(int index) const
{
    if (index < 3) {
        return MetisMISOSettings::getSampleRateFromIndex(m_settings.m_sampleRateIndex);
    } else {
        return 0;
    }
}

quint64 MetisMISO::getSourceCenterFrequency(int index) const
{
    switch(index)
    {
    case 0:
        return m_settings.m_rx1CenterFrequency;
        break;
    case 1:
        return m_settings.m_rx2CenterFrequency;
        break;
    default:
        return 0;
    }
}

void MetisMISO::setSourceCenterFrequency(qint64 centerFrequency, int index)
{
    MetisMISOSettings settings = m_settings; // note: calls copy constructor

    if (index < 2)
    {
        if (index == 0) {
            settings.m_rx1CenterFrequency = centerFrequency;
        } else {
            settings.m_rx2CenterFrequency = centerFrequency;
        }

        MsgConfigureMetisMISO* message = MsgConfigureMetisMISO::create(settings, false);
        m_inputMessageQueue.push(message);

        if (m_guiMessageQueue)
        {
            MsgConfigureMetisMISO* messageToGUI = MsgConfigureMetisMISO::create(settings, false);
            m_guiMessageQueue->push(messageToGUI);
        }
    }
}

bool MetisMISO::handleMessage(const Message& message)
{
    if (MsgConfigureMetisMISO::match(message))
    {
        MsgConfigureMetisMISO& conf = (MsgConfigureMetisMISO&) message;
        qDebug() << "MetisMISO::handleMessage: MsgConfigureMetisMISO";

        bool success = applySettings(conf.getSettings(), conf.getForce());

        if (!success)
        {
            qDebug("MetisMISO::handleMessage: config error");
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "MetisMISO::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            // Start Rx engine
            if (m_deviceAPI->initDeviceEngine(0)) {
                m_deviceAPI->startDeviceEngine(0);
            }

            // Start Tx engine
            if (m_deviceAPI->initDeviceEngine(1)) {
                m_deviceAPI->startDeviceEngine(1);
            }
        }
        else
        {
            m_deviceAPI->stopDeviceEngine(0); // Stop Rx engine
            m_deviceAPI->stopDeviceEngine(1); // Stop Tx engine
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

bool MetisMISO::applySettings(const MetisMISOSettings& settings, bool force)
{
    QList<QString> reverseAPIKeys;

    qDebug() << "MetisMISO::applySettings: "
        << " m_nbReceivers:" << settings.m_nbReceivers
        << " m_txEnable:" << settings.m_txEnable
        << " m_rx1CenterFrequency:" << settings.m_rx1CenterFrequency
        << " m_rx2CenterFrequency:" << settings.m_rx2CenterFrequency
        << " m_rx3CenterFrequency:" << settings.m_rx3CenterFrequency
        << " m_rx4CenterFrequency:" << settings.m_rx4CenterFrequency
        << " m_rx5CenterFrequency:" << settings.m_rx5CenterFrequency
        << " m_rx6CenterFrequency:" << settings.m_rx6CenterFrequency
        << " m_rx7CenterFrequency:" << settings.m_rx7CenterFrequency
        << " m_rx8CenterFrequency:" << settings.m_rx8CenterFrequency
        << " m_txCenterFrequency:" << settings.m_txCenterFrequency
        << " m_sampleRateIndex:" << settings.m_sampleRateIndex
        << " m_log2Decim:" << settings.m_log2Decim
        << " m_preamp:" << settings.m_preamp
        << " m_random:" << settings.m_random
        << " m_dither:" << settings.m_dither
        << " m_duplex:" << settings.m_duplex
        << " m_dcBlock:" << settings.m_dcBlock
        << " m_iqCorrection:" << settings.m_iqCorrection
        << " m_useReverseAPI: " << settings.m_useReverseAPI
        << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
        << " m_reverseAPIPort: " << settings.m_reverseAPIPort
        << " m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex;

    bool propagateSettings = false;

    if ((m_settings.m_nbReceivers != settings.m_nbReceivers) || force)
    {
        reverseAPIKeys.append("nbReceivers");
        propagateSettings = true;
    }

    if ((m_settings.m_txEnable != settings.m_txEnable) || force)
    {
        reverseAPIKeys.append("txEnable");
        propagateSettings = true;
    }

    if ((m_settings.m_rx1CenterFrequency != settings.m_rx1CenterFrequency) || force)
    {
        reverseAPIKeys.append("rx1CenterFrequency");
        propagateSettings = true;
    }

    if ((m_settings.m_rx2CenterFrequency != settings.m_rx2CenterFrequency) || force)
    {
        reverseAPIKeys.append("rx2CenterFrequency");
        propagateSettings = true;
    }

    if ((m_settings.m_rx3CenterFrequency != settings.m_rx3CenterFrequency) || force)
    {
        reverseAPIKeys.append("rx3CenterFrequency");
        propagateSettings = true;
    }

    if ((m_settings.m_rx4CenterFrequency != settings.m_rx4CenterFrequency) || force)
    {
        reverseAPIKeys.append("rx4CenterFrequency");
        propagateSettings = true;
    }

    if ((m_settings.m_rx5CenterFrequency != settings.m_rx5CenterFrequency) || force)
    {
        reverseAPIKeys.append("rx5CenterFrequency");
        propagateSettings = true;
    }

    if ((m_settings.m_rx6CenterFrequency != settings.m_rx6CenterFrequency) || force)
    {
        reverseAPIKeys.append("rx6CenterFrequency");
        propagateSettings = true;
    }

    if ((m_settings.m_rx7CenterFrequency != settings.m_rx7CenterFrequency) || force)
    {
        reverseAPIKeys.append("rx7CenterFrequency");
        propagateSettings = true;
    }

    if ((m_settings.m_rx8CenterFrequency != settings.m_rx8CenterFrequency) || force)
    {
        reverseAPIKeys.append("rx8CenterFrequency");
        propagateSettings = true;
    }

    if ((m_settings.m_txCenterFrequency != settings.m_txCenterFrequency) || force)
    {
        reverseAPIKeys.append("txCenterFrequency");
        propagateSettings = true;
    }

    if ((m_settings.m_sampleRateIndex != settings.m_sampleRateIndex) || force)
    {
        reverseAPIKeys.append("sampleRateIndex");
        propagateSettings = true;
    }

    if ((m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        reverseAPIKeys.append("log2Decim");
        propagateSettings = true;
    }

    if ((m_settings.m_dcBlock != settings.m_dcBlock) || force) {
        reverseAPIKeys.append("dcBlock");
    }

    if ((m_settings.m_iqCorrection != settings.m_iqCorrection) || force) {
        reverseAPIKeys.append("iqCorrection");
    }

    if ((m_settings.m_dcBlock != settings.m_dcBlock) ||
        (m_settings.m_iqCorrection != settings.m_iqCorrection) || force)
    {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection, 0);
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection, 1);
    }

    if ((m_settings.m_rx1CenterFrequency != settings.m_rx1CenterFrequency) ||
        (m_settings.m_sampleRateIndex != settings.m_sampleRateIndex) ||
        (m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        int devSampleRate = (1<<settings.m_sampleRateIndex) * 48000;
        int sampleRate = devSampleRate / (1<<settings.m_log2Decim);
        DSPMIMOSignalNotification *engineRx1Notif = new DSPMIMOSignalNotification(
            sampleRate, settings.m_rx1CenterFrequency, true, 0);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(engineRx1Notif);
    }

    if ((m_settings.m_rx2CenterFrequency != settings.m_rx2CenterFrequency) ||
        (m_settings.m_sampleRateIndex != settings.m_sampleRateIndex) ||
        (m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        int devSampleRate = (1<<settings.m_sampleRateIndex) * 48000;
        int sampleRate = devSampleRate / (1<<settings.m_log2Decim);
        DSPMIMOSignalNotification *engineRx2Notif = new DSPMIMOSignalNotification(
            sampleRate, settings.m_rx2CenterFrequency, true, 1);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(engineRx2Notif);
    }

    if ((m_settings.m_rx3CenterFrequency != settings.m_rx3CenterFrequency) ||
        (m_settings.m_sampleRateIndex != settings.m_sampleRateIndex) ||
        (m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        int devSampleRate = (1<<settings.m_sampleRateIndex) * 48000;
        int sampleRate = devSampleRate / (1<<settings.m_log2Decim);
        DSPMIMOSignalNotification *engineRx3Notif = new DSPMIMOSignalNotification(
            sampleRate, settings.m_rx3CenterFrequency, true, 2);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(engineRx3Notif);
    }

    if ((m_settings.m_rx4CenterFrequency != settings.m_rx4CenterFrequency) ||
        (m_settings.m_sampleRateIndex != settings.m_sampleRateIndex) ||
        (m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        int devSampleRate = (1<<settings.m_sampleRateIndex) * 48000;
        int sampleRate = devSampleRate / (1<<settings.m_log2Decim);
        DSPMIMOSignalNotification *engineRx4Notif = new DSPMIMOSignalNotification(
            sampleRate, settings.m_rx4CenterFrequency, true, 3);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(engineRx4Notif);
    }

    if ((m_settings.m_rx5CenterFrequency != settings.m_rx5CenterFrequency) ||
        (m_settings.m_sampleRateIndex != settings.m_sampleRateIndex) ||
        (m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        int devSampleRate = (1<<settings.m_sampleRateIndex) * 48000;
        int sampleRate = devSampleRate / (1<<settings.m_log2Decim);
        DSPMIMOSignalNotification *engineRx5Notif = new DSPMIMOSignalNotification(
            sampleRate, settings.m_rx5CenterFrequency, true, 4);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(engineRx5Notif);
    }

    if ((m_settings.m_rx6CenterFrequency != settings.m_rx6CenterFrequency) ||
        (m_settings.m_sampleRateIndex != settings.m_sampleRateIndex) ||
        (m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        int devSampleRate = (1<<settings.m_sampleRateIndex) * 48000;
        int sampleRate = devSampleRate / (1<<settings.m_log2Decim);
        DSPMIMOSignalNotification *engineRx6Notif = new DSPMIMOSignalNotification(
            sampleRate, settings.m_rx6CenterFrequency, true, 5);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(engineRx6Notif);
    }

    if ((m_settings.m_rx7CenterFrequency != settings.m_rx7CenterFrequency) ||
        (m_settings.m_sampleRateIndex != settings.m_sampleRateIndex) ||
        (m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        int devSampleRate = (1<<settings.m_sampleRateIndex) * 48000;
        int sampleRate = devSampleRate / (1<<settings.m_log2Decim);
        DSPMIMOSignalNotification *engineRx7Notif = new DSPMIMOSignalNotification(
            sampleRate, settings.m_rx7CenterFrequency, true, 6);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(engineRx7Notif);
    }

    if ((m_settings.m_rx8CenterFrequency != settings.m_rx8CenterFrequency) ||
        (m_settings.m_sampleRateIndex != settings.m_sampleRateIndex) ||
        (m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        int devSampleRate = (1<<settings.m_sampleRateIndex) * 48000;
        int sampleRate = devSampleRate / (1<<settings.m_log2Decim);
        DSPMIMOSignalNotification *engineRx8Notif = new DSPMIMOSignalNotification(
            sampleRate, settings.m_rx8CenterFrequency, true, 7);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(engineRx8Notif);
    }

    if ((m_settings.m_txCenterFrequency != settings.m_txCenterFrequency) ||
        (m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        DSPMIMOSignalNotification *engineTxNotif = new DSPMIMOSignalNotification(
            48000, settings.m_txCenterFrequency, false, 0);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(engineTxNotif);
    }

    if (propagateSettings) {
        m_udpHandler.applySettings(settings);
    }

    if (settings.m_useReverseAPI)
    {
        qDebug("MetisMISO::applySettings: call webapiReverseSendSettings");
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    m_settings = settings;
    return true;
}

int MetisMISO::webapiRunGet(
        int subsystemIndex,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    if ((subsystemIndex == 0) || (subsystemIndex == 1)) // Rx and Tx always started together
    {
        m_deviceAPI->getDeviceEngineStateStr(*response.getState());
        return 200;
    }
    else
    {
        errorMessage = QString("Subsystem index invalid: expect 0 (Rx) only");
        return 404;
    }
}

int MetisMISO::webapiRun(
        bool run,
        int subsystemIndex,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    // Rx and Tx are started or stopped together
    if ((subsystemIndex == 0) || (subsystemIndex == 1))
    {
        m_deviceAPI->getDeviceEngineStateStr(*response.getState()); // Rx driven
        MsgStartStop *message = MsgStartStop::create(run);
        m_inputMessageQueue.push(message);

        if (m_guiMessageQueue) // forward to GUI if any
        {
            MsgStartStop *msgToGUI = MsgStartStop::create(run);
            m_guiMessageQueue->push(msgToGUI);
        }

        return 200;
    }
    else
    {
        errorMessage = QString("Subsystem index invalid: expect 0 (Rx) only");
        return 404;
    }

}

int MetisMISO::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setMetisMisoSettings(new SWGSDRangel::SWGMetisMISOSettings());
    response.getMetisMisoSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int MetisMISO::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    MetisMISOSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureMetisMISO *msg = MsgConfigureMetisMISO::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureMetisMISO *msgToGUI = MsgConfigureMetisMISO::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void MetisMISO::webapiUpdateDeviceSettings(
        MetisMISOSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("nbReceivers")) {
        settings.m_nbReceivers = response.getMetisMisoSettings()->getNbReceivers();
    }
    if (deviceSettingsKeys.contains("txEnable")) {
        settings.m_txEnable = response.getMetisMisoSettings()->getTxEnable() != 0;
    }
    if (deviceSettingsKeys.contains("rx1CenterFrequency")) {
        settings.m_rx1CenterFrequency = response.getMetisMisoSettings()->getRx1CenterFrequency();
    }
    if (deviceSettingsKeys.contains("rx2CenterFrequency")) {
        settings.m_rx2CenterFrequency = response.getMetisMisoSettings()->getRx2CenterFrequency();
    }
    if (deviceSettingsKeys.contains("rx3CenterFrequency")) {
        settings.m_rx3CenterFrequency = response.getMetisMisoSettings()->getRx3CenterFrequency();
    }
    if (deviceSettingsKeys.contains("rx4CenterFrequency")) {
        settings.m_rx4CenterFrequency = response.getMetisMisoSettings()->getRx4CenterFrequency();
    }
    if (deviceSettingsKeys.contains("rx5CenterFrequency")) {
        settings.m_rx5CenterFrequency = response.getMetisMisoSettings()->getRx5CenterFrequency();
    }
    if (deviceSettingsKeys.contains("rx6CenterFrequency")) {
        settings.m_rx6CenterFrequency = response.getMetisMisoSettings()->getRx6CenterFrequency();
    }
    if (deviceSettingsKeys.contains("rx7CenterFrequency")) {
        settings.m_rx7CenterFrequency = response.getMetisMisoSettings()->getRx7CenterFrequency();
    }
    if (deviceSettingsKeys.contains("rx8CenterFrequency")) {
        settings.m_rx8CenterFrequency = response.getMetisMisoSettings()->getRx8CenterFrequency();
    }
    if (deviceSettingsKeys.contains("txCenterFrequency")) {
        settings.m_txCenterFrequency = response.getMetisMisoSettings()->getTxCenterFrequency();
    }
    if (deviceSettingsKeys.contains("sampleRateIndex")) {
        settings.m_sampleRateIndex = response.getMetisMisoSettings()->getSampleRateIndex();
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getMetisMisoSettings()->getLog2Decim();
    }
    if (deviceSettingsKeys.contains("preamp")) {
        settings.m_preamp = response.getMetisMisoSettings()->getPreamp() != 0;
    }
    if (deviceSettingsKeys.contains("random")) {
        settings.m_random = response.getMetisMisoSettings()->getRandom() != 0;
    }
    if (deviceSettingsKeys.contains("dither")) {
        settings.m_dither = response.getMetisMisoSettings()->getDither() != 0;
    }
    if (deviceSettingsKeys.contains("duplex")) {
        settings.m_duplex = response.getMetisMisoSettings()->getDuplex() != 0;
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getMetisMisoSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getMetisMisoSettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getMetisMisoSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getMetisMisoSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getMetisMisoSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getMetisMisoSettings()->getReverseApiDeviceIndex();
    }
}

void MetisMISO::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const MetisMISOSettings& settings)
{
    response.getMetisMisoSettings()->setNbReceivers(settings.m_nbReceivers);
    response.getMetisMisoSettings()->setTxEnable(settings.m_txEnable ? 1 : 0);
    response.getMetisMisoSettings()->setRx1CenterFrequency(settings.m_rx1CenterFrequency);
    response.getMetisMisoSettings()->setRx2CenterFrequency(settings.m_rx2CenterFrequency);
    response.getMetisMisoSettings()->setRx3CenterFrequency(settings.m_rx3CenterFrequency);
    response.getMetisMisoSettings()->setRx4CenterFrequency(settings.m_rx4CenterFrequency);
    response.getMetisMisoSettings()->setRx5CenterFrequency(settings.m_rx5CenterFrequency);
    response.getMetisMisoSettings()->setRx6CenterFrequency(settings.m_rx6CenterFrequency);
    response.getMetisMisoSettings()->setRx7CenterFrequency(settings.m_rx7CenterFrequency);
    response.getMetisMisoSettings()->setRx8CenterFrequency(settings.m_rx8CenterFrequency);
    response.getMetisMisoSettings()->setTxCenterFrequency(settings.m_txCenterFrequency);
    response.getMetisMisoSettings()->setSampleRateIndex(settings.m_sampleRateIndex);
    response.getMetisMisoSettings()->setLog2Decim(settings.m_log2Decim);
    response.getMetisMisoSettings()->setPreamp(settings.m_preamp ? 1 : 0);
    response.getMetisMisoSettings()->setRandom(settings.m_random ? 1 : 0);
    response.getMetisMisoSettings()->setDither(settings.m_dither ? 1 : 0);
    response.getMetisMisoSettings()->setDuplex(settings.m_duplex ? 1 : 0);
    response.getMetisMisoSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getMetisMisoSettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    response.getMetisMisoSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getMetisMisoSettings()->getReverseApiAddress()) {
        *response.getMetisMisoSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getMetisMisoSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getMetisMisoSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getMetisMisoSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

void MetisMISO::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const MetisMISOSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(2); // MIMO
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("MetisMISO"));
    swgDeviceSettings->setMetisMisoSettings(new SWGSDRangel::SWGMetisMISOSettings());
    SWGSDRangel::SWGMetisMISOSettings *swgMetisMISOSettings = swgDeviceSettings->getMetisMisoSettings();

    if (deviceSettingsKeys.contains("nbReceivers") || force) {
        swgMetisMISOSettings->setNbReceivers(settings.m_nbReceivers);
    }
    if (deviceSettingsKeys.contains("txEnable") || force) {
        swgMetisMISOSettings->setTxEnable(settings.m_txEnable ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("rx1CenterFrequency") || force) {
        swgMetisMISOSettings->setRx1CenterFrequency(settings.m_rx1CenterFrequency);
    }
    if (deviceSettingsKeys.contains("rx2CenterFrequency") || force) {
        swgMetisMISOSettings->setRx2CenterFrequency(settings.m_rx2CenterFrequency);
    }
    if (deviceSettingsKeys.contains("rx3CenterFrequency") || force) {
        swgMetisMISOSettings->setRx3CenterFrequency(settings.m_rx3CenterFrequency);
    }
    if (deviceSettingsKeys.contains("rx4CenterFrequency") || force) {
        swgMetisMISOSettings->setRx4CenterFrequency(settings.m_rx4CenterFrequency);
    }
    if (deviceSettingsKeys.contains("rx5CenterFrequency") || force) {
        swgMetisMISOSettings->setRx5CenterFrequency(settings.m_rx5CenterFrequency);
    }
    if (deviceSettingsKeys.contains("rx6CenterFrequency") || force) {
        swgMetisMISOSettings->setRx6CenterFrequency(settings.m_rx6CenterFrequency);
    }
    if (deviceSettingsKeys.contains("rx7CenterFrequency") || force) {
        swgMetisMISOSettings->setRx7CenterFrequency(settings.m_rx7CenterFrequency);
    }
    if (deviceSettingsKeys.contains("rx8CenterFrequency") || force) {
        swgMetisMISOSettings->setRx8CenterFrequency(settings.m_rx8CenterFrequency);
    }
    if (deviceSettingsKeys.contains("txCenterFrequency") || force) {
        swgMetisMISOSettings->setTxCenterFrequency(settings.m_txCenterFrequency);
    }
    if (deviceSettingsKeys.contains("sampleRateIndex") || force) {
        swgMetisMISOSettings->setSampleRateIndex(settings.m_sampleRateIndex);
    }
    if (deviceSettingsKeys.contains("log2Decim") || force) {
        swgMetisMISOSettings->setLog2Decim(settings.m_log2Decim);
    }
    if (deviceSettingsKeys.contains("preamp") || force) {
        swgMetisMISOSettings->setPreamp(settings.m_preamp ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("random") || force) {
        swgMetisMISOSettings->setRandom(settings.m_random ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("dither") || force) {
        swgMetisMISOSettings->setDither(settings.m_dither ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("duplex") || force) {
        swgMetisMISOSettings->setDuplex(settings.m_duplex ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("dcBlock") || force) {
        swgMetisMISOSettings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("iqCorrection") || force) {
        swgMetisMISOSettings->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    }

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
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

void MetisMISO::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(2); // MIMO
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("MetisMISO"));

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/run")
            .arg(m_settings.m_reverseAPIAddress)
            .arg(m_settings.m_reverseAPIPort)
            .arg(m_settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
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

void MetisMISO::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "MetisMISO::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("MetisMISO::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
