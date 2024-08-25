///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
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

#include <QThread>
#include <QDebug>
#include <QBuffer>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"

#include "device/deviceapi.h"
#include "dsp/hbfilterchainconverter.h"
#include "dsp/dspcommands.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/devicesamplesource.h"
#include "dsp/dspengine.h"
#include "device/deviceset.h"
#include "maincore.h"

#include "interferometerbaseband.h"
#include "interferometer.h"

MESSAGE_CLASS_DEFINITION(Interferometer::MsgConfigureInterferometer, Message)
MESSAGE_CLASS_DEFINITION(Interferometer::MsgBasebandNotification, Message)
MESSAGE_CLASS_DEFINITION(Interferometer::MsgReportDevices, Message)

const char* const Interferometer::m_channelIdURI = "sdrangel.channel.interferometer";
const char* const Interferometer::m_channelId = "Interferometer";
const int Interferometer::m_fftSize = 4096;

Interferometer::Interferometer(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamMIMO),
    m_deviceAPI(deviceAPI),
    m_spectrumVis(SDR_RX_SCALEF),
    m_thread(nullptr),
    m_basebandSink(nullptr),
    m_running(false),
    m_guiMessageQueue(nullptr),
    m_centerFrequency(0),
    m_frequencyOffset(0),
    m_deviceSampleRate(48000)
{
    setObjectName(m_channelId);

    m_deviceAPI->addMIMOChannel(this);
    m_deviceAPI->addMIMOChannelAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &Interferometer::networkManagerFinished
    );
    // Update device list when devices are added or removed
    QObject::connect(
        MainCore::instance(),
        &MainCore::deviceSetAdded,
        this,
        &Interferometer::updateDeviceSetList
    );
    QObject::connect(
        MainCore::instance(),
        &MainCore::deviceSetRemoved,
        this,
        &Interferometer::updateDeviceSetList
    );
    updateDeviceSetList();
    Interferometer::startSinks();
}

Interferometer::~Interferometer()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &Interferometer::networkManagerFinished
    );
    delete m_networkManager;

    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeMIMOChannel(this);
    Interferometer::stopSinks();
}

void Interferometer::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI != m_deviceAPI)
    {
        m_deviceAPI->removeChannelSinkAPI(this);
        m_deviceAPI->removeMIMOChannel(this);
        m_deviceAPI = deviceAPI;
        m_deviceAPI->addMIMOChannel(this);
        m_deviceAPI->addChannelSinkAPI(this);
    }
}

void Interferometer::startSinks()
{
    QMutexLocker mlock(&m_mutex);

    if (m_running) {
        return;
    }

    qDebug("Interferometer::startSinks");
    m_thread = new QThread(this);
    m_basebandSink = new InterferometerBaseband(m_fftSize);
    m_basebandSink->setSpectrumSink(&m_spectrumVis);
    m_basebandSink->setScopeSink(&m_scopeSink);
    m_basebandSink->moveToThread(m_thread);

    QObject::connect(m_thread, &QThread::finished, m_basebandSink, &QObject::deleteLater);
    QObject::connect(m_thread, &QThread::finished, m_thread, &QThread::deleteLater);

    if (m_deviceSampleRate != 0) {
        m_basebandSink->setBasebandSampleRate(m_deviceSampleRate);
    }

    m_basebandSink->reset();
    m_thread->start();
    m_running = true;
    mlock.unlock();

    InterferometerBaseband::MsgConfigureChannelizer *msg = InterferometerBaseband::MsgConfigureChannelizer::create(
        m_settings.m_log2Decim, m_settings.m_filterChainHash);
    m_basebandSink->getInputMessageQueue()->push(msg);

    DeviceSampleSource *deviceSource = getLocalDevice(m_settings.m_localDeviceIndex);
    InterferometerBaseband::MsgConfigureLocalDeviceSampleSource *msgDevice =
        InterferometerBaseband::MsgConfigureLocalDeviceSampleSource::create(deviceSource);
    m_basebandSink->getInputMessageQueue()->push(msgDevice);
}

void Interferometer::stopSinks()
{
    QMutexLocker mlock(&m_mutex);

    if (!m_running) {
        return;
    }

    qDebug("Interferometer::stopSinks");
    m_running = false;
	m_thread->exit();
	m_thread->wait();
    m_basebandSink = nullptr;
    m_thread = nullptr;
}

void Interferometer::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, unsigned int sinkIndex)
{
    if (m_running) {
        m_basebandSink->feed(begin, end, sinkIndex);
    }
}

void Interferometer::pull(SampleVector::iterator& begin, unsigned int nbSamples, unsigned int sourceIndex)
{
    (void) begin;
    (void) nbSamples;
    (void) sourceIndex;
}

void Interferometer::applySettings(const InterferometerSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "Interferometer::applySettings:" << settings.getDebugString(settingsKeys, force) << "force: " << force;

    if (m_running && (settingsKeys.contains("log2Decim")
        || settingsKeys.contains("filterChainHash") || force))
    {
        InterferometerBaseband::MsgConfigureChannelizer *msg = InterferometerBaseband::MsgConfigureChannelizer::create(
            settings.m_log2Decim, settings.m_filterChainHash);
        m_basebandSink->getInputMessageQueue()->push(msg);
        calculateFrequencyOffset(settings.m_log2Decim, settings.m_filterChainHash);
        propagateSampleRateAndFrequency(m_settings.m_localDeviceIndex, settings.m_log2Decim);
    }

    if (m_running && ((settingsKeys.contains("correlationType")) || force))
    {
        InterferometerBaseband::MsgConfigureCorrelation *msg = InterferometerBaseband::MsgConfigureCorrelation::create(
            settings.m_correlationType);
        m_basebandSink->getInputMessageQueue()->push(msg);
    }

    if (m_running && ((settingsKeys.contains("phase")) || force)) {
        m_basebandSink->setPhase(settings.m_phase);
    }

    if (m_running && ((settingsKeys.contains("gain")) || force)) {
        m_basebandSink->setGain(settings.m_gain);
    }

    if (settingsKeys.contains("localDeviceIndex") || force)
    {
        propagateSampleRateAndFrequency(settings.m_localDeviceIndex, settings.m_log2Decim);

        if (m_running)
        {
            DeviceSampleSource *deviceSource = getLocalDevice(settings.m_localDeviceIndex);
            InterferometerBaseband::MsgConfigureLocalDeviceSampleSource *msg =
                InterferometerBaseband::MsgConfigureLocalDeviceSampleSource::create(deviceSource);
            m_basebandSink->getInputMessageQueue()->push(msg);
        }
    }

    if (m_running && (settingsKeys.contains("play") || force)) {
        m_basebandSink->play(settings.m_play);
    }

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "settings", pipes);

    if (!pipes.empty()) {
        sendChannelSettings(pipes, settingsKeys, settings, force);
    }

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = (settingsKeys.contains("useReverseAPI") && settings.m_useReverseAPI) ||
                settingsKeys.contains("reverseAPIAddress") ||
                settingsKeys.contains("reverseAPIPort") ||
                settingsKeys.contains("reverseAPIFeatureSetIndex") ||
                settingsKeys.contains("reverseAPIFeatureIndex");
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

void Interferometer::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

bool Interferometer::handleMessage(const Message& cmd)
{
    if (MsgConfigureInterferometer::match(cmd))
    {
        auto& cfg = (const MsgConfigureInterferometer&) cmd;
        qDebug() << "Interferometer::handleMessage: MsgConfigureInterferometer";
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());
        return true;
    }
    else if (DSPMIMOSignalNotification::match(cmd))
    {
        auto& notif = (const DSPMIMOSignalNotification&) cmd;

        qDebug() << "Interferometer::handleMessage: DSPMIMOSignalNotification:"
                << " inputSampleRate: " << notif.getSampleRate()
                << " centerFrequency: " << notif.getCenterFrequency()
                << " sourceElseSink: " << notif.getSourceOrSink()
                << " streamIndex: " << notif.getIndex();

        if (notif.getSourceOrSink()) // deals with source messages only
        {
            m_deviceSampleRate = notif.getSampleRate();
            if (notif.getIndex() == 0) { // Take stream 0 (channel A) as the reference channel
                m_centerFrequency = notif.getCenterFrequency();
            }
            calculateFrequencyOffset(m_settings.m_log2Decim, m_settings.m_filterChainHash); // This is when device sample rate changes
            propagateSampleRateAndFrequency(m_settings.m_localDeviceIndex, m_settings.m_log2Decim);

            // Notify baseband sink of input sample rate change
            if (m_running)
            {
                InterferometerBaseband::MsgSignalNotification *sig = InterferometerBaseband::MsgSignalNotification::create(
                    m_deviceSampleRate, notif.getCenterFrequency(), notif.getIndex()
                );
                qDebug() << "Interferometer::handleMessage: DSPMIMOSignalNotification: push to sink";
                m_basebandSink->getInputMessageQueue()->push(sig);
            }

            if (getMessageQueueToGUI())
            {
                qDebug() << "Interferometer::handleMessage: DSPMIMOSignalNotification: push to GUI";
                MsgBasebandNotification *msg = MsgBasebandNotification::create(
                    notif.getSampleRate(), notif.getCenterFrequency());
                getMessageQueueToGUI()->push(msg);
            }
        }

        return true;
    }
    else
    {
        return false;
    }
}

QByteArray Interferometer::serialize() const
{
    return m_settings.serialize();
}

bool Interferometer::deserialize(const QByteArray& data)
{
    (void) data;
    if (m_settings.deserialize(data))
    {
        MsgConfigureInterferometer *msg = MsgConfigureInterferometer::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureInterferometer *msg = MsgConfigureInterferometer::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void Interferometer::validateFilterChainHash(InterferometerSettings& settings)
{
    unsigned int s = 1;

    for (unsigned int i = 0; i < settings.m_log2Decim; i++) {
        s *= 3;
    }

    settings.m_filterChainHash = settings.m_filterChainHash >= s ? s-1 : settings.m_filterChainHash;
}

void Interferometer::calculateFrequencyOffset(uint32_t log2Decim, uint32_t filterChainHash)
{
    double shiftFactor = HBFilterChainConverter::getShiftFactor(log2Decim, filterChainHash);
    m_frequencyOffset = (int64_t) (m_deviceSampleRate * shiftFactor);
}

void Interferometer::applyChannelSettings(uint32_t log2Decim, uint32_t filterChainHash)
{
    if (!m_running) {
        return;
    }

    InterferometerBaseband::MsgConfigureChannelizer *msg = InterferometerBaseband::MsgConfigureChannelizer::create(log2Decim, filterChainHash);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void Interferometer::updateDeviceSetList()
{
    MainCore *mainCore = MainCore::instance();
    std::vector<DeviceSet*>& deviceSets = mainCore->getDeviceSets();
    std::vector<DeviceSet*>::const_iterator it = deviceSets.begin();

    m_localInputDeviceIndexes.clear();
    unsigned int deviceSetIndex = 0;

    for (; it != deviceSets.end(); ++it, deviceSetIndex++)
    {
        DSPDeviceSourceEngine *deviceSourceEngine = (*it)->m_deviceSourceEngine;

        if (deviceSourceEngine)
        {
            const DeviceSampleSource *deviceSource = deviceSourceEngine->getSource();

            if (deviceSource->getDeviceDescription() == "LocalInput") {
                m_localInputDeviceIndexes.append(deviceSetIndex);
            }
        }
    }

    if (m_guiMessageQueue)
    {
        MsgReportDevices *msg = MsgReportDevices::create();
        msg->getDeviceSetIndexes() = m_localInputDeviceIndexes;
        m_guiMessageQueue->push(msg);
    }

    InterferometerSettings settings = m_settings;
    int newIndexInList;

    if (!m_localInputDeviceIndexes.empty()) // there are some local input devices
    {
        if (m_settings.m_localDeviceIndex < 0) { // not set before
            newIndexInList = 0; // set to first device in list
        } else if (m_settings.m_localDeviceIndex >= m_localInputDeviceIndexes.size()) { // past last device
            newIndexInList = m_localInputDeviceIndexes.size() - 1; // set to last device in list
        } else { // no change
            newIndexInList = m_settings.m_localDeviceIndex;
        }
    }
    else // there are no local input devices
    {
        newIndexInList = -1; // set index to nothing
    }

    if (newIndexInList < 0) {
        settings.m_localDeviceIndex = -1; // means no device
    } else {
        settings.m_localDeviceIndex = m_localInputDeviceIndexes[newIndexInList];
    }

    qDebug("Interferometer::updateDeviceSetLists: new device index: %d device: %d", newIndexInList, settings.m_localDeviceIndex);
    applySettings(settings, QList<QString>{"localDeviceIndex"});

    if (m_guiMessageQueue)
    {
        MsgConfigureInterferometer *msg = MsgConfigureInterferometer::create(m_settings, QList<QString>{"localDeviceIndex"}, false);
        m_guiMessageQueue->push(msg);
    }
}

DeviceSampleSource *Interferometer::getLocalDevice(int deviceSetIndex)
{
    if (deviceSetIndex < 0) {
        return nullptr;
    }

    MainCore *mainCore = MainCore::instance();
    std::vector<DeviceSet*>& deviceSets = mainCore->getDeviceSets();

    if (deviceSetIndex < (int) deviceSets.size())
    {
        DeviceSet *sourceDeviceSet = deviceSets[deviceSetIndex];
        DSPDeviceSourceEngine *deviceSourceEngine = sourceDeviceSet->m_deviceSourceEngine;

        if (deviceSourceEngine)
        {
            DeviceSampleSource *deviceSource = deviceSourceEngine->getSource();

            if (deviceSource->getDeviceDescription() == "LocalInput") {
                return deviceSource;
            } else {
                qDebug("Interferometer::getLocalDevice: source device at index %u is not a Local Input source", deviceSetIndex);
            }
        }
        else
        {
            qDebug("Interferometer::getLocalDevice: device set at index %d has not a source device", deviceSetIndex);
        }
    }
    else
    {
        qDebug("Interferometer::getLocalDevice: non existent device set at index: %d", deviceSetIndex);
    }

    return nullptr;
}

void Interferometer::propagateSampleRateAndFrequency(int deviceSetIndex, uint32_t log2Decim)
{
    qDebug() << "Interferometer::propagateSampleRateAndFrequency:"
        << " index: " << deviceSetIndex
        << " baseband_freq: " << m_deviceSampleRate
        << " log2Decim: " <<  log2Decim
        << " frequency: " << m_centerFrequency + m_frequencyOffset;

    DeviceSampleSource *deviceSource = getLocalDevice(deviceSetIndex);

    if (deviceSource)
    {
        deviceSource->setSampleRate(m_deviceSampleRate / (1 << log2Decim));
        deviceSource->setCenterFrequency(m_centerFrequency + m_frequencyOffset);
    }
    else
    {
        qDebug("Interferometer::propagateSampleRateAndFrequency: no suitable device at index %u", deviceSetIndex);
    }
}

int Interferometer::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setInterferometerSettings(new SWGSDRangel::SWGInterferometerSettings());
    response.getInterferometerSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int Interferometer::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int Interferometer::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    InterferometerSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureInterferometer *msg = MsgConfigureInterferometer::create(settings, channelSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (getMessageQueueToGUI()) // forward to GUI if any
    {
        MsgConfigureInterferometer *msgToGUI = MsgConfigureInterferometer::create(settings, channelSettingsKeys, force);
        getMessageQueueToGUI()->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void Interferometer::webapiUpdateChannelSettings(
        InterferometerSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getInterferometerSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getInterferometerSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getInterferometerSettings()->getLog2Decim();
    }
    if (channelSettingsKeys.contains("phase")) {
        settings.m_phase = response.getInterferometerSettings()->getPhase();
    }
    if (channelSettingsKeys.contains("gain")) {
        settings.m_gain = response.getInterferometerSettings()->getGain();
    }
    if (channelSettingsKeys.contains("localDeviceIndex")) {
        settings.m_localDeviceIndex = response.getInterferometerSettings()->getLocalDeviceIndex();
    }
    if (channelSettingsKeys.contains("play")) {
        settings.m_play = response.getInterferometerSettings()->getPlay() != 0;
    }

    if (channelSettingsKeys.contains("filterChainHash"))
    {
        settings.m_filterChainHash = response.getInterferometerSettings()->getFilterChainHash();
        validateFilterChainHash(settings);
    }

    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getInterferometerSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getInterferometerSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = (uint16_t) response.getInterferometerSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = (uint16_t) response.getInterferometerSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = (uint16_t) response.getInterferometerSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_spectrumGUI && channelSettingsKeys.contains("spectrumConfig")) {
        settings.m_spectrumGUI->updateFrom(channelSettingsKeys, response.getInterferometerSettings()->getSpectrumConfig());
    }
    if (settings.m_scopeGUI && channelSettingsKeys.contains("scopeConfig")) {
        settings.m_scopeGUI->updateFrom(channelSettingsKeys, response.getInterferometerSettings()->getScopeConfig());
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getInterferometerSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getInterferometerSettings()->getRollupState());
    }
}

void Interferometer::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const InterferometerSettings& settings)
{
    response.getInterferometerSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getInterferometerSettings()->getTitle()) {
        *response.getInterferometerSettings()->getTitle() = settings.m_title;
    } else {
        response.getInterferometerSettings()->setTitle(new QString(settings.m_title));
    }

    response.getInterferometerSettings()->setLog2Decim(settings.m_log2Decim);
    response.getInterferometerSettings()->setPhase(settings.m_phase);
    response.getInterferometerSettings()->setGain(settings.m_gain);
    response.getInterferometerSettings()->setLocalDeviceIndex(settings.m_localDeviceIndex);
    response.getInterferometerSettings()->setPlay(settings.m_play ? 1 : 0);
    response.getInterferometerSettings()->setFilterChainHash(settings.m_filterChainHash);
    response.getInterferometerSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getInterferometerSettings()->getReverseApiAddress()) {
        *response.getInterferometerSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getInterferometerSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getInterferometerSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getInterferometerSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getInterferometerSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_spectrumGUI)
    {
        if (response.getInterferometerSettings()->getSpectrumConfig())
        {
            settings.m_spectrumGUI->formatTo(response.getInterferometerSettings()->getSpectrumConfig());
        }
        else
        {
            auto *swgGLSpectrum = new SWGSDRangel::SWGGLSpectrum();
            settings.m_spectrumGUI->formatTo(swgGLSpectrum);
            response.getInterferometerSettings()->setSpectrumConfig(swgGLSpectrum);
        }
    }

    if (settings.m_scopeGUI)
    {
        if (response.getInterferometerSettings()->getScopeConfig())
        {
            settings.m_scopeGUI->formatTo(response.getInterferometerSettings()->getScopeConfig());
        }
        else
        {
            auto *swgGLScope = new SWGSDRangel::SWGGLScope();
            settings.m_scopeGUI->formatTo(swgGLScope);
            response.getInterferometerSettings()->setScopeConfig(swgGLScope);
        }
    }

    if (settings.m_channelMarker)
    {
        if (response.getInterferometerSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getInterferometerSettings()->getChannelMarker());
        }
        else
        {
            auto *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getInterferometerSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getInterferometerSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getInterferometerSettings()->getRollupState());
        }
        else
        {
            auto *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getInterferometerSettings()->setRollupState(swgRollupState);
        }
    }
}

void Interferometer::webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const InterferometerSettings& settings, bool force)
{
    auto *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    webapiFormatChannelSettings(channelSettingsKeys, swgChannelSettings, settings, force);

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/channel/%4/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex)
            .arg(settings.m_reverseAPIChannelIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    auto *buffer = new QBuffer();
    buffer->open(QBuffer::ReadWrite);
    buffer->write(swgChannelSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    reply->setParent(buffer);

    delete swgChannelSettings;
}

void Interferometer::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    const QList<QString>& channelSettingsKeys,
    const InterferometerSettings& settings,
    bool force) const
{
    for (const auto& pipe : pipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);

        if (messageQueue)
        {
            auto *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
            webapiFormatChannelSettings(channelSettingsKeys, swgChannelSettings, settings, force);
            MainCore::MsgChannelSettings *msg = MainCore::MsgChannelSettings::create(
                this,
                channelSettingsKeys,
                swgChannelSettings,
                force
            );
            messageQueue->push(msg);
        }
    }
}

void Interferometer::webapiFormatChannelSettings(
        const QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const InterferometerSettings& settings,
        bool force
) const
{
    swgChannelSettings->setDirection(2); // MIMO sink
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("Interferometer"));
    swgChannelSettings->setInterferometerSettings(new SWGSDRangel::SWGInterferometerSettings());
    SWGSDRangel::SWGInterferometerSettings *swgInterferometerSettings = swgChannelSettings->getInterferometerSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgInterferometerSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgInterferometerSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("log2Decim") || force) {
        swgInterferometerSettings->setLog2Decim(settings.m_log2Decim);
    }
    if (channelSettingsKeys.contains("phase") || force) {
        swgInterferometerSettings->setPhase(settings.m_phase);
    }
    if (channelSettingsKeys.contains("gain") || force) {
        swgInterferometerSettings->setGain(settings.m_gain);
    }
    if (channelSettingsKeys.contains("localDeviceIndex") || force) {
        swgInterferometerSettings->setLocalDeviceIndex(settings.m_localDeviceIndex);
    }
    if (channelSettingsKeys.contains("play") || force) {
        swgInterferometerSettings->setPlay(settings.m_play ? 1 : 0);
    }
    if (channelSettingsKeys.contains("filterChainHash") || force) {
        swgInterferometerSettings->setFilterChainHash(settings.m_filterChainHash);
    }

    if (settings.m_spectrumGUI && (channelSettingsKeys.contains("spectrumConfig") || force))
    {
        auto *swgGLSpectrum = new SWGSDRangel::SWGGLSpectrum();
        settings.m_spectrumGUI->formatTo(swgGLSpectrum);
        swgInterferometerSettings->setSpectrumConfig(swgGLSpectrum);
    }

    if (settings.m_scopeGUI && (channelSettingsKeys.contains("scopeConfig") || force))
    {
        auto *swgGLScope = new SWGSDRangel::SWGGLScope();
        settings.m_scopeGUI->formatTo(swgGLScope);
        swgInterferometerSettings->setScopeConfig(swgGLScope);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        auto *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgInterferometerSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        auto *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgInterferometerSettings->setRollupState(swgRollupState);
    }
}

void Interferometer::networkManagerFinished(QNetworkReply *reply) const
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "Interferometer::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("Interferometer::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
