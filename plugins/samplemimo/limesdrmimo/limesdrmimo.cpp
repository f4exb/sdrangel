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

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/dspdevicemimoengine.h"
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"
#include "dsp/filerecord.h"
#include "limesdr/devicelimesdrparam.h"
#include "limesdr/devicelimesdrshared.h"

#include "limesdrmithread.h"
#include "limesdrmothread.h"
#include "limesdrmimo.h"

MESSAGE_CLASS_DEFINITION(LimeSDRMIMO::MsgConfigureLimeSDRMIMO, Message)
MESSAGE_CLASS_DEFINITION(LimeSDRMIMO::MsgGetStreamInfo, Message)
MESSAGE_CLASS_DEFINITION(LimeSDRMIMO::MsgGetDeviceInfo, Message)
MESSAGE_CLASS_DEFINITION(LimeSDRMIMO::MsgReportStreamInfo, Message)
MESSAGE_CLASS_DEFINITION(LimeSDRMIMO::MsgFileRecord, Message)
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
    }

    m_open = openDevice();
    m_mimoType = MIMOHalfSynchronous;
    m_sampleMIFifo.init(2, 4096 * 64);
    m_sampleMOFifo.init(2, 4096 * 64);
    m_deviceAPI->setNbSourceStreams(m_deviceParams->m_nbRxChannels);
    m_deviceAPI->setNbSinkStreams(m_deviceParams->m_nbTxChannels);
    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

LimeSDRMIMO::~LimeSDRMIMO()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
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
    m_rxStreams[channel].channel =  channel; // channel number
    m_rxStreams[channel].fifoSize = 1024 * 1024;              // fifo size in samples (SR / 10 take ~5MS/s)
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
    m_txStreams[channel].channel = channel; // channel number
    m_txStreams[channel].fifoSize = 1024 * 1024;              // fifo size in samples (SR / 10 take ~5MS/s)
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
    applySettings(m_settings, true);
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

    MsgConfigureLimeSDRMIMO* message = MsgConfigureLimeSDRMIMO::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureLimeSDRMIMO* messageToGUI = MsgConfigureLimeSDRMIMO::create(m_settings, true);
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

    MsgConfigureLimeSDRMIMO* message = MsgConfigureLimeSDRMIMO::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureLimeSDRMIMO* messageToGUI = MsgConfigureLimeSDRMIMO::create(settings, false);
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

    MsgConfigureLimeSDRMIMO* message = MsgConfigureLimeSDRMIMO::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureLimeSDRMIMO* messageToGUI = MsgConfigureLimeSDRMIMO::create(settings, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool LimeSDRMIMO::handleMessage(const Message& message)
{
    if (MsgConfigureLimeSDRMIMO::match(message))
    {
        MsgConfigureLimeSDRMIMO& conf = (MsgConfigureLimeSDRMIMO&) message;
        qDebug() << "LimeSDRMIMO::handleMessage: MsgConfigureLimeSDRMIMO";

        bool success = applySettings(conf.getSettings(), conf.getForce());

        if (!success) {
            qDebug("LimeSDRMIMO::handleMessage: config error");
        }

        return true;
    }
    else if (MsgFileRecord::match(message))
    {
        // TODO
        // MsgFileRecord& conf = (MsgFileRecord&) message;
        // qDebug() << "LimeSDRMIMO::handleMessage: MsgFileRecord: " << conf.getStartStop();
        // int istream = conf.getStreamIndex();

        // if (conf.getStartStop())
        // {
        //     if (m_settings.m_fileRecordName.size() != 0) {
        //         m_fileSinks[istream]->setFileName(m_settings.m_fileRecordName + "_0.sdriq");
        //     } else {
        //         m_fileSinks[istream]->genUniqueFileName(m_deviceAPI->getDeviceUID(), istream);
        //     }

        //     m_fileSinks[istream]->startRecording();
        // }
        // else
        // {
        //     m_fileSinks[istream]->stopRecording();
        // }

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
    else
    {
        return false;
    }
}

bool LimeSDRMIMO::applySettings(const LimeSDRMIMOSettings& settings, bool force)
{
    QList<QString> reverseAPIKeys;
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

    qDebug() << "LimeSDRMIMO::applySettings: "
        << " m_devSampleRate: " << settings.m_devSampleRate
        << " m_LOppmTenths: " << settings.m_LOppmTenths
        << " m_gpioDir: " << settings.m_gpioDir
        << " m_gpioPins: " << settings.m_gpioPins
        << " m_extClock: " << settings.m_extClock
        << " m_extClockFreq: " << settings.m_extClockFreq
        << " m_fileRecordName: " << settings.m_fileRecordName
        << " m_useReverseAPI: " << settings.m_useReverseAPI
        << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
        << " m_reverseAPIPort: " << settings.m_reverseAPIPort
        << " m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex
        // Rx general
        << " m_rxCenterFrequency: " << settings.m_rxCenterFrequency
        << " m_log2HardDecim: " << settings.m_log2HardDecim
        << " m_log2SoftDecim: " << settings.m_log2SoftDecim
        << " m_dcBlock: " << settings.m_dcBlock
        << " m_iqCorrection: " << settings.m_iqCorrection
        << " m_rxTransverterMode: " << settings.m_rxTransverterMode
        << " m_rxTransverterDeltaFrequency: " << settings.m_rxTransverterDeltaFrequency
        // Rx0
        << " m_lpfBWRx0: " << settings.m_lpfBWRx0
        << " m_lpfFIREnableRx0: " << settings.m_lpfFIREnableRx0
        << " m_lpfFIRBWRx0: " << settings.m_lpfFIRBWRx0
        << " m_gainRx0: " << settings.m_gainRx0
        << " m_ncoEnableRx0: " << settings.m_ncoEnableRx0
        << " m_ncoFrequencyRx0: " << settings.m_ncoFrequencyRx0
        << " m_antennaPathRx0: " << settings.m_antennaPathRx0
        << " m_gainModeRx0: " << settings.m_gainModeRx0
        << " m_lnaGainRx0: " << settings.m_lnaGainRx0
        << " m_tiaGainRx0: " << settings.m_tiaGainRx0
        << " m_pgaGainRx0: " << settings.m_pgaGainRx0
        // Rx1
        << " m_lpfBWRx1: " << settings.m_lpfBWRx1
        << " m_lpfFIREnableRx1: " << settings.m_lpfFIREnableRx1
        << " m_lpfFIRBWRx1: " << settings.m_lpfFIRBWRx1
        << " m_gainRx1: " << settings.m_gainRx1
        << " m_ncoEnableRx1: " << settings.m_ncoEnableRx1
        << " m_ncoFrequencyRx1: " << settings.m_ncoFrequencyRx1
        << " m_antennaPathRx1: " << settings.m_antennaPathRx1
        << " m_gainModeRx1: " << settings.m_gainModeRx1
        << " m_lnaGainRx1: " << settings.m_lnaGainRx1
        << " m_tiaGainRx1: " << settings.m_tiaGainRx1
        << " m_pgaGainRx1: " << settings.m_pgaGainRx1
        // Tx general
        << " m_txCenterFrequency: " << settings.m_txCenterFrequency
        << " m_log2HardInterp: " << settings.m_log2HardInterp
        << " m_log2SoftInterp: " << settings.m_log2SoftInterp
        << " m_txTransverterMode: " << settings.m_txTransverterMode
        << " m_txTransverterDeltaFrequency: " << settings.m_txTransverterDeltaFrequency
        // Tx0
        << " m_lpfBWTx0: " << settings.m_lpfBWTx0
        << " m_lpfFIREnableTx0: " << settings.m_lpfFIREnableTx0
        << " m_lpfFIRBWTx0: " << settings.m_lpfFIRBWTx0
        << " m_gainTx0: " << settings.m_gainTx0
        << " m_ncoEnableTx0: " << settings.m_ncoEnableTx0
        << " m_ncoFrequencyTx0: " << settings.m_ncoFrequencyTx0
        << " m_antennaPathTx0: " << settings.m_antennaPathTx0
        // Tx1
        << " m_lpfBWTx1: " << settings.m_lpfBWTx1
        << " m_lpfFIREnableTx1: " << settings.m_lpfFIREnableTx1
        << " m_lpfFIRBWTx1: " << settings.m_lpfFIRBWTx1
        << " m_gainTx1: " << settings.m_gainTx1
        << " m_ncoEnableTx1: " << settings.m_ncoEnableTx1
        << " m_ncoFrequencyTx1: " << settings.m_ncoFrequencyTx1
        << " m_antennaPathTx1: " << settings.m_antennaPathTx1
        // force
        << " force: " << force;

    qint64 rxDeviceCenterFrequency = settings.m_rxCenterFrequency;
    rxDeviceCenterFrequency -= settings.m_rxTransverterMode ? settings.m_rxTransverterDeltaFrequency : 0;
    rxDeviceCenterFrequency = rxDeviceCenterFrequency < 0 ? 0 : rxDeviceCenterFrequency;

    qint64 txDeviceCenterFrequency = settings.m_txCenterFrequency;
    txDeviceCenterFrequency -= settings.m_txTransverterMode ? settings.m_txTransverterDeltaFrequency : 0;
    txDeviceCenterFrequency = txDeviceCenterFrequency < 0 ? 0 : txDeviceCenterFrequency;

    if (LMS_GetClockFreq(m_deviceParams->getDevice(), LMS_CLOCK_CGEN, &clockGenFreq) != 0) {
        qCritical("LimeSDRMIMO::applySettings: could not get clock gen frequency");
    } else {
        qDebug() << "LimeSDRMIMO::applySettings: clock gen frequency: " << clockGenFreq;
    }

    if ((m_settings.m_devSampleRate != settings.m_devSampleRate) || force) {
        reverseAPIKeys.append("devSampleRate");
    }
    if ((m_settings.m_dcBlock != settings.m_dcBlock) || force) {
        reverseAPIKeys.append("dcBlock");
    }
    if ((m_settings.m_iqCorrection != settings.m_iqCorrection) || force) {
        reverseAPIKeys.append("iqCorrection");
    }

    // Rx

    if ((m_settings.m_dcBlock != settings.m_dcBlock) ||
        (m_settings.m_iqCorrection != settings.m_iqCorrection) || force)
    {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection, 0);
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection, 1);
    }

    if ((m_settings.m_gainModeRx0 != settings.m_gainModeRx0) || force)
    {
        reverseAPIKeys.append("gainModeRx0");
        applyRxGainMode(
            0,
            doCalibrationRx0,
            settings.m_gainModeRx0,
            settings.m_gainRx0,
            settings.m_lnaGainRx0,
            settings.m_tiaGainRx0,
            settings.m_pgaGainRx0
        );
    }

    if ((m_settings.m_gainModeRx1 != settings.m_gainModeRx1) || force)
    {
        reverseAPIKeys.append("gainModeRx1");
        applyRxGainMode(
            1,
            doCalibrationRx1,
            settings.m_gainModeRx1,
            settings.m_gainRx1,
            settings.m_lnaGainRx1,
            settings.m_tiaGainRx1,
            settings.m_pgaGainRx1
        );
    }

    if ((m_settings.m_gainModeRx0 == LimeSDRMIMOSettings::GAIN_AUTO) && (m_settings.m_gainRx0 != settings.m_gainRx0))
    {
        reverseAPIKeys.append("gainRx0");
        applyRxGain(0, doCalibrationRx0, settings.m_gainRx0);
    }

    if ((m_settings.m_gainModeRx1 == LimeSDRMIMOSettings::GAIN_AUTO) && (m_settings.m_gainRx1 != settings.m_gainRx1))
    {
        reverseAPIKeys.append("gainRx1");
        applyRxGain(1, doCalibrationRx1, settings.m_gainRx1);
    }

    if ((m_settings.m_gainModeRx0 == LimeSDRMIMOSettings::GAIN_MANUAL) && (m_settings.m_lnaGainRx0 != settings.m_lnaGainRx0))
    {
        reverseAPIKeys.append("lnaGainRx0");
        applyRxLNAGain(0, doCalibrationRx0, settings.m_lnaGainRx0);
    }

    if ((m_settings.m_gainModeRx1 == LimeSDRMIMOSettings::GAIN_MANUAL) && (m_settings.m_lnaGainRx1 != settings.m_lnaGainRx1))
    {
        reverseAPIKeys.append("lnaGainRx1");
        applyRxLNAGain(1, doCalibrationRx1, settings.m_lnaGainRx1);
    }

    if ((m_settings.m_gainModeRx0 == LimeSDRMIMOSettings::GAIN_MANUAL) && (m_settings.m_tiaGainRx0 != settings.m_tiaGainRx0))
    {
        reverseAPIKeys.append("tiaGainRx0");
        applyRxLNAGain(0, doCalibrationRx0, settings.m_tiaGainRx0);
    }

    if ((m_settings.m_gainModeRx1 == LimeSDRMIMOSettings::GAIN_MANUAL) && (m_settings.m_tiaGainRx1 != settings.m_tiaGainRx1))
    {
        reverseAPIKeys.append("tiaGainRx1");
        applyRxLNAGain(1, doCalibrationRx1, settings.m_tiaGainRx1);
    }

    if ((m_settings.m_gainModeRx0 == LimeSDRMIMOSettings::GAIN_MANUAL) && (m_settings.m_pgaGainRx0 != settings.m_pgaGainRx0))
    {
        reverseAPIKeys.append("pgaGainRx0");
        applyRxLNAGain(0, doCalibrationRx0, settings.m_pgaGainRx0);
    }

    if ((m_settings.m_gainModeRx1 == LimeSDRMIMOSettings::GAIN_MANUAL) && (m_settings.m_pgaGainRx1 != settings.m_pgaGainRx1))
    {
        reverseAPIKeys.append("pgaGainRx1");
        applyRxLNAGain(1, doCalibrationRx1, settings.m_pgaGainRx1);
    }

    if ((m_settings.m_devSampleRate != settings.m_devSampleRate)
       || (m_settings.m_log2HardDecim != settings.m_log2HardDecim) || force)
    {
        reverseAPIKeys.append("log2HardDecim");

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
                //doCalibration = true;
                forceRxNCOFrequency = true;
                qDebug("LimeSDRMIMO::applySettings: set sample rate set to %d with Rx oversampling of %d",
                        settings.m_devSampleRate,
                        1<<settings.m_log2HardDecim);
            }
        }
    }

    if ((m_settings.m_lpfBWRx0 != settings.m_lpfBWRx0) || force)
    {
        reverseAPIKeys.append("lpfBWRx0");
        doLPCalibrationRx0 = true;
    }

    if ((m_settings.m_lpfBWRx1 != settings.m_lpfBWRx1) || force)
    {
        reverseAPIKeys.append("lpfBWRx1");
        doLPCalibrationRx1 = true;
    }

    if ((m_settings.m_lpfFIRBWRx0 != settings.m_lpfFIRBWRx0) ||
        (m_settings.m_lpfFIREnableRx0 != settings.m_lpfFIREnableRx0) || force)
    {
        reverseAPIKeys.append("lpfFIRBWx0");
        reverseAPIKeys.append("lpfFIREnableRx0");
        applyRxLPFIRBW(0, settings.m_lpfFIREnableRx0, settings.m_lpfFIRBWRx0);
    }

    if ((m_settings.m_lpfFIRBWRx1 != settings.m_lpfFIRBWRx1) ||
        (m_settings.m_lpfFIREnableRx1 != settings.m_lpfFIREnableRx1) || force)
    {
        reverseAPIKeys.append("lpfFIRBWx1");
        reverseAPIKeys.append("lpfFIREnableRx1");
        applyRxLPFIRBW(1, settings.m_lpfFIREnableRx1, settings.m_lpfFIRBWRx1);
    }

    if ((m_settings.m_ncoFrequencyRx0 != settings.m_ncoFrequencyRx0) ||
        (m_settings.m_ncoEnableRx0 != settings.m_ncoEnableRx0) || force)
    {
        reverseAPIKeys.append("ncoFrequencyRx0");
        reverseAPIKeys.append("ncoEnableRx0");
        applyRxNCOFrequency(0, settings.m_ncoEnableRx0, settings.m_ncoFrequencyRx0);
    }

    if ((m_settings.m_ncoFrequencyRx1 != settings.m_ncoFrequencyRx1) ||
        (m_settings.m_ncoEnableRx1 != settings.m_ncoEnableRx1) || force)
    {
        reverseAPIKeys.append("ncoFrequencyRx1");
        reverseAPIKeys.append("ncoEnableRx1");
        applyRxNCOFrequency(1, settings.m_ncoEnableRx1, settings.m_ncoFrequencyRx1);
    }

    if ((m_settings.m_log2SoftDecim != settings.m_log2SoftDecim) || force)
    {
        reverseAPIKeys.append("log2SoftDecim");

        if (m_sourceThread)
        {
            m_sourceThread->setLog2Decimation(settings.m_log2SoftDecim);
            qDebug() << "LimeSDRMIMO::applySettings: set soft decimation to " << (1<<settings.m_log2SoftDecim);
        }
    }

    if ((m_settings.m_antennaPathRx0 != settings.m_antennaPathRx0) || force)
    {
        reverseAPIKeys.append("antennaPathRx0");
        applyRxAntennaPath(0, doCalibrationRx0, settings.m_antennaPathRx0);
    }

    if ((m_settings.m_antennaPathRx1 != settings.m_antennaPathRx1) || force)
    {
        reverseAPIKeys.append("antennaPathRx1");
        applyRxAntennaPath(1, doCalibrationRx1, settings.m_antennaPathRx1);
    }

    if ((m_settings.m_rxCenterFrequency != settings.m_rxCenterFrequency)
        || (m_settings.m_rxTransverterMode != settings.m_rxTransverterMode)
        || (m_settings.m_rxTransverterDeltaFrequency != settings.m_rxTransverterDeltaFrequency)
        || force)
    {
        reverseAPIKeys.append("rxCenterFrequency");
        reverseAPIKeys.append("rxTransverterMode");
        reverseAPIKeys.append("rxTransverterDeltaFrequency");

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

    // Tx

    if ((m_settings.m_gainTx0 != settings.m_gainTx0) || force)
    {
        reverseAPIKeys.append("gainTx0");
        applyTxGain(0, doCalibrationTx0, settings.m_gainTx0);
    }

    if ((m_settings.m_gainTx1 != settings.m_gainTx1) || force)
    {
        reverseAPIKeys.append("gainTx1");
        applyTxGain(1, doCalibrationTx1, settings.m_gainTx1);
    }

    if ((m_settings.m_devSampleRate != settings.m_devSampleRate)
       || (m_settings.m_log2HardInterp != settings.m_log2HardInterp) || force)
    {
        reverseAPIKeys.append("log2HardInterp");

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

    if ((m_settings.m_devSampleRate != settings.m_devSampleRate)
       || (m_settings.m_log2SoftInterp != settings.m_log2SoftInterp) || force)
    {
        reverseAPIKeys.append("log2SoftInterp");

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

    if ((m_settings.m_lpfBWTx0 != settings.m_lpfBWTx0) || force)
    {
        reverseAPIKeys.append("lpfBWTx0");
        doLPCalibrationTx0 = true;
    }

    if ((m_settings.m_lpfBWTx1 != settings.m_lpfBWTx1) || force)
    {
        reverseAPIKeys.append("lpfBWTx1");
        doLPCalibrationTx1 = true;
    }

    if ((m_settings.m_lpfFIRBWTx0 != settings.m_lpfFIRBWTx0) ||
        (m_settings.m_lpfFIREnableTx0 != settings.m_lpfFIREnableTx0) || force)
    {
        reverseAPIKeys.append("lpfFIRBWTx0");
        reverseAPIKeys.append("lpfFIREnableTx0");
        applyTxLPFIRBW(0, settings.m_lpfFIREnableTx0, settings.m_lpfFIRBWTx0);
    }

    if ((m_settings.m_lpfFIRBWTx1 != settings.m_lpfFIRBWTx1) ||
        (m_settings.m_lpfFIREnableTx1 != settings.m_lpfFIREnableTx1) || force)
    {
        reverseAPIKeys.append("lpfFIRBWTx1");
        reverseAPIKeys.append("lpfFIREnableTx1");
        applyTxLPFIRBW(1, settings.m_lpfFIREnableTx1, settings.m_lpfFIRBWTx1);
    }

    if ((m_settings.m_ncoFrequencyTx0 != settings.m_ncoFrequencyTx0) ||
        (m_settings.m_ncoEnableTx0 != settings.m_ncoEnableTx0) || force || forceTxNCOFrequency)
    {
        reverseAPIKeys.append("ncoFrequencyTx0");
        reverseAPIKeys.append("ncoEnableTx0");
        applyTxNCOFrequency(0, settings.m_ncoEnableTx0, settings.m_ncoFrequencyTx0);
    }

    if ((m_settings.m_ncoFrequencyTx1 != settings.m_ncoFrequencyTx1) ||
        (m_settings.m_ncoEnableTx1 != settings.m_ncoEnableTx1) || force || forceTxNCOFrequency)
    {
        reverseAPIKeys.append("ncoFrequencyTx1");
        reverseAPIKeys.append("ncoEnableTx1");
        applyTxNCOFrequency(1, settings.m_ncoEnableTx1, settings.m_ncoFrequencyTx1);
    }

    if ((m_settings.m_log2SoftInterp != settings.m_log2SoftInterp) || force)
    {
        reverseAPIKeys.append("log2SoftInterp");

        if (m_sinkThread)
        {
            m_sinkThread->setLog2Interpolation(settings.m_log2SoftInterp);
            qDebug() << "LimeSDRMIMO::applySettings: set soft interpolation to " << (1<<settings.m_log2SoftInterp);
        }
    }

    if ((m_settings.m_antennaPathTx0 != settings.m_antennaPathTx0) || force)
    {
        reverseAPIKeys.append("antennaPathTx0");
        applyTxAntennaPath(0, doCalibrationTx0, settings.m_antennaPathTx0);
    }

    if ((m_settings.m_antennaPathTx1 != settings.m_antennaPathTx1) || force)
    {
        reverseAPIKeys.append("antennaPathTx1");
        applyTxAntennaPath(1, doCalibrationTx1, settings.m_antennaPathTx1);
    }

    if ((m_settings.m_txCenterFrequency != settings.m_txCenterFrequency)
        || (m_settings.m_txTransverterMode != settings.m_txTransverterMode)
        || (m_settings.m_txTransverterDeltaFrequency != settings.m_txTransverterDeltaFrequency)
        || force)
    {
        reverseAPIKeys.append("txCenterFrequency");
        reverseAPIKeys.append("txTransverterMode");
        reverseAPIKeys.append("txTransverterDeltaFrequency");
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

    // common

    if ((m_settings.m_extClock != settings.m_extClock) ||
        (settings.m_extClock && (m_settings.m_extClockFreq != settings.m_extClockFreq)) || force)
    {
        reverseAPIKeys.append("extClock");
        reverseAPIKeys.append("extClockFreq");

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
        if ((m_settings.m_gpioDir != settings.m_gpioDir) || force)
        {
            reverseAPIKeys.append("gpioDir");

            if (LMS_GPIODirWrite(m_deviceParams->getDevice(), &settings.m_gpioDir, 1) != 0) {
                qCritical("LimeSDRMIMO::applySettings: could not set GPIO directions to %u", settings.m_gpioDir);
            } else {
                qDebug("LimeSDRMIMO::applySettings: GPIO directions set to %u", settings.m_gpioDir);
            }
        }

        if ((m_settings.m_gpioPins != settings.m_gpioPins) || force)
        {
            reverseAPIKeys.append("gpioPins");

            if (LMS_GPIOWrite(m_deviceParams->getDevice(), &settings.m_gpioPins, 1) != 0) {
                qCritical("LimeSDRMIMO::applySettings: could not set GPIO pins to %u", settings.m_gpioPins);
            } else {
                qDebug("LimeSDRMIMO::applySettings: GPIO pins set to %u", settings.m_gpioPins);
            }
        }
    }

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    m_settings = settings;
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

void LimeSDRMIMO::webapiFormatDeviceSettings(
    SWGSDRangel::SWGDeviceSettings& response,
    const LimeSDRMIMOSettings& settings)
{
    // TODO
    (void) response;
    (void) settings;
}

void LimeSDRMIMO::webapiUpdateDeviceSettings(
    LimeSDRMIMOSettings& settings,
    const QStringList& deviceSettingsKeys,
    SWGSDRangel::SWGDeviceSettings& response)
{
    // TODO
    (void) settings;
    (void) deviceSettingsKeys;
    (void) response;
}
