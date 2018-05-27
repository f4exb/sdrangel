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
    m_fileSink = new FileRecord(QString("test_%1.sdriq").arg(m_deviceAPI->getDeviceUID()));
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

    m_testSourceThread = new TestSourceThread(&m_sampleFifo);
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

        if (conf.getStartStop())
        {
            if (m_settings.m_fileRecordName.size() != 0) {
                m_fileSink->setFileName(m_settings.m_fileRecordName);
            } else {
                m_fileSink->genUniqueFileName(m_deviceAPI->getDeviceUID());
            }

            m_fileSink->startRecording();
        }
        else
        {
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
            }
        }
        else
        {
            m_deviceAPI->stopAcquisition();
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
    if ((m_settings.m_autoCorrOptions != settings.m_autoCorrOptions) || force)
    {
        switch(settings.m_autoCorrOptions)
        {
        case TestSourceSettings::AutoCorrDC:
            m_deviceAPI->configureCorrections(true, false);
            break;
        case TestSourceSettings::AutoCorrDCAndIQ:
            m_deviceAPI->configureCorrections(true, true);
            break;
        case TestSourceSettings::AutoCorrNone:
        default:
            m_deviceAPI->configureCorrections(false, false);
            break;
        }
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

    if ((m_settings.m_centerFrequency != settings.m_centerFrequency)
        || (m_settings.m_fcPos != settings.m_fcPos)
        || (m_settings.m_frequencyShift != settings.m_frequencyShift)
        || (m_settings.m_sampleRate != settings.m_sampleRate)
        || (m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        qint64 deviceCenterFrequency = DeviceSampleSource::calculateDeviceCenterFrequency(
                settings.m_centerFrequency,
                0, // no transverter mode
                settings.m_log2Decim,
                (DeviceSampleSource::fcPos_t) settings.m_fcPos,
                settings.m_sampleRate);

        int frequencyShift = settings.m_frequencyShift;
        quint32 devSampleRate = settings.m_sampleRate;

        if (settings.m_log2Decim != 0)
        {
            frequencyShift += DeviceSampleSource::calculateFrequencyShift(
                    settings.m_log2Decim,
                    (DeviceSampleSource::fcPos_t) settings.m_fcPos,
                    settings.m_sampleRate);
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

    if ((m_settings.m_phaseImbalance != settings.m_phaseImbalance) || force)
    {
        if (m_testSourceThread != 0) {
            m_testSourceThread->setPhaseImbalance(settings.m_phaseImbalance);
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

    if ((m_settings.m_modulationTone != settings.m_modulationTone) || force)
    {
        if (m_testSourceThread != 0) {
            m_testSourceThread->setToneFrequency(settings.m_modulationTone * 10);
        }
    }

    if ((m_settings.m_modulation != settings.m_modulation) || force)
    {
        if (m_testSourceThread != 0) {
            m_testSourceThread->setModulation(settings.m_modulation);
        }
    }

    if ((m_settings.m_amModulation != settings.m_amModulation) || force)
    {
        if (m_testSourceThread != 0) {
            m_testSourceThread->setAMModulation(settings.m_amModulation / 100.0f);
        }
    }

    if ((m_settings.m_fmDeviation != settings.m_fmDeviation) || force)
    {
        if (m_testSourceThread != 0) {
            m_testSourceThread->setFMDeviation(settings.m_fmDeviation * 100.0f);
        }
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

int TestSourceInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage __attribute__((unused)))
{
    response.setTestSourceSettings(new SWGSDRangel::SWGTestSourceSettings());
    response.getTestSourceSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int TestSourceInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage __attribute__((unused)))
{
    TestSourceSettings settings = m_settings;

    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getTestSourceSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("frequencyShift")) {
        settings.m_frequencyShift = response.getTestSourceSettings()->getFrequencyShift();
    }
    if (deviceSettingsKeys.contains("sampleRate")) {
        settings.m_sampleRate = response.getTestSourceSettings()->getSampleRate();
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getTestSourceSettings()->getLog2Decim();
    }
    if (deviceSettingsKeys.contains("fcPos")) {
        int fcPos = response.getTestSourceSettings()->getFcPos();
        fcPos = fcPos < 0 ? 0 : fcPos > 2 ? 2 : fcPos;
        settings.m_fcPos = (TestSourceSettings::fcPos_t) fcPos;
    }
    if (deviceSettingsKeys.contains("sampleSizeIndex")) {
        int sampleSizeIndex = response.getTestSourceSettings()->getSampleSizeIndex();
        sampleSizeIndex = sampleSizeIndex < 0 ? 0 : sampleSizeIndex > 1 ? 2 : sampleSizeIndex;
        settings.m_sampleSizeIndex = sampleSizeIndex;
    }
    if (deviceSettingsKeys.contains("amplitudeBits")) {
        settings.m_amplitudeBits = response.getTestSourceSettings()->getAmplitudeBits();
    }
    if (deviceSettingsKeys.contains("autoCorrOptions")) {
        int autoCorrOptions = response.getTestSourceSettings()->getAutoCorrOptions();
        autoCorrOptions = autoCorrOptions < 0 ? 0 : autoCorrOptions >= TestSourceSettings::AutoCorrLast ? TestSourceSettings::AutoCorrLast-1 : autoCorrOptions;
        settings.m_sampleSizeIndex = (TestSourceSettings::AutoCorrOptions) autoCorrOptions;
    }
    if (deviceSettingsKeys.contains("modulation")) {
        int modulation = response.getTestSourceSettings()->getModulation();
        modulation = modulation < 0 ? 0 : modulation >= TestSourceSettings::ModulationLast ? TestSourceSettings::ModulationLast-1 : modulation;
        settings.m_modulation = (TestSourceSettings::Modulation) modulation;
    }
    if (deviceSettingsKeys.contains("modulationTone")) {
        settings.m_modulationTone = response.getTestSourceSettings()->getModulationTone();
    }
    if (deviceSettingsKeys.contains("amModulation")) {
        settings.m_amModulation = response.getTestSourceSettings()->getAmModulation();
    };
    if (deviceSettingsKeys.contains("fmDeviation")) {
        settings.m_fmDeviation = response.getTestSourceSettings()->getFmDeviation();
    };
    if (deviceSettingsKeys.contains("dcFactor")) {
        settings.m_dcFactor = response.getTestSourceSettings()->getDcFactor();
    };
    if (deviceSettingsKeys.contains("iFactor")) {
        settings.m_iFactor = response.getTestSourceSettings()->getIFactor();
    };
    if (deviceSettingsKeys.contains("qFactor")) {
        settings.m_qFactor = response.getTestSourceSettings()->getQFactor();
    };
    if (deviceSettingsKeys.contains("phaseImbalance")) {
        settings.m_phaseImbalance = response.getTestSourceSettings()->getPhaseImbalance();
    };
    if (deviceSettingsKeys.contains("fileRecordName")) {
        settings.m_fileRecordName = *response.getTestSourceSettings()->getFileRecordName();
    }

    MsgConfigureTestSource *msg = MsgConfigureTestSource::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureTestSource *msgToGUI = MsgConfigureTestSource::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void TestSourceInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const TestSourceSettings& settings)
{
    response.getTestSourceSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getTestSourceSettings()->setFrequencyShift(settings.m_frequencyShift);
    response.getTestSourceSettings()->setSampleRate(settings.m_sampleRate);
    response.getTestSourceSettings()->setLog2Decim(settings.m_log2Decim);
    response.getTestSourceSettings()->setFcPos((int) settings.m_fcPos);
    response.getTestSourceSettings()->setSampleSizeIndex((int) settings.m_sampleSizeIndex);
    response.getTestSourceSettings()->setAmplitudeBits(settings.m_amplitudeBits);
    response.getTestSourceSettings()->setAutoCorrOptions((int) settings.m_autoCorrOptions);
    response.getTestSourceSettings()->setModulation((int) settings.m_modulation);
    response.getTestSourceSettings()->setModulationTone(settings.m_modulationTone);
    response.getTestSourceSettings()->setAmModulation(settings.m_amModulation);
    response.getTestSourceSettings()->setFmDeviation(settings.m_fmDeviation);
    response.getTestSourceSettings()->setDcFactor(settings.m_dcFactor);
    response.getTestSourceSettings()->setIFactor(settings.m_iFactor);
    response.getTestSourceSettings()->setQFactor(settings.m_qFactor);
    response.getTestSourceSettings()->setPhaseImbalance(settings.m_phaseImbalance);

    if (response.getTestSourceSettings()->getFileRecordName()) {
        *response.getTestSourceSettings()->getFileRecordName() = settings.m_fileRecordName;
    } else {
        response.getTestSourceSettings()->setFileRecordName(new QString(settings.m_fileRecordName));
    }
}

