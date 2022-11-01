///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Edouard Griffiths, F4EXB                                   //
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
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/dspdevicemimoengine.h"
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"
#include "plutosdr/deviceplutosdrparams.h"
#include "plutosdr/deviceplutosdrshared.h"

#include "plutosdrmithread.h"
#include "plutosdrmothread.h"
#include "plutosdrmimo.h"

MESSAGE_CLASS_DEFINITION(PlutoSDRMIMO::MsgConfigurePlutoSDRMIMO, Message)
MESSAGE_CLASS_DEFINITION(PlutoSDRMIMO::MsgStartStop, Message)

PlutoSDRMIMO::PlutoSDRMIMO(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
    m_sourceThread(nullptr),
    m_sinkThread(nullptr),
	m_deviceDescription("PlutoSDRMIMO"),
	m_runningRx(false),
    m_runningTx(false),
    m_plutoRxBuffer(nullptr),
    m_plutoTxBuffer(nullptr),
    m_plutoParams(nullptr),
    m_open(false),
    m_nbRx(0),
    m_nbTx(0)
{
    m_mimoType = MIMOHalfSynchronous;
    m_sampleMIFifo.init(2, 16 * PlutoSDRMIMOSettings::m_plutoSDRBlockSizeSamples);
    m_sampleMOFifo.init(2, 16 * PlutoSDRMIMOSettings::m_plutoSDRBlockSizeSamples);
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &PlutoSDRMIMO::networkManagerFinished
    );

    m_open = openDevice();

    if (m_plutoParams)
    {
        m_nbRx = m_plutoParams->getBox()->getNbRx();
        m_deviceAPI->setNbSourceStreams(m_nbRx);
        m_nbTx = m_plutoParams->getBox()->getNbTx();
        m_deviceAPI->setNbSinkStreams(m_nbTx);
        qDebug("PlutoSDRMIMO::PlutoSDRMIMO: m_nbRx: %d m_nbTx: %d", m_nbRx, m_nbTx);
    }
}

PlutoSDRMIMO::~PlutoSDRMIMO()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &PlutoSDRMIMO::networkManagerFinished
    );
    delete m_networkManager;
    closeDevice();
}

void PlutoSDRMIMO::destroy()
{
    delete this;
}

bool PlutoSDRMIMO::openDevice()
{
    qDebug("PlutoSDRMIMO::openDevice: open device here");
    m_plutoParams = new DevicePlutoSDRParams();

    if (m_deviceAPI->getHardwareUserArguments().size() != 0)
    {
        QStringList kv = m_deviceAPI->getHardwareUserArguments().split('='); // expecting "uri=xxx"

        if (kv.size() > 1)
        {
            if (kv.at(0) == "uri")
            {
                if (!m_plutoParams->openURI(kv.at(1).toStdString()))
                {
                    qCritical("PlutoSDRMIMO::openDevice: open network device uri=%s failed", qPrintable(kv.at(1)));
                    return false;
                }
            }
            else
            {
                qCritical("PlutoSDRMIMO::openDevice: unexpected user parameter key %s", qPrintable(kv.at(0)));
                return false;
            }
        }
        else
        {
            qCritical("PlutoSDRMIMO::openDevice: unexpected user arguments %s", qPrintable(m_deviceAPI->getHardwareUserArguments()));
            return false;
        }
    }
    else
    {
        char serial[256];
        strcpy(serial, qPrintable(m_deviceAPI->getSamplingDeviceSerial()));

        if (m_plutoParams->open(serial))
        {
            qDebug("PlutoSDRMIMO::openDevice: device serial %s opened", serial);
        }
        else
        {
            qCritical("PlutoSDRMIMO::openDevice: open serial %s failed", serial);
            return false;
        }
    }

    return true;
}

void PlutoSDRMIMO::closeDevice()
{
    if (m_plutoParams == nullptr) { // was never open
        return;
    }

    if (m_runningRx) {
        stopRx();
    }

    if (m_runningTx) {
        stopTx();
    }

    m_plutoParams->close();
    delete m_plutoParams;
    m_plutoParams = nullptr;
    m_open = false;
}

void PlutoSDRMIMO::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

bool PlutoSDRMIMO::startRx()
{
    qDebug("PlutoSDRMIMO::startRx");

    if (!m_open)
    {
        qCritical("PlutoSDRMIMO::startRx: device was not opened");
        return false;
    }

	QMutexLocker mutexLocker(&m_mutex);

    if (m_runningRx) {
        stopRx();
    }

    m_sourceThread = new PlutoSDRMIThread(m_plutoParams->getBox());
    m_sampleMIFifo.reset();
    m_sourceThread->setFifo(&m_sampleMIFifo);
    m_sourceThread->setFcPos(m_settings.m_fcPosRx);
    m_sourceThread->setLog2Decimation(m_settings.m_log2Decim);
    m_sourceThread->setIQOrder(m_settings.m_iqOrder);

    if (m_nbRx > 0) {
        m_plutoParams->getBox()->openRx();
    }

    if (m_nbRx > 1) {
        m_plutoParams->getBox()->openSecondRx();
    }

    m_plutoRxBuffer = m_plutoParams->getBox()->createRxBuffer(PlutoSDRMIMOSettings::m_plutoSDRBlockSizeSamples, false);
	m_sourceThread->startWork();
	mutexLocker.unlock();
	m_runningRx = true;

    return true;
}

bool PlutoSDRMIMO::startTx()
{
    qDebug("PlutoSDRMIMO::startTx");

    if (!m_open)
    {
        qCritical("PlutoSDRMIMO::startTx: device was not opened");
        return false;
    }

	QMutexLocker mutexLocker(&m_mutex);

    if (m_runningTx) {
        stopTx();
    }

    m_sinkThread = new PlutoSDRMOThread(m_plutoParams->getBox());
    m_sampleMOFifo.reset();
    m_sinkThread->setFifo(&m_sampleMOFifo);
    m_sinkThread->setFcPos(m_settings.m_fcPosTx);
    m_sinkThread->setLog2Interpolation(m_settings.m_log2Interp);

    if (m_nbTx > 0) {
        m_plutoParams->getBox()->openTx();
    }

    if (m_nbTx > 1) {
        m_plutoParams->getBox()->openSecondTx();
    }

    m_plutoTxBuffer = m_plutoParams->getBox()->createTxBuffer(PlutoSDRMIMOSettings::m_plutoSDRBlockSizeSamples, false);
	m_sinkThread->startWork();
	mutexLocker.unlock();
	m_runningTx = true;

    return true;
}

void PlutoSDRMIMO::stopRx()
{
    qDebug("PlutoSDRMIMO::stopRx");

    if (!m_sourceThread) {
        return;
    }

	QMutexLocker mutexLocker(&m_mutex);

    m_sourceThread->stopWork();
    delete m_sourceThread;
    m_sourceThread = nullptr;
    m_runningRx = false;

    if (m_nbRx > 1) {
        m_plutoParams->getBox()->closeSecondRx();
    }

    if (m_nbRx > 0) {
        m_plutoParams->getBox()->closeRx();
    }

    m_plutoParams->getBox()->deleteRxBuffer();
    m_plutoTxBuffer = nullptr;
}

void PlutoSDRMIMO::stopTx()
{
    qDebug("PlutoSDRMIMO::stopTx");

    if (!m_sinkThread) {
        return;
    }

	QMutexLocker mutexLocker(&m_mutex);

    m_sinkThread->stopWork();
    delete m_sinkThread;
    m_sinkThread = nullptr;
    m_runningTx = false;

    if (m_nbTx > 1) {
        m_plutoParams->getBox()->closeSecondTx();
    }

    if (m_nbTx > 0) {
        m_plutoParams->getBox()->closeTx();
    }

    m_plutoParams->getBox()->deleteTxBuffer();
    m_plutoRxBuffer = nullptr;
}

QByteArray PlutoSDRMIMO::serialize() const
{
    return m_settings.serialize();
}

bool PlutoSDRMIMO::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigurePlutoSDRMIMO* message = MsgConfigurePlutoSDRMIMO::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigurePlutoSDRMIMO* messageToGUI = MsgConfigurePlutoSDRMIMO::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& PlutoSDRMIMO::getDeviceDescription() const
{
	return m_deviceDescription;
}

int PlutoSDRMIMO::getSourceSampleRate(int index) const
{
    (void) index;
    int rate = m_settings.m_devSampleRate;
    return (rate / (1<<m_settings.m_log2Decim));
}

int PlutoSDRMIMO::getSinkSampleRate(int index) const
{
    (void) index;
    int rate = m_settings.m_devSampleRate;
    return (rate / (1<<m_settings.m_log2Interp));
}

quint64 PlutoSDRMIMO::getSourceCenterFrequency(int index) const
{
    (void) index;
    return m_settings.m_rxCenterFrequency;
}

void PlutoSDRMIMO::setSourceCenterFrequency(qint64 centerFrequency, int index)
{
    (void) index;
    PlutoSDRMIMOSettings settings = m_settings;
    settings.m_rxCenterFrequency = centerFrequency;

    MsgConfigurePlutoSDRMIMO* message = MsgConfigurePlutoSDRMIMO::create(settings, QList<QString>{"rxCenterFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigurePlutoSDRMIMO* messageToGUI = MsgConfigurePlutoSDRMIMO::create(settings, QList<QString>{"rxCenterFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

quint64 PlutoSDRMIMO::getSinkCenterFrequency(int index) const
{
    (void) index;
    return m_settings.m_txCenterFrequency;
}

void PlutoSDRMIMO::setSinkCenterFrequency(qint64 centerFrequency, int index)
{
    (void) index;
    PlutoSDRMIMOSettings settings = m_settings;
    settings.m_txCenterFrequency = centerFrequency;

    MsgConfigurePlutoSDRMIMO* message = MsgConfigurePlutoSDRMIMO::create(settings, QList<QString>{"txCenterFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigurePlutoSDRMIMO* messageToGUI = MsgConfigurePlutoSDRMIMO::create(settings, QList<QString>{"txCenterFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool PlutoSDRMIMO::handleMessage(const Message& message)
{
    if (MsgConfigurePlutoSDRMIMO::match(message))
    {
        MsgConfigurePlutoSDRMIMO& conf = (MsgConfigurePlutoSDRMIMO&) message;
        qDebug() << "PlutoSDRMIMO::handleMessage: MsgConfigurePlutoSDRMIMO";

        bool success = applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());

        if (!success) {
            qDebug("PlutoSDRMIMO::handleMessage: config error");
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "PlutoSDRMIMO::handleMessage: "
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

bool PlutoSDRMIMO::applySettings(const PlutoSDRMIMOSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "PlutoSDRMIMO::applySettings: force:" << force << settings.getDebugString(settingsKeys, force);

    bool forwardChangeRxDSP = false;
    bool forwardChangeTxDSP = false;
    DevicePlutoSDRBox *plutoBox = m_plutoParams ? m_plutoParams->getBox() : nullptr;

    // Rx

    if (settingsKeys.contains("dcBlock") ||
        settingsKeys.contains("iqCorrection") || force)
    {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, m_settings.m_iqCorrection);
    }

    // Change affecting device sample rate chain and other buddies
    if (settingsKeys.contains("devSampleRate") ||
        settingsKeys.contains("lpfRxFIREnable") ||
        settingsKeys.contains("lpfRxFIRlog2Decim") ||
        settingsKeys.contains("lpfRxFIRBW") ||
        settingsKeys.contains("m_lpfRxFIRGain") || force)
    {
        if (plutoBox)
        {
            plutoBox->setFIR(
                settings.m_devSampleRate,
                settings.m_lpfRxFIRlog2Decim,
                DevicePlutoSDRBox::USE_RX,
                settings.m_lpfRxFIRBW,
                settings.m_lpfRxFIRGain
            );
            plutoBox->setFIREnable(settings.m_lpfRxFIREnable);   // eventually enable/disable FIR
            plutoBox->setSampleRate(settings.m_devSampleRate); // and set end point sample rate

            plutoBox->getRxSampleRates(m_rxDeviceSampleRates); // pick up possible new rates
            qDebug() << "PlutoSDRMIMO::applySettings: Rx: BBPLL(Hz): " << m_rxDeviceSampleRates.m_bbRateHz
                    << " ADC: " << m_rxDeviceSampleRates.m_addaConnvRate
                    << " -HB3-> " << m_rxDeviceSampleRates.m_hb3Rate
                    << " -HB2-> " << m_rxDeviceSampleRates.m_hb2Rate
                    << " -HB1-> " << m_rxDeviceSampleRates.m_hb1Rate
                    << " -FIR-> " << m_rxDeviceSampleRates.m_firRate;
        }

        forwardChangeRxDSP = (m_settings.m_devSampleRate != settings.m_devSampleRate) || force;
    }

    if (settingsKeys.contains("log2Decim") || force)
    {
        if (m_sourceThread)
        {
            m_sourceThread->setLog2Decimation(settings.m_log2Decim);
            qDebug() << "PlutoSDRMIMO::applySettings: set soft decimation to " << (1<<settings.m_log2Decim);
        }

        forwardChangeRxDSP = true;
    }

    if (settingsKeys.contains("iqOrder") || force)
    {
        if (m_sourceThread) {
            m_sourceThread->setIQOrder(settings.m_iqOrder);
        }
    }

    if (settingsKeys.contains("LOppmTenths") || force)
    {
        if (plutoBox) {
            plutoBox->setLOPPMTenths(settings.m_LOppmTenths);
        }
    }

    std::vector<std::string> params;
    bool paramsToSet = false;

    if (settingsKeys.contains("rxCenterFrequency")
        || settingsKeys.contains("fcPosRx")
        || settingsKeys.contains("log2Decim")
        || settingsKeys.contains("devSampleRate")
        || settingsKeys.contains("rxTransverterMode")
        || settingsKeys.contains("rxTransverterDeltaFrequency") || force)
    {
        qint64 deviceCenterFrequency = DeviceSampleSource::calculateDeviceCenterFrequency(
                settings.m_rxCenterFrequency,
                settings.m_rxTransverterDeltaFrequency,
                settings.m_log2Decim,
                (DeviceSampleSource::fcPos_t) settings.m_fcPosRx,
                settings.m_devSampleRate,
                DeviceSampleSource::FrequencyShiftScheme::FSHIFT_STD,
                settings.m_rxTransverterMode);

        params.push_back(QString(tr("out_altvoltage0_RX_LO_frequency=%1").arg(deviceCenterFrequency)).toStdString());
        paramsToSet = true;
        forwardChangeRxDSP = true;

        if (settingsKeys.contains("fcPosRx") || force)
        {
            if (m_sourceThread)
            {
                m_sourceThread->setFcPos(settings.m_fcPosRx);
                qDebug() << "PlutoSDRMIMO::applySettings: set fcPos to " << settings.m_fcPosRx;
            }
        }
    }

    if (settingsKeys.contains("lpfBWRx") || force)
    {
        params.push_back(QString(tr("in_voltage_rf_bandwidth=%1").arg(settings.m_lpfBWRx)).toStdString());
        paramsToSet = true;
    }

    if (settingsKeys.contains("rx0AntennaPath") || force)
    {
        QString rfPortStr;
        PlutoSDRMIMOSettings::translateRFPathRx(settings.m_rx0AntennaPath, rfPortStr);
        params.push_back(QString(tr("in_voltage0_rf_port_select=%1").arg(rfPortStr)).toStdString());
        paramsToSet = true;
    }

    if (settingsKeys.contains("rx1AntennaPath") || force)
    {
        QString rfPortStr;
        PlutoSDRMIMOSettings::translateRFPathRx(settings.m_rx1AntennaPath, rfPortStr);
        params.push_back(QString(tr("in_voltage1_rf_port_select=%1").arg(rfPortStr)).toStdString());
        paramsToSet = true;
    }

    if (settingsKeys.contains("rx0GainMode") || force)
    {
        QString gainModeStr;
        PlutoSDRMIMOSettings::translateGainMode(settings.m_rx0GainMode, gainModeStr);
        params.push_back(QString(tr("in_voltage0_gain_control_mode=%1").arg(gainModeStr)).toStdString());
        paramsToSet = true;
    }

    if (settingsKeys.contains("rx1GainMode") || force)
    {
        QString gainModeStr;
        PlutoSDRMIMOSettings::translateGainMode(settings.m_rx1GainMode, gainModeStr);
        params.push_back(QString(tr("in_voltage1_gain_control_mode=%1").arg(gainModeStr)).toStdString());
        paramsToSet = true;
    }

    if (settingsKeys.contains("rx0Gain") || force)
    {
        params.push_back(QString(tr("in_voltage0_hardwaregain=%1").arg(settings.m_rx0Gain)).toStdString());
        paramsToSet = true;
    }

    if (settingsKeys.contains("rx1Gain") || force)
    {
        params.push_back(QString(tr("in_voltage1_hardwaregain=%1").arg(settings.m_rx1Gain)).toStdString());
        paramsToSet = true;
    }

    if (settingsKeys.contains("hwBBDCBlock") || force)
    {
        params.push_back(QString(tr("in_voltage_bb_dc_offset_tracking_en=%1").arg(settings.m_hwBBDCBlock ? 1 : 0)).toStdString());
        paramsToSet = true;
    }

    if (settingsKeys.contains("hwRFDCBlock") || force)
    {
        params.push_back(QString(tr("in_voltage_rf_dc_offset_tracking_en=%1").arg(settings.m_hwRFDCBlock ? 1 : 0)).toStdString());
        paramsToSet = true;
    }

    if (settingsKeys.contains("hwIQCorrection") || force)
    {
        params.push_back(QString(tr("in_voltage_quadrature_tracking_en=%1").arg(settings.m_hwIQCorrection ? 1 : 0)).toStdString());
        paramsToSet = true;
    }

    // Tx

    // Change affecting device sample rate chain and other buddies
    if (settingsKeys.contains("devSampleRate") ||
        settingsKeys.contains("lpfTxFIREnable") ||
        settingsKeys.contains("lpfTxFIRlog2Interp") ||
        settingsKeys.contains("lpfTxFIRBW") ||
        settingsKeys.contains("lpfTxFIRGain") || force)
    {
        plutoBox->setFIR(
            settings.m_devSampleRate,
            settings.m_lpfTxFIRlog2Interp,
            DevicePlutoSDRBox::USE_TX,
            settings.m_lpfTxFIRBW,
            settings.m_lpfTxFIRGain
        );
        plutoBox->setFIREnable(settings.m_lpfTxFIREnable);   // eventually enable/disable FIR
        plutoBox->setSampleRate(settings.m_devSampleRate); // and set end point sample rate

        plutoBox->getTxSampleRates(m_txDeviceSampleRates); // pick up possible new rates
        qDebug() << "PlutoSDRMIMO::applySettings: Tx: BBPLL(Hz): " << m_txDeviceSampleRates.m_bbRateHz
                 << " DAC: " << m_txDeviceSampleRates.m_addaConnvRate
                 << " <-HB3- " << m_txDeviceSampleRates.m_hb3Rate
                 << " <-HB2- " << m_txDeviceSampleRates.m_hb2Rate
                 << " <-HB1- " << m_txDeviceSampleRates.m_hb1Rate
                 << " <-FIR- " << m_txDeviceSampleRates.m_firRate;

        forwardChangeTxDSP = (m_settings.m_devSampleRate != settings.m_devSampleRate) || force;
    }

    if (settingsKeys.contains("log2Interp") || force)
    {
        if (m_sinkThread != 0)
        {
            m_sinkThread->setLog2Interpolation(settings.m_log2Interp);
            qDebug() << "PlutoSDRMIMO::applySettings: set soft interpolation in thread to " << (1<<settings.m_log2Interp);
        }

        forwardChangeTxDSP = true;
    }

    if (force || settingsKeys.contains("txCenterFrequency")
        || settingsKeys.contains("log2Interp")
        || settingsKeys.contains("fcPosTx")
        || settingsKeys.contains("devSampleRate")
        || settingsKeys.contains("txTransverterMode")
        || settingsKeys.contains("txTransverterDeltaFrequency"))
    {
        qint64 deviceCenterFrequency = DeviceSampleSink::calculateDeviceCenterFrequency(
            settings.m_txCenterFrequency,
            settings.m_txTransverterDeltaFrequency,
            settings.m_log2Interp,
            (DeviceSampleSink::fcPos_t) settings.m_fcPosTx,
            settings.m_devSampleRate,
            settings.m_txTransverterMode
        );

        params.push_back(QString(tr("out_altvoltage1_TX_LO_frequency=%1").arg(deviceCenterFrequency)).toStdString());
        paramsToSet = true;
        forwardChangeTxDSP = true;

        qDebug() << "PlutoSDRMIMO::applySettings: Tx center freq: " << settings.m_txCenterFrequency << " Hz"
                << " device center freq: " << deviceCenterFrequency << " Hz";

    }

    if (settingsKeys.contains("lpfBWTx") || force)
    {
        params.push_back(QString(tr("out_voltage_rf_bandwidth=%1").arg(settings.m_lpfBWTx)).toStdString());
        paramsToSet = true;
    }

    if (settingsKeys.contains("tx0AntennaPath") || force)
    {
        QString rfPortStr;
        PlutoSDRMIMOSettings::translateRFPathTx(settings.m_tx0AntennaPath, rfPortStr);
        params.push_back(QString(tr("out_voltage0_rf_port_select=%1").arg(rfPortStr)).toStdString());
        paramsToSet = true;
    }

    if (settingsKeys.contains("tx1AntennaPath") || force)
    {
        QString rfPortStr;
        PlutoSDRMIMOSettings::translateRFPathTx(settings.m_tx1AntennaPath, rfPortStr);
        params.push_back(QString(tr("out_voltage1_rf_port_select=%1").arg(rfPortStr)).toStdString());
        paramsToSet = true;
    }

    if (settingsKeys.contains("tx0Att") || force)
    {
        float attF = settings.m_tx0Att * 0.25f;
        params.push_back(QString(tr("out_voltage0_hardwaregain=%1").arg(attF)).toStdString());
        paramsToSet = true;
    }

    if (settingsKeys.contains("tx1Att") || force)
    {
        float attF = settings.m_tx1Att * 0.25f;
        params.push_back(QString(tr("out_voltage1_hardwaregain=%1").arg(attF)).toStdString());
        paramsToSet = true;
    }

    if (paramsToSet) {
        plutoBox->set_params(DevicePlutoSDRBox::DEVICE_PHY, params);
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

    return true;
}

int PlutoSDRMIMO::webapiSettingsGet(
    SWGSDRangel::SWGDeviceSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setPlutoSdrMimoSettings(new SWGSDRangel::SWGPlutoSdrMIMOSettings());
    response.getPlutoSdrMimoSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int PlutoSDRMIMO::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    PlutoSDRMIMOSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigurePlutoSDRMIMO *msg = MsgConfigurePlutoSDRMIMO::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigurePlutoSDRMIMO *msgToGUI = MsgConfigurePlutoSDRMIMO::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void PlutoSDRMIMO::webapiUpdateDeviceSettings(
        PlutoSDRMIMOSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getPlutoSdrMimoSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("LOppmTenths")) {
        settings.m_LOppmTenths = response.getPlutoSdrMimoSettings()->getLOppmTenths();
    }
    if (deviceSettingsKeys.contains("rxCenterFrequency")) {
        settings.m_rxCenterFrequency = response.getPlutoSdrMimoSettings()->getRxCenterFrequency();
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getPlutoSdrMimoSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getPlutoSdrMimoSettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("hwBBDCBlock")) {
        settings.m_hwBBDCBlock = response.getPlutoSdrMimoSettings()->getHwBbdcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("hwRFDCBlock")) {
        settings.m_hwRFDCBlock = response.getPlutoSdrMimoSettings()->getHwRfdcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("hwIQCorrection")) {
        settings.m_hwIQCorrection = response.getPlutoSdrMimoSettings()->getHwIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("fcPosRx")) {
        settings.m_fcPosRx = static_cast<PlutoSDRMIMOSettings::fcPos_t>(response.getPlutoSdrMimoSettings()->getFcPosRx());
    }
    if (deviceSettingsKeys.contains("rxTransverterMode")) {
        settings.m_rxTransverterMode = response.getPlutoSdrMimoSettings()->getRxTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("rxTransverterDeltaFrequency")) {
        settings.m_rxTransverterDeltaFrequency = response.getPlutoSdrMimoSettings()->getRxTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("iqOrder")) {
        settings.m_iqOrder = response.getPlutoSdrMimoSettings()->getIqOrder() != 0;
    }
    if (deviceSettingsKeys.contains("lpfBWRx")) {
        settings.m_lpfBWRx = response.getPlutoSdrMimoSettings()->getLpfBwRx();
    }
    if (deviceSettingsKeys.contains("lpfRxFIREnable")) {
        settings.m_lpfRxFIREnable = response.getPlutoSdrMimoSettings()->getLpfRxFirEnable() != 0;
    }
    if (deviceSettingsKeys.contains("lpfRxFIRBW")) {
        settings.m_lpfRxFIRBW = response.getPlutoSdrMimoSettings()->getLpfRxFirbw();
    }
    if (deviceSettingsKeys.contains("lpfRxFIRlog2Decim")) {
        settings.m_lpfRxFIRlog2Decim = response.getPlutoSdrMimoSettings()->getLpfRxFiRlog2Decim();
    }
    if (deviceSettingsKeys.contains("lpfRxFIRGain")) {
        settings.m_lpfRxFIRGain = response.getPlutoSdrMimoSettings()->getLpfRxFirGain();
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getPlutoSdrMimoSettings()->getLog2Decim();
    }
    if (deviceSettingsKeys.contains("rx0Gain")) {
        settings.m_rx0Gain = response.getPlutoSdrMimoSettings()->getRx0Gain();
    }
    if (deviceSettingsKeys.contains("rx0GainMode")) {
        settings.m_rx0GainMode = static_cast<PlutoSDRMIMOSettings::GainMode>(response.getPlutoSdrMimoSettings()->getRx0GainMode());
    }
    if (deviceSettingsKeys.contains("rx0AntennaPath")) {
        settings.m_rx0AntennaPath = static_cast<PlutoSDRMIMOSettings::RFPathRx>(response.getPlutoSdrMimoSettings()->getRx0AntennaPath());
    }
    if (deviceSettingsKeys.contains("rx1Gain")) {
        settings.m_rx1Gain = response.getPlutoSdrMimoSettings()->getRx1Gain();
    }
    if (deviceSettingsKeys.contains("rx1GainMode")) {
        settings.m_rx1GainMode = static_cast<PlutoSDRMIMOSettings::GainMode>(response.getPlutoSdrMimoSettings()->getRx1GainMode());
    }
    if (deviceSettingsKeys.contains("rx1AntennaPath")) {
        settings.m_rx1AntennaPath = static_cast<PlutoSDRMIMOSettings::RFPathRx>(response.getPlutoSdrMimoSettings()->getRx1AntennaPath());
    }
    if (deviceSettingsKeys.contains("txCenterFrequency")) {
        settings.m_txCenterFrequency = response.getPlutoSdrMimoSettings()->getTxCenterFrequency();
    }
    if (deviceSettingsKeys.contains("fcPosTx")) {
        settings.m_fcPosTx = static_cast<PlutoSDRMIMOSettings::fcPos_t>(response.getPlutoSdrMimoSettings()->getFcPosTx());
    }
    if (deviceSettingsKeys.contains("txTransverterMode")) {
        settings.m_txTransverterMode = response.getPlutoSdrMimoSettings()->getTxTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("txTransverterDeltaFrequency")) {
        settings.m_txTransverterDeltaFrequency = response.getPlutoSdrMimoSettings()->getTxTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("lpfBWTx")) {
        settings.m_lpfBWTx = response.getPlutoSdrMimoSettings()->getLpfBwTx();
    }
    if (deviceSettingsKeys.contains("lpfTxFIREnable")) {
        settings.m_lpfTxFIREnable = response.getPlutoSdrMimoSettings()->getLpfTxFirEnable() != 0;
    }
    if (deviceSettingsKeys.contains("lpfTxFIRBW")) {
        settings.m_lpfTxFIRBW = response.getPlutoSdrMimoSettings()->getLpfTxFirbw();
    }
    if (deviceSettingsKeys.contains("lpfTxFIRlog2Interp")) {
        settings.m_lpfTxFIRlog2Interp = response.getPlutoSdrMimoSettings()->getLpfTxFiRlog2Interp();
    }
    if (deviceSettingsKeys.contains("lpfTxFIRGain")) {
        settings.m_lpfTxFIRGain = response.getPlutoSdrMimoSettings()->getLpfTxFirGain();
    }
    if (deviceSettingsKeys.contains("log2Interp")) {
        settings.m_log2Interp = response.getPlutoSdrMimoSettings()->getLog2Interp();
    }
    if (deviceSettingsKeys.contains("tx0Att")) {
        settings.m_tx0Att = response.getPlutoSdrMimoSettings()->getTx0Att();
    }
    if (deviceSettingsKeys.contains("tx0AntennaPath")) {
        settings.m_tx0AntennaPath = static_cast<PlutoSDRMIMOSettings::RFPathTx>(response.getPlutoSdrMimoSettings()->getTx0AntennaPath());
    }
    if (deviceSettingsKeys.contains("tx1Att")) {
        settings.m_tx1Att = response.getPlutoSdrMimoSettings()->getTx1Att();
    }
    if (deviceSettingsKeys.contains("tx1AntennaPath")) {
        settings.m_tx1AntennaPath = static_cast<PlutoSDRMIMOSettings::RFPathTx>(response.getPlutoSdrMimoSettings()->getTx1AntennaPath());
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getPlutoSdrMimoSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getPlutoSdrMimoSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getPlutoSdrMimoSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getPlutoSdrMimoSettings()->getReverseApiDeviceIndex();
    }
}

void PlutoSDRMIMO::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const PlutoSDRMIMOSettings& settings)
{
    response.getPlutoSdrMimoSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getPlutoSdrMimoSettings()->setLOppmTenths(settings.m_LOppmTenths);
    response.getPlutoSdrMimoSettings()->setRxCenterFrequency(settings.m_rxCenterFrequency);
    response.getPlutoSdrMimoSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getPlutoSdrMimoSettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    response.getPlutoSdrMimoSettings()->setHwBbdcBlock(settings.m_hwBBDCBlock ? 1 : 0);
    response.getPlutoSdrMimoSettings()->setHwRfdcBlock(settings.m_hwRFDCBlock ? 1 : 0);
    response.getPlutoSdrMimoSettings()->setHwIqCorrection(settings.m_hwIQCorrection ? 1 : 0);
    response.getPlutoSdrMimoSettings()->setFcPosRx((int) settings.m_fcPosRx);
    response.getPlutoSdrMimoSettings()->setRxTransverterMode(settings.m_rxTransverterMode ? 1 : 0);
    response.getPlutoSdrMimoSettings()->setRxTransverterDeltaFrequency(settings.m_rxTransverterDeltaFrequency);
    response.getPlutoSdrMimoSettings()->setIqOrder(settings.m_iqOrder ? 1 : 0);
    response.getPlutoSdrMimoSettings()->setLpfBwRx(settings.m_lpfBWRx);
    response.getPlutoSdrMimoSettings()->setLpfRxFirEnable(settings.m_lpfRxFIREnable ? 1 : 0);
    response.getPlutoSdrMimoSettings()->setLpfRxFirbw(settings.m_lpfRxFIRBW);
    response.getPlutoSdrMimoSettings()->setLpfRxFiRlog2Decim(settings.m_lpfRxFIRlog2Decim);
    response.getPlutoSdrMimoSettings()->setLpfRxFirGain(settings.m_lpfRxFIRGain);
    response.getPlutoSdrMimoSettings()->setLog2Decim(settings.m_log2Decim);
    response.getPlutoSdrMimoSettings()->setRx0Gain(settings.m_rx0Gain);
    response.getPlutoSdrMimoSettings()->setRx0GainMode((int) settings.m_rx0GainMode);
    response.getPlutoSdrMimoSettings()->setRx0AntennaPath((int) settings.m_rx0AntennaPath);
    response.getPlutoSdrMimoSettings()->setRx1Gain(settings.m_rx1Gain);
    response.getPlutoSdrMimoSettings()->setRx1GainMode((int) settings.m_rx1GainMode);
    response.getPlutoSdrMimoSettings()->setRx1AntennaPath((int) settings.m_rx1AntennaPath);
    response.getPlutoSdrMimoSettings()->setTxCenterFrequency(settings.m_txCenterFrequency);
    response.getPlutoSdrMimoSettings()->setFcPosTx((int) settings.m_fcPosTx);
    response.getPlutoSdrMimoSettings()->setTxTransverterMode(settings.m_txTransverterMode ? 1 : 0);
    response.getPlutoSdrMimoSettings()->setTxTransverterDeltaFrequency(settings.m_txTransverterDeltaFrequency);
    response.getPlutoSdrMimoSettings()->setLpfBwTx(settings.m_lpfBWTx);
    response.getPlutoSdrMimoSettings()->setLpfTxFirEnable(settings.m_lpfTxFIREnable ? 1 : 0);
    response.getPlutoSdrMimoSettings()->setLpfTxFirbw(settings.m_lpfTxFIRBW);
    response.getPlutoSdrMimoSettings()->setLpfTxFiRlog2Interp(settings.m_lpfTxFIRlog2Interp);
    response.getPlutoSdrMimoSettings()->setLpfTxFirGain(settings.m_lpfTxFIRGain);
    response.getPlutoSdrMimoSettings()->setLog2Interp(settings.m_log2Interp);
    response.getPlutoSdrMimoSettings()->setTx0Att(settings.m_tx0Att);
    response.getPlutoSdrMimoSettings()->setTx0AntennaPath((int) settings.m_tx0AntennaPath);
    response.getPlutoSdrMimoSettings()->setTx1Att(settings.m_tx1Att);
    response.getPlutoSdrMimoSettings()->setTx1AntennaPath((int) settings.m_tx1AntennaPath);
    response.getPlutoSdrMimoSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getPlutoSdrMimoSettings()->getReverseApiAddress()) {
        *response.getPlutoSdrMimoSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getPlutoSdrMimoSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getPlutoSdrMimoSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getPlutoSdrMimoSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

int PlutoSDRMIMO::webapiRunGet(
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

int PlutoSDRMIMO::webapiRun(
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

void PlutoSDRMIMO::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const PlutoSDRMIMOSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(2); // MIMO
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("PlutoSDR"));
    swgDeviceSettings->setPlutoSdrMimoSettings(new SWGSDRangel::SWGPlutoSdrMIMOSettings());
    SWGSDRangel::SWGPlutoSdrMIMOSettings *swgPlutoSDRMIMOSettings = swgDeviceSettings->getPlutoSdrMimoSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("devSampleRate") || force) {
        swgPlutoSDRMIMOSettings->setDevSampleRate(settings.m_devSampleRate);
    }
    if (deviceSettingsKeys.contains("LOppmTenths")) {
        swgPlutoSDRMIMOSettings->setLOppmTenths(settings.m_LOppmTenths);
    }
    if (deviceSettingsKeys.contains("rxCenterFrequency")) {
        swgPlutoSDRMIMOSettings->setRxCenterFrequency(settings.m_rxCenterFrequency);
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        swgPlutoSDRMIMOSettings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        swgPlutoSDRMIMOSettings->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("hwBBDCBlock")) {
        swgPlutoSDRMIMOSettings->setHwBbdcBlock(settings.m_hwBBDCBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("hwRFDCBlock")) {
        swgPlutoSDRMIMOSettings->setHwRfdcBlock(settings.m_hwRFDCBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("hwIQCorrection")) {
        swgPlutoSDRMIMOSettings->setHwIqCorrection(settings.m_hwIQCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("fcPosRx")) {
        swgPlutoSDRMIMOSettings->setFcPosRx((int) settings.m_fcPosRx);
    }
    if (deviceSettingsKeys.contains("rxTransverterMode")) {
        swgPlutoSDRMIMOSettings->setRxTransverterMode(settings.m_rxTransverterMode ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("rxTransverterDeltaFrequency")) {
        swgPlutoSDRMIMOSettings->setRxTransverterDeltaFrequency(settings.m_rxTransverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("iqOrder")) {
        swgPlutoSDRMIMOSettings->setIqOrder(settings.m_iqOrder ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("lpfBWRx")) {
        swgPlutoSDRMIMOSettings->setLpfBwRx(settings.m_lpfBWRx);
    }
    if (deviceSettingsKeys.contains("lpfRxFIREnable")) {
        swgPlutoSDRMIMOSettings->setLpfRxFirEnable(settings.m_lpfRxFIREnable ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("lpfRxFIRBW")) {
        swgPlutoSDRMIMOSettings->setLpfRxFirbw(settings.m_lpfRxFIRBW);
    }
    if (deviceSettingsKeys.contains("lpfRxFIRlog2Decim")) {
        swgPlutoSDRMIMOSettings->setLpfRxFiRlog2Decim(settings.m_lpfRxFIRlog2Decim);
    }
    if (deviceSettingsKeys.contains("lpfRxFIRGain")) {
        swgPlutoSDRMIMOSettings->setLpfRxFirGain(settings.m_lpfRxFIRGain);
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        swgPlutoSDRMIMOSettings->setLog2Decim(settings.m_log2Decim);
    }
    if (deviceSettingsKeys.contains("rx0Gain")) {
        swgPlutoSDRMIMOSettings->setRx0Gain(settings.m_rx0Gain);
    }
    if (deviceSettingsKeys.contains("rx0GainMode")) {
        swgPlutoSDRMIMOSettings->setRx0GainMode((int) settings.m_rx0GainMode);
    }
    if (deviceSettingsKeys.contains("rx0AntennaPath")) {
        swgPlutoSDRMIMOSettings->setRx0AntennaPath((int) settings.m_rx0AntennaPath);
    }
    if (deviceSettingsKeys.contains("rx1Gain")) {
        swgPlutoSDRMIMOSettings->setRx1Gain(settings.m_rx1Gain);
    }
    if (deviceSettingsKeys.contains("rx1GainMode")) {
        swgPlutoSDRMIMOSettings->setRx1GainMode((int) settings.m_rx1GainMode);
    }
    if (deviceSettingsKeys.contains("rx1AntennaPath")) {
        swgPlutoSDRMIMOSettings->setRx1AntennaPath((int) settings.m_rx1AntennaPath);
    }
    if (deviceSettingsKeys.contains("txCenterFrequency")) {
        swgPlutoSDRMIMOSettings->setTxCenterFrequency(settings.m_txCenterFrequency);
    }
    if (deviceSettingsKeys.contains("fcPosTx")) {
        swgPlutoSDRMIMOSettings->setFcPosTx((int) settings.m_fcPosTx);
    }
    if (deviceSettingsKeys.contains("txTransverterMode")) {
        swgPlutoSDRMIMOSettings->setTxTransverterMode(settings.m_txTransverterMode ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("txTransverterDeltaFrequency")) {
        swgPlutoSDRMIMOSettings->setTxTransverterDeltaFrequency(settings.m_txTransverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("lpfBWTx")) {
        swgPlutoSDRMIMOSettings->setLpfBwTx(settings.m_lpfBWTx);
    }
    if (deviceSettingsKeys.contains("lpfTxFIREnable")) {
        swgPlutoSDRMIMOSettings->setLpfTxFirEnable(settings.m_lpfTxFIREnable ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("lpfTxFIRBW")) {
        swgPlutoSDRMIMOSettings->setLpfTxFirbw(settings.m_lpfTxFIRBW);
    }
    if (deviceSettingsKeys.contains("lpfTxFIRlog2Interp")) {
        swgPlutoSDRMIMOSettings->setLpfTxFiRlog2Interp(settings.m_lpfTxFIRlog2Interp);
    }
    if (deviceSettingsKeys.contains("lpfTxFIRGain")) {
        swgPlutoSDRMIMOSettings->setLpfTxFirGain(settings.m_lpfTxFIRGain);
    }
    if (deviceSettingsKeys.contains("log2Interp")) {
        swgPlutoSDRMIMOSettings->setLog2Interp(settings.m_log2Interp);
    }
    if (deviceSettingsKeys.contains("tx0Att")) {
        swgPlutoSDRMIMOSettings->setTx0Att(settings.m_tx0Att);
    }
    if (deviceSettingsKeys.contains("tx0AntennaPath")) {
        swgPlutoSDRMIMOSettings->setTx0AntennaPath((int) settings.m_tx0AntennaPath);
    }
    if (deviceSettingsKeys.contains("tx1Att")) {
        swgPlutoSDRMIMOSettings->setTx1Att(settings.m_tx1Att);
    }
    if (deviceSettingsKeys.contains("tx1AntennaPath")) {
        swgPlutoSDRMIMOSettings->setTx1AntennaPath((int) settings.m_tx1AntennaPath);
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

void PlutoSDRMIMO::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(2); // MIMO
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("PlutoSDR"));

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

int PlutoSDRMIMO::webapiReportGet(SWGSDRangel::SWGDeviceReport& response, QString& errorMessage)
{
    (void) errorMessage;
    response.setPlutoSdrMimoReport(new SWGSDRangel::SWGPlutoSdrMIMOReport());
    response.getPlutoSdrMimoReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void PlutoSDRMIMO::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    response.getPlutoSdrMimoReport()->setAdcRate(getADCSampleRate());
    std::string rssiStr;
    getRxRSSI(rssiStr, 0);
    response.getPlutoSdrMimoReport()->setRssiRx0(new QString(rssiStr.c_str()));
    getRxRSSI(rssiStr, 1);
    response.getPlutoSdrMimoReport()->setRssiRx1(new QString(rssiStr.c_str()));
    int gainDB;
    getRxGain(gainDB, 0);
    response.getPlutoSdrMimoReport()->setRx0GainDb(gainDB);
    getRxGain(gainDB, 1);
    response.getPlutoSdrMimoReport()->setRx1GainDb(gainDB);
    response.getPlutoSdrMimoReport()->setDacRate(getDACSampleRate());
    getTxRSSI(rssiStr, 0);
    response.getPlutoSdrMimoReport()->setRssiTx0(new QString(rssiStr.c_str()));
    getTxRSSI(rssiStr, 1);
    response.getPlutoSdrMimoReport()->setRssiTx1(new QString(rssiStr.c_str()));
}

void PlutoSDRMIMO::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "PlutoSDRMIMO::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("PlutoSDRMIMO::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void PlutoSDRMIMO::getRxRSSI(std::string& rssiStr, int chan)
{
    if (!m_open)
    {
        qDebug("PlutoSDRMIMO::getRxRSSI: device not open");
        return;
    }

    if (m_plutoParams)
    {
        DevicePlutoSDRBox *plutoBox =  m_plutoParams->getBox();

        if (!plutoBox->getRxRSSI(rssiStr, chan)) {
            rssiStr = "xxx dB";
        }
    }
}

void PlutoSDRMIMO::getTxRSSI(std::string& rssiStr, int chan)
{
    if (!m_open)
    {
        qDebug("PlutoSDRMIMO::getTxRSSI: device not open");
        return;
    }

    if (m_plutoParams)
    {
        DevicePlutoSDRBox *plutoBox =  m_plutoParams->getBox();

        if (!plutoBox->getTxRSSI(rssiStr, chan)) {
            rssiStr = "xxx dB";
        }
    }

}

void PlutoSDRMIMO::getRxGain(int& gaindB, int chan)
{
    if (!m_open)
    {
        qDebug("PlutoSDRMIMO::getRxGain: device not open");
        return;
    }

    if (m_plutoParams)
    {
        DevicePlutoSDRBox *plutoBox =  m_plutoParams->getBox();

        if (!plutoBox->getRxGain(gaindB, chan)) {
            gaindB = 0;
        }
    }
}

void PlutoSDRMIMO::getbbLPRange(quint32& minLimit, quint32& maxLimit)
{
    if (!m_open)
    {
        qDebug("PlutoSDRMIMO::getbbLPRange: device not open");
        return;
    }

    if (m_plutoParams)
    {
        DevicePlutoSDRBox *plutoBox =  m_plutoParams->getBox();

        if (plutoBox)
        {
            uint32_t min, max;
            plutoBox->getbbLPRxRange(min, max);
            minLimit = min;
            maxLimit = max;
        }
    }
}

void PlutoSDRMIMO::getLORange(qint64& minLimit, qint64& maxLimit)
{
    if (!m_open)
    {
        qDebug("PlutoSDRMIMO::getLORange: device not open");
        return;
    }

    if (m_plutoParams)
    {
        DevicePlutoSDRBox *plutoBox = m_plutoParams->getBox();

        if (plutoBox)
        {
            uint64_t min, max;
            plutoBox->getRxLORange(min, max);
            minLimit = min;
            maxLimit = max;
        }
    }
}

bool PlutoSDRMIMO::fetchTemperature()
{
    if (!m_open)
    {
        qDebug("PlutoSDRMIMO::fetchTemperature: device not open");
        return false;
    }

    if (m_plutoParams)
    {
        DevicePlutoSDRBox *plutoBox =  m_plutoParams->getBox();

        if (plutoBox) {
            return plutoBox->fetchTemp();
        }
    }

    return false;
}

float PlutoSDRMIMO::getTemperature()
{
    if (!m_open)
    {
        qDebug("PlutoSDRMIMO::getTemperature: device not open");
        return 0.0;
    }

    if (m_plutoParams)
    {
        DevicePlutoSDRBox *plutoBox =  m_plutoParams->getBox();

        if (plutoBox) {
            return plutoBox->getTemp();
        }
    }

    return 0.0;
}
