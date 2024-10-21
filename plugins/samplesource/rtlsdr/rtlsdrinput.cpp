///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2014-2015 John Greb <hexameron@spam.no>                         //
// Copyright (C) 2015-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2018 beta-tester <alpha-beta-release@gmx.net>                   //
// Copyright (C) 2022-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
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
#include <QBuffer>

#include "SWGDeviceSettings.h"
#include "SWGRtlSdrSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGRtlSdrReport.h"

#include "rtlsdrinput.h"
#include "device/deviceapi.h"
#include "rtlsdrthread.h"
#include "dsp/dspcommands.h"
#ifdef ANDROID
#include "util/android.h"
#endif

MESSAGE_CLASS_DEFINITION(RTLSDRInput::MsgConfigureRTLSDR, Message)
MESSAGE_CLASS_DEFINITION(RTLSDRInput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(RTLSDRInput::MsgSaveReplay, Message)

const quint64 RTLSDRInput::frequencyLowRangeMin = 0UL;
const quint64 RTLSDRInput::frequencyLowRangeMax = 275000UL;
const quint64 RTLSDRInput::frequencyHighRangeMax = 2400000UL;
const int RTLSDRInput::sampleRateLowRangeMin = 225001;
const int RTLSDRInput::sampleRateLowRangeMax = 300000;
const int RTLSDRInput::sampleRateHighRangeMin = 900001;
const int RTLSDRInput::sampleRateHighRangeMax = 3200000;

RTLSDRInput::RTLSDRInput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_dev(nullptr),
	m_rtlSDRThread(nullptr),
	m_deviceDescription("RTLSDR"),
    m_tunerType(RTLSDR_TUNER_UNKNOWN),
	m_running(false)
{
    m_sampleFifo.setLabel(m_deviceDescription);
    openDevice();

    m_deviceAPI->setNbSourceStreams(1);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &RTLSDRInput::networkManagerFinished
    );
}

RTLSDRInput::~RTLSDRInput()
{
    qDebug("RTLSDRInput::~RTLSDRInput");
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &RTLSDRInput::networkManagerFinished
    );
    delete m_networkManager;

    if (m_running) {
        stop();
    }

    closeDevice();
}

void RTLSDRInput::destroy()
{
    delete this;
}

bool RTLSDRInput::openDevice()
{
    if (m_dev != nullptr)
    {
        closeDevice();
    }

    char vendor[256];
    char product[256];
    char serial[256];
    int numberOfGains;

    if (!m_sampleFifo.setSize(96000 * 4))
    {
        qCritical("RTLSDRInput::openDevice: Could not allocate SampleFifo");
        return false;
    }

    m_sampleFifo.setWrittenSignalRateDivider(32);

    int device;

#ifdef ANDROID
    int fd;
    if ((fd = Android::openUSBDevice(m_deviceAPI->getSamplingDeviceSerial())) < 0)
    {
        qCritical("RTLSDRInput::openDevice: could not open USB device %s", qPrintable(m_deviceAPI->getSamplingDeviceSerial()));
        return false;
    }
    if ((rtlsdr_open_fd(&m_dev, fd)) < 0)
    {
        qCritical("RTLSDRInput::openDevice: could not open RTLSDR: %s", strerror(errno));
        return false;
    }
#else
    if ((device = rtlsdr_get_index_by_serial(qPrintable(m_deviceAPI->getSamplingDeviceSerial()))) < 0)
    {
        qCritical("RTLSDRInput::openDevice: could not get RTLSDR serial number");
        return false;
    }

    if ((rtlsdr_open(&m_dev, device)) < 0)
    {
        qCritical("RTLSDRInput::openDevice: could not open RTLSDR #%d: %s", device, strerror(errno));
        return false;
    }
#endif

    vendor[0] = '\0';
    product[0] = '\0';
    serial[0] = '\0';

    if ((rtlsdr_get_usb_strings(m_dev, vendor, product, serial)) < 0)
    {
        qCritical("RTLSDRInput::openDevice: error accessing USB device");
        stop();
        return false;
    }

    m_tunerType = rtlsdr_get_tuner_type(m_dev);

    qInfo("RTLSDRInput::openDevice: open: %s %s, SN: %s Tuner: %s", vendor, product, serial, qPrintable(getTunerName()));
    m_deviceDescription = QString("%1 (SN %2)").arg(product).arg(serial);

    if ((vendor == QStringLiteral("RTLSDRBlog")) && (product == QStringLiteral("Blog V4"))) {
        m_frequencyHighRangeMin = 0;
    } else {
        m_frequencyHighRangeMin = 24000UL;
    }

    if ((rtlsdr_set_sample_rate(m_dev, 1152000)) < 0)
    {
        qCritical("RTLSDRInput::openDevice: could not set sample rate: 1024k S/s");
        stop();
        return false;
    }

    if ((rtlsdr_set_tuner_gain_mode(m_dev, 1)) < 0)
    {
        qCritical("RTLSDRInput::openDevice: error setting tuner gain mode");
        stop();
        return false;
    }

    if ((rtlsdr_set_agc_mode(m_dev, 0)) < 0)
    {
        qCritical("RTLSDRInput::openDevice: error setting agc mode");
        stop();
        return false;
    }

    numberOfGains = rtlsdr_get_tuner_gains(m_dev, nullptr);

    if (numberOfGains < 0)
    {
        qCritical("RTLSDRInput::openDevice: error getting number of gain values supported");
        stop();
        return false;
    }

    m_gains.resize(numberOfGains);

    if (rtlsdr_get_tuner_gains(m_dev, &m_gains[0]) < 0)
    {
        qCritical("RTLSDRInput::openDevice: error getting gain values");
        stop();
        return false;
    }
    else
    {
        qDebug() << "RTLSDRInput::openDevice: " << m_gains.size() << "gains";
    }

    if ((rtlsdr_reset_buffer(m_dev)) < 0)
    {
        qCritical("RTLSDRInput::openDevice: could not reset USB EP buffers: %s", strerror(errno));
        stop();
        return false;
    }

    return true;
}

void RTLSDRInput::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

bool RTLSDRInput::start()
{
	QMutexLocker mutexLocker(&m_mutex);

    if (m_running) {
        return true;
    }

	if (!m_dev) {
        return false;
	}

    qDebug("RTLSDRInput::start");

    m_rtlSDRThread = new RTLSDRThread(m_dev, &m_sampleFifo, &m_replayBuffer, m_settings);
    connect(m_rtlSDRThread, &QThread::finished, m_rtlSDRThread, &QObject::deleteLater);
	m_rtlSDRThread->startWork();
	m_running = true;

	mutexLocker.unlock();

	applySettings(m_settings, QList<QString>(), true);

	return true;
}

void RTLSDRInput::closeDevice()
{
    if (m_dev != nullptr)
    {
        rtlsdr_close(m_dev);
        m_dev = nullptr;
    }

    m_deviceDescription.clear();
}

void RTLSDRInput::stop()
{
	QMutexLocker mutexLocker(&m_mutex);

    if (!m_running) {
        return;
    }

    qDebug("RTLSDRInput::stop");

	if (m_rtlSDRThread)
	{
		m_rtlSDRThread->stopWork();
		m_rtlSDRThread = nullptr; // Deleted automatically when thread finishes
	}

	m_running = false;
}

QByteArray RTLSDRInput::serialize() const
{
    return m_settings.serialize();
}

bool RTLSDRInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureRTLSDR* message = MsgConfigureRTLSDR::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureRTLSDR* messageToGUI = MsgConfigureRTLSDR::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& RTLSDRInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int RTLSDRInput::getSampleRate() const
{
	int rate = m_settings.m_devSampleRate;
	return (rate / (1<<m_settings.m_log2Decim));
}

quint64 RTLSDRInput::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void RTLSDRInput::setCenterFrequency(qint64 centerFrequency)
{
    RTLSDRSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureRTLSDR* message = MsgConfigureRTLSDR::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureRTLSDR* messageToGUI = MsgConfigureRTLSDR::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool RTLSDRInput::handleMessage(const Message& message)
{
    if (MsgConfigureRTLSDR::match(message))
    {
        auto& conf = (const MsgConfigureRTLSDR&) message;
        qDebug() << "RTLSDRInput::handleMessage: MsgConfigureRTLSDR";

        bool success = applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());

        if (!success)
        {
            qDebug("RTLSDRInput::handleMessage: config error");
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        auto& cmd = (const MsgStartStop&) message;
        qDebug() << "RTLSDRInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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
    else if (MsgSaveReplay::match(message))
    {
        auto& cmd = (const MsgSaveReplay&) message;
        m_replayBuffer.save(cmd.getFilename(), m_settings.m_devSampleRate, getCenterFrequency());
        return true;
    }
    else
    {
        return false;
    }
}

bool RTLSDRInput::applySettings(const RTLSDRSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "RTLSDRInput::applySettings: force: " << force << settings.getDebugString(settingsKeys, force);
    bool forwardChange = false;

    if (settingsKeys.contains("dcBlock") || settingsKeys.contains("iqImbalance") || force)
    {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqImbalance);
        qDebug("RTLSDRInput::applySettings: corrections: DC block: %s IQ imbalance: %s",
                settings.m_dcBlock ? "true" : "false",
                settings.m_iqImbalance ? "true" : "false");
    }

    if (settingsKeys.contains("devSampleRate") || force)
    {
        if (settings.m_devSampleRate != m_settings.m_devSampleRate) {
            m_replayBuffer.clear();
        }
    }

    if (settingsKeys.contains("centerFrequency")
        || settingsKeys.contains("fcPos")
        || settingsKeys.contains("log2Decim")
        || settingsKeys.contains("devSampleRate")
        || settingsKeys.contains("transverterMode")
        || settingsKeys.contains("transverterDeltaFrequency")
        || force)
    {
        forwardChange = true;
    }

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = (settingsKeys.contains("useReverseAPI") && settings.m_useReverseAPI) ||
            settingsKeys.contains("reverseAPIAddress") ||
            settingsKeys.contains("reverseAPIPort") ||
            settingsKeys.contains("reverseAPIDeviceIndex");
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    if (m_rtlSDRThread)
    {
        MsgConfigureRTLSDR* message = MsgConfigureRTLSDR::create(settings, settingsKeys, force);
        m_rtlSDRThread->getInputMessageQueue()->push(message);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    if (settingsKeys.contains("replayLength") || settingsKeys.contains("devSampleRate") || force) {
        m_replayBuffer.setSize(m_settings.m_replayLength, m_settings.m_devSampleRate);
    }

    if (settingsKeys.contains("replayOffset") || settingsKeys.contains("devSampleRate")  || force) {
        m_replayBuffer.setReadOffset(((unsigned)(m_settings.m_replayOffset * (float) m_settings.m_devSampleRate)) * 2);
    }

    if (settingsKeys.contains("replayLoop") || force) {
        m_replayBuffer.setLoop(m_settings.m_replayLoop);
    }

    if (forwardChange)
    {
        int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2Decim);
        auto *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    return true;
}

QString RTLSDRInput::getTunerName() const
{
    const static QStringList names = {"Unknown", "E4000", "FC0012", "FC0013", "FC2580", "R820T", "R828D"};

    if ((int) m_tunerType <= names.size()) {
        return names[(int) m_tunerType];
    } else {
        return names[0];
    }
}

void RTLSDRInput::set_ds_mode(int on)
{
	rtlsdr_set_direct_sampling(m_dev, on);
}

int RTLSDRInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setRtlSdrSettings(new SWGSDRangel::SWGRtlSdrSettings());
    response.getRtlSdrSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int RTLSDRInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    RTLSDRSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureRTLSDR *msg = MsgConfigureRTLSDR::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureRTLSDR *msgToGUI = MsgConfigureRTLSDR::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void RTLSDRInput::webapiUpdateDeviceSettings(
        RTLSDRSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("agc")) {
        settings.m_agc = response.getRtlSdrSettings()->getAgc() != 0;
    }
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getRtlSdrSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getRtlSdrSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getRtlSdrSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("fcPos")) {
        settings.m_fcPos = (RTLSDRSettings::fcPos_t) response.getRtlSdrSettings()->getFcPos();
    }
    if (deviceSettingsKeys.contains("gain")) {
        settings.m_gain = response.getRtlSdrSettings()->getGain();
    }
    if (deviceSettingsKeys.contains("iqImbalance")) {
        settings.m_iqImbalance = response.getRtlSdrSettings()->getIqImbalance() != 0;
    }
    if (deviceSettingsKeys.contains("loPpmCorrection")) {
        settings.m_loPpmCorrection = response.getRtlSdrSettings()->getLoPpmCorrection();
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getRtlSdrSettings()->getLog2Decim();
    }
    if (deviceSettingsKeys.contains("iqOrder")) {
        settings.m_iqOrder = response.getRtlSdrSettings()->getIqOrder() != 0;
    }
    if (deviceSettingsKeys.contains("lowSampleRate")) {
        settings.m_lowSampleRate = response.getRtlSdrSettings()->getLowSampleRate() != 0;
    }
    if (deviceSettingsKeys.contains("noModMode")) {
        settings.m_noModMode = response.getRtlSdrSettings()->getNoModMode() != 0;
    }
    if (deviceSettingsKeys.contains("offsetTuning")) {
        settings.m_offsetTuning = response.getRtlSdrSettings()->getOffsetTuning() != 0;
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency")) {
        settings.m_transverterDeltaFrequency = response.getRtlSdrSettings()->getTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("transverterMode")) {
        settings.m_transverterMode = response.getRtlSdrSettings()->getTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getRtlSdrSettings()->getRfBandwidth();
    }
    if (deviceSettingsKeys.contains("biasTee")) {
        settings.m_biasTee = response.getRtlSdrSettings()->getBiasTee() != 0;
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getRtlSdrSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getRtlSdrSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = (uint16_t) response.getRtlSdrSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = (uint16_t) response.getRtlSdrSettings()->getReverseApiDeviceIndex();
    }
}

void RTLSDRInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const RTLSDRSettings& settings)
{
    response.getRtlSdrSettings()->setAgc(settings.m_agc ? 1 : 0);
    response.getRtlSdrSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getRtlSdrSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getRtlSdrSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getRtlSdrSettings()->setFcPos((int) settings.m_fcPos);
    response.getRtlSdrSettings()->setGain(settings.m_gain);
    response.getRtlSdrSettings()->setIqImbalance(settings.m_iqImbalance ? 1 : 0);
    response.getRtlSdrSettings()->setLoPpmCorrection(settings.m_loPpmCorrection);
    response.getRtlSdrSettings()->setLog2Decim(settings.m_log2Decim);
    response.getRtlSdrSettings()->setIqOrder(settings.m_iqOrder ? 1 : 0);
    response.getRtlSdrSettings()->setLowSampleRate(settings.m_lowSampleRate ? 1 : 0);
    response.getRtlSdrSettings()->setNoModMode(settings.m_noModMode ? 1 : 0);
    response.getRtlSdrSettings()->setOffsetTuning(settings.m_offsetTuning ? 1 : 0);
    response.getRtlSdrSettings()->setBiasTee(settings.m_biasTee ? 1 : 0);
    response.getRtlSdrSettings()->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    response.getRtlSdrSettings()->setTransverterMode(settings.m_transverterMode ? 1 : 0);
    response.getRtlSdrSettings()->setRfBandwidth(settings.m_rfBandwidth);

    response.getRtlSdrSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getRtlSdrSettings()->getReverseApiAddress()) {
        *response.getRtlSdrSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getRtlSdrSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getRtlSdrSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getRtlSdrSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

int RTLSDRInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int RTLSDRInput::webapiRun(
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

int RTLSDRInput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setRtlSdrReport(new SWGSDRangel::SWGRtlSdrReport());
    response.getRtlSdrReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void RTLSDRInput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response) const
{
    response.getRtlSdrReport()->setGains(new QList<SWGSDRangel::SWGGain*>);

    for (auto it = getGains().begin(); it != getGains().end(); ++it)
    {
        response.getRtlSdrReport()->getGains()->append(new SWGSDRangel::SWGGain);
        response.getRtlSdrReport()->getGains()->back()->setGainCb(*it);
    }

    response.getRtlSdrReport()->setTunerType(new QString(getTunerName()));
}

void RTLSDRInput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const RTLSDRSettings& settings, bool force)
{
    auto *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setDeviceHwType(new QString("RTLSDR"));
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setRtlSdrSettings(new SWGSDRangel::SWGRtlSdrSettings());
    SWGSDRangel::SWGRtlSdrSettings *swgRtlSdrSettings = swgDeviceSettings->getRtlSdrSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("agc") || force) {
        swgRtlSdrSettings->setAgc(settings.m_agc ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        swgRtlSdrSettings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("dcBlock") || force) {
        swgRtlSdrSettings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("devSampleRate") || force) {
        swgRtlSdrSettings->setDevSampleRate(settings.m_devSampleRate);
    }
    if (deviceSettingsKeys.contains("fcPos") || force) {
        swgRtlSdrSettings->setFcPos(settings.m_fcPos);
    }
    if (deviceSettingsKeys.contains("gain") || force) {
        swgRtlSdrSettings->setGain(settings.m_gain);
    }
    if (deviceSettingsKeys.contains("iqImbalance") || force) {
        swgRtlSdrSettings->setIqImbalance(settings.m_iqImbalance ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("loPpmCorrection") || force) {
        swgRtlSdrSettings->setLoPpmCorrection(settings.m_loPpmCorrection);
    }
    if (deviceSettingsKeys.contains("log2Decim") || force) {
        swgRtlSdrSettings->setLog2Decim(settings.m_log2Decim);
    }
    if (deviceSettingsKeys.contains("iqOrder") || force) {
        swgRtlSdrSettings->setIqOrder(settings.m_iqOrder ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("lowSampleRate") || force) {
        swgRtlSdrSettings->setLowSampleRate(settings.m_lowSampleRate);
    }
    if (deviceSettingsKeys.contains("noModMode") || force) {
        swgRtlSdrSettings->setNoModMode(settings.m_noModMode ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("offsetTuning") || force) {
        swgRtlSdrSettings->setOffsetTuning(settings.m_offsetTuning ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency") || force) {
        swgRtlSdrSettings->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("transverterMode") || force) {
        swgRtlSdrSettings->setTransverterMode(settings.m_transverterMode ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("rfBandwidth") || force) {
        swgRtlSdrSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (deviceSettingsKeys.contains("biasTee") || force) {
        swgRtlSdrSettings->setBiasTee(settings.m_biasTee ? 1 : 0);
    }

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    auto *buffer = new QBuffer();
    buffer->open(QBuffer::ReadWrite);
    buffer->write(swgDeviceSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);

    delete swgDeviceSettings;
}

void RTLSDRInput::webapiReverseSendStartStop(bool start)
{
    auto *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setDeviceHwType(new QString("RTLSDR"));
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/run")
            .arg(m_settings.m_reverseAPIAddress)
            .arg(m_settings.m_reverseAPIPort)
            .arg(m_settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    auto *buffer = new QBuffer();
    buffer->open(QBuffer::ReadWrite);
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

void RTLSDRInput::networkManagerFinished(QNetworkReply *reply) const
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
        qDebug("RTLSDRInput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
