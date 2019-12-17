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

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/dspdevicemimoengine.h"
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"

#include "testmosyncthread.h"
#include "testmosync.h"

MESSAGE_CLASS_DEFINITION(TestMOSync::MsgConfigureTestMOSync, Message)
MESSAGE_CLASS_DEFINITION(TestMOSync::MsgStartStop, Message)

TestMOSync::TestMOSync(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
    m_sinkThread(nullptr),
	m_deviceDescription("TestMOSync"),
    m_runningTx(false)
{
    m_mimoType = MIMOHalfSynchronous;
    m_sampleMOFifo.init(2, 96000 * 4);
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
    applySettings(m_settings, true);
}

bool TestMOSync::startTx()
{
    qDebug("TestMOSync::startTx");
	QMutexLocker mutexLocker(&m_mutex);

    if (m_runningTx) {
        stopTx();
    }

    m_sinkThread = new TestMOSyncThread();
    m_sampleMOFifo.reset();
    m_sinkThread->setFifo(&m_sampleMOFifo);
    m_sinkThread->setFcPos(m_settings.m_fcPosTx);
    m_sinkThread->setLog2Interpolation(m_settings.m_log2Interp);
	m_sinkThread->startWork();
	mutexLocker.unlock();
	m_runningTx = true;

    return true;
}

void TestMOSync::stopTx()
{
    qDebug("TestMOSync::stopTx");

    if (!m_sinkThread) {
        return;
    }

	QMutexLocker mutexLocker(&m_mutex);

    m_sinkThread->stopWork();
    delete m_sinkThread;
    m_sinkThread = nullptr;
    m_runningTx = false;
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

    MsgConfigureTestMOSync* message = MsgConfigureTestMOSync::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureTestMOSync* messageToGUI = MsgConfigureTestMOSync::create(m_settings, true);
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

    MsgConfigureTestMOSync* message = MsgConfigureTestMOSync::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureTestMOSync* messageToGUI = MsgConfigureTestMOSync::create(settings, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool TestMOSync::handleMessage(const Message& message)
{
    if (MsgConfigureTestMOSync::match(message))
    {
        MsgConfigureTestMOSync& conf = (MsgConfigureTestMOSync&) message;
        qDebug() << "TestMOSync::handleMessage: MsgConfigureTestMOSync";

        bool success = applySettings(conf.getSettings(), conf.getForce());

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

bool TestMOSync::applySettings(const TestMOSyncSettings& settings, bool force)
{
    QList<QString> reverseAPIKeys;
    bool forwardChangeTxDSP = false;

    qDebug() << "TestMOSync::applySettings: common: "
        << " m_sampleRate: " << settings.m_sampleRate
        << " m_centerFrequency: " << settings.m_centerFrequency
        << " m_log2Interp: " << settings.m_log2Interp
        << " m_fcPosTx: " << settings.m_fcPosTx
        << " force: " << force;

    if ((m_settings.m_sampleRate != settings.m_sampleRate) || force)
    {
        if (m_sinkThread) {
            m_sinkThread->setSamplerate(settings.m_sampleRate);
        }

        forwardChangeTxDSP = true;
    }

    if ((m_settings.m_fcPosTx != settings.m_fcPosTx) || force)
    {
        if (m_sinkThread) {
            m_sinkThread->setFcPos((int) settings.m_fcPosTx);
        }

        forwardChangeTxDSP = true;
    }

    if ((m_settings.m_log2Interp != settings.m_log2Interp) || force)
    {
        if (m_sinkThread)
        {
            m_sinkThread->setLog2Interpolation(settings.m_log2Interp);
            qDebug() << "BladeRF2Input::applySettings: set interpolation to " << (1<<settings.m_log2Interp);
        }

        forwardChangeTxDSP = true;
    }

    if (forwardChangeTxDSP)
    {
        int sampleRate = settings.m_sampleRate/(1<<settings.m_log2Interp);
        DSPMIMOSignalNotification *notif0 = new DSPMIMOSignalNotification(sampleRate, settings.m_centerFrequency, false, 0);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif0);
        DSPMIMOSignalNotification *notif1 = new DSPMIMOSignalNotification(sampleRate, settings.m_centerFrequency, false, 1);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif1);
    }

    m_settings = settings;
    return true;
}

int TestMOSync::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int TestMOSync::webapiRun(
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
