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
#include "SWGXtrxMIMOSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGDeviceActions.h"
#include "SWGXtrxMIMOReport.h"

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/dspdevicemimoengine.h"
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"
#include "xtrx/devicextrxparam.h"
#include "xtrx/devicextrxshared.h"
#include "xtrx/devicextrx.h"

#include "xtrxmithread.h"
#include "xtrxmothread.h"
#include "xtrxmimo.h"

MESSAGE_CLASS_DEFINITION(XTRXMIMO::MsgConfigureXTRXMIMO, Message)
MESSAGE_CLASS_DEFINITION(XTRXMIMO::MsgGetStreamInfo, Message)
MESSAGE_CLASS_DEFINITION(XTRXMIMO::MsgGetDeviceInfo, Message)
MESSAGE_CLASS_DEFINITION(XTRXMIMO::MsgReportClockGenChange, Message)
MESSAGE_CLASS_DEFINITION(XTRXMIMO::MsgReportStreamInfo, Message)
MESSAGE_CLASS_DEFINITION(XTRXMIMO::MsgStartStop, Message)

XTRXMIMO::XTRXMIMO(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
    m_sourceThread(nullptr),
    m_sinkThread(nullptr),
	m_deviceDescription("XTRXMIMO"),
	m_runningRx(false),
    m_runningTx(false),
    m_open(false)
{
    m_open = openDevice();
    m_mimoType = MIMOHalfSynchronous;
    m_sampleMIFifo.init(2, 4096 * 64);
    m_sampleMOFifo.init(2, 4096 * 64);
    m_deviceAPI->setNbSourceStreams(2);
    m_deviceAPI->setNbSinkStreams(2);
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &XTRXMIMO::networkManagerFinished
    );
}

XTRXMIMO::~XTRXMIMO()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &XTRXMIMO::networkManagerFinished
    );
    delete m_networkManager;
    closeDevice();
}

bool XTRXMIMO::openDevice()
{
    m_deviceShared.m_dev = new DeviceXTRX();
    char serial[256];
    strcpy(serial, qPrintable(m_deviceAPI->getSamplingDeviceSerial()));

    if (!m_deviceShared.m_dev->open(serial))
    {
        qCritical("XTRXMIMO::openDevice: cannot open XTRX device");
        return false;
    }

    return true;
}

void XTRXMIMO::closeDevice()
{
    if (m_runningRx) {
        stopRx();
    }

    if (m_runningTx) {
        stopTx();
    }

    m_deviceShared.m_dev->close();
    delete m_deviceShared.m_dev;
    m_deviceShared.m_dev = nullptr;
}

void XTRXMIMO::destroy()
{
    delete this;
}

void XTRXMIMO::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

bool XTRXMIMO::startRx()
{
    qDebug("XTRXMIMO::startRx");

    if (!m_open)
    {
        qCritical("XTRXMIMO::startRx: device was not opened");
        return false;
    }

	QMutexLocker mutexLocker(&m_mutex);

    if (m_runningRx) {
        stopRx();
    }

    m_sourceThread = new XTRXMIThread(m_deviceShared.m_dev->getDevice());
    m_sampleMIFifo.reset();
    m_sourceThread->setFifo(&m_sampleMIFifo);
    m_sourceThread->setLog2Decimation(m_settings.m_log2SoftDecim);
    m_sourceThread->setIQOrder(m_settings.m_iqOrder);
	m_sourceThread->startWork();
	mutexLocker.unlock();
	m_runningRx = true;

    return true;
}

bool XTRXMIMO::startTx()
{
    qDebug("XTRXMIMO::startTx");

    if (!m_open)
    {
        qCritical("XTRXMIMO::startTx: device was not opened");
        return false;
    }

	QMutexLocker mutexLocker(&m_mutex);

    if (m_runningRx) {
        stopRx();
    }

    m_sinkThread = new XTRXMOThread(m_deviceShared.m_dev->getDevice());
    m_sampleMOFifo.reset();
    m_sinkThread->setFifo(&m_sampleMOFifo);
    m_sinkThread->setLog2Interpolation(m_settings.m_log2SoftInterp);
	m_sinkThread->startWork();
	mutexLocker.unlock();
	m_runningTx = true;

    return true;
}

void XTRXMIMO::stopRx()
{
    qDebug("XTRXMIMO::stopRx");

    if (!m_sourceThread) {
        return;
    }

	QMutexLocker mutexLocker(&m_mutex);

    m_sourceThread->stopWork();
    delete m_sourceThread;
    m_sourceThread = nullptr;
    m_runningRx = false;
}

void XTRXMIMO::stopTx()
{
    qDebug("XTRXMIMO::stopTx");

    if (!m_sinkThread) {
        return;
    }

	QMutexLocker mutexLocker(&m_mutex);

    m_sinkThread->stopWork();
    delete m_sinkThread;
    m_sinkThread = nullptr;
    m_runningTx = false;
}

QByteArray XTRXMIMO::serialize() const
{
    return m_settings.serialize();
}

bool XTRXMIMO::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureXTRXMIMO* message = MsgConfigureXTRXMIMO::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureXTRXMIMO* messageToGUI = MsgConfigureXTRXMIMO::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& XTRXMIMO::getDeviceDescription() const
{
	return m_deviceDescription;
}

int XTRXMIMO::getSourceSampleRate(int index) const
{
    (void) index;
    uint32_t rate = getRxDevSampleRate();
    return (rate / (1<<m_settings.m_log2SoftDecim));
}

int XTRXMIMO::getSinkSampleRate(int index) const
{
    (void) index;
    uint32_t rate = getTxDevSampleRate();
    return (rate / (1<<m_settings.m_log2SoftInterp));
}

quint64 XTRXMIMO::getSourceCenterFrequency(int index) const
{
    (void) index;
    return m_settings.m_rxCenterFrequency;
}

void XTRXMIMO::setSourceCenterFrequency(qint64 centerFrequency, int index)
{
    (void) index;
    XTRXMIMOSettings settings = m_settings;
    settings.m_rxCenterFrequency = centerFrequency;

    MsgConfigureXTRXMIMO* message = MsgConfigureXTRXMIMO::create(settings, QList<QString>{"rxCenterFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureXTRXMIMO* messageToGUI = MsgConfigureXTRXMIMO::create(settings, QList<QString>{"rxCenterFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

quint64 XTRXMIMO::getSinkCenterFrequency(int index) const
{
    (void) index;
    return m_settings.m_txCenterFrequency;
}

void XTRXMIMO::setSinkCenterFrequency(qint64 centerFrequency, int index)
{
    (void) index;
    XTRXMIMOSettings settings = m_settings;
    settings.m_txCenterFrequency = centerFrequency;

    MsgConfigureXTRXMIMO* message = MsgConfigureXTRXMIMO::create(settings, QList<QString>{"txCenterFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureXTRXMIMO* messageToGUI = MsgConfigureXTRXMIMO::create(settings, QList<QString>{"txCenterFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

uint32_t XTRXMIMO::getRxDevSampleRate() const
{
    if (m_deviceShared.m_dev) {
        return m_deviceShared.m_dev->getActualInputRate();
    } else {
        return m_settings.m_rxDevSampleRate;
    }
}

uint32_t XTRXMIMO::getTxDevSampleRate() const
{
    if (m_deviceShared.m_dev) {
        return m_deviceShared.m_dev->getActualOutputRate();
    } else {
        return m_settings.m_txDevSampleRate;
    }
}

uint32_t XTRXMIMO::getLog2HardDecim() const
{
    if (m_deviceShared.m_dev && (m_deviceShared.m_dev->getActualInputRate() != 0.0)) {
        return log2(m_deviceShared.m_dev->getClockGen() / m_deviceShared.m_dev->getActualInputRate() / 4);
    } else {
        return m_settings.m_log2HardDecim;
    }
}

uint32_t XTRXMIMO::getLog2HardInterp() const
{
    if (m_deviceShared.m_dev && (m_deviceShared.m_dev->getActualOutputRate() != 0.0)) {
        return log2(m_deviceShared.m_dev->getClockGen() / m_deviceShared.m_dev->getActualOutputRate() / 4);
    } else {
        return m_settings.m_log2HardInterp;
    }
}

double XTRXMIMO::getClockGen() const
{
    if (m_deviceShared.m_dev) {
        return m_deviceShared.m_dev->getClockGen();
    } else {
        return 0.0;
    }
}

bool XTRXMIMO::handleMessage(const Message& message)
{
    if (MsgConfigureXTRXMIMO::match(message))
    {
        MsgConfigureXTRXMIMO& conf = (MsgConfigureXTRXMIMO&) message;
        qDebug() << "XTRXMIMO::handleMessage: MsgConfigureXTRXMIMO";

        bool success = applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());

        if (!success) {
            qDebug("XTRXMIMO::handleMessage: config error");
        }

        return true;
    }
    else if (MsgGetStreamInfo::match(message))
    {
        if (getMessageQueueToGUI() && m_deviceShared.m_dev && m_deviceShared.m_dev->getDevice())
        {
            uint64_t fifolevelRx = 0;
            uint64_t fifolevelTx = 0;

            xtrx_val_get(m_deviceShared.m_dev->getDevice(), XTRX_RX, XTRX_CH_AB, XTRX_PERF_LLFIFO, &fifolevelRx);
            xtrx_val_get(m_deviceShared.m_dev->getDevice(), XTRX_TX, XTRX_CH_AB, XTRX_PERF_LLFIFO, &fifolevelTx);

            MsgReportStreamInfo *report = MsgReportStreamInfo::create(
                        true,
                        true,
                        fifolevelRx,
                        fifolevelTx,
                        65536);

            getMessageQueueToGUI()->push(report);
        }

        return true;
    }
    else if (MsgGetDeviceInfo::match(message))
    {
        double board_temp = 0.0;
        bool gps_locked = false;

        if (!m_deviceShared.m_dev->getDevice() || ((board_temp = m_deviceShared.get_board_temperature() / 256.0) == 0.0)) {
            qDebug("XTRXMIMO::handleMessage: MsgGetDeviceInfo: cannot get board temperature");
        }

        if (!m_deviceShared.m_dev->getDevice()) {
            qDebug("XTRXMIMO::handleMessage: MsgGetDeviceInfo: cannot get GPS lock status");
        } else {
            gps_locked = m_deviceShared.get_gps_status();
        }

        // send to oneself
        if (getMessageQueueToGUI())
        {
            DeviceXTRXShared::MsgReportDeviceInfo *report = DeviceXTRXShared::MsgReportDeviceInfo::create(board_temp, gps_locked);
            getMessageQueueToGUI()->push(report);
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "XTRXMIMO::handleMessage: "
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

bool XTRXMIMO::applySettings(const XTRXMIMOSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "XTRXMIMO::applySettings: force:" << force << settings.getDebugString(settingsKeys, force);
    bool doRxLPCalibration = false;
    bool doRxChangeSampleRate = false;
    bool doRxChangeFreq = false;
    bool doTxLPCalibration = false;
    bool doTxChangeSampleRate = false;
    bool doTxChangeFreq = false;
    bool forceNCOFrequencyRx = false;
    bool forceNCOFrequencyTx = false;
    bool forwardChangeRxDSP = false;
    bool forwardChangeTxDSP = false;

    qint64 rxXlatedDeviceCenterFrequency = settings.m_rxCenterFrequency;
    qint64 txXlatedDeviceCenterFrequency = settings.m_txCenterFrequency;

    // common

    if (settingsKeys.contains("extClock")
     || (settings.m_extClock && settingsKeys.contains("extClockFreq")) || force)
    {
        if (m_deviceShared.m_dev->getDevice() != 0)
        {
            xtrx_set_ref_clk(m_deviceShared.m_dev->getDevice(),
                             (settings.m_extClock) ? settings.m_extClockFreq : 0,
                             (settings.m_extClock) ? XTRX_CLKSRC_EXT : XTRX_CLKSRC_INT);
            {
                doRxChangeSampleRate = true;
                doTxChangeSampleRate = true;
                doRxChangeFreq = true;
                doTxChangeFreq = true;
                qDebug("XTRXMIMO::applySettings: clock set to %s (Ext: %d Hz)",
                       settings.m_extClock ? "external" : "internal",
                       settings.m_extClockFreq);
            }
        }
    }

    // Rx

    if (settingsKeys.contains("dcBlock") || force) {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
    }

    if (settingsKeys.contains("iqCorrection") || force) {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
    }

    if (settingsKeys.contains("rxDevSampleRate")
     || settingsKeys.contains("log2HardDecim") || force)
    {
        forwardChangeRxDSP = true;

        if (m_deviceShared.m_dev->getDevice()) {
            doRxChangeSampleRate = true;
        }
    }

    if (settingsKeys.contains("log2SoftDecim") || force)
    {
        forwardChangeRxDSP = true;

        if (m_sourceThread)
        {
            m_sourceThread->setLog2Decimation(settings.m_log2SoftDecim);
            qDebug() << "XTRXMIMO::applySettings: set soft decimation to " << (1<<settings.m_log2SoftDecim);
        }
    }

    if (settingsKeys.contains("iqOrder") || force)
    {
        if (m_sourceThread) {
            m_sourceThread->setIQOrder(settings.m_iqOrder);
        }
    }

    if (settingsKeys.contains("ncoFrequencyRx")
     || settingsKeys.contains("ncoEnableRx") || force)
    {
        forceNCOFrequencyRx = true;
    }

    if (settingsKeys.contains("antennaPathRx") || force)
    {
        if (m_deviceShared.m_dev->getDevice())
        {
            if (xtrx_set_antenna(m_deviceShared.m_dev->getDevice(), toXTRXAntennaRx(settings.m_antennaPathRx)) < 0) {
                qCritical("XTRXMIMO::applySettings: could not set antenna path of Rx to %d", (int) settings.m_antennaPathRx);
            } else {
                qDebug("XTRXMIMO::applySettings: set Rx antenna path to %d", (int) settings.m_antennaPathRx);
            }
        }
    }

    if (settingsKeys.contains("rxCenterFrequency") || force) {
        doRxChangeFreq = true;
    }

    // Rx0/1

    if (settingsKeys.contains("pwrmodeRx0") || force)
    {
        if (m_deviceShared.m_dev->getDevice())
        {
            if (xtrx_val_set(m_deviceShared.m_dev->getDevice(),
                    XTRX_TRX,
                    XTRX_CH_A,
                    XTRX_LMS7_PWR_MODE,
                    settings.m_pwrmodeRx0) < 0) {
                qCritical("XTRXMIMO::applySettings: could not set Rx0 power mode %d", settings.m_pwrmodeRx0);
            }
        }
    }

    if (settingsKeys.contains("pwrmodeRx1") || force)
    {
        if (m_deviceShared.m_dev->getDevice())
        {
            if (xtrx_val_set(m_deviceShared.m_dev->getDevice(),
                    XTRX_TRX,
                    XTRX_CH_B,
                    XTRX_LMS7_PWR_MODE,
                    settings.m_pwrmodeRx1) < 0) {
                qCritical("XTRXMIMO::applySettings: could not set Rx1 power mode %d", settings.m_pwrmodeRx0);
            }
        }
    }

    if (m_deviceShared.m_dev->getDevice())
    {
        bool doGainAuto = false;
        bool doGainLna = false;
        bool doGainTia = false;
        bool doGainPga = false;

        if (settingsKeys.contains("gainModeRx0") || force)
        {
            if (settings.m_gainModeRx0 == XTRXMIMOSettings::GAIN_AUTO)
            {
                doGainAuto = true;
            }
            else
            {
                doGainLna = true;
                doGainTia = true;
                doGainPga = true;
            }
        }
        else if (m_settings.m_gainModeRx0 == XTRXMIMOSettings::GAIN_AUTO)
        {
            if (m_settings.m_gainRx0 != settings.m_gainRx0) {
                doGainAuto = true;
            }
        }
        else if (m_settings.m_gainModeRx0 == XTRXMIMOSettings::GAIN_MANUAL)
        {
            if (settingsKeys.contains("lnaGainRx0")) {
                doGainLna = true;
            }
            if (settingsKeys.contains("tiaGainRx0")) {
                doGainTia = true;
            }
            if (settingsKeys.contains("pgaGainRx0")) {
                doGainPga = true;
            }
        }

        if (doGainAuto) {
            applyGainAuto(0, m_settings.m_gainRx0);
        }
        if (doGainLna) {
            applyGainLNA(0, m_settings.m_lnaGainRx0);
        }
        if (doGainTia) {
            applyGainTIA(0, tiaToDB(m_settings.m_tiaGainRx0));
        }
        if (doGainPga) {
            applyGainPGA(0, m_settings.m_pgaGainRx0);
        }

        doGainAuto = false;
        doGainLna = false;
        doGainTia = false;
        doGainPga = false;

        if (settingsKeys.contains("gainModeRx1") || force)
        {
            if (settings.m_gainModeRx1 == XTRXMIMOSettings::GAIN_AUTO)
            {
                doGainAuto = true;
            }
            else
            {
                doGainLna = true;
                doGainTia = true;
                doGainPga = true;
            }
        }
        else if (m_settings.m_gainModeRx1 == XTRXMIMOSettings::GAIN_AUTO)
        {
            if (m_settings.m_gainRx1 != settings.m_gainRx1) {
                doGainAuto = true;
            }
        }
        else if (m_settings.m_gainModeRx1 == XTRXMIMOSettings::GAIN_MANUAL)
        {
            if (settingsKeys.contains("lnaGainRx1")) {
                doGainLna = true;
            }
            if (settingsKeys.contains("tiaGainRx1")) {
                doGainTia = true;
            }
            if (settingsKeys.contains("pgaGainRx1")) {
                doGainPga = true;
            }
        }

        if (doGainAuto) {
            applyGainAuto(1, m_settings.m_gainRx1);
        }
        if (doGainLna) {
            applyGainLNA(1, m_settings.m_lnaGainRx1);
        }
        if (doGainTia) {
            applyGainTIA(1, tiaToDB(m_settings.m_tiaGainRx1));
        }
        if (doGainPga) {
            applyGainPGA(1, m_settings.m_pgaGainRx1);
        }
    }

    if (settingsKeys.contains("lpfBWRx0") || force)
    {
        if (m_deviceShared.m_dev->getDevice()) {
            doRxLPCalibration = true;
        }
    }

    if (settingsKeys.contains("lpfBWRx1") || force)
    {
        if (m_deviceShared.m_dev->getDevice()) {
            doRxLPCalibration = true;
        }
    }

    // Tx

    if (settingsKeys.contains("txDevSampleRate")
     || settingsKeys.contains("log2HardInterp") || force)
    {
        forwardChangeTxDSP = true;

        if (m_deviceShared.m_dev->getDevice()) {
            doTxChangeSampleRate = true;
        }
    }

    if (settingsKeys.contains("log2SoftInterp") || force)
    {
        forwardChangeTxDSP = true;

        if (m_sinkThread)
        {
            m_sinkThread->setLog2Interpolation(settings.m_log2SoftInterp);
            qDebug("XTRXMIMO::applySettings: set soft interpolation to %u", (1<<settings.m_log2SoftInterp));
        }
    }

    if (settingsKeys.contains("ncoFrequencyTx")
     || settingsKeys.contains("ncoEnableTx") || force)
    {
        forceNCOFrequencyTx = true;
    }

    if (settingsKeys.contains("antennaPathTx") || force)
    {
        if (m_deviceShared.m_dev->getDevice())
        {
            if (xtrx_set_antenna(m_deviceShared.m_dev->getDevice(), toXTRXAntennaTx(settings.m_antennaPathTx)) < 0) {
                qCritical("XTRXMIMO::applySettings: could not set Tx antenna path to %d", (int) settings.m_antennaPathTx);
            } else {
                qDebug("XTRXMIMO::applySettings: set Tx antenna path to %d", (int) settings.m_antennaPathTx);
            }
        }
    }

    if (settingsKeys.contains("txCenterFrequency") || force)
    {
        doTxChangeFreq = true;
    }

    // Tx0

    if (settingsKeys.contains("pwrmodeTx0") || force)
    {
        if (m_deviceShared.m_dev->getDevice())
        {
            if (xtrx_val_set(m_deviceShared.m_dev->getDevice(),
                    XTRX_TRX,
                    XTRX_CH_A,
                    XTRX_LMS7_PWR_MODE,
                    settings.m_pwrmodeTx0) < 0) {
                qCritical("XTRXMIMO::applySettings: could not set Tx0 power mode %d", settings.m_pwrmodeTx0);
            }
        }
    }

    if (settingsKeys.contains("gainTx0") || force)
    {
        if (m_deviceShared.m_dev->getDevice())
        {
            if (xtrx_set_gain(m_deviceShared.m_dev->getDevice(),
                    XTRX_CH_A,
                    XTRX_TX_PAD_GAIN,
                    settings.m_gainTx0,
                    0) < 0) {
                qDebug("XTRXMIMO::applySettings: Tx0 gain (PAD) set to %u failed", settings.m_gainTx0);
            } else {
                qDebug("XTRXMIMO::applySettings: Tx0 gain (PAD) set to %u", settings.m_gainTx0);
            }
        }
    }

    if (settingsKeys.contains("lpfBWTx0") || force)
    {
        if (m_deviceShared.m_dev->getDevice()) {
            doTxLPCalibration = true;
        }
    }

    // Reverse API

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

    // Post Rx

    if (doRxChangeSampleRate && (m_settings.m_rxDevSampleRate != 0))
    {
        // if (m_sourceThread && m_sourceThread->isRunning())
        // {
        //     m_sourceThread->stopWork();
        //     rxThreadWasRunning = true;
        // }

        bool success = m_deviceShared.m_dev->setSamplerate(
            m_settings.m_rxDevSampleRate,
            m_settings.m_log2HardDecim,
            m_settings.m_log2HardInterp,
            false
        );

        doRxChangeFreq = true;
        forceNCOFrequencyRx = true;
        forwardChangeRxDSP = true;
        m_settings.m_rxDevSampleRate = m_deviceShared.m_dev->getActualInputRate();
        m_settings.m_txDevSampleRate = m_deviceShared.m_dev->getActualOutputRate();
        m_settings.m_log2HardDecim = getLog2HardDecim();
        m_settings.m_log2HardInterp = getLog2HardInterp();

        qDebug("XTRXMIMO::applySettings: sample rate set %s to Rx:%f Tx:%f with decimation of %d and interpolation of %d",
            success ? "unchanged" : "changed",
            m_settings.m_rxDevSampleRate,
            m_settings.m_txDevSampleRate,
            1 << m_settings.m_log2HardDecim,
            1 << m_settings.m_log2HardInterp);

        // if (rxThreadWasRunning) {
        //     m_sourceThread->startWork();
        // }
    }

    if (doRxLPCalibration)
    {
        if (xtrx_tune_rx_bandwidth(m_deviceShared.m_dev->getDevice(),
                XTRX_CH_A,
                m_settings.m_lpfBWRx0,
                0) < 0) {
            qCritical("XTRXMIMO::applySettings: could not set Rx0 LPF to %f Hz", m_settings.m_lpfBWRx0);
        } else {
            qDebug("XTRXMIMO::applySettings: Rx0 LPF set to %f Hz", m_settings.m_lpfBWRx0);
        }

        if (xtrx_tune_rx_bandwidth(m_deviceShared.m_dev->getDevice(),
                XTRX_CH_B,
                m_settings.m_lpfBWRx1,
                0) < 0) {
            qCritical("XTRXMIMO::applySettings: could not set Rx1 LPF to %f Hz", m_settings.m_lpfBWRx1);
        } else {
            qDebug("XTRXMIMO::applySettings: Rx1 LPF set to %f Hz", m_settings.m_lpfBWRx1);
        }
    }

    if (doRxChangeFreq)
    {
        forwardChangeRxDSP = true;

        if (m_deviceShared.m_dev->getDevice())
        {
            qint64 deviceCenterFrequency = DeviceSampleSource::calculateDeviceCenterFrequency(
                    rxXlatedDeviceCenterFrequency,
                    0,
                    m_settings.m_log2SoftDecim,
                    DeviceSampleSource::FC_POS_CENTER,
                    m_settings.m_rxDevSampleRate,
                    DeviceSampleSource::FrequencyShiftScheme::FSHIFT_STD,
                    false);
            setRxDeviceCenterFrequency(m_deviceShared.m_dev->getDevice(), deviceCenterFrequency);
        }
    }

    if (forceNCOFrequencyRx)
    {
        forwardChangeRxDSP = true;

        if (m_deviceShared.m_dev->getDevice())
        {
            if (xtrx_tune_ex(m_deviceShared.m_dev->getDevice(),
                    XTRX_TUNE_BB_RX,
                    XTRX_CH_AB,
                    (m_settings.m_ncoEnableRx) ? m_settings.m_ncoFrequencyRx : 0,
                    nullptr) < 0)
            {
                qCritical("XTRXMIMO::applySettings: could not %s and set Rx NCO to %d Hz",
                          m_settings.m_ncoEnableRx ? "enable" : "disable",
                          m_settings.m_ncoFrequencyRx);
            }
            else
            {
                qDebug("XTRXMIMO::applySettings: %sd and set NCO Rx to %d Hz",
                       m_settings.m_ncoEnableRx ? "enable" : "disable",
                       m_settings.m_ncoFrequencyRx);
            }
        }
    }

    // Post Tx

    if (doTxChangeSampleRate && !doRxChangeSampleRate && (m_settings.m_txDevSampleRate != 0))
    {
        // if (m_sinkThread && m_sinkThread->isRunning())
        // {
        //     m_sinkThread->stopWork();
        //     txThreadWasRunning = true;
        // }

        bool success = m_deviceShared.m_dev->setSamplerate(
            m_settings.m_txDevSampleRate,
            m_settings.m_log2HardDecim,
            m_settings.m_log2HardInterp,
            true
        );

        doTxChangeFreq = true;
        forceNCOFrequencyTx = true;
        forwardChangeTxDSP = true;
        m_settings.m_rxDevSampleRate = m_deviceShared.m_dev->getActualInputRate();
        m_settings.m_txDevSampleRate = m_deviceShared.m_dev->getActualOutputRate();
        m_settings.m_log2HardDecim = getLog2HardDecim();
        m_settings.m_log2HardInterp = getLog2HardInterp();

        qDebug("XTRXMIMO::applySettings: sample rate set %s to Rx:%f Tx:%f with decimation of %d and interpolation of %d",
            success ? "unchanged" : "changed",
            m_settings.m_rxDevSampleRate,
            m_settings.m_txDevSampleRate,
            1 << m_settings.m_log2HardDecim,
            1 << m_settings.m_log2HardInterp);

        // if (txThreadWasRunning) {
        //     m_sinkThread->startWork();
        // }
    }

    if (doTxLPCalibration)
    {
        if (xtrx_tune_tx_bandwidth(m_deviceShared.m_dev->getDevice(),
                XTRX_CH_A,
                m_settings.m_lpfBWTx0,
                0) < 0) {
            qCritical("XTRXMIMO::applySettings: could not set Tx0 LPF to %f Hz", m_settings.m_lpfBWTx0);
        } else {
            qDebug("XTRXMIMO::applySettings: Tx0 LPF set to %f Hz", m_settings.m_lpfBWTx0);
        }

        if (xtrx_tune_tx_bandwidth(m_deviceShared.m_dev->getDevice(),
                XTRX_CH_B,
                m_settings.m_lpfBWTx1,
                0) < 0) {
            qCritical("XTRXMIMO::applySettings: could not set Tx1 LPF to %f Hz", m_settings.m_lpfBWTx1);
        } else {
            qDebug("XTRXMIMO::applySettings: Tx1 LPF set to %f Hz", m_settings.m_lpfBWTx1);
        }
    }

    if (doTxChangeFreq)
    {
        forwardChangeTxDSP = true;

        if (m_deviceShared.m_dev->getDevice())
        {
            qint64 deviceCenterFrequency = DeviceSampleSink::calculateDeviceCenterFrequency(
                txXlatedDeviceCenterFrequency,
                0,
                settings.m_log2SoftInterp,
                DeviceSampleSink::FC_POS_CENTER,
                m_settings.m_txDevSampleRate,
                false);
            setTxDeviceCenterFrequency(m_deviceShared.m_dev->getDevice(), deviceCenterFrequency);
        }
    }

    if (forceNCOFrequencyTx)
    {
        forwardChangeTxDSP = true;

        if (m_deviceShared.m_dev->getDevice())
        {
            if (xtrx_tune_ex(m_deviceShared.m_dev->getDevice(),
                    XTRX_TUNE_BB_TX,
                    XTRX_CH_AB,
                    (m_settings.m_ncoEnableTx) ? m_settings.m_ncoFrequencyTx : 0,
                    nullptr) < 0)
            {
                qCritical("XTRXMIMO::applySettings: could not %s and set Tx NCO to %d Hz",
                          m_settings.m_ncoEnableTx ? "enable" : "disable",
                          m_settings.m_ncoFrequencyTx);
            }
            else
            {
                qDebug("XTRXMIMO::applySettings: %sd and set Tx NCO to %d Hz",
                       m_settings.m_ncoEnableTx ? "enable" : "disable",
                       m_settings.m_ncoFrequencyTx);
            }
        }
    }

    unsigned int fifoRate = std::max(
        (unsigned int) m_settings.m_txDevSampleRate / (1<<m_settings.m_log2SoftInterp),
        DeviceXTRXShared::m_sampleFifoMinRate);
    m_sampleMOFifo.resize(SampleMOFifo::getSizePolicy(fifoRate));

    // forward changes

    if (forwardChangeRxDSP || forwardChangeTxDSP)
    {
        if (getMessageQueueToGUI())
        {
            MsgReportClockGenChange *report = MsgReportClockGenChange::create();
            getMessageQueueToGUI()->push(report);
        }

        int sampleRate = m_settings.m_rxDevSampleRate/(1<<m_settings.m_log2SoftDecim);
        int ncoShift = m_settings.m_ncoEnableRx ? m_settings.m_ncoFrequencyRx : 0;
        DSPMIMOSignalNotification *notifRx0 = new DSPMIMOSignalNotification(sampleRate, m_settings.m_rxCenterFrequency + ncoShift, true, 0);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notifRx0);
        DSPMIMOSignalNotification *notifRx1 = new DSPMIMOSignalNotification(sampleRate, m_settings.m_rxCenterFrequency + ncoShift, true, 1);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notifRx1);

        sampleRate = m_settings.m_txDevSampleRate/(1<<m_settings.m_log2SoftInterp);
        ncoShift = m_settings.m_ncoEnableTx ? m_settings.m_ncoFrequencyTx : 0;
        DSPMIMOSignalNotification *notifTx0 = new DSPMIMOSignalNotification(sampleRate, m_settings.m_txCenterFrequency + ncoShift, false, 0);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notifTx0);
        DSPMIMOSignalNotification *notifTx1 = new DSPMIMOSignalNotification(sampleRate, m_settings.m_txCenterFrequency + ncoShift, false, 1);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notifTx1);
    }

    // if (forwardChangeRxDSP)
    // {
    //     int sampleRate = m_settings.m_rxDevSampleRate/(1<<m_settings.m_log2SoftDecim);
    //     int ncoShift = m_settings.m_ncoEnableRx ? m_settings.m_ncoFrequencyRx : 0;
    //     DSPMIMOSignalNotification *notif0 = new DSPMIMOSignalNotification(sampleRate, m_settings.m_rxCenterFrequency + ncoShift, true, 0);
    //     m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif0);
    //     DSPMIMOSignalNotification *notif1 = new DSPMIMOSignalNotification(sampleRate, m_settings.m_rxCenterFrequency + ncoShift, true, 1);
    //     m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif1);
    // }

    // if (forwardChangeTxDSP)
    // {
    //     int sampleRate = m_settings.m_txDevSampleRate/(1<<m_settings.m_log2SoftInterp);
    //     int ncoShift = m_settings.m_ncoEnableTx ? m_settings.m_ncoFrequencyTx : 0;
    //     DSPMIMOSignalNotification *notif0 = new DSPMIMOSignalNotification(sampleRate, m_settings.m_txCenterFrequency + ncoShift, false, 0);
    //     m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif0);
    //     DSPMIMOSignalNotification *notif1 = new DSPMIMOSignalNotification(sampleRate, m_settings.m_txCenterFrequency + ncoShift, false, 1);
    //     m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif1);
    // }

    return true;
}

void XTRXMIMO::applyGainAuto(unsigned int channel, uint32_t gain)
{
    uint32_t lna, tia, pga;

    DeviceXTRX::getAutoGains(gain, lna, tia, pga);

    applyGainLNA(channel, lna);
    applyGainTIA(channel, tiaToDB(tia));
    applyGainPGA(channel, pga);
}

void XTRXMIMO::applyGainLNA(unsigned int channel, double gain)
{
    if (xtrx_set_gain(m_deviceShared.m_dev->getDevice(),
            channel == 0 ? XTRX_CH_A : XTRX_CH_B,
            XTRX_RX_LNA_GAIN,
            gain,
            0) < 0) {
        qDebug("XTRXMIMO::applyGainLNA: set Rx%u gain (LNA) to %f failed", channel, gain);
    } else {
        qDebug("XTRXMIMO::applyGainLNA: Rx%u gain (LNA) set to %f", channel, gain);
    }
}

void XTRXMIMO::applyGainTIA(unsigned int channel, double gain)
{
    if (xtrx_set_gain(m_deviceShared.m_dev->getDevice(),
            channel == 0 ? XTRX_CH_A : XTRX_CH_B,
            XTRX_RX_TIA_GAIN,
            gain,
            0) < 0) {
        qDebug("XTRXMIMO::applyGainTIA: set Rx%u gain (TIA) to %f failed", channel, gain);
    } else {
        qDebug("XTRXMIMO::applyGainTIA: Rx%u gain (TIA) set to %f", channel, gain);
    }
}

void XTRXMIMO::applyGainPGA(unsigned int channel, double gain)
{
    if (xtrx_set_gain(m_deviceShared.m_dev->getDevice(),
            channel == 0 ? XTRX_CH_A : XTRX_CH_B,
            XTRX_RX_PGA_GAIN,
            gain,
            0) < 0)
    {
        qDebug("XTRXMIMO::applyGainPGA: set Rx%u gain (PGA) to %f failed", channel, gain);
    }
    else
    {
        qDebug("XTRXMIMO::applyGainPGA: Rx%u gain (PGA) set to %f", channel, gain);
    }
}

double XTRXMIMO::tiaToDB(unsigned idx)
{
    switch (idx) {
    case 1: return 12;
    case 2: return 9;
    default: return 0;
    }
}

xtrx_antenna_t XTRXMIMO::toXTRXAntennaRx(XTRXMIMOSettings::RxAntenna antennaPath)
{
    switch (antennaPath) {
        case XTRXMIMOSettings::RXANT_LO: return XTRX_RX_L;
        case XTRXMIMOSettings::RXANT_HI: return XTRX_RX_H;
        default: return XTRX_RX_W;
    }
}

xtrx_antenna_t XTRXMIMO::toXTRXAntennaTx(XTRXMIMOSettings::TxAntenna antennaPath)
{
    switch (antennaPath) {
        case XTRXMIMOSettings::TXANT_HI: return XTRX_TX_H;
        default: return XTRX_TX_W;
    }
}

void XTRXMIMO::getLORange(float& minF, float& maxF, float& stepF) const
{
    minF = 29e6;
    maxF = 3840e6;
    stepF = 10;
    qDebug("XTRXMIMO::getLORange: min: %f max: %f step: %f",
           minF, maxF, stepF);
}

void XTRXMIMO::getSRRange(float& minF, float& maxF, float& stepF) const
{
    minF = 100e3;
    maxF = 120e6;
    stepF = 10;
    qDebug("XTRXMIMO::getSRRange: min: %f max: %f step: %f",
           minF, maxF, stepF);
}

void XTRXMIMO::getLPRange(float& minF, float& maxF, float& stepF) const
{
    minF = 500e3;
    maxF = 130e6;
    stepF = 10;
    qDebug("XTRXMIMO::getLPRange: min: %f max: %f step: %f",
           minF, maxF, stepF);
}

void XTRXMIMO::setRxDeviceCenterFrequency(xtrx_dev *dev, quint64 freq_hz)
{
    if (dev)
    {
        if (xtrx_tune(dev,
                XTRX_TUNE_RX_FDD,
                freq_hz,
                nullptr) < 0) {
            qCritical("XTRXMIMO::setRxDeviceCenterFrequency: could not set Rx frequency to %llu", freq_hz);
        } else {
            qDebug("XTRXMIMO::setRxDeviceCenterFrequency: Rx frequency set to %llu", freq_hz);
        }
    }
}

void XTRXMIMO::setTxDeviceCenterFrequency(xtrx_dev *dev, quint64 freq_hz)
{
    if (dev)
    {
        if (xtrx_tune(dev,
                XTRX_TUNE_TX_FDD,
                freq_hz,
                nullptr) < 0) {
            qCritical("XTRXMIMO::setTxDeviceCenterFrequency: could not set Tx frequency to %llu", freq_hz);
        } else {
            qDebug("XTRXMIMO::setTxDeviceCenterFrequency: Tx frequency set to %llu", freq_hz);
        }
    }
}

int XTRXMIMO::webapiSettingsGet(
    SWGSDRangel::SWGDeviceSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setXtrxMimoSettings(new SWGSDRangel::SWGXtrxMIMOSettings());
    response.getXtrxMimoSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int XTRXMIMO::webapiSettingsPutPatch(
    bool force,
    const QStringList& deviceSettingsKeys,
    SWGSDRangel::SWGDeviceSettings& response, // query + response
    QString& errorMessage)
{
    (void) errorMessage;
    XTRXMIMOSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureXTRXMIMO *msg = MsgConfigureXTRXMIMO::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureXTRXMIMO *msgToGUI = MsgConfigureXTRXMIMO::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void XTRXMIMO::webapiUpdateDeviceSettings(
    XTRXMIMOSettings& settings,
    const QStringList& deviceSettingsKeys,
    SWGSDRangel::SWGDeviceSettings& response)
{
    // common
    if (deviceSettingsKeys.contains("extClock")) {
        settings.m_extClock = response.getXtrxMimoSettings()->getExtClock() != 0;
    }
    if (deviceSettingsKeys.contains("extClockFreq")) {
        settings.m_extClockFreq = response.getXtrxMimoSettings()->getExtClockFreq();
    }
    if (deviceSettingsKeys.contains("gpioDir")) {
        settings.m_gpioDir = response.getXtrxMimoSettings()->getGpioDir();
    }
    if (deviceSettingsKeys.contains("gpioPins")) {
        settings.m_gpioPins = response.getXtrxMimoSettings()->getGpioPins();
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getXtrxInputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getXtrxInputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getXtrxInputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getXtrxInputSettings()->getReverseApiDeviceIndex();
    }
    // Rx
    if (deviceSettingsKeys.contains("rxDevSampleRate")) {
        settings.m_rxDevSampleRate = response.getXtrxMimoSettings()->getRxDevSampleRate();
    }
    if (deviceSettingsKeys.contains("log2HardDecim")) {
        settings.m_log2HardDecim = response.getXtrxMimoSettings()->getLog2HardDecim();
    }
    if (deviceSettingsKeys.contains("log2SoftDecim")) {
        settings.m_log2SoftDecim = response.getXtrxMimoSettings()->getLog2SoftDecim();
    }
    if (deviceSettingsKeys.contains("iqOrder")) {
        settings.m_iqOrder = response.getXtrxMimoSettings()->getIqOrder() != 0;
    }
    if (deviceSettingsKeys.contains("rxCenterFrequency")) {
        settings.m_rxCenterFrequency = response.getXtrxMimoSettings()->getRxCenterFrequency();
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getXtrxMimoSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getXtrxMimoSettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("ncoEnableRx")) {
        settings.m_ncoEnableRx = response.getXtrxMimoSettings()->getNcoEnableRx() != 0;
    }
    if (deviceSettingsKeys.contains("ncoFrequencyRx")) {
        settings.m_ncoFrequencyRx = response.getXtrxMimoSettings()->getNcoFrequencyRx();
    }
    if (deviceSettingsKeys.contains("antennaPathRx")) {
        settings.m_antennaPathRx = (XTRXMIMOSettings::RxAntenna) response.getXtrxMimoSettings()->getAntennaPathRx();
    }
    // Rx0
    if (deviceSettingsKeys.contains("lpfBWRx0")) {
        settings.m_lpfBWRx0 = response.getXtrxMimoSettings()->getLpfBwRx0();
    }
    if (deviceSettingsKeys.contains("gainRx0")) {
        settings.m_gainRx0 = response.getXtrxMimoSettings()->getGainRx0();
    }
    if (deviceSettingsKeys.contains("gainModeRx0")) {
        settings.m_gainModeRx0 = (XTRXMIMOSettings::GainMode) response.getXtrxMimoSettings()->getGainModeRx0();
    }
    if (deviceSettingsKeys.contains("lnaGainRx0")) {
        settings.m_lnaGainRx0 = response.getXtrxMimoSettings()->getLnaGainRx0();
    }
    if (deviceSettingsKeys.contains("tiaGainRx0")) {
        settings.m_tiaGainRx0 = response.getXtrxMimoSettings()->getTiaGainRx0();
    }
    if (deviceSettingsKeys.contains("pgaGainRx0")) {
        settings.m_pgaGainRx0 = response.getXtrxMimoSettings()->getPgaGainRx0();
    }
    if (deviceSettingsKeys.contains("pwrmodeRx0")) {
        settings.m_pwrmodeRx0 = response.getXtrxMimoSettings()->getPwrmodeRx0();
    }
    // Rx1
    if (deviceSettingsKeys.contains("lpfBWRx1")) {
        settings.m_lpfBWRx1 = response.getXtrxMimoSettings()->getLpfBwRx1();
    }
    if (deviceSettingsKeys.contains("gainRx1")) {
        settings.m_gainRx1 = response.getXtrxMimoSettings()->getGainRx1();
    }
    if (deviceSettingsKeys.contains("gainModeRx1")) {
        settings.m_gainModeRx1 = (XTRXMIMOSettings::GainMode) response.getXtrxMimoSettings()->getGainModeRx1();
    }
    if (deviceSettingsKeys.contains("lnaGainRx1")) {
        settings.m_lnaGainRx1 = response.getXtrxMimoSettings()->getLnaGainRx1();
    }
    if (deviceSettingsKeys.contains("tiaGainRx1")) {
        settings.m_tiaGainRx1 = response.getXtrxMimoSettings()->getTiaGainRx1();
    }
    if (deviceSettingsKeys.contains("pgaGainRx1")) {
        settings.m_pgaGainRx1 = response.getXtrxMimoSettings()->getPgaGainRx1();
    }
    if (deviceSettingsKeys.contains("pwrmodeRx1")) {
        settings.m_pwrmodeRx1 = response.getXtrxMimoSettings()->getPwrmodeRx1();
    }
    // Tx
    if (deviceSettingsKeys.contains("txDevSampleRate")) {
        settings.m_txDevSampleRate = response.getXtrxMimoSettings()->getTxDevSampleRate();
    }
    if (deviceSettingsKeys.contains("log2HardInterp")) {
        settings.m_log2HardInterp = response.getXtrxMimoSettings()->getLog2HardInterp();
    }
    if (deviceSettingsKeys.contains("log2SoftInterp")) {
        settings.m_log2SoftInterp = response.getXtrxMimoSettings()->getLog2SoftInterp();
    }
    if (deviceSettingsKeys.contains("txCenterFrequency")) {
        settings.m_txCenterFrequency = response.getXtrxMimoSettings()->getTxCenterFrequency();
    }
    if (deviceSettingsKeys.contains("ncoEnableTx")) {
        settings.m_ncoEnableTx = response.getXtrxMimoSettings()->getNcoEnableTx() != 0;
    }
    if (deviceSettingsKeys.contains("ncoFrequencyTx")) {
        settings.m_ncoFrequencyTx = response.getXtrxMimoSettings()->getNcoFrequencyTx();
    }
    if (deviceSettingsKeys.contains("antennaPathTx")) {
        settings.m_antennaPathTx = (XTRXMIMOSettings::TxAntenna) response.getXtrxMimoSettings()->getAntennaPathTx();
    }
    // Tx0
    if (deviceSettingsKeys.contains("lpfBWTx0")) {
        settings.m_lpfBWTx0 = response.getXtrxMimoSettings()->getLpfBwTx0();
    }
    if (deviceSettingsKeys.contains("gainTx0")) {
        settings.m_gainTx0 = response.getXtrxMimoSettings()->getGainTx0();
    }
    if (deviceSettingsKeys.contains("pwrmodeTx0")) {
        settings.m_pwrmodeRx0 = response.getXtrxMimoSettings()->getPwrmodeTx0();
    }
    // Tx1
    if (deviceSettingsKeys.contains("lpfBWTx1")) {
        settings.m_lpfBWTx1 = response.getXtrxMimoSettings()->getLpfBwTx1();
    }
    if (deviceSettingsKeys.contains("gainTx1")) {
        settings.m_gainTx1 = response.getXtrxMimoSettings()->getGainTx1();
    }
    if (deviceSettingsKeys.contains("pwrmodeTx1")) {
        settings.m_pwrmodeRx1 = response.getXtrxMimoSettings()->getPwrmodeTx1();
    }
}

void XTRXMIMO::webapiFormatDeviceSettings(
    SWGSDRangel::SWGDeviceSettings& response,
    const XTRXMIMOSettings& settings)
{
    // common
    response.getXtrxMimoSettings()->setExtClock(settings.m_extClock ? 1 : 0);
    response.getXtrxMimoSettings()->setExtClockFreq(settings.m_extClockFreq);

    response.getXtrxMimoSettings()->setGpioDir(settings.m_gpioDir & 0xFF);
    response.getXtrxMimoSettings()->setGpioPins(settings.m_gpioPins & 0xFF);
    response.getXtrxMimoSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getXtrxMimoSettings()->getReverseApiAddress()) {
        *response.getXtrxMimoSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getXtrxMimoSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getXtrxMimoSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getXtrxMimoSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    // Rx
    response.getXtrxMimoSettings()->setRxDevSampleRate(settings.m_rxDevSampleRate);
    response.getXtrxMimoSettings()->setLog2HardDecim(settings.m_log2HardDecim);
    response.getXtrxMimoSettings()->setLog2SoftDecim(settings.m_log2SoftDecim);
    response.getXtrxMimoSettings()->setIqOrder(settings.m_iqOrder ? 1 : 0);
    response.getXtrxMimoSettings()->setRxCenterFrequency(settings.m_rxCenterFrequency);
    response.getXtrxMimoSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getXtrxMimoSettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    response.getXtrxMimoSettings()->setNcoEnableRx(settings.m_ncoEnableRx ? 1 : 0);
    response.getXtrxMimoSettings()->setNcoFrequencyRx(settings.m_ncoFrequencyRx);
    response.getXtrxMimoSettings()->setAntennaPathRx((int) settings.m_antennaPathRx);
    // Rx0
    response.getXtrxMimoSettings()->setLpfBwRx0(settings.m_lpfBWRx0);
    response.getXtrxMimoSettings()->setGainRx0(settings.m_gainRx0);
    response.getXtrxMimoSettings()->setGainModeRx0((int) settings.m_gainModeRx0);
    response.getXtrxMimoSettings()->setLnaGainRx0(settings.m_lnaGainRx0);
    response.getXtrxMimoSettings()->setTiaGainRx0(settings.m_tiaGainRx0);
    response.getXtrxMimoSettings()->setPgaGainRx0(settings.m_pgaGainRx0);
    response.getXtrxMimoSettings()->setPwrmodeRx0(settings.m_pwrmodeRx0);
    // Rx1
    response.getXtrxMimoSettings()->setLpfBwRx1(settings.m_lpfBWRx1);
    response.getXtrxMimoSettings()->setGainRx1(settings.m_gainRx1);
    response.getXtrxMimoSettings()->setGainModeRx1((int) settings.m_gainModeRx1);
    response.getXtrxMimoSettings()->setLnaGainRx1(settings.m_lnaGainRx1);
    response.getXtrxMimoSettings()->setTiaGainRx1(settings.m_tiaGainRx1);
    response.getXtrxMimoSettings()->setPgaGainRx1(settings.m_pgaGainRx1);
    response.getXtrxMimoSettings()->setPwrmodeRx1(settings.m_pwrmodeRx1);
    // Tx
    response.getXtrxMimoSettings()->setTxDevSampleRate(settings.m_txDevSampleRate);
    response.getXtrxMimoSettings()->setLog2HardInterp(settings.m_log2HardInterp);
    response.getXtrxMimoSettings()->setLog2SoftInterp(settings.m_log2SoftInterp);
    response.getXtrxMimoSettings()->setTxCenterFrequency(settings.m_txCenterFrequency);
    response.getXtrxMimoSettings()->setNcoEnableTx(settings.m_ncoEnableTx ? 1 : 0);
    response.getXtrxMimoSettings()->setNcoFrequencyTx(settings.m_ncoFrequencyTx);
    response.getXtrxMimoSettings()->setAntennaPathTx((int) settings.m_antennaPathTx);
    // Tx0
    response.getXtrxMimoSettings()->setLpfBwTx0(settings.m_lpfBWTx0);
    response.getXtrxMimoSettings()->setGainTx0(settings.m_gainTx0);
    response.getXtrxMimoSettings()->setPwrmodeTx0(settings.m_pwrmodeTx0);
    // Tx1
    response.getXtrxMimoSettings()->setLpfBwTx1(settings.m_lpfBWTx1);
    response.getXtrxMimoSettings()->setGainTx1(settings.m_gainTx1);
    response.getXtrxMimoSettings()->setPwrmodeTx1(settings.m_pwrmodeTx1);
}

int XTRXMIMO::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setXtrxInputReport(new SWGSDRangel::SWGXtrxInputReport());
    response.getXtrxInputReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

int XTRXMIMO::webapiRunGet(
        int subsystemIndex,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    if ((subsystemIndex == 0) || (subsystemIndex == 1))
    {
        m_deviceAPI->getDeviceEngineStateStr(*response.getState(), subsystemIndex);
        return 200;
    }
    else
    {
        errorMessage = QString("Subsystem invalid: must be 0 (Rx) or 1 (Tx)");
        return 404;
    }

}

int XTRXMIMO::webapiRun(
        bool run,
        int subsystemIndex,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    if ((subsystemIndex == 0) || (subsystemIndex == 1))
    {
        m_deviceAPI->getDeviceEngineStateStr(*response.getState(), subsystemIndex);
        MsgStartStop *message = MsgStartStop::create(run, subsystemIndex == 0);
        m_inputMessageQueue.push(message);

        if (m_guiMessageQueue) // forward to GUI if any
        {
            MsgStartStop *msgToGUI = MsgStartStop::create(run, subsystemIndex == 0);
            m_guiMessageQueue->push(msgToGUI);
        }

        return 200;
    }
    else
    {
        errorMessage = QString("Subsystem invalid: must be 0 (Rx) or 1 (Tx)");
        return 404;
    }
}

void XTRXMIMO::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    bool success = false;
    double temp = 0.0;
    bool gpsStatus = false;
    uint64_t fifolevelRx = 0;
    uint64_t fifolevelTx = 0;
    uint32_t fifosize = 1<<16;

    if (m_deviceShared.m_dev->getDevice())
    {
        int ret = xtrx_val_get(m_deviceShared.m_dev->getDevice(),
            XTRX_RX, XTRX_CH_AB, XTRX_PERF_LLFIFO, &fifolevelRx);

        success = (ret >= 0);

        ret = xtrx_val_get(m_deviceShared.m_dev->getDevice(),
            XTRX_TX, XTRX_CH_AB, XTRX_PERF_LLFIFO, &fifolevelTx);

        success = success & (ret >= 0);
        temp = m_deviceShared.get_board_temperature() / 256.0;
        gpsStatus = m_deviceShared.get_gps_status();
    }

    response.getXtrxMimoReport()->setSuccess(success ? 1 : 0);
    response.getXtrxMimoReport()->setFifoSize(fifosize);
    response.getXtrxMimoReport()->setFifoFillRx(fifolevelRx);
    response.getXtrxMimoReport()->setFifoFillTx(fifolevelTx);
    response.getXtrxMimoReport()->setTemperature(temp);
    response.getXtrxMimoReport()->setGpsLock(gpsStatus ? 1 : 0);
}

void XTRXMIMO::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const XTRXMIMOSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(2); // MIMO
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("XTRX"));
    swgDeviceSettings->setXtrxMimoSettings(new SWGSDRangel::SWGXtrxMIMOSettings());
    SWGSDRangel::SWGXtrxMIMOSettings *swgXTRXMIMOSettings = swgDeviceSettings->getXtrxMimoSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    // common
    if (deviceSettingsKeys.contains("extClock") || force) {
        swgXTRXMIMOSettings->setExtClock(settings.m_extClock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("extClock") || force) {
        swgXTRXMIMOSettings->setExtClockFreq(settings.m_extClockFreq);
    }
    if (deviceSettingsKeys.contains("gpioDir") || force) {
        swgXTRXMIMOSettings->setGpioDir(settings.m_gpioDir & 0xFF);
    }
    if (deviceSettingsKeys.contains("gpioPins") || force) {
        swgXTRXMIMOSettings->setGpioPins(settings.m_gpioPins & 0xFF);
    }
    // Rx
    if (deviceSettingsKeys.contains("rxDevSampleRate") || force) {
        swgXTRXMIMOSettings->setRxDevSampleRate(settings.m_rxDevSampleRate);
    }
    if (deviceSettingsKeys.contains("log2HardDecim") || force) {
        swgXTRXMIMOSettings->setLog2HardDecim(settings.m_log2HardDecim);
    }
    if (deviceSettingsKeys.contains("log2SoftDecim") || force) {
        swgXTRXMIMOSettings->setLog2SoftDecim(settings.m_log2SoftDecim);
    }
    if (deviceSettingsKeys.contains("iqOrder") || force) {
        swgXTRXMIMOSettings->setIqOrder(settings.m_iqOrder ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("rxCenterFrequency") || force) {
        swgXTRXMIMOSettings->setRxCenterFrequency(settings.m_rxCenterFrequency);
    }
    if (deviceSettingsKeys.contains("dcBlock") || force) {
        swgXTRXMIMOSettings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("iqCorrection") || force) {
        swgXTRXMIMOSettings->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("ncoEnableRx") || force) {
        swgXTRXMIMOSettings->setNcoEnableRx(settings.m_ncoEnableRx ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("ncoFrequencyRx") || force) {
        swgXTRXMIMOSettings->setNcoFrequencyRx(settings.m_ncoFrequencyRx);
    }
    if (deviceSettingsKeys.contains("antennaPathRx") || force) {
        swgXTRXMIMOSettings->setAntennaPathRx((int) settings.m_antennaPathRx);
    }
    // Rx0
    if (deviceSettingsKeys.contains("lpfBWRx0") || force) {
        swgXTRXMIMOSettings->setLpfBwRx0(settings.m_lpfBWRx0);
    }
    if (deviceSettingsKeys.contains("gainRx0") || force) {
        swgXTRXMIMOSettings->setGainRx0(settings.m_gainRx0);
    }
    if (deviceSettingsKeys.contains("gainModeRx0") || force) {
        swgXTRXMIMOSettings->setGainModeRx0((int) settings.m_gainModeRx0);
    }
    if (deviceSettingsKeys.contains("lnaGainRx0") || force) {
        swgXTRXMIMOSettings->setLnaGainRx0(settings.m_lnaGainRx0);
    }
    if (deviceSettingsKeys.contains("tiaGainRx0") || force) {
        swgXTRXMIMOSettings->setTiaGainRx0(settings.m_tiaGainRx0);
    }
    if (deviceSettingsKeys.contains("pgaGainRx0") || force) {
        swgXTRXMIMOSettings->setPgaGainRx0(settings.m_pgaGainRx0);
    }
    if (deviceSettingsKeys.contains("pwrmodeRx0") || force) {
        swgXTRXMIMOSettings->setPwrmodeRx0(settings.m_pwrmodeRx0);
    }
    // Rx1
    if (deviceSettingsKeys.contains("lpfBWRx1") || force) {
        swgXTRXMIMOSettings->setLpfBwRx1(settings.m_lpfBWRx1);
    }
    if (deviceSettingsKeys.contains("gainRx1") || force) {
        swgXTRXMIMOSettings->setGainRx1(settings.m_gainRx1);
    }
    if (deviceSettingsKeys.contains("gainModeRx1") || force) {
        swgXTRXMIMOSettings->setGainModeRx1((int) settings.m_gainModeRx1);
    }
    if (deviceSettingsKeys.contains("lnaGainRx1") || force) {
        swgXTRXMIMOSettings->setLnaGainRx1(settings.m_lnaGainRx1);
    }
    if (deviceSettingsKeys.contains("tiaGainRx1") || force) {
        swgXTRXMIMOSettings->setTiaGainRx1(settings.m_tiaGainRx1);
    }
    if (deviceSettingsKeys.contains("pgaGainRx1") || force) {
        swgXTRXMIMOSettings->setPgaGainRx1(settings.m_pgaGainRx1);
    }
    if (deviceSettingsKeys.contains("pwrmodeRx1") || force) {
        swgXTRXMIMOSettings->setPwrmodeRx1(settings.m_pwrmodeRx1);
    }
    // Tx
    if (deviceSettingsKeys.contains("txDevSampleRate") || force) {
        swgXTRXMIMOSettings->setTxDevSampleRate(settings.m_txDevSampleRate);
    }
    if (deviceSettingsKeys.contains("log2HardInterp") || force) {
        swgXTRXMIMOSettings->setLog2HardInterp(settings.m_log2HardInterp);
    }
    if (deviceSettingsKeys.contains("log2SoftInterp") || force) {
        swgXTRXMIMOSettings->setLog2SoftInterp(settings.m_log2SoftInterp);
    }
    if (deviceSettingsKeys.contains("txCenterFrequency") || force) {
        swgXTRXMIMOSettings->setTxCenterFrequency(settings.m_txCenterFrequency);
    }
    if (deviceSettingsKeys.contains("ncoEnableTx") || force) {
        swgXTRXMIMOSettings->setNcoEnableTx(settings.m_ncoEnableTx ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("ncoFrequencyTx") || force) {
        swgXTRXMIMOSettings->setNcoFrequencyTx(settings.m_ncoFrequencyTx);
    }
    if (deviceSettingsKeys.contains("antennaPathTx") || force) {
        swgXTRXMIMOSettings->setAntennaPathTx((int) settings.m_antennaPathTx);
    }
    // Tx0
    if (deviceSettingsKeys.contains("lpfBWTx0") || force) {
        swgXTRXMIMOSettings->setLpfBwTx0(settings.m_lpfBWTx0);
    }
    if (deviceSettingsKeys.contains("gainTx0") || force) {
        swgXTRXMIMOSettings->setGainTx0(settings.m_gainTx0);
    }
    if (deviceSettingsKeys.contains("pwrmodeTx0") || force) {
        swgXTRXMIMOSettings->setPwrmodeTx0(settings.m_pwrmodeTx0);
    }
    // Tx0
    if (deviceSettingsKeys.contains("lpfBWTx0") || force) {
        swgXTRXMIMOSettings->setLpfBwTx0(settings.m_lpfBWTx0);
    }
    if (deviceSettingsKeys.contains("gainTx0") || force) {
        swgXTRXMIMOSettings->setGainTx0(settings.m_gainTx0);
    }
    if (deviceSettingsKeys.contains("pwrmodeTx0") || force) {
        swgXTRXMIMOSettings->setPwrmodeTx0(settings.m_pwrmodeTx0);
    }
    // Tx1
    if (deviceSettingsKeys.contains("lpfBWTx1") || force) {
        swgXTRXMIMOSettings->setLpfBwTx1(settings.m_lpfBWTx1);
    }
    if (deviceSettingsKeys.contains("gainTx1") || force) {
        swgXTRXMIMOSettings->setGainTx1(settings.m_gainTx1);
    }
    if (deviceSettingsKeys.contains("pwrmodeTx1") || force) {
        swgXTRXMIMOSettings->setPwrmodeTx1(settings.m_pwrmodeTx1);
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

void XTRXMIMO::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(2); // MIMO
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("XTRX"));

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

void XTRXMIMO::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "XTRXMIMO::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("XTRXMIMO::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
