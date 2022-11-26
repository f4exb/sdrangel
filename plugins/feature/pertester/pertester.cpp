///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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
#include <QTimer>

#include "SWGFeatureSettings.h"
#include "SWGFeatureReport.h"
#include "SWGFeatureActions.h"
#include "SWGDeviceState.h"
#include "SWGPERTesterActions.h"

#include "dsp/dspengine.h"
#include "device/deviceset.h"
#include "channel/channelapi.h"
#include "feature/featureset.h"
#include "settings/serializable.h"
#include "maincore.h"

#include "pertester.h"
#include "pertesterworker.h"
#include "pertesterreport.h"

MESSAGE_CLASS_DEFINITION(PERTester::MsgConfigurePERTester, Message)
MESSAGE_CLASS_DEFINITION(PERTester::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(PERTester::MsgResetStats, Message)
MESSAGE_CLASS_DEFINITION(PERTester::MsgReportWorker, Message)

const char* const PERTester::m_featureIdURI = "sdrangel.feature.pertester";
const char* const PERTester::m_featureId = "PERTester";

PERTester::PERTester(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface),
    m_thread(nullptr),
    m_worker(nullptr)
{
    qDebug("PERTester::PERTester: webAPIAdapterInterface: %p", webAPIAdapterInterface);
    setObjectName(m_featureId);
    m_state = StIdle;
    m_errorMessage = "PERTester error";
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &PERTester::networkManagerFinished
    );
}

PERTester::~PERTester()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &PERTester::networkManagerFinished
    );
    delete m_networkManager;
    stop();
}

void PERTester::start()
{
    qDebug("PERTester::start");

    m_thread = new QThread();
    m_worker = new PERTesterWorker();
    m_worker->moveToThread(m_thread);

    QObject::connect(m_thread, &QThread::started, m_worker, &PERTesterWorker::startWork);
    QObject::connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    QObject::connect(m_thread, &QThread::finished, m_thread, &QThread::deleteLater);

    m_worker->setMessageQueueToFeature(getInputMessageQueue());
    m_worker->setMessageQueueToGUI(getMessageQueueToGUI());
    m_worker->getInputMessageQueue()->push(PERTesterWorker::MsgConfigurePERTesterWorker::create(m_settings, QList<QString>(), true));
    if (m_settings.m_start == PERTesterSettings::START_IMMEDIATELY)
    {
        m_thread->start();
        m_state = StRunning;
    }
    else
    {
        // Wait for AOS
        m_state = StIdle;
    }
    m_thread->start();
}

void PERTester::stop()
{
    qDebug("PERTester::stop");
    m_state = StIdle;
    if (m_thread)
    {
        m_thread->quit();
        m_thread->wait();
        m_thread = nullptr;
        m_worker = nullptr;
    }
}

bool PERTester::handleMessage(const Message& cmd)
{
    if (MsgConfigurePERTester::match(cmd))
    {
        MsgConfigurePERTester& cfg = (MsgConfigurePERTester&) cmd;
        qDebug() << "PERTester::handleMessage: MsgConfigurePERTester";
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

        return true;
    }
    else if (MsgStartStop::match(cmd))
    {
        MsgStartStop& cfg = (MsgStartStop&) cmd;
        qDebug() << "PERTester::handleMessage: MsgStartStop: start:" << cfg.getStartStop();

        if (cfg.getStartStop()) {
            start();
        } else {
            stop();
        }

        return true;
    }
    else if (MsgResetStats::match(cmd))
    {
        if (m_worker) {
            m_worker->getInputMessageQueue()->push(MsgResetStats::create());
        }
        return true;
    }
    else if (MsgReportWorker::match(cmd))
    {
        MsgReportWorker& report = (MsgReportWorker&) cmd;
        if (report.getMessage() == "Complete")
        {
            stop();
        }
        else
        {
            m_state = StError;
            m_errorMessage = report.getMessage();
        }
        return true;
    }
    else
    {
        return false;
    }
}

QByteArray PERTester::serialize() const
{
    return m_settings.serialize();
}

bool PERTester::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigurePERTester *msg = MsgConfigurePERTester::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigurePERTester *msg = MsgConfigurePERTester::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void PERTester::applySettings(const PERTesterSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "PERTester::applySettings:" << settings.getDebugString(settingsKeys, force) << " force: " << force;

    PERTesterWorker::MsgConfigurePERTesterWorker *msg = PERTesterWorker::MsgConfigurePERTesterWorker::create(
        settings, settingsKeys, force
    );
    if (m_worker) {
        m_worker->getInputMessageQueue()->push(msg);
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

    m_settings = settings;
}

int PERTester::webapiRun(bool run,
    SWGSDRangel::SWGDeviceState& response,
    QString& errorMessage)
{
    (void) errorMessage;
    getFeatureStateStr(*response.getState());
    MsgStartStop *msg = MsgStartStop::create(run);
    getInputMessageQueue()->push(msg);
    return 202;
}

int PERTester::webapiSettingsGet(
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setPerTesterSettings(new SWGSDRangel::SWGPERTesterSettings());
    response.getPerTesterSettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

int PERTester::webapiSettingsPutPatch(
    bool force,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    PERTesterSettings settings = m_settings;
    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    MsgConfigurePERTester *msg = MsgConfigurePERTester::create(settings, featureSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigurePERTester *msgToGUI = MsgConfigurePERTester::create(settings, featureSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);

    return 200;
}

static QList<QString *> *convertStringListToPtrs(QStringList listIn)
{
    QList<QString *> *listOut = new QList<QString *>();

    for (int i = 0; i < listIn.size(); i++)
        listOut->append(new QString(listIn[i]));

    return listOut;
}

static QStringList convertPtrsToStringList(QList<QString *> *listIn)
{
    QStringList listOut;

    for (int i = 0; i < listIn->size(); i++)
        listOut.append(*listIn->at(i));

    return listOut;
}

void PERTester::webapiFormatFeatureSettings(
    SWGSDRangel::SWGFeatureSettings& response,
    const PERTesterSettings& settings)
{
    response.getPerTesterSettings()->setPacketCount(settings.m_packetCount);
    response.getPerTesterSettings()->setInterval(settings.m_interval);
    response.getPerTesterSettings()->setStart((int)settings.m_start);
    response.getPerTesterSettings()->setSatellites(convertStringListToPtrs(settings.m_satellites));
    response.getPerTesterSettings()->setPacket(new QString(settings.m_packet));
    response.getPerTesterSettings()->setIgnoreLeadingBytes(settings.m_ignoreLeadingBytes);
    response.getPerTesterSettings()->setIgnoreTrailingBytes(settings.m_ignoreTrailingBytes);
    response.getPerTesterSettings()->setTxUdpAddress(new QString(settings.m_txUDPAddress));
    response.getPerTesterSettings()->setTxUdpPort(settings.m_txUDPPort);
    response.getPerTesterSettings()->setRxUdpAddress(new QString(settings.m_rxUDPAddress));
    response.getPerTesterSettings()->setRxUdpPort(settings.m_rxUDPPort);

    if (response.getPerTesterSettings()->getTitle()) {
        *response.getPerTesterSettings()->getTitle() = settings.m_title;
    } else {
        response.getPerTesterSettings()->setTitle(new QString(settings.m_title));
    }

    response.getPerTesterSettings()->setRgbColor(settings.m_rgbColor);
    response.getPerTesterSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getPerTesterSettings()->getReverseApiAddress()) {
        *response.getPerTesterSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getPerTesterSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getPerTesterSettings()->setReverseApiPort(settings.m_reverseAPIPort);

    if (settings.m_rollupState)
    {
        if (response.getPerTesterSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getPerTesterSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getPerTesterSettings()->setRollupState(swgRollupState);
        }
    }
}

void PERTester::webapiUpdateFeatureSettings(
    PERTesterSettings& settings,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    if (featureSettingsKeys.contains("packetCount")) {
        settings.m_packetCount = response.getPerTesterSettings()->getPacketCount();
    }
    if (featureSettingsKeys.contains("interval")) {
        settings.m_interval = response.getPerTesterSettings()->getInterval();
    }
    if (featureSettingsKeys.contains("start")) {
        settings.m_start = (PERTesterSettings::Start)response.getPerTesterSettings()->getStart();
    }
    if (featureSettingsKeys.contains("satellites")) {
        settings.m_satellites = convertPtrsToStringList(response.getPerTesterSettings()->getSatellites());
    }
    if (featureSettingsKeys.contains("packet")) {
        settings.m_packet = *response.getPerTesterSettings()->getPacket();
    }
    if (featureSettingsKeys.contains("ignoreLeadingBytes")) {
        settings.m_ignoreLeadingBytes = response.getPerTesterSettings()->getIgnoreLeadingBytes();
    }
    if (featureSettingsKeys.contains("ignoreTrailingBytes")) {
        settings.m_ignoreTrailingBytes = response.getPerTesterSettings()->getIgnoreTrailingBytes();
    }
    if (featureSettingsKeys.contains("txUDPAddress")) {
        settings.m_txUDPAddress = *response.getPerTesterSettings()->getTxUdpAddress();
    }
    if (featureSettingsKeys.contains("txUDPPort")) {
        settings.m_txUDPPort = response.getPerTesterSettings()->getTxUdpPort();
    }
    if (featureSettingsKeys.contains("rxUDPAddress")) {
        settings.m_txUDPAddress = *response.getPerTesterSettings()->getRxUdpAddress();
    }
    if (featureSettingsKeys.contains("rxUDPPort")) {
        settings.m_rxUDPPort = response.getPerTesterSettings()->getRxUdpPort();
    }
    if (featureSettingsKeys.contains("title")) {
        settings.m_title = *response.getPerTesterSettings()->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getPerTesterSettings()->getRgbColor();
    }
    if (featureSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getPerTesterSettings()->getUseReverseApi() != 0;
    }
    if (featureSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getPerTesterSettings()->getReverseApiAddress();
    }
    if (featureSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getPerTesterSettings()->getReverseApiPort();
    }
    if (settings.m_rollupState && featureSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(featureSettingsKeys, response.getPerTesterSettings()->getRollupState());
    }
}

void PERTester::webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const PERTesterSettings& settings, bool force)
{
    SWGSDRangel::SWGFeatureSettings *swgFeatureSettings = new SWGSDRangel::SWGFeatureSettings();
    // swgFeatureSettings->setOriginatorFeatureIndex(getIndexInDeviceSet());
    // swgFeatureSettings->setOriginatorFeatureSetIndex(getDeviceSetIndex());
    swgFeatureSettings->setFeatureType(new QString("PERTester"));
    swgFeatureSettings->setPerTesterSettings(new SWGSDRangel::SWGPERTesterSettings());
    SWGSDRangel::SWGPERTesterSettings *swgPERTesterSettings = swgFeatureSettings->getPerTesterSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (featureSettingsKeys.contains("packetCount") || force) {
        swgPERTesterSettings->setPacketCount(settings.m_packetCount);
    }
    if (featureSettingsKeys.contains("interval") || force) {
        swgPERTesterSettings->setInterval(settings.m_interval);
    }
    if (featureSettingsKeys.contains("start") || force) {
        swgPERTesterSettings->setStart((int)settings.m_start);
    }
    if (featureSettingsKeys.contains("satellites") || force) {
        swgPERTesterSettings->setSatellites(convertStringListToPtrs(settings.m_satellites));
    }
    if (featureSettingsKeys.contains("packet") || force) {
        swgPERTesterSettings->setPacket(new QString(settings.m_packet));
    }
    if (featureSettingsKeys.contains("ignoreLeadingBytes") || force) {
        swgPERTesterSettings->setIgnoreLeadingBytes(settings.m_ignoreLeadingBytes);
    }
    if (featureSettingsKeys.contains("ignoreTrailingBytes") || force) {
        swgPERTesterSettings->setIgnoreTrailingBytes(settings.m_ignoreTrailingBytes);
    }
    if (featureSettingsKeys.contains("txUDPAddress") || force) {
        swgPERTesterSettings->setTxUdpAddress(new QString(settings.m_txUDPAddress));
    }
    if (featureSettingsKeys.contains("txUDPPort") || force) {
        swgPERTesterSettings->setTxUdpPort(settings.m_txUDPPort);
    }
    if (featureSettingsKeys.contains("rxUDPAddress") || force) {
        swgPERTesterSettings->setRxUdpAddress(new QString(settings.m_rxUDPAddress));
    }
    if (featureSettingsKeys.contains("rxUDPPort") || force) {
        swgPERTesterSettings->setRxUdpPort(settings.m_rxUDPPort);
    }

    if (featureSettingsKeys.contains("title") || force) {
        swgPERTesterSettings->setTitle(new QString(settings.m_title));
    }
    if (featureSettingsKeys.contains("rgbColor") || force) {
        swgPERTesterSettings->setRgbColor(settings.m_rgbColor);
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

int PERTester::webapiReportGet(
    SWGSDRangel::SWGFeatureReport& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setPerTesterReport(new SWGSDRangel::SWGPERTesterReport());
    response.getPerTesterReport()->init();
    webapiFormatFeatureReport(response);
    return 200;
}

void PERTester::webapiFormatFeatureReport(SWGSDRangel::SWGFeatureReport& response)
{
    response.getPerTesterReport()->setRunningState(getState());
}

int PERTester::webapiActionsPost(
        const QStringList& featureActionsKeys,
        SWGSDRangel::SWGFeatureActions& query,
        QString& errorMessage)
{
    SWGSDRangel::SWGPERTesterActions *swgPERTesterActions = query.getPerTesterActions();

    if (swgPERTesterActions)
    {
        bool unknownAction = true;

        if (featureActionsKeys.contains("run"))
        {
            bool featureRun = swgPERTesterActions->getRun() != 0;
            unknownAction = false;
            MsgStartStop *msg = MsgStartStop::create(featureRun);
            getInputMessageQueue()->push(msg);
        }

        if (featureActionsKeys.contains("aos"))
        {
            SWGSDRangel::SWGPERTesterActions_aos* aos = swgPERTesterActions->getAos();
            unknownAction = false;
            QString *satelliteName = aos->getSatelliteName();

            if (satelliteName != nullptr)
            {
                if (m_settings.m_satellites.contains(*satelliteName))
                {
                    if (m_settings.m_start == PERTesterSettings::START_ON_AOS)
                    {
                        m_thread->start();
                        m_state = StRunning;
                    }
                    else if (m_settings.m_start == PERTesterSettings::START_ON_MID_PASS)
                    {
                        QString aosTimeString = *aos->getAosTime();
                        QString losTimeString = *aos->getLosTime();
                        QDateTime aosTime = QDateTime::fromString(aosTimeString);
                        QDateTime losTime = QDateTime::fromString(losTimeString);
                        qint64 msecs = aosTime.msecsTo(losTime) / 2;
                        QTimer::singleShot(msecs, [this] {
                            m_thread->start();
                            m_state = StRunning;
                        });
                    }
                }
            }
            else
            {
                errorMessage = "Missing satellite name";
                return 400;
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
        errorMessage = "Missing PERTesterActions in query";
        return 400;
    }
}

void PERTester::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "PERTester::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("PERTester::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
