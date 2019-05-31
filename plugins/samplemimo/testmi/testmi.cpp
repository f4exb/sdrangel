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

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/dspdevicemimoengine.h"
#include "dsp/devicesamplesource.h"
#include "dsp/filerecord.h"

#include "testmithread.h"
#include "testmi.h"

MESSAGE_CLASS_DEFINITION(TestMI::MsgConfigureTestSource, Message)
MESSAGE_CLASS_DEFINITION(TestMI::MsgFileRecord, Message)
MESSAGE_CLASS_DEFINITION(TestMI::MsgStartStop, Message)


TestMI::TestMI(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_testSourceThread(0),
	m_deviceDescription(),
	m_running(false),
	m_masterTimer(deviceAPI->getMasterTimer())
{
    m_fileSink = new FileRecord(QString("test_%1.sdriq").arg(m_deviceAPI->getDeviceUID()));
    m_deviceAPI->setNbSourceStreams(1);
    m_deviceAPI->addSourceStream(); // Add a new source stream data set in the engine
    m_deviceAPI->addAncillarySink(m_fileSink);
    m_sampleSinkFifos.push_back(SampleSinkFifo(96000 * 4));
    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

TestMI::~TestMI()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;

    if (m_running) {
        stop();
    }

    m_deviceAPI->removeAncillarySink(m_fileSink);
    m_deviceAPI->removeLastSourceStream(); // Remove the last source stream data set in the engine
    delete m_fileSink;
}

void TestMI::destroy()
{
    delete this;
}

void TestMI::init()
{
    applySettings(m_settings, true);
}

bool TestMI::start()
{
	QMutexLocker mutexLocker(&m_mutex);

    if (m_running) stop();

    m_testSourceThread = new TestMIThread(&m_sampleSinkFifos[0]);
	m_testSourceThread->setSamplerate(m_settings.m_sampleRate);
	m_testSourceThread->startStop(true);

	mutexLocker.unlock();

	applySettings(m_settings, true);
	m_running = true;

	return true;
}

void TestMI::stop()
{
	QMutexLocker mutexLocker(&m_mutex);

	if (m_testSourceThread != 0)
	{
	    m_testSourceThread->startStop(false);
        m_testSourceThread->deleteLater();
		m_testSourceThread = 0;
	}

	m_running = false;
}

QByteArray TestMI::serialize() const
{
    return m_settings.serialize();
}

bool TestMI::deserialize(const QByteArray& data)
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

const QString& TestMI::getDeviceDescription() const
{
	return m_deviceDescription;
}

int TestMI::getSourceSampleRate(int index) const
{
    (void) index;
	return m_settings.m_sampleRate/(1<<m_settings.m_log2Decim);
}

quint64 TestMI::getSourceCenterFrequency(int index) const
{
    (void) index;
	return m_settings.m_centerFrequency;
}

void TestMI::setSourceCenterFrequency(qint64 centerFrequency, int index)
{
    (void) index;
    TestMISettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureTestSource* message = MsgConfigureTestSource::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureTestSource* messageToGUI = MsgConfigureTestSource::create(settings, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool TestMI::handleMessage(const Message& message)
{
    if (MsgConfigureTestSource::match(message))
    {
        MsgConfigureTestSource& conf = (MsgConfigureTestSource&) message;
        qDebug() << "TestMI::handleMessage: MsgConfigureTestSource";

        bool success = applySettings(conf.getSettings(), conf.getForce());

        if (!success)
        {
            qDebug("TestMI::handleMessage: config error");
        }

        return true;
    }
    else if (MsgFileRecord::match(message))
    {
        MsgFileRecord& conf = (MsgFileRecord&) message;
        qDebug() << "TestMI::handleMessage: MsgFileRecord: " << conf.getStartStop();

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
        qDebug() << "TestMI::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initDeviceEngine())
            {
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

bool TestMI::applySettings(const TestMISettings& settings, bool force)
{
    QList<QString> reverseAPIKeys;

    if ((m_settings.m_autoCorrOptions != settings.m_autoCorrOptions) || force)
    {
        reverseAPIKeys.append("autoCorrOptions");

        switch(settings.m_autoCorrOptions)
        {
        case TestMISettings::AutoCorrDC:
            m_deviceAPI->configureCorrections(true, false);
            break;
        case TestMISettings::AutoCorrDCAndIQ:
            m_deviceAPI->configureCorrections(true, true);
            break;
        case TestMISettings::AutoCorrNone:
        default:
            m_deviceAPI->configureCorrections(false, false);
            break;
        }
    }

    if ((m_settings.m_sampleRate != settings.m_sampleRate) || force)
    {
        reverseAPIKeys.append("sampleRate");

        if (m_testSourceThread != 0)
        {
            m_testSourceThread->setSamplerate(settings.m_sampleRate);
            qDebug("TestMI::applySettings: sample rate set to %d", settings.m_sampleRate);
        }
    }

    if ((m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        reverseAPIKeys.append("log2Decim");

        if (m_testSourceThread != 0)
        {
            m_testSourceThread->setLog2Decimation(settings.m_log2Decim);
            qDebug() << "TestMI::applySettings: set decimation to " << (1<<settings.m_log2Decim);
        }
    }

    if ((m_settings.m_centerFrequency != settings.m_centerFrequency)
        || (m_settings.m_fcPos != settings.m_fcPos)
        || (m_settings.m_frequencyShift != settings.m_frequencyShift)
        || (m_settings.m_sampleRate != settings.m_sampleRate)
        || (m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        reverseAPIKeys.append("centerFrequency");
        reverseAPIKeys.append("fcPos");
        reverseAPIKeys.append("frequencyShift");

        qint64 deviceCenterFrequency = DeviceSampleSource::calculateDeviceCenterFrequency(
                settings.m_centerFrequency,
                0, // no transverter mode
                settings.m_log2Decim,
                (DeviceSampleSource::fcPos_t) settings.m_fcPos,
                settings.m_sampleRate,
                DeviceSampleSource::FrequencyShiftScheme::FSHIFT_STD,
                false);

        int frequencyShift = settings.m_frequencyShift;
        quint32 devSampleRate = settings.m_sampleRate;

        if (settings.m_log2Decim != 0)
        {
            frequencyShift += DeviceSampleSource::calculateFrequencyShift(
                    settings.m_log2Decim,
                    (DeviceSampleSource::fcPos_t) settings.m_fcPos,
                    settings.m_sampleRate,
                    DeviceSampleSource::FSHIFT_STD);
        }

        if (m_testSourceThread != 0)
        {
            m_testSourceThread->setFcPos((int) settings.m_fcPos);
            m_testSourceThread->setFrequencyShift(frequencyShift);
            qDebug() << "TestMI::applySettings:"
                    << " center freq: " << settings.m_centerFrequency << " Hz"
                    << " device center freq: " << deviceCenterFrequency << " Hz"
                    << " device sample rate: " << devSampleRate << "Hz"
                    << " Actual sample rate: " << devSampleRate/(1<<m_settings.m_log2Decim) << "Hz"
                    << " f shift: " << settings.m_frequencyShift;
        }
    }

    if ((m_settings.m_amplitudeBits != settings.m_amplitudeBits) || force)
    {
        reverseAPIKeys.append("amplitudeBits");

        if (m_testSourceThread != 0) {
            m_testSourceThread->setAmplitudeBits(settings.m_amplitudeBits);
        }
    }

    if ((m_settings.m_dcFactor != settings.m_dcFactor) || force)
    {
        reverseAPIKeys.append("dcFactor");

        if (m_testSourceThread != 0) {
            m_testSourceThread->setDCFactor(settings.m_dcFactor);
        }
    }

    if ((m_settings.m_iFactor != settings.m_iFactor) || force)
    {
        reverseAPIKeys.append("iFactor");

        if (m_testSourceThread != 0) {
            m_testSourceThread->setIFactor(settings.m_iFactor);
        }
    }

    if ((m_settings.m_qFactor != settings.m_qFactor) || force)
    {
        reverseAPIKeys.append("qFactor");

        if (m_testSourceThread != 0) {
            m_testSourceThread->setQFactor(settings.m_qFactor);
        }
    }

    if ((m_settings.m_phaseImbalance != settings.m_phaseImbalance) || force)
    {
        reverseAPIKeys.append("phaseImbalance");

        if (m_testSourceThread != 0) {
            m_testSourceThread->setPhaseImbalance(settings.m_phaseImbalance);
        }
    }

    if ((m_settings.m_sampleSizeIndex != settings.m_sampleSizeIndex) || force)
    {
        reverseAPIKeys.append("sampleSizeIndex");

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
        DSPDeviceMIMOEngine::SignalNotification *engineNotif = new DSPDeviceMIMOEngine::SignalNotification(
            sampleRate, settings.m_centerFrequency, true, 0);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(engineNotif);
    }

    if ((m_settings.m_modulationTone != settings.m_modulationTone) || force)
    {
        reverseAPIKeys.append("modulationTone");

        if (m_testSourceThread != 0) {
            m_testSourceThread->setToneFrequency(settings.m_modulationTone * 10);
        }
    }

    if ((m_settings.m_modulation != settings.m_modulation) || force)
    {
        reverseAPIKeys.append("modulation");

        if (m_testSourceThread != 0)
        {
            m_testSourceThread->setModulation(settings.m_modulation);

            if (settings.m_modulation == TestMISettings::ModulationPattern0) {
                m_testSourceThread->setPattern0();
            } else if (settings.m_modulation == TestMISettings::ModulationPattern1) {
                m_testSourceThread->setPattern1();
            } else if (settings.m_modulation == TestMISettings::ModulationPattern2) {
                m_testSourceThread->setPattern2();
            }
        }
    }

    if ((m_settings.m_amModulation != settings.m_amModulation) || force)
    {
        reverseAPIKeys.append("amModulation");

        if (m_testSourceThread != 0) {
            m_testSourceThread->setAMModulation(settings.m_amModulation / 100.0f);
        }
    }

    if ((m_settings.m_fmDeviation != settings.m_fmDeviation) || force)
    {
        reverseAPIKeys.append("fmDeviation");

        if (m_testSourceThread != 0) {
            m_testSourceThread->setFMDeviation(settings.m_fmDeviation * 100.0f);
        }
    }

    if (settings.m_useReverseAPI)
    {
        qDebug("TestMI::applySettings: call webapiReverseSendSettings");
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    m_settings = settings;
    return true;
}

int TestMI::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int TestMI::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
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

int TestMI::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setTestMiSettings(new SWGSDRangel::SWGTestMISettings());
    response.getTestMiSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int TestMI::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    TestMISettings settings = m_settings;

    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getTestMiSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("frequencyShift")) {
        settings.m_frequencyShift = response.getTestMiSettings()->getFrequencyShift();
    }
    if (deviceSettingsKeys.contains("sampleRate")) {
        settings.m_sampleRate = response.getTestMiSettings()->getSampleRate();
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getTestMiSettings()->getLog2Decim();
    }
    if (deviceSettingsKeys.contains("fcPos")) {
        int fcPos = response.getTestMiSettings()->getFcPos();
        fcPos = fcPos < 0 ? 0 : fcPos > 2 ? 2 : fcPos;
        settings.m_fcPos = (TestMISettings::fcPos_t) fcPos;
    }
    if (deviceSettingsKeys.contains("sampleSizeIndex")) {
        int sampleSizeIndex = response.getTestMiSettings()->getSampleSizeIndex();
        sampleSizeIndex = sampleSizeIndex < 0 ? 0 : sampleSizeIndex > 1 ? 2 : sampleSizeIndex;
        settings.m_sampleSizeIndex = sampleSizeIndex;
    }
    if (deviceSettingsKeys.contains("amplitudeBits")) {
        settings.m_amplitudeBits = response.getTestMiSettings()->getAmplitudeBits();
    }
    if (deviceSettingsKeys.contains("autoCorrOptions")) {
        int autoCorrOptions = response.getTestMiSettings()->getAutoCorrOptions();
        autoCorrOptions = autoCorrOptions < 0 ? 0 : autoCorrOptions >= TestMISettings::AutoCorrLast ? TestMISettings::AutoCorrLast-1 : autoCorrOptions;
        settings.m_sampleSizeIndex = (TestMISettings::AutoCorrOptions) autoCorrOptions;
    }
    if (deviceSettingsKeys.contains("modulation")) {
        int modulation = response.getTestMiSettings()->getModulation();
        modulation = modulation < 0 ? 0 : modulation >= TestMISettings::ModulationLast ? TestMISettings::ModulationLast-1 : modulation;
        settings.m_modulation = (TestMISettings::Modulation) modulation;
    }
    if (deviceSettingsKeys.contains("modulationTone")) {
        settings.m_modulationTone = response.getTestMiSettings()->getModulationTone();
    }
    if (deviceSettingsKeys.contains("amModulation")) {
        settings.m_amModulation = response.getTestMiSettings()->getAmModulation();
    };
    if (deviceSettingsKeys.contains("fmDeviation")) {
        settings.m_fmDeviation = response.getTestMiSettings()->getFmDeviation();
    };
    if (deviceSettingsKeys.contains("dcFactor")) {
        settings.m_dcFactor = response.getTestMiSettings()->getDcFactor();
    };
    if (deviceSettingsKeys.contains("iFactor")) {
        settings.m_iFactor = response.getTestMiSettings()->getIFactor();
    };
    if (deviceSettingsKeys.contains("qFactor")) {
        settings.m_qFactor = response.getTestMiSettings()->getQFactor();
    };
    if (deviceSettingsKeys.contains("phaseImbalance")) {
        settings.m_phaseImbalance = response.getTestMiSettings()->getPhaseImbalance();
    };
    if (deviceSettingsKeys.contains("fileRecordName")) {
        settings.m_fileRecordName = *response.getTestMiSettings()->getFileRecordName();
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getTestMiSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getTestMiSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getTestMiSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getTestMiSettings()->getReverseApiDeviceIndex();
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

void TestMI::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const TestMISettings& settings)
{
    response.getTestMiSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getTestMiSettings()->setFrequencyShift(settings.m_frequencyShift);
    response.getTestMiSettings()->setSampleRate(settings.m_sampleRate);
    response.getTestMiSettings()->setLog2Decim(settings.m_log2Decim);
    response.getTestMiSettings()->setFcPos((int) settings.m_fcPos);
    response.getTestMiSettings()->setSampleSizeIndex((int) settings.m_sampleSizeIndex);
    response.getTestMiSettings()->setAmplitudeBits(settings.m_amplitudeBits);
    response.getTestMiSettings()->setAutoCorrOptions((int) settings.m_autoCorrOptions);
    response.getTestMiSettings()->setModulation((int) settings.m_modulation);
    response.getTestMiSettings()->setModulationTone(settings.m_modulationTone);
    response.getTestMiSettings()->setAmModulation(settings.m_amModulation);
    response.getTestMiSettings()->setFmDeviation(settings.m_fmDeviation);
    response.getTestMiSettings()->setDcFactor(settings.m_dcFactor);
    response.getTestMiSettings()->setIFactor(settings.m_iFactor);
    response.getTestMiSettings()->setQFactor(settings.m_qFactor);
    response.getTestMiSettings()->setPhaseImbalance(settings.m_phaseImbalance);

    if (response.getTestMiSettings()->getFileRecordName()) {
        *response.getTestMiSettings()->getFileRecordName() = settings.m_fileRecordName;
    } else {
        response.getTestMiSettings()->setFileRecordName(new QString(settings.m_fileRecordName));
    }

    response.getTestMiSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getTestMiSettings()->getReverseApiAddress()) {
        *response.getTestMiSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getTestMiSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getTestMiSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getTestMiSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

void TestMI::webapiReverseSendSettings(QList<QString>& deviceSettingsKeys, const TestMISettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("TestSource"));
    swgDeviceSettings->setTestMiSettings(new SWGSDRangel::SWGTestMISettings());
    SWGSDRangel::SWGTestMISettings *swgTestMISettings = swgDeviceSettings->getTestMiSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        swgTestMISettings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("frequencyShift") || force) {
        swgTestMISettings->setFrequencyShift(settings.m_frequencyShift);
    }
    if (deviceSettingsKeys.contains("sampleRate") || force) {
        swgTestMISettings->setSampleRate(settings.m_sampleRate);
    }
    if (deviceSettingsKeys.contains("log2Decim") || force) {
        swgTestMISettings->setLog2Decim(settings.m_log2Decim);
    }
    if (deviceSettingsKeys.contains("fcPos") || force) {
        swgTestMISettings->setFcPos((int) settings.m_fcPos);
    }
    if (deviceSettingsKeys.contains("sampleSizeIndex") || force) {
        swgTestMISettings->setSampleSizeIndex(settings.m_sampleSizeIndex);
    }
    if (deviceSettingsKeys.contains("amplitudeBits") || force) {
        swgTestMISettings->setAmplitudeBits(settings.m_amplitudeBits);
    }
    if (deviceSettingsKeys.contains("autoCorrOptions") || force) {
        swgTestMISettings->setAutoCorrOptions((int) settings.m_sampleSizeIndex);
    }
    if (deviceSettingsKeys.contains("modulation") || force) {
        swgTestMISettings->setModulation((int) settings.m_modulation);
    }
    if (deviceSettingsKeys.contains("modulationTone")) {
        swgTestMISettings->setModulationTone(settings.m_modulationTone);
    }
    if (deviceSettingsKeys.contains("amModulation") || force) {
        swgTestMISettings->setAmModulation(settings.m_amModulation);
    };
    if (deviceSettingsKeys.contains("fmDeviation") || force) {
        swgTestMISettings->setFmDeviation(settings.m_fmDeviation);
    };
    if (deviceSettingsKeys.contains("dcFactor") || force) {
        swgTestMISettings->setDcFactor(settings.m_dcFactor);
    };
    if (deviceSettingsKeys.contains("iFactor") || force) {
        swgTestMISettings->setIFactor(settings.m_iFactor);
    };
    if (deviceSettingsKeys.contains("qFactor") || force) {
        swgTestMISettings->setQFactor(settings.m_qFactor);
    };
    if (deviceSettingsKeys.contains("phaseImbalance") || force) {
        swgTestMISettings->setPhaseImbalance(settings.m_phaseImbalance);
    };
    if (deviceSettingsKeys.contains("fileRecordName") || force) {
        swgTestMISettings->setFileRecordName(new QString(settings.m_fileRecordName));
    }

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer=new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgDeviceSettings->asJson().toUtf8());
    buffer->seek(0);
//    qDebug("TestMI::webapiReverseSendSettings: %s", channelSettingsURL.toStdString().c_str());
//    qDebug("TestMI::webapiReverseSendSettings: query:\n%s", swgDeviceSettings->asJson().toStdString().c_str());

    // Always use PATCH to avoid passing reverse API settings
    m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);

    delete swgDeviceSettings;
}

void TestMI::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("TestSource"));

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/run")
            .arg(m_settings.m_reverseAPIAddress)
            .arg(m_settings.m_reverseAPIPort)
            .arg(m_settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
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
}

void TestMI::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "TestMI::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
        return;
    }

    QString answer = reply->readAll();
    answer.chop(1); // remove last \n
    qDebug("TestMI::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
}
