///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2021-2024 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#include <algorithm>

#include "SWGFeatureSettings.h"
#include "SWGFeatureReport.h"
#include "SWGAISReport.h"
#include "SWGAISVessel.h"

#include "feature/featureset.h"
#include "settings/serializable.h"
#include "maincore.h"

#include "ais.h"

MESSAGE_CLASS_DEFINITION(AIS::MsgConfigureAIS, Message)
MESSAGE_CLASS_DEFINITION(AIS::MsgReportVessels, Message)

const char* const AIS::m_featureIdURI = "sdrangel.feature.ais";
const char* const AIS::m_featureId = "AIS";

AIS::AIS(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface),
    m_availableChannelHandler({"sdrangel.channel.aisdemod"}, QStringList{"ais"})
{
    qDebug("AIS::AIS: webAPIAdapterInterface: %p", webAPIAdapterInterface);
    setObjectName(m_featureId);
    setState(StIdle);
    m_errorMessage = "AIS error";
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AIS::networkManagerFinished
    );
    QObject::connect(
        &m_availableChannelHandler,
        &AvailableChannelOrFeatureHandler::messageEnqueued,
        this,
        &AIS::handleChannelMessageQueue);
}

AIS::~AIS()
{
    QObject::disconnect(
        &m_availableChannelHandler,
        &AvailableChannelOrFeatureHandler::messageEnqueued,
        this,
        &AIS::handleChannelMessageQueue);
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AIS::networkManagerFinished
    );
    delete m_networkManager;
}

void AIS::start()
{
    qDebug("AIS::start");
    setState(StRunning);
}

void AIS::stop()
{
    qDebug("AIS::stop");
    setState(StIdle);
}

bool AIS::handleMessage(const Message& cmd)
{
    if (MsgConfigureAIS::match(cmd))
    {
        MsgConfigureAIS& cfg = (MsgConfigureAIS&) cmd;
        qDebug() << "AIS::handleMessage: MsgConfigureAIS";
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

        return true;
    }
    else if (MsgReportVessels::match(cmd))
    {
        MsgReportVessels& report = (MsgReportVessels&) cmd;
        m_vessels = report.getVessels();
        m_vesselsUpdated = QDateTime::currentDateTimeUtc();

        return true;
    }
    else if (MainCore::MsgPacket::match(cmd))
    {
        MainCore::MsgPacket& report = (MainCore::MsgPacket&) cmd;
        if (getMessageQueueToGUI())
        {
            MainCore::MsgPacket *copy = new MainCore::MsgPacket(report);
            getMessageQueueToGUI()->push(copy);
        }
        return true;
    }
    else
    {
        return false;
    }
}

QByteArray AIS::serialize() const
{
    return m_settings.serialize();
}

bool AIS::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureAIS *msg = MsgConfigureAIS::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureAIS *msg = MsgConfigureAIS::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void AIS::applySettings(const AISSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "AIS::applySettings:" << settings.getDebugString(settingsKeys, force) << force;

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

int AIS::webapiSettingsGet(
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setAisSettings(new SWGSDRangel::SWGAISSettings());
    response.getAisSettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

int AIS::webapiSettingsPutPatch(
    bool force,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    AISSettings settings = m_settings;
    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    MsgConfigureAIS *msg = MsgConfigureAIS::create(settings, featureSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAIS *msgToGUI = MsgConfigureAIS::create(settings, featureSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);

    return 200;
}

void AIS::webapiFormatFeatureSettings(
    SWGSDRangel::SWGFeatureSettings& response,
    const AISSettings& settings)
{
    if (response.getAisSettings()->getTitle()) {
        *response.getAisSettings()->getTitle() = settings.m_title;
    } else {
        response.getAisSettings()->setTitle(new QString(settings.m_title));
    }

    response.getAisSettings()->setRgbColor(settings.m_rgbColor);
    response.getAisSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getAisSettings()->getReverseApiAddress()) {
        *response.getAisSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getAisSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getAisSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getAisSettings()->setReverseApiFeatureSetIndex(settings.m_reverseAPIFeatureSetIndex);
    response.getAisSettings()->setReverseApiFeatureIndex(settings.m_reverseAPIFeatureIndex);

    if (settings.m_rollupState)
    {
        if (response.getAisSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getAisSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getAisSettings()->setRollupState(swgRollupState);
        }
    }

    if (!response.getAisSettings()->getVesselColumnIndexes()) {
        response.getAisSettings()->setVesselColumnIndexes(new QList<int>());
    }

    response.getAisSettings()->getVesselColumnIndexes()->clear();

    for (int i = 0; i < AIS_VESSEL_COLUMNS; i++) {
        response.getAisSettings()->getVesselColumnIndexes()->push_back(settings.m_vesselColumnIndexes[i]);
    }

    if (!response.getAisSettings()->getVesselColumnSizes()) {
        response.getAisSettings()->setVesselColumnSizes(new QList<int>());
    }

    response.getAisSettings()->getVesselColumnSizes()->clear();

    for (int i = 0; i < AIS_VESSEL_COLUMNS; i++) {
        response.getAisSettings()->getVesselColumnSizes()->push_back(settings.m_vesselColumnSizes[i]);
    }
}


int AIS::webapiReportGet(
    SWGSDRangel::SWGFeatureReport& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setAisReport(new SWGSDRangel::SWGAISReport());
    response.getAisReport()->init();
    webapiFormatFeatureReport(response);
    return 200;
}

void AIS::webapiFormatFeatureReport(SWGSDRangel::SWGFeatureReport& response)
{
    SWGSDRangel::SWGAISReport *report = response.getAisReport();
    report->setVesselCount(m_vessels.size());
    if (report->getReportDateTime()) {
        *report->getReportDateTime() = m_vesselsUpdated.toString(Qt::ISODateWithMs);
    } else {
        report->setReportDateTime(new QString(m_vesselsUpdated.toString(Qt::ISODateWithMs)));
    }
    report->setVessels(new QList<SWGSDRangel::SWGAISVessel *>);

    for (const auto& vessel : m_vessels)
    {
        SWGSDRangel::SWGAISVessel *swgVessel = new SWGSDRangel::SWGAISVessel();
        report->getVessels()->append(swgVessel);
        if (swgVessel->getMmsi()) {
            *swgVessel->getMmsi() = vessel.m_mmsi;
        } else {
            swgVessel->setMmsi(new QString(vessel.m_mmsi));
        }
        swgVessel->setMessages(vessel.m_messages);

        // Only what has been received is set, so that the rest is left out of the JSON rather
        // than reported as a zero a caller cannot tell from a real value
        if (!vessel.m_name.isEmpty()) {
            if (swgVessel->getName()) {
                *swgVessel->getName() = vessel.m_name;
            } else {
                swgVessel->setName(new QString(vessel.m_name));
            }
        }
        if (!vessel.m_callsign.isEmpty()) {
            if (swgVessel->getCallsign()) {
                *swgVessel->getCallsign() = vessel.m_callsign;
            } else {
                swgVessel->setCallsign(new QString(vessel.m_callsign));
            }
        }
        if (!vessel.m_imo.isEmpty()) {
            if (swgVessel->getImo()) {
                *swgVessel->getImo() = vessel.m_imo;
            } else {
                swgVessel->setImo(new QString(vessel.m_imo));
            }
        }
        if (!vessel.m_country.isEmpty()) {
            if (swgVessel->getCountry()) {
                *swgVessel->getCountry() = vessel.m_country;
            } else {
                swgVessel->setCountry(new QString(vessel.m_country));
            }
        }
        if (!vessel.m_type.isEmpty()) {
            if (swgVessel->getType()) {
                *swgVessel->getType() = vessel.m_type;
            } else {
                swgVessel->setType(new QString(vessel.m_type));
            }
        }
        if (!vessel.m_shipType.isEmpty()) {
            if (swgVessel->getShipType()) {
                *swgVessel->getShipType() = vessel.m_shipType;
            } else {
                swgVessel->setShipType(new QString(vessel.m_shipType));
            }
        }
        if (!vessel.m_status.isEmpty()) {
            if (swgVessel->getStatus()) {
                *swgVessel->getStatus() = vessel.m_status;
            } else {
                swgVessel->setStatus(new QString(vessel.m_status));
            }
        }
        if (!vessel.m_destination.isEmpty()) {
            if (swgVessel->getDestination()) {
                *swgVessel->getDestination() = vessel.m_destination;
            } else {
                swgVessel->setDestination(new QString(vessel.m_destination));
            }
        }
        if (vessel.m_hasPosition)
        {
            swgVessel->setLatitude(vessel.m_latitude);
            swgVessel->setLongitude(vessel.m_longitude);
        }
        if (vessel.m_hasCourse) {
            swgVessel->setCourse(vessel.m_course);
        }
        if (vessel.m_hasSpeed) {
            swgVessel->setSpeed(vessel.m_speed);
        }
        if (vessel.m_hasHeading) {
            swgVessel->setHeading(vessel.m_heading);
        }
        if (vessel.m_hasLength) {
            swgVessel->setLength(vessel.m_length);
        }
        if (vessel.m_positionUpdate.isValid()) {
            if (swgVessel->getPositionUpdate()) {
                *swgVessel->getPositionUpdate() = vessel.m_positionUpdate.toString(Qt::ISODateWithMs);
            } else {
                swgVessel->setPositionUpdate(new QString(vessel.m_positionUpdate.toString(Qt::ISODateWithMs)));
            }
        }
        if (vessel.m_lastUpdate.isValid()) {
            if (swgVessel->getLastUpdate()) {
                *swgVessel->getLastUpdate() = vessel.m_lastUpdate.toString(Qt::ISODateWithMs);
            } else {
                swgVessel->setLastUpdate(new QString(vessel.m_lastUpdate.toString(Qt::ISODateWithMs)));
            }
        }
    }
}

void AIS::webapiUpdateFeatureSettings(
    AISSettings& settings,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    if (featureSettingsKeys.contains("title")) {
        settings.m_title = *response.getAisSettings()->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getAisSettings()->getRgbColor();
    }
    if (featureSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getAisSettings()->getUseReverseApi() != 0;
    }
    if (featureSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getAisSettings()->getReverseApiAddress();
    }
    if (featureSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getAisSettings()->getReverseApiPort();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureSetIndex")) {
        settings.m_reverseAPIFeatureSetIndex = response.getAisSettings()->getReverseApiFeatureSetIndex();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureIndex")) {
        settings.m_reverseAPIFeatureIndex = response.getAisSettings()->getReverseApiFeatureIndex();
    }
    if (settings.m_rollupState && featureSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(featureSettingsKeys, response.getAisSettings()->getRollupState());
    }

    if (featureSettingsKeys.contains("vesselColumnIndexes"))
    {
        const QList<int> *indexes = response.getAisSettings()->getVesselColumnIndexes();
        int count = std::min(AIS_VESSEL_COLUMNS, (int)indexes->size());

        for (int i = 0; i < count; i++) {
            settings.m_vesselColumnIndexes[i] = (*indexes)[i];
        }
    }

    if (featureSettingsKeys.contains("vesselColumnSizes"))
    {
        const QList<int> *indexes = response.getAisSettings()->getVesselColumnSizes();
        int count = std::min(AIS_VESSEL_COLUMNS, (int)indexes->size());

        for (int i = 0; i < count; i++) {
            settings.m_vesselColumnSizes[i] = (*indexes)[i];
        }
    }
}

void AIS::webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const AISSettings& settings, bool force)
{
    SWGSDRangel::SWGFeatureSettings *swgFeatureSettings = new SWGSDRangel::SWGFeatureSettings();
    // swgFeatureSettings->setOriginatorFeatureIndex(getIndexInDeviceSet());
    // swgFeatureSettings->setOriginatorFeatureSetIndex(getDeviceSetIndex());
    swgFeatureSettings->setFeatureType(new QString("AIS"));
    swgFeatureSettings->setAisSettings(new SWGSDRangel::SWGAISSettings());
    SWGSDRangel::SWGAISSettings *swgAISSettings = swgFeatureSettings->getAisSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (featureSettingsKeys.contains("title") || force) {
        swgAISSettings->setTitle(new QString(settings.m_title));
    }
    if (featureSettingsKeys.contains("rgbColor") || force) {
        swgAISSettings->setRgbColor(settings.m_rgbColor);
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

void AIS::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "AIS::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("AIS::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void AIS::handleChannelMessageQueue(MessageQueue* messageQueue)
{
    Message* message;

    while ((message = messageQueue->pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}
