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
#include "settings/serializable.h"
#include "feature/feature.h"
#include "maincore.h"

#include "beamsteeringcwmodbaseband.h"
#include "beamsteeringcwmod.h"

MESSAGE_CLASS_DEFINITION(BeamSteeringCWMod::MsgConfigureBeamSteeringCWMod, Message)
MESSAGE_CLASS_DEFINITION(BeamSteeringCWMod::MsgBasebandNotification, Message)

const char* const BeamSteeringCWMod::m_channelIdURI = "sdrangel.channel.beamsteeringcwmod";
const char* const BeamSteeringCWMod::m_channelId = "BeamSteeringCWMod";

BeamSteeringCWMod::BeamSteeringCWMod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamMIMO),
    m_deviceAPI(deviceAPI),
    m_guiMessageQueue(nullptr),
    m_frequencyOffset(0),
    m_basebandSampleRate(48000)
{
    setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSource = new BeamSteeringCWModBaseband();
    m_basebandSource->moveToThread(m_thread);
    m_deviceAPI->addMIMOChannel(this);
    m_deviceAPI->addMIMOChannelAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &BeamSteeringCWMod::networkManagerFinished
    );
}

BeamSteeringCWMod::~BeamSteeringCWMod()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &BeamSteeringCWMod::networkManagerFinished
    );
    delete m_networkManager;

    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeMIMOChannel(this);
    delete m_basebandSource;
    delete m_thread;
}

void BeamSteeringCWMod::setDeviceAPI(DeviceAPI *deviceAPI)
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

void BeamSteeringCWMod::startSources()
{
    qDebug("BeamSteeringCWMod::startSources");
    m_basebandSource->reset();
    m_thread->start();

    BeamSteeringCWModBaseband::MsgSignalNotification *sig = BeamSteeringCWModBaseband::MsgSignalNotification::create(
        m_basebandSampleRate);
    m_basebandSource->getInputMessageQueue()->push(sig);

    BeamSteeringCWModBaseband::MsgConfigureBeamSteeringCWModBaseband *msg = BeamSteeringCWModBaseband::MsgConfigureBeamSteeringCWModBaseband::create(
        m_settings, true);
    m_basebandSource->getInputMessageQueue()->push(msg);
}

void BeamSteeringCWMod::stopSources()
{
    qDebug("BeamSteeringCWMod::stopSources");
	m_thread->exit();
	m_thread->wait();
}

void BeamSteeringCWMod::pull(SampleVector::iterator& begin, unsigned int nbSamples, unsigned int sourceIndex)
{
    m_basebandSource->pull(begin, nbSamples, sourceIndex);
}

void BeamSteeringCWMod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, unsigned int sinkIndex)
{
    (void) begin;
    (void) end;
    (void) sinkIndex;
}

void BeamSteeringCWMod::applySettings(const BeamSteeringCWModSettings& settings, bool force)
{
    qDebug() << "BeamSteeringCWMod::applySettings: "
        << "m_steerDegrees: " << settings.m_steerDegrees
        << "m_channelOutput: " << settings.m_channelOutput
        << "m_filterChainHash: " << settings.m_filterChainHash
        << "m_log2Interp: " << settings.m_log2Interp
        << "m_filterChainHash: " << settings.m_filterChainHash
        << "m_useReverseAPI: " << settings.m_useReverseAPI
        << "m_reverseAPIAddress: " << settings.m_reverseAPIAddress
        << "m_reverseAPIPort: " << settings.m_reverseAPIPort
        << "m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex
        << "m_reverseAPIChannelIndex: " << settings.m_reverseAPIChannelIndex
        << "m_title: " << settings.m_title;

    QList<QString> reverseAPIKeys;

    if ((m_settings.m_steerDegrees != settings.m_steerDegrees) || force) {
        reverseAPIKeys.append("steerDegrees");
    }
    if ((m_settings.m_channelOutput != settings.m_channelOutput) || force) {
        reverseAPIKeys.append("channelOutput");
    }
    if ((m_settings.m_filterChainHash != settings.m_filterChainHash) || force) {
        reverseAPIKeys.append("filterChainHash");
    }
    if ((m_settings.m_log2Interp != settings.m_log2Interp) || force) {
        reverseAPIKeys.append("log2Interp");
    }
    if ((m_settings.m_filterChainHash != settings.m_filterChainHash) || force) {
        reverseAPIKeys.append("filterChainHash");
    }

    BeamSteeringCWModBaseband::MsgConfigureBeamSteeringCWModBaseband *msg = BeamSteeringCWModBaseband::MsgConfigureBeamSteeringCWModBaseband::create(settings, force);
    m_basebandSource->getInputMessageQueue()->push(msg);

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "settings", pipes);

    if (pipes.size() > 0) {
        sendChannelSettings(pipes, reverseAPIKeys, settings, force);
    }

    m_settings = settings;
}

void BeamSteeringCWMod::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

bool BeamSteeringCWMod::handleMessage(const Message& cmd)
{
    if (MsgConfigureBeamSteeringCWMod::match(cmd))
    {
        MsgConfigureBeamSteeringCWMod& cfg = (MsgConfigureBeamSteeringCWMod&) cmd;
        qDebug() << "BeamSteeringCWMod::handleMessage: MsgConfigureBeamSteeringCWMod";
        applySettings(cfg.getSettings(), cfg.getForce());
        return true;
    }
    else if (DSPMIMOSignalNotification::match(cmd))
    {
        DSPMIMOSignalNotification& notif = (DSPMIMOSignalNotification&) cmd;

        qDebug() << "BeamSteeringCWMod::handleMessage: DSPMIMOSignalNotification:"
                << " basebandSampleRate: " << notif.getSampleRate()
                << " centerFrequency: " << notif.getCenterFrequency()
                << " sourceElseSink: " << notif.getSourceOrSink()
                << " streamIndex: " << notif.getIndex();

        if (!notif.getSourceOrSink()) // deals with sink messages only
        {
            m_basebandSampleRate = notif.getSampleRate();
            calculateFrequencyOffset(); // This is when device sample rate changes

            // Notify source of input sample rate change
            BeamSteeringCWModBaseband::MsgSignalNotification *sig = BeamSteeringCWModBaseband::MsgSignalNotification::create(
                m_basebandSampleRate);
            qDebug() << "BeamSteeringCWMod::handleMessage: DSPMIMOSignalNotification: push to source";
            m_basebandSource->getInputMessageQueue()->push(sig);

            if (m_guiMessageQueue)
            {
                qDebug() << "BeamSteeringCWMod::handleMessage: DSPMIMOSignalNotification: push to GUI";
                MsgBasebandNotification *msg = MsgBasebandNotification::create(
                    notif.getSampleRate(), notif.getCenterFrequency());
                m_guiMessageQueue->push(msg);
            }
        }

        return true;
    }
    else
    {
        return false;
    }
}

QByteArray BeamSteeringCWMod::serialize() const
{
    return m_settings.serialize();
}

bool BeamSteeringCWMod::deserialize(const QByteArray& data)
{
    (void) data;
    if (m_settings.deserialize(data))
    {
        MsgConfigureBeamSteeringCWMod *msg = MsgConfigureBeamSteeringCWMod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureBeamSteeringCWMod *msg = MsgConfigureBeamSteeringCWMod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void BeamSteeringCWMod::validateFilterChainHash(BeamSteeringCWModSettings& settings)
{
    unsigned int s = 1;

    for (unsigned int i = 0; i < settings.m_log2Interp; i++) {
        s *= 3;
    }

    settings.m_filterChainHash = settings.m_filterChainHash >= s ? s-1 : settings.m_filterChainHash;
}

void BeamSteeringCWMod::calculateFrequencyOffset()
{
    double shiftFactor = HBFilterChainConverter::getShiftFactor(m_settings.m_log2Interp, m_settings.m_filterChainHash);
    m_frequencyOffset = m_basebandSampleRate * shiftFactor;
}

int BeamSteeringCWMod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setBeamSteeringCwModSettings(new SWGSDRangel::SWGBeamSteeringCWModSettings());
    response.getBeamSteeringCwModSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int BeamSteeringCWMod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int BeamSteeringCWMod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    BeamSteeringCWModSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureBeamSteeringCWMod *msg = MsgConfigureBeamSteeringCWMod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (getMessageQueueToGUI()) // forward to GUI if any
    {
        MsgConfigureBeamSteeringCWMod *msgToGUI = MsgConfigureBeamSteeringCWMod::create(settings, force);
        getMessageQueueToGUI()->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void BeamSteeringCWMod::webapiUpdateChannelSettings(
        BeamSteeringCWModSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("steerDegrees")) {
        settings.m_rgbColor = response.getBeamSteeringCwModSettings()->getSteerDegrees();
    }

    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getBeamSteeringCwModSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getBeamSteeringCwModSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("log2Interp")) {
        settings.m_log2Interp = response.getBeamSteeringCwModSettings()->getLog2Interp();
    }

    if (channelSettingsKeys.contains("filterChainHash"))
    {
        settings.m_filterChainHash = response.getBeamSteeringCwModSettings()->getFilterChainHash();
        validateFilterChainHash(settings);
    }

    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getBeamSteeringCwModSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getBeamSteeringCwModSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getBeamSteeringCwModSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getBeamSteeringCwModSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getBeamSteeringCwModSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getBeamSteeringCwModSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getBeamSteeringCwModSettings()->getRollupState());
    }
}

void BeamSteeringCWMod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const BeamSteeringCWModSettings& settings)
{
    response.getBeamSteeringCwModSettings()->setSteerDegrees(settings.m_steerDegrees);
    response.getBeamSteeringCwModSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getBeamSteeringCwModSettings()->getTitle()) {
        *response.getBeamSteeringCwModSettings()->getTitle() = settings.m_title;
    } else {
        response.getBeamSteeringCwModSettings()->setTitle(new QString(settings.m_title));
    }

    response.getBeamSteeringCwModSettings()->setLog2Interp(settings.m_log2Interp);
    response.getBeamSteeringCwModSettings()->setFilterChainHash(settings.m_filterChainHash);
    response.getBeamSteeringCwModSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getBeamSteeringCwModSettings()->getReverseApiAddress()) {
        *response.getBeamSteeringCwModSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getBeamSteeringCwModSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getBeamSteeringCwModSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getBeamSteeringCwModSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getBeamSteeringCwModSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getBeamSteeringCwModSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getBeamSteeringCwModSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getBeamSteeringCwModSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getBeamSteeringCwModSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getBeamSteeringCwModSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getBeamSteeringCwModSettings()->setRollupState(swgRollupState);
        }
    }
}

void BeamSteeringCWMod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const BeamSteeringCWModSettings& settings, bool force)
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

void BeamSteeringCWMod::sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        QList<QString>& channelSettingsKeys,
        const BeamSteeringCWModSettings& settings,
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

void BeamSteeringCWMod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const BeamSteeringCWModSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(2); // MIMO sink
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("BeamSteeringCWSource"));
    swgChannelSettings->setBeamSteeringCwModSettings(new SWGSDRangel::SWGBeamSteeringCWModSettings());
    SWGSDRangel::SWGBeamSteeringCWModSettings *swgBeamSteeringCWSettings = swgChannelSettings->getBeamSteeringCwModSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("steerDegrees") || force) {
        swgBeamSteeringCWSettings->setSteerDegrees(settings.m_steerDegrees);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgBeamSteeringCWSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgBeamSteeringCWSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("log2Decim") || force) {
        swgBeamSteeringCWSettings->setLog2Interp(settings.m_log2Interp);
    }
    if (channelSettingsKeys.contains("filterChainHash") || force) {
        swgBeamSteeringCWSettings->setFilterChainHash(settings.m_filterChainHash);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgBeamSteeringCWSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgBeamSteeringCWSettings->setRollupState(swgRollupState);
    }
}

void BeamSteeringCWMod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "BeamSteeringCWMod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("BeamSteeringCWMod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
