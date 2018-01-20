///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
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
#include <string.h>
#include <errno.h>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"

#include "testsourceinput.h"
#include "device/devicesourceapi.h"
#include "testsourcethread.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/filerecord.h"

MESSAGE_CLASS_DEFINITION(TestSourceInput::MsgConfigureTestSource, Message)
MESSAGE_CLASS_DEFINITION(TestSourceInput::MsgFileRecord, Message)
MESSAGE_CLASS_DEFINITION(TestSourceInput::MsgStartStop, Message)


TestSourceInput::TestSourceInput(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_testSourceThread(0),
	m_deviceDescription(),
	m_running(false),
	m_masterTimer(deviceAPI->getMasterTimer())
{
    char recFileNameCStr[30];
    sprintf(recFileNameCStr, "test_%d.sdriq", m_deviceAPI->getDeviceUID());
    m_fileSink = new FileRecord(std::string(recFileNameCStr));
    m_deviceAPI->addSink(m_fileSink);

    if (!m_sampleFifo.setSize(96000 * 4)) {
        qCritical("TestSourceInput::TestSourceInput: Could not allocate SampleFifo");
    }
}

TestSourceInput::~TestSourceInput()
{
    if (m_running) { stop(); }
    m_deviceAPI->removeSink(m_fileSink);
    delete m_fileSink;
}

void TestSourceInput::destroy()
{
    delete this;
}

void TestSourceInput::init()
{
    applySettings(m_settings, true);
}

bool TestSourceInput::start()
{
	QMutexLocker mutexLocker(&m_mutex);

    if (m_running) stop();

	if ((m_testSourceThread = new TestSourceThread(&m_sampleFifo)) == 0)
	{
		qFatal("TestSourceInput::start: out of memory");
		stop();
		return false;
	}

	m_testSourceThread->setSamplerate(m_settings.m_sampleRate);
	m_testSourceThread->connectTimer(m_masterTimer);
	m_testSourceThread->startWork();

	mutexLocker.unlock();

	applySettings(m_settings, true);
	m_running = true;

	return true;
}

void TestSourceInput::stop()
{
	QMutexLocker mutexLocker(&m_mutex);

	if (m_testSourceThread != 0)
	{
	    m_testSourceThread->stopWork();
		delete m_testSourceThread;
		m_testSourceThread = 0;
	}

	m_running = false;
}

QByteArray TestSourceInput::serialize() const
{
    return m_settings.serialize();
}

bool TestSourceInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureTestSource* message = MsgConfigureTestSource::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureTestSource* messageToGUI = MsgConfigureTestSource::create(m_settings, true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& TestSourceInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int TestSourceInput::getSampleRate() const
{
	return m_settings.m_sampleRate;
}

quint64 TestSourceInput::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void TestSourceInput::setCenterFrequency(qint64 centerFrequency)
{
    TestSourceSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureTestSource* message = MsgConfigureTestSource::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureTestSource* messageToGUI = MsgConfigureTestSource::create(settings, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool TestSourceInput::handleMessage(const Message& message)
{
    if (MsgConfigureTestSource::match(message))
    {
        MsgConfigureTestSource& conf = (MsgConfigureTestSource&) message;
        qDebug() << "TestSourceInput::handleMessage: MsgConfigureTestSource";

        bool success = applySettings(conf.getSettings(), conf.getForce());

        if (!success)
        {
            qDebug("TestSourceInput::handleMessage: config error");
        }

        return true;
    }
    else if (MsgFileRecord::match(message))
    {
        MsgFileRecord& conf = (MsgFileRecord&) message;
        qDebug() << "RTLSDRInput::handleMessage: MsgFileRecord: " << conf.getStartStop();

        if (conf.getStartStop()) {
            m_fileSink->startRecording();
        } else {
            m_fileSink->stopRecording();
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "RTLSDRInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initAcquisition())
            {
                m_deviceAPI->startAcquisition();
                DSPEngine::instance()->startAudioOutput();
            }
        }
        else
        {
            m_deviceAPI->stopAcquisition();
            DSPEngine::instance()->stopAudioOutput();
        }

        return true;
    }
    else
    {
        return false;
    }
}

bool TestSourceInput::applySettings(const TestSourceSettings& settings, bool force)
{
    if ((m_settings.m_dcBlock != settings.m_dcBlock) || (m_settings.m_iqImbalance != settings.m_iqImbalance) || force)
    {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqImbalance);
        qDebug("TestSourceInput::applySettings: corrections: DC block: %s IQ imbalance: %s",
                settings.m_dcBlock ? "true" : "false",
                settings.m_iqImbalance ? "true" : "false");
    }

    if ((m_settings.m_sampleRate != settings.m_sampleRate) || force)
    {
        if (m_testSourceThread != 0)
        {
            m_testSourceThread->setSamplerate(settings.m_sampleRate);
            qDebug("TestSourceInput::applySettings: sample rate set to %d", settings.m_sampleRate);
        }
    }

    if ((m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        if (m_testSourceThread != 0)
        {
            m_testSourceThread->setLog2Decimation(settings.m_log2Decim);
            qDebug() << "TestSourceInput::applySettings: set decimation to " << (1<<settings.m_log2Decim);
        }
    }

    if (force || (m_settings.m_centerFrequency != settings.m_centerFrequency)
            || (m_settings.m_fcPos != settings.m_fcPos)
            || (m_settings.m_frequencyShift != settings.m_frequencyShift)
            || (m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        qint64 deviceCenterFrequency = settings.m_centerFrequency;
        int frequencyShift = settings.m_frequencyShift;
        qint64 f_img = deviceCenterFrequency;
        quint32 devSampleRate = settings.m_sampleRate;

        if (settings.m_log2Decim != 0)
        {
            if (settings.m_fcPos == TestSourceSettings::FC_POS_INFRA)
            {
                deviceCenterFrequency += (devSampleRate / 4);
                frequencyShift -= (devSampleRate / 4);
                f_img = deviceCenterFrequency + devSampleRate/2;
            }
            else if (settings.m_fcPos == TestSourceSettings::FC_POS_SUPRA)
            {
                deviceCenterFrequency -= (devSampleRate / 4);
                frequencyShift += (devSampleRate / 4);
                f_img = deviceCenterFrequency - devSampleRate/2;
            }
        }

        if (m_testSourceThread != 0)
        {
            m_testSourceThread->setFcPos((int) settings.m_fcPos);
            m_testSourceThread->setFrequencyShift(frequencyShift);
            qDebug() << "TestSourceInput::applySettings:"
                    << " center freq: " << settings.m_centerFrequency << " Hz"
                    << " device center freq: " << deviceCenterFrequency << " Hz"
                    << " device sample rate: " << devSampleRate << "Hz"
                    << " Actual sample rate: " << devSampleRate/(1<<m_settings.m_log2Decim) << "Hz"
                    << " fc shift: " << f_img << "Hz"
                    << " f shift: " << settings.m_frequencyShift;
        }
    }

    if ((m_settings.m_amplitudeBits != settings.m_amplitudeBits) || force)
    {
        if (m_testSourceThread != 0) {
            m_testSourceThread->setAmplitudeBits(settings.m_amplitudeBits);
        }
    }

    if ((m_settings.m_dcFactor != settings.m_dcFactor) || force)
    {
        if (m_testSourceThread != 0) {
            m_testSourceThread->setDCFactor(settings.m_dcFactor);
        }
    }

    if ((m_settings.m_iFactor != settings.m_iFactor) || force)
    {
        if (m_testSourceThread != 0) {
            m_testSourceThread->setIFactor(settings.m_iFactor);
        }
    }

    if ((m_settings.m_qFactor != settings.m_qFactor) || force)
    {
        if (m_testSourceThread != 0) {
            m_testSourceThread->setQFactor(settings.m_qFactor);
        }
    }

    if ((m_settings.m_sampleSizeIndex != settings.m_sampleSizeIndex) || force)
    {
        if (m_testSourceThread != 0) {
            m_testSourceThread->setBitSize(settings.m_sampleSizeIndex);
        }
    }

    if ((m_settings.m_sampleRate != settings.m_sampleRate)
        || (m_settings.m_centerFrequency != settings.m_centerFrequency)
        || (m_settings.m_log2Decim != settings.m_log2Decim)
        || (m_settings.m_fcPos != settings.m_fcPos) || force)
    {
        int sampleRate = settings.m_sampleRate/(1<<settings.m_log2Decim);
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, settings.m_centerFrequency);
        m_fileSink->handleMessage(*notif); // forward to file sink
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    m_settings = settings;
    return true;
}

int TestSourceInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int TestSourceInput::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    MsgStartStop *message = MsgStartStop::create(run);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgStartStop *msgToGUI = MsgStartStop::create(run);
        m_guiMessageQueue->push(msgToGUI);
    }

    return 200;
}
