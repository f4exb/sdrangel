///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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
#include "SWGDVSerialDevices.h"
#include "SWGDVSerialDevice.h"
#include "SWGAMBEDevices.h"

#include "settings/serializable.h"
#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"

#include "ambe.h"

MESSAGE_CLASS_DEFINITION(AMBE::MsgConfigureAMBE, Message)
MESSAGE_CLASS_DEFINITION(AMBE::MsgReportDevices, Message)

const char* const AMBE::m_featureIdURI = "sdrangel.feature.ambe";
const char* const AMBE::m_featureId = "AMBE";

AMBE::AMBE(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface)
{
    setObjectName(m_featureId);
    m_state = StIdle;
    m_errorMessage = "AMBE error";
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AMBE::networkManagerFinished
    );
}

AMBE::~AMBE()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AMBE::networkManagerFinished
    );
    delete m_networkManager;
}

void AMBE::start()
{
	qDebug("AMBE::start");
    m_state = StRunning;
}

void AMBE::stop()
{
    qDebug("AMBE::stop");
    m_state = StIdle;
}

void AMBE::applySettings(const AMBESettings& settings, const QList<QString>& settingsKeys,  bool force)
{
    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

bool AMBE::handleMessage(const Message& cmd)
{
	if (MsgConfigureAMBE::match(cmd))
	{
        MsgConfigureAMBE& cfg = (MsgConfigureAMBE&) cmd;
        qDebug() << "AMBE::handleMessage: MsgConfigureAMBE";
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());
		return true;
	}
    else if (DSPPushMbeFrame::match(cmd))
    {
        DSPPushMbeFrame& cfg = (DSPPushMbeFrame&) cmd;
        m_ambeEngine.pushMbeFrame(
            cfg.getMbeFrame(),
            cfg.getMbeRateIndex(),
            cfg.getMbeVolumeIndex(),
            cfg.getChannels(),
            cfg.getUseHP(),
            cfg.getUpsampling(),
            cfg.getAudioFifo()
        );
        return true;
    }

    return false;
}

QByteArray AMBE::serialize() const
{
    SimpleSerializer s(1);
    s.writeBlob(1, m_settings.serialize());
    return s.final();
}

bool AMBE::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        m_settings.resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray bytetmp;
        d.readBlob(1, &bytetmp);

        if (m_settings.deserialize(bytetmp))
        {
            MsgConfigureAMBE *msg = MsgConfigureAMBE::create(m_settings, QList<QString>(), true);
            m_inputMessageQueue.push(msg);
            return true;
        }
        else
        {
            m_settings.resetToDefaults();
            MsgConfigureAMBE *msg = MsgConfigureAMBE::create(m_settings, QList<QString>(), true);
            m_inputMessageQueue.push(msg);
            return false;
        }
    }
    else
    {
        return false;
    }
}

void AMBE::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "AMBE::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("AMBE::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

int AMBE::webapiSettingsGet(
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setAmbeSettings(new SWGSDRangel::SWGAMBESettings());
    response.getAmbeSettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

int AMBE::webapiSettingsPutPatch(
    bool force,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    AMBESettings settings = m_settings;
    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    MsgConfigureAMBE *msg = MsgConfigureAMBE::create(settings, featureSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAMBE *msgToGUI = MsgConfigureAMBE::create(settings, featureSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);

    return 200;
}

int AMBE::webapiReportGet(
    SWGSDRangel::SWGFeatureReport& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setAmbeReport(new SWGSDRangel::SWGAMBEReport());
    response.getAmbeReport()->init();
    webapiFormatFeatureReport(response);
    return 200;
}

void AMBE::webapiFormatFeatureSettings(
    SWGSDRangel::SWGFeatureSettings& response,
    const AMBESettings& settings)
{
    if (response.getAmbeSettings()->getTitle()) {
        *response.getAmbeSettings()->getTitle() = settings.m_title;
    } else {
        response.getAmbeSettings()->setTitle(new QString(settings.m_title));
    }

    response.getAmbeSettings()->setRgbColor(settings.m_rgbColor);
    response.getAmbeSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getAmbeSettings()->getReverseApiAddress()) {
        *response.getAmbeSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getAmbeSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getAmbeSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getAmbeSettings()->setReverseApiFeatureSetIndex(settings.m_reverseAPIFeatureSetIndex);
    response.getAmbeSettings()->setReverseApiFeatureIndex(settings.m_reverseAPIFeatureIndex);

    if (settings.m_rollupState)
    {
        if (response.getAmbeSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getAmbeSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getAmbeSettings()->setRollupState(swgRollupState);
        }
    }
}

void AMBE::webapiUpdateFeatureSettings(
    AMBESettings& settings,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    if (featureSettingsKeys.contains("title")) {
        settings.m_title = *response.getAmbeSettings()->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getAmbeSettings()->getRgbColor();
    }
    if (featureSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getAmbeSettings()->getUseReverseApi() != 0;
    }
    if (featureSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getAmbeSettings()->getReverseApiAddress();
    }
    if (featureSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getAmbeSettings()->getReverseApiPort();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureSetIndex")) {
        settings.m_reverseAPIFeatureSetIndex = response.getAmbeSettings()->getReverseApiFeatureSetIndex();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureIndex")) {
        settings.m_reverseAPIFeatureIndex = response.getAmbeSettings()->getReverseApiFeatureIndex();
    }
    if (settings.m_rollupState && featureSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(featureSettingsKeys, response.getAmbeSettings()->getRollupState());
    }
}

void AMBE::webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const AMBESettings& settings, bool force)
{
    SWGSDRangel::SWGFeatureSettings *swgFeatureSettings = new SWGSDRangel::SWGFeatureSettings();
    // swgFeatureSettings->setOriginatorFeatureIndex(getIndexInDeviceSet());
    // swgFeatureSettings->setOriginatorFeatureSetIndex(getDeviceSetIndex());
    swgFeatureSettings->setFeatureType(new QString("AMBE"));
    swgFeatureSettings->setAmbeSettings(new SWGSDRangel::SWGAMBESettings());
    SWGSDRangel::SWGAMBESettings *swgAMBESettings = swgFeatureSettings->getAmbeSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (featureSettingsKeys.contains("title") || force) {
        swgAMBESettings->setTitle(new QString(settings.m_title));
    }
    if (featureSettingsKeys.contains("rgbColor") || force) {
        swgAMBESettings->setRgbColor(settings.m_rgbColor);
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

void AMBE::webapiFormatFeatureReport(SWGSDRangel::SWGFeatureReport& response)
{
    // serial

    SWGSDRangel::SWGDVSerialDevices *swgDVSerialDevices = response.getAmbeReport()->getSerial();
    swgDVSerialDevices->init();

    QList<QString> qDeviceNames;
    getAMBEEngine()->scan(qDeviceNames);
    swgDVSerialDevices->setNbDevices((int) qDeviceNames.size());
    QList<SWGSDRangel::SWGDVSerialDevice*> *serialDeviceNamesList = swgDVSerialDevices->getDvSerialDevices();

    for (const auto& deviceName : qDeviceNames)
    {
        serialDeviceNamesList->append(new SWGSDRangel::SWGDVSerialDevice);
        serialDeviceNamesList->back()->init();
        *serialDeviceNamesList->back()->getDeviceName() = deviceName;
    }

    // devices

    response.getAmbeReport()->setDevices(new QList<SWGSDRangel::SWGAMBEDeviceReport*>);
    QList<AMBEEngine::DeviceRef> deviceRefs;
    getAMBEEngine()->getDeviceRefs(deviceRefs);

    for (auto& deviceRef : deviceRefs)
    {
        response.getAmbeReport()->getDevices()->append(new SWGSDRangel::SWGAMBEDeviceReport);
        response.getAmbeReport()->getDevices()->back()->setDevicePath(new QString(deviceRef.m_devicePath));
        response.getAmbeReport()->getDevices()->back()->setSuccessCount(deviceRef.m_successCount);
        response.getAmbeReport()->getDevices()->back()->setFailureCount(deviceRef.m_failureCount);
    }
}

int AMBE::webapiActionsPost(
    const QStringList& featureActionsKeys,
    SWGSDRangel::SWGFeatureActions& query,
    QString& errorMessage)
{
    SWGSDRangel::SWGAMBEActions *swgAMBEActions = query.getAmbeActions();

    if (swgAMBEActions)
    {
        bool unknownAction = true;

        if (featureActionsKeys.contains("removeAll") && (swgAMBEActions->getRemoveAll() != 0))
        {
            unknownAction = false;
            getAMBEEngine()->releaseAll();

            if (getMessageQueueToGUI())
            {
                MsgReportDevices *msg = MsgReportDevices::create();
                getAMBEEngine()->scan(msg->getAvailableDevices());
                getAMBEEngine()->getDeviceRefs(msg->getUsedDevices());
                getMessageQueueToGUI()->push(msg);
            }
        }

        if (featureActionsKeys.contains("updateDevices"))
        {
            unknownAction = false;
            bool updated = false;
            SWGSDRangel::SWGAMBEDevices *swgAMBEDevices = swgAMBEActions->getUpdateDevices();
            QList<SWGSDRangel::SWGAMBEDevice *> *ambeList = swgAMBEDevices->getAmbeDevices();

            for (QList<SWGSDRangel::SWGAMBEDevice *>::const_iterator it = ambeList->begin(); it != ambeList->end(); ++it)
            {
                updated = true;

                if ((*it)->getDelete() != 0) {
                    getAMBEEngine()->releaseController((*it)->getDeviceRef()->toStdString());
                } else {
                    getAMBEEngine()->registerController((*it)->getDeviceRef()->toStdString());
                }
            }

            if (updated && getMessageQueueToGUI())
            {
                MsgReportDevices *msg = MsgReportDevices::create();
                getAMBEEngine()->scan(msg->getAvailableDevices());
                getAMBEEngine()->getDeviceRefs(msg->getUsedDevices());
                getMessageQueueToGUI()->push(msg);
            }
        }

        if (unknownAction)
        {
            errorMessage = "Unknown action";
            return 400;
        }
        else
        {
            return 202;
        }
    }
    else
    {
        errorMessage = "Missing AMBEActions in query";
        return 400;
    }
}
