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
#include <QThread>

#include "SWGFeatureSettings.h"
#include "SWGFeatureReport.h"
#include "SWGFeatureActions.h"
#include "SWGSimplePTTReport.h"
#include "SWGDeviceState.h"

#include "dsp/dspengine.h"
#include "settings/serializable.h"

#include "simplepttworker.h"
#include "simpleptt.h"

MESSAGE_CLASS_DEFINITION(SimplePTT::MsgConfigureSimplePTT, Message)
MESSAGE_CLASS_DEFINITION(SimplePTT::MsgPTT, Message)
MESSAGE_CLASS_DEFINITION(SimplePTT::MsgStartStop, Message)

const char* const SimplePTT::m_featureIdURI = "sdrangel.feature.simpleptt";
const char* const SimplePTT::m_featureId = "SimplePTT";

SimplePTT::SimplePTT(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface),
    m_thread(nullptr),
    m_running(false),
    m_worker(nullptr),
    m_ptt(false)
{
    setObjectName(m_featureId);
    m_errorMessage = "SimplePTT error";
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &SimplePTT::networkManagerFinished
    );
}

SimplePTT::~SimplePTT()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &SimplePTT::networkManagerFinished
    );
    delete m_networkManager;
    stop();
}

void SimplePTT::start()
{
    QMutexLocker m_lock(&m_mutex);

    if (m_running) {
        return;
    }

	qDebug("SimplePTT::start");
    m_thread = new QThread();
    m_worker = new SimplePTTWorker(getWebAPIAdapterInterface());
    m_worker->moveToThread(m_thread);

    QObject::connect(
        m_thread,
        &QThread::started,
        m_worker,
        &SimplePTTWorker::startWork
    );
    QObject::connect(
        m_thread,
        &QThread::finished,
        m_worker,
        &QObject::deleteLater
    );
    QObject::connect(
        m_thread,
        &QThread::finished,
        m_thread,
        &QThread::deleteLater
    );

    m_worker->setMessageQueueToGUI(getMessageQueueToGUI());
    m_worker->startWork();
    m_state = StRunning;
    m_thread->start();

    SimplePTTWorker::MsgConfigureSimplePTTWorker *msg = SimplePTTWorker::MsgConfigureSimplePTTWorker::create(m_settings, QList<QString>(), true);
    m_worker->getInputMessageQueue()->push(msg);

    m_running = true;
}

void SimplePTT::stop()
{
    QMutexLocker m_lock(&m_mutex);

    if (!m_running) {
        return;
    }

    qDebug("SimplePTT::stop");
    m_running = false;
	m_worker->stopWork();
    m_state = StIdle;
	m_thread->quit();
	m_thread->wait();
}

bool SimplePTT::handleMessage(const Message& cmd)
{
	if (MsgConfigureSimplePTT::match(cmd))
	{
        MsgConfigureSimplePTT& cfg = (MsgConfigureSimplePTT&) cmd;
        qDebug() << "SimplePTT::handleMessage: MsgConfigureSimplePTT";
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

		return true;
	}
    else if (MsgPTT::match(cmd))
    {
        MsgPTT& cfg = (MsgPTT&) cmd;
        m_ptt = cfg.getTx();
        qDebug() << "SimplePTT::handleMessage: MsgPTT: tx:" << m_ptt;

        if (m_running)
        {
            SimplePTTWorker::MsgPTT *msg = SimplePTTWorker::MsgPTT::create(m_ptt);
            m_worker->getInputMessageQueue()->push(msg);
        }

        return true;
    }
    else if (MsgStartStop::match(cmd))
    {
        MsgStartStop& cfg = (MsgStartStop&) cmd;
        qDebug() << "SimplePTT::handleMessage: MsgStartStop: start:" << cfg.getStartStop();

        if (cfg.getStartStop()) {
            start();
        } else {
            stop();
        }

        return true;
    }
	else
	{
		return false;
	}
}

QByteArray SimplePTT::serialize() const
{
    return m_settings.serialize();
}

bool SimplePTT::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureSimplePTT *msg = MsgConfigureSimplePTT::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureSimplePTT *msg = MsgConfigureSimplePTT::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void SimplePTT::getAudioPeak(float& peak)
{
    if (m_running) {
        m_worker->getAudioPeak(peak);
    }
}

void SimplePTT::applySettings(const SimplePTTSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "SimplePTT::applySettings:" << settings.getDebugString(settingsKeys, force) << " force: " << force;

    if (m_running)
    {
        SimplePTTWorker::MsgConfigureSimplePTTWorker *msg = SimplePTTWorker::MsgConfigureSimplePTTWorker::create(
            settings, settingsKeys, force
        );
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

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

int SimplePTT::webapiRun(bool run,
    SWGSDRangel::SWGDeviceState& response,
    QString& errorMessage)
{
    (void) errorMessage;
    getFeatureStateStr(*response.getState());
    MsgStartStop *msg = MsgStartStop::create(run);
    getInputMessageQueue()->push(msg);
    return 202;
}

int SimplePTT::webapiSettingsGet(
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setSimplePttSettings(new SWGSDRangel::SWGSimplePTTSettings());
    response.getSimplePttSettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

int SimplePTT::webapiSettingsPutPatch(
    bool force,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    SimplePTTSettings settings = m_settings;
    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    MsgConfigureSimplePTT *msg = MsgConfigureSimplePTT::create(settings, featureSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    qDebug("SimplePTT::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureSimplePTT *msgToGUI = MsgConfigureSimplePTT::create(settings, featureSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);

    return 200;
}

int SimplePTT::webapiReportGet(
    SWGSDRangel::SWGFeatureReport& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setSimplePttReport(new SWGSDRangel::SWGSimplePTTReport());
    response.getSimplePttReport()->init();
    webapiFormatFeatureReport(response);
    return 200;
}

int SimplePTT::webapiActionsPost(
    const QStringList& featureActionsKeys,
    SWGSDRangel::SWGFeatureActions& query,
    QString& errorMessage)
{
    SWGSDRangel::SWGSimplePTTActions *swgSimplePTTActions = query.getSimplePttActions();

    if (swgSimplePTTActions)
    {
        bool unknownAction = true;

        if (featureActionsKeys.contains("run"))
        {
            bool featureRun = swgSimplePTTActions->getRun() != 0;
            unknownAction = false;
            MsgStartStop *msg = MsgStartStop::create(featureRun);
            getInputMessageQueue()->push(msg);
        }

        if (featureActionsKeys.contains("ptt"))
        {
            bool ptt = swgSimplePTTActions->getPtt() != 0;
            unknownAction = false;
            MsgPTT *msg = MsgPTT::create(ptt);

            getInputMessageQueue()->push(msg);

            if (getMessageQueueToGUI())
            {
                MsgPTT *msgToGUI = MsgPTT::create(ptt);
                getMessageQueueToGUI()->push(msgToGUI);
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
        errorMessage = "Missing SimplePTTActions in query";
        return 400;
    }
}

void SimplePTT::webapiFormatFeatureSettings(
    SWGSDRangel::SWGFeatureSettings& response,
    const SimplePTTSettings& settings)
{
    if (response.getSimplePttSettings()->getTitle()) {
        *response.getSimplePttSettings()->getTitle() = settings.m_title;
    } else {
        response.getSimplePttSettings()->setTitle(new QString(settings.m_title));
    }

    response.getSimplePttSettings()->setRgbColor(settings.m_rgbColor);
    response.getSimplePttSettings()->setRxDeviceSetIndex(settings.m_rxDeviceSetIndex);
    response.getSimplePttSettings()->setTxDeviceSetIndex(settings.m_txDeviceSetIndex);
    response.getSimplePttSettings()->setRx2TxDelayMs(settings.m_rx2TxDelayMs);
    response.getSimplePttSettings()->setTx2RxDelayMs(settings.m_tx2RxDelayMs);
    response.getSimplePttSettings()->setVox(settings.m_vox ? 1 : 0);
    response.getSimplePttSettings()->setVoxEnable(settings.m_voxEnable ? 1 : 0);
    response.getSimplePttSettings()->setVoxHold(settings.m_voxHold);
    response.getSimplePttSettings()->setVoxLevel(settings.m_voxLevel);
    response.getSimplePttSettings()->setGpioControl((int) settings.m_gpioControl);
    response.getSimplePttSettings()->setRx2txGpioEnable(settings.m_rx2txGPIOEnable ? 1 : 0);
    response.getSimplePttSettings()->setRx2txGpioMask(settings.m_rx2txGPIOMask);
    response.getSimplePttSettings()->setRx2txGpioValues(settings.m_rx2txGPIOValues);
    response.getSimplePttSettings()->setRx2txCommandEnable(settings.m_rx2txCommandEnable ? 1 : 0);

    if (response.getSimplePttSettings()->getRx2txCommand()) {
        *response.getSimplePttSettings()->getRx2txCommand() = settings.m_rx2txCommand;
    } else {
        response.getSimplePttSettings()->setRx2txCommand(new QString(settings.m_rx2txCommand));
    }

    response.getSimplePttSettings()->setTx2rxGpioEnable(settings.m_tx2rxGPIOEnable ? 1 : 0);
    response.getSimplePttSettings()->setTx2rxGpioMask(settings.m_tx2rxGPIOMask);
    response.getSimplePttSettings()->setTx2rxGpioValues(settings.m_tx2rxGPIOValues);
    response.getSimplePttSettings()->setTx2rxCommandEnable(settings.m_tx2rxCommandEnable ? 1 : 0);

    if (response.getSimplePttSettings()->getTx2rxCommand()) {
        *response.getSimplePttSettings()->getTx2rxCommand() = settings.m_tx2rxCommand;
    } else {
        response.getSimplePttSettings()->setTx2rxCommand(new QString(settings.m_tx2rxCommand));
    }

    response.getSimplePttSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getSimplePttSettings()->getReverseApiAddress()) {
        *response.getSimplePttSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getSimplePttSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getSimplePttSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getSimplePttSettings()->setReverseApiFeatureSetIndex(settings.m_reverseAPIFeatureSetIndex);
    response.getSimplePttSettings()->setReverseApiFeatureIndex(settings.m_reverseAPIFeatureIndex);

    if (settings.m_rollupState)
    {
        if (response.getSimplePttSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getSimplePttSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getSimplePttSettings()->setRollupState(swgRollupState);
        }
    }
}

void SimplePTT::webapiUpdateFeatureSettings(
    SimplePTTSettings& settings,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    if (featureSettingsKeys.contains("title")) {
        settings.m_title = *response.getSimplePttSettings()->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getSimplePttSettings()->getRgbColor();
    }
    if (featureSettingsKeys.contains("rxDeviceSetIndex")) {
        settings.m_rxDeviceSetIndex = response.getSimplePttSettings()->getRxDeviceSetIndex();
    }
    if (featureSettingsKeys.contains("txDeviceSetIndex")) {
        settings.m_txDeviceSetIndex = response.getSimplePttSettings()->getTxDeviceSetIndex();
    }
    if (featureSettingsKeys.contains("rx2TxDelayMs")) {
        settings.m_rx2TxDelayMs = response.getSimplePttSettings()->getRx2TxDelayMs();
    }
    if (featureSettingsKeys.contains("tx2RxDelayMs")) {
        settings.m_tx2RxDelayMs = response.getSimplePttSettings()->getTx2RxDelayMs();
    }
    if (featureSettingsKeys.contains("vox")) {
        settings.m_vox = response.getSimplePttSettings()->getVox() != 0;
    }
    if (featureSettingsKeys.contains("voxEnable")) {
        settings.m_voxEnable = response.getSimplePttSettings()->getVoxEnable() != 0;
    }
    if (featureSettingsKeys.contains("voxHold")) {
        settings.m_voxHold = response.getSimplePttSettings()->getVoxHold();
    }
    if (featureSettingsKeys.contains("voxLevel")) {
        settings.m_voxLevel = response.getSimplePttSettings()->getVoxLevel();
    }
    if (featureSettingsKeys.contains("gpioControl")) {
        settings.m_gpioControl = (SimplePTTSettings::GPIOControl) response.getSimplePttSettings()->getGpioControl();
    }
    if (featureSettingsKeys.contains("rx2txGPIOEnable")) {
        settings.m_rx2txGPIOEnable = response.getSimplePttSettings()->getRx2txGpioEnable() != 0;
    }
    if (featureSettingsKeys.contains("rx2txGPIOMask")) {
        settings.m_rx2txGPIOMask = response.getSimplePttSettings()->getRx2txGpioMask();
    }
    if (featureSettingsKeys.contains("rx2txGPIOValues")) {
        settings.m_rx2txGPIOValues = response.getSimplePttSettings()->getRx2txGpioValues();
    }
    if (featureSettingsKeys.contains("rx2txGPIOEnable")) {
        settings.m_rx2txCommandEnable = response.getSimplePttSettings()->getRx2txCommandEnable() != 0;
    }
    if (featureSettingsKeys.contains("rx2txCommand")) {
        settings.m_rx2txCommand = *response.getSimplePttSettings()->getRx2txCommand();
    }
    if (featureSettingsKeys.contains("tx2rxGPIOEnable")) {
        settings.m_tx2rxGPIOEnable = response.getSimplePttSettings()->getTx2rxGpioEnable() != 0;
    }
    if (featureSettingsKeys.contains("tx2rxGPIOMask")) {
        settings.m_tx2rxGPIOMask = response.getSimplePttSettings()->getTx2rxGpioMask();
    }
    if (featureSettingsKeys.contains("tx2rxGPIOValues")) {
        settings.m_tx2rxGPIOValues = response.getSimplePttSettings()->getTx2rxGpioValues();
    }
    if (featureSettingsKeys.contains("tx2rxGPIOEnable")) {
        settings.m_tx2rxCommandEnable = response.getSimplePttSettings()->getTx2rxCommandEnable() != 0;
    }
    if (featureSettingsKeys.contains("tx2rxCommand")) {
        settings.m_tx2rxCommand = *response.getSimplePttSettings()->getTx2rxCommand();
    }
    if (featureSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getSimplePttSettings()->getUseReverseApi() != 0;
    }
    if (featureSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getSimplePttSettings()->getReverseApiAddress();
    }
    if (featureSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getSimplePttSettings()->getReverseApiPort();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureSetIndex")) {
        settings.m_reverseAPIFeatureSetIndex = response.getSimplePttSettings()->getReverseApiFeatureSetIndex();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureIndex")) {
        settings.m_reverseAPIFeatureIndex = response.getSimplePttSettings()->getReverseApiFeatureIndex();
    }
    if (settings.m_rollupState && featureSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(featureSettingsKeys, response.getSimplePttSettings()->getRollupState());
    }
}

void SimplePTT::webapiFormatFeatureReport(SWGSDRangel::SWGFeatureReport& response)
{
    response.getSimplePttReport()->setPtt(m_ptt ? 1 : 0);
    response.getSimplePttReport()->setRunningState(getState());
}

void SimplePTT::webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const SimplePTTSettings& settings, bool force)
{
    SWGSDRangel::SWGFeatureSettings *swgFeatureSettings = new SWGSDRangel::SWGFeatureSettings();
    // swgFeatureSettings->setOriginatorFeatureIndex(getIndexInDeviceSet());
    // swgFeatureSettings->setOriginatorFeatureSetIndex(getDeviceSetIndex());
    swgFeatureSettings->setFeatureType(new QString("SimplePTT"));
    swgFeatureSettings->setSimplePttSettings(new SWGSDRangel::SWGSimplePTTSettings());
    SWGSDRangel::SWGSimplePTTSettings *swgSimplePTTSettings = swgFeatureSettings->getSimplePttSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("title") || force) {
        swgSimplePTTSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgSimplePTTSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("rxDeviceSetIndex") || force) {
        swgSimplePTTSettings->setRxDeviceSetIndex(settings.m_rxDeviceSetIndex);
    }
    if (channelSettingsKeys.contains("txDeviceSetIndex") || force) {
        swgSimplePTTSettings->setTxDeviceSetIndex(settings.m_txDeviceSetIndex);
    }
    if (channelSettingsKeys.contains("rx2TxDelayMs") || force) {
        swgSimplePTTSettings->setRx2TxDelayMs(settings.m_rx2TxDelayMs);
    }
    if (channelSettingsKeys.contains("tx2RxDelayMs") || force) {
        swgSimplePTTSettings->setTx2RxDelayMs(settings.m_tx2RxDelayMs);
    }
    if (channelSettingsKeys.contains("vox") || force) {
        swgSimplePTTSettings->setVox(settings.m_vox ? 1 : 0);
    }
    if (channelSettingsKeys.contains("voxEnable") || force) {
        swgSimplePTTSettings->setVoxEnable(settings.m_voxEnable ? 1 : 0);
    }
    if (channelSettingsKeys.contains("voxHold") || force) {
        swgSimplePTTSettings->setVoxHold(settings.m_voxHold);
    }
    if (channelSettingsKeys.contains("voxLevel") || force) {
        swgSimplePTTSettings->setVoxLevel(settings.m_voxLevel);
    }
    if (channelSettingsKeys.contains("gpioControl") || force) {
        swgSimplePTTSettings->setGpioControl((int) settings.m_gpioControl);
    }
    if (channelSettingsKeys.contains("rx2txGPIOEnable") || force) {
        swgSimplePTTSettings->setRx2txGpioEnable(settings.m_rx2txGPIOEnable ? 1 : 0);
    }
    if (channelSettingsKeys.contains("rx2txGPIOMask") || force) {
        swgSimplePTTSettings->setRx2txGpioMask(settings.m_rx2txGPIOMask);
    }
    if (channelSettingsKeys.contains("rx2txGPIOValues") || force) {
        swgSimplePTTSettings->setRx2txGpioValues(settings.m_rx2txGPIOValues);
    }
    if (channelSettingsKeys.contains("rx2txCommandEnable") || force) {
        swgSimplePTTSettings->setRx2txCommandEnable(settings.m_rx2txCommandEnable ? 1 : 0);
    }
    if (channelSettingsKeys.contains("rx2txCommand") || force) {
        swgSimplePTTSettings->setRx2txCommand(new QString(settings.m_rx2txCommand));
    }
    if (channelSettingsKeys.contains("tx2rxGPIOEnable") || force) {
        swgSimplePTTSettings->setTx2rxGpioEnable(settings.m_tx2rxGPIOEnable ? 1 : 0);
    }
    if (channelSettingsKeys.contains("t2rxGPIOMask") || force) {
        swgSimplePTTSettings->setTx2rxGpioMask(settings.m_tx2rxGPIOMask);
    }
    if (channelSettingsKeys.contains("tx2rxGPIOValues") || force) {
        swgSimplePTTSettings->setTx2rxGpioValues(settings.m_tx2rxGPIOValues);
    }
    if (channelSettingsKeys.contains("tx2rxCommandEnable") || force) {
        swgSimplePTTSettings->setTx2rxCommandEnable(settings.m_tx2rxCommandEnable ? 1 : 0);
    }
    if (channelSettingsKeys.contains("tx2rxCommand") || force) {
        swgSimplePTTSettings->setTx2rxCommand(new QString(settings.m_tx2rxCommand));
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

void SimplePTT::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "SimplePTT::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("SimplePTT::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
