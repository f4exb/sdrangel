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
#include "dsp/samplesourcefifo.h"
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
    m_sampleMOFifo.init(1, SampleSourceFifo::getSizePolicy(48000));
    m_deviceAPI->setNbSourceStreams(MetisMISOSettings::m_maxReceivers);
    m_deviceAPI->setNbSinkStreams(1);
    int deviceSequence = m_deviceAPI->getSamplingDeviceSequence();
    const DeviceMetisScan::DeviceScan *deviceScan = DeviceMetis::instance().getDeviceScanAt(deviceSequence);
    m_udpHandler.setMetisAddress(deviceScan->m_address, deviceScan->m_port);
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &MetisMISO::networkManagerFinished
    );
}

MetisMISO::~MetisMISO()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &MetisMISO::networkManagerFinished
    );
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
    applySettings(m_settings, QList<QString>(), true);
}

bool MetisMISO::startRx()
{
    qDebug("MetisMISO::startRx");
	QMutexLocker mutexLocker(&m_mutex);

    if (!m_running) {
        startMetis();
    }

	mutexLocker.unlock();

	applySettings(m_settings, QList<QString>(), true);
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

	applySettings(m_settings, QList<QString>(), true);
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

    MsgConfigureMetisMISO* message = MsgConfigureMetisMISO::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureMetisMISO* messageToGUI = MsgConfigureMetisMISO::create(m_settings, QList<QString>(), true);
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
    if (index < MetisMISOSettings::m_maxReceivers) {
        return m_settings.m_rxCenterFrequencies[index];
    } else {
        return 0;
    }
}

void MetisMISO::setSourceCenterFrequency(qint64 centerFrequency, int index)
{
    MetisMISOSettings settings = m_settings; // note: calls copy constructor

    if (index < MetisMISOSettings::m_maxReceivers)
    {
        settings.m_rxCenterFrequencies[index] = centerFrequency;
        QList<QString> settingsKeys;
        settingsKeys.append(tr("rx%1CenterFrequency").arg(index+1));

        MsgConfigureMetisMISO* message = MsgConfigureMetisMISO::create(settings, settingsKeys, false);
        m_inputMessageQueue.push(message);

        if (m_guiMessageQueue)
        {
            MsgConfigureMetisMISO* messageToGUI = MsgConfigureMetisMISO::create(settings, settingsKeys, false);
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

        bool success = applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());

        if (!success) {
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

bool MetisMISO::applySettings(const MetisMISOSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "MetisMISO::applySettings: force:" << force << settings.getDebugString(settingsKeys, force);

    bool propagateSettings = false;

    if (settingsKeys.contains("nbReceivers") || force) {
        propagateSettings = true;
    }

    if (settingsKeys.contains("txEnable") || force) {
        propagateSettings = true;
    }

    for (int i = 0; i < MetisMISOSettings::m_maxReceivers; i++)
    {
        if (settingsKeys.contains(QString("rx%1CenterFrequency").arg(i+1)) || force) {
            propagateSettings = true;
        }

        if (settingsKeys.contains(QString("rx%1SubsamplingIndex").arg(i+1)) || force) {
            propagateSettings = true;
        }
    }

    if (settingsKeys.contains("txCenterFrequency") || force) {
        propagateSettings = true;
    }

    if (settingsKeys.contains("rxTransverterMode") || force) {
        propagateSettings = true;
    }

    if (settingsKeys.contains("rxTransverterDeltaFrequency") || force) {
        propagateSettings = true;
    }

    if (settingsKeys.contains("txTransverterMode") || force) {
        propagateSettings = true;
    }

    if (settingsKeys.contains("txTransverterDeltaFrequency") || force) {
        propagateSettings = true;
    }

    if (settingsKeys.contains("iqOrder") || force) {
        propagateSettings = true;
    }

    if (settingsKeys.contains("sampleRateIndex") || force) {
        propagateSettings = true;
    }

    if (settingsKeys.contains("log2Decim") || force) {
        propagateSettings = true;
    }

    if (settingsKeys.contains("LOppmTenths") || force) {
        propagateSettings = true;
    }

    if (settingsKeys.contains("txDrive") || force) {
        propagateSettings = true;
    }

    if (settingsKeys.contains("dcBlock") ||
        settingsKeys.contains("iqCorrection") || force)
    {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection, 0);
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection, 1);
    }

    for (int i = 0; i < MetisMISOSettings::m_maxReceivers; i++)
    {
        if (settingsKeys.contains(QString("rx%1CenterFrequency").arg(i+1)) ||
            settingsKeys.contains("sampleRateIndex") ||
            settingsKeys.contains("log2Decim") || force)
        {
            int devSampleRate = (1<<settings.m_sampleRateIndex) * 48000;
            int sampleRate = devSampleRate / (1<<settings.m_log2Decim);
            DSPMIMOSignalNotification *engineRxNotif = new DSPMIMOSignalNotification(
                sampleRate, settings.m_rxCenterFrequencies[i], true, i);
            m_deviceAPI->getDeviceEngineInputMessageQueue()->push(engineRxNotif);
        }
    }

    if (settingsKeys.contains("txCenterFrequency") || force)
    {
        DSPMIMOSignalNotification *engineTxNotif = new DSPMIMOSignalNotification(
            48000, settings.m_txCenterFrequency, false, 0);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(engineTxNotif);
    }

    if (propagateSettings) {
        m_udpHandler.applySettings(settings);
    }

    if (settingsKeys.contains("useReverseAPI"))
    {
        qDebug("MetisMISO::applySettings: call webapiReverseSendSettings");
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

    MsgConfigureMetisMISO *msg = MsgConfigureMetisMISO::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureMetisMISO *msgToGUI = MsgConfigureMetisMISO::create(settings, deviceSettingsKeys, force);
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
        settings.m_rxCenterFrequencies[0] = response.getMetisMisoSettings()->getRx1CenterFrequency();
    }
    if (deviceSettingsKeys.contains("rx2CenterFrequency")) {
        settings.m_rxCenterFrequencies[1] = response.getMetisMisoSettings()->getRx2CenterFrequency();
    }
    if (deviceSettingsKeys.contains("rx3CenterFrequency")) {
        settings.m_rxCenterFrequencies[2] = response.getMetisMisoSettings()->getRx3CenterFrequency();
    }
    if (deviceSettingsKeys.contains("rx4CenterFrequency")) {
        settings.m_rxCenterFrequencies[3] = response.getMetisMisoSettings()->getRx4CenterFrequency();
    }
    if (deviceSettingsKeys.contains("rx5CenterFrequency")) {
        settings.m_rxCenterFrequencies[4] = response.getMetisMisoSettings()->getRx5CenterFrequency();
    }
    if (deviceSettingsKeys.contains("rx6CenterFrequency")) {
        settings.m_rxCenterFrequencies[5] = response.getMetisMisoSettings()->getRx6CenterFrequency();
    }
    if (deviceSettingsKeys.contains("rx7CenterFrequency")) {
        settings.m_rxCenterFrequencies[6] = response.getMetisMisoSettings()->getRx7CenterFrequency();
    }
    if (deviceSettingsKeys.contains("rx8CenterFrequency")) {
        settings.m_rxCenterFrequencies[7] = response.getMetisMisoSettings()->getRx8CenterFrequency();
    }
    if (deviceSettingsKeys.contains("txCenterFrequency")) {
        settings.m_txCenterFrequency = response.getMetisMisoSettings()->getTxCenterFrequency();
    }
    if (deviceSettingsKeys.contains("rxTransverterMode")) {
        settings.m_rxTransverterMode = response.getMetisMisoSettings()->getRxTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("rxTransverterDeltaFrequency")) {
        settings.m_rxTransverterDeltaFrequency = response.getMetisMisoSettings()->getRxTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("txTransverterMode")) {
        settings.m_txTransverterMode = response.getMetisMisoSettings()->getTxTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("txTransverterDeltaFrequency")) {
        settings.m_txTransverterDeltaFrequency = response.getMetisMisoSettings()->getTxTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("iqOrder")) {
        settings.m_iqOrder = response.getMetisMisoSettings()->getIqOrder() != 0;
    }
    if (deviceSettingsKeys.contains("rx1SubsamplingIndex")) {
        settings.m_rxSubsamplingIndexes[0] = response.getMetisMisoSettings()->getRx1SubsamplingIndex();
    }
    if (deviceSettingsKeys.contains("rx2SubsamplingIndex")) {
        settings.m_rxSubsamplingIndexes[1] = response.getMetisMisoSettings()->getRx2SubsamplingIndex();
    }
    if (deviceSettingsKeys.contains("rx3SubsamplingIndex")) {
        settings.m_rxSubsamplingIndexes[2] = response.getMetisMisoSettings()->getRx3SubsamplingIndex();
    }
    if (deviceSettingsKeys.contains("rx4SubsamplingIndex")) {
        settings.m_rxSubsamplingIndexes[3] = response.getMetisMisoSettings()->getRx4SubsamplingIndex();
    }
    if (deviceSettingsKeys.contains("rx5SubsamplingIndex")) {
        settings.m_rxSubsamplingIndexes[4] = response.getMetisMisoSettings()->getRx5SubsamplingIndex();
    }
    if (deviceSettingsKeys.contains("rx6SubsamplingIndex")) {
        settings.m_rxSubsamplingIndexes[5] = response.getMetisMisoSettings()->getRx6SubsamplingIndex();
    }
    if (deviceSettingsKeys.contains("rx7SubsamplingIndex")) {
        settings.m_rxSubsamplingIndexes[6] = response.getMetisMisoSettings()->getRx7SubsamplingIndex();
    }
    if (deviceSettingsKeys.contains("rx8SubsamplingIndex")) {
        settings.m_rxSubsamplingIndexes[7] = response.getMetisMisoSettings()->getRx8SubsamplingIndex();
    }
    if (deviceSettingsKeys.contains("sampleRateIndex")) {
        settings.m_sampleRateIndex = response.getMetisMisoSettings()->getSampleRateIndex();
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getMetisMisoSettings()->getLog2Decim();
    }
    if (deviceSettingsKeys.contains("LOppmTenths")) {
        settings.m_LOppmTenths = response.getMetisMisoSettings()->getLOppmTenths();
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
    if (deviceSettingsKeys.contains("txDrive")) {
        settings.m_txDrive = response.getMetisMisoSettings()->getTxDrive();
    }
    if (deviceSettingsKeys.contains("spectrumStreamIndex")) {
        settings.m_spectrumStreamIndex = response.getMetisMisoSettings()->getSpectrumStreamIndex();
    }
    if (deviceSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getMetisMisoSettings()->getStreamIndex();
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

    response.getMetisMisoSettings()->setRx1CenterFrequency(settings.m_rxCenterFrequencies[0]);
    response.getMetisMisoSettings()->setRx2CenterFrequency(settings.m_rxCenterFrequencies[1]);
    response.getMetisMisoSettings()->setRx3CenterFrequency(settings.m_rxCenterFrequencies[2]);
    response.getMetisMisoSettings()->setRx4CenterFrequency(settings.m_rxCenterFrequencies[3]);
    response.getMetisMisoSettings()->setRx5CenterFrequency(settings.m_rxCenterFrequencies[4]);
    response.getMetisMisoSettings()->setRx6CenterFrequency(settings.m_rxCenterFrequencies[5]);
    response.getMetisMisoSettings()->setRx7CenterFrequency(settings.m_rxCenterFrequencies[6]);
    response.getMetisMisoSettings()->setRx8CenterFrequency(settings.m_rxCenterFrequencies[7]);

    response.getMetisMisoSettings()->setRx1SubsamplingIndex(settings.m_rxSubsamplingIndexes[0]);
    response.getMetisMisoSettings()->setRx2SubsamplingIndex(settings.m_rxSubsamplingIndexes[1]);
    response.getMetisMisoSettings()->setRx3SubsamplingIndex(settings.m_rxSubsamplingIndexes[2]);
    response.getMetisMisoSettings()->setRx4SubsamplingIndex(settings.m_rxSubsamplingIndexes[3]);
    response.getMetisMisoSettings()->setRx5SubsamplingIndex(settings.m_rxSubsamplingIndexes[4]);
    response.getMetisMisoSettings()->setRx6SubsamplingIndex(settings.m_rxSubsamplingIndexes[5]);
    response.getMetisMisoSettings()->setRx7SubsamplingIndex(settings.m_rxSubsamplingIndexes[6]);
    response.getMetisMisoSettings()->setRx8SubsamplingIndex(settings.m_rxSubsamplingIndexes[7]);

    response.getMetisMisoSettings()->setTxCenterFrequency(settings.m_txCenterFrequency);
    response.getMetisMisoSettings()->setRxTransverterMode(settings.m_rxTransverterMode ? 1 : 0);
    response.getMetisMisoSettings()->setRxTransverterDeltaFrequency(settings.m_rxTransverterDeltaFrequency);
    response.getMetisMisoSettings()->setTxTransverterMode(settings.m_txTransverterMode ? 1 : 0);
    response.getMetisMisoSettings()->setTxTransverterDeltaFrequency(settings.m_txTransverterDeltaFrequency);
    response.getMetisMisoSettings()->setIqOrder(settings.m_iqOrder ? 1 : 0);
    response.getMetisMisoSettings()->setSampleRateIndex(settings.m_sampleRateIndex);
    response.getMetisMisoSettings()->setLog2Decim(settings.m_log2Decim);
    response.getMetisMisoSettings()->setLOppmTenths(settings.m_LOppmTenths);
    response.getMetisMisoSettings()->setPreamp(settings.m_preamp ? 1 : 0);
    response.getMetisMisoSettings()->setRandom(settings.m_random ? 1 : 0);
    response.getMetisMisoSettings()->setDither(settings.m_dither ? 1 : 0);
    response.getMetisMisoSettings()->setDuplex(settings.m_duplex ? 1 : 0);
    response.getMetisMisoSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getMetisMisoSettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    response.getMetisMisoSettings()->setTxDrive(settings.m_txDrive);
    response.getMetisMisoSettings()->setSpectrumStreamIndex(settings.m_spectrumStreamIndex);
    response.getMetisMisoSettings()->setStreamIndex(settings.m_streamIndex);
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
        swgMetisMISOSettings->setRx1CenterFrequency(settings.m_rxCenterFrequencies[0]);
    }
    if (deviceSettingsKeys.contains("rx2CenterFrequency") || force) {
        swgMetisMISOSettings->setRx2CenterFrequency(settings.m_rxCenterFrequencies[1]);
    }
    if (deviceSettingsKeys.contains("rx3CenterFrequency") || force) {
        swgMetisMISOSettings->setRx3CenterFrequency(settings.m_rxCenterFrequencies[2]);
    }
    if (deviceSettingsKeys.contains("rx4CenterFrequency") || force) {
        swgMetisMISOSettings->setRx4CenterFrequency(settings.m_rxCenterFrequencies[3]);
    }
    if (deviceSettingsKeys.contains("rx5CenterFrequency") || force) {
        swgMetisMISOSettings->setRx5CenterFrequency(settings.m_rxCenterFrequencies[4]);
    }
    if (deviceSettingsKeys.contains("rx6CenterFrequency") || force) {
        swgMetisMISOSettings->setRx6CenterFrequency(settings.m_rxCenterFrequencies[5]);
    }
    if (deviceSettingsKeys.contains("rx7CenterFrequency") || force) {
        swgMetisMISOSettings->setRx7CenterFrequency(settings.m_rxCenterFrequencies[6]);
    }
    if (deviceSettingsKeys.contains("rx8CenterFrequency") || force) {
        swgMetisMISOSettings->setRx8CenterFrequency(settings.m_rxCenterFrequencies[7]);
    }

    if (deviceSettingsKeys.contains("rx1SubsamplingIndex") || force) {
        swgMetisMISOSettings->setRx1SubsamplingIndex(settings.m_rxSubsamplingIndexes[0]);
    }
    if (deviceSettingsKeys.contains("rx2SubsamplingIndex") || force) {
        swgMetisMISOSettings->setRx2SubsamplingIndex(settings.m_rxSubsamplingIndexes[1]);
    }
    if (deviceSettingsKeys.contains("rx3SubsamplingIndex") || force) {
        swgMetisMISOSettings->setRx3SubsamplingIndex(settings.m_rxSubsamplingIndexes[2]);
    }
    if (deviceSettingsKeys.contains("rx4SubsamplingIndex") || force) {
        swgMetisMISOSettings->setRx4SubsamplingIndex(settings.m_rxSubsamplingIndexes[3]);
    }
    if (deviceSettingsKeys.contains("rx5SubsamplingIndex") || force) {
        swgMetisMISOSettings->setRx5SubsamplingIndex(settings.m_rxSubsamplingIndexes[4]);
    }
    if (deviceSettingsKeys.contains("rx6SubsamplingIndex") || force) {
        swgMetisMISOSettings->setRx6SubsamplingIndex(settings.m_rxSubsamplingIndexes[5]);
    }
    if (deviceSettingsKeys.contains("rx7SubsamplingIndex") || force) {
        swgMetisMISOSettings->setRx7SubsamplingIndex(settings.m_rxSubsamplingIndexes[6]);
    }
    if (deviceSettingsKeys.contains("rx8SubsamplingIndex") || force) {
        swgMetisMISOSettings->setRx8SubsamplingIndex(settings.m_rxSubsamplingIndexes[7]);
    }

    if (deviceSettingsKeys.contains("txCenterFrequency") || force) {
        swgMetisMISOSettings->setTxCenterFrequency(settings.m_txCenterFrequency);
    }
    if (deviceSettingsKeys.contains("rxTransverterMode") || force) {
        swgMetisMISOSettings->setRxTransverterMode(settings.m_rxTransverterMode ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("rxTransverterDeltaFrequency") || force) {
        swgMetisMISOSettings->setRxTransverterDeltaFrequency(settings.m_rxTransverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("txTransverterMode") || force) {
        swgMetisMISOSettings->setTxTransverterMode(settings.m_txTransverterMode ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("txTransverterDeltaFrequency") || force) {
        swgMetisMISOSettings->setTxTransverterDeltaFrequency(settings.m_txTransverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("iqOrder") || force) {
        swgMetisMISOSettings->setIqOrder(settings.m_iqOrder ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("sampleRateIndex") || force) {
        swgMetisMISOSettings->setSampleRateIndex(settings.m_sampleRateIndex);
    }
    if (deviceSettingsKeys.contains("log2Decim") || force) {
        swgMetisMISOSettings->setLog2Decim(settings.m_log2Decim);
    }
    if (deviceSettingsKeys.contains("LOppmTenths") || force) {
        swgMetisMISOSettings->setLOppmTenths(settings.m_LOppmTenths);
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
    if (deviceSettingsKeys.contains("txDrive") || force) {
        swgMetisMISOSettings->setTxDrive(settings.m_txDrive);
    }
    if (deviceSettingsKeys.contains("spectrumStreamIndex") || force) {
        swgMetisMISOSettings->setSpectrumStreamIndex(settings.m_spectrumStreamIndex);
    }
    if (deviceSettingsKeys.contains("streamIndex") || force) {
        swgMetisMISOSettings->setStreamIndex(settings.m_streamIndex);
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
