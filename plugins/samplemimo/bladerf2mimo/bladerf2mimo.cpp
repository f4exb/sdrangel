///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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
#include "SWGTestMISettings.h"
#include "SWGDeviceReport.h"

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/dspdevicemimoengine.h"
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"
#include "bladerf2/devicebladerf2.h"

#include "bladerf2mithread.h"
#include "bladerf2mothread.h"
#include "bladerf2mimo.h"

MESSAGE_CLASS_DEFINITION(BladeRF2MIMO::MsgConfigureBladeRF2MIMO, Message)
MESSAGE_CLASS_DEFINITION(BladeRF2MIMO::MsgStartStop, Message)

BladeRF2MIMO::BladeRF2MIMO(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
    m_sourceThread(nullptr),
    m_sinkThread(nullptr),
	m_deviceDescription("BladeRF2MIMO"),
	m_runningRx(false),
    m_runningTx(false),
    m_dev(nullptr),
    m_open(false)
{
    m_open = openDevice();

    if (m_dev)
    {
        const bladerf_gain_modes *modes = nullptr;
        int nbModes = m_dev->getGainModesRx(&modes);

        if (modes)
        {
            for (int i = 0; i < nbModes; i++) {
                m_rxGainModes.push_back(GainMode{QString(modes[i].name), modes[i].mode});
            }
        }
    }

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
        &BladeRF2MIMO::networkManagerFinished
    );
}

BladeRF2MIMO::~BladeRF2MIMO()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &BladeRF2MIMO::networkManagerFinished
    );
    delete m_networkManager;
    closeDevice();
}

void BladeRF2MIMO::destroy()
{
    delete this;
}

bool BladeRF2MIMO::openDevice()
{
    m_dev = new DeviceBladeRF2();
    char serial[256];
    strcpy(serial, qPrintable(m_deviceAPI->getSamplingDeviceSerial()));

    if (!m_dev->open(serial))
    {
        qCritical("BladeRF2MIMO::openDevice: cannot open BladeRF2 device");
        return false;
    }
    else
    {
        qDebug("BladeRF2MIMO::openDevice: device opened");
        return true;
    }
}

void BladeRF2MIMO::closeDevice()
{
    if (m_dev == nullptr) { // was never open
        return;
    }

    if (m_runningRx) {
        stopRx();
    }

    if (m_runningTx) {
        stopTx();
    }

    m_dev->close();
    delete m_dev;
    m_dev = nullptr;
    m_open = false;
}

void BladeRF2MIMO::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

bool BladeRF2MIMO::startRx()
{
    qDebug("BladeRF2MIMO::startRx");

    if (!m_open)
    {
        qCritical("BladeRF2MIMO::startRx: device was not opened");
        return false;
    }

	QMutexLocker mutexLocker(&m_mutex);

    if (m_runningRx) {
        stopRx();
    }

    m_sourceThread = new BladeRF2MIThread(m_dev->getDev());
    m_sampleMIFifo.reset();
    m_sourceThread->setFifo(&m_sampleMIFifo);
    m_sourceThread->setFcPos(m_settings.m_fcPosRx);
    m_sourceThread->setLog2Decimation(m_settings.m_log2Decim);
    m_sourceThread->setIQOrder(m_settings.m_iqOrder);

    for (int i = 0; i < 2; i++)
    {
        if (!m_dev->openRx(i)) {
            qCritical("BladeRF2MIMO::startRx: Rx channel %u cannot be enabled", i);
        }
    }

	m_sourceThread->startWork();
	mutexLocker.unlock();
	m_runningRx = true;

    return true;
}

bool BladeRF2MIMO::startTx()
{
    qDebug("BladeRF2MIMO::startTx");

    if (!m_open)
    {
        qCritical("BladeRF2MIMO::startRx: device was not opened");
        return false;
    }

	QMutexLocker mutexLocker(&m_mutex);

    if (m_runningTx) {
        stopTx();
    }

    m_sinkThread = new BladeRF2MOThread(m_dev->getDev());
    m_sampleMOFifo.reset();
    m_sinkThread->setFifo(&m_sampleMOFifo);
    m_sinkThread->setFcPos(m_settings.m_fcPosTx);
    m_sinkThread->setLog2Interpolation(m_settings.m_log2Interp);

    for (int i = 0; i < 2; i++)
    {
        if (!m_dev->openTx(i)) {
            qCritical("BladeRF2MIMO::startTx: Tx channel %u cannot be enabled", i);
        }
    }

	m_sinkThread->startWork();
	mutexLocker.unlock();
	m_runningTx = true;

    return true;
}

void BladeRF2MIMO::stopRx()
{
    qDebug("BladeRF2MIMO::stopRx");

    if (!m_sourceThread) {
        return;
    }

	QMutexLocker mutexLocker(&m_mutex);

    m_sourceThread->stopWork();
    delete m_sourceThread;
    m_sourceThread = nullptr;
    m_runningRx = false;

    for (int i = 0; i < 2; i++) {
        m_dev->closeRx(i);
    }
}

void BladeRF2MIMO::stopTx()
{
    qDebug("BladeRF2MIMO::stopTx");

    if (!m_sinkThread) {
        return;
    }

	QMutexLocker mutexLocker(&m_mutex);

    m_sinkThread->stopWork();
    delete m_sinkThread;
    m_sinkThread = nullptr;
    m_runningTx = false;

    for (int i = 0; i < 2; i++) {
        m_dev->closeTx(i);
    }
}

QByteArray BladeRF2MIMO::serialize() const
{
    return m_settings.serialize();
}

bool BladeRF2MIMO::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureBladeRF2MIMO* message = MsgConfigureBladeRF2MIMO::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureBladeRF2MIMO* messageToGUI = MsgConfigureBladeRF2MIMO::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& BladeRF2MIMO::getDeviceDescription() const
{
	return m_deviceDescription;
}

int BladeRF2MIMO::getSourceSampleRate(int index) const
{
    (void) index;
    int rate = m_settings.m_devSampleRate;
    return (rate / (1<<m_settings.m_log2Decim));
}

int BladeRF2MIMO::getSinkSampleRate(int index) const
{
    (void) index;
    int rate = m_settings.m_devSampleRate;
    return (rate / (1<<m_settings.m_log2Interp));
}

quint64 BladeRF2MIMO::getSourceCenterFrequency(int index) const
{
    (void) index;
    return m_settings.m_rxCenterFrequency;
}

void BladeRF2MIMO::setSourceCenterFrequency(qint64 centerFrequency, int index)
{
    (void) index;
    BladeRF2MIMOSettings settings = m_settings;
    settings.m_rxCenterFrequency = centerFrequency;

    MsgConfigureBladeRF2MIMO* message = MsgConfigureBladeRF2MIMO::create(settings, QList<QString>{"rxCenterFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureBladeRF2MIMO* messageToGUI = MsgConfigureBladeRF2MIMO::create(settings, QList<QString>{"rxCenterFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

quint64 BladeRF2MIMO::getSinkCenterFrequency(int index) const
{
    (void) index;
    return m_settings.m_txCenterFrequency;
}

void BladeRF2MIMO::setSinkCenterFrequency(qint64 centerFrequency, int index)
{
    (void) index;
    BladeRF2MIMOSettings settings = m_settings;
    settings.m_txCenterFrequency = centerFrequency;

    MsgConfigureBladeRF2MIMO* message = MsgConfigureBladeRF2MIMO::create(settings, QList<QString>{"txCenterFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureBladeRF2MIMO* messageToGUI = MsgConfigureBladeRF2MIMO::create(settings, QList<QString>{"txCenterFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool BladeRF2MIMO::handleMessage(const Message& message)
{
    if (MsgConfigureBladeRF2MIMO::match(message))
    {
        MsgConfigureBladeRF2MIMO& conf = (MsgConfigureBladeRF2MIMO&) message;
        qDebug() << "BladeRF2MIMO::handleMessage: MsgConfigureBladeRF2MIMO";

        bool success = applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());

        if (!success) {
            qDebug("BladeRF2MIMO::handleMessage: config error");
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "BladeRF2MIMO::handleMessage: "
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

bool BladeRF2MIMO::applySettings(const BladeRF2MIMOSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    bool forwardChangeRxDSP = false;
    bool forwardChangeTxDSP = false;

    qDebug() << "BladeRF2MIMO::applySettings: force:" << force << settings.getDebugString(settingsKeys, force);

    struct bladerf *dev = m_dev ? m_dev->getDev() : nullptr;

    qint64 rxXlatedDeviceCenterFrequency = settings.m_rxCenterFrequency;
    rxXlatedDeviceCenterFrequency -= settings.m_rxTransverterMode ? settings.m_rxTransverterDeltaFrequency : 0;
    rxXlatedDeviceCenterFrequency = rxXlatedDeviceCenterFrequency < 0 ? 0 : rxXlatedDeviceCenterFrequency;


    qint64 txXlatedDeviceCenterFrequency = settings.m_txCenterFrequency;
    txXlatedDeviceCenterFrequency -= settings.m_txTransverterMode ? settings.m_txTransverterDeltaFrequency : 0;
    txXlatedDeviceCenterFrequency = txXlatedDeviceCenterFrequency < 0 ? 0 : txXlatedDeviceCenterFrequency;

    if (settingsKeys.contains("devSampleRate") || force)
    {
        if (dev)
        {
            unsigned int actualSamplerate;
            int status = bladerf_set_sample_rate(dev, BLADERF_CHANNEL_RX(0), settings.m_devSampleRate, &actualSamplerate);

            if (status < 0)
            {
                qCritical("BladeRF2MIMO::applySettings: could not set sample rate: %d: %s",
                        settings.m_devSampleRate, bladerf_strerror(status));
            }
            else
            {
                qDebug() << "BladeRF2MIMO::applySettings: bladerf_set_sample_rate: actual sample rate is " << actualSamplerate;
            }

        }
    }

    // Rx settings

    if (settingsKeys.contains("dcBlock") ||
        settingsKeys.contains("iqCorrection") || force)
    {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection, 0);
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection, 1);
    }

    if (settingsKeys.contains("rxBandwidth") || force)
    {
        if (dev)
        {
            unsigned int actualBandwidth;
            int status = bladerf_set_bandwidth(dev, BLADERF_CHANNEL_RX(0), settings.m_rxBandwidth, &actualBandwidth);

            if(status < 0) {
                qCritical("BladeRF2MIMO::applySettings: could not set RX0 bandwidth: %d: %s", settings.m_rxBandwidth, bladerf_strerror(status));
            } else {
                qDebug() << "BladeRF2MIMO::applySettings: RX0: bladerf_set_bandwidth: actual bandwidth is " << actualBandwidth;
            }

            status = bladerf_set_bandwidth(dev, BLADERF_CHANNEL_RX(1), settings.m_rxBandwidth, &actualBandwidth);

            if(status < 0) {
                qCritical("BladeRF2MIMO::applySettings: could not set RX1 bandwidth: %d: %s", settings.m_rxBandwidth, bladerf_strerror(status));
            } else {
                qDebug() << "BladeRF2MIMO::applySettings: RX1: bladerf_set_bandwidth: actual bandwidth is " << actualBandwidth;
            }
        }
    }

    if (settingsKeys.contains("fcPosRx") || force)
    {
        if (m_sourceThread)
        {
            m_sourceThread->setFcPos((int) settings.m_fcPosRx);
            qDebug() << "BladeRF2MIMO::applySettings: set Rx fc pos (enum) to " << (int) settings.m_fcPosRx;
        }
    }

    if (settingsKeys.contains("log2Decim") || force)
    {
        if (m_sourceThread)
        {
            m_sourceThread->setLog2Decimation(settings.m_log2Decim);
            qDebug() << "BladeRF2MIMO::applySettings: set decimation to " << (1<<settings.m_log2Decim);
        }
    }

    if (settingsKeys.contains("iqOrder") || force)
    {
        if (m_sourceThread)
        {
            m_sourceThread->setIQOrder(settings.m_iqOrder);
            qDebug() << "BladeRF2MIMO::applySettings: set IQ order to " << (settings.m_iqOrder ? "IQ" : "QI");
        }
    }

    if (settingsKeys.contains("fcPosTx") || force)
    {
        if (m_sourceThread)
        {
            m_sourceThread->setFcPos((int) settings.m_fcPosTx);
            qDebug() << "BladeRF2MIMO::applySettings: set Tx fc pos (enum) to " << (int) settings.m_fcPosTx;
        }
    }

    if (settingsKeys.contains("log2Interp") || force)
    {
        if (m_sinkThread)
        {
            m_sinkThread->setLog2Interpolation(settings.m_log2Interp);
            qDebug() << "BladeRF2Input::applySettings: set interpolation to " << (1<<settings.m_log2Interp);
        }
    }

    // if ((m_settings.m_rxCenterFrequency != settings.m_rxCenterFrequency)
    //     || (m_settings.m_rxTransverterMode != settings.m_rxTransverterMode)
    //     || (m_settings.m_rxTransverterDeltaFrequency != settings.m_rxTransverterDeltaFrequency)
    //     || (m_settings.m_LOppmTenths != settings.m_LOppmTenths)
    //     || (m_settings.m_devSampleRate != settings.m_devSampleRate)
    //     || (m_settings.m_fcPosRx != settings.m_fcPosRx)
    //     || (m_settings.m_log2Decim != settings.m_log2Decim) || force)
    if (settingsKeys.contains("rxCenterFrequency")
        || settingsKeys.contains("rxTransverterMode")
        || settingsKeys.contains("rxTransverterDeltaFrequency")
        || settingsKeys.contains("LOppmTenths")
        || settingsKeys.contains("devSampleRate")
        || settingsKeys.contains("fcPosRx")
        || settingsKeys.contains("log2Decim") || force)
    {
        if (dev)
        {
            qint64 deviceCenterFrequency = DeviceSampleSource::calculateDeviceCenterFrequency(
                    rxXlatedDeviceCenterFrequency,
                    0,
                    settings.m_log2Decim,
                    (DeviceSampleSource::fcPos_t) settings.m_fcPosRx,
                    settings.m_devSampleRate,
                    DeviceSampleSource::FrequencyShiftScheme::FSHIFT_STD,
                    false);
            setRxDeviceCenterFrequency(dev, deviceCenterFrequency, settings.m_LOppmTenths);
        }

        forwardChangeRxDSP = true;
    }

    if (settingsKeys.contains("rxBiasTee") || force)
    {
        if (m_dev) {
            m_dev->setBiasTeeRx(settings.m_rxBiasTee);
        }
    }

    if (settingsKeys.contains("rx0GainMode") || force)
    {
        if (dev)
        {
            int status = bladerf_set_gain_mode(dev, BLADERF_CHANNEL_RX(0), (bladerf_gain_mode) settings.m_rx0GainMode);

            if (status < 0) {
                qWarning("BladeRF2MIMO::applySettings: RX0: bladerf_set_gain_mode(%d) failed: %s",
                        settings.m_rx0GainMode, bladerf_strerror(status));
            } else {
                qDebug("BladeRF2MIMO::applySettings: RX0: bladerf_set_gain_mode(%d)", settings.m_rx0GainMode);
            }
        }
    }

    if (settingsKeys.contains("rx1GainMode") || force)
    {
        if (dev)
        {
            int status = bladerf_set_gain_mode(dev, BLADERF_CHANNEL_RX(1), (bladerf_gain_mode) settings.m_rx1GainMode);

            if (status < 0) {
                qWarning("BladeRF2MIMO::applySettings: RX1: bladerf_set_gain_mode(%d) failed: %s",
                        settings.m_rx1GainMode, bladerf_strerror(status));
            } else {
                qDebug("BladeRF2MIMO::applySettings: RX1: bladerf_set_gain_mode(%d)", settings.m_rx1GainMode);
            }
        }
    }

    if (settingsKeys.contains("rx0GlobalGain")
       || (settingsKeys.contains("rx0GlobalGain") && (settings.m_rx0GlobalGain == BLADERF_GAIN_MANUAL)) || force)
    {
        if (dev)
        {
            int status = bladerf_set_gain(dev, BLADERF_CHANNEL_RX(0), settings.m_rx0GlobalGain);

            if (status < 0) {
                qWarning("BladeRF2MIMO::applySettings: RX0: bladerf_set_gain(%d) failed: %s",
                        settings.m_rx0GlobalGain, bladerf_strerror(status));
            } else {
                qDebug("BladeRF2MIMO::applySettings: RX0: bladerf_set_gain(%d)", settings.m_rx0GlobalGain);
            }
        }
    }

    if (settingsKeys.contains("rx1GlobalGain")
       || (settingsKeys.contains("rx1GlobalGain") && (settings.m_rx1GlobalGain == BLADERF_GAIN_MANUAL)) || force)
    {
        if (dev)
        {
            int status = bladerf_set_gain(dev, BLADERF_CHANNEL_RX(1), settings.m_rx1GlobalGain);

            if (status < 0) {
                qWarning("BladeRF2MIMO::applySettings: RX1: bladerf_set_gain(%d) failed: %s",
                        settings.m_rx1GlobalGain, bladerf_strerror(status));
            } else {
                qDebug("BladeRF2MIMO::applySettings: RX1: bladerf_set_gain(%d)", settings.m_rx1GlobalGain);
            }
        }
    }

    // Tx settings

    if (settingsKeys.contains("txCenterFrequency")
        || settingsKeys.contains("txTransverterMode")
        || settingsKeys.contains("txTransverterDeltaFrequency")
        || settingsKeys.contains("fcPosTx")
        || settingsKeys.contains("log2Interp")
        || settingsKeys.contains("LOppmTenths")
        || settingsKeys.contains("devSampleRate") || force)
    {
        if (dev)
        {
            qint64 deviceCenterFrequency = DeviceSampleSink::calculateDeviceCenterFrequency(
                settings.m_txCenterFrequency,
                settings.m_txTransverterDeltaFrequency,
                settings.m_log2Interp,
                (DeviceSampleSink::fcPos_t) settings.m_fcPosTx,
                settings.m_devSampleRate,
                settings.m_txTransverterMode);
            setTxDeviceCenterFrequency(dev, deviceCenterFrequency, settings.m_LOppmTenths);
        }

        forwardChangeTxDSP = true;
    }

    if (settingsKeys.contains("txBandwidth") || force)
    {
        if (dev)
        {
            unsigned int actualBandwidth;
            int status = bladerf_set_bandwidth(dev, BLADERF_CHANNEL_TX(0), settings.m_txBandwidth, &actualBandwidth);

            if (status < 0)
            {
                qCritical("BladeRF2MIMO::applySettings: TX0: could not set bandwidth: %d: %s",
                        settings.m_txBandwidth, bladerf_strerror(status));
            }
            else
            {
                qDebug() << "BladeRF2MIMO::applySettings: TX0: bladerf_set_bandwidth: actual bandwidth is " << actualBandwidth;
            }

            status = bladerf_set_bandwidth(dev, BLADERF_CHANNEL_TX(0), settings.m_txBandwidth, &actualBandwidth);

            if (status < 0)
            {
                qCritical("BladeRF2MIMO::applySettings: TX1: could not set bandwidth: %d: %s",
                        settings.m_txBandwidth, bladerf_strerror(status));
            }
            else
            {
                qDebug() << "BladeRF2MIMO::applySettings: TX1: bladerf_set_bandwidth: actual bandwidth is " << actualBandwidth;
            }
        }
    }

    if (settingsKeys.contains("log2Interp") || force)
    {
        if (m_sinkThread)
        {
            m_sinkThread->setLog2Interpolation(settings.m_log2Interp);
            qDebug() << "BladeRF2MIMO::applySettings: set interpolation to " << (1<<settings.m_log2Interp);
        }
    }

    if (settingsKeys.contains("txBiasTee") || force)
    {
        if (m_dev) {
            m_dev->setBiasTeeTx(settings.m_txBiasTee);
        }
    }

    if (settingsKeys.contains("tx0GlobalGain") || force)
    {
        if (dev)
        {
            int status = bladerf_set_gain(dev, BLADERF_CHANNEL_TX(0), settings.m_tx0GlobalGain);

            if (status < 0) {
                qWarning("BladeRF2MIMO::applySettings: TX0: bladerf_set_gain(%d) failed: %s",
                        settings.m_tx0GlobalGain, bladerf_strerror(status));
            } else {
                qDebug("BladeRF2MIMO::applySettings: TX0: bladerf_set_gain(%d)", settings.m_tx0GlobalGain);
            }
        }
    }

    if (settingsKeys.contains("tx1GlobalGain") || force)
    {
        if (dev)
        {
            int status = bladerf_set_gain(dev, BLADERF_CHANNEL_TX(1), settings.m_tx1GlobalGain);

            if (status < 0) {
                qWarning("BladeRF2MIMO::applySettings: TX1: bladerf_set_gain(%d) failed: %s",
                        settings.m_tx1GlobalGain, bladerf_strerror(status));
            } else {
                qDebug("BladeRF2MIMO::applySettings: TX1: bladerf_set_gain(%d)", settings.m_tx1GlobalGain);
            }
        }
    }

    if (forwardChangeRxDSP)
    {
        int sampleRate = settings.m_devSampleRate/(1<<settings.m_log2Decim);
        DSPMIMOSignalNotification *notif0 = new DSPMIMOSignalNotification(sampleRate, settings.m_rxCenterFrequency, true, 0);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif0);
        DSPMIMOSignalNotification *notif1 = new DSPMIMOSignalNotification(sampleRate, settings.m_rxCenterFrequency, true, 1);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif1);
    }

    if (forwardChangeTxDSP)
    {
        int sampleRate = settings.m_devSampleRate/(1<<settings.m_log2Interp);
        DSPMIMOSignalNotification *notif0 = new DSPMIMOSignalNotification(sampleRate, settings.m_txCenterFrequency, false, 0);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif0);
        DSPMIMOSignalNotification *notif1 = new DSPMIMOSignalNotification(sampleRate, settings.m_txCenterFrequency, false, 1);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif1);
    }

    // Reverse API settings

    if (settingsKeys.contains("useReverseAPI"))
    {
        bool fullUpdate = (settingsKeys.contains("useReverseAPI") && settings.m_useReverseAPI) ||
            settingsKeys.contains("reverseAPIAddress") ||
            settingsKeys.contains("reverseAPIPort") ||
            settingsKeys.contains("reverseAPIDeviceIndex");
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    m_settings = settings;
    return true;
}

bool BladeRF2MIMO::setRxDeviceCenterFrequency(struct bladerf *dev, quint64 freq_hz, int loPpmTenths)
{
    qint64 df = ((qint64)freq_hz * loPpmTenths) / 10000000LL;
    freq_hz += df;

    int status = bladerf_set_frequency(dev, BLADERF_CHANNEL_RX(0), freq_hz);

    if (status < 0)
    {
        qWarning("BladeRF2MIMO::setRxDeviceCenterFrequency: RX0: bladerf_set_frequency(%lld) failed: %s",
                freq_hz, bladerf_strerror(status));
        return false;
    }
    else
    {
        qDebug("BladeRF2MIMO::setRxDeviceCenterFrequency: RX0: bladerf_set_frequency(%lld)", freq_hz);
    }

    status = bladerf_set_frequency(dev, BLADERF_CHANNEL_RX(1), freq_hz);

    if (status < 0)
    {
        qWarning("BladeRF2MIMO::setRxDeviceCenterFrequency: RX1: bladerf_set_frequency(%lld) failed: %s",
                freq_hz, bladerf_strerror(status));
        return false;
    }
    else
    {
        qDebug("BladeRF2MIMO::setRxDeviceCenterFrequency: RX1: bladerf_set_frequency(%lld)", freq_hz);
    }

    return true;
}

bool BladeRF2MIMO::setTxDeviceCenterFrequency(struct bladerf *dev, quint64 freq_hz, int loPpmTenths)
{
    qint64 df = ((qint64)freq_hz * loPpmTenths) / 10000000LL;
    freq_hz += df;

    int status = bladerf_set_frequency(dev, BLADERF_CHANNEL_TX(0), freq_hz);

    if (status < 0) {
        qWarning("BladeRF2Output::setTxDeviceCenterFrequency: TX0: bladerf_set_frequency(%lld) failed: %s",
                freq_hz, bladerf_strerror(status));
        return false;
    }
    else
    {
        qDebug("BladeRF2Output::setTxDeviceCenterFrequency: TX0: bladerf_set_frequency(%lld)", freq_hz);
    }

    status = bladerf_set_frequency(dev, BLADERF_CHANNEL_TX(1), freq_hz);

    if (status < 0) {
        qWarning("BladeRF2Output::setTxDeviceCenterFrequency: TX1: bladerf_set_frequency(%lld) failed: %s",
                freq_hz, bladerf_strerror(status));
        return false;
    }
    else
    {
        qDebug("BladeRF2Output::setTxDeviceCenterFrequency: TX1: bladerf_set_frequency(%lld)", freq_hz);
    }

    return true;
}

void BladeRF2MIMO::getRxFrequencyRange(uint64_t& min, uint64_t& max, int& step, float& scale)
{
    if (m_dev) {
        m_dev->getFrequencyRangeRx(min, max, step, scale);
    }
}

void BladeRF2MIMO::getRxSampleRateRange(int& min, int& max, int& step, float& scale)
{
    if (m_dev) {
        m_dev->getSampleRateRangeRx(min, max, step, scale);
    }
}

void BladeRF2MIMO::getRxBandwidthRange(int& min, int& max, int& step, float& scale)
{
    if (m_dev) {
        m_dev->getBandwidthRangeRx(min, max, step, scale);
    }
}

void BladeRF2MIMO::getRxGlobalGainRange(int& min, int& max, int& step, float& scale)
{
    if (m_dev) {
        m_dev->getGlobalGainRangeRx(min, max, step, scale);
    }
}

void BladeRF2MIMO::getTxFrequencyRange(uint64_t& min, uint64_t& max, int& step, float& scale)
{
    if (m_dev) {
        m_dev->getFrequencyRangeTx(min, max, step, scale);
    }
}

void BladeRF2MIMO::getTxSampleRateRange(int& min, int& max, int& step, float& scale)
{
    if (m_dev) {
        m_dev->getSampleRateRangeTx(min, max, step, scale);
    }
}

void BladeRF2MIMO::getTxBandwidthRange(int& min, int& max, int& step, float& scale)
{
    if (m_dev) {
        m_dev->getBandwidthRangeTx(min, max, step, scale);
    }
}

void BladeRF2MIMO::getTxGlobalGainRange(int& min, int& max, int& step, float& scale)
{
    if (m_dev) {
        m_dev->getGlobalGainRangeTx(min, max, step, scale);
    }
}

int BladeRF2MIMO::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setBladeRf2MimoSettings(new SWGSDRangel::SWGBladeRF2MIMOSettings());
    response.getBladeRf2MimoSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int BladeRF2MIMO::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    BladeRF2MIMOSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureBladeRF2MIMO *msg = MsgConfigureBladeRF2MIMO::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureBladeRF2MIMO *msgToGUI = MsgConfigureBladeRF2MIMO::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void BladeRF2MIMO::webapiUpdateDeviceSettings(
        BladeRF2MIMOSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getBladeRf2MimoSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("LOppmTenths")) {
        settings.m_LOppmTenths = response.getBladeRf2MimoSettings()->getLOppmTenths();
    }

    if (deviceSettingsKeys.contains("rxCenterFrequency")) {
        settings.m_rxCenterFrequency = response.getBladeRf2MimoSettings()->getRxCenterFrequency();
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getBladeRf2MimoSettings()->getLog2Decim();
    }
    if (deviceSettingsKeys.contains("iqOrder")) {
        settings.m_iqOrder = response.getBladeRf2MimoSettings()->getIqOrder() != 0;
    }
    if (deviceSettingsKeys.contains("fcPosRx")) {
        settings.m_fcPosRx = static_cast<BladeRF2MIMOSettings::fcPos_t>(response.getBladeRf2MimoSettings()->getFcPosRx());
    }
    if (deviceSettingsKeys.contains("rxBandwidth")) {
        settings.m_rxBandwidth = response.getBladeRf2MimoSettings()->getRxBandwidth();
    }
    if (deviceSettingsKeys.contains("rx0GainMode")) {
        settings.m_rx0GainMode = response.getBladeRf2MimoSettings()->getRx0GainMode();
    }
    if (deviceSettingsKeys.contains("rx0GlobalGain")) {
        settings.m_rx0GlobalGain = response.getBladeRf2MimoSettings()->getRx0GlobalGain();
    }
    if (deviceSettingsKeys.contains("rx1GainMode")) {
        settings.m_rx1GainMode = response.getBladeRf2MimoSettings()->getRx1GainMode();
    }
    if (deviceSettingsKeys.contains("rx1GlobalGain")) {
        settings.m_rx1GlobalGain = response.getBladeRf2MimoSettings()->getRx1GlobalGain();
    }
    if (deviceSettingsKeys.contains("rxBiasTee")) {
        settings.m_rxBiasTee = response.getBladeRf2MimoSettings()->getRxBiasTee() != 0;
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getBladeRf2MimoSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getBladeRf2MimoSettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("rxTransverterDeltaFrequency")) {
        settings.m_rxTransverterDeltaFrequency = response.getBladeRf2MimoSettings()->getRxTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("rxTransverterMode")) {
        settings.m_rxTransverterMode = response.getBladeRf2MimoSettings()->getRxTransverterMode() != 0;
    }

    if (deviceSettingsKeys.contains("txCenterFrequency")) {
        settings.m_txCenterFrequency = response.getBladeRf2MimoSettings()->getTxCenterFrequency();
    }
    if (deviceSettingsKeys.contains("log2Interp")) {
        settings.m_log2Interp = response.getBladeRf2MimoSettings()->getLog2Interp();
    }
    if (deviceSettingsKeys.contains("fcPosTx")) {
        settings.m_fcPosRx = static_cast<BladeRF2MIMOSettings::fcPos_t>(response.getBladeRf2MimoSettings()->getFcPosTx());
    }
    if (deviceSettingsKeys.contains("txBandwidth")) {
        settings.m_txBandwidth = response.getBladeRf2MimoSettings()->getTxBandwidth();
    }
    if (deviceSettingsKeys.contains("tx0GlobalGain")) {
        settings.m_tx0GlobalGain = response.getBladeRf2MimoSettings()->getTx0GlobalGain();
    }
    if (deviceSettingsKeys.contains("tx1GlobalGain")) {
        settings.m_tx1GlobalGain = response.getBladeRf2MimoSettings()->getTx1GlobalGain();
    }
    if (deviceSettingsKeys.contains("txBiasTee")) {
        settings.m_txBiasTee = response.getBladeRf2MimoSettings()->getTxBiasTee() != 0;
    }
    if (deviceSettingsKeys.contains("txTransverterMode")) {
        settings.m_txTransverterMode = response.getBladeRf2MimoSettings()->getTxTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("txTransverterDeltaFrequency")) {
        settings.m_txTransverterDeltaFrequency = response.getBladeRf2MimoSettings()->getTxTransverterDeltaFrequency();
    }

    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getBladeRf2MimoSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getBladeRf2MimoSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getBladeRf2MimoSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getBladeRf2MimoSettings()->getReverseApiDeviceIndex();
    }
}

void BladeRF2MIMO::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const BladeRF2MIMOSettings& settings)
{
    response.getBladeRf2MimoSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getBladeRf2MimoSettings()->setLOppmTenths(settings.m_LOppmTenths);

    response.getBladeRf2MimoSettings()->setRxCenterFrequency(settings.m_rxCenterFrequency);
    response.getBladeRf2MimoSettings()->setLog2Decim(settings.m_log2Decim);
    response.getBladeRf2MimoSettings()->setIqOrder(settings.m_iqOrder ? 1 : 0);
    response.getBladeRf2MimoSettings()->setFcPosRx((int) settings.m_fcPosRx);
    response.getBladeRf2MimoSettings()->setRxBandwidth(settings.m_rxBandwidth);
    response.getBladeRf2MimoSettings()->setRx0GainMode(settings.m_rx0GainMode);
    response.getBladeRf2MimoSettings()->setRx0GlobalGain(settings.m_rx0GlobalGain);
    response.getBladeRf2MimoSettings()->setRx1GainMode(settings.m_rx1GainMode);
    response.getBladeRf2MimoSettings()->setRx1GlobalGain(settings.m_rx1GlobalGain);
    response.getBladeRf2MimoSettings()->setRxBiasTee(settings.m_rxBiasTee ? 1 : 0);
    response.getBladeRf2MimoSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getBladeRf2MimoSettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    response.getBladeRf2MimoSettings()->setRxTransverterDeltaFrequency(settings.m_rxTransverterDeltaFrequency);
    response.getBladeRf2MimoSettings()->setRxTransverterMode(settings.m_rxTransverterMode ? 1 : 0);

    response.getBladeRf2MimoSettings()->setTxCenterFrequency(settings.m_txCenterFrequency);
    response.getBladeRf2MimoSettings()->setLog2Interp(settings.m_log2Interp);
    response.getBladeRf2MimoSettings()->setFcPosTx((int) settings.m_fcPosTx);
    response.getBladeRf2MimoSettings()->setTxBandwidth(settings.m_txBandwidth);
    response.getBladeRf2MimoSettings()->setTx0GlobalGain(settings.m_tx0GlobalGain);
    response.getBladeRf2MimoSettings()->setTx1GlobalGain(settings.m_tx1GlobalGain);
    response.getBladeRf2MimoSettings()->setTxBiasTee(settings.m_txBiasTee ? 1 : 0);
    response.getBladeRf2MimoSettings()->setTxTransverterDeltaFrequency(settings.m_txTransverterDeltaFrequency);
    response.getBladeRf2MimoSettings()->setTxTransverterMode(settings.m_txTransverterMode ? 1 : 0);

    response.getBladeRf2MimoSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getBladeRf2MimoSettings()->getReverseApiAddress()) {
        *response.getBladeRf2MimoSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getBladeRf2MimoSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getBladeRf2MimoSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getBladeRf2MimoSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

int BladeRF2MIMO::webapiRunGet(
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

int BladeRF2MIMO::webapiRun(
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

void BladeRF2MIMO::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const BladeRF2MIMOSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(2); // MIMO
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("BladeRF2"));
    swgDeviceSettings->setBladeRf2MimoSettings(new SWGSDRangel::SWGBladeRF2MIMOSettings());
    SWGSDRangel::SWGBladeRF2MIMOSettings *swgBladeRF2MIMOSettings = swgDeviceSettings->getBladeRf2MimoSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("devSampleRate") || force) {
        swgBladeRF2MIMOSettings->setDevSampleRate(settings.m_devSampleRate);
    }
    if (deviceSettingsKeys.contains("LOppmTenths") || force) {
        swgBladeRF2MIMOSettings->setLOppmTenths(settings.m_LOppmTenths);
    }

    if (deviceSettingsKeys.contains("rxCenterFrequency") || force) {
        swgBladeRF2MIMOSettings->setRxCenterFrequency(settings.m_rxCenterFrequency);
    }
    if (deviceSettingsKeys.contains("log2Decim") || force) {
        swgBladeRF2MIMOSettings->setLog2Decim(settings.m_log2Decim);
    }
    if (deviceSettingsKeys.contains("iqOrder") || force) {
        swgBladeRF2MIMOSettings->setIqOrder(settings.m_iqOrder ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("fcPosRx") || force) {
        swgBladeRF2MIMOSettings->setFcPosRx((int) settings.m_fcPosRx);
    }
    if (deviceSettingsKeys.contains("rxBandwidth") || force) {
        swgBladeRF2MIMOSettings->setRxBandwidth(settings.m_rxBandwidth);
    }
    if (deviceSettingsKeys.contains("rx0GainMode")) {
        swgBladeRF2MIMOSettings->setRx0GainMode(settings.m_rx0GainMode);
    }
    if (deviceSettingsKeys.contains("rx0GlobalGain")) {
        swgBladeRF2MIMOSettings->setRx0GlobalGain(settings.m_rx0GlobalGain);
    }
    if (deviceSettingsKeys.contains("rx1GainMode")) {
        swgBladeRF2MIMOSettings->setRx1GainMode(settings.m_rx1GainMode);
    }
    if (deviceSettingsKeys.contains("rx1GlobalGain")) {
        swgBladeRF2MIMOSettings->setRx1GlobalGain(settings.m_rx1GlobalGain);
    }
    if (deviceSettingsKeys.contains("rxBiasTee") || force) {
        swgBladeRF2MIMOSettings->setRxBiasTee(settings.m_rxBiasTee ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("dcBlock") || force) {
        swgBladeRF2MIMOSettings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("iqCorrection") || force) {
        swgBladeRF2MIMOSettings->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("rxTransverterDeltaFrequency") || force) {
        swgBladeRF2MIMOSettings->setRxTransverterDeltaFrequency(settings.m_rxTransverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("rxTransverterMode") || force) {
        swgBladeRF2MIMOSettings->setRxTransverterMode(settings.m_rxTransverterMode ? 1 : 0);
    }

    if (deviceSettingsKeys.contains("txCenterFrequency") || force) {
        swgBladeRF2MIMOSettings->setTxCenterFrequency(settings.m_txCenterFrequency);
    }
    if (deviceSettingsKeys.contains("log2Interp") || force) {
        swgBladeRF2MIMOSettings->setLog2Interp(settings.m_log2Interp);
    }
    if (deviceSettingsKeys.contains("fcPosTx") || force) {
        swgBladeRF2MIMOSettings->setFcPosTx((int) settings.m_fcPosTx);
    }
    if (deviceSettingsKeys.contains("txBandwidth") || force) {
        swgBladeRF2MIMOSettings->setTxBandwidth(settings.m_txBandwidth);
    }
    if (deviceSettingsKeys.contains("tx0GlobalGain") || force) {
        swgBladeRF2MIMOSettings->setTx0GlobalGain(settings.m_tx0GlobalGain);
    }
    if (deviceSettingsKeys.contains("tx1GlobalGain") || force) {
        swgBladeRF2MIMOSettings->setTx1GlobalGain(settings.m_tx1GlobalGain);
    }
    if (deviceSettingsKeys.contains("txBiasTee") || force) {
        swgBladeRF2MIMOSettings->setTxBiasTee(settings.m_txBiasTee ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("txTransverterDeltaFrequency") || force) {
        swgBladeRF2MIMOSettings->setTxTransverterDeltaFrequency(settings.m_txTransverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("txTransverterMode") || force) {
        swgBladeRF2MIMOSettings->setTxTransverterMode(settings.m_txTransverterMode ? 1 : 0);
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

void BladeRF2MIMO::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(2); // MIMO
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("BladeRF2"));

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

int BladeRF2MIMO::webapiReportGet(SWGSDRangel::SWGDeviceReport& response, QString& errorMessage)
{
    (void) errorMessage;
    response.setBladeRf2MimoReport(new SWGSDRangel::SWGBladeRF2MIMOReport());
    response.getBladeRf2MimoReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void BladeRF2MIMO::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    if (m_dev)
    {
        int min, max, step;
        float scale;
        uint64_t f_min, f_max;

        m_dev->getBandwidthRangeRx(min, max, step, scale);

        response.getBladeRf2MimoReport()->setBandwidthRangeRx(new SWGSDRangel::SWGRange);
        response.getBladeRf2MimoReport()->getBandwidthRangeRx()->setMin(min);
        response.getBladeRf2MimoReport()->getBandwidthRangeRx()->setMax(max);
        response.getBladeRf2MimoReport()->getBandwidthRangeRx()->setStep(step);
        response.getBladeRf2MimoReport()->getBandwidthRangeRx()->setScale(scale);

        m_dev->getFrequencyRangeRx(f_min, f_max, step, scale);

        response.getBladeRf2MimoReport()->setFrequencyRangeRx(new SWGSDRangel::SWGFrequencyRange);
        response.getBladeRf2MimoReport()->getFrequencyRangeRx()->setMin(f_min);
        response.getBladeRf2MimoReport()->getFrequencyRangeRx()->setMax(f_max);
        response.getBladeRf2MimoReport()->getFrequencyRangeRx()->setStep(step);
        response.getBladeRf2MimoReport()->getFrequencyRangeRx()->setScale(scale);

        m_dev->getGlobalGainRangeRx(min, max, step, scale);

        response.getBladeRf2MimoReport()->setGlobalGainRangeRx(new SWGSDRangel::SWGRange);
        response.getBladeRf2MimoReport()->getGlobalGainRangeRx()->setMin(min);
        response.getBladeRf2MimoReport()->getGlobalGainRangeRx()->setMax(max);
        response.getBladeRf2MimoReport()->getGlobalGainRangeRx()->setStep(step);
        response.getBladeRf2MimoReport()->getGlobalGainRangeRx()->setScale(scale);

        m_dev->getSampleRateRangeRx(min, max, step, scale);

        response.getBladeRf2MimoReport()->setSampleRateRangeRx(new SWGSDRangel::SWGRange);
        response.getBladeRf2MimoReport()->getSampleRateRangeRx()->setMin(min);
        response.getBladeRf2MimoReport()->getSampleRateRangeRx()->setMax(max);
        response.getBladeRf2MimoReport()->getSampleRateRangeRx()->setStep(step);
        response.getBladeRf2MimoReport()->getSampleRateRangeRx()->setScale(scale);

        m_dev->getBandwidthRangeTx(min, max, step, scale);

        response.getBladeRf2MimoReport()->setBandwidthRangeTx(new SWGSDRangel::SWGRange);
        response.getBladeRf2MimoReport()->getBandwidthRangeTx()->setMin(min);
        response.getBladeRf2MimoReport()->getBandwidthRangeTx()->setMax(max);
        response.getBladeRf2MimoReport()->getBandwidthRangeTx()->setStep(step);
        response.getBladeRf2MimoReport()->getBandwidthRangeTx()->setScale(scale);

        m_dev->getFrequencyRangeTx(f_min, f_max, step, scale);

        response.getBladeRf2MimoReport()->setFrequencyRangeTx(new SWGSDRangel::SWGFrequencyRange);
        response.getBladeRf2MimoReport()->getFrequencyRangeTx()->setMin(f_min);
        response.getBladeRf2MimoReport()->getFrequencyRangeTx()->setMax(f_max);
        response.getBladeRf2MimoReport()->getFrequencyRangeTx()->setStep(step);
        response.getBladeRf2MimoReport()->getFrequencyRangeTx()->setScale(scale);

        m_dev->getGlobalGainRangeTx(min, max, step, scale);

        response.getBladeRf2MimoReport()->setGlobalGainRangeTx(new SWGSDRangel::SWGRange);
        response.getBladeRf2MimoReport()->getGlobalGainRangeTx()->setMin(min);
        response.getBladeRf2MimoReport()->getGlobalGainRangeTx()->setMax(max);
        response.getBladeRf2MimoReport()->getGlobalGainRangeTx()->setStep(step);
        response.getBladeRf2MimoReport()->getGlobalGainRangeTx()->setScale(scale);

        m_dev->getSampleRateRangeTx(min, max, step, scale);

        response.getBladeRf2MimoReport()->setSampleRateRangeTx(new SWGSDRangel::SWGRange);
        response.getBladeRf2MimoReport()->getSampleRateRangeTx()->setMin(min);
        response.getBladeRf2MimoReport()->getSampleRateRangeTx()->setMax(max);
        response.getBladeRf2MimoReport()->getSampleRateRangeTx()->setStep(step);
        response.getBladeRf2MimoReport()->getSampleRateRangeTx()->setScale(scale);
    }
}

void BladeRF2MIMO::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "BladeRF2MIMO::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("BladeRF2MIMO::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
