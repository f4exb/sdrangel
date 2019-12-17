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

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/dspdevicemimoengine.h"
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"
#include "dsp/filerecord.h"
#include "bladerf2/devicebladerf2.h"

#include "bladerf2mithread.h"
#include "bladerf2mothread.h"
#include "bladerf2mimo.h"

MESSAGE_CLASS_DEFINITION(BladeRF2MIMO::MsgConfigureBladeRF2MIMO, Message)
MESSAGE_CLASS_DEFINITION(BladeRF2MIMO::MsgFileRecord, Message)
MESSAGE_CLASS_DEFINITION(BladeRF2MIMO::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(BladeRF2MIMO::MsgReportGainRange, Message)

BladeRF2MIMO::BladeRF2MIMO(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
    m_sourceThread(nullptr),
    m_sinkThread(nullptr),
	m_deviceDescription("BladeRF2MIMO"),
    m_rxElseTx(true),
	m_runningRx(false),
    m_runningTx(false),
    m_dev(nullptr),
    m_open(false)
{
    m_open = openDevice();

    if (m_dev)
    {
        const bladerf_gain_modes *modes = 0;
        int nbModes = m_dev->getGainModesRx(&modes);

        if (modes)
        {
            for (int i = 0; i < nbModes; i++) {
                m_rxGainModes.push_back(GainMode{QString(modes[i].name), modes[i].mode});
            }
        }
    }

    m_mimoType = MIMOHalfSynchronous;
    m_sampleMIFifo.init(2, 96000 * 4);
    m_deviceAPI->setNbSourceStreams(2);
    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

BladeRF2MIMO::~BladeRF2MIMO()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;

    if (m_runningRx) {
        stop();
    }

    std::vector<FileRecord*>::iterator it = m_fileSinks.begin();
    int istream = 0;

    for (; it != m_fileSinks.end(); ++it, istream++)
    {
        m_deviceAPI->removeAncillarySink(*it, istream);
        delete *it;
    }

    m_deviceAPI->removeLastSourceStream(); // Remove the last source stream data set in the engine
    m_deviceAPI->removeLastSourceStream(); // Remove the last source stream data set in the engine
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
        stop();
    }

    m_dev->close();
    delete m_dev;
    m_dev = nullptr;
}

void BladeRF2MIMO::init()
{
    m_fileSinks.push_back(new FileRecord(QString("test_0_%1.sdriq").arg(m_deviceAPI->getDeviceUID())));
    m_fileSinks.push_back(new FileRecord(QString("test_1_%1.sdriq").arg(m_deviceAPI->getDeviceUID())));
    m_deviceAPI->addAncillarySink(m_fileSinks[0], 0);
    m_deviceAPI->addAncillarySink(m_fileSinks[1], 1);

    applySettings(m_settings, true);
}

bool BladeRF2MIMO::start()
{
    if (!m_open)
    {
        qCritical("BladeRF2MIMO::start: device could not be opened");
        return false;
    }

    if (m_rxElseTx) {
        startRx();
    } else {
        startTx();
    }

    return true;
}

void BladeRF2MIMO::startRx()
{
    qDebug("BladeRF2MIMO::start");
	QMutexLocker mutexLocker(&m_mutex);

    if (m_runningRx) {
        stop();
    }

    m_sourceThread = new BladeRF2MIThread(m_dev->getDev());
    m_sourceThread->setFifo(&m_sampleMIFifo);
    m_sourceThread->setFcPos(m_settings.m_fcPos);
    m_sourceThread->setLog2Decimation(m_settings.m_log2Decim);

    for (int i = 0; i < 2; i++)
    {
        if (!m_dev->openRx(i)) {
            qCritical("BladeRF2MIMO::start: Rx channel %u cannot be enabled", i);
        }
    }

	m_sourceThread->startWork();

	mutexLocker.unlock();

	applySettings(m_settings, true);
	m_runningRx = true;
}

void BladeRF2MIMO::startTx()
{
    // TODO
}

void BladeRF2MIMO::stop()
{
    if (m_rxElseTx) {
        stopRx();
    } else {
        stopTx();
    }
}

void BladeRF2MIMO::stopRx()
{
    qDebug("BladeRF2MIMO::stop");

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
    // TODO
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

    MsgConfigureBladeRF2MIMO* message = MsgConfigureBladeRF2MIMO::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureBladeRF2MIMO* messageToGUI = MsgConfigureBladeRF2MIMO::create(m_settings, true);
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

    MsgConfigureBladeRF2MIMO* message = MsgConfigureBladeRF2MIMO::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureBladeRF2MIMO* messageToGUI = MsgConfigureBladeRF2MIMO::create(settings, false);
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

    MsgConfigureBladeRF2MIMO* message = MsgConfigureBladeRF2MIMO::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureBladeRF2MIMO* messageToGUI = MsgConfigureBladeRF2MIMO::create(settings, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool BladeRF2MIMO::handleMessage(const Message& message)
{
    if (MsgConfigureBladeRF2MIMO::match(message))
    {
        MsgConfigureBladeRF2MIMO& conf = (MsgConfigureBladeRF2MIMO&) message;
        qDebug() << "BladeRF2MIMO::handleMessage: MsgConfigureBladeRF2MIMO";

        bool success = applySettings(conf.getSettings(), conf.getForce());

        if (!success) {
            qDebug("BladeRF2MIMO::handleMessage: config error");
        }

        return true;
    }
    else if (MsgFileRecord::match(message))
    {
        MsgFileRecord& conf = (MsgFileRecord&) message;
        qDebug() << "BladeRF2MIMO::handleMessage: MsgFileRecord: " << conf.getStartStop();
        int istream = conf.getStreamIndex();

        if (conf.getStartStop())
        {
            if (m_settings.m_fileRecordName.size() != 0) {
                m_fileSinks[istream]->setFileName(m_settings.m_fileRecordName + "_0.sdriq");
            } else {
                m_fileSinks[istream]->genUniqueFileName(m_deviceAPI->getDeviceUID(), istream);
            }

            m_fileSinks[istream]->startRecording();
        }
        else
        {
            m_fileSinks[istream]->stopRecording();
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "BladeRF2MIMO::handleMessage: "
            << " " << (cmd.getRxElseTx() ? "Rx" : "Tx")
            << " MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        m_rxElseTx = cmd.getRxElseTx();

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

bool BladeRF2MIMO::applySettings(const BladeRF2MIMOSettings& settings, bool force)
{
    QList<QString> reverseAPIKeys;
    bool forwardChangeRxDSP = false;
    bool forwardChangeTxDSP = false;

    qDebug() << "BladeRF2MIMO::applySettings: common: "
        << " m_devSampleRate: " << settings.m_devSampleRate
        << " m_LOppmTenths: " << settings.m_LOppmTenths
        << " m_rxCenterFrequency: " << settings.m_rxCenterFrequency
        << " m_log2Decim: " << settings.m_log2Decim
        << " m_fcPos: " << settings.m_fcPos
        << " m_rxBandwidth: " << settings.m_rxBandwidth
        << " m_rx0GainMode: " << settings.m_rx0GainMode
        << " m_rx0GlobalGain: " << settings.m_rx0GlobalGain
        << " m_rx1GainMode: " << settings.m_rx1GainMode
        << " m_rx1GlobalGain: " << settings.m_rx1GlobalGain
        << " m_rxBiasTee: " << settings.m_rxBiasTee
        << " m_dcBlock: " << settings.m_dcBlock
        << " m_iqCorrection: " << settings.m_iqCorrection
        << " m_rxTransverterMode: " << settings.m_rxTransverterMode
        << " m_rxTransverterDeltaFrequency: " << settings.m_rxTransverterDeltaFrequency
        << " m_txCenterFrequency: " << settings.m_txCenterFrequency
        << " m_log2Interp: " << settings.m_log2Interp
        << " m_txBandwidth: " << settings.m_txBandwidth
        << " m_tx0GlobalGain: " << settings.m_tx0GlobalGain
        << " m_tx1GlobalGain: " << settings.m_tx1GlobalGain
        << " m_txBiasTee: " << settings.m_txBiasTee
        << " m_txTransverterMode: " << settings.m_txTransverterMode
        << " m_txTransverterDeltaFrequency: " << settings.m_txTransverterDeltaFrequency
        << " m_fileRecordName: " << settings.m_fileRecordName
        << " m_useReverseAPI: " << settings.m_useReverseAPI
        << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
        << " m_reverseAPIPort: " << settings.m_reverseAPIPort
        << " m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex;

    struct bladerf *dev = m_dev ? m_dev->getDev() : nullptr;

    qint64 rxXlatedDeviceCenterFrequency = settings.m_rxCenterFrequency;
    rxXlatedDeviceCenterFrequency -= settings.m_rxTransverterMode ? settings.m_rxTransverterDeltaFrequency : 0;
    rxXlatedDeviceCenterFrequency = rxXlatedDeviceCenterFrequency < 0 ? 0 : rxXlatedDeviceCenterFrequency;


    qint64 txXlatedDeviceCenterFrequency = settings.m_txCenterFrequency;
    txXlatedDeviceCenterFrequency -= settings.m_txTransverterMode ? settings.m_txTransverterDeltaFrequency : 0;
    txXlatedDeviceCenterFrequency = txXlatedDeviceCenterFrequency < 0 ? 0 : txXlatedDeviceCenterFrequency;

    if ((m_settings.m_devSampleRate != settings.m_devSampleRate) || force)
    {
        reverseAPIKeys.append("devSampleRate");

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

    if ((m_settings.m_rxBandwidth != settings.m_rxBandwidth) || force)
    {
        reverseAPIKeys.append("rxBandwidth");

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

    if ((m_settings.m_fcPos != settings.m_fcPos) || force)
    {
        reverseAPIKeys.append("fcPos");

        if (m_sourceThread)
        {
            m_sourceThread->setFcPos((int) settings.m_fcPos);
            qDebug() << "BladeRF2MIMO::applySettings: set fc pos (enum) to " << (int) settings.m_fcPos;
        }
    }

    if ((m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        reverseAPIKeys.append("log2Decim");

        if (m_sourceThread)
        {
            m_sourceThread->setLog2Decimation(settings.m_log2Decim);
            qDebug() << "BladeRF2MIMO::applySettings: set decimation to " << (1<<settings.m_log2Decim);
        }
    }

    if ((m_settings.m_log2Interp != settings.m_log2Interp) || force)
    {
        reverseAPIKeys.append("log2Interp");

        if (m_sinkThread)
        {
            m_sinkThread->setLog2Interpolation(settings.m_log2Interp);
            qDebug() << "BladeRF2Input::applySettings: set interpolation to " << (1<<settings.m_log2Interp);
        }
    }

    if ((m_settings.m_rxCenterFrequency != settings.m_rxCenterFrequency) || force) {
        reverseAPIKeys.append("rxCenterFrequency");
    }
    if ((m_settings.m_rxTransverterMode != settings.m_rxTransverterMode) || force) {
        reverseAPIKeys.append("rxTransverterMode");
    }
    if ((m_settings.m_rxTransverterDeltaFrequency != settings.m_rxTransverterDeltaFrequency) || force) {
        reverseAPIKeys.append("rxTransverterDeltaFrequency");
    }
    if ((m_settings.m_LOppmTenths != settings.m_LOppmTenths) || force) {
        reverseAPIKeys.append("LOppmTenths");
    }

    if ((m_settings.m_rxCenterFrequency != settings.m_rxCenterFrequency)
        || (m_settings.m_rxTransverterMode != settings.m_rxTransverterMode)
        || (m_settings.m_rxTransverterDeltaFrequency != settings.m_rxTransverterDeltaFrequency)
        || (m_settings.m_LOppmTenths != settings.m_LOppmTenths)
        || (m_settings.m_devSampleRate != settings.m_devSampleRate)
        || (m_settings.m_fcPos != settings.m_fcPos)
        || (m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        qint64 deviceCenterFrequency = DeviceSampleSource::calculateDeviceCenterFrequency(
                rxXlatedDeviceCenterFrequency,
                0,
                settings.m_log2Decim,
                (DeviceSampleSource::fcPos_t) settings.m_fcPos,
                settings.m_devSampleRate,
                DeviceSampleSource::FrequencyShiftScheme::FSHIFT_STD,
                false);

        if (dev)
        {
            if (setRxDeviceCenterFrequency(dev, deviceCenterFrequency, settings.m_LOppmTenths))
            {
                if (getMessageQueueToGUI())
                {
                    int min, max, step;
                    getRxGlobalGainRange(min, max, step);
                    MsgReportGainRange *msg = MsgReportGainRange::create(min, max, step, true);
                    getMessageQueueToGUI()->push(msg);
                }

            }
        }

        forwardChangeRxDSP = true;
    }

    if ((m_settings.m_rxBiasTee != settings.m_rxBiasTee) || force)
    {
        reverseAPIKeys.append("rxBiasTee");

        if (m_dev) {
            m_dev->setBiasTeeRx(settings.m_rxBiasTee);
        }
    }

    if ((m_settings.m_rx0GainMode != settings.m_rx0GainMode) || force)
    {
        reverseAPIKeys.append("rx0GainMode");

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

    if ((m_settings.m_rx1GainMode != settings.m_rx1GainMode) || force)
    {
        reverseAPIKeys.append("rx1GainMode");

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

    if ((m_settings.m_rx0GlobalGain != settings.m_rx0GlobalGain) || force) {
        reverseAPIKeys.append("rx0GlobalGain");
    }
    if ((m_settings.m_rx1GlobalGain != settings.m_rx1GlobalGain) || force) {
        reverseAPIKeys.append("rx1GlobalGain");
    }

    if ((m_settings.m_rx0GlobalGain != settings.m_rx0GlobalGain)
       || ((m_settings.m_rx0GlobalGain != settings.m_rx0GlobalGain) && (settings.m_rx0GlobalGain == BLADERF_GAIN_MANUAL)) || force)
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

    if ((m_settings.m_rx1GlobalGain != settings.m_rx1GlobalGain)
       || ((m_settings.m_rx1GlobalGain != settings.m_rx1GlobalGain) && (settings.m_rx1GlobalGain == BLADERF_GAIN_MANUAL)) || force)
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

    if ((m_settings.m_txCenterFrequency != settings.m_txCenterFrequency) || force) {
        reverseAPIKeys.append("txCenterFrequency");
    }
    if ((m_settings.m_txTransverterMode != settings.m_txTransverterMode) || force) {
        reverseAPIKeys.append("txTransverterMode");
    }
    if ((m_settings.m_txTransverterDeltaFrequency != settings.m_txTransverterDeltaFrequency) || force) {
        reverseAPIKeys.append("txTransverterDeltaFrequency");
    }

    if ((m_settings.m_txCenterFrequency != settings.m_txCenterFrequency)
        || (m_settings.m_txTransverterMode != settings.m_txTransverterMode)
        || (m_settings.m_txTransverterDeltaFrequency != settings.m_txTransverterDeltaFrequency)
        || (m_settings.m_LOppmTenths != settings.m_LOppmTenths)
        || (m_settings.m_devSampleRate != settings.m_devSampleRate) || force)
    {
        if (dev)
        {
            qint64 deviceCenterFrequency = DeviceSampleSink::calculateDeviceCenterFrequency(
                settings.m_txCenterFrequency,
                settings.m_txTransverterDeltaFrequency,
                settings.m_log2Interp,
                (DeviceSampleSink::fcPos_t) DeviceSampleSink::FC_POS_CENTER,
                settings.m_devSampleRate,
                settings.m_txTransverterMode);

            if (setTxDeviceCenterFrequency(dev, deviceCenterFrequency, settings.m_LOppmTenths))
            {
                if (getMessageQueueToGUI())
                {
                    int min, max, step;
                    getTxGlobalGainRange(min, max, step);
                    MsgReportGainRange *msg = MsgReportGainRange::create(min, max, step, false);
                    getMessageQueueToGUI()->push(msg);
                }
            }
        }

        forwardChangeTxDSP = true;
    }

    if ((m_settings.m_txBandwidth != settings.m_txBandwidth) || force)
    {
        reverseAPIKeys.append("txBandwidth");

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

    if ((m_settings.m_log2Interp != settings.m_log2Interp) || force)
    {
        reverseAPIKeys.append("log2Interp");

        if (m_sinkThread)
        {
            m_sinkThread->setLog2Interpolation(settings.m_log2Interp);
            qDebug() << "BladeRF2MIMO::applySettings: set interpolation to " << (1<<settings.m_log2Interp);
        }
    }

    if ((m_settings.m_txBiasTee != settings.m_txBiasTee) || force)
    {
        reverseAPIKeys.append("txBiasTee");

        if (m_dev) {
            m_dev->setBiasTeeTx(settings.m_txBiasTee);
        }
    }

    if ((m_settings.m_tx0GlobalGain != settings.m_tx0GlobalGain) || force)
    {
        reverseAPIKeys.append("tx0GlobalGain");

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

    if ((m_settings.m_tx1GlobalGain != settings.m_tx1GlobalGain) || force)
    {
        reverseAPIKeys.append("tx1GlobalGain");

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
        DSPSignalNotification *notifFileSink0 = new DSPSignalNotification(sampleRate, settings.m_rxCenterFrequency);
        m_fileSinks[0]->handleMessage(*notifFileSink0); // forward to file sinks
        DSPSignalNotification *notifFileSink1 = new DSPSignalNotification(sampleRate, settings.m_rxCenterFrequency);
        m_fileSinks[1]->handleMessage(*notifFileSink0); // forward to file sinks
        DSPMIMOSignalNotification *notif0 = new DSPMIMOSignalNotification(sampleRate, settings.m_rxCenterFrequency, true, 0);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif0);
        DSPMIMOSignalNotification *notif1 = new DSPMIMOSignalNotification(sampleRate, settings.m_rxCenterFrequency, true, 1);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif1);
    }

    if (forwardChangeTxDSP)
    {
        int sampleRate = settings.m_devSampleRate/(1<<settings.m_log2Interp);
        DSPMIMOSignalNotification *notif0 = new DSPMIMOSignalNotification(sampleRate, settings.m_rxCenterFrequency, false, 0);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif0);
        DSPMIMOSignalNotification *notif1 = new DSPMIMOSignalNotification(sampleRate, settings.m_rxCenterFrequency, false, 1);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif1);
    }

    // Reverse API settings

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
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
        qWarning("BladeRF2MIMO::setDeviceCenterFrequency: RX0: bladerf_set_frequency(%lld) failed: %s",
                freq_hz, bladerf_strerror(status));
        return false;
    }
    else
    {
        qDebug("BladeRF2MIMO::setDeviceCenterFrequency: RX0: bladerf_set_frequency(%lld)", freq_hz);
    }

    status = bladerf_set_frequency(dev, BLADERF_CHANNEL_RX(1), freq_hz);

    if (status < 0)
    {
        qWarning("BladeRF2MIMO::setDeviceCenterFrequency: RX1: bladerf_set_frequency(%lld) failed: %s",
                freq_hz, bladerf_strerror(status));
        return false;
    }
    else
    {
        qDebug("BladeRF2MIMO::setDeviceCenterFrequency: RX1: bladerf_set_frequency(%lld)", freq_hz);
    }

    return true;
}

bool BladeRF2MIMO::setTxDeviceCenterFrequency(struct bladerf *dev, quint64 freq_hz, int loPpmTenths)
{
    qint64 df = ((qint64)freq_hz * loPpmTenths) / 10000000LL;
    freq_hz += df;

    int status = bladerf_set_frequency(dev, BLADERF_CHANNEL_TX(0), freq_hz);

    if (status < 0) {
        qWarning("BladeRF2Output::setDeviceCenterFrequency: TX0: bladerf_set_frequency(%lld) failed: %s",
                freq_hz, bladerf_strerror(status));
        return false;
    }
    else
    {
        qDebug("BladeRF2Output::setDeviceCenterFrequency: TX0: bladerf_set_frequency(%lld)", freq_hz);
        return true;
    }

    status = bladerf_set_frequency(dev, BLADERF_CHANNEL_TX(1), freq_hz);

    if (status < 0) {
        qWarning("BladeRF2Output::setDeviceCenterFrequency: TX1: bladerf_set_frequency(%lld) failed: %s",
                freq_hz, bladerf_strerror(status));
        return false;
    }
    else
    {
        qDebug("BladeRF2Output::setDeviceCenterFrequency: TX1: bladerf_set_frequency(%lld)", freq_hz);
        return true;
    }
}

void BladeRF2MIMO::getRxFrequencyRange(uint64_t& min, uint64_t& max, int& step)
{
    if (m_dev) {
        m_dev->getFrequencyRangeRx(min, max, step);
    }
}

void BladeRF2MIMO::getRxSampleRateRange(int& min, int& max, int& step)
{
    if (m_dev) {
        m_dev->getSampleRateRangeRx(min, max, step);
    }
}

void BladeRF2MIMO::getRxBandwidthRange(int& min, int& max, int& step)
{
    if (m_dev) {
        m_dev->getBandwidthRangeRx(min, max, step);
    }
}

void BladeRF2MIMO::getRxGlobalGainRange(int& min, int& max, int& step)
{
    if (m_dev) {
        m_dev->getGlobalGainRangeRx(min, max, step);
    }
}

void BladeRF2MIMO::getTxFrequencyRange(uint64_t& min, uint64_t& max, int& step)
{
    if (m_dev) {
        m_dev->getFrequencyRangeTx(min, max, step);
    }
}

void BladeRF2MIMO::getTxSampleRateRange(int& min, int& max, int& step)
{
    if (m_dev) {
        m_dev->getSampleRateRangeTx(min, max, step);
    }
}

void BladeRF2MIMO::getTxBandwidthRange(int& min, int& max, int& step)
{
    if (m_dev) {
        m_dev->getBandwidthRangeTx(min, max, step);
    }
}

void BladeRF2MIMO::getTxGlobalGainRange(int& min, int& max, int& step)
{
    if (m_dev) {
        m_dev->getGlobalGainRangeTx(min, max, step);
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

    MsgConfigureBladeRF2MIMO *msg = MsgConfigureBladeRF2MIMO::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureBladeRF2MIMO *msgToGUI = MsgConfigureBladeRF2MIMO::create(settings, force);
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
    if (deviceSettingsKeys.contains("fcPos")) {
        settings.m_fcPos = static_cast<BladeRF2MIMOSettings::fcPos_t>(response.getBladeRf2MimoSettings()->getFcPos());
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

    if (deviceSettingsKeys.contains("fileRecordName")) {
        settings.m_fileRecordName = *response.getBladeRf2MimoSettings()->getFileRecordName();
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getBladeRf2OutputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getBladeRf2OutputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getBladeRf2OutputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getBladeRf2OutputSettings()->getReverseApiDeviceIndex();
    }
}

void BladeRF2MIMO::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const BladeRF2MIMOSettings& settings)
{
    response.getBladeRf2MimoSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getBladeRf2MimoSettings()->setLOppmTenths(settings.m_LOppmTenths);

    response.getBladeRf2MimoSettings()->setRxCenterFrequency(settings.m_rxCenterFrequency);
    response.getBladeRf2MimoSettings()->setLog2Decim(settings.m_log2Decim);
    response.getBladeRf2MimoSettings()->setFcPos((int) settings.m_fcPos);
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
    response.getBladeRf2MimoSettings()->setTxBandwidth(settings.m_txBandwidth);
    response.getBladeRf2MimoSettings()->setTx0GlobalGain(settings.m_tx0GlobalGain);
    response.getBladeRf2MimoSettings()->setTx1GlobalGain(settings.m_tx1GlobalGain);
    response.getBladeRf2MimoSettings()->setTxBiasTee(settings.m_txBiasTee ? 1 : 0);
    response.getBladeRf2MimoSettings()->setTxTransverterDeltaFrequency(settings.m_txTransverterDeltaFrequency);
    response.getBladeRf2MimoSettings()->setTxTransverterMode(settings.m_txTransverterMode ? 1 : 0);

    if (response.getBladeRf2MimoSettings()->getFileRecordName()) {
        *response.getBladeRf2MimoSettings()->getFileRecordName() = settings.m_fileRecordName;
    } else {
        response.getBladeRf2MimoSettings()->setFileRecordName(new QString(settings.m_fileRecordName));
    }

    response.getBladeRf2OutputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getBladeRf2OutputSettings()->getReverseApiAddress()) {
        *response.getBladeRf2OutputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getBladeRf2OutputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getBladeRf2OutputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getBladeRf2OutputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

int BladeRF2MIMO::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int BladeRF2MIMO::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    MsgStartStop *message = MsgStartStop::create(run, true); // TODO: Tx support
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgStartStop *msgToGUI = MsgStartStop::create(run, true); // TODO: Tx support
        m_guiMessageQueue->push(msgToGUI);
    }

    return 200;
}

void BladeRF2MIMO::webapiReverseSendSettings(QList<QString>& deviceSettingsKeys, const BladeRF2MIMOSettings& settings, bool force)
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
    if (deviceSettingsKeys.contains("fcPos") || force) {
        swgBladeRF2MIMOSettings->setFcPos((int) settings.m_fcPos);
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

    QBuffer *buffer=new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgDeviceSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);

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

    QBuffer *buffer=new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgDeviceSettings->asJson().toUtf8());
    buffer->seek(0);

    if (start) {
        m_networkManager->sendCustomRequest(m_networkRequest, "POST", buffer);
    } else {
        m_networkManager->sendCustomRequest(m_networkRequest, "DELETE", buffer);
    }

    delete swgDeviceSettings;
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
        return;
    }

    QString answer = reply->readAll();
    answer.chop(1); // remove last \n
    qDebug("BladeRF2MIMO::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
}
