///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
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


#include <boost/crc.hpp>
#include <boost/cstdint.hpp>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspengine.h"
#include "dsp/devicesamplesource.h"
#include "dsp/hbfilterchainconverter.h"
#include "dsp/devicesamplemimo.h"
#include "device/deviceapi.h"
#include "device/deviceset.h"
#include "feature/feature.h"
#include "settings/serializable.h"
#include "maincore.h"

#include "localsinkbaseband.h"
#include "localsink.h"

MESSAGE_CLASS_DEFINITION(LocalSink::MsgConfigureLocalSink, Message)
MESSAGE_CLASS_DEFINITION(LocalSink::MsgReportDevices, Message)

const char* const LocalSink::m_channelIdURI = "sdrangel.channel.localsink";
const char* const LocalSink::m_channelId = "LocalSink";

LocalSink::LocalSink(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_thread(nullptr),
        m_basebandSink(nullptr),
        m_running(false),
        m_spectrumVis(SDR_RX_SCALEF),
        m_centerFrequency(0),
        m_frequencyOffset(0),
        m_basebandSampleRate(48000)
{
    setObjectName(m_channelId);
    applySettings(m_settings, QList<QString>(), true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &LocalSink::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &LocalSink::handleIndexInDeviceSetChanged
    );
    // Update device list when devices are added or removed
    QObject::connect(
        MainCore::instance(),
        &MainCore::deviceSetAdded,
        this,
        &LocalSink::updateDeviceSetList
    );
    QObject::connect(
        MainCore::instance(),
        &MainCore::deviceSetRemoved,
        this,
        &LocalSink::updateDeviceSetList
    );
}

LocalSink::~LocalSink()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &LocalSink::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);
    stopProcessing();
}

void LocalSink::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI != m_deviceAPI)
    {
        m_deviceAPI->removeChannelSinkAPI(this);
        m_deviceAPI->removeChannelSink(this);
        m_deviceAPI = deviceAPI;
        m_deviceAPI->addChannelSink(this);
        m_deviceAPI->addChannelSinkAPI(this);
    }
}

uint32_t LocalSink::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void LocalSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;

    if (m_running) {
        m_basebandSink->feed(begin, end);
    }
}

void LocalSink::start()
{ }

void LocalSink::stop()
{ }

void LocalSink::startProcessing()
{
    if (m_running) {
        return;
    }

	qDebug("LocalSink::startProcessing");
    m_thread = new QThread(this);
    m_basebandSink = new LocalSinkBaseband();
    m_basebandSink->setSpectrumSink(&m_spectrumVis);
    m_basebandSink->moveToThread(m_thread);

    QObject::connect(m_thread, &QThread::finished, m_basebandSink, &QObject::deleteLater);
    QObject::connect(m_thread, &QThread::finished, m_thread, &QThread::deleteLater);

    m_basebandSink->reset();
    m_thread->start();

    DeviceSampleSource *deviceSource = getLocalDevice(m_settings.m_localDeviceIndex);
    LocalSinkBaseband::MsgConfigureLocalDeviceSampleSource *msgDevice =
        LocalSinkBaseband::MsgConfigureLocalDeviceSampleSource::create(deviceSource);
    m_basebandSink->getInputMessageQueue()->push(msgDevice);

    LocalSinkBaseband::MsgConfigureLocalSinkBaseband *msgConfig = LocalSinkBaseband::MsgConfigureLocalSinkBaseband::create(m_settings, QList<QString>(), true);
    m_basebandSink->getInputMessageQueue()->push(msgConfig);

    LocalSinkBaseband::MsgSetSpectrumSampleRateAndFrequency *msgSpectrum = LocalSinkBaseband::MsgSetSpectrumSampleRateAndFrequency::create(
        m_basebandSampleRate / (1 << m_settings.m_log2Decim),
        m_centerFrequency + m_frequencyOffset
    );
    m_basebandSink->getInputMessageQueue()->push(msgSpectrum);

    m_running = true;
}

void LocalSink::stopProcessing()
{
    if (!m_running) {
        return;
    }

    qDebug("LocalSink::stopProcessing");
    m_running = false;
	m_thread->exit();
	m_thread->wait();
}

bool LocalSink::handleMessage(const Message& cmd)
{
    if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;

        qDebug() << "LocalSink::handleMessage: DSPSignalNotification:"
                << " inputSampleRate: " << notif.getSampleRate()
                << " centerFrequency: " << notif.getCenterFrequency();

        m_basebandSampleRate = notif.getSampleRate();
        m_centerFrequency = notif.getCenterFrequency();

        calculateFrequencyOffset(m_settings.m_log2Decim, m_settings.m_filterChainHash); // This is when device sample rate changes
        propagateSampleRateAndFrequency(m_settings.m_localDeviceIndex, m_settings.m_log2Decim);

        if (m_running)
        {
            DSPSignalNotification *msg = new DSPSignalNotification(notif.getSampleRate(), notif.getCenterFrequency());
            m_basebandSink->getInputMessageQueue()->push(msg);
            LocalSinkBaseband::MsgSetSpectrumSampleRateAndFrequency *msgSpectrum =
                LocalSinkBaseband::MsgSetSpectrumSampleRateAndFrequency::create(
                    m_basebandSampleRate / (1 << m_settings.m_log2Decim),
                    m_centerFrequency + m_frequencyOffset
                );
            m_basebandSink->getInputMessageQueue()->push(msgSpectrum);
        }

        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (MsgConfigureLocalSink::match(cmd))
    {
        MsgConfigureLocalSink& cfg = (MsgConfigureLocalSink&) cmd;
        qDebug() << "LocalSink::handleMessage: MsgConfigureLocalSink";
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

        return true;
    }
    else
    {
        return false;
    }
}

QByteArray LocalSink::serialize() const
{
    return m_settings.serialize();
}

bool LocalSink::deserialize(const QByteArray& data)
{
    (void) data;
    if (m_settings.deserialize(data))
    {
        MsgConfigureLocalSink *msg = MsgConfigureLocalSink::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureLocalSink *msg = MsgConfigureLocalSink::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

DeviceSampleSource *LocalSink::getLocalDevice(int index)
{
    if (index < 0) {
        return nullptr;
    }

    DSPEngine *dspEngine = DSPEngine::instance();

    if (index < (int) dspEngine->getDeviceSourceEnginesNumber())
    {
        DSPDeviceSourceEngine *deviceSourceEngine = dspEngine->getDeviceSourceEngineByIndex(index);
        DeviceSampleSource *deviceSource = deviceSourceEngine->getSource();

        if (deviceSource->getDeviceDescription() == "LocalInput")
        {
            if (!getDeviceAPI()) {
                qDebug("LocalSink::getLocalDevice: the parent device is unset");
            } else if (getDeviceAPI()->getDeviceUID() == deviceSourceEngine->getUID()) {
                qDebug("LocalSink::getLocalDevice: source device at index %u is the parent device", index);
            } else {
                return deviceSource;
            }
        }
        else
        {
            qDebug("LocalSink::getLocalDevice: source device at index %u is not a Local Input source", index);
        }
    }
    else
    {
        qDebug("LocalSink::getLocalDevice: non existent source device index: %u", index);
    }

    return nullptr;
}

void LocalSink::propagateSampleRateAndFrequency(int index, uint32_t log2Decim)
{
    qDebug() << "LocalSink::propagateSampleRateAndFrequency:"
        << " index: " << index
        << " baseband_freq: " << m_basebandSampleRate
        << " log2Decim: " <<  log2Decim
        << " frequency: " << m_centerFrequency + m_frequencyOffset;

    DeviceSampleSource *deviceSource = getLocalDevice(index);

    if (deviceSource)
    {
        deviceSource->setSampleRate(m_basebandSampleRate / (1 << log2Decim));
        deviceSource->setCenterFrequency(m_centerFrequency + m_frequencyOffset);
    }
    else
    {
        qDebug("LocalSink::propagateSampleRateAndFrequency: no suitable device at index %u", index);
    }
}

void LocalSink::applySettings(const LocalSinkSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "LocalSink::applySettings:" << settings.getDebugString(settingsKeys, force) << "force: " << force;

    if (settingsKeys.contains("localDeviceIndex") || force)
    {
        propagateSampleRateAndFrequency(settings.m_localDeviceIndex, settings.m_log2Decim);

        if (m_running)
        {
            DeviceSampleSource *deviceSource = getLocalDevice(settings.m_localDeviceIndex);
            LocalSinkBaseband::MsgConfigureLocalDeviceSampleSource *msg =
                LocalSinkBaseband::MsgConfigureLocalDeviceSampleSource::create(deviceSource);
            m_basebandSink->getInputMessageQueue()->push(msg);
        }
    }

    if (settingsKeys.contains("log2Decim")
     || settingsKeys.contains("filterChainHash") || force)
    {
        calculateFrequencyOffset(settings.m_log2Decim, settings.m_filterChainHash);
        propagateSampleRateAndFrequency(m_settings.m_localDeviceIndex, settings.m_log2Decim);

        if (m_running)
        {
            LocalSinkBaseband::MsgSetSpectrumSampleRateAndFrequency *msgSpectrum =
                LocalSinkBaseband::MsgSetSpectrumSampleRateAndFrequency::create(
                    m_basebandSampleRate / (1 << m_settings.m_log2Decim),
                    m_centerFrequency + m_frequencyOffset
                );
            m_basebandSink->getInputMessageQueue()->push(msgSpectrum);
        }
    }

    if (settingsKeys.contains("play") || force)
    {
        if (settings.m_play) {
            startProcessing();
        } else {
            stopProcessing();
        }
    }

    if (settingsKeys.contains("streamIndex"))
    {
        if (m_deviceAPI->getSampleMIMO()) // change of stream is possible for MIMO devices only
        {
            m_deviceAPI->removeChannelSinkAPI(this);
            m_deviceAPI->removeChannelSink(this, m_settings.m_streamIndex);
            m_deviceAPI->addChannelSink(this, settings.m_streamIndex);
            m_deviceAPI->addChannelSinkAPI(this);
        }
    }

    if (m_running)
    {
        LocalSinkBaseband::MsgConfigureLocalSinkBaseband *msg = LocalSinkBaseband::MsgConfigureLocalSinkBaseband::create(settings, settingsKeys, force);
        m_basebandSink->getInputMessageQueue()->push(msg);
    }

    if (settingsKeys.contains("useReverseAPI"))
    {
        bool fullUpdate = (settingsKeys.contains("useReverseAPI") && settings.m_useReverseAPI) ||
                settingsKeys.contains("reverseAPIAddress") ||
                settingsKeys.contains("reverseAPIPort") ||
                settingsKeys.contains("reverseAPIFeatureSetIndex") ||
                settingsKeys.contains("m_reverseAPIFeatureIndex");
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "settings", pipes);

    if (pipes.size() > 0) {
        sendChannelSettings(pipes, settingsKeys, settings, force);
    }

    m_settings = settings;
}

void LocalSink::validateFilterChainHash(LocalSinkSettings& settings)
{
    unsigned int s = 1;

    for (unsigned int i = 0; i < settings.m_log2Decim; i++) {
        s *= 3;
    }

    settings.m_filterChainHash = settings.m_filterChainHash >= s ? s-1 : settings.m_filterChainHash;
}

void LocalSink::calculateFrequencyOffset(uint32_t log2Decim, uint32_t filterChainHash)
{
    double shiftFactor = HBFilterChainConverter::getShiftFactor(log2Decim, filterChainHash);
    m_frequencyOffset = m_basebandSampleRate * shiftFactor;
}

int LocalSink::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setLocalSinkSettings(new SWGSDRangel::SWGLocalSinkSettings());
    response.getLocalSinkSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int LocalSink::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int LocalSink::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    LocalSinkSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureLocalSink *msg = MsgConfigureLocalSink::create(settings, channelSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    qDebug("LocalSink::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureLocalSink *msgToGUI = MsgConfigureLocalSink::create(settings, channelSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void LocalSink::webapiUpdateChannelSettings(
        LocalSinkSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("localDeviceIndex")) {
        settings.m_localDeviceIndex = response.getLocalSinkSettings()->getLocalDeviceIndex();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getLocalSinkSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getLocalSinkSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getLocalSinkSettings()->getLog2Decim();
    }

    if (channelSettingsKeys.contains("filterChainHash"))
    {
        settings.m_filterChainHash = response.getLocalSinkSettings()->getFilterChainHash();
        validateFilterChainHash(settings);
    }

    if (channelSettingsKeys.contains("play")) {
        settings.m_play = response.getLocalSinkSettings()->getPlay() != 0;
    }
    if (channelSettingsKeys.contains("dsp")) {
        settings.m_dsp = response.getLocalSinkSettings()->getDsp() != 0;
    }
    if (channelSettingsKeys.contains("gaindB")) {
        settings.m_gaindB = response.getLocalSinkSettings()->getGaindB();
    }
    if (channelSettingsKeys.contains("fftOn")) {
        settings.m_fftOn = response.getLocalSinkSettings()->getFftOn() != 0;
    }
    if (channelSettingsKeys.contains("log2FFT")) {
        settings.m_log2FFT = response.getLocalSinkSettings()->getLog2Fft();
    }
    if (channelSettingsKeys.contains("fftWindow")) {
        settings.m_fftWindow = (FFTWindow::Function) response.getLocalSinkSettings()->getFftWindow();
    }
    if (channelSettingsKeys.contains("reverseFilter")) {
        settings.m_reverseFilter = response.getLocalSinkSettings()->getReverseFilter() != 0;
    }

    if (channelSettingsKeys.contains("fftBands"))
    {
        QList<SWGSDRangel::SWGFFTBand *> *fftBands = response.getLocalSinkSettings()->getFftBands();
        settings.m_fftBands.clear();

        for (const auto& fftBand : *fftBands) {
            settings.m_fftBands.push_back(std::pair<float, float>{fftBand->getFstart(), fftBand->getBandwidth()});
        }
    }

    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getLocalSinkSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getLocalSinkSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getLocalSinkSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getLocalSinkSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getLocalSinkSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getLocalSinkSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getLocalSinkSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getLocalSinkSettings()->getRollupState());
    }
}

void LocalSink::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const LocalSinkSettings& settings)
{
    response.getLocalSinkSettings()->setLocalDeviceIndex(settings.m_localDeviceIndex);
    response.getLocalSinkSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getLocalSinkSettings()->getTitle()) {
        *response.getLocalSinkSettings()->getTitle() = settings.m_title;
    } else {
        response.getLocalSinkSettings()->setTitle(new QString(settings.m_title));
    }

    response.getLocalSinkSettings()->setLog2Decim(settings.m_log2Decim);
    response.getLocalSinkSettings()->setFilterChainHash(settings.m_filterChainHash);
    response.getLocalSinkSettings()->setPlay(settings.m_play ? 1 : 0);
    response.getLocalSinkSettings()->setDsp(settings.m_dsp ? 1 : 0);
    response.getLocalSinkSettings()->setGaindB(settings.m_gaindB);
    response.getLocalSinkSettings()->setFftOn(settings.m_fftOn ? 1 : 0);
    response.getLocalSinkSettings()->setLog2Fft(settings.m_log2FFT);
    response.getLocalSinkSettings()->setFftWindow((int) settings.m_fftWindow);
    response.getLocalSinkSettings()->setReverseFilter(settings.m_reverseFilter ? 1 : 0);

    if (!response.getLocalSinkSettings()->getFftBands()) {
        response.getLocalSinkSettings()->setFftBands(new QList<SWGSDRangel::SWGFFTBand *>());
    }

    response.getLocalSinkSettings()->getFftBands()->clear();

    for (const auto& fftBand : settings.m_fftBands)
    {
        response.getLocalSinkSettings()->getFftBands()->push_back(new SWGSDRangel::SWGFFTBand);
        response.getLocalSinkSettings()->getFftBands()->back()->setFstart(fftBand.first);
        response.getLocalSinkSettings()->getFftBands()->back()->setBandwidth(fftBand.second);
    }

    response.getLocalSinkSettings()->setStreamIndex(settings.m_streamIndex);
    response.getLocalSinkSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getLocalSinkSettings()->getReverseApiAddress()) {
        *response.getLocalSinkSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getLocalSinkSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getLocalSinkSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getLocalSinkSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getLocalSinkSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getLocalSinkSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getLocalSinkSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getLocalSinkSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getLocalSinkSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getLocalSinkSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getLocalSinkSettings()->setRollupState(swgRollupState);
        }
    }
}

void LocalSink::webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const LocalSinkSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    webapiFormatChannelSettings(channelSettingsKeys, swgChannelSettings, settings, force);

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/channel/%4/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex)
            .arg(settings.m_reverseAPIChannelIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgChannelSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);

    delete swgChannelSettings;
}

void LocalSink::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    const QList<QString>& channelSettingsKeys,
    const LocalSinkSettings& settings,
    bool force)
{
    for (const auto& pipe : pipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);

        if (messageQueue)
        {
            SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
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

void LocalSink::webapiFormatChannelSettings(
        const QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const LocalSinkSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setLocalSinkSettings(new SWGSDRangel::SWGLocalSinkSettings());
    SWGSDRangel::SWGLocalSinkSettings *swgLocalSinkSettings = swgChannelSettings->getLocalSinkSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("localDeviceIndex") || force) {
        swgLocalSinkSettings->setLocalDeviceIndex(settings.m_localDeviceIndex);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgLocalSinkSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgLocalSinkSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("log2Decim") || force) {
        swgLocalSinkSettings->setLog2Decim(settings.m_log2Decim);
    }
    if (channelSettingsKeys.contains("filterChainHash") || force) {
        swgLocalSinkSettings->setFilterChainHash(settings.m_filterChainHash);
    }
    if (channelSettingsKeys.contains("play") || force) {
        swgLocalSinkSettings->setPlay(settings.m_play ? 1 : 0);
    }
    if (channelSettingsKeys.contains("dsp") || force) {
        swgLocalSinkSettings->setDsp(settings.m_dsp ? 1 : 0);
    }
    if (channelSettingsKeys.contains("gaindB") || force) {
        swgLocalSinkSettings->setGaindB(settings.m_gaindB);
    }
    if (channelSettingsKeys.contains("log2FFT") || force) {
        swgLocalSinkSettings->setLog2Fft(settings.m_log2FFT);
    }
    if (channelSettingsKeys.contains("fftWindow") || force) {
        swgLocalSinkSettings->setFftWindow((int) settings.m_fftWindow);
    }
    if (channelSettingsKeys.contains("fftOn") || force) {
        swgLocalSinkSettings->setFftOn(settings.m_fftOn ? 1 : 0);
    }
    if (channelSettingsKeys.contains("reverseFilter") || force) {
        swgLocalSinkSettings->setReverseFilter(settings.m_reverseFilter ? 1 : 0);
    }

    if (channelSettingsKeys.contains("fftBands") || force)
    {
        swgLocalSinkSettings->setFftBands(new QList<SWGSDRangel::SWGFFTBand *>());

        for (const auto& fftBand : settings.m_fftBands)
        {
            swgLocalSinkSettings->getFftBands()->push_back(new SWGSDRangel::SWGFFTBand);
            swgLocalSinkSettings->getFftBands()->back()->setFstart(fftBand.first);
            swgLocalSinkSettings->getFftBands()->back()->setBandwidth(fftBand.second);
        }
    }

    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgLocalSinkSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgLocalSinkSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgLocalSinkSettings->setRollupState(swgRollupState);
    }
}

void LocalSink::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "LocalSink::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("LocalSink::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void LocalSink::handleIndexInDeviceSetChanged(int index)
{
    if (!m_running || (index < 0)) {
        return;
    }

    QString fifoLabel = QString("%1 [%2:%3]")
        .arg(m_channelId)
        .arg(m_deviceAPI->getDeviceSetIndex())
        .arg(index);
    m_basebandSink->setFifoLabel(fifoLabel);
}

void LocalSink::updateDeviceSetList()
{
    MainCore *mainCore = MainCore::instance();
    std::vector<DeviceSet*>& deviceSets = mainCore->getDeviceSets();
    std::vector<DeviceSet*>::const_iterator it = deviceSets.begin();

    m_localInputDeviceIndexes.clear();
    unsigned int deviceIndex = 0;

    for (; it != deviceSets.end(); ++it, deviceIndex++)
    {
        DSPDeviceSourceEngine *deviceSourceEngine = (*it)->m_deviceSourceEngine;

        if (deviceSourceEngine)
        {
            DeviceSampleSource *deviceSource = deviceSourceEngine->getSource();

            if (deviceSource->getDeviceDescription() == "LocalInput") {
                m_localInputDeviceIndexes.append(deviceIndex);
            }
        }
    }

    if (m_guiMessageQueue)
    {
        MsgReportDevices *msg = MsgReportDevices::create();
        msg->getDeviceSetIndexes() = m_localInputDeviceIndexes;
        m_guiMessageQueue->push(msg);
    }

    LocalSinkSettings settings = m_settings;
    int newIndexInList;

    if (it != deviceSets.begin())
    {
        if (m_settings.m_localDeviceIndex < 0) {
            newIndexInList = 0;
        } else if (m_settings.m_localDeviceIndex >= m_localInputDeviceIndexes.size()) {
            newIndexInList = m_localInputDeviceIndexes.size() - 1;
        } else {
            newIndexInList = m_settings.m_localDeviceIndex;
        }
    }
    else
    {
        newIndexInList = -1;
    }

    if (newIndexInList < 0) {
        settings.m_localDeviceIndex = -1; // means no device
    } else {
        settings.m_localDeviceIndex = m_localInputDeviceIndexes[newIndexInList];
    }

    qDebug("LocalSink::updateDeviceSetLists: new device index: %d device: %d", newIndexInList, settings.m_localDeviceIndex);
    applySettings(settings, QList<QString>{"localDeviceIndex"});

    if (m_guiMessageQueue)
    {
        MsgConfigureLocalSink *msg = MsgConfigureLocalSink::create(m_settings, QList<QString>{"localDeviceIndex"}, false);
        m_guiMessageQueue->push(msg);
    }
}
