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
#include "feature/feature.h"
#include "maincore.h"

#include "interferometerbaseband.h"
#include "interferometer.h"

MESSAGE_CLASS_DEFINITION(Interferometer::MsgConfigureInterferometer, Message)
MESSAGE_CLASS_DEFINITION(Interferometer::MsgBasebandNotification, Message)

const char* const Interferometer::m_channelIdURI = "sdrangel.channel.interferometer";
const char* const Interferometer::m_channelId = "Interferometer";
const int Interferometer::m_fftSize = 4096;

Interferometer::Interferometer(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamMIMO),
    m_deviceAPI(deviceAPI),
    m_spectrumVis(SDR_RX_SCALEF),
    m_guiMessageQueue(nullptr),
    m_frequencyOffset(0),
    m_deviceSampleRate(48000)
{
    setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSink = new InterferometerBaseband(m_fftSize);
    m_basebandSink->setSpectrumSink(&m_spectrumVis);
    m_basebandSink->setScopeSink(&m_scopeSink);
    m_basebandSink->moveToThread(m_thread);
    m_deviceAPI->addMIMOChannel(this);
    m_deviceAPI->addMIMOChannelAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &Interferometer::networkManagerFinished
    );
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
    delete m_basebandSink;
    delete m_thread;
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
    if (m_deviceSampleRate != 0) {
        m_basebandSink->setBasebandSampleRate(m_deviceSampleRate);
    }

    m_basebandSink->reset();
    m_thread->start();

    InterferometerBaseband::MsgConfigureChannelizer *msg = InterferometerBaseband::MsgConfigureChannelizer::create(
        m_settings.m_log2Decim, m_settings.m_filterChainHash);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void Interferometer::stopSinks()
{
	m_thread->exit();
	m_thread->wait();
}

void Interferometer::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, unsigned int sinkIndex)
{
    m_basebandSink->feed(begin, end, sinkIndex);
}

void Interferometer::pull(SampleVector::iterator& begin, unsigned int nbSamples, unsigned int sourceIndex)
{
    (void) begin;
    (void) nbSamples;
    (void) sourceIndex;
}

void Interferometer::applySettings(const InterferometerSettings& settings, bool force)
{
    qDebug() << "Interferometer::applySettings: "
        << "m_correlationType: " << settings.m_correlationType
        << "m_filterChainHash: " << settings.m_filterChainHash
        << "m_log2Decim: " << settings.m_log2Decim
        << "m_phase: " << settings.m_phase
        << "m_useReverseAPI: " << settings.m_useReverseAPI
        << "m_reverseAPIAddress: " << settings.m_reverseAPIAddress
        << "m_reverseAPIPort: " << settings.m_reverseAPIPort
        << "m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex
        << "m_reverseAPIChannelIndex: " << settings.m_reverseAPIChannelIndex
        << "m_title: " << settings.m_title;

    QList<QString> reverseAPIKeys;

    if ((m_settings.m_correlationType != settings.m_correlationType) || force) {
        reverseAPIKeys.append("correlationType");
    }
    if ((m_settings.m_filterChainHash != settings.m_filterChainHash) || force) {
        reverseAPIKeys.append("filterChainHash");
    }
    if ((m_settings.m_log2Decim != settings.m_log2Decim) || force) {
        reverseAPIKeys.append("log2Decim");
    }
    if ((m_settings.m_phase != settings.m_phase) || force) {
        reverseAPIKeys.append("phase");
    }
    if ((m_settings.m_title != settings.m_title) || force) {
        reverseAPIKeys.append("title");
    }

    if ((m_settings.m_log2Decim != settings.m_log2Decim)
     || (m_settings.m_filterChainHash != settings.m_filterChainHash) || force)
    {
        InterferometerBaseband::MsgConfigureChannelizer *msg = InterferometerBaseband::MsgConfigureChannelizer::create(
            settings.m_log2Decim, settings.m_filterChainHash);
        m_basebandSink->getInputMessageQueue()->push(msg);
    }

    if ((m_settings.m_correlationType != settings.m_correlationType) || force)
    {
        InterferometerBaseband::MsgConfigureCorrelation *msg = InterferometerBaseband::MsgConfigureCorrelation::create(
            settings.m_correlationType);
        m_basebandSink->getInputMessageQueue()->push(msg);
    }

    if ((m_settings.m_phase != settings.m_phase) || force) {
        m_basebandSink->setPhase(settings.m_phase);
    }

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "settings", pipes);

    if (pipes.size() > 0) {
        sendChannelSettings(pipes, reverseAPIKeys, settings, force);
    }

    m_settings = settings;
}

void Interferometer::handleInputMessages()
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

bool Interferometer::handleMessage(const Message& cmd)
{
    if (MsgConfigureInterferometer::match(cmd))
    {
        MsgConfigureInterferometer& cfg = (MsgConfigureInterferometer&) cmd;
        qDebug() << "Interferometer::handleMessage: MsgConfigureInterferometer";
        applySettings(cfg.getSettings(), cfg.getForce());
        return true;
    }
    else if (DSPMIMOSignalNotification::match(cmd))
    {
        DSPMIMOSignalNotification& notif = (DSPMIMOSignalNotification&) cmd;

        qDebug() << "Interferometer::handleMessage: DSPMIMOSignalNotification:"
                << " inputSampleRate: " << notif.getSampleRate()
                << " centerFrequency: " << notif.getCenterFrequency()
                << " sourceElseSink: " << notif.getSourceOrSink()
                << " streamIndex: " << notif.getIndex();

        if (notif.getSourceOrSink()) // deals with source messages only
        {
            m_deviceSampleRate = notif.getSampleRate();
            calculateFrequencyOffset(); // This is when device sample rate changes

            // Notify baseband sink of input sample rate change
            InterferometerBaseband::MsgSignalNotification *sig = InterferometerBaseband::MsgSignalNotification::create(
                m_deviceSampleRate, notif.getCenterFrequency(), notif.getIndex()
            );
            qDebug() << "Interferometer::handleMessage: DSPMIMOSignalNotification: push to sink";
            m_basebandSink->getInputMessageQueue()->push(sig);

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
        MsgConfigureInterferometer *msg = MsgConfigureInterferometer::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureInterferometer *msg = MsgConfigureInterferometer::create(m_settings, true);
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

void Interferometer::calculateFrequencyOffset()
{
    double shiftFactor = HBFilterChainConverter::getShiftFactor(m_settings.m_log2Decim, m_settings.m_filterChainHash);
    m_frequencyOffset = m_deviceSampleRate * shiftFactor;
}

void Interferometer::applyChannelSettings(uint32_t log2Decim, uint32_t filterChainHash)
{
    InterferometerBaseband::MsgConfigureChannelizer *msg = InterferometerBaseband::MsgConfigureChannelizer::create(log2Decim, filterChainHash);
    m_basebandSink->getInputMessageQueue()->push(msg);
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

    MsgConfigureInterferometer *msg = MsgConfigureInterferometer::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (getMessageQueueToGUI()) // forward to GUI if any
    {
        MsgConfigureInterferometer *msgToGUI = MsgConfigureInterferometer::create(settings, force);
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
        settings.m_reverseAPIPort = response.getInterferometerSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getInterferometerSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getInterferometerSettings()->getReverseApiChannelIndex();
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
            SWGSDRangel::SWGGLSpectrum *swgGLSpectrum = new SWGSDRangel::SWGGLSpectrum();
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
            SWGSDRangel::SWGGLScope *swgGLScope = new SWGSDRangel::SWGGLScope();
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
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
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
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getInterferometerSettings()->setRollupState(swgRollupState);
        }
    }
}

void Interferometer::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const InterferometerSettings& settings, bool force)
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
    reply->setParent(buffer);

    delete swgChannelSettings;
}

void Interferometer::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const InterferometerSettings& settings,
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

void Interferometer::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const InterferometerSettings& settings,
        bool force
)
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
    if (channelSettingsKeys.contains("filterChainHash") || force) {
        swgInterferometerSettings->setFilterChainHash(settings.m_filterChainHash);
    }

    if (settings.m_spectrumGUI)
    {
        if (channelSettingsKeys.contains("spectrumConfig") || force) {
            settings.m_spectrumGUI->formatTo(swgInterferometerSettings->getSpectrumConfig());
        }
    }

    if (settings.m_scopeGUI)
    {
        if (channelSettingsKeys.contains("scopeConfig") || force) {
            settings.m_scopeGUI->formatTo(swgInterferometerSettings->getScopeConfig());
        }
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgInterferometerSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgInterferometerSettings->setRollupState(swgRollupState);
    }
}

void Interferometer::networkManagerFinished(QNetworkReply *reply)
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
