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
#include <QBuffer>
#include <QThread>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/dspdevicemimoengine.h"
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"

#include "testmosyncworker.h"
#include "testmosync.h"

MESSAGE_CLASS_DEFINITION(TestMOSync::MsgConfigureTestMOSync, Message)
MESSAGE_CLASS_DEFINITION(TestMOSync::MsgStartStop, Message)

TestMOSync::TestMOSync(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_spectrumVis(SDR_TX_SCALEF),
	m_settings(),
    m_sinkWorker(nullptr),
    m_sinkWorkerThread(nullptr),
	m_deviceDescription("TestMOSync"),
    m_runningTx(false),
    m_masterTimer(deviceAPI->getMasterTimer()),
    m_feedSpectrumIndex(0)
{
    m_mimoType = MIMOHalfSynchronous;
    m_sampleMOFifo.init(2, SampleMOFifo::getSizePolicy(m_settings.m_sampleRate));
    m_deviceAPI->setNbSourceStreams(0);
    m_deviceAPI->setNbSinkStreams(2);
}

TestMOSync::~TestMOSync()
{}

void TestMOSync::destroy()
{
    delete this;
}

void TestMOSync::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

bool TestMOSync::startTx()
{
	QMutexLocker mutexLocker(&m_mutex);

    if (m_runningTx) {
        return true;
    }

    qDebug("TestMOSync::startTx");
    m_sinkWorkerThread = new QThread();
    m_sinkWorker = new TestMOSyncWorker();
    m_sinkWorker->moveToThread(m_sinkWorkerThread);

    QObject::connect(m_sinkWorkerThread, &QThread::finished, m_sinkWorker, &QObject::deleteLater);
    QObject::connect(m_sinkWorkerThread, &QThread::finished, m_sinkWorkerThread, &QThread::deleteLater);

    m_sampleMOFifo.reset();
    m_sinkWorker->setFifo(&m_sampleMOFifo);
    m_sinkWorker->setFcPos(m_settings.m_fcPosTx);
    m_sinkWorker->setSamplerate(m_settings.m_sampleRate);
    m_sinkWorker->setLog2Interpolation(m_settings.m_log2Interp);
    m_sinkWorker->setSpectrumSink(&m_spectrumVis);
    m_sinkWorker->setFeedSpectrumIndex(m_feedSpectrumIndex);
    m_sinkWorker->connectTimer(m_masterTimer);
	startWorker();
	mutexLocker.unlock();
	m_runningTx = true;

    return true;
}

void TestMOSync::stopTx()
{
	QMutexLocker mutexLocker(&m_mutex);

    if (!m_runningTx) {
        return;
    }

    qDebug("TestMOSync::stopTx");
    m_runningTx = false;

    stopWorker();
    m_sinkWorker = nullptr;
    m_sinkWorkerThread = nullptr;
}

void TestMOSync::startWorker()
{
    m_sinkWorker->startWork();
    m_sinkWorkerThread->start();
}

void TestMOSync::stopWorker()
{
    m_sinkWorker->stopWork();
    m_sinkWorkerThread->quit();
    m_sinkWorkerThread->wait();
}

QByteArray TestMOSync::serialize() const
{
    return m_settings.serialize();
}

bool TestMOSync::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureTestMOSync* message = MsgConfigureTestMOSync::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureTestMOSync* messageToGUI = MsgConfigureTestMOSync::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& TestMOSync::getDeviceDescription() const
{
	return m_deviceDescription;
}

int TestMOSync::getSourceSampleRate(int index) const
{
    (void) index;
    return 0;
}

int TestMOSync::getSinkSampleRate(int index) const
{
    (void) index;
    int rate = m_settings.m_sampleRate;
    return (rate / (1<<m_settings.m_log2Interp));
}

quint64 TestMOSync::getSourceCenterFrequency(int index) const
{
    (void) index;
    return 0;
}

void TestMOSync::setSourceCenterFrequency(qint64 centerFrequency, int index)
{
    (void) centerFrequency;
    (void) index;
}

quint64 TestMOSync::getSinkCenterFrequency(int index) const
{
    (void) index;
    return m_settings.m_centerFrequency;
}

void TestMOSync::setSinkCenterFrequency(qint64 centerFrequency, int index)
{
    (void) index;
    TestMOSyncSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureTestMOSync* message = MsgConfigureTestMOSync::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureTestMOSync* messageToGUI = MsgConfigureTestMOSync::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool TestMOSync::handleMessage(const Message& message)
{
    if (MsgConfigureTestMOSync::match(message))
    {
        MsgConfigureTestMOSync& conf = (MsgConfigureTestMOSync&) message;
        qDebug() << "TestMOSync::handleMessage: MsgConfigureTestMOSync";

        bool success = applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());

        if (!success) {
            qDebug("TestMOSync::handleMessage: config error");
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "TestMOSync::handleMessage: "
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

        return true;
    }
    else
    {
        return false;
    }
}

void TestMOSync::setFeedSpectrumIndex(unsigned int feedSpectrumIndex)
{
    m_feedSpectrumIndex = feedSpectrumIndex > 1 ? 1 : feedSpectrumIndex;

    if (m_sinkWorker) {
        m_sinkWorker->setFeedSpectrumIndex(m_feedSpectrumIndex);
    }
}

bool TestMOSync::applySettings(const TestMOSyncSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "TestMOSync::applySettings: force:" << force << settings.getDebugString(settingsKeys, force);
    bool forwardChangeTxDSP = false;

    if (settingsKeys.contains("centerFrequency") || force) {
        forwardChangeTxDSP = true;
    }

    if (settingsKeys.contains("sampleRate")
       || settingsKeys.contains("log2Interp") || force)
    {
        m_sampleMOFifo.resize(SampleMOFifo::getSizePolicy(m_settings.m_sampleRate));
    }

    if (settingsKeys.contains("sampleRate") || force)
    {
        if (m_sinkWorker) {
            m_sinkWorker->setSamplerate(settings.m_sampleRate);
        }

        forwardChangeTxDSP = true;
    }

    if (settingsKeys.contains("fcPosTx") || force)
    {
        if (m_sinkWorker) {
            m_sinkWorker->setFcPos((int) settings.m_fcPosTx);
        }

        forwardChangeTxDSP = true;
    }

    if (settingsKeys.contains("log2Interp") || force)
    {
        if (m_sinkWorker)
        {
            m_sinkWorker->setLog2Interpolation(settings.m_log2Interp);
            qDebug() << "TestMOSync::applySettings: set interpolation to " << (1<<settings.m_log2Interp);
        }

        forwardChangeTxDSP = true;
    }

    if (forwardChangeTxDSP)
    {
        DSPMIMOSignalNotification *notif0 = new DSPMIMOSignalNotification(settings.m_sampleRate, settings.m_centerFrequency, false, 0);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif0);
        DSPMIMOSignalNotification *notif1 = new DSPMIMOSignalNotification(settings.m_sampleRate, settings.m_centerFrequency, false, 1);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif1);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    return true;
}

int TestMOSync::webapiRunGet(
        int subsystemIndex,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    if (subsystemIndex == 1)
    {
        m_deviceAPI->getDeviceEngineStateStr(*response.getState(), 1); // Tx only
        return 200;
    }
    else
    {
        errorMessage = QString("Subsystem index invalid: expect 1 (Tx) only");
        return 404;
    }

    response.setState(new QString("N/A"));
    return 200;
}

int TestMOSync::webapiRun(
        bool run,
        int subsystemIndex,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    if (subsystemIndex == 1)
    {
        m_deviceAPI->getDeviceEngineStateStr(*response.getState());
        MsgStartStop *message = MsgStartStop::create(run, true); // Tx only
        m_inputMessageQueue.push(message);

        if (m_guiMessageQueue) // forward to GUI if any
        {
            MsgStartStop *msgToGUI = MsgStartStop::create(run, true); // Tx only
            m_guiMessageQueue->push(msgToGUI);
        }

        return 200;
    }
    else
    {
        errorMessage = QString("Subsystem index invalid: expect 1 (Tx) only");
        return 404;
    }
}

int TestMOSync::webapiSettingsGet(
        SWGSDRangel::SWGDeviceSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setTestMoSyncSettings(new SWGSDRangel::SWGTestMOSyncSettings());
    response.getTestMoSyncSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int TestMOSync::webapiSettingsPutPatch(
        bool force,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response, // query + response
        QString& errorMessage)
{
    (void) errorMessage;
    TestMOSyncSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureTestMOSync *msg = MsgConfigureTestMOSync::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureTestMOSync *msgToGUI = MsgConfigureTestMOSync::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void TestMOSync::webapiFormatDeviceSettings(
        SWGSDRangel::SWGDeviceSettings& response,
        const TestMOSyncSettings& settings)
{
    response.getTestMoSyncSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getTestMoSyncSettings()->setFcPosTx((int) settings.m_fcPosTx);
    response.getTestMoSyncSettings()->setLog2Interp(settings.m_log2Interp);
    response.getTestMoSyncSettings()->setSampleRate(settings.m_sampleRate);
}

void TestMOSync::webapiUpdateDeviceSettings(
        TestMOSyncSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getTestMoSyncSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("fcPosTx")) {
        settings.m_fcPosTx = (TestMOSyncSettings::fcPos_t) response.getTestMoSyncSettings()->getFcPosTx();
    }
    if (deviceSettingsKeys.contains("log2Interp")) {
        settings.m_log2Interp = response.getTestMoSyncSettings()->getLog2Interp();
    }
    if (deviceSettingsKeys.contains("sampleRate")) {
        settings.m_sampleRate = response.getTestMoSyncSettings()->getSampleRate();
    }
}
