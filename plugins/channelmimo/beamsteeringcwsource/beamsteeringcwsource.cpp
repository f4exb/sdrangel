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

#include "device/deviceapi.h"
#include "dsp/upchannelizer.h"
#include "dsp/threadedbasebandsamplesource.h"
#include "dsp/hbfilterchainconverter.h"
#include "dsp/dspcommands.h"

#include "SWGChannelSettings.h"

#include "beamsteeringcwsourceworker.h"
#include "beamsteeringcwsource.h"

MESSAGE_CLASS_DEFINITION(BeamSteeringCWSource::MsgConfigureBeamSteeringCWSource, Message)
MESSAGE_CLASS_DEFINITION(BeamSteeringCWSource::MsgBasebandNotification, Message)

const QString BeamSteeringCWSource::m_channelIdURI = "sdrangel.channel.beamsteeringcwsource";
const QString BeamSteeringCWSource::m_channelId = "BeamSteeringCWSource";

BeamSteeringCWSource::BeamSteeringCWSource(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamMIMO),
    m_deviceAPI(deviceAPI),
    m_guiMessageQueue(nullptr),
    m_frequencyOffset(0),
    m_deviceSampleRate(48000)
{
    setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_sourceWorker = new BeamSteeringCWSourceWorker();
    m_sourceWorker->moveToThread(m_thread);
    m_deviceAPI->addMIMOChannel(this);
    m_deviceAPI->addMIMOChannelAPI(this);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

BeamSteeringCWSource::~BeamSteeringCWSource()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;

    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeMIMOChannel(this);
    delete m_sourceWorker;
    delete m_thread;
}

void BeamSteeringCWSource::startSources()
{
    m_sourceWorker->reset();
    m_thread->start();
}

void BeamSteeringCWSource::stopSources()
{
	m_thread->exit();
	m_thread->wait();
}

void BeamSteeringCWSource::pull(const SampleVector::const_iterator& begin, unsigned int nbSamples, unsigned int sourceIndex)
{
    m_sourceWorker->pull(begin, nbSamples, sourceIndex);
}

void BeamSteeringCWSource::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, unsigned int sinkIndex)
{
    (void) begin;
    (void) end;
    (void) sinkIndex;
}

void BeamSteeringCWSource::applySettings(const BeamSteeringCWSourceSettings& settings, bool force)
{
    qDebug() << "BeamSteeringCWSource::applySettings: "
        << "m_steerDegrees: " << settings.m_steerDegrees
        << "m_filterChainHash: " << settings.m_filterChainHash
        << "m_log2Interp: " << settings.m_log2Interp
        << "m_useReverseAPI: " << settings.m_useReverseAPI
        << "m_reverseAPIAddress: " << settings.m_reverseAPIAddress
        << "m_reverseAPIPort: " << settings.m_reverseAPIPort
        << "m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex
        << "m_reverseAPIChannelIndex: " << settings.m_reverseAPIChannelIndex
        << "m_title: " << settings.m_title;

    if ((m_settings.m_log2Interp != settings.m_log2Interp)
     || (m_settings.m_filterChainHash != settings.m_filterChainHash) || force)
    {
        BeamSteeringCWSourceWorker::MsgConfigureChannelizer *msg = BeamSteeringCWSourceWorker::MsgConfigureChannelizer::create(
            settings.m_log2Interp, settings.m_filterChainHash);
        m_sourceWorker->getInputMessageQueue()->push(msg);
    }

    if ((m_settings.m_steerDegrees != settings.m_steerDegrees) || force) {
        m_sourceWorker->setSteeringDegrees(settings.m_steerDegrees);
    }

    m_settings = settings;
}

void BeamSteeringCWSource::handleInputMessages()
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

bool BeamSteeringCWSource::handleMessage(const Message& cmd)
{
    if (MsgConfigureBeamSteeringCWSource::match(cmd))
    {
        MsgConfigureBeamSteeringCWSource& cfg = (MsgConfigureBeamSteeringCWSource&) cmd;
        qDebug() << "BeamSteeringCWSource::handleMessage: MsgConfigureBeamSteeringCWSource";
        applySettings(cfg.getSettings(), cfg.getForce());
        return true;
    }
    else if (DSPMIMOSignalNotification::match(cmd))
    {
        DSPMIMOSignalNotification& notif = (DSPMIMOSignalNotification&) cmd;

        qDebug() << "BeamSteeringCWSource::handleMessage: DSPMIMOSignalNotification:"
                << " outputSampleRate: " << notif.getSampleRate()
                << " centerFrequency: " << notif.getCenterFrequency()
                << " sourceElseSink: " << notif.getSourceOrSink()
                << " streamIndex: " << notif.getIndex();

        if (!notif.getSourceOrSink()) // deals with sink messages only
        {
            m_deviceSampleRate = notif.getSampleRate();
            calculateFrequencyOffset(); // This is when device sample rate changes

            // Notify sink of input sample rate change
            BeamSteeringCWSourceWorker::MsgSignalNotification *sig = BeamSteeringCWSourceWorker::MsgSignalNotification::create(
                m_deviceSampleRate, notif.getCenterFrequency(), notif.getIndex()
            );
            qDebug() << "BeamSteeringCWSource::handleMessage: DSPMIMOSignalNotification: push to worker";
            m_sourceWorker->getInputMessageQueue()->push(sig);

            // Redo the channelizer stuff with the new sample rate to re-synchronize everything
            BeamSteeringCWSourceWorker::MsgConfigureChannelizer *msg = BeamSteeringCWSourceWorker::MsgConfigureChannelizer::create(
                m_settings.m_log2Interp,
                m_settings.m_filterChainHash);
            m_sourceWorker->getInputMessageQueue()->push(msg);

            if (m_guiMessageQueue)
            {
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

QByteArray BeamSteeringCWSource::serialize() const
{
    return m_settings.serialize();
}

bool BeamSteeringCWSource::deserialize(const QByteArray& data)
{
    (void) data;
    if (m_settings.deserialize(data))
    {
        MsgConfigureBeamSteeringCWSource *msg = MsgConfigureBeamSteeringCWSource::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureBeamSteeringCWSource *msg = MsgConfigureBeamSteeringCWSource::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void BeamSteeringCWSource::validateFilterChainHash(BeamSteeringCWSourceSettings& settings)
{
    unsigned int s = 1;

    for (unsigned int i = 0; i < settings.m_log2Interp; i++) {
        s *= 3;
    }

    settings.m_filterChainHash = settings.m_filterChainHash >= s ? s-1 : settings.m_filterChainHash;
}

void BeamSteeringCWSource::calculateFrequencyOffset()
{
    double shiftFactor = HBFilterChainConverter::getShiftFactor(m_settings.m_log2Interp, m_settings.m_filterChainHash);
    m_frequencyOffset = m_deviceSampleRate * shiftFactor;
}

void BeamSteeringCWSource::applyChannelSettings(uint32_t log2Interp, uint32_t filterChainHash)
{
    BeamSteeringCWSourceWorker::MsgConfigureChannelizer *msg = BeamSteeringCWSourceWorker::MsgConfigureChannelizer::create(log2Interp, filterChainHash);
    m_sourceWorker->getInputMessageQueue()->push(msg);
}

int BeamSteeringCWSource::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setBeamSteeringCwSourceSettings(new SWGSDRangel::SWGBeamSteeringCWSourceSettings());
    response.getBeamSteeringCwSourceSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int BeamSteeringCWSource::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    BeamSteeringCWSourceSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureBeamSteeringCWSource *msg = MsgConfigureBeamSteeringCWSource::create(settings, force);
    m_inputMessageQueue.push(msg);

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void BeamSteeringCWSource::webapiUpdateChannelSettings(
        BeamSteeringCWSourceSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("steerDegrees")) {
        settings.m_rgbColor = response.getBeamSteeringCwSourceSettings()->getSteerDegrees();
    }

    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getBeamSteeringCwSourceSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getBeamSteeringCwSourceSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("log2Interp")) {
        settings.m_log2Interp = response.getBeamSteeringCwSourceSettings()->getLog2Interp();
    }

    if (channelSettingsKeys.contains("filterChainHash"))
    {
        settings.m_filterChainHash = response.getBeamSteeringCwSourceSettings()->getFilterChainHash();
        validateFilterChainHash(settings);
    }

    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getBeamSteeringCwSourceSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getBeamSteeringCwSourceSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getBeamSteeringCwSourceSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getBeamSteeringCwSourceSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getBeamSteeringCwSourceSettings()->getReverseApiChannelIndex();
    }
}

void BeamSteeringCWSource::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const BeamSteeringCWSourceSettings& settings)
{
    response.getBeamSteeringCwSourceSettings()->setSteerDegrees(settings.m_steerDegrees);
    response.getBeamSteeringCwSourceSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getBeamSteeringCwSourceSettings()->getTitle()) {
        *response.getBeamSteeringCwSourceSettings()->getTitle() = settings.m_title;
    } else {
        response.getBeamSteeringCwSourceSettings()->setTitle(new QString(settings.m_title));
    }

    response.getBeamSteeringCwSourceSettings()->setLog2Interp(settings.m_log2Interp);
    response.getBeamSteeringCwSourceSettings()->setFilterChainHash(settings.m_filterChainHash);
    response.getBeamSteeringCwSourceSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getBeamSteeringCwSourceSettings()->getReverseApiAddress()) {
        *response.getBeamSteeringCwSourceSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getBeamSteeringCwSourceSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getBeamSteeringCwSourceSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getBeamSteeringCwSourceSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getBeamSteeringCwSourceSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
}

void BeamSteeringCWSource::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const BeamSteeringCWSourceSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    swgChannelSettings->setDirection(2); // MIMO sink
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("BeamSteeringCWSource"));
    swgChannelSettings->setBeamSteeringCwSourceSettings(new SWGSDRangel::SWGBeamSteeringCWSourceSettings());
    SWGSDRangel::SWGBeamSteeringCWSourceSettings *swgBeamSteeringCWSettings = swgChannelSettings->getBeamSteeringCwSourceSettings();

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

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/channel/%4/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex)
            .arg(settings.m_reverseAPIChannelIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer=new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgChannelSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);

    delete swgChannelSettings;
}

void BeamSteeringCWSource::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "BeamSteeringCWSource::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
        return;
    }

    QString answer = reply->readAll();
    answer.chop(1); // remove last \n
    qDebug("BeamSteeringCWSource::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
}
