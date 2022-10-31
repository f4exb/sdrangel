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

#include <QDebug>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QBuffer>

#include "SWGDeviceSettings.h"
#include "SWGLimeSdrMIMOSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGDeviceActions.h"
#include "SWGLimeSdrMIMOReport.h"

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/dspdevicemimoengine.h"
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"
#include "limesdr/devicelimesdrparam.h"
#include "limesdr/devicelimesdrshared.h"

#include "limesdrmithread.h"
#include "limesdrmothread.h"
#include "limesdrmimo.h"

MESSAGE_CLASS_DEFINITION(LimeSDRMIMO::MsgConfigureLimeSDRMIMO, Message)
MESSAGE_CLASS_DEFINITION(LimeSDRMIMO::MsgGetStreamInfo, Message)
MESSAGE_CLASS_DEFINITION(LimeSDRMIMO::MsgGetDeviceInfo, Message)
MESSAGE_CLASS_DEFINITION(LimeSDRMIMO::MsgReportStreamInfo, Message)
MESSAGE_CLASS_DEFINITION(LimeSDRMIMO::MsgStartStop, Message)

LimeSDRMIMO::LimeSDRMIMO(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
    m_sourceThread(nullptr),
    m_sinkThread(nullptr),
	m_deviceDescription("LimeSDRMIMO"),
	m_runningRx(false),
    m_runningTx(false),
    m_deviceParams(nullptr),
    m_open(false)
{
    for (unsigned int channel = 0; channel < 2; channel++)
    {
        m_rxChannelEnabled[channel] = false;
        m_txChannelEnabled[channel] = false;
        m_rxStreamStarted[channel] = false;
        m_txStreamStarted[channel] = false;
        m_rxStreams[channel].handle = 0;
        m_txStreams[channel].handle = 0;
    }

    m_open = openDevice();
    m_mimoType = MIMOHalfSynchronous;
    m_sampleMIFifo.init(2, 4096 * 64);
    m_sampleMOFifo.init(2, 4096 * 64);
    m_deviceAPI->setNbSourceStreams(m_deviceParams->m_nbRxChannels);
    m_deviceAPI->setNbSinkStreams(m_deviceParams->m_nbTxChannels);
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &LimeSDRMIMO::networkManagerFinished
    );
}

LimeSDRMIMO::~LimeSDRMIMO()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &LimeSDRMIMO::networkManagerFinished
    );
    delete m_networkManager;
    closeDevice();
}

bool LimeSDRMIMO::openDevice()
{
    m_deviceParams = new DeviceLimeSDRParams();
    char serial[256];
    strcpy(serial, qPrintable(m_deviceAPI->getSamplingDeviceSerial()));

    if (!m_deviceParams->open(serial)) {
        return false;
    }

    for (unsigned int rxChannel = 0; rxChannel < m_deviceParams->m_nbRxChannels; rxChannel++)
    {
        if (LMS_EnableChannel(m_deviceParams->getDevice(), LMS_CH_RX, rxChannel, true) != 0)
        {
            qCritical("LimeSDRMIMO::openDevice: cannot enable Rx channel %d", rxChannel);
            return false;
        }
        else
        {
            qDebug("LimeSDRMIMO::openDevice: Rx channel %d enabled", rxChannel);
            m_rxChannelEnabled[rxChannel] = true;
        }
    }

    for (unsigned int txChannel = 0; txChannel < m_deviceParams->m_nbTxChannels; txChannel++)
    {
        if (LMS_EnableChannel(m_deviceParams->getDevice(), LMS_CH_TX, txChannel, true) != 0)
        {
            qCritical("LimeSDRMIMO::openDevice: cannot enable Tx channel %d", txChannel);
            return false;
        }
        else
        {
            qDebug("LimeSDRMIMO::openDevice: Tx channel %d enabled", txChannel);
            m_txChannelEnabled[txChannel] = true;
        }
    }

    return true;
}

void LimeSDRMIMO::closeDevice()
{
    if (m_deviceParams == nullptr) { // was never open
        return;
    }

    if (m_runningRx) {
        stopRx();
    }

    if (m_runningTx) {
        stopTx();
    }

    for (unsigned int rxChannel = 0; rxChannel < m_deviceParams->m_nbRxChannels; rxChannel++)
    {
        if (LMS_EnableChannel(m_deviceParams->getDevice(), LMS_CH_RX, rxChannel, false) != 0) {
            qWarning("LimeSDRMIMO::closeDevice: cannot disable Rx channel %d", rxChannel);
        } else {
            qDebug("LimeSDRMIMO::closeDevice: Rx channel %d disabled", rxChannel);
        }
    }

    for (unsigned int txChannel = 0; txChannel < m_deviceParams->m_nbTxChannels; txChannel++)
    {
        if (LMS_EnableChannel(m_deviceParams->getDevice(), LMS_CH_TX, txChannel, false) != 0) {
            qWarning("LimeSDROutput::closeDevice: cannot disable Tx channel %d", txChannel);
        } else {
            qDebug("LimeSDROutput::closeDevice: Tx channel %d released", txChannel);
        }
    }

    m_deviceParams->close();
    delete m_deviceParams;
    m_deviceParams = nullptr;
}

bool LimeSDRMIMO::setupRxStream(unsigned int channel)
{
    // channel index out of range
    if (channel >= m_deviceAPI->getNbSourceStreams()) {
        return false;
    }

    // set up the stream
    m_rxStreams[channel].channel =  channel | LMS_ALIGN_CH_PHASE; // channel number
    m_rxStreams[channel].fifoSize = 1024 * 256;               // fifo size in samples
    m_rxStreams[channel].throughputVsLatency = 0.5;           // optimize for min latency
    m_rxStreams[channel].isTx = false;                        // RX channel
    m_rxStreams[channel].dataFmt = lms_stream_t::LMS_FMT_I12; // 12-bit integers

    if (LMS_SetupStream(m_deviceParams->getDevice(), &m_rxStreams[channel]) != 0)
    {
        qCritical("LimeSDRMIMO::setupRxStream: cannot setup the stream on Rx channel %d", channel);
        return false;
    }
    else
    {
        qDebug("LimeSDRMIMO::setupRxStream: stream set up on Rx channel %d", channel);
    }

    return true;
}

void LimeSDRMIMO::destroyRxStream(unsigned int channel)
{
    // destroy the stream
    if (LMS_DestroyStream(m_deviceParams->getDevice(), &m_rxStreams[channel]) != 0) {
        qWarning("LimeSDRMIMO::destroyRxStream: cannot destroy the stream on Rx channel %d", channel);
    } else {
        qDebug("LimeSDRMIMO::destroyRxStream: stream destroyed on Rx channel %d", channel);
    }

    m_rxStreams[channel].handle = 0;
}

bool LimeSDRMIMO::setupTxStream(unsigned int channel)
{
    // channel index out of range
    if (channel >= m_deviceAPI->getNbSinkStreams()) {
        return false;
    }

    // set up the stream
    m_txStreams[channel].channel = channel | LMS_ALIGN_CH_PHASE; // channel number
    m_txStreams[channel].fifoSize = 1024 * 256;               // fifo size in samples
    m_txStreams[channel].throughputVsLatency = 0.5;           // optimize for min latency
    m_txStreams[channel].isTx = true;                         // TX channel
    m_txStreams[channel].dataFmt = lms_stream_t::LMS_FMT_I12; // 12-bit integers

    if (LMS_SetupStream(m_deviceParams->getDevice(), &m_txStreams[channel]) != 0)
    {
        qCritical("LimeSDROutput::setupTxStream: cannot setup the stream on Tx channel %d", channel);
        return false;
    }
    else
    {
        qDebug("LimeSDROutput::setupTxStream: stream set up on Tx channel %d", channel);
    }

    return true;
}

void LimeSDRMIMO::destroyTxStream(unsigned int channel)
{
    // destroy the stream
    if (LMS_DestroyStream(m_deviceParams->getDevice(), &m_txStreams[channel]) != 0) {
        qWarning("LimeSDROutput::destroyTxStream: cannot destroy the stream on Tx channel %d", channel);
    } else {
        qDebug("LimeSDROutput::destroyTxStream: stream destroyed on Tx channel %d", channel);
    }

    m_txStreams[channel].handle = 0;
}

void LimeSDRMIMO::destroy()
{
    delete this;
}

void LimeSDRMIMO::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

bool LimeSDRMIMO::startRx()
{
    qDebug("LimeSDRMIMO::startRx");
    lms_stream_t *streams[2];

    if (!m_open)
    {
        qCritical("LimeSDRMIMO::startRx: device was not opened");
        return false;
    }

	QMutexLocker mutexLocker(&m_mutex);

    if (m_runningRx) {
        stopRx();
    }

    for (unsigned int channel = 0; channel < 2; channel++)
    {
        if (channel < m_deviceAPI->getNbSourceStreams())
        {
            if (setupRxStream(channel))
            {
                streams[channel] = &m_rxStreams[channel];
                m_rxStreamStarted[channel] = true;
            }
            else
            {
                qInfo("LimeSDRMIMO::startRx: stream Rx %u not started", channel);
                streams[channel] = nullptr;
                m_rxStreamStarted[channel] = false;
            }
        }
        else
        {
            streams[channel] = nullptr;
            m_rxStreamStarted[channel] = false;
        }
    }

    m_sourceThread = new LimeSDRMIThread(streams[0], streams[1]);
    m_sampleMIFifo.reset();
    m_sourceThread->setFifo(&m_sampleMIFifo);
    m_sourceThread->setLog2Decimation(m_settings.m_log2SoftDecim);
    m_sourceThread->setIQOrder(m_settings.m_iqOrder);
	m_sourceThread->startWork();
	mutexLocker.unlock();
	m_runningRx = true;

    return true;
}

void LimeSDRMIMO::stopRx()
{
    qDebug("LimeSDRMIMO::stopRx");

    if (!m_sourceThread) {
        return;
    }

	QMutexLocker mutexLocker(&m_mutex);

    m_sourceThread->stopWork();
    delete m_sourceThread;
    m_sourceThread = nullptr;
    m_runningRx = false;

    for (unsigned int channel = 0; channel < 2; channel++)
    {
        if (m_rxStreamStarted[channel]) {
            destroyRxStream(channel);
        }
    }
}

bool LimeSDRMIMO::startTx()
{
    qDebug("LimeSDRMIMO::startTx");
    lms_stream_t *streams[2];

    if (!m_open)
    {
        qCritical("LimeSDRMIMO::startTx: device was not opened");
        return false;
    }

	QMutexLocker mutexLocker(&m_mutex);

    if (m_runningTx) {
        stopTx();
    }

    for (unsigned int channel = 0; channel < 2; channel++)
    {
        if (channel < m_deviceAPI->getNbSinkStreams())
        {
            if (setupTxStream(channel))
            {
                streams[channel] = &m_txStreams[channel];
                m_txStreamStarted[channel] = true;
            }
            else
            {
                qInfo("LimeSDRMIMO::startTx: stream Tx %u not started", channel);
                streams[channel] = nullptr;
                m_txStreamStarted[channel] = false;
            }
        }
        else
        {
            streams[channel] = nullptr;
            m_txStreamStarted[channel] = false;
        }
    }

    m_sinkThread = new LimeSDRMOThread(streams[0], streams[1]);
    m_sampleMOFifo.reset();
    m_sinkThread->setFifo(&m_sampleMOFifo);
    m_sinkThread->setLog2Interpolation(m_settings.m_log2SoftInterp);
	m_sinkThread->startWork();
	mutexLocker.unlock();
	m_runningTx = true;

    return true;
}

void LimeSDRMIMO::stopTx()
{
    qDebug("LimeSDRMIMO::stopTx");

    if (!m_sinkThread) {
        return;
    }

	QMutexLocker mutexLocker(&m_mutex);

    m_sinkThread->stopWork();
    delete m_sinkThread;
    m_sinkThread = nullptr;
    m_runningTx = false;

    for (unsigned int channel = 0; channel < 2; channel++)
    {
        if (m_txStreamStarted[channel]) {
            destroyTxStream(channel);
        }
    }
}

QByteArray LimeSDRMIMO::serialize() const
{
    return m_settings.serialize();
}

bool LimeSDRMIMO::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureLimeSDRMIMO* message = MsgConfigureLimeSDRMIMO::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureLimeSDRMIMO* messageToGUI = MsgConfigureLimeSDRMIMO::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& LimeSDRMIMO::getDeviceDescription() const
{
	return m_deviceDescription;
}

int LimeSDRMIMO::getSourceSampleRate(int index) const
{
    (void) index;
    int rate = m_settings.m_devSampleRate;
    return (rate / (1<<m_settings.m_log2SoftDecim));
}

int LimeSDRMIMO::getSinkSampleRate(int index) const
{
    (void) index;
    int rate = m_settings.m_devSampleRate;
    return (rate / (1<<m_settings.m_log2SoftInterp));
}

quint64 LimeSDRMIMO::getSourceCenterFrequency(int index) const
{
    (void) index;
    return m_settings.m_rxCenterFrequency;
}

void LimeSDRMIMO::setSourceCenterFrequency(qint64 centerFrequency, int index)
{
    (void) index;
    LimeSDRMIMOSettings settings = m_settings;
    settings.m_rxCenterFrequency = centerFrequency;

    MsgConfigureLimeSDRMIMO* message = MsgConfigureLimeSDRMIMO::create(settings, QList<QString>{"rxCenterFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureLimeSDRMIMO* messageToGUI = MsgConfigureLimeSDRMIMO::create(settings, QList<QString>{"rxCenterFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

quint64 LimeSDRMIMO::getSinkCenterFrequency(int index) const
{
    (void) index;
    return m_settings.m_txCenterFrequency;
}

void LimeSDRMIMO::setSinkCenterFrequency(qint64 centerFrequency, int index)
{
    (void) index;
    LimeSDRMIMOSettings settings = m_settings;
    settings.m_txCenterFrequency = centerFrequency;

    MsgConfigureLimeSDRMIMO* message = MsgConfigureLimeSDRMIMO::create(settings, QList<QString>{"txCenterFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureLimeSDRMIMO* messageToGUI = MsgConfigureLimeSDRMIMO::create(settings, QList<QString>{"txCenterFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool LimeSDRMIMO::handleMessage(const Message& message)
{
    if (MsgConfigureLimeSDRMIMO::match(message))
    {
        MsgConfigureLimeSDRMIMO& conf = (MsgConfigureLimeSDRMIMO&) message;
        qDebug() << "LimeSDRMIMO::handleMessage: MsgConfigureLimeSDRMIMO";

        bool success = applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());

        if (!success) {
            qDebug("LimeSDRMIMO::handleMessage: config error");
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "LimeSDRMIMO::handleMessage: "
            << " " << (cmd.getRxElseTx() ? "Rx" : "Tx")
            << " MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        bool startStopRxElseTx = cmd.getRxElseTx();

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initDeviceEngine(startStopRxElseTx ? 0 : 1)) {
                m_deviceAPI->startDeviceEngine(startStopRxElseTx ? 0 : 1);
            }
        }
        else
        {
            m_deviceAPI->stopDeviceEngine(startStopRxElseTx ? 0 : 1);
        }

        if (m_settings.m_useReverseAPI) {
            webapiReverseSendStartStop(cmd.getStartStop());
        }

        return true;
    }
    else if (MsgGetStreamInfo::match(message))
    {
        MsgGetStreamInfo& cmd = (MsgGetStreamInfo&) message;
        lms_stream_status_t status;
        lms_stream_t *stream;

        if (cmd.getRxElseTx() && (cmd.getChannel() == 0) && m_rxStreams[0].handle) {
            stream = &m_rxStreams[0];
        } else if (cmd.getRxElseTx() && (cmd.getChannel() == 1) && m_rxStreams[1].handle) {
            stream = &m_rxStreams[1];
        } else if (!cmd.getRxElseTx() && (cmd.getChannel() == 0) && m_txStreams[0].handle) {
            stream = &m_txStreams[0];
        } else if (!cmd.getRxElseTx() && (cmd.getChannel() == 1) && m_txStreams[1].handle) {
            stream = &m_txStreams[1];
        } else {
            stream = nullptr;
        }

        if (stream && (LMS_GetStreamStatus(stream, &status) == 0))
        {
            if (getMessageQueueToGUI())
            {
                MsgReportStreamInfo *report = MsgReportStreamInfo::create(
                        true, // Success
                        status.active,
                        status.fifoFilledCount,
                        status.fifoSize,
                        status.underrun,
                        status.overrun,
                        status.droppedPackets,
                        status.linkRate,
                        status.timestamp);
                getMessageQueueToGUI()->push(report);
            }
        }
        else
        {
            if (m_deviceAPI->getSamplingDeviceGUIMessageQueue())
            {
                MsgReportStreamInfo *report = MsgReportStreamInfo::create(
                        false, // Success
                        false, // status.active,
                        0,     // status.fifoFilledCount,
                        16384, // status.fifoSize,
                        0,     // status.underrun,
                        0,     // status.overrun,
                        0,     // status.droppedPackets,
                        0,     // status.linkRate,
                        0);    // status.timestamp);
                m_deviceAPI->getSamplingDeviceGUIMessageQueue()->push(report);
            }
        }

        return true;
    }
    else if (MsgGetDeviceInfo::match(message))
    {
        double temp = 0.0;
        uint8_t gpioPins = 0;

        if (m_deviceParams->getDevice() && (LMS_GetChipTemperature(m_deviceParams->getDevice(), 0, &temp) != 0)) {
            qDebug("LimeSDRMIMO::handleMessage: MsgGetDeviceInfo: cannot get temperature");
        }

        if ((m_deviceParams->m_type != DeviceLimeSDRParams::LimeMini)
            && (m_deviceParams->m_type != DeviceLimeSDRParams::LimeUndefined))
        {
            if (m_deviceParams->getDevice() && (LMS_GPIORead(m_deviceParams->getDevice(), &gpioPins, 1) != 0)) {
                qDebug("LimeSDRMIMO::handleMessage: MsgGetDeviceInfo: cannot get GPIO pins values");
            }
        }

        // send to oneself
        if (getMessageQueueToGUI())
        {
            DeviceLimeSDRShared::MsgReportDeviceInfo *report = DeviceLimeSDRShared::MsgReportDeviceInfo::create(temp, gpioPins);
            getMessageQueueToGUI()->push(report);
        }

        return true;
    }
    else
    {
        return false;
    }
}

bool LimeSDRMIMO::applySettings(const LimeSDRMIMOSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "LimeSDRMIMO::applySettings: force:" << force << settings.getDebugString(settingsKeys, force);
    bool forwardChangeRxDSP  = false;
    bool forwardChangeTxDSP  = false;
    double clockGenFreq      = 0.0;
    bool doCalibrationRx0    = false;
    bool doCalibrationRx1    = false;
    bool doLPCalibrationRx0  = false;
    bool doLPCalibrationRx1  = false;
    bool doCalibrationTx0    = false;
    bool doCalibrationTx1    = false;
    bool doLPCalibrationTx0  = false;
    bool doLPCalibrationTx1  = false;
    bool forceRxNCOFrequency = false;
    bool forceTxNCOFrequency = false;

    qint64 rxDeviceCenterFrequency = settings.m_rxCenterFrequency;
    rxDeviceCenterFrequency -= settings.m_rxTransverterMode ? settings.m_rxTransverterDeltaFrequency : 0;
    rxDeviceCenterFrequency = rxDeviceCenterFrequency < 0 ? 0 : rxDeviceCenterFrequency;

    qint64 txDeviceCenterFrequency = settings.m_txCenterFrequency;
    txDeviceCenterFrequency -= settings.m_txTransverterMode ? settings.m_txTransverterDeltaFrequency : 0;
    txDeviceCenterFrequency = txDeviceCenterFrequency < 0 ? 0 : txDeviceCenterFrequency;

    // Common

    if (LMS_GetClockFreq(m_deviceParams->getDevice(), LMS_CLOCK_CGEN, &clockGenFreq) != 0) {
        qCritical("LimeSDRMIMO::applySettings: could not get clock gen frequency");
    } else {
        qDebug() << "LimeSDRMIMO::applySettings: clock gen frequency: " << clockGenFreq;
    }

    // Rx

    if (settingsKeys.contains("dcBlock") ||
        settingsKeys.contains("iqCorrection") || force)
    {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection, 0);
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection, 1);
    }

    if (settingsKeys.contains("devSampleRate")
       || settingsKeys.contains("log2HardDecim") || force)
    {
        forwardChangeRxDSP = true;

        if (m_deviceParams->getDevice())
        {
            if (LMS_SetSampleRateDir(
                m_deviceParams->getDevice(),
                LMS_CH_RX,
                settings.m_devSampleRate,
                1<<settings.m_log2HardDecim) < 0)
            {
                qCritical("LimeSDRMIMO::applySettings: could not set sample rate to %d with Rx oversampling of %d",
                        settings.m_devSampleRate,
                        1<<settings.m_log2HardDecim);
            }
            else
            {
                m_deviceParams->m_log2OvSRRx = settings.m_log2HardDecim;
                m_deviceParams->m_sampleRate = settings.m_devSampleRate;
                forceRxNCOFrequency = true;
                qDebug("LimeSDRMIMO::applySettings: set sample rate set to %d with Rx oversampling of %d",
                        settings.m_devSampleRate,
                        1<<settings.m_log2HardDecim);
            }
        }
    }

    if (settingsKeys.contains("rxCenterFrequency")
        || settingsKeys.contains("rxTransverterMode")
        || settingsKeys.contains("rxTransverterDeltaFrequency")
        || force)
    {
        forwardChangeRxDSP = true;

        if (m_deviceParams->getDevice())
        {
            if (LMS_SetClockFreq(m_deviceParams->getDevice(), LMS_CLOCK_SXR, rxDeviceCenterFrequency) < 0)
            {
                qCritical("LimeSDRMIMO::applySettings: could not set Rx frequency to %lld", rxDeviceCenterFrequency);
            }
            else
            {
                doCalibrationRx0 = true;
                doCalibrationRx1 = true;
                qDebug("LimeSDRMIMO::applySettings: Rx frequency set to %lld", rxDeviceCenterFrequency);
            }
        }
    }

    if (settingsKeys.contains("ncoFrequencyRx") ||
        settingsKeys.contains("ncoEnableRx") || force || forceRxNCOFrequency)
    {
        forwardChangeRxDSP = true;
        applyRxNCOFrequency(0, settings.m_ncoEnableRx, settings.m_ncoFrequencyRx);
        applyRxNCOFrequency(1, settings.m_ncoEnableRx, settings.m_ncoFrequencyRx);
    }

    // Rx0/1

    if (settingsKeys.contains("gainModeRx0") || force)
    {
        applyRxGainMode(
            0,
            doCalibrationRx0,
            settings.m_gainModeRx0,
            m_settings.m_gainRx0,
            m_settings.m_lnaGainRx0,
            m_settings.m_tiaGainRx0,
            m_settings.m_pgaGainRx0
        );
    }

    if (settingsKeys.contains("gainModeRx1") || force)
    {
        applyRxGainMode(
            1,
            doCalibrationRx1,
            settings.m_gainModeRx1,
            m_settings.m_gainRx1,
            m_settings.m_lnaGainRx1,
            m_settings.m_tiaGainRx1,
            m_settings.m_pgaGainRx1
        );
    }

    if ((m_settings.m_gainModeRx0 == LimeSDRMIMOSettings::GAIN_AUTO) && settingsKeys.contains("gainRx0"))
    {
        applyRxGain(0, doCalibrationRx0, settings.m_gainRx0);
    }

    if ((m_settings.m_gainModeRx1 == LimeSDRMIMOSettings::GAIN_AUTO) && settingsKeys.contains("gainRx1"))
    {
        applyRxGain(1, doCalibrationRx1, settings.m_gainRx1);
    }

    if ((m_settings.m_gainModeRx0 == LimeSDRMIMOSettings::GAIN_MANUAL) && settingsKeys.contains("lnaGainRx0"))
    {
        applyRxLNAGain(0, doCalibrationRx0, settings.m_lnaGainRx0);
    }

    if ((m_settings.m_gainModeRx1 == LimeSDRMIMOSettings::GAIN_MANUAL) && settingsKeys.contains("lnaGainRx1"))
    {
        applyRxLNAGain(1, doCalibrationRx1, settings.m_lnaGainRx1);
    }

    if ((m_settings.m_gainModeRx0 == LimeSDRMIMOSettings::GAIN_MANUAL) && settingsKeys.contains("tiaGainRx0"))
    {
        applyRxLNAGain(0, doCalibrationRx0, settings.m_tiaGainRx0);
    }

    if ((m_settings.m_gainModeRx1 == LimeSDRMIMOSettings::GAIN_MANUAL) && settingsKeys.contains("tiaGainRx1"))
    {
        applyRxLNAGain(1, doCalibrationRx1, settings.m_tiaGainRx1);
    }

    if ((m_settings.m_gainModeRx0 == LimeSDRMIMOSettings::GAIN_MANUAL) && settingsKeys.contains("pgaGainRx0"))
    {
        applyRxLNAGain(0, doCalibrationRx0, settings.m_pgaGainRx0);
    }

    if ((m_settings.m_gainModeRx1 == LimeSDRMIMOSettings::GAIN_MANUAL) && settingsKeys.contains("pgaGainRx1"))
    {
        applyRxLNAGain(1, doCalibrationRx1, settings.m_pgaGainRx1);
    }

    if (settingsKeys.contains("lpfBWRx0") || force)
    {
        doLPCalibrationRx0 = true;
    }

    if (settingsKeys.contains("lpfBWRx1") || force)
    {
        doLPCalibrationRx1 = true;
    }

    if (settingsKeys.contains("lpfFIRBWx0") ||
        settingsKeys.contains("lpfFIREnableRx0") || force)
    {
        applyRxLPFIRBW(0, settings.m_lpfFIREnableRx0, settings.m_lpfFIRBWRx0);
    }

    if (settingsKeys.contains("lpfFIRBWx1") ||
        settingsKeys.contains("lpfFIREnableRx1") || force)
    {
        applyRxLPFIRBW(1, settings.m_lpfFIREnableRx1, settings.m_lpfFIRBWRx1);
    }

    if (settingsKeys.contains("log2SoftDecim") || force)
    {
        if (m_sourceThread)
        {
            m_sourceThread->setLog2Decimation(settings.m_log2SoftDecim);
            qDebug() << "LimeSDRMIMO::applySettings: set soft decimation to " << (1<<settings.m_log2SoftDecim);
        }
    }

    if (settingsKeys.contains("iqOrder") || force)
    {
        if (m_sourceThread) {
            m_sourceThread->setIQOrder(settings.m_iqOrder);
        }
    }

    if (settingsKeys.contains("antennaPathRx0") || force)
    {
        applyRxAntennaPath(0, doCalibrationRx0, settings.m_antennaPathRx0);
    }

    if (settingsKeys.contains("antennaPathRx1") || force)
    {
        applyRxAntennaPath(1, doCalibrationRx1, settings.m_antennaPathRx1);
    }

    // Tx

    if (settingsKeys.contains("gainTx0") || force)
    {
        applyTxGain(0, doCalibrationTx0, settings.m_gainTx0);
    }

    if (settingsKeys.contains("gainTx1") || force)
    {
        applyTxGain(1, doCalibrationTx1, settings.m_gainTx1);
    }

    if (settingsKeys.contains("devSampleRate")
       || settingsKeys.contains("log2HardInterp") || force)
    {
        forwardChangeTxDSP = true;

        if (m_deviceParams->getDevice())
        {
            if (LMS_SetSampleRateDir(
                m_deviceParams->getDevice(),
                LMS_CH_TX,
                settings.m_devSampleRate,
                1<<settings.m_log2HardInterp) < 0)
            {
                qCritical("LimeSDRMIMO::applySettings: could not set sample rate to %d with Tx oversampling of %d",
                        settings.m_devSampleRate,
                        1<<settings.m_log2HardInterp);
            }
            else
            {
                m_deviceParams->m_log2OvSRTx = settings.m_log2HardInterp;
                m_deviceParams->m_sampleRate = settings.m_devSampleRate;
                forceTxNCOFrequency = true;
                qDebug("LimeSDRMIMO::applySettings: set sample rate set to %d with Tx oversampling of %d",
                        settings.m_devSampleRate,
                        1<<settings.m_log2HardInterp);
            }
        }
    }

    if (settingsKeys.contains("devSampleRate")
       || settingsKeys.contains("log2SoftInterp") || force)
    {
#if defined(_MSC_VER)
        unsigned int fifoRate = (unsigned int) settings.m_devSampleRate / (1<<settings.m_log2SoftInterp);
        fifoRate = fifoRate < 48000U ? 48000U : fifoRate;
#else
        unsigned int fifoRate = std::max(
            (unsigned int) settings.m_devSampleRate / (1<<settings.m_log2SoftInterp),
            DeviceLimeSDRShared::m_sampleFifoMinRate);
#endif
        m_sampleMOFifo.resize(SampleMOFifo::getSizePolicy(fifoRate));
        qDebug("LimeSDRMIMO::applySettings: resize MO FIFO: rate %u", fifoRate);
    }

    if (settingsKeys.contains("txCenterFrequency")
        || settingsKeys.contains("txTransverterMode")
        || settingsKeys.contains("txTransverterDeltaFrequency")
        || force)
    {
        forwardChangeTxDSP = true;

        if (m_deviceParams->getDevice())
        {
            if (LMS_SetClockFreq(m_deviceParams->getDevice(), LMS_CLOCK_SXT, txDeviceCenterFrequency) < 0)
            {
                qCritical("LimeSDRMIMO::applySettings: could not set Tx frequency to %lld", txDeviceCenterFrequency);
            }
            else
            {
                doCalibrationTx0 = true;
                doCalibrationTx1 = true;
                qDebug("LimeSDRMIMO::applySettings: Tx frequency set to %lld", txDeviceCenterFrequency);
            }
        }
    }

    if (settingsKeys.contains("ncoFrequencyTx") ||
        settingsKeys.contains("ncoEnableTx") || force || forceTxNCOFrequency)
    {
        forwardChangeTxDSP = true;
        applyTxNCOFrequency(0, settings.m_ncoEnableTx, settings.m_ncoFrequencyTx);
        applyTxNCOFrequency(1, settings.m_ncoEnableTx, settings.m_ncoFrequencyTx);
    }

    // Tx 0/1

    if (settingsKeys.contains("lpfBWTx0") || force)
    {
        doLPCalibrationTx0 = true;
    }

    if (settingsKeys.contains("lpfBWTx1") || force)
    {
        doLPCalibrationTx1 = true;
    }

    if (settingsKeys.contains("lpfFIRBWTx0") ||
        settingsKeys.contains("lpfFIREnableTx0") || force)
    {
        applyTxLPFIRBW(0, settings.m_lpfFIREnableTx0, settings.m_lpfFIRBWTx0);
    }

    if (settingsKeys.contains("lpfFIRBWTx1") ||
        settingsKeys.contains("lpfFIREnableTx1") || force)
    {
        applyTxLPFIRBW(1, settings.m_lpfFIREnableTx1, settings.m_lpfFIRBWTx1);
    }

    if (settingsKeys.contains("log2SoftInterp") || force)
    {
        if (m_sinkThread)
        {
            m_sinkThread->setLog2Interpolation(settings.m_log2SoftInterp);
            qDebug() << "LimeSDRMIMO::applySettings: set soft interpolation to " << (1<<settings.m_log2SoftInterp);
        }
    }

    if (settingsKeys.contains("antennaPathTx0") || force)
    {
        applyTxAntennaPath(0, doCalibrationTx0, settings.m_antennaPathTx0);
    }

    if (settingsKeys.contains("antennaPathTx1") || force)
    {
        applyTxAntennaPath(1, doCalibrationTx1, settings.m_antennaPathTx1);
    }

    // Post common

    if (settingsKeys.contains("extClock") ||
        (settings.m_extClock && settingsKeys.contains("extClockFreq")) || force)
    {
        if (DeviceLimeSDR::setClockSource(m_deviceParams->getDevice(),
                settings.m_extClock,
                settings.m_extClockFreq))
        {
            doCalibrationRx0 = true;
            doCalibrationRx1 = true;
            qDebug("LimeSDRMIMO::applySettings: clock set to %s (Ext: %d Hz)",
                    settings.m_extClock ? "external" : "internal",
                    settings.m_extClockFreq);
        }
        else
        {
            qCritical("LimeSDRMIMO::applySettings: could not set clock to %s (Ext: %d Hz)",
                    settings.m_extClock ? "external" : "internal",
                    settings.m_extClockFreq);
        }
    }

    if ((m_deviceParams->m_type != DeviceLimeSDRParams::LimeMini)
        && (m_deviceParams->m_type != DeviceLimeSDRParams::LimeUndefined))
    {
        if (settingsKeys.contains("gpioDir") || force)
        {
            if (LMS_GPIODirWrite(m_deviceParams->getDevice(), &settings.m_gpioDir, 1) != 0) {
                qCritical("LimeSDRMIMO::applySettings: could not set GPIO directions to %u", settings.m_gpioDir);
            } else {
                qDebug("LimeSDRMIMO::applySettings: GPIO directions set to %u", settings.m_gpioDir);
            }
        }

        if (settingsKeys.contains("gpioPins") || force)
        {
            if (LMS_GPIOWrite(m_deviceParams->getDevice(), &settings.m_gpioPins, 1) != 0) {
                qCritical("LimeSDRMIMO::applySettings: could not set GPIO pins to %u", settings.m_gpioPins);
            } else {
                qDebug("LimeSDRMIMO::applySettings: GPIO pins set to %u", settings.m_gpioPins);
            }
        }
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

    double clockGenFreqAfter = 0.0;

    if (LMS_GetClockFreq(m_deviceParams->getDevice(), LMS_CLOCK_CGEN, &clockGenFreqAfter) != 0)
    {
        qCritical("LimeSDRMIMO::applySettings: could not get clock gen frequency");
    }
    else
    {
        qDebug() << "LimeSDRMIMO::applySettings: clock gen frequency after: " << clockGenFreqAfter;
        doCalibrationRx0 = doCalibrationRx0 || (clockGenFreqAfter != clockGenFreq);
        doCalibrationRx1 = doCalibrationRx1 || (clockGenFreqAfter != clockGenFreq);
        doCalibrationTx0 = doCalibrationTx0 || (clockGenFreqAfter != clockGenFreq);
        doCalibrationTx1 = doCalibrationTx1 || (clockGenFreqAfter != clockGenFreq);
    }

    if (doCalibrationRx0 || doLPCalibrationRx0 || doCalibrationRx1 || doLPCalibrationRx1)
    {
        bool ownThreadWasRunning = false;

        if (m_sourceThread && m_sourceThread->isRunning())
        {
            m_sourceThread->stopWork();
            ownThreadWasRunning = true;
        }

        if (doLPCalibrationRx0) {
            applyRxLPCalibration(0, settings.m_lpfBWRx0);
        }
        if (doLPCalibrationRx1) {
            applyRxLPCalibration(1, settings.m_lpfBWRx1);
        }
        if (doCalibrationRx0) {
            applyRxCalibration(0, settings.m_devSampleRate);
        }
        if (doCalibrationRx1) {
            applyRxCalibration(1, settings.m_devSampleRate);
        }

        if (ownThreadWasRunning) {
            m_sourceThread->startWork();
        }
    }

    if (doCalibrationTx0 || doLPCalibrationTx0 || doCalibrationTx1 || doLPCalibrationTx1)
    {
        bool ownThreadWasRunning = false;

        if (m_sinkThread && m_sinkThread->isRunning())
        {
            m_sinkThread->stopWork();
            ownThreadWasRunning = true;
        }

        if (doLPCalibrationTx0) {
            applyTxLPCalibration(0, settings.m_lpfBWTx0);
        }
        if (doLPCalibrationTx1) {
            applyTxLPCalibration(1, settings.m_lpfBWTx1);
        }
        if (doCalibrationTx0) {
            applyTxCalibration(0, settings.m_devSampleRate);
        }
        if (doCalibrationTx1) {
            applyTxCalibration(1, settings.m_devSampleRate);
        }

        if (ownThreadWasRunning) {
            m_sinkThread->startWork();
        }
    }

    // forward changes

    if (forwardChangeRxDSP)
    {
        int sampleRate = settings.m_devSampleRate/(1<<settings.m_log2SoftDecim);
        int ncoShift = m_settings.m_ncoEnableRx ? m_settings.m_ncoFrequencyRx : 0;
        DSPMIMOSignalNotification *notif0 = new DSPMIMOSignalNotification(sampleRate, settings.m_rxCenterFrequency + ncoShift, true, 0);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif0);
        DSPMIMOSignalNotification *notif1 = new DSPMIMOSignalNotification(sampleRate, settings.m_rxCenterFrequency + ncoShift, true, 1);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif1);
    }

    if (forwardChangeTxDSP)
    {
        int sampleRate = settings.m_devSampleRate/(1<<settings.m_log2SoftInterp);
        int ncoShift = m_settings.m_ncoEnableTx ? m_settings.m_ncoFrequencyTx : 0;
        DSPMIMOSignalNotification *notif0 = new DSPMIMOSignalNotification(sampleRate, settings.m_txCenterFrequency + ncoShift, false, 0);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif0);
        DSPMIMOSignalNotification *notif1 = new DSPMIMOSignalNotification(sampleRate, settings.m_txCenterFrequency + ncoShift, false, 1);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif1);
    }

    return true;
}

void LimeSDRMIMO::applyRxGainMode(
    unsigned int channel,
    bool& doCalibration,
    LimeSDRMIMOSettings::RxGainMode gainMode,
    uint32_t gain,
    uint32_t lnaGain,
    uint32_t tiaGain,
    uint32_t pgaGain)
{
    if (gainMode == LimeSDRMIMOSettings::GAIN_AUTO)
    {
        if (m_deviceParams->getDevice() != 0 && m_rxChannelEnabled[channel])
        {
            if (LMS_SetGaindB(m_deviceParams->getDevice(),
                    LMS_CH_RX,
                    channel,
                    gain) < 0)
            {
                qDebug("LimeSDRMIMO::applyRxGainMode: LMS_SetGaindB() Rx%u failed", channel);
            }
            else
            {
                doCalibration = true;
                qDebug("LimeSDRMIMO::applyRxGainMode: Gain (auto) Rx%u set to %u" , channel, gain);
            }
        }
    }
    else
    {
        if (m_deviceParams->getDevice() != 0 && m_rxChannelEnabled[channel])
        {
            if (DeviceLimeSDR::SetRFELNA_dB(m_deviceParams->getDevice(),
                    channel,
                    lnaGain))
            {
                doCalibration = true;
                qDebug("LimeSDRMIMO::applyRxGainMode: LNA gain (manual) Rx%u set to %u", channel, lnaGain);
            }
            else
            {
                qDebug("LimeSDRMIMO::applyRxGainMode: DeviceLimeSDR::SetRFELNA_dB() Rx%u failed", channel);
            }

            if (DeviceLimeSDR::SetRFETIA_dB(m_deviceParams->getDevice(),
                    channel,
                    tiaGain))
            {
                doCalibration = true;
                qDebug("LimeSDRMIMO::applyRxGainMode: TIA gain (manual) Rx%u set to %u", channel, tiaGain);
            }
            else
            {
                qDebug("LimeSDRMIMO::applyRxGainMode: DeviceLimeSDR::SetRFETIA_dB() Rx%u failed", channel);
            }

            if (DeviceLimeSDR::SetRBBPGA_dB(m_deviceParams->getDevice(),
                    channel,
                    pgaGain))
            {
                doCalibration = true;
                qDebug("LimeSDRMIMO::applyRxGainMode: PGA gain (manual) Rx%u set to %u", channel, pgaGain);
            }
            else
            {
                qDebug("LimeSDRMIMO::applyRxGainMode: DeviceLimeSDR::SetRBBPGA_dB() Rx%u failed", channel);
            }
        }
    }
}

void LimeSDRMIMO::applyRxGain(unsigned int channel, bool& doCalibration, uint32_t gain)
{
    if (m_deviceParams->getDevice() != 0 && m_rxChannelEnabled[channel])
    {
        if (LMS_SetGaindB(
            m_deviceParams->getDevice(),
            LMS_CH_RX,
            channel,
            gain) < 0)
        {
            qDebug("LimeSDRInput::applySettings: LMS_SetGaindB() Rx%u failed", channel);
        }
        else
        {
            doCalibration = true;
            qDebug("LimeSDRInput::applySettings: Gain (auto) Rx%u set to %u", channel, gain);
        }
    }
}

void LimeSDRMIMO::applyRxLNAGain(unsigned int channel, bool& doCalibration, uint32_t lnaGain)
{
    if (m_deviceParams->getDevice() && m_rxChannelEnabled[channel])
    {
        if (DeviceLimeSDR::SetRFELNA_dB(
            m_deviceParams->getDevice(),
            channel,
            lnaGain))
        {
            doCalibration = true;
            qDebug("LimeSDRMIMO::applyRxLNAGain: LNA gain (manual) Rx%u set to %u", channel, lnaGain);
        }
        else
        {
            qDebug("LimeSDRMIMO::applyRxLNAGain: DeviceLimeSDR::SetRFELNA_dB() Rx%u failed", channel);
        }
    }
}

void LimeSDRMIMO::applyRxTIAGain(unsigned int channel, bool& doCalibration, uint32_t tiaGain)
{
    if (m_deviceParams->getDevice() && m_rxChannelEnabled[channel])
    {
        if (DeviceLimeSDR::SetRFETIA_dB(
            m_deviceParams->getDevice(),
            channel,
            tiaGain))
        {
            doCalibration = true;
            qDebug("LimeSDRMIMO::applyRxTIAGain: TIA gain (manual) Rx%u set to %u", channel, tiaGain);
        }
        else
        {
            qDebug("LimeSDRMIMO::applyRxTIAGain: DeviceLimeSDR::SetRFETIA_dB() Rx%u failed", channel);
        }
    }
}

void LimeSDRMIMO::applyRxPGAGain(unsigned int channel, bool& doCalibration, uint32_t pgaGain)
{
    if (m_deviceParams->getDevice() != 0 && m_rxChannelEnabled[channel])
    {
        if (DeviceLimeSDR::SetRBBPGA_dB(
            m_deviceParams->getDevice(),
            channel,
            pgaGain))
        {
            doCalibration = true;
            qDebug("LimeSDRMIMO::applyRxPGAGain: PGA gain (manual) Rx%u set to %u", channel, pgaGain);
        }
        else
        {
            qDebug("LimeSDRMIMO::applyRxPGAGain: DeviceLimeSDR::SetRBBPGA_dB() Rx%u failed", channel);
        }
    }
}

void LimeSDRMIMO::applyRxLPFIRBW(unsigned int channel, bool lpfFIREnable, float lpfFIRBW)
{
    if (m_deviceParams->getDevice() && m_rxChannelEnabled[channel])
    {
        if (LMS_SetGFIRLPF(m_deviceParams->getDevice(),
                LMS_CH_RX,
                channel,
                lpfFIREnable,
                lpfFIRBW) < 0)
        {
            qCritical("LimeSDRMIMO::applyLPFIRBwRx: Rx%u could %s and set LPF FIR to %f Hz",
                channel,
                lpfFIREnable ? "enable" : "disable",
                lpfFIRBW);
        }
        else
        {
            //doCalibration = true;
            qDebug("LimeSDRMIMO::applyLPFIRBwRx: Rx%u %sd and set LPF FIR to %f Hz",
                channel,
                lpfFIREnable ? "enable" : "disable",
                lpfFIRBW);
        }
    }
}

void LimeSDRMIMO::applyRxNCOFrequency(unsigned int channel, bool ncoEnable, int ncoFrequency)
{
    if (m_deviceParams->getDevice() != 0 && m_rxChannelEnabled[channel])
    {
        if (DeviceLimeSDR::setNCOFrequency(m_deviceParams->getDevice(),
                LMS_CH_RX,
                channel,
                ncoEnable,
                ncoFrequency))
        {
            //doCalibration = true;
            qDebug("LimeSDRMIMO::applyRxNCOFrequency: Rx%u %sd and set NCO to %d Hz",
                channel,
                ncoEnable ? "enable" : "disable",
                ncoFrequency);
        }
        else
        {
            qCritical("LimeSDRMIMO::applyRxNCOFrequency: Rx%u could not %s and set NCO to %d Hz",
                channel,
                ncoEnable ? "enable" : "disable",
                ncoFrequency);
        }
    }
}

void LimeSDRMIMO::applyRxAntennaPath(unsigned int channel, bool& doCalibration, LimeSDRMIMOSettings::PathRxRFE path)
{
    if (m_deviceParams->getDevice() != 0 && m_rxChannelEnabled[channel])
    {
        if (DeviceLimeSDR::setRxAntennaPath(
            m_deviceParams->getDevice(),
            channel,
            path))
        {
            doCalibration = true;
            qDebug("LimeSDRMIMO::applyRxAntennaPath: set antenna path to %d on Rx channel %u",
                (int) path, channel);
        }
        else
        {
            qCritical("LimeSDRMIMO::applyRxAntennaPath: could not set antenna path to %d on Rx channel %u",
                (int) path, channel);
        }
    }
}

void LimeSDRMIMO::applyRxCalibration(unsigned int channel, qint32 devSampleRate)
{
    if (m_deviceParams->getDevice() && m_rxStreamStarted[channel])
    {
        if (LMS_Calibrate(
            m_deviceParams->getDevice(),
            LMS_CH_RX,
            channel,
            devSampleRate,
            0) < 0)
        {
            qCritical("LimeSDRMIMO::applyRxCalibration: calibration failed on Rx channel %u", channel);
        }
        else
        {
            qDebug("LimeSDRMIMO::applyRxCalibration: calibration successful on Rx channel %u", channel);
        }
    }
}

void LimeSDRMIMO::applyRxLPCalibration(unsigned int channel, float lpfBW)
{
    if (m_deviceParams->getDevice() && m_rxStreamStarted[channel])
    {
        if (LMS_SetLPFBW(m_deviceParams->getDevice(),
                LMS_CH_RX,
                channel,
                lpfBW) < 0)
        {
            qCritical("LimeSDRMIMO::applyRxLPCalibration: could not set LPF to %f Hz on Rx channel %u",
                lpfBW, channel);
        }
        else
        {
            qDebug("LimeSDRMIMO::applyRxLPCalibration: LPF set to %f Hz on Rx channel %u",
                lpfBW, channel);
        }
    }
}

void LimeSDRMIMO::applyTxGain(unsigned int channel, bool& doCalibration, uint32_t gain)
{
    if (m_deviceParams->getDevice() != 0 && m_txChannelEnabled[channel])
    {
        if (LMS_SetGaindB(m_deviceParams->getDevice(),
                LMS_CH_TX,
                channel,
                gain) < 0)
        {
            qDebug("LimeSDRMIMO::applyTxGain: Tx%u LMS_SetGaindB() failed", channel);
        }
        else
        {
            doCalibration = true;
            qDebug("LimeSDROutput::applySettings: Tx%u Gain set to %u", channel, gain);
        }
    }
}

void LimeSDRMIMO::applyTxLPFIRBW(unsigned int channel, bool lpfFIREnable, float lpfFIRBW)
{
    if (m_deviceParams->getDevice() != 0 && m_txChannelEnabled[channel])
    {
        if (LMS_SetGFIRLPF(
            m_deviceParams->getDevice(),
            LMS_CH_TX,
            channel,
            lpfFIREnable,
            lpfFIRBW) < 0)
        {
            qCritical("LimeSDRMIMO::applyTxLPFIRBW: Tx%u could %s and set LPF FIR to %f Hz",
                channel,
                lpfFIREnable ? "enable" : "disable",
                lpfFIRBW);
        }
        else
        {
            qDebug("LimeSDRMIMO::applyTxLPFIRBW: Tx%u %sd and set LPF FIR to %f Hz",
                channel,
                lpfFIREnable ? "enable" : "disable",
                lpfFIRBW);
        }
    }
}

void LimeSDRMIMO::applyTxNCOFrequency(unsigned int channel, bool ncoEnable, int ncoFrequency)
{
    if (m_deviceParams->getDevice() && m_txChannelEnabled[channel])
    {
        if (DeviceLimeSDR::setNCOFrequency(
                m_deviceParams->getDevice(),
                LMS_CH_TX,
                channel,
                ncoEnable,
                ncoFrequency))
        {
            qDebug("LimeSDRMIMO::applyTxNCOFrequency: Tx%u %sd and set NCO to %d Hz",
                channel,
                ncoEnable ? "enable" : "disable",
                ncoFrequency);
        }
        else
        {
            qCritical("LimeSDRMIMO::applyTxNCOFrequency: Tx%u could not %s and set NCO to %d Hz",
                channel,
                ncoEnable ? "enable" : "disable",
                ncoFrequency);
        }
    }
}

void LimeSDRMIMO::applyTxAntennaPath(unsigned int channel, bool& doCalibration, LimeSDRMIMOSettings::PathTxRFE path)
{
    if (m_deviceParams->getDevice() && m_txChannelEnabled[channel])
    {
        if (DeviceLimeSDR::setTxAntennaPath(
            m_deviceParams->getDevice(),
            channel,
            path))
        {
            doCalibration = true;
            qDebug("LimeSDRMIMO::applyTxAntennaPath: Tx%u set antenna path to %d",
                channel, (int) path);
        }
        else
        {
            qCritical("LimeSDRMIMO::applyTxAntennaPath: Tx%u could not set antenna path to %d",
                channel, (int) path);
        }
    }
}

void LimeSDRMIMO::applyTxCalibration(unsigned int channel, qint32 devSampleRate)
{
    if (m_deviceParams->getDevice() && m_txStreamStarted[channel])
    {
        if (m_deviceParams->getDevice())
        {
            if (LMS_Calibrate(
                m_deviceParams->getDevice(),
                LMS_CH_TX,
                channel,
                devSampleRate,
                0) < 0)
            {
                qCritical("LimeSDRMIMO::applyTxCalibration: calibration failed on Tx channel %d", channel);
            }
            else
            {
                qDebug("LimeSDRMIMO::applyTxCalibration: calibration successful on Tx channel %d", channel);
            }
        }
    }
}

void LimeSDRMIMO::applyTxLPCalibration(unsigned int channel, float lpfBW)
{
    if (m_deviceParams->getDevice() && m_txStreamStarted[channel])
    {
        if (LMS_SetLPFBW(
            m_deviceParams->getDevice(),
            LMS_CH_TX,
            channel,
            lpfBW) < 0)
        {
            qCritical("LimeSDRMIMO::applyTxLPCalibration: could not set LPF to %f Hz", lpfBW);
        }
        else
        {
            qDebug("LimeSDRMIMO::applyTxLPCalibration: LPF set to %f Hz", lpfBW);
        }
    }
}

void LimeSDRMIMO::getRxFrequencyRange(uint64_t& min, uint64_t& max, int& step)
{
    min = m_deviceParams->m_loRangeRx.min;
    max = m_deviceParams->m_loRangeRx.max;
    step = m_deviceParams->m_loRangeRx.step;
}

void LimeSDRMIMO::getRxSampleRateRange(int& min, int& max, int& step)
{
    min = m_deviceParams->m_srRangeRx.min;
    max = m_deviceParams->m_srRangeRx.max;
    step = m_deviceParams->m_srRangeRx.step;
}

void LimeSDRMIMO::getRxLPFRange(int& min, int& max, int& step)
{
    min = m_deviceParams->m_lpfRangeRx.min;
    max = m_deviceParams->m_lpfRangeRx.max;
    step = m_deviceParams->m_lpfRangeRx.step;
}

void LimeSDRMIMO::getTxFrequencyRange(uint64_t& min, uint64_t& max, int& step)
{
    min = m_deviceParams->m_loRangeTx.min;
    max = m_deviceParams->m_loRangeTx.max;
    step = m_deviceParams->m_loRangeTx.step;
}

void LimeSDRMIMO::getTxSampleRateRange(int& min, int& max, int& step)
{
    min = m_deviceParams->m_srRangeTx.min;
    max = m_deviceParams->m_srRangeTx.max;
    step = m_deviceParams->m_srRangeTx.step;
}

void LimeSDRMIMO::getTxLPFRange(int& min, int& max, int& step)
{
    min = m_deviceParams->m_lpfRangeTx.min;
    max = m_deviceParams->m_lpfRangeTx.max;
    step = m_deviceParams->m_lpfRangeTx.step;
}

int LimeSDRMIMO::webapiSettingsGet(
    SWGSDRangel::SWGDeviceSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setLimeSdrMimoSettings(new SWGSDRangel::SWGLimeSdrMIMOSettings());
    response.getLimeSdrMimoSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int LimeSDRMIMO::webapiSettingsPutPatch(
    bool force,
    const QStringList& deviceSettingsKeys,
    SWGSDRangel::SWGDeviceSettings& response, // query + response
    QString& errorMessage)
{
    (void) errorMessage;
    LimeSDRMIMOSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureLimeSDRMIMO *msg = MsgConfigureLimeSDRMIMO::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (getMessageQueueToGUI()) // forward to GUI if any
    {
        MsgConfigureLimeSDRMIMO *msgToGUI = MsgConfigureLimeSDRMIMO::create(settings, deviceSettingsKeys, force);
        getMessageQueueToGUI()->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

int LimeSDRMIMO::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setLimeSdrMimoReport(new SWGSDRangel::SWGLimeSdrMIMOReport());
    response.getLimeSdrMimoReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

int LimeSDRMIMO::webapiRunGet(
    int subsystemIndex,
    SWGSDRangel::SWGDeviceState& response,
    QString& errorMessage)
{
    (void) subsystemIndex;
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int LimeSDRMIMO::webapiRun(
    bool run,
    int subsystemIndex,
    SWGSDRangel::SWGDeviceState& response,
    QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    MsgStartStop *message = MsgStartStop::create(run, subsystemIndex == 0);
    m_inputMessageQueue.push(message);

    if (getMessageQueueToGUI()) // forward to GUI if any
    {
        MsgStartStop *msgToGUI = MsgStartStop::create(run, subsystemIndex == 0);
        getMessageQueueToGUI()->push(msgToGUI);
    }

    return 200;
}

void LimeSDRMIMO::webapiFormatDeviceSettings(
    SWGSDRangel::SWGDeviceSettings& response,
    const LimeSDRMIMOSettings& settings)
{
    // Common
    response.getLimeSdrMimoSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getLimeSdrMimoSettings()->setExtClock(settings.m_extClock ? 1 : 0);
    response.getLimeSdrMimoSettings()->setExtClockFreq(settings.m_extClockFreq);

    response.getLimeSdrMimoSettings()->setGpioDir(settings.m_gpioDir);
    response.getLimeSdrMimoSettings()->setGpioPins(settings.m_gpioPins);
    response.getLimeSdrMimoSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getLimeSdrMimoSettings()->getReverseApiAddress()) {
        *response.getLimeSdrMimoSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getLimeSdrMimoSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getLimeSdrMimoSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getLimeSdrMimoSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);

    // Rx
    response.getLimeSdrMimoSettings()->setRxCenterFrequency(settings.m_rxCenterFrequency);
    response.getLimeSdrMimoSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getLimeSdrMimoSettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    response.getLimeSdrMimoSettings()->setLog2HardDecim(settings.m_log2HardDecim);
    response.getLimeSdrMimoSettings()->setLog2SoftDecim(settings.m_log2SoftDecim);
    response.getLimeSdrMimoSettings()->setIqOrder(settings.m_iqOrder ? 1 : 0);
    response.getLimeSdrMimoSettings()->setNcoEnableRx(settings.m_ncoEnableRx ? 1 : 0);
    response.getLimeSdrMimoSettings()->setNcoFrequencyRx(settings.m_ncoFrequencyRx);
    response.getLimeSdrMimoSettings()->setRxTransverterDeltaFrequency(settings.m_rxTransverterDeltaFrequency);
    response.getLimeSdrMimoSettings()->setRxTransverterMode(settings.m_rxTransverterMode ? 1 : 0);
    // Rx0
    response.getLimeSdrMimoSettings()->setAntennaPathRx0((int) settings.m_antennaPathRx0);
    response.getLimeSdrMimoSettings()->setGainRx0(settings.m_gainRx0);
    response.getLimeSdrMimoSettings()->setGainModeRx0((int) settings.m_gainModeRx0);
    response.getLimeSdrMimoSettings()->setLnaGainRx0(settings.m_lnaGainRx0);
    response.getLimeSdrMimoSettings()->setTiaGainRx0(settings.m_tiaGainRx0);
    response.getLimeSdrMimoSettings()->setPgaGainRx0(settings.m_pgaGainRx0);
    response.getLimeSdrMimoSettings()->setLpfBwRx0(settings.m_lpfBWRx0);
    response.getLimeSdrMimoSettings()->setLpfFirEnableRx0(settings.m_lpfFIREnableRx0 ? 1 : 0);
    response.getLimeSdrMimoSettings()->setLpfFirbwRx0(settings.m_lpfFIRBWRx0);
    // Rx1
    response.getLimeSdrMimoSettings()->setAntennaPathRx1((int) settings.m_antennaPathRx1);
    response.getLimeSdrMimoSettings()->setGainRx1(settings.m_gainRx1);
    response.getLimeSdrMimoSettings()->setGainModeRx1((int) settings.m_gainModeRx1);
    response.getLimeSdrMimoSettings()->setLnaGainRx1(settings.m_lnaGainRx1);
    response.getLimeSdrMimoSettings()->setTiaGainRx1(settings.m_tiaGainRx1);
    response.getLimeSdrMimoSettings()->setPgaGainRx1(settings.m_pgaGainRx1);
    response.getLimeSdrMimoSettings()->setLpfBwRx1(settings.m_lpfBWRx1);
    response.getLimeSdrMimoSettings()->setLpfFirEnableRx1(settings.m_lpfFIREnableRx1 ? 1 : 0);
    response.getLimeSdrMimoSettings()->setLpfFirbwRx1(settings.m_lpfFIRBWRx1);
    // Tx
    response.getLimeSdrMimoSettings()->setTxCenterFrequency(settings.m_txCenterFrequency);
    response.getLimeSdrMimoSettings()->setLog2HardInterp(settings.m_log2HardInterp);
    response.getLimeSdrMimoSettings()->setLog2SoftInterp(settings.m_log2SoftInterp);
    response.getLimeSdrMimoSettings()->setNcoEnableTx(settings.m_ncoEnableTx ? 1 : 0);
    response.getLimeSdrMimoSettings()->setNcoFrequencyTx(settings.m_ncoFrequencyTx);
    response.getLimeSdrMimoSettings()->setTxTransverterDeltaFrequency(settings.m_txTransverterDeltaFrequency);
    response.getLimeSdrMimoSettings()->setTxTransverterMode(settings.m_txTransverterMode ? 1 : 0);
    // Tx0
    response.getLimeSdrMimoSettings()->setAntennaPathTx0((int) settings.m_antennaPathTx0);
    response.getLimeSdrMimoSettings()->setGainTx0(settings.m_gainTx0);
    response.getLimeSdrMimoSettings()->setLpfBwTx0(settings.m_lpfBWTx0);
    response.getLimeSdrMimoSettings()->setLpfFirEnableTx0(settings.m_lpfFIREnableTx0 ? 1 : 0);
    response.getLimeSdrMimoSettings()->setLpfFirbwTx0(settings.m_lpfFIRBWTx0);
    // Tx1
    response.getLimeSdrMimoSettings()->setAntennaPathTx1((int) settings.m_antennaPathTx1);
    response.getLimeSdrMimoSettings()->setGainTx1(settings.m_gainTx1);
    response.getLimeSdrMimoSettings()->setLpfBwTx1(settings.m_lpfBWTx1);
    response.getLimeSdrMimoSettings()->setLpfFirEnableTx1(settings.m_lpfFIREnableTx1 ? 1 : 0);
    response.getLimeSdrMimoSettings()->setLpfFirbwTx1(settings.m_lpfFIRBWTx1);
}

void LimeSDRMIMO::webapiUpdateDeviceSettings(
    LimeSDRMIMOSettings& settings,
    const QStringList& deviceSettingsKeys,
    SWGSDRangel::SWGDeviceSettings& response)
{
    // Common
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getLimeSdrMimoSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("extClock")) {
        settings.m_extClock = response.getLimeSdrMimoSettings()->getExtClock() != 0;
    }
    if (deviceSettingsKeys.contains("extClockFreq")) {
        settings.m_extClockFreq = response.getLimeSdrMimoSettings()->getExtClockFreq();
    }
    if (deviceSettingsKeys.contains("gpioDir")) {
        settings.m_gpioDir = response.getLimeSdrMimoSettings()->getGpioDir() & 0xFF;
    }
    if (deviceSettingsKeys.contains("gpioPins")) {
        settings.m_gpioPins = response.getLimeSdrMimoSettings()->getGpioPins() & 0xFF;
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getLimeSdrMimoSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getLimeSdrMimoSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getLimeSdrMimoSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getLimeSdrMimoSettings()->getReverseApiDeviceIndex();
    }

    // Rx
    if (deviceSettingsKeys.contains("rxCenterFrequency")) {
        settings.m_rxCenterFrequency = response.getLimeSdrMimoSettings()->getRxCenterFrequency();
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getLimeSdrMimoSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getLimeSdrMimoSettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("log2HardDecim")) {
        settings.m_log2HardDecim = response.getLimeSdrMimoSettings()->getLog2HardDecim();
    }
    if (deviceSettingsKeys.contains("log2SoftDecim")) {
        settings.m_log2SoftDecim = response.getLimeSdrMimoSettings()->getLog2SoftDecim();
    }
    if (deviceSettingsKeys.contains("iqOrder")) {
        settings.m_iqOrder = response.getLimeSdrMimoSettings()->getIqOrder() != 0;
    }
    if (deviceSettingsKeys.contains("ncoEnableRx")) {
        settings.m_ncoEnableRx = response.getLimeSdrMimoSettings()->getNcoEnableRx() != 0;
    }
    if (deviceSettingsKeys.contains("ncoFrequencyRx")) {
        settings.m_ncoFrequencyRx = response.getLimeSdrMimoSettings()->getNcoFrequencyRx();
    }
    if (deviceSettingsKeys.contains("rxTransverterDeltaFrequency")) {
        settings.m_rxTransverterDeltaFrequency = response.getLimeSdrMimoSettings()->getRxTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("rxTransverterMode")) {
        settings.m_rxTransverterMode = response.getLimeSdrMimoSettings()->getRxTransverterMode() != 0;
    }

    // Rx0
    if (deviceSettingsKeys.contains("antennaPathRx0")) {
        settings.m_antennaPathRx0 = (LimeSDRMIMOSettings::PathRxRFE) response.getLimeSdrMimoSettings()->getAntennaPathRx0();
    }
    if (deviceSettingsKeys.contains("gainModeRx0")) {
        settings.m_gainModeRx0 = (LimeSDRMIMOSettings::RxGainMode) response.getLimeSdrMimoSettings()->getGainModeRx0();
    }
    if (deviceSettingsKeys.contains("gainRx0")) {
        settings.m_gainRx0 = response.getLimeSdrMimoSettings()->getGainRx0();
    }
    if (deviceSettingsKeys.contains("lnaGainRx0")) {
        settings.m_lnaGainRx0 = response.getLimeSdrMimoSettings()->getLnaGainRx0();
    }
    if (deviceSettingsKeys.contains("tiaGainRx0")) {
        settings.m_tiaGainRx0 = response.getLimeSdrMimoSettings()->getTiaGainRx0();
    }
    if (deviceSettingsKeys.contains("pgaGainRx0")) {
        settings.m_pgaGainRx0 = response.getLimeSdrMimoSettings()->getPgaGainRx0();
    }
    if (deviceSettingsKeys.contains("lpfBWRx0")) {
        settings.m_lpfBWRx0 = response.getLimeSdrMimoSettings()->getLpfBwRx0();
    }
    if (deviceSettingsKeys.contains("lpfFIREnableRx0")) {
        settings.m_lpfFIREnableRx0 = response.getLimeSdrMimoSettings()->getLpfFirEnableRx0() != 0;
    }
    if (deviceSettingsKeys.contains("lpfFIRBWRx0")) {
        settings.m_lpfFIRBWRx0 = response.getLimeSdrMimoSettings()->getLpfFirbwRx0();
    }

    // Rx1
    if (deviceSettingsKeys.contains("antennaPathRx1")) {
        settings.m_antennaPathRx1 = (LimeSDRMIMOSettings::PathRxRFE) response.getLimeSdrMimoSettings()->getAntennaPathRx1();
    }
    if (deviceSettingsKeys.contains("gainModeRx1")) {
        settings.m_gainModeRx1 = (LimeSDRMIMOSettings::RxGainMode) response.getLimeSdrMimoSettings()->getGainModeRx1();
    }
    if (deviceSettingsKeys.contains("gainRx1")) {
        settings.m_gainRx1 = response.getLimeSdrMimoSettings()->getGainRx1();
    }
    if (deviceSettingsKeys.contains("lnaGainRx1")) {
        settings.m_lnaGainRx1 = response.getLimeSdrMimoSettings()->getLnaGainRx1();
    }
    if (deviceSettingsKeys.contains("tiaGainRx1")) {
        settings.m_tiaGainRx1 = response.getLimeSdrMimoSettings()->getTiaGainRx1();
    }
    if (deviceSettingsKeys.contains("pgaGainRx1")) {
        settings.m_pgaGainRx1 = response.getLimeSdrMimoSettings()->getPgaGainRx1();
    }
    if (deviceSettingsKeys.contains("lpfBWRx1")) {
        settings.m_lpfBWRx1 = response.getLimeSdrMimoSettings()->getLpfBwRx1();
    }
    if (deviceSettingsKeys.contains("lpfFIREnableRx1")) {
        settings.m_lpfFIREnableRx1 = response.getLimeSdrMimoSettings()->getLpfFirEnableRx1() != 0;
    }
    if (deviceSettingsKeys.contains("lpfFIRBWRx1")) {
        settings.m_lpfFIRBWRx1 = response.getLimeSdrMimoSettings()->getLpfFirbwRx1();
    }

    // Tx
    if (deviceSettingsKeys.contains("txCenterFrequency")) {
        settings.m_txCenterFrequency = response.getLimeSdrMimoSettings()->getTxCenterFrequency();
    }
    if (deviceSettingsKeys.contains("log2HardInterp")) {
        settings.m_log2HardInterp = response.getLimeSdrMimoSettings()->getLog2HardInterp();
    }
    if (deviceSettingsKeys.contains("log2SoftInterp")) {
        settings.m_log2SoftInterp = response.getLimeSdrMimoSettings()->getLog2SoftInterp();
    }
    if (deviceSettingsKeys.contains("ncoEnableTx")) {
        settings.m_ncoEnableTx = response.getLimeSdrMimoSettings()->getNcoEnableTx() != 0;
    }
    if (deviceSettingsKeys.contains("ncoFrequencyTx")) {
        settings.m_ncoFrequencyTx = response.getLimeSdrMimoSettings()->getNcoFrequencyTx();
    }
    if (deviceSettingsKeys.contains("txTransverterDeltaFrequency")) {
        settings.m_txTransverterDeltaFrequency = response.getLimeSdrMimoSettings()->getTxTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("txTransverterMode")) {
        settings.m_txTransverterMode = response.getLimeSdrMimoSettings()->getTxTransverterMode() != 0;
    }

    // Tx0
    if (deviceSettingsKeys.contains("antennaPathTx0")) {
        settings.m_antennaPathTx0 = (LimeSDRMIMOSettings::PathTxRFE) response.getLimeSdrMimoSettings()->getAntennaPathTx0();
    }
    if (deviceSettingsKeys.contains("gainTx0")) {
        settings.m_gainTx0 = response.getLimeSdrMimoSettings()->getGainTx0();
    }
    if (deviceSettingsKeys.contains("lpfBWTx0")) {
        settings.m_lpfBWTx0 = response.getLimeSdrMimoSettings()->getLpfBwTx0();
    }
    if (deviceSettingsKeys.contains("lpfFIREnableTx0")) {
        settings.m_lpfFIREnableTx0 = response.getLimeSdrMimoSettings()->getLpfFirEnableTx0() != 0;
    }
    if (deviceSettingsKeys.contains("lpfFIRBWTx0")) {
        settings.m_lpfFIRBWTx0 = response.getLimeSdrMimoSettings()->getLpfFirbwTx0();
    }

    // Tx1
    if (deviceSettingsKeys.contains("antennaPathTx1")) {
        settings.m_antennaPathTx1 = (LimeSDRMIMOSettings::PathTxRFE) response.getLimeSdrMimoSettings()->getAntennaPathTx0();
    }
    if (deviceSettingsKeys.contains("gainTx1")) {
        settings.m_gainTx1 = response.getLimeSdrMimoSettings()->getGainTx1();
    }
    if (deviceSettingsKeys.contains("lpfBWTx1")) {
        settings.m_lpfBWTx1 = response.getLimeSdrMimoSettings()->getLpfBwTx1();
    }
    if (deviceSettingsKeys.contains("lpfFIREnableTx1")) {
        settings.m_lpfFIREnableTx1 = response.getLimeSdrMimoSettings()->getLpfFirEnableTx1() != 0;
    }
    if (deviceSettingsKeys.contains("lpfFIRBWTx1")) {
        settings.m_lpfFIRBWTx1 = response.getLimeSdrMimoSettings()->getLpfFirbwTx1();
    }
}

void LimeSDRMIMO::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    bool success = false;
    double temp = 0.0;
    uint8_t gpioDir = 0;
    uint8_t gpioPins = 0;
    lms_stream_status_t status;
    status.active = false;
    status.fifoFilledCount = 0;
    status.fifoSize = 1;
    status.underrun = 0;
    status.overrun = 0;
    status.droppedPackets = 0;
    status.linkRate = 0.0;
    status.timestamp = 0;

    if (m_rxStreams[0].handle) {
        success = (m_rxStreams[0].handle && (LMS_GetStreamStatus(&m_rxStreams[0], &status) == 0));
    }

    response.getLimeSdrMimoReport()->setSuccessRx(success ? 1 : 0);
    response.getLimeSdrMimoReport()->setStreamActiveRx(status.active ? 1 : 0);
    response.getLimeSdrMimoReport()->setFifoSizeRx(status.fifoSize);
    response.getLimeSdrMimoReport()->setFifoFillRx(status.fifoFilledCount);
    response.getLimeSdrMimoReport()->setUnderrunCountRx(status.underrun);
    response.getLimeSdrMimoReport()->setOverrunCountRx(status.overrun);
    response.getLimeSdrMimoReport()->setDroppedPacketsCountRx(status.droppedPackets);
    response.getLimeSdrMimoReport()->setLinkRateRx(status.linkRate);
    response.getLimeSdrMimoReport()->setHwTimestamp(status.timestamp);

    if (m_deviceParams->getDevice())
    {
        LMS_GetChipTemperature(m_deviceParams->getDevice(), 0, &temp);
        LMS_GPIODirRead(m_deviceParams->getDevice(), &gpioDir, 1);
        LMS_GPIORead(m_deviceParams->getDevice(), &gpioPins, 1);
    }

    response.getLimeSdrMimoReport()->setTemperature(temp);
    response.getLimeSdrMimoReport()->setGpioDir(gpioDir);
    response.getLimeSdrMimoReport()->setGpioPins(gpioPins);

    success = false;
    status.active = false;
    status.fifoFilledCount = 0;
    status.fifoSize = 1;
    status.underrun = 0;
    status.overrun = 0;
    status.droppedPackets = 0;
    status.linkRate = 0.0;

    if (m_txStreams[0].handle) {
        success = (m_txStreams[0].handle && (LMS_GetStreamStatus(&m_txStreams[0], &status) == 0));
    }

    response.getLimeSdrMimoReport()->setSuccessTx(success ? 1 : 0);
    response.getLimeSdrMimoReport()->setStreamActiveTx(status.active ? 1 : 0);
    response.getLimeSdrMimoReport()->setFifoSizeTx(status.fifoSize);
    response.getLimeSdrMimoReport()->setFifoFillTx(status.fifoFilledCount);
    response.getLimeSdrMimoReport()->setUnderrunCountTx(status.underrun);
    response.getLimeSdrMimoReport()->setOverrunCountTx(status.overrun);
    response.getLimeSdrMimoReport()->setDroppedPacketsCountTx(status.droppedPackets);
    response.getLimeSdrMimoReport()->setLinkRateTx(status.linkRate);
}

void LimeSDRMIMO::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const LimeSDRMIMOSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(2); // MIMO
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("LimeSDR"));
    swgDeviceSettings->setLimeSdrMimoSettings(new SWGSDRangel::SWGLimeSdrMIMOSettings());
    SWGSDRangel::SWGLimeSdrMIMOSettings *swgLimeSdrMIMOSettings = swgDeviceSettings->getLimeSdrMimoSettings();

    // Common
    if (deviceSettingsKeys.contains("devSampleRate") || force) {
        swgLimeSdrMIMOSettings->setDevSampleRate(settings.m_devSampleRate);
    }
    if (deviceSettingsKeys.contains("extClock") || force) {
        swgLimeSdrMIMOSettings->setExtClock(settings.m_extClock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("extClockFreq") || force) {
        swgLimeSdrMIMOSettings->setExtClockFreq(settings.m_extClockFreq);
    }
    if (deviceSettingsKeys.contains("gpioDir") || force) {
        swgLimeSdrMIMOSettings->setGpioDir(settings.m_gpioDir & 0xFF);
    }
    if (deviceSettingsKeys.contains("gpioPins") || force) {
        swgLimeSdrMIMOSettings->setGpioPins(settings.m_gpioPins & 0xFF);
    }
    if (deviceSettingsKeys.contains("useReverseAPI") || force) {
        swgLimeSdrMIMOSettings->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress") || force) {
        swgLimeSdrMIMOSettings->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }
    if (deviceSettingsKeys.contains("reverseAPIPort") || force) {
        swgLimeSdrMIMOSettings->setReverseApiPort(settings.m_reverseAPIPort);
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex") || force) {
        swgLimeSdrMIMOSettings->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    }

    // Rx
    if (deviceSettingsKeys.contains("rxCenterFrequency") || force) {
        swgLimeSdrMIMOSettings->setRxCenterFrequency(settings.m_rxCenterFrequency);
    }
    if (deviceSettingsKeys.contains("dcBlock") || force) {
        swgLimeSdrMIMOSettings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("iqCorrection") || force) {
        swgLimeSdrMIMOSettings->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("log2HardDecim") || force) {
        swgLimeSdrMIMOSettings->setLog2HardDecim(settings.m_log2HardDecim);
    }
    if (deviceSettingsKeys.contains("log2SoftDecim") || force) {
        swgLimeSdrMIMOSettings->setLog2SoftDecim(settings.m_log2SoftDecim);
    }
    if (deviceSettingsKeys.contains("iqOrder") || force) {
        swgLimeSdrMIMOSettings->setIqOrder(settings.m_iqOrder ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("ncoEnableRx") || force) {
        swgLimeSdrMIMOSettings->setNcoEnableRx(settings.m_ncoEnableRx ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("ncoFrequencyRx") || force) {
        swgLimeSdrMIMOSettings->setNcoFrequencyRx(settings.m_ncoFrequencyRx);
    }
    if (deviceSettingsKeys.contains("rxTransverterDeltaFrequency") || force) {
        swgLimeSdrMIMOSettings->setRxTransverterDeltaFrequency(settings.m_rxTransverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("rxTransverterMode") || force) {
        swgLimeSdrMIMOSettings->setRxTransverterMode(settings.m_rxTransverterMode ? 1 : 0);
    }

    // Rx0
    if (deviceSettingsKeys.contains("antennaPathRx0") || force) {
        swgLimeSdrMIMOSettings->setAntennaPathRx0((int) settings.m_antennaPathRx0);
    }
    if (deviceSettingsKeys.contains("gainModeRx0") || force) {
        swgLimeSdrMIMOSettings->setGainModeRx0((int) settings.m_gainModeRx0);
    }
    if (deviceSettingsKeys.contains("gainRx0") || force) {
        swgLimeSdrMIMOSettings->setGainRx0(settings.m_gainRx0);
    }
    if (deviceSettingsKeys.contains("lnaGainRx0") || force) {
        swgLimeSdrMIMOSettings->setLnaGainRx0(settings.m_lnaGainRx0);
    }
    if (deviceSettingsKeys.contains("tiaGainRx0") || force) {
        swgLimeSdrMIMOSettings->setTiaGainRx0(settings.m_tiaGainRx0);
    }
    if (deviceSettingsKeys.contains("pgaGainRx0") || force) {
        swgLimeSdrMIMOSettings->setPgaGainRx0(settings.m_pgaGainRx0);
    }
    if (deviceSettingsKeys.contains("lpfBWRx0") || force) {
        swgLimeSdrMIMOSettings->setLpfBwRx0(settings.m_lpfBWRx0);
    }
    if (deviceSettingsKeys.contains("lpfFIREnableRx0") || force) {
        swgLimeSdrMIMOSettings->setLpfFirEnableRx0(settings.m_lpfFIREnableRx0 ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("lpfFIRBWRx0") || force) {
        swgLimeSdrMIMOSettings->setLpfFirbwRx0(settings.m_lpfFIRBWRx0);
    }

    // Rx1
    if (deviceSettingsKeys.contains("antennaPathRx1") || force) {
        swgLimeSdrMIMOSettings->setAntennaPathRx1((int) settings.m_antennaPathRx1);
    }
    if (deviceSettingsKeys.contains("gainModeRx1") || force) {
        swgLimeSdrMIMOSettings->setGainModeRx1((int) settings.m_gainModeRx1);
    }
    if (deviceSettingsKeys.contains("gainRx1") || force) {
        swgLimeSdrMIMOSettings->setGainRx1(settings.m_gainRx1);
    }
    if (deviceSettingsKeys.contains("lnaGainRx1") || force) {
        swgLimeSdrMIMOSettings->setLnaGainRx1(settings.m_lnaGainRx1);
    }
    if (deviceSettingsKeys.contains("tiaGainRx1") || force) {
        swgLimeSdrMIMOSettings->setTiaGainRx1(settings.m_tiaGainRx1);
    }
    if (deviceSettingsKeys.contains("pgaGainRx1") || force) {
        swgLimeSdrMIMOSettings->setPgaGainRx1(settings.m_pgaGainRx1);
    }
    if (deviceSettingsKeys.contains("lpfBWRx1") || force) {
        swgLimeSdrMIMOSettings->setLpfBwRx1(settings.m_lpfBWRx1);
    }
    if (deviceSettingsKeys.contains("lpfFIREnableRx1") || force) {
        swgLimeSdrMIMOSettings->setLpfFirEnableRx1(settings.m_lpfFIREnableRx1 ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("lpfFIRBWRx1") || force) {
        swgLimeSdrMIMOSettings->setLpfFirbwRx1(settings.m_lpfFIRBWRx1);
    }

    // Tx
    if (deviceSettingsKeys.contains("txCenterFrequency") || force) {
        swgLimeSdrMIMOSettings->setTxCenterFrequency(settings.m_txCenterFrequency);
    }
    if (deviceSettingsKeys.contains("log2HardInterp") || force) {
        swgLimeSdrMIMOSettings->setLog2HardInterp(settings.m_log2HardInterp);
    }
    if (deviceSettingsKeys.contains("log2SoftInterp") || force) {
        swgLimeSdrMIMOSettings->setLog2SoftInterp(settings.m_log2SoftInterp);
    }
    if (deviceSettingsKeys.contains("ncoEnableTx") || force) {
        swgLimeSdrMIMOSettings->setNcoEnableTx(settings.m_ncoEnableTx ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("ncoFrequencyTx") || force) {
        swgLimeSdrMIMOSettings->setNcoFrequencyTx(settings.m_ncoFrequencyTx);
    }
    if (deviceSettingsKeys.contains("txTransverterDeltaFrequency") || force) {
        swgLimeSdrMIMOSettings->setTxTransverterDeltaFrequency(settings.m_txTransverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("txTransverterMode") || force) {
        swgLimeSdrMIMOSettings->setTxTransverterMode(settings.m_txTransverterMode ? 1 : 0);
    }

    // Tx0
    if (deviceSettingsKeys.contains("antennaPathTx0") || force) {
        swgLimeSdrMIMOSettings->setAntennaPathTx0((int) settings.m_antennaPathTx0);
    }
    if (deviceSettingsKeys.contains("gainTx0") || force) {
        swgLimeSdrMIMOSettings->setGainTx0(settings.m_gainTx0);
    }
    if (deviceSettingsKeys.contains("lpfBWTx0") || force) {
        swgLimeSdrMIMOSettings->setLpfBwTx0(settings.m_lpfBWTx0);
    }
    if (deviceSettingsKeys.contains("lpfFIREnableTx0") || force) {
        swgLimeSdrMIMOSettings->setLpfFirEnableTx0(settings.m_lpfFIREnableTx0 ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("lpfFIRBWTx0") || force) {
        swgLimeSdrMIMOSettings->setLpfFirbwTx0(settings.m_lpfFIRBWTx0);
    }

    // Tx1
    if (deviceSettingsKeys.contains("antennaPathTx1") || force) {
        swgLimeSdrMIMOSettings->setAntennaPathTx1((int) settings.m_antennaPathTx1);
    }
    if (deviceSettingsKeys.contains("gainTx1") || force) {
        swgLimeSdrMIMOSettings->setGainTx1(settings.m_gainTx1);
    }
    if (deviceSettingsKeys.contains("lpfBWTx1") || force) {
        swgLimeSdrMIMOSettings->setLpfBwTx1(settings.m_lpfBWTx1);
    }
    if (deviceSettingsKeys.contains("lpfFIREnableTx1") || force) {
        swgLimeSdrMIMOSettings->setLpfFirEnableTx1(settings.m_lpfFIREnableTx1 ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("lpfFIRBWTx1") || force) {
        swgLimeSdrMIMOSettings->setLpfFirbwTx1(settings.m_lpfFIRBWTx1);
    }
}

void LimeSDRMIMO::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(2); // MIMO
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("LimeSDR"));

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

void LimeSDRMIMO::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "LimeSDRMIMO::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("LimeSDRMIMO::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
