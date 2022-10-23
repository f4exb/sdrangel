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

#include <QDebug>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGPerseusReport.h"

#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "device/deviceapi.h"
#include "perseus/deviceperseus.h"

#include "perseusinput.h"
#include "perseusworker.h"

MESSAGE_CLASS_DEFINITION(PerseusInput::MsgConfigurePerseus, Message)
MESSAGE_CLASS_DEFINITION(PerseusInput::MsgStartStop, Message)

PerseusInput::PerseusInput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_deviceDescription("PerseusInput"),
    m_running(false),
    m_perseusWorker(nullptr),
    m_perseusDescriptor(0)
{
    m_sampleFifo.setLabel(m_deviceDescription);
    openDevice();
    m_deviceAPI->setNbSourceStreams(1);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &PerseusInput::networkManagerFinished
    );
}

PerseusInput::~PerseusInput()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &PerseusInput::networkManagerFinished
    );
    delete m_networkManager;
}

void PerseusInput::destroy()
{
    delete this;
}

void PerseusInput::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

bool PerseusInput::start()
{
    QMutexLocker mlock(&m_mutex);

    if (m_running) {
        return true;
    }

    // start / stop streaming is done in the thread.

    m_perseusWorkerThread = new QThread();
    m_perseusWorker = new PerseusWorker(m_perseusDescriptor, &m_sampleFifo);
    m_perseusWorker->moveToThread(m_perseusWorkerThread);
    qDebug("PerseusInput::start: worker created");

    QObject::connect(m_perseusWorkerThread, &QThread::started, m_perseusWorker, &PerseusWorker::startWork);
    QObject::connect(m_perseusWorkerThread, &QThread::finished, m_perseusWorker, &QObject::deleteLater);
    QObject::connect(m_perseusWorkerThread, &QThread::finished, m_perseusWorkerThread, &QThread::deleteLater);

    m_perseusWorker->setIQOrder(m_settings.m_iqOrder);
    m_perseusWorker->setLog2Decimation(m_settings.m_log2Decim);
    m_perseusWorkerThread->start();

    applySettings(m_settings, QList<QString>(), true);
    m_running = true;

    return true;
}

void PerseusInput::stop()
{
    QMutexLocker mlock(&m_mutex);

    if (!m_running) {
        return;
    }

    m_running = false;

    if (m_perseusWorkerThread)
    {
        m_perseusWorkerThread->quit();
        m_perseusWorkerThread->wait();
        m_perseusWorker = nullptr;
        m_perseusWorkerThread = nullptr;
    }

}

QByteArray PerseusInput::serialize() const
{
    return m_settings.serialize();
}

bool PerseusInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigurePerseus* message = MsgConfigurePerseus::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigurePerseus* messageToGUI = MsgConfigurePerseus::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& PerseusInput::getDeviceDescription() const
{
    return m_deviceDescription;
}
int PerseusInput::getSampleRate() const
{
    if (m_settings.m_devSampleRateIndex < m_sampleRates.size()) {
        return (m_sampleRates[m_settings.m_devSampleRateIndex] / (1<<m_settings.m_log2Decim));
    } else {
        return (m_sampleRates[0] / (1<<m_settings.m_log2Decim));
    }
}

quint64 PerseusInput::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void PerseusInput::setCenterFrequency(qint64 centerFrequency)
{
    PerseusSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigurePerseus* message = MsgConfigurePerseus::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigurePerseus* messageToGUI = MsgConfigurePerseus::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool PerseusInput::handleMessage(const Message& message)
{
    if (MsgConfigurePerseus::match(message))
    {
        MsgConfigurePerseus& conf = (MsgConfigurePerseus&) message;
        qDebug() << "PerseusInput::handleMessage: MsgConfigurePerseus";

        bool success = applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());

        if (!success) {
            qDebug("MsgConfigurePerseus::handleMessage: Perseus config error");
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "PerseusInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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

bool PerseusInput::openDevice()
{
    if (m_perseusDescriptor != 0) {
        closeDevice();
    }

    if (!m_sampleFifo.setSize(PERSEUS_NBSAMPLES))
    {
        qCritical("PerseusInput::start: could not allocate SampleFifo");
        return false;
    }

    m_deviceAPI->getSamplingDeviceSerial();
    int deviceSequence = DevicePerseus::instance().getSequenceFromSerial(m_deviceAPI->getSamplingDeviceSerial().toStdString());

    if ((m_perseusDescriptor = perseus_open(deviceSequence)) == 0)
    {
        qCritical("PerseusInput::openDevice: cannot open device: %s", perseus_errorstr());
        return false;
    }

    int buf[32];
    m_sampleRates.clear();

    if (perseus_get_sampling_rates(m_perseusDescriptor, buf, sizeof(buf)/sizeof(buf[0])) < 0)
    {
        qCritical("PerseusInput::openDevice: cannot get sampling rates: %s", perseus_errorstr());
        perseus_close(m_perseusDescriptor);
        return false;
    }
    else
    {
         for (int i = 0; (i < 32) && (buf[i] != 0); i++)
         {
             qDebug("PerseusInput::openDevice: sample rate: %d", buf[i]);
             m_sampleRates.push_back(buf[i]);
         }
    }

    return true;
}

void PerseusInput::closeDevice()
{
    if (m_perseusDescriptor)
    {
        if (m_running) { stop(); }
        perseus_close(m_perseusDescriptor);
    }
}

void PerseusInput::setDeviceCenterFrequency(quint64 freq_hz, const PerseusSettings& settings)
{
    qint64 df = ((qint64)freq_hz * settings.m_LOppmTenths) / 10000000LL;
    freq_hz += df;

    // wideband flag is inverted since parameter is set to enable preselection filters
    int rc = perseus_set_ddc_center_freq(m_perseusDescriptor, freq_hz, settings.m_wideBand ? 0 : 1);

    if (rc < 0) {
        qWarning("PerseusInput::setDeviceCenterFrequency: could not set frequency to %llu Hz: %s", freq_hz, perseus_errorstr());
    } else {
        qDebug("PerseusInput::setDeviceCenterFrequency: frequency set to %llu Hz", freq_hz);
    }
}

bool PerseusInput::applySettings(const PerseusSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "PerseusInput::applySettings: force: " << force << settings.getDebugString(settingsKeys, force);
    bool forwardChange = false;
    int sampleRateIndex = settings.m_devSampleRateIndex;

    if (settingsKeys.contains("devSampleRateIndex") || force)
    {
        forwardChange = true;

        if (settings.m_devSampleRateIndex >= m_sampleRates.size()) {
            sampleRateIndex = m_sampleRates.size() - 1;
        }

        if (m_perseusDescriptor != 0)
        {
            int rate = m_sampleRates[settings.m_devSampleRateIndex < m_sampleRates.size() ? settings.m_devSampleRateIndex: 0];
            int rc;

            for (int i = 0; i < 2; i++) // it turns out that it has to be done twice
            {
                rc = perseus_set_sampling_rate(m_perseusDescriptor, rate);

                if (rc < 0) {
                    qCritical("PerseusInput::applySettings: could not set sample rate index %u (%d S/s): %s",
                            settings.m_devSampleRateIndex, rate, perseus_errorstr());
                    break;
                }
                else
                {
                    qDebug("PerseusInput::applySettings: sample rate set to index #%d: %u (%d S/s)",
                            i, settings.m_devSampleRateIndex, rate);
                }
            }
        }
    }

    if (settingsKeys.contains("log2Decim") || force)
    {
        forwardChange = true;

        if (m_running)
        {
            m_perseusWorker->setLog2Decimation(settings.m_log2Decim);
            qDebug("PerseusInput: set decimation to %d", (1<<settings.m_log2Decim));
        }
    }

    if (settingsKeys.contains("iqOrder") || force)
    {
        if (m_running) {
            m_perseusWorker->setIQOrder(settings.m_iqOrder);
        }
    }

    if (force || settingsKeys.contains("centerFrequency")
            || settingsKeys.contains("LOppmTenths")
            || settingsKeys.contains("wideBand")
            || settingsKeys.contains("transverterMode")
            || settingsKeys.contains("transverterDeltaFrequency")
            || settingsKeys.contains("devSampleRateIndex"))
    {
        qint64 deviceCenterFrequency = settings.m_centerFrequency;
        deviceCenterFrequency -= settings.m_transverterMode ? settings.m_transverterDeltaFrequency : 0;
        deviceCenterFrequency = deviceCenterFrequency < 0 ? 0 : deviceCenterFrequency;

        if (m_perseusDescriptor != 0)
        {
            setDeviceCenterFrequency(deviceCenterFrequency, settings);
            qDebug("PerseusInput::applySettings: center freq: %llu Hz", settings.m_centerFrequency);
        }

        forwardChange = true;
    }

    if (settingsKeys.contains("attenuator") || force)
    {
        int rc = perseus_set_attenuator_n(m_perseusDescriptor, (int) settings.m_attenuator);

        if (rc < 0) {
            qWarning("PerseusInput::applySettings: cannot set attenuator to %d dB: %s", (int) settings.m_attenuator*10, perseus_errorstr());
        } else {
            qDebug("PerseusInput::applySettings: attenuator set to %d dB", (int) settings.m_attenuator*10);
        }
    }

    if (settingsKeys.contains("adcDither")
       || settingsKeys.contains("adcPreamp") || force)
    {
        int rc = perseus_set_adc(m_perseusDescriptor, settings.m_adcDither ? 1 : 0, settings.m_adcPreamp ? 1 : 0);

        if (rc < 0) {
            qWarning("PerseusInput::applySettings: cannot set ADC to dither %s and preamp %s: %s",
                    settings.m_adcDither ? "on" : "off", settings.m_adcPreamp ? "on" : "off", perseus_errorstr());
        } else {
            qDebug("PerseusInput::applySettings: ADC set to dither %s and preamp %s",
                    settings.m_adcDither ? "on" : "off", settings.m_adcPreamp ? "on" : "off");
        }
    }

    if (forwardChange)
    {
        int sampleRate = m_sampleRates[sampleRateIndex]/(1<<settings.m_log2Decim);
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, settings.m_centerFrequency);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
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

    m_settings.m_devSampleRateIndex = sampleRateIndex;

    return true;
}

int PerseusInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int PerseusInput::webapiRun(
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

int PerseusInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setPerseusSettings(new SWGSDRangel::SWGPerseusSettings());
    response.getPerseusSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int PerseusInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    PerseusSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigurePerseus *msg = MsgConfigurePerseus::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigurePerseus *msgToGUI = MsgConfigurePerseus::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void PerseusInput::webapiUpdateDeviceSettings(
    PerseusSettings& settings,
    const QStringList& deviceSettingsKeys,
    SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getPerseusSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("LOppmTenths")) {
        settings.m_LOppmTenths = response.getPerseusSettings()->getLOppmTenths();
    }
    if (deviceSettingsKeys.contains("devSampleRateIndex")) {
        settings.m_devSampleRateIndex = response.getPerseusSettings()->getDevSampleRateIndex();
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getPerseusSettings()->getLog2Decim();
    }
    if (deviceSettingsKeys.contains("iqOrder")) {
        settings.m_iqOrder = response.getPerseusSettings()->getIqOrder() != 0;
    }
    if (deviceSettingsKeys.contains("adcDither")) {
        settings.m_adcDither = response.getPerseusSettings()->getAdcDither() != 0;
    }
    if (deviceSettingsKeys.contains("adcPreamp")) {
        settings.m_adcPreamp = response.getPerseusSettings()->getAdcPreamp() != 0;
    }
    if (deviceSettingsKeys.contains("wideBand")) {
        settings.m_wideBand = response.getPerseusSettings()->getWideBand() != 0;
    }
    if (deviceSettingsKeys.contains("attenuator")) {
        int attenuator = response.getPerseusSettings()->getAttenuator();
        attenuator = attenuator < 0 ? 0 : attenuator > 3 ? 3 : attenuator;
        settings.m_attenuator = (PerseusSettings::Attenuator) attenuator;
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency")) {
        settings.m_transverterDeltaFrequency = response.getPerseusSettings()->getTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("transverterMode")) {
        settings.m_transverterMode = response.getPerseusSettings()->getTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getPerseusSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getPerseusSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getPerseusSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getPerseusSettings()->getReverseApiDeviceIndex();
    }
}

int PerseusInput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setPerseusReport(new SWGSDRangel::SWGPerseusReport());
    response.getPerseusReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void PerseusInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const PerseusSettings& settings)
{
    response.getPerseusSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getPerseusSettings()->setLOppmTenths(settings.m_LOppmTenths);
    response.getPerseusSettings()->setDevSampleRateIndex(settings.m_devSampleRateIndex);
    response.getPerseusSettings()->setLog2Decim(settings.m_log2Decim);
    response.getPerseusSettings()->setIqOrder(settings.m_iqOrder ? 1 : 0);
    response.getPerseusSettings()->setAdcDither(settings.m_adcDither ? 1 : 0);
    response.getPerseusSettings()->setAdcPreamp(settings.m_adcPreamp ? 1 : 0);
    response.getPerseusSettings()->setWideBand(settings.m_wideBand ? 1 : 0);
    response.getPerseusSettings()->setAttenuator((int) settings.m_attenuator);
    response.getPerseusSettings()->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    response.getPerseusSettings()->setTransverterMode(settings.m_transverterMode ? 1 : 0);

    response.getPerseusSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getPerseusSettings()->getReverseApiAddress()) {
        *response.getPerseusSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getPerseusSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getPerseusSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getPerseusSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

void PerseusInput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    response.getPerseusReport()->setSampleRates(new QList<SWGSDRangel::SWGSampleRate*>);

    for (std::vector<uint32_t>::const_iterator it = getSampleRates().begin(); it != getSampleRates().end(); ++it)
    {
        response.getPerseusReport()->getSampleRates()->append(new SWGSDRangel::SWGSampleRate);
        response.getPerseusReport()->getSampleRates()->back()->setRate(*it);
    }
}

void PerseusInput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const PerseusSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("Perseus"));
    swgDeviceSettings->setPerseusSettings(new SWGSDRangel::SWGPerseusSettings());
    SWGSDRangel::SWGPerseusSettings *swgPerseusSettings = swgDeviceSettings->getPerseusSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        swgPerseusSettings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("LOppmTenths") || force) {
        swgPerseusSettings->setLOppmTenths(settings.m_LOppmTenths);
    }
    if (deviceSettingsKeys.contains("devSampleRateIndex") || force) {
        swgPerseusSettings->setDevSampleRateIndex(settings.m_devSampleRateIndex);
    }
    if (deviceSettingsKeys.contains("log2Decim") || force) {
        swgPerseusSettings->setLog2Decim(settings.m_log2Decim);
    }
    if (deviceSettingsKeys.contains("iqOrder") || force) {
        swgPerseusSettings->setIqOrder(settings.m_iqOrder ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("adcDither") || force) {
        swgPerseusSettings->setAdcDither(settings.m_adcDither ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("adcPreamp") || force) {
        swgPerseusSettings->setAdcPreamp(settings.m_adcPreamp ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("wideBand") || force) {
        swgPerseusSettings->setWideBand(settings.m_wideBand ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("attenuator") || force) {
        swgPerseusSettings->setAttenuator((int) settings.m_attenuator);
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency") || force) {
        swgPerseusSettings->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("transverterMode") || force) {
        swgPerseusSettings->setTransverterMode(settings.m_transverterMode ? 1 : 0);
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

void PerseusInput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("Perseus"));

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

void PerseusInput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "PerseusInput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("PerseusInput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
