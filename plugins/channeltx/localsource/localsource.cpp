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

#include "localsource.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspdevicesinkengine.h"
#include "dsp/dspengine.h"
#include "dsp/devicesamplesink.h"
#include "dsp/hbfilterchainconverter.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "settings/serializable.h"
#include "maincore.h"

#include "localsourcebaseband.h"

MESSAGE_CLASS_DEFINITION(LocalSource::MsgConfigureLocalSource, Message)

const char* const LocalSource::m_channelIdURI = "sdrangel.channel.localsource";
const char* const LocalSource::m_channelId = "LocalSource";

LocalSource::LocalSource(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
        m_deviceAPI(deviceAPI),
        m_centerFrequency(0),
        m_frequencyOffset(0),
        m_basebandSampleRate(48000)
{
    setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSource = new LocalSourceBaseband();
    m_basebandSource->moveToThread(m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSource(this);
    m_deviceAPI->addChannelSourceAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &LocalSource::networkManagerFinished
    );
}

LocalSource::~LocalSource()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &LocalSource::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this);
    delete m_basebandSource;
    delete m_thread;
}

void LocalSource::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI != m_deviceAPI)
    {
        m_deviceAPI->removeChannelSourceAPI(this);
        m_deviceAPI->removeChannelSource(this);
        m_deviceAPI = deviceAPI;
        m_deviceAPI->addChannelSource(this);
        m_deviceAPI->addChannelSinkAPI(this);
    }
}

void LocalSource::start()
{
	qDebug("LocalSource::start");
    m_basebandSource->reset();
    m_thread->start();
}

void LocalSource::stop()
{
    qDebug("LocalSource::stop");
	m_thread->exit();
	m_thread->wait();
}

void LocalSource::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    m_basebandSource->pull(begin, nbSamples);
}

bool LocalSource::handleMessage(const Message& cmd)
{
    if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& cfg = (DSPSignalNotification&) cmd;
        qDebug() << "LocalSource::handleMessage: DSPSignalNotification: "
            << "basband sample rate: " << cfg.getSampleRate()
            << "center frequency: " << cfg.getCenterFrequency();

        m_basebandSampleRate = cfg.getSampleRate();
        m_centerFrequency = cfg.getCenterFrequency();

        calculateFrequencyOffset(m_settings.m_log2Interp, m_settings.m_filterChainHash);
        propagateSampleRateAndFrequency(m_settings.m_localDeviceIndex, m_settings.m_log2Interp);

        DSPSignalNotification *msg = new DSPSignalNotification(cfg.getSampleRate(), cfg.getCenterFrequency());
        m_basebandSource->getInputMessageQueue()->push(msg);

        if (m_guiMessageQueue) {
            m_guiMessageQueue->push(new DSPSignalNotification(cfg));
        }

        return true;
    }
    if (MsgConfigureLocalSource::match(cmd))
    {
        MsgConfigureLocalSource& cfg = (MsgConfigureLocalSource&) cmd;
        qDebug() << "LocalSource::handleMessage: MsgConfigureLocalSink";
        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else
    {
        return false;
    }
}

QByteArray LocalSource::serialize() const
{
    return m_settings.serialize();
}

bool LocalSource::deserialize(const QByteArray& data)
{
    (void) data;
    if (m_settings.deserialize(data))
    {
        MsgConfigureLocalSource *msg = MsgConfigureLocalSource::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureLocalSource *msg = MsgConfigureLocalSource::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void LocalSource::getLocalDevices(std::vector<uint32_t>& indexes)
{
    indexes.clear();
    DSPEngine *dspEngine = DSPEngine::instance();

    for (uint32_t i = 0; i < dspEngine->getDeviceSinkEnginesNumber(); i++)
    {
        DSPDeviceSinkEngine *deviceSinkEngine = dspEngine->getDeviceSinkEngineByIndex(i);
        DeviceSampleSink *deviceSink = deviceSinkEngine->getSink();

        if (deviceSink->getDeviceDescription() == "LocalOutput")
        {
            qDebug("LocalSource::getLocalDevices: index: %u: LocalOutput found", i);
            indexes.push_back(i);
        }
        else
        {
            qDebug("LocalSource::getLocalDevices: index: %u: %s", i, qPrintable(deviceSink->getDeviceDescription()));
        }
    }
}

DeviceSampleSink *LocalSource::getLocalDevice(uint32_t index)
{
    DSPEngine *dspEngine = DSPEngine::instance();

    if (index < dspEngine->getDeviceSinkEnginesNumber())
    {
        DSPDeviceSinkEngine *deviceSinkEngine = dspEngine->getDeviceSinkEngineByIndex(index);
        DeviceSampleSink *deviceSink = deviceSinkEngine->getSink();

        if (deviceSink->getDeviceDescription() == "LocalOutput")
        {
            if (!getDeviceAPI()) {
                qDebug("LocalSource::getLocalDevice: the parent device is unset");
            } else if (getDeviceAPI()->getDeviceUID() == deviceSinkEngine->getUID()) {
                qDebug("LocalSource::getLocalDevice: sink device at index %u is the parent device", index);
            } else {
                return deviceSink;
            }
        }
        else
        {
            qDebug("LocalSource::getLocalDevice: sink device at index %u is not a Local Output source", index);
        }
    }
    else
    {
        qDebug("LocalSource::getLocalDevice: non existent sink device index: %u", index);
    }

    return nullptr;
}

void LocalSource::propagateSampleRateAndFrequency(uint32_t index, uint32_t log2Interp)
{
    qDebug() << "LocalSource::propagateSampleRateAndFrequency:"
        << " index: " << index
        << " baseband_freq: " << m_basebandSampleRate
        << " log2interp: " <<  log2Interp
        << " frequency: " << m_centerFrequency + m_frequencyOffset;

    DeviceSampleSink *deviceSink = getLocalDevice(index);

    if (deviceSink)
    {
        deviceSink->setSampleRate(m_basebandSampleRate / (1 << log2Interp));
        deviceSink->setCenterFrequency(m_centerFrequency + m_frequencyOffset);
    }
    else
    {
        qDebug("LocalSource::propagateSampleRateAndFrequency: no suitable device at index %u", index);
    }
}

void LocalSource::applySettings(const LocalSourceSettings& settings, bool force)
{
    qDebug() << "LocalSource::applySettings:"
        << "m_localDeviceIndex:" << settings.m_localDeviceIndex
        << "m_log2Interp:" << settings.m_log2Interp
        << "m_filterChainHash:" << settings.m_filterChainHash
        << "m_play:" << settings.m_play
        << "m_rgbColor:" << settings.m_rgbColor
        << "m_title:" << settings.m_title
        << "m_useReverseAPI:" << settings.m_useReverseAPI
        << "m_reverseAPIAddress:" << settings.m_reverseAPIAddress
        << "m_reverseAPIChannelIndex:" << settings.m_reverseAPIChannelIndex
        << "m_reverseAPIDeviceIndex:" << settings.m_reverseAPIDeviceIndex
        << "m_reverseAPIPort:" << settings.m_reverseAPIPort
        << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((settings.m_log2Interp != m_settings.m_log2Interp) || force) {
        reverseAPIKeys.append("log2Interp");
    }
    if ((settings.m_filterChainHash != m_settings.m_filterChainHash) || force) {
        reverseAPIKeys.append("filterChainHash");
    }
    if ((settings.m_localDeviceIndex != m_settings.m_localDeviceIndex) || force)
    {
        reverseAPIKeys.append("localDeviceIndex");
        propagateSampleRateAndFrequency(settings.m_localDeviceIndex, settings.m_log2Interp);
        DeviceSampleSink *deviceSampleSink = getLocalDevice(settings.m_localDeviceIndex);
        LocalSourceBaseband::MsgConfigureLocalDeviceSampleSink *msg =
            LocalSourceBaseband::MsgConfigureLocalDeviceSampleSink::create(deviceSampleSink);
        m_basebandSource->getInputMessageQueue()->push(msg);
    }

    if ((settings.m_log2Interp != m_settings.m_log2Interp)
     || (settings.m_filterChainHash != m_settings.m_filterChainHash) || force)
    {
        calculateFrequencyOffset(settings.m_log2Interp, settings.m_filterChainHash);
        propagateSampleRateAndFrequency(m_settings.m_localDeviceIndex, settings.m_log2Interp);
    }

    if ((settings.m_play != m_settings.m_play) || force)
    {
        reverseAPIKeys.append("play");
        LocalSourceBaseband::MsgConfigureLocalSourceWork *msg = LocalSourceBaseband::MsgConfigureLocalSourceWork::create(
            settings.m_play
        );
        m_basebandSource->getInputMessageQueue()->push(msg);
    }

    if (m_settings.m_streamIndex != settings.m_streamIndex)
    {
        if (m_deviceAPI->getSampleMIMO()) // change of stream is possible for MIMO devices only
        {
            m_deviceAPI->removeChannelSourceAPI(this);
            m_deviceAPI->removeChannelSource(this, m_settings.m_streamIndex);
            m_deviceAPI->addChannelSource(this, settings.m_streamIndex);
            m_deviceAPI->addChannelSourceAPI(this);
        }

        reverseAPIKeys.append("streamIndex");
    }

    LocalSourceBaseband::MsgConfigureLocalSourceBaseband *msg = LocalSourceBaseband::MsgConfigureLocalSourceBaseband::create(settings, force);
    m_basebandSource->getInputMessageQueue()->push(msg);

    if ((settings.m_useReverseAPI) && (reverseAPIKeys.size() != 0))
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex) ||
                (m_settings.m_reverseAPIChannelIndex != settings.m_reverseAPIChannelIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "settings", pipes);

    if (pipes.size() > 0) {
        sendChannelSettings(pipes, reverseAPIKeys, settings, force);
    }

    m_settings = settings;
}

void LocalSource::validateFilterChainHash(LocalSourceSettings& settings)
{
    unsigned int s = 1;

    for (unsigned int i = 0; i < settings.m_log2Interp; i++) {
        s *= 3;
    }

    settings.m_filterChainHash = settings.m_filterChainHash >= s ? s-1 : settings.m_filterChainHash;
}

void LocalSource::calculateFrequencyOffset(uint32_t log2Interp, uint32_t filterChainHash)
{
    double shiftFactor = HBFilterChainConverter::getShiftFactor(log2Interp, filterChainHash);
    m_frequencyOffset = m_basebandSampleRate * shiftFactor;
}

int LocalSource::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setLocalSourceSettings(new SWGSDRangel::SWGLocalSourceSettings());
    response.getLocalSourceSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int LocalSource::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int LocalSource::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    LocalSourceSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureLocalSource *msg = MsgConfigureLocalSource::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("LocalSource::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureLocalSource *msgToGUI = MsgConfigureLocalSource::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void LocalSource::webapiUpdateChannelSettings(
        LocalSourceSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("localDeviceIndex")) {
        settings.m_localDeviceIndex = response.getLocalSourceSettings()->getLocalDeviceIndex();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getLocalSourceSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getLocalSourceSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("log2Interp")) {
        settings.m_log2Interp = response.getLocalSourceSettings()->getLog2Interp();
    }

    if (channelSettingsKeys.contains("filterChainHash"))
    {
        settings.m_filterChainHash = response.getLocalSourceSettings()->getFilterChainHash();
        validateFilterChainHash(settings);
    }

    if (channelSettingsKeys.contains("play")) {
        settings.m_play = response.getLocalSourceSettings()->getPlay() != 0;
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getLocalSourceSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getLocalSourceSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getLocalSourceSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getLocalSourceSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getLocalSourceSettings()->getReverseApiChannelIndex();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getLocalSourceSettings()->getStreamIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getLocalSourceSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getLocalSourceSettings()->getRollupState());
    }
}

void LocalSource::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const LocalSourceSettings& settings)
{
    response.getLocalSourceSettings()->setLocalDeviceIndex(settings.m_localDeviceIndex);
    response.getLocalSourceSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getLocalSourceSettings()->getTitle()) {
        *response.getLocalSourceSettings()->getTitle() = settings.m_title;
    } else {
        response.getLocalSourceSettings()->setTitle(new QString(settings.m_title));
    }

    response.getLocalSourceSettings()->setLog2Interp(settings.m_log2Interp);
    response.getLocalSourceSettings()->setFilterChainHash(settings.m_filterChainHash);
    response.getLocalSourceSettings()->setPlay(settings.m_play ? 1 : 0);
    response.getLocalSourceSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getLocalSourceSettings()->getReverseApiAddress()) {
        *response.getLocalSourceSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getLocalSourceSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getLocalSourceSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getLocalSourceSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getLocalSourceSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getLocalSourceSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getLocalSourceSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getLocalSourceSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getLocalSourceSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getLocalSourceSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getLocalSourceSettings()->setRollupState(swgRollupState);
        }
    }
}

void LocalSource::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const LocalSourceSettings& settings, bool force)
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

void LocalSource::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const LocalSourceSettings& settings,
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

void LocalSource::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const LocalSourceSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setLocalSourceSettings(new SWGSDRangel::SWGLocalSourceSettings());
    SWGSDRangel::SWGLocalSourceSettings *swgLocalSourceSettings = swgChannelSettings->getLocalSourceSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("localDeviceIndex") || force) {
        swgLocalSourceSettings->setLocalDeviceIndex(settings.m_localDeviceIndex);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgLocalSourceSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgLocalSourceSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("log2Interp") || force) {
        swgLocalSourceSettings->setLog2Interp(settings.m_log2Interp);
    }
    if (channelSettingsKeys.contains("filterChainHash") || force) {
        swgLocalSourceSettings->setFilterChainHash(settings.m_filterChainHash);
    }
    if (channelSettingsKeys.contains("play") || force) {
        swgLocalSourceSettings->setPlay(settings.m_play ? 1 : 0);
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgLocalSourceSettings->setRgbColor(settings.m_streamIndex);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgLocalSourceSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgLocalSourceSettings->setRollupState(swgRollupState);
    }
}

void LocalSource::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "LocalSource::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("LocalSource::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

uint32_t LocalSource::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}
