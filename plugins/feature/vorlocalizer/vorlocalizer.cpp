///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGFeatureSettings.h"
#include "SWGFeatureReport.h"
#include "SWGFeatureActions.h"
#include "SWGSimplePTTReport.h"
#include "SWGDeviceState.h"

#include "dsp/dspengine.h"

#include "vorlocalizerworker.h"
#include "vorlocalizer.h"

MESSAGE_CLASS_DEFINITION(VORLocalizer::MsgConfigureVORLocalizer, Message)
MESSAGE_CLASS_DEFINITION(VORLocalizer::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(VORLocalizer::MsgAddVORChannel, Message)
MESSAGE_CLASS_DEFINITION(VORLocalizer::MsgRemoveVORChannel, Message)
MESSAGE_CLASS_DEFINITION(VORLocalizer::MsgRefreshChannels, Message)

const char* const VORLocalizer::m_featureIdURI = "sdrangel.feature.vorlocalizer";
const char* const VORLocalizer::m_featureId = "VORLocalizer";

VORLocalizer::VORLocalizer(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface),
    m_ptt(false)
{
    setObjectName(m_featureId);
    m_worker = new VorLocalizerWorker(webAPIAdapterInterface);
    m_state = StIdle;
    m_errorMessage = "VORLocalizer error";
}

VORLocalizer::~VORLocalizer()
{
    if (m_worker->isRunning()) {
        stop();
    }

    delete m_worker;
}

void VORLocalizer::start()
{
	qDebug("VORLocalizer::start");

    m_worker->reset();
    m_worker->setMessageQueueToGUI(getMessageQueueToGUI());
    bool ok = m_worker->startWork();
    m_state = ok ? StRunning : StError;
    m_thread.start();

    VorLocalizerWorker::MsgConfigureVORLocalizerWorker *msg = VorLocalizerWorker::MsgConfigureVORLocalizerWorker::create(m_settings, true);
    m_worker->getInputMessageQueue()->push(msg);
}

void VORLocalizer::stop()
{
    qDebug("VORLocalizer::stop");
	m_worker->stopWork();
    m_state = StIdle;
	m_thread.quit();
	m_thread.wait();
}

bool VORLocalizer::handleMessage(const Message& cmd)
{
	if (MsgConfigureVORLocalizer::match(cmd))
	{
        MsgConfigureVORLocalizer& cfg = (MsgConfigureVORLocalizer&) cmd;
        qDebug() << "VORLocalizer::handleMessage: MsgConfigureVORLocalizer";
        applySettings(cfg.getSettings(), cfg.getForce());

		return true;
	}
    else if (MsgStartStop::match(cmd))
    {
        MsgStartStop& cfg = (MsgStartStop&) cmd;
        qDebug() << "VORLocalizer::handleMessage: MsgStartStop: start:" << cfg.getStartStop();

        if (cfg.getStartStop()) {
            start();
        } else {
            stop();
        }

        return true;
    }
    else if (MsgRefreshChannels::match(cmd))
    {
        qDebug() << "VORLocalizer::handleMessage: MsgRefreshChannels";
        VorLocalizerWorker::MsgRefreshChannels *msg = VorLocalizerWorker::MsgRefreshChannels::create();
        m_worker->getInputMessageQueue()->push(msg);
        return true;
    }
	else
	{
		return false;
	}
}

QByteArray VORLocalizer::serialize() const
{
    return m_settings.serialize();
}

bool VORLocalizer::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureVORLocalizer *msg = MsgConfigureVORLocalizer::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureVORLocalizer *msg = MsgConfigureVORLocalizer::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void VORLocalizer::applySettings(const VORLocalizerSettings& settings, bool force)
{
    qDebug() << "VORLocalizer::applySettings:"
            << " m_title: " << settings.m_title
            << " m_rgbColor: " << settings.m_rgbColor
            << " m_magDecAdjust: " << settings.m_magDecAdjust
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((m_settings.m_title != settings.m_title) || force) {
        reverseAPIKeys.append("title");
    }
    if ((m_settings.m_rgbColor != settings.m_rgbColor) || force) {
        reverseAPIKeys.append("rgbColor");
    }
    if ((m_settings.m_magDecAdjust != settings.m_magDecAdjust) || force) {
        reverseAPIKeys.append("magDecAdjust");
    }

    VorLocalizerWorker::MsgConfigureVORLocalizerWorker *msg = VorLocalizerWorker::MsgConfigureVORLocalizerWorker::create(
        settings, force
    );
    m_worker->getInputMessageQueue()->push(msg);

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIFeatureSetIndex != settings.m_reverseAPIFeatureSetIndex) ||
                (m_settings.m_reverseAPIFeatureIndex != settings.m_reverseAPIFeatureIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    m_settings = settings;
}

int VORLocalizer::webapiRun(bool run,
    SWGSDRangel::SWGDeviceState& response,
    QString& errorMessage)
{
    (void) errorMessage;
    getFeatureStateStr(*response.getState());
    MsgStartStop *msg = MsgStartStop::create(run);
    getInputMessageQueue()->push(msg);
    return 202;
}

int VORLocalizer::webapiSettingsGet(
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setSimplePttSettings(new SWGSDRangel::SWGSimplePTTSettings());
    response.getSimplePttSettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

int VORLocalizer::webapiSettingsPutPatch(
    bool force,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    VORLocalizerSettings settings = m_settings;
    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    MsgConfigureVORLocalizer *msg = MsgConfigureVORLocalizer::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("VORLocalizer::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureVORLocalizer *msgToGUI = MsgConfigureVORLocalizer::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);

    return 200;
}

void VORLocalizer::webapiFormatFeatureSettings(
    SWGSDRangel::SWGFeatureSettings& response,
    const VORLocalizerSettings& settings)
{
    if (response.getVorLocalizerSettings()->getTitle()) {
        *response.getVorLocalizerSettings()->getTitle() = settings.m_title;
    } else {
        response.getVorLocalizerSettings()->setTitle(new QString(settings.m_title));
    }

    response.getVorLocalizerSettings()->setRgbColor(settings.m_rgbColor);
    response.getVorLocalizerSettings()->setMagDecAdjust(settings.m_magDecAdjust);

    response.getVorLocalizerSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getVorLocalizerSettings()->getReverseApiAddress()) {
        *response.getVorLocalizerSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getVorLocalizerSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getVorLocalizerSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getVorLocalizerSettings()->setReverseApiFeatureSetIndex(settings.m_reverseAPIFeatureSetIndex);
    response.getVorLocalizerSettings()->setReverseApiFeatureIndex(settings.m_reverseAPIFeatureIndex);
}

void VORLocalizer::webapiUpdateFeatureSettings(
    VORLocalizerSettings& settings,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    if (featureSettingsKeys.contains("title")) {
        settings.m_title = *response.getVorLocalizerSettings()->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getVorLocalizerSettings()->getRgbColor();
    }
    if (featureSettingsKeys.contains("magDecAdjust")) {
        settings.m_magDecAdjust = response.getVorLocalizerSettings()->getMagDecAdjust();
    }
    if (featureSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getVorLocalizerSettings()->getUseReverseApi() != 0;
    }
    if (featureSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getVorLocalizerSettings()->getReverseApiAddress();
    }
    if (featureSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getVorLocalizerSettings()->getReverseApiPort();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureSetIndex")) {
        settings.m_reverseAPIFeatureSetIndex = response.getVorLocalizerSettings()->getReverseApiFeatureSetIndex();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureIndex")) {
        settings.m_reverseAPIFeatureIndex = response.getVorLocalizerSettings()->getReverseApiFeatureIndex();
    }
}

void VORLocalizer::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const VORLocalizerSettings& settings, bool force)
{
    SWGSDRangel::SWGFeatureSettings *swgFeatureSettings = new SWGSDRangel::SWGFeatureSettings();
    // swgFeatureSettings->setOriginatorFeatureIndex(getIndexInDeviceSet());
    // swgFeatureSettings->setOriginatorFeatureSetIndex(getDeviceSetIndex());
    swgFeatureSettings->setFeatureType(new QString("VORLocalizer"));
    swgFeatureSettings->setVorLocalizerSettings(new SWGSDRangel::SWGVORLocalizerSettings());
    SWGSDRangel::SWGVORLocalizerSettings *swgVORLocalizerSettings = swgFeatureSettings->getVorLocalizerSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("title") || force) {
        swgVORLocalizerSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgVORLocalizerSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("magDecAdjust") || force) {
        swgVORLocalizerSettings->setMagDecAdjust(settings.m_magDecAdjust);
    }

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/featureset/%3/feature/%4/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIFeatureSetIndex)
            .arg(settings.m_reverseAPIFeatureIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgFeatureSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);

    delete swgFeatureSettings;
}

void VORLocalizer::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "VORLocalizer::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("VORLocalizer::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
