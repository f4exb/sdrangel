///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include <algorithm>
#include <cmath>

#include <QDebug>
#include <QThread>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QBuffer>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGUSRPMIMOSettings.h"
#include "SWGUSRPMIMOReport.h"

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "dsp/samplemofifo.h"
#include "util/poweroftwo.h"
#include "usrp/deviceusrp.h"
#include "usrp/deviceusrpparam.h"
#include "usrp/deviceusrpshared.h"

#include "usrpmithread.h"
#include "usrpmothread.h"
#include "usrpmimoworker.h"
#include "usrpmimo.h"

MESSAGE_CLASS_DEFINITION(USRPMIMO::MsgConfigureUSRPMIMO, Message)
MESSAGE_CLASS_DEFINITION(USRPMIMO::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(USRPMIMO::MsgGetStreamInfo, Message)
MESSAGE_CLASS_DEFINITION(USRPMIMO::MsgReportStreamInfo, Message)
MESSAGE_CLASS_DEFINITION(USRPMIMO::MsgGetDeviceInfo, Message)

USRPMIMO::USRPMIMO(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_sourceThread(nullptr),
    m_sinkThread(nullptr),
    m_deviceDescription("USRPMIMO"),
    m_runningRx(false),
    m_runningTx(false),
    m_deviceParams(nullptr),
    m_nbRx(0),
    m_nbTx(0),
    m_workerThread(nullptr),
    m_worker(nullptr)
{
    openDevice();

    m_mimoType = MIMOHalfSynchronous;
    m_sampleMIFifo.init(m_nbRx, 1<<20);
    m_sampleMOFifo.init(m_nbTx, 4096 * 64);
    m_deviceAPI->setNbSourceStreams(m_nbRx);
    m_deviceAPI->setNbSinkStreams(m_nbTx);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &USRPMIMO::networkManagerFinished
    );

    if (m_deviceParams)
    {
        // Settings are applied to the device in a separate thread, so they don't block the GUI
        m_workerThread = new QThread();
        m_workerThread->setObjectName("USRPMIMOWorker");
        m_worker = new USRPMIMOWorker(m_deviceParams, m_nbRx, m_nbTx, getInputMessageQueue());
        m_worker->moveToThread(m_workerThread);
        m_workerThread->start();
    }
}

USRPMIMO::~USRPMIMO()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &USRPMIMO::networkManagerFinished
    );
    delete m_networkManager;

    if (m_workerThread)
    {
        m_workerThread->quit();
        m_workerThread->wait();
        delete m_worker;
        delete m_workerThread;
    }

    closeDevice();
}

void USRPMIMO::destroy()
{
    delete this;
}

bool USRPMIMO::openDevice()
{
    QString deviceStr;

    // If a non-discoverable device, serial will be of the form USRP-N
    if (m_deviceAPI->getSamplingDeviceSerial().startsWith("USRP"))
    {
        deviceStr = m_deviceAPI->getHardwareUserArguments();
    }
    else
    {
        deviceStr = m_deviceAPI->getSamplingDeviceSerial();

        if (m_deviceAPI->getHardwareUserArguments().size() != 0) {
            deviceStr = deviceStr + ',' + m_deviceAPI->getHardwareUserArguments();
        }
    }

    m_deviceParams = new DeviceUSRPParams();

    if (!m_deviceParams->open(deviceStr, false))
    {
        qCritical("USRPMIMO::openDevice: failed to open device");
        delete m_deviceParams;
        m_deviceParams = nullptr;
        // Nominal number of streams, so GUI can be displayed
        m_nbRx = 2;
        m_nbTx = 2;
        return false;
    }

    m_nbRx = std::min(2, (int) m_deviceParams->m_nbRxChannels);
    m_nbTx = std::min(2, (int) m_deviceParams->m_nbTxChannels);
    qDebug("USRPMIMO::openDevice: using %d Rx and %d Tx channels", m_nbRx, m_nbTx);

    return true;
}

void USRPMIMO::closeDevice()
{
    // Stop Tx first, so stopRx doesn't pause and resume the Tx thread, which may be called from the GUI thread,
    // while the device engine thread might be resizing the Tx FIFO
    if (m_runningTx) {
        stopTx();
    }

    if (m_runningRx) {
        stopRx();
    }

    if (m_deviceParams)
    {
        m_deviceParams->close();
        delete m_deviceParams;
        m_deviceParams = nullptr;
    }
}

void USRPMIMO::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

// Called from the device engine thread
bool USRPMIMO::startRx()
{
    QMutexLocker mutexLocker(&m_mutex);

    if (m_runningRx) {
        return true;
    }

    uhd::usrp::multi_usrp::sptr device = m_deviceParams ? m_deviceParams->getDevice() : nullptr;

    if (!device || (m_nbRx == 0))
    {
        qCritical("USRPMIMO::startRx: device was not opened or has no Rx channels");
        return false;
    }

    qDebug("USRPMIMO::startRx");
    size_t bufSamples = 0;

    USRPMIMOSettings settings;

    // Pause Tx streaming while the device is reconfigured
    bool txWasRunning = m_sinkThread && m_sinkThread->isRunning();
    if (txWasRunning) {
        m_sinkThread->stopWork();
    }

    {
        // Prevent settings being applied to the device while we set up the stream
        QMutexLocker deviceLocker(&m_deviceParams->m_mutex);

        {
            // Take a copy of the settings, as they can be changed in the GUI thread while the stream is being created.
            // Take it with the device mutex held, so it includes any settings already applied by the worker.
            // Settings changed after this will be applied by the worker once we release the device mutex.
            QMutexLocker settingsLocker(&m_settingsMutex);
            settings = m_settings;
        }

        try
        {
            // Apply Rx settings before creating stream
            // However, don't set LPF to <10MHz at this stage, otherwise there is massive TX LO leakage
            USRPMIMOWorker::applyDeviceSettings(m_deviceParams, m_nbRx, m_nbTx, settings, QList<QString>(),
                true, false, true, false, false, getInputMessageQueue());

            uhd::stream_args_t streamArgs("sc16", "sc16");

            for (int channel = 0; channel < m_nbRx; channel++)
            {
                device->set_rx_bandwidth(56000000, channel);
                streamArgs.channels.push_back(channel);
            }

            m_rxStream = device->get_rx_stream(streamArgs);

            // Read multiple packets per recv() call at high sample rates, to reduce overhead
            bufSamples = DeviceUSRP::getRecvBufferSamples(m_rxStream->get_max_num_samps(), settings.m_devSampleRate);

            // Wait for reference and LO to lock, then we can set desired bandwidth
            // Use actual clock source, in case requested clock wasn't detected
            QString clockSource = QString::fromStdString(device->get_clock_source(0));

            for (int channel = 0; channel < m_nbRx; channel++)
            {
                DeviceUSRP::waitForLock(device, clockSource, channel, true);
                device->set_rx_bandwidth(settings.m_rxLpfBW, channel);
            }
        }
        catch (std::exception& e)
        {
            qCritical() << "USRPMIMO::startRx: exception: " << e.what();
        }
    }

    if (txWasRunning) {
        m_sinkThread->startWork();
    }

    if (!m_rxStream) {
        return false;
    }

    m_sampleMIFifo.reset();
    USRPMIThread *thread = new USRPMIThread(device, m_rxStream, bufSamples, m_nbRx, &m_sampleMIFifo);
    thread->setLog2Decimation(settings.m_log2SoftDecim);
    thread->startWork();

    {
        // Settings changed while the stream was being created weren't sent to the thread, as it didn't exist,
        // so use current settings. Lock settings first, so no more changes can be made until it's published.
        QMutexLocker settingsLocker(&m_settingsMutex);
        QMutexLocker threadLocker(&m_threadMutex);
        thread->setLog2Decimation(m_settings.m_log2SoftDecim);
        m_sourceThread = thread;
    }

    m_runningRx = true;

    return true;
}

// Called from the device engine thread
bool USRPMIMO::startTx()
{
    QMutexLocker mutexLocker(&m_mutex);

    if (m_runningTx) {
        return true;
    }

    uhd::usrp::multi_usrp::sptr device = m_deviceParams ? m_deviceParams->getDevice() : nullptr;

    if (!device || (m_nbTx == 0))
    {
        qCritical("USRPMIMO::startTx: device was not opened or has no Tx channels");
        return false;
    }

    qDebug("USRPMIMO::startTx");
    size_t bufSamples = 0;

    USRPMIMOSettings settings;

    // Pause Rx streaming while the device is reconfigured. Rx channels will be realigned on restart
    bool rxWasRunning = m_sourceThread && m_sourceThread->isRunning();
    if (rxWasRunning) {
        m_sourceThread->stopWork();
    }

    {
        // Prevent settings being applied to the device while we set up the stream
        QMutexLocker deviceLocker(&m_deviceParams->m_mutex);

        {
            // Take a copy of the settings, as they can be changed in the GUI thread while the stream is being created.
            // Take it with the device mutex held, so it includes any settings already applied by the worker.
            // Settings changed after this will be applied by the worker once we release the device mutex.
            QMutexLocker settingsLocker(&m_settingsMutex);
            settings = m_settings;
        }

        try
        {
            // Apply Tx settings before creating stream
            // However, don't set LPF to <10MHz at this stage, otherwise there is massive TX LO leakage
            USRPMIMOWorker::applyDeviceSettings(m_deviceParams, m_nbRx, m_nbTx, settings, QList<QString>(),
                true, false, false, true, false, getInputMessageQueue());

            uhd::stream_args_t streamArgs("sc16", "sc16");

            for (int channel = 0; channel < m_nbTx; channel++)
            {
                device->set_tx_bandwidth(56000000, channel);
                streamArgs.channels.push_back(channel);
            }

            m_txStream = device->get_tx_stream(streamArgs);

            // Match our transmit buffer size to what UHD uses
            bufSamples = m_txStream->get_max_num_samps();

            // Wait for reference and LO to lock, then we can set desired bandwidth
            // Use actual clock source, in case requested clock wasn't detected
            QString clockSource = QString::fromStdString(device->get_clock_source(0));

            for (int channel = 0; channel < m_nbTx; channel++)
            {
                DeviceUSRP::waitForLock(device, clockSource, channel, false);
                device->set_tx_bandwidth(settings.m_txLpfBW, channel);
            }
        }
        catch (std::exception& e)
        {
            qCritical() << "USRPMIMO::startTx: exception: " << e.what();
        }
    }

    if (rxWasRunning) {
        m_sourceThread->startWork();
    }

    if (!m_txStream) {
        return false;
    }

    // Size FIFO for sample rate. This is safe here, as Tx isn't running, so FIFO isn't being written to
    m_sampleMOFifo.resize(getSampleMOFifoSize(settings));
    USRPMOThread *thread = new USRPMOThread(device, m_txStream, bufSamples, m_nbTx, &m_sampleMOFifo);
    thread->setLog2Interpolation(settings.m_log2SoftInterp);
    thread->startWork();

    {
        // Settings changed while the stream was being created weren't sent to the thread, as it didn't exist,
        // so use current settings. Lock settings first, so no more changes can be made until it's published.
        QMutexLocker settingsLocker(&m_settingsMutex);
        QMutexLocker threadLocker(&m_threadMutex);
        thread->setLog2Interpolation(m_settings.m_log2SoftInterp);
        unsigned int fifoSize = getSampleMOFifoSize(m_settings);

        if (fifoSize != getSampleMOFifoSize(settings)) {
            // Resize in device engine thread once we return
            thread->getInputMessageQueue()->push(DeviceUSRPShared::MsgResizeSampleFifo::create(fifoSize));
        }

        m_sinkThread = thread;
    }

    m_runningTx = true;

    return true;
}

// Usually called from the device engine thread, but can be called from the GUI thread from the destructor
void USRPMIMO::stopRx()
{
    QMutexLocker mutexLocker(&m_mutex);

    if (!m_runningRx) {
        return;
    }

    qDebug("USRPMIMO::stopRx");
    m_runningRx = false;

    USRPMIThread *thread;
    {
        QMutexLocker threadLocker(&m_threadMutex);
        thread = m_sourceThread;
        m_sourceThread = nullptr;
    }

    if (thread)
    {
        thread->stopWork();
        delete thread;
    }

    // Pause Tx streaming while the Rx stream is destroyed, as that reconfigures the device
    bool txWasRunning = m_sinkThread && m_sinkThread->isRunning();
    if (txWasRunning) {
        m_sinkThread->stopWork();
    }

    m_rxStream = nullptr;

    if (txWasRunning) {
        m_sinkThread->startWork();
    }
}

// Usually called from the device engine thread, but can be called from the GUI thread from the destructor
void USRPMIMO::stopTx()
{
    QMutexLocker mutexLocker(&m_mutex);

    if (!m_runningTx) {
        return;
    }

    qDebug("USRPMIMO::stopTx");
    m_runningTx = false;

    USRPMOThread *thread;
    {
        QMutexLocker threadLocker(&m_threadMutex);
        thread = m_sinkThread;
        m_sinkThread = nullptr;
    }

    if (thread)
    {
        // Wait for any FIFO resize being performed in the device engine thread to complete and prevent any more
        thread->detach();
        thread->stopWork();
        // Release stream, so it's destroyed before the device, even if the thread is deleted later
        thread->releaseStream();

        QThread *ownerThread = thread->thread();
        if (!ownerThread || (ownerThread == QThread::currentThread()) || ownerThread->isFinished()) {
            delete thread;
        } else {
            // Called from another thread while the device engine thread is still running, which may have
            // a pending call to handleInputMessages, so delete it in the device engine thread
            thread->deleteLater();
        }
    }

    // Pause Rx streaming while the Tx stream is destroyed, as that reconfigures the device
    bool rxWasRunning = m_sourceThread && m_sourceThread->isRunning();
    if (rxWasRunning) {
        m_sourceThread->stopWork();
    }

    m_txStream = nullptr;

    if (rxWasRunning) {
        m_sourceThread->startWork();
    }
}

QByteArray USRPMIMO::serialize() const
{
    return m_settings.serialize();
}

bool USRPMIMO::deserialize(const QByteArray& data)
{
    bool success = true;

    {
        QMutexLocker settingsLocker(&m_settingsMutex);

        if (!m_settings.deserialize(data))
        {
            m_settings.resetToDefaults();
            success = false;
        }
    }

    MsgConfigureUSRPMIMO* message = MsgConfigureUSRPMIMO::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureUSRPMIMO* messageToGUI = MsgConfigureUSRPMIMO::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& USRPMIMO::getDeviceDescription() const
{
    return m_deviceDescription;
}

int USRPMIMO::getSourceSampleRate(int index) const
{
    (void) index;
    return m_settings.m_devSampleRate / (1<<m_settings.m_log2SoftDecim);
}

int USRPMIMO::getSinkSampleRate(int index) const
{
    (void) index;
    return m_settings.m_devSampleRate / (1<<m_settings.m_log2SoftInterp);
}

quint64 USRPMIMO::getSourceCenterFrequency(int index) const
{
    (void) index;
    return m_settings.m_rxCenterFrequency;
}

void USRPMIMO::setSourceCenterFrequency(qint64 centerFrequency, int index)
{
    (void) index;
    USRPMIMOSettings settings = m_settings;
    settings.m_rxCenterFrequency = centerFrequency;

    MsgConfigureUSRPMIMO* message = MsgConfigureUSRPMIMO::create(settings, QList<QString>{"rxCenterFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureUSRPMIMO* messageToGUI = MsgConfigureUSRPMIMO::create(settings, QList<QString>{"rxCenterFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

quint64 USRPMIMO::getSinkCenterFrequency(int index) const
{
    (void) index;
    return m_settings.m_txCenterFrequency;
}

void USRPMIMO::setSinkCenterFrequency(qint64 centerFrequency, int index)
{
    (void) index;
    USRPMIMOSettings settings = m_settings;
    settings.m_txCenterFrequency = centerFrequency;

    MsgConfigureUSRPMIMO* message = MsgConfigureUSRPMIMO::create(settings, QList<QString>{"txCenterFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureUSRPMIMO* messageToGUI = MsgConfigureUSRPMIMO::create(settings, QList<QString>{"txCenterFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

void USRPMIMO::getRxLORange(float& minF, float& maxF) const
{
    minF = m_deviceParams ? m_deviceParams->m_loRangeRx.start() : 0.0f;
    maxF = m_deviceParams ? m_deviceParams->m_loRangeRx.stop() : 0.0f;
}

void USRPMIMO::getTxLORange(float& minF, float& maxF) const
{
    minF = m_deviceParams ? m_deviceParams->m_loRangeTx.start() : 0.0f;
    maxF = m_deviceParams ? m_deviceParams->m_loRangeTx.stop() : 0.0f;
}

// Sample rate is common to Rx and Tx, so return intersection of ranges
void USRPMIMO::getSRRange(float& minF, float& maxF) const
{
    minF = 0.0f;
    maxF = 0.0f;

    if (m_deviceParams)
    {
        if ((m_nbRx > 0) && (m_nbTx > 0))
        {
            minF = std::max(m_deviceParams->m_srRangeRx.start(), m_deviceParams->m_srRangeTx.start());
            maxF = std::min(m_deviceParams->m_srRangeRx.stop(), m_deviceParams->m_srRangeTx.stop());
        }
        else if (m_nbRx > 0)
        {
            minF = m_deviceParams->m_srRangeRx.start();
            maxF = m_deviceParams->m_srRangeRx.stop();
        }
        else if (m_nbTx > 0)
        {
            minF = m_deviceParams->m_srRangeTx.start();
            maxF = m_deviceParams->m_srRangeTx.stop();
        }
    }
}

void USRPMIMO::getRxLPRange(float& minF, float& maxF) const
{
    minF = m_deviceParams ? m_deviceParams->m_lpfRangeRx.start() : 0.0f;
    maxF = m_deviceParams ? m_deviceParams->m_lpfRangeRx.stop() : 0.0f;
}

void USRPMIMO::getTxLPRange(float& minF, float& maxF) const
{
    minF = m_deviceParams ? m_deviceParams->m_lpfRangeTx.start() : 0.0f;
    maxF = m_deviceParams ? m_deviceParams->m_lpfRangeTx.stop() : 0.0f;
}

void USRPMIMO::getRxGainRange(float& minF, float& maxF) const
{
    minF = m_deviceParams ? m_deviceParams->m_gainRangeRx.start() : 0.0f;
    maxF = m_deviceParams ? m_deviceParams->m_gainRangeRx.stop() : 0.0f;
}

void USRPMIMO::getTxGainRange(float& minF, float& maxF) const
{
    minF = m_deviceParams ? m_deviceParams->m_gainRangeTx.start() : 0.0f;
    maxF = m_deviceParams ? m_deviceParams->m_gainRangeTx.stop() : 0.0f;
}

QStringList USRPMIMO::getRxAntennas() const
{
    return m_deviceParams ? m_deviceParams->m_rxAntennas : QStringList();
}

QStringList USRPMIMO::getTxAntennas() const
{
    return m_deviceParams ? m_deviceParams->m_txAntennas : QStringList();
}

QStringList USRPMIMO::getClockSources() const
{
    return m_deviceParams ? m_deviceParams->m_clockSources : QStringList();
}

bool USRPMIMO::handleMessage(const Message& message)
{
    if (MsgConfigureUSRPMIMO::match(message))
    {
        MsgConfigureUSRPMIMO& conf = (MsgConfigureUSRPMIMO&) message;
        qDebug() << "USRPMIMO::handleMessage: MsgConfigureUSRPMIMO";

        if (!applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce())) {
            qDebug("USRPMIMO::handleMessage: config error");
        }

        return true;
    }
    else if (DeviceUSRPShared::MsgReportDeviceSettings::match(message))
    {
        // Settings actually set on the device, as read back in the worker or device engine thread
        DeviceUSRPShared::MsgReportDeviceSettings& report = (DeviceUSRPShared::MsgReportDeviceSettings&) message;
        QMutexLocker settingsLocker(&m_settingsMutex);
        QList<QString> settingsKeys;

        if (report.getClockSourceValid() && (report.getClockSource() != m_settings.m_clockSource))
        {
            m_settings.m_clockSource = report.getClockSource();
            settingsKeys.append("clockSource");
        }

        if (report.getSampleRateValid())
        {
            int devSampleRate = (int) std::round(report.getSampleRate());
            int masterClockRate = (int) std::round(report.getMasterClockRate());
            qDebug() << "USRPMIMO::handleMessage: MsgReportDeviceSettings: devSampleRate:" << devSampleRate << "masterClockRate:" << masterClockRate;

            if ((devSampleRate != m_settings.m_devSampleRate) || (masterClockRate != m_settings.m_masterClockRate))
            {
                bool sampleRateChanged = devSampleRate != m_settings.m_devSampleRate;
                m_settings.m_devSampleRate = devSampleRate;
                m_settings.m_masterClockRate = masterClockRate;

                if (sampleRateChanged) {
                    resizeSampleMOFifo();
                }

                settingsKeys.append("devSampleRate");
                settingsKeys.append("masterClockRate");
                forwardChangeRxDSP();
                forwardChangeTxDSP();
            }
        }

        if (!settingsKeys.isEmpty() && m_guiMessageQueue) {
            m_guiMessageQueue->push(MsgConfigureUSRPMIMO::create(m_settings, settingsKeys, false));
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "USRPMIMO::handleMessage: "
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
            webapiReverseSendStartStop(startStopRxElseTx, cmd.getStartStop());
        }

        return true;
    }
    else if (MsgGetStreamInfo::match(message))
    {
        if (m_guiMessageQueue)
        {
            QMutexLocker threadLocker(&m_threadMutex);
            bool active;
            quint32 overUnderRuns;
            quint32 timeoutsDropped;

            if (m_sourceThread)
            {
                m_sourceThread->getStreamStatus(active, overUnderRuns, timeoutsDropped);
                m_guiMessageQueue->push(MsgReportStreamInfo::create(true, true, active, overUnderRuns, timeoutsDropped));
            }
            else
            {
                m_guiMessageQueue->push(MsgReportStreamInfo::create(true, false, false, 0, 0));
            }

            if (m_sinkThread)
            {
                m_sinkThread->getStreamStatus(active, overUnderRuns, timeoutsDropped);
                m_guiMessageQueue->push(MsgReportStreamInfo::create(false, true, active, overUnderRuns, timeoutsDropped));
            }
            else
            {
                m_guiMessageQueue->push(MsgReportStreamInfo::create(false, false, false, 0, 0));
            }
        }

        return true;
    }
    else if (MsgGetDeviceInfo::match(message))
    {
        // Reading a sensor is quick, so do it here, but don't block the GUI thread
        // if settings are being applied to the device in another thread
        if (m_deviceParams && m_deviceParams->getDevice() && m_deviceParams->m_mutex.tryLock())
        {
            double temperature = 0.0;
            bool temperatureValid = DeviceUSRP::getTemperature(m_deviceParams->getDevice(), m_nbRx > 0, 0, temperature);
            m_deviceParams->m_mutex.unlock();

            if (m_guiMessageQueue) {
                m_guiMessageQueue->push(DeviceUSRPShared::MsgReportDeviceInfo::create(temperatureValid, temperature));
            }
        }

        return true;
    }
    else
    {
        return false;
    }
}

bool USRPMIMO::applySettings(const USRPMIMOSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    QMutexLocker settingsLocker(&m_settingsMutex);
    qDebug() << "USRPMIMO::applySettings: force:" << force << settings.getDebugString(settingsKeys, force);
    bool changeRxDSP = false;
    bool changeTxDSP = false;

    if (settingsKeys.contains("devSampleRate") || force)
    {
        changeRxDSP = true;
        changeTxDSP = true;
    }

    if (settingsKeys.contains("rxCenterFrequency")
        || settingsKeys.contains("rxTransverterMode")
        || settingsKeys.contains("rxTransverterDeltaFrequency")
        || settingsKeys.contains("log2SoftDecim")
        || force)
    {
        changeRxDSP = true;
    }

    if (settingsKeys.contains("txCenterFrequency")
        || settingsKeys.contains("txTransverterMode")
        || settingsKeys.contains("txTransverterDeltaFrequency")
        || settingsKeys.contains("log2SoftInterp")
        || force)
    {
        changeTxDSP = true;
    }

    {
        QMutexLocker threadLocker(&m_threadMutex);

        if (m_sourceThread && (settingsKeys.contains("log2SoftDecim") || force)) {
            m_sourceThread->setLog2Decimation(settings.m_log2SoftDecim);
        }

        if (m_sinkThread && (settingsKeys.contains("log2SoftInterp") || force)) {
            m_sinkThread->setLog2Interpolation(settings.m_log2SoftInterp);
        }
    }

    // Applying settings to the device can take a long time (E.g. sample rate, clock source and bandwidth),
    // so they are applied in the worker thread, rather than blocking the GUI thread.
    // Values actually set are reported back via DeviceUSRPShared::MsgReportDeviceSettings.
    if (m_worker) {
        m_worker->getInputMessageQueue()->push(MsgConfigureUSRPMIMO::create(settings, settingsKeys, force));
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

    if (settingsKeys.contains("devSampleRate") || settingsKeys.contains("log2SoftInterp") || force) {
        resizeSampleMOFifo();
    }

    // If the device coerces the sample rate, this will be sent again with the actual rate, once reported
    if (changeRxDSP) {
        forwardChangeRxDSP();
    }
    if (changeTxDSP) {
        forwardChangeTxDSP();
    }

    return true;
}

unsigned int USRPMIMO::getSampleMOFifoSize(const USRPMIMOSettings& settings)
{
    unsigned int fifoRate = std::max(
        (unsigned int) settings.m_devSampleRate / (1<<settings.m_log2SoftInterp),
        48000U);
    return SampleMOFifo::getSizePolicy(fifoRate);
}

// Resize Tx FIFO for current sample rate. Called from GUI thread
void USRPMIMO::resizeSampleMOFifo()
{
    QMutexLocker threadLocker(&m_threadMutex);

    // Samples are written to the FIFO in the device engine thread, so resize it there, with streaming paused.
    // If Tx isn't running, it will be resized when started
    if (m_sinkThread) {
        m_sinkThread->getInputMessageQueue()->push(DeviceUSRPShared::MsgResizeSampleFifo::create(getSampleMOFifoSize(m_settings)));
    }
}

void USRPMIMO::forwardChangeRxDSP()
{
    int sampleRate = m_settings.m_devSampleRate / (1<<m_settings.m_log2SoftDecim);

    for (int i = 0; i < m_nbRx; i++)
    {
        DSPMIMOSignalNotification *notif = new DSPMIMOSignalNotification(sampleRate, m_settings.m_rxCenterFrequency, true, i);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }
}

void USRPMIMO::forwardChangeTxDSP()
{
    int sampleRate = m_settings.m_devSampleRate / (1<<m_settings.m_log2SoftInterp);

    for (int i = 0; i < m_nbTx; i++)
    {
        DSPMIMOSignalNotification *notif = new DSPMIMOSignalNotification(sampleRate, m_settings.m_txCenterFrequency, false, i);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }
}

int USRPMIMO::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setUsrpMimoSettings(new SWGSDRangel::SWGUSRPMIMOSettings());
    response.getUsrpMimoSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int USRPMIMO::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    USRPMIMOSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureUSRPMIMO *msg = MsgConfigureUSRPMIMO::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureUSRPMIMO *msgToGUI = MsgConfigureUSRPMIMO::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void USRPMIMO::webapiUpdateDeviceSettings(
        USRPMIMOSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("title")) {
        settings.m_title = *response.getUsrpMimoSettings()->getTitle();
    }
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getUsrpMimoSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("clockSource")) {
        settings.m_clockSource = *response.getUsrpMimoSettings()->getClockSource();
    }
    if (deviceSettingsKeys.contains("gpioDir")) {
        settings.m_gpioDir = response.getUsrpMimoSettings()->getGpioDir() & 0xFF;
    }
    if (deviceSettingsKeys.contains("gpioPins")) {
        settings.m_gpioPins = response.getUsrpMimoSettings()->getGpioPins() & 0xFF;
    }
    if (deviceSettingsKeys.contains("rxCenterFrequency")) {
        settings.m_rxCenterFrequency = response.getUsrpMimoSettings()->getRxCenterFrequency();
    }
    if (deviceSettingsKeys.contains("rxLOOffset")) {
        settings.m_rxLOOffset = response.getUsrpMimoSettings()->getRxLoOffset();
    }
    if (deviceSettingsKeys.contains("log2SoftDecim")) {
        settings.m_log2SoftDecim = qBound(0, response.getUsrpMimoSettings()->getLog2SoftDecim(), 6);
    }
    if (deviceSettingsKeys.contains("rxLpfBW")) {
        settings.m_rxLpfBW = response.getUsrpMimoSettings()->getRxLpfBw();
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getUsrpMimoSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getUsrpMimoSettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("rxTransverterMode")) {
        settings.m_rxTransverterMode = response.getUsrpMimoSettings()->getRxTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("rxTransverterDeltaFrequency")) {
        settings.m_rxTransverterDeltaFrequency = response.getUsrpMimoSettings()->getRxTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("rx0GainMode")) {
        settings.m_rx0GainMode = response.getUsrpMimoSettings()->getRx0GainMode() == 0 ? USRPMIMOSettings::GAIN_AUTO : USRPMIMOSettings::GAIN_MANUAL;
    }
    if (deviceSettingsKeys.contains("rx0Gain")) {
        settings.m_rx0Gain = response.getUsrpMimoSettings()->getRx0Gain();
    }
    if (deviceSettingsKeys.contains("rx0AntennaPath")) {
        settings.m_rx0AntennaPath = *response.getUsrpMimoSettings()->getRx0AntennaPath();
    }
    if (deviceSettingsKeys.contains("rx1GainMode")) {
        settings.m_rx1GainMode = response.getUsrpMimoSettings()->getRx1GainMode() == 0 ? USRPMIMOSettings::GAIN_AUTO : USRPMIMOSettings::GAIN_MANUAL;
    }
    if (deviceSettingsKeys.contains("rx1Gain")) {
        settings.m_rx1Gain = response.getUsrpMimoSettings()->getRx1Gain();
    }
    if (deviceSettingsKeys.contains("rx1AntennaPath")) {
        settings.m_rx1AntennaPath = *response.getUsrpMimoSettings()->getRx1AntennaPath();
    }
    if (deviceSettingsKeys.contains("txCenterFrequency")) {
        settings.m_txCenterFrequency = response.getUsrpMimoSettings()->getTxCenterFrequency();
    }
    if (deviceSettingsKeys.contains("txLOOffset")) {
        settings.m_txLOOffset = response.getUsrpMimoSettings()->getTxLoOffset();
    }
    if (deviceSettingsKeys.contains("log2SoftInterp")) {
        settings.m_log2SoftInterp = qBound(0, response.getUsrpMimoSettings()->getLog2SoftInterp(), 6);
    }
    if (deviceSettingsKeys.contains("txLpfBW")) {
        settings.m_txLpfBW = response.getUsrpMimoSettings()->getTxLpfBw();
    }
    if (deviceSettingsKeys.contains("txTransverterMode")) {
        settings.m_txTransverterMode = response.getUsrpMimoSettings()->getTxTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("txTransverterDeltaFrequency")) {
        settings.m_txTransverterDeltaFrequency = response.getUsrpMimoSettings()->getTxTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("tx0Gain")) {
        settings.m_tx0Gain = response.getUsrpMimoSettings()->getTx0Gain();
    }
    if (deviceSettingsKeys.contains("tx0AntennaPath")) {
        settings.m_tx0AntennaPath = *response.getUsrpMimoSettings()->getTx0AntennaPath();
    }
    if (deviceSettingsKeys.contains("tx1Gain")) {
        settings.m_tx1Gain = response.getUsrpMimoSettings()->getTx1Gain();
    }
    if (deviceSettingsKeys.contains("tx1AntennaPath")) {
        settings.m_tx1AntennaPath = *response.getUsrpMimoSettings()->getTx1AntennaPath();
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getUsrpMimoSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getUsrpMimoSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getUsrpMimoSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getUsrpMimoSettings()->getReverseApiDeviceIndex();
    }
}

void USRPMIMO::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const USRPMIMOSettings& settings)
{
    if (response.getUsrpMimoSettings()->getTitle()) {
        *response.getUsrpMimoSettings()->getTitle() = settings.m_title;
    } else {
        response.getUsrpMimoSettings()->setTitle(new QString(settings.m_title));
    }
    response.getUsrpMimoSettings()->setDevSampleRate(settings.m_devSampleRate);
    if (response.getUsrpMimoSettings()->getClockSource()) {
        *response.getUsrpMimoSettings()->getClockSource() = settings.m_clockSource;
    } else {
        response.getUsrpMimoSettings()->setClockSource(new QString(settings.m_clockSource));
    }
    response.getUsrpMimoSettings()->setGpioDir(settings.m_gpioDir);
    response.getUsrpMimoSettings()->setGpioPins(settings.m_gpioPins);
    response.getUsrpMimoSettings()->setRxCenterFrequency(settings.m_rxCenterFrequency);
    response.getUsrpMimoSettings()->setRxLoOffset(settings.m_rxLOOffset);
    response.getUsrpMimoSettings()->setLog2SoftDecim(settings.m_log2SoftDecim);
    response.getUsrpMimoSettings()->setRxLpfBw((qint32) settings.m_rxLpfBW);
    response.getUsrpMimoSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getUsrpMimoSettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    response.getUsrpMimoSettings()->setRxTransverterMode(settings.m_rxTransverterMode ? 1 : 0);
    response.getUsrpMimoSettings()->setRxTransverterDeltaFrequency(settings.m_rxTransverterDeltaFrequency);
    response.getUsrpMimoSettings()->setRx0GainMode((int) settings.m_rx0GainMode);
    response.getUsrpMimoSettings()->setRx0Gain(settings.m_rx0Gain);
    if (response.getUsrpMimoSettings()->getRx0AntennaPath()) {
        *response.getUsrpMimoSettings()->getRx0AntennaPath() = settings.m_rx0AntennaPath;
    } else {
        response.getUsrpMimoSettings()->setRx0AntennaPath(new QString(settings.m_rx0AntennaPath));
    }
    response.getUsrpMimoSettings()->setRx1GainMode((int) settings.m_rx1GainMode);
    response.getUsrpMimoSettings()->setRx1Gain(settings.m_rx1Gain);
    if (response.getUsrpMimoSettings()->getRx1AntennaPath()) {
        *response.getUsrpMimoSettings()->getRx1AntennaPath() = settings.m_rx1AntennaPath;
    } else {
        response.getUsrpMimoSettings()->setRx1AntennaPath(new QString(settings.m_rx1AntennaPath));
    }
    response.getUsrpMimoSettings()->setTxCenterFrequency(settings.m_txCenterFrequency);
    response.getUsrpMimoSettings()->setTxLoOffset(settings.m_txLOOffset);
    response.getUsrpMimoSettings()->setLog2SoftInterp(settings.m_log2SoftInterp);
    response.getUsrpMimoSettings()->setTxLpfBw((qint32) settings.m_txLpfBW);
    response.getUsrpMimoSettings()->setTxTransverterMode(settings.m_txTransverterMode ? 1 : 0);
    response.getUsrpMimoSettings()->setTxTransverterDeltaFrequency(settings.m_txTransverterDeltaFrequency);
    response.getUsrpMimoSettings()->setTx0Gain(settings.m_tx0Gain);
    if (response.getUsrpMimoSettings()->getTx0AntennaPath()) {
        *response.getUsrpMimoSettings()->getTx0AntennaPath() = settings.m_tx0AntennaPath;
    } else {
        response.getUsrpMimoSettings()->setTx0AntennaPath(new QString(settings.m_tx0AntennaPath));
    }
    response.getUsrpMimoSettings()->setTx1Gain(settings.m_tx1Gain);
    if (response.getUsrpMimoSettings()->getTx1AntennaPath()) {
        *response.getUsrpMimoSettings()->getTx1AntennaPath() = settings.m_tx1AntennaPath;
    } else {
        response.getUsrpMimoSettings()->setTx1AntennaPath(new QString(settings.m_tx1AntennaPath));
    }
    response.getUsrpMimoSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);
    if (response.getUsrpMimoSettings()->getReverseApiAddress()) {
        *response.getUsrpMimoSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getUsrpMimoSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }
    response.getUsrpMimoSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getUsrpMimoSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

int USRPMIMO::webapiRunGet(
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

int USRPMIMO::webapiRun(
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

int USRPMIMO::webapiReportGet(SWGSDRangel::SWGDeviceReport& response, QString& errorMessage)
{
    (void) errorMessage;
    response.setUsrpMimoReport(new SWGSDRangel::SWGUSRPMIMOReport());
    response.getUsrpMimoReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void USRPMIMO::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    SWGSDRangel::SWGUSRPMIMOReport *report = response.getUsrpMimoReport();
    bool active = false;
    quint32 overUnderRuns = 0;
    quint32 timeoutsDropped = 0;

    report->setSuccess(m_deviceParams ? 1 : 0);

    {
        QMutexLocker threadLocker(&m_threadMutex);

        if (m_sourceThread) {
            m_sourceThread->getStreamStatus(active, overUnderRuns, timeoutsDropped);
        }

        report->setRxStreamActive(active ? 1 : 0);
        report->setRxOverrunCount(overUnderRuns);
        report->setRxTimeoutCount(timeoutsDropped);

        active = false;
        overUnderRuns = 0;
        timeoutsDropped = 0;

        if (m_sinkThread) {
            m_sinkThread->getStreamStatus(active, overUnderRuns, timeoutsDropped);
        }

        report->setTxStreamActive(active ? 1 : 0);
        report->setTxUnderrunCount(overUnderRuns);
        report->setTxDroppedPacketsCount(timeoutsDropped);
    }

    report->setMasterClockRate(m_settings.m_masterClockRate);

    // Don't block if settings are being applied to the device in another thread
    if (m_deviceParams && m_deviceParams->getDevice() && m_deviceParams->m_mutex.tryLock())
    {
        double temperature;

        if (DeviceUSRP::getTemperature(m_deviceParams->getDevice(), m_nbRx > 0, 0, temperature)) {
            report->setTemperature(temperature);
        }

        m_deviceParams->m_mutex.unlock();
    }
}

void USRPMIMO::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const USRPMIMOSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(2); // MIMO
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("USRP"));
    swgDeviceSettings->setUsrpMimoSettings(new SWGSDRangel::SWGUSRPMIMOSettings());
    SWGSDRangel::SWGUSRPMIMOSettings *swgUSRPMIMOSettings = swgDeviceSettings->getUsrpMimoSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("title") || force) {
        swgUSRPMIMOSettings->setTitle(new QString(settings.m_title));
    }
    if (deviceSettingsKeys.contains("devSampleRate") || force) {
        swgUSRPMIMOSettings->setDevSampleRate(settings.m_devSampleRate);
    }
    if (deviceSettingsKeys.contains("clockSource") || force) {
        swgUSRPMIMOSettings->setClockSource(new QString(settings.m_clockSource));
    }
    if (deviceSettingsKeys.contains("gpioDir") || force) {
        swgUSRPMIMOSettings->setGpioDir(settings.m_gpioDir);
    }
    if (deviceSettingsKeys.contains("gpioPins") || force) {
        swgUSRPMIMOSettings->setGpioPins(settings.m_gpioPins);
    }
    if (deviceSettingsKeys.contains("rxCenterFrequency") || force) {
        swgUSRPMIMOSettings->setRxCenterFrequency(settings.m_rxCenterFrequency);
    }
    if (deviceSettingsKeys.contains("rxLOOffset") || force) {
        swgUSRPMIMOSettings->setRxLoOffset(settings.m_rxLOOffset);
    }
    if (deviceSettingsKeys.contains("log2SoftDecim") || force) {
        swgUSRPMIMOSettings->setLog2SoftDecim(settings.m_log2SoftDecim);
    }
    if (deviceSettingsKeys.contains("rxLpfBW") || force) {
        swgUSRPMIMOSettings->setRxLpfBw((qint32) settings.m_rxLpfBW);
    }
    if (deviceSettingsKeys.contains("dcBlock") || force) {
        swgUSRPMIMOSettings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("iqCorrection") || force) {
        swgUSRPMIMOSettings->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("rxTransverterMode") || force) {
        swgUSRPMIMOSettings->setRxTransverterMode(settings.m_rxTransverterMode ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("rxTransverterDeltaFrequency") || force) {
        swgUSRPMIMOSettings->setRxTransverterDeltaFrequency(settings.m_rxTransverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("rx0GainMode") || force) {
        swgUSRPMIMOSettings->setRx0GainMode((int) settings.m_rx0GainMode);
    }
    if (deviceSettingsKeys.contains("rx0Gain") || force) {
        swgUSRPMIMOSettings->setRx0Gain(settings.m_rx0Gain);
    }
    if (deviceSettingsKeys.contains("rx0AntennaPath") || force) {
        swgUSRPMIMOSettings->setRx0AntennaPath(new QString(settings.m_rx0AntennaPath));
    }
    if (deviceSettingsKeys.contains("rx1GainMode") || force) {
        swgUSRPMIMOSettings->setRx1GainMode((int) settings.m_rx1GainMode);
    }
    if (deviceSettingsKeys.contains("rx1Gain") || force) {
        swgUSRPMIMOSettings->setRx1Gain(settings.m_rx1Gain);
    }
    if (deviceSettingsKeys.contains("rx1AntennaPath") || force) {
        swgUSRPMIMOSettings->setRx1AntennaPath(new QString(settings.m_rx1AntennaPath));
    }
    if (deviceSettingsKeys.contains("txCenterFrequency") || force) {
        swgUSRPMIMOSettings->setTxCenterFrequency(settings.m_txCenterFrequency);
    }
    if (deviceSettingsKeys.contains("txLOOffset") || force) {
        swgUSRPMIMOSettings->setTxLoOffset(settings.m_txLOOffset);
    }
    if (deviceSettingsKeys.contains("log2SoftInterp") || force) {
        swgUSRPMIMOSettings->setLog2SoftInterp(settings.m_log2SoftInterp);
    }
    if (deviceSettingsKeys.contains("txLpfBW") || force) {
        swgUSRPMIMOSettings->setTxLpfBw((qint32) settings.m_txLpfBW);
    }
    if (deviceSettingsKeys.contains("txTransverterMode") || force) {
        swgUSRPMIMOSettings->setTxTransverterMode(settings.m_txTransverterMode ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("txTransverterDeltaFrequency") || force) {
        swgUSRPMIMOSettings->setTxTransverterDeltaFrequency(settings.m_txTransverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("tx0Gain") || force) {
        swgUSRPMIMOSettings->setTx0Gain(settings.m_tx0Gain);
    }
    if (deviceSettingsKeys.contains("tx0AntennaPath") || force) {
        swgUSRPMIMOSettings->setTx0AntennaPath(new QString(settings.m_tx0AntennaPath));
    }
    if (deviceSettingsKeys.contains("tx1Gain") || force) {
        swgUSRPMIMOSettings->setTx1Gain(settings.m_tx1Gain);
    }
    if (deviceSettingsKeys.contains("tx1AntennaPath") || force) {
        swgUSRPMIMOSettings->setTx1AntennaPath(new QString(settings.m_tx1AntennaPath));
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

void USRPMIMO::webapiReverseSendStartStop(bool rxElseTx, bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(2); // MIMO
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("USRP"));

    // MIMO devices are started and stopped per subsystem (0 for Rx, 1 for Tx), as /device/run only supports Rx or Tx devices
    QString deviceSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/subdevice/%4/run")
            .arg(m_settings.m_reverseAPIAddress)
            .arg(m_settings.m_reverseAPIPort)
            .arg(m_settings.m_reverseAPIDeviceIndex)
            .arg(rxElseTx ? 0 : 1);
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

void USRPMIMO::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "USRPMIMO::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("USRPMIMO::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
