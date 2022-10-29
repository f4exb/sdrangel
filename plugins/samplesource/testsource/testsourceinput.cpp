///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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
#include <QThread>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"

#include "testsourceinput.h"
#include "device/deviceapi.h"
#include "testsourceworker.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"

MESSAGE_CLASS_DEFINITION(TestSourceInput::MsgConfigureTestSource, Message)
MESSAGE_CLASS_DEFINITION(TestSourceInput::MsgStartStop, Message)


TestSourceInput::TestSourceInput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_testSourceWorker(nullptr),
    m_testSourceWorkerThread(nullptr),
	m_deviceDescription("TestSourceInput"),
	m_running(false),
	m_masterTimer(deviceAPI->getMasterTimer())
{
    m_sampleFifo.setLabel(m_deviceDescription);
    m_deviceAPI->setNbSourceStreams(1);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &TestSourceInput::networkManagerFinished
    );
}

TestSourceInput::~TestSourceInput()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &TestSourceInput::networkManagerFinished
    );
    delete m_networkManager;

    if (m_running) {
        stop();
    }
}

void TestSourceInput::destroy()
{
    delete this;
}

void TestSourceInput::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

bool TestSourceInput::start()
{
	QMutexLocker mutexLocker(&m_mutex);

    if (m_running) {
        return true;
    }

    if (!m_sampleFifo.setSize(96000 * 4))
    {
        qCritical("TestSourceInput::TestSourceInput: Could not allocate SampleFifo");
        return false;
    }

    m_testSourceWorkerThread = new QThread();
    m_testSourceWorker = new TestSourceWorker(&m_sampleFifo);
    m_testSourceWorker->moveToThread(m_testSourceWorkerThread);

    QObject::connect(m_testSourceWorkerThread, &QThread::started, m_testSourceWorker, &TestSourceWorker::startWork);
    QObject::connect(m_testSourceWorkerThread, &QThread::finished, m_testSourceWorker, &QObject::deleteLater);
    QObject::connect(m_testSourceWorkerThread, &QThread::finished, m_testSourceWorkerThread, &QThread::deleteLater);

	m_testSourceWorker->setSamplerate(m_settings.m_sampleRate);
    m_testSourceWorkerThread->start();
	m_running = true;

	mutexLocker.unlock();

	applySettings(m_settings, QList<QString>(), true);

	return true;
}

void TestSourceInput::stop()
{
	QMutexLocker mutexLocker(&m_mutex);
    m_running = false;

    if (m_testSourceWorkerThread)
    {
    	m_testSourceWorker->stopWork();
        m_testSourceWorkerThread->quit();
        m_testSourceWorkerThread->wait();
        m_testSourceWorker = nullptr;
        m_testSourceWorkerThread = nullptr;
    }
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

    MsgConfigureTestSource* message = MsgConfigureTestSource::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureTestSource* messageToGUI = MsgConfigureTestSource::create(m_settings, QList<QString>(), true);
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
	return m_settings.m_sampleRate/(1<<m_settings.m_log2Decim);
}

quint64 TestSourceInput::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void TestSourceInput::setCenterFrequency(qint64 centerFrequency)
{
    TestSourceSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureTestSource* message = MsgConfigureTestSource::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureTestSource* messageToGUI = MsgConfigureTestSource::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool TestSourceInput::handleMessage(const Message& message)
{
    if (MsgConfigureTestSource::match(message))
    {
        MsgConfigureTestSource& conf = (MsgConfigureTestSource&) message;
        qDebug() << "TestSourceInput::handleMessage: MsgConfigureTestSource";

        bool success = applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());

        if (!success)
        {
            qDebug("TestSourceInput::handleMessage: config error");
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "TestSourceInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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

bool TestSourceInput::applySettings(const TestSourceSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "TestSourceInput::applySettings: force:" << force << settings.getDebugString(settingsKeys, force);

    if (settingsKeys.contains("autoCorrOptions") || force)
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

    if (settingsKeys.contains("sampleRate") || force)
    {
        if (m_testSourceWorker != 0)
        {
            m_testSourceWorker->setSamplerate(settings.m_sampleRate);
            qDebug("TestSourceInput::applySettings: sample rate set to %d", settings.m_sampleRate);
        }
    }

    if (settingsKeys.contains("log2Decim") || force)
    {
        if (m_testSourceWorker != 0)
        {
            m_testSourceWorker->setLog2Decimation(settings.m_log2Decim);
            qDebug() << "TestSourceInput::applySettings: set decimation to " << (1<<settings.m_log2Decim);
        }
    }

    if (settingsKeys.contains("centerFrequency")
        || settingsKeys.contains("fcPos")
        || settingsKeys.contains("frequencyShift")
        || settingsKeys.contains("sampleRate")
        || settingsKeys.contains("log2Decim") || force)
    {
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

        if (m_testSourceWorker != 0)
        {
            m_testSourceWorker->setFcPos((int) settings.m_fcPos);
            m_testSourceWorker->setFrequencyShift(frequencyShift);
            qDebug() << "TestSourceInput::applySettings:"
                    << " center freq: " << settings.m_centerFrequency << " Hz"
                    << " device center freq: " << deviceCenterFrequency << " Hz"
                    << " device sample rate: " << devSampleRate << "Hz"
                    << " Actual sample rate: " << devSampleRate/(1<<m_settings.m_log2Decim) << "Hz"
                    << " f shift: " << settings.m_frequencyShift;
        }
    }

    if (settingsKeys.contains("amplitudeBits") || force)
    {
        if (m_testSourceWorker != 0) {
            m_testSourceWorker->setAmplitudeBits(settings.m_amplitudeBits);
        }
    }

    if (settingsKeys.contains("dcFactor") || force)
    {
        if (m_testSourceWorker != 0) {
            m_testSourceWorker->setDCFactor(settings.m_dcFactor);
        }
    }

    if (settingsKeys.contains("iFactor") || force)
    {
        if (m_testSourceWorker != 0) {
            m_testSourceWorker->setIFactor(settings.m_iFactor);
        }
    }

    if (settingsKeys.contains("qFactor") || force)
    {
        if (m_testSourceWorker != 0) {
            m_testSourceWorker->setQFactor(settings.m_qFactor);
        }
    }

    if (settingsKeys.contains("phaseImbalance") || force)
    {
        if (m_testSourceWorker != 0) {
            m_testSourceWorker->setPhaseImbalance(settings.m_phaseImbalance);
        }
    }

    if (settingsKeys.contains("sampleSizeIndex") || force)
    {
        if (m_testSourceWorker != 0) {
            m_testSourceWorker->setBitSize(settings.m_sampleSizeIndex);
        }
    }

    // if ((m_settings.m_sampleRate != settings.m_sampleRate)
    //     || (m_settings.m_centerFrequency != settings.m_centerFrequency)
    //     || (m_settings.m_log2Decim != settings.m_log2Decim)
    //     || (m_settings.m_fcPos != settings.m_fcPos) || force)
    if (settingsKeys.contains("sampleRate")
        || settingsKeys.contains("centerFrequency")
        || settingsKeys.contains("log2Decim")
        || settingsKeys.contains("fcPos") || force)
    {
        int sampleRate = settings.m_sampleRate/(1<<settings.m_log2Decim);
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, settings.m_centerFrequency);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    if (settingsKeys.contains("modulationTone") || force)
    {
        if (m_testSourceWorker != 0) {
            m_testSourceWorker->setToneFrequency(settings.m_modulationTone * 10);
        }
    }

    if (settingsKeys.contains("modulation") || force)
    {
        if (m_testSourceWorker != 0)
        {
            m_testSourceWorker->setModulation(settings.m_modulation);

            if (settings.m_modulation == TestSourceSettings::ModulationPattern0) {
                m_testSourceWorker->setPattern0();
            } else if (settings.m_modulation == TestSourceSettings::ModulationPattern1) {
                m_testSourceWorker->setPattern1();
            } else if (settings.m_modulation == TestSourceSettings::ModulationPattern2) {
                m_testSourceWorker->setPattern2();
            }
        }
    }

    if (settingsKeys.contains("amModulation") || force)
    {
        if (m_testSourceWorker != 0) {
            m_testSourceWorker->setAMModulation(settings.m_amModulation / 100.0f);
        }
    }

    if (settingsKeys.contains("fmDeviation") || force)
    {
        if (m_testSourceWorker != 0) {
            m_testSourceWorker->setFMDeviation(settings.m_fmDeviation * 100.0f);
        }
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

    return true;
}

int TestSourceInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int TestSourceInput::webapiRun(
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

int TestSourceInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setTestSourceSettings(new SWGSDRangel::SWGTestSourceSettings());
    response.getTestSourceSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int TestSourceInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    TestSourceSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureTestSource *msg = MsgConfigureTestSource::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureTestSource *msgToGUI = MsgConfigureTestSource::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void TestSourceInput::webapiUpdateDeviceSettings(
    TestSourceSettings& settings,
    const QStringList& deviceSettingsKeys,
    SWGSDRangel::SWGDeviceSettings& response)
{
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
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getTestSourceSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getTestSourceSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getTestSourceSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getTestSourceSettings()->getReverseApiDeviceIndex();
    }
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

    response.getTestSourceSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getTestSourceSettings()->getReverseApiAddress()) {
        *response.getTestSourceSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getTestSourceSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getTestSourceSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getTestSourceSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

void TestSourceInput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const TestSourceSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("TestSource"));
    swgDeviceSettings->setTestSourceSettings(new SWGSDRangel::SWGTestSourceSettings());
    SWGSDRangel::SWGTestSourceSettings *swgTestSourceSettings = swgDeviceSettings->getTestSourceSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        swgTestSourceSettings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("frequencyShift") || force) {
        swgTestSourceSettings->setFrequencyShift(settings.m_frequencyShift);
    }
    if (deviceSettingsKeys.contains("sampleRate") || force) {
        swgTestSourceSettings->setSampleRate(settings.m_sampleRate);
    }
    if (deviceSettingsKeys.contains("log2Decim") || force) {
        swgTestSourceSettings->setLog2Decim(settings.m_log2Decim);
    }
    if (deviceSettingsKeys.contains("fcPos") || force) {
        swgTestSourceSettings->setFcPos((int) settings.m_fcPos);
    }
    if (deviceSettingsKeys.contains("sampleSizeIndex") || force) {
        swgTestSourceSettings->setSampleSizeIndex(settings.m_sampleSizeIndex);
    }
    if (deviceSettingsKeys.contains("amplitudeBits") || force) {
        swgTestSourceSettings->setAmplitudeBits(settings.m_amplitudeBits);
    }
    if (deviceSettingsKeys.contains("autoCorrOptions") || force) {
        swgTestSourceSettings->setAutoCorrOptions((int) settings.m_sampleSizeIndex);
    }
    if (deviceSettingsKeys.contains("modulation") || force) {
        swgTestSourceSettings->setModulation((int) settings.m_modulation);
    }
    if (deviceSettingsKeys.contains("modulationTone")) {
        swgTestSourceSettings->setModulationTone(settings.m_modulationTone);
    }
    if (deviceSettingsKeys.contains("amModulation") || force) {
        swgTestSourceSettings->setAmModulation(settings.m_amModulation);
    };
    if (deviceSettingsKeys.contains("fmDeviation") || force) {
        swgTestSourceSettings->setFmDeviation(settings.m_fmDeviation);
    };
    if (deviceSettingsKeys.contains("dcFactor") || force) {
        swgTestSourceSettings->setDcFactor(settings.m_dcFactor);
    };
    if (deviceSettingsKeys.contains("iFactor") || force) {
        swgTestSourceSettings->setIFactor(settings.m_iFactor);
    };
    if (deviceSettingsKeys.contains("qFactor") || force) {
        swgTestSourceSettings->setQFactor(settings.m_qFactor);
    };
    if (deviceSettingsKeys.contains("phaseImbalance") || force) {
        swgTestSourceSettings->setPhaseImbalance(settings.m_phaseImbalance);
    };

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgDeviceSettings->asJson().toUtf8());
    buffer->seek(0);
//    qDebug("TestSourceInput::webapiReverseSendSettings: %s", channelSettingsURL.toStdString().c_str());
//    qDebug("TestSourceInput::webapiReverseSendSettings: query:\n%s", swgDeviceSettings->asJson().toStdString().c_str());

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);

    delete swgDeviceSettings;
}

void TestSourceInput::webapiReverseSendStartStop(bool start)
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

void TestSourceInput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "TestSourceInput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("TestSourceInput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
