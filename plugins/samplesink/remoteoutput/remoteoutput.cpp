///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include <string.h>
#include <errno.h>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonParseError>
#include <QBuffer>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGRemoteOutputReport.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"

#include "device/deviceapi.h"

#include "remoteoutput.h"
#include "remoteoutputworker.h"

MESSAGE_CLASS_DEFINITION(RemoteOutput::MsgConfigureRemoteOutput, Message)
MESSAGE_CLASS_DEFINITION(RemoteOutput::MsgConfigureRemoteOutputWork, Message)
MESSAGE_CLASS_DEFINITION(RemoteOutput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(RemoteOutput::MsgConfigureRemoteOutputChunkCorrection, Message)

const uint32_t RemoteOutput::NbSamplesForRateCorrection = 5000000;

RemoteOutput::RemoteOutput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_centerFrequency(0),
    m_remoteOutputWorker(nullptr),
	m_deviceDescription("RemoteOutput"),
    m_startingTimeStamp(0),
	m_masterTimer(deviceAPI->getMasterTimer()),
	m_tickCount(0),
    m_tickMultiplier(20),
	m_lastRemoteSampleCount(0),
	m_lastSampleCount(0),
	m_lastRemoteTimestampRateCorrection(0),
	m_lastTimestampRateCorrection(0),
	m_lastQueueLength(-2),
	m_nbRemoteSamplesSinceRateCorrection(0),
	m_nbSamplesSinceRateCorrection(0),
	m_chunkSizeCorrection(0)
{
    m_deviceAPI->setNbSinkStreams(1);
    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    connect(&m_masterTimer, SIGNAL(timeout()), this, SLOT(tick()));
}

RemoteOutput::~RemoteOutput()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
	stop();
	delete m_networkManager;
}

void RemoteOutput::destroy()
{
    delete this;
}

bool RemoteOutput::start()
{
	QMutexLocker mutexLocker(&m_mutex);
	qDebug() << "RemoteOutput::start";

	m_remoteOutputWorker = new RemoteOutputWorker(&m_sampleSourceFifo);
    m_remoteOutputWorker->moveToThread(&m_remoteOutputWorkerThread);
	m_remoteOutputWorker->setDataAddress(m_settings.m_dataAddress, m_settings.m_dataPort);
	m_remoteOutputWorker->setSamplerate(m_settings.m_sampleRate);
	m_remoteOutputWorker->setNbBlocksFEC(m_settings.m_nbFECBlocks);
	m_remoteOutputWorker->connectTimer(m_masterTimer);
	startWorker();

	// restart auto rate correction
	m_lastRemoteTimestampRateCorrection = 0;
	m_lastTimestampRateCorrection = 0;
	m_lastQueueLength = -2; // set first value out of bounds
	m_chunkSizeCorrection = 0;

    m_remoteOutputWorker->setTxDelay(m_settings.m_txDelay);

	mutexLocker.unlock();
	//applySettings(m_generalSettings, m_settings, true);
	qDebug("RemoteOutput::start: started");

	return true;
}

void RemoteOutput::init()
{
    applySettings(m_settings, true);
}

void RemoteOutput::stop()
{
	qDebug() << "RemoteOutput::stop";
	QMutexLocker mutexLocker(&m_mutex);

	if (m_remoteOutputWorker)
	{
	    stopWorker();
		delete m_remoteOutputWorker;
		m_remoteOutputWorker = nullptr;
	}
}

void RemoteOutput::startWorker()
{
    m_remoteOutputWorker->startWork();
    m_remoteOutputWorkerThread.start();
}

void RemoteOutput::stopWorker()
{
	m_remoteOutputWorker->stopWork();
	m_remoteOutputWorkerThread.quit();
	m_remoteOutputWorkerThread.wait();
}

QByteArray RemoteOutput::serialize() const
{
    return m_settings.serialize();
}

bool RemoteOutput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureRemoteOutput* message = MsgConfigureRemoteOutput::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureRemoteOutput* messageToGUI = MsgConfigureRemoteOutput::create(m_settings, true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& RemoteOutput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int RemoteOutput::getSampleRate() const
{
	return m_settings.m_sampleRate;
}

quint64 RemoteOutput::getCenterFrequency() const
{
	return m_centerFrequency;
}

std::time_t RemoteOutput::getStartingTimeStamp() const
{
	return m_startingTimeStamp;
}

bool RemoteOutput::handleMessage(const Message& message)
{

    if (MsgConfigureRemoteOutput::match(message))
    {
        qDebug() << "RemoteOutput::handleMessage:" << message.getIdentifier();
	    MsgConfigureRemoteOutput& conf = (MsgConfigureRemoteOutput&) message;
        applySettings(conf.getSettings(), conf.getForce());
        return true;
    }
	else if (MsgConfigureRemoteOutputWork::match(message))
	{
		MsgConfigureRemoteOutputWork& conf = (MsgConfigureRemoteOutputWork&) message;
		bool working = conf.isWorking();

		if (m_remoteOutputWorker != 0)
		{
			if (working)
			{
			    m_remoteOutputWorker->startWork();
			}
			else
			{
			    m_remoteOutputWorker->stopWork();
			}
		}

		return true;
	}
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "RemoteOutput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initDeviceEngine())
            {
                m_deviceAPI->startDeviceEngine();
            }
        }
        else
        {
            m_deviceAPI->stopDeviceEngine();
        }

        if (m_settings.m_useReverseAPI) {
            webapiReverseSendStartStop(cmd.getStartStop());
        }

        return true;
    }
	else if (MsgConfigureRemoteOutputChunkCorrection::match(message))
	{
	    MsgConfigureRemoteOutputChunkCorrection& conf = (MsgConfigureRemoteOutputChunkCorrection&) message;

	    if (m_remoteOutputWorker != 0)
        {
	        m_remoteOutputWorker->setChunkCorrection(conf.getChunkCorrection());
        }

	    return true;
	}
	else
	{
		return false;
	}
}

void RemoteOutput::applySettings(const RemoteOutputSettings& settings, bool force)
{
    QMutexLocker mutexLocker(&m_mutex);
    bool forwardChange = false;
    bool changeTxDelay = false;
    QList<QString> reverseAPIKeys;

    if ((m_settings.m_dataAddress != settings.m_dataAddress) || force) {
        reverseAPIKeys.append("dataAddress");
    }
    if ((m_settings.m_dataPort != settings.m_dataPort) || force) {
        reverseAPIKeys.append("dataPort");
    }
    if ((m_settings.m_apiAddress != settings.m_apiAddress) || force) {
        reverseAPIKeys.append("apiAddress");
    }
    if ((m_settings.m_apiPort != settings.m_apiPort) || force) {
        reverseAPIKeys.append("apiPort");
    }

    if (force || (m_settings.m_dataAddress != settings.m_dataAddress) || (m_settings.m_dataPort != settings.m_dataPort))
    {
        if (m_remoteOutputWorker != 0) {
            m_remoteOutputWorker->setDataAddress(settings.m_dataAddress, settings.m_dataPort);
        }
    }

    if (force || (m_settings.m_sampleRate != settings.m_sampleRate))
    {
        reverseAPIKeys.append("sampleRate");

        if (m_remoteOutputWorker != 0) {
            m_remoteOutputWorker->setSamplerate(settings.m_sampleRate);
        }

        m_tickMultiplier = (21*NbSamplesForRateCorrection) / (2*settings.m_sampleRate); // two times per sample filling period plus small extension
        m_tickMultiplier = m_tickMultiplier < 20 ? 20 : m_tickMultiplier; // not below half a second

        forwardChange = true;
        changeTxDelay = true;
    }

    if (force || (m_settings.m_nbFECBlocks != settings.m_nbFECBlocks))
    {
        reverseAPIKeys.append("nbFECBlocks");

        if (m_remoteOutputWorker != 0) {
            m_remoteOutputWorker->setNbBlocksFEC(settings.m_nbFECBlocks);
        }

        changeTxDelay = true;
    }

    if (force || (m_settings.m_txDelay != settings.m_txDelay))
    {
        reverseAPIKeys.append("txDelay");
        changeTxDelay = true;
    }

    if (changeTxDelay)
    {
        if (m_remoteOutputWorker != 0) {
            m_remoteOutputWorker->setTxDelay(settings.m_txDelay);
        }
    }

    mutexLocker.unlock();

    qDebug() << "RemoteOutput::applySettings:"
            << " m_sampleRate: " << settings.m_sampleRate
            << " m_txDelay: " << settings.m_txDelay
            << " m_nbFECBlocks: " << settings.m_nbFECBlocks
            << " m_apiAddress: " << settings.m_apiAddress
            << " m_apiPort: " << settings.m_apiPort
            << " m_dataAddress: " << settings.m_dataAddress
            << " m_dataPort: " << settings.m_dataPort;

    if (forwardChange)
    {
        DSPSignalNotification *notif = new DSPSignalNotification(settings.m_sampleRate, m_centerFrequency);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    m_settings = settings;
}

int RemoteOutput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int RemoteOutput::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    MsgStartStop *message = MsgStartStop::create(run);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgStartStop *messagetoGui = MsgStartStop::create(run);
        m_guiMessageQueue->push(messagetoGui);
    }

    return 200;
}

int RemoteOutput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setRemoteOutputSettings(new SWGSDRangel::SWGRemoteOutputSettings());
    response.getRemoteOutputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int RemoteOutput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    RemoteOutputSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureRemoteOutput *msg = MsgConfigureRemoteOutput::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureRemoteOutput *msgToGUI = MsgConfigureRemoteOutput::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void RemoteOutput::webapiUpdateDeviceSettings(
        RemoteOutputSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("sampleRate")) {
        settings.m_sampleRate = response.getRemoteOutputSettings()->getSampleRate();
    }
    if (deviceSettingsKeys.contains("txDelay")) {
        settings.m_txDelay = response.getRemoteOutputSettings()->getTxDelay();
    }
    if (deviceSettingsKeys.contains("nbFECBlocks")) {
        settings.m_nbFECBlocks = response.getRemoteOutputSettings()->getNbFecBlocks();
    }
    if (deviceSettingsKeys.contains("apiAddress")) {
        settings.m_apiAddress = *response.getRemoteOutputSettings()->getApiAddress();
    }
    if (deviceSettingsKeys.contains("apiPort")) {
        settings.m_apiPort = response.getRemoteOutputSettings()->getApiPort();
    }
    if (deviceSettingsKeys.contains("dataAddress")) {
        settings.m_dataAddress = *response.getRemoteOutputSettings()->getDataAddress();
    }
    if (deviceSettingsKeys.contains("dataPort")) {
        settings.m_dataPort = response.getRemoteOutputSettings()->getDataPort();
    }
    if (deviceSettingsKeys.contains("deviceIndex")) {
        settings.m_deviceIndex = response.getRemoteOutputSettings()->getDeviceIndex();
    }
    if (deviceSettingsKeys.contains("channelIndex")) {
        settings.m_channelIndex = response.getRemoteOutputSettings()->getChannelIndex();
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getRemoteOutputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getRemoteOutputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getRemoteOutputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getRemoteOutputSettings()->getReverseApiDeviceIndex();
    }
}

int RemoteOutput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setRemoteOutputReport(new SWGSDRangel::SWGRemoteOutputReport());
    response.getRemoteOutputReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void RemoteOutput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const RemoteOutputSettings& settings)
{
    response.getRemoteOutputSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getRemoteOutputSettings()->setSampleRate(settings.m_sampleRate);
    response.getRemoteOutputSettings()->setTxDelay(settings.m_txDelay);
    response.getRemoteOutputSettings()->setNbFecBlocks(settings.m_nbFECBlocks);
    response.getRemoteOutputSettings()->setApiAddress(new QString(settings.m_apiAddress));
    response.getRemoteOutputSettings()->setApiPort(settings.m_apiPort);
    response.getRemoteOutputSettings()->setDataAddress(new QString(settings.m_dataAddress));
    response.getRemoteOutputSettings()->setDataPort(settings.m_dataPort);
    response.getRemoteOutputSettings()->setDeviceIndex(settings.m_deviceIndex);
    response.getRemoteOutputSettings()->setChannelIndex(settings.m_channelIndex);
    response.getRemoteOutputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getRemoteOutputSettings()->getReverseApiAddress()) {
        *response.getRemoteOutputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getRemoteOutputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getRemoteOutputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getRemoteOutputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

void RemoteOutput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    uint64_t ts_usecs;
    response.getRemoteOutputReport()->setBufferRwBalance(m_sampleSourceFifo.getRWBalance());
    response.getRemoteOutputReport()->setSampleCount(m_remoteOutputWorker ? (int) m_remoteOutputWorker->getSamplesCount(ts_usecs) : 0);
}

void RemoteOutput::tick()
{
    if (++m_tickCount == m_tickMultiplier)
    {
        QString reportURL;

        reportURL = QString("http://%1:%2/sdrangel/deviceset/%3/channel/%4/report")
                .arg(m_settings.m_apiAddress)
                .arg(m_settings.m_apiPort)
                .arg(m_settings.m_deviceIndex)
                .arg(m_settings.m_channelIndex);

        m_networkRequest.setUrl(QUrl(reportURL));
        m_networkManager->get(m_networkRequest);

        m_tickCount = 0;
    }
}

void RemoteOutput::networkManagerFinished(QNetworkReply *reply)
{
    if (reply->error())
    {
        qInfo("RemoteOutput::networkManagerFinished: error: %s", qPrintable(reply->errorString()));
    }
    else
    {
        QString answer = reply->readAll();

        try
        {
            QByteArray jsonBytes(answer.toStdString().c_str());
            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &error);

            if (error.error == QJsonParseError::NoError)
            {
                analyzeApiReply(doc.object(), answer);
            }
            else
            {
                QString errorMsg = QString("Reply JSON error: ") + error.errorString() + QString(" at offset ") + QString::number(error.offset);
                qInfo().noquote() << "RemoteOutput::networkManagerFinished" << errorMsg;
            }
        }
        catch (const std::exception& ex)
        {
            QString errorMsg = QString("Error parsing request: ") + ex.what();
            qInfo().noquote() << "RemoteOutput::networkManagerFinished" << errorMsg;
        }
    }

    reply->deleteLater();
}

void RemoteOutput::analyzeApiReply(const QJsonObject& jsonObject, const QString& answer)
{
    if (jsonObject.contains("RemoteSourceReport"))
    {
        QJsonObject report = jsonObject["RemoteSourceReport"].toObject();
        m_settings.m_centerFrequency = report["deviceCenterFreq"].toInt();
        m_centerFrequency = m_settings.m_centerFrequency * 1000;

        if (!m_remoteOutputWorker) {
            return;
        }

        int queueSize = report["queueSize"].toInt();
        queueSize = queueSize == 0 ? 10 : queueSize;
        int queueLength = report["queueLength"].toInt();
        int queueLengthPercent = (queueLength*100)/queueSize;
        uint64_t remoteTimestampUs = report["tvSec"].toInt()*1000000ULL + report["tvUSec"].toInt();

        uint32_t remoteSampleCountDelta, remoteSampleCount;
        remoteSampleCount = report["samplesCount"].toInt();

        if (remoteSampleCount < m_lastRemoteSampleCount) {
            remoteSampleCountDelta = (0xFFFFFFFFU - m_lastRemoteSampleCount) + remoteSampleCount + 1;
        } else {
            remoteSampleCountDelta = remoteSampleCount - m_lastRemoteSampleCount;
        }

        uint32_t sampleCountDelta, sampleCount;
        uint64_t timestampUs;
        sampleCount = m_remoteOutputWorker->getSamplesCount(timestampUs);

        if (sampleCount < m_lastSampleCount) {
            sampleCountDelta = (0xFFFFFFFFU - m_lastSampleCount) + sampleCount + 1;
        } else {
            sampleCountDelta = sampleCount - m_lastSampleCount;
        }

        // on initial state wait for queue stabilization
        if ((m_lastRemoteTimestampRateCorrection == 0) && (queueLength >= m_lastQueueLength-1) && (queueLength <= m_lastQueueLength+1))
        {
            m_lastRemoteTimestampRateCorrection = remoteTimestampUs;
            m_lastTimestampRateCorrection = timestampUs;
            m_nbRemoteSamplesSinceRateCorrection = 0;
            m_nbSamplesSinceRateCorrection = 0;
        }
        else
        {
            m_nbRemoteSamplesSinceRateCorrection += remoteSampleCountDelta;
            m_nbSamplesSinceRateCorrection += sampleCountDelta;

            qDebug("RemoteOutput::analyzeApiReply: queueLengthPercent: %d m_nbSamplesSinceRateCorrection: %u",
                queueLengthPercent,
                m_nbRemoteSamplesSinceRateCorrection);

            if (m_nbRemoteSamplesSinceRateCorrection > NbSamplesForRateCorrection)
            {
                sampleRateCorrection(remoteTimestampUs - m_lastRemoteTimestampRateCorrection,
                        timestampUs - m_lastTimestampRateCorrection,
                        m_nbRemoteSamplesSinceRateCorrection,
                        m_nbSamplesSinceRateCorrection);
                m_lastRemoteTimestampRateCorrection = remoteTimestampUs;
                m_lastTimestampRateCorrection = timestampUs;
                m_nbRemoteSamplesSinceRateCorrection = 0;
                m_nbSamplesSinceRateCorrection = 0;
            }
        }

        m_lastRemoteSampleCount = remoteSampleCount;
        m_lastSampleCount = sampleCount;
        m_lastQueueLength = queueLength;
    }
    else if (jsonObject.contains("remoteOutputSettings"))
    {
        qDebug("RemoteOutput::analyzeApiReply: reply:\n%s", answer.toStdString().c_str());
    }
}

void RemoteOutput::sampleRateCorrection(double remoteTimeDeltaUs, double timeDeltaUs, uint32_t remoteSampleCount, uint32_t sampleCount)
{
    double deltaSR = (remoteSampleCount/remoteTimeDeltaUs) - (sampleCount/timeDeltaUs);
    double chunkCorr = 50000 * deltaSR; // for 50ms chunk intervals (50000us)
    m_chunkSizeCorrection += roundf(chunkCorr);

    qDebug("RemoteOutput::sampleRateCorrection: %d (%f) samples", m_chunkSizeCorrection, chunkCorr);

    MsgConfigureRemoteOutputChunkCorrection* message = MsgConfigureRemoteOutputChunkCorrection::create(m_chunkSizeCorrection);
    getInputMessageQueue()->push(message);
}

void RemoteOutput::webapiReverseSendSettings(QList<QString>& deviceSettingsKeys, const RemoteOutputSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(1); // single Tx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("RemoteOutput"));
    swgDeviceSettings->setRemoteOutputSettings(new SWGSDRangel::SWGRemoteOutputSettings());
    SWGSDRangel::SWGRemoteOutputSettings *swgRemoteOutputSettings = swgDeviceSettings->getRemoteOutputSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("sampleRate") || force) {
        swgRemoteOutputSettings->setSampleRate(settings.m_sampleRate);
    }
    if (deviceSettingsKeys.contains("txDelay") || force) {
        swgRemoteOutputSettings->setTxDelay(settings.m_txDelay);
    }
    if (deviceSettingsKeys.contains("nbFECBlocks") || force) {
        swgRemoteOutputSettings->setNbFecBlocks(settings.m_nbFECBlocks);
    }
    if (deviceSettingsKeys.contains("apiAddress") || force) {
        swgRemoteOutputSettings->setApiAddress(new QString(settings.m_apiAddress));
    }
    if (deviceSettingsKeys.contains("apiPort") || force) {
        swgRemoteOutputSettings->setApiPort(settings.m_apiPort);
    }
    if (deviceSettingsKeys.contains("dataAddress") || force) {
        swgRemoteOutputSettings->setDataAddress(new QString(settings.m_dataAddress));
    }
    if (deviceSettingsKeys.contains("dataPort") || force) {
        swgRemoteOutputSettings->setDataPort(settings.m_dataPort);
    }
    if (deviceSettingsKeys.contains("deviceIndex") || force) {
        swgRemoteOutputSettings->setDeviceIndex(settings.m_deviceIndex);
    }
    if (deviceSettingsKeys.contains("channelIndex") || force) {
        swgRemoteOutputSettings->setChannelIndex(settings.m_channelIndex);
    }

    QString deviceSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(deviceSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgDeviceSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);

    delete swgDeviceSettings;
}

void RemoteOutput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(1); // single Tx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("RemoteOutput"));

    QString deviceSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/run")
            .arg(m_settings.m_reverseAPIAddress)
            .arg(m_settings.m_reverseAPIPort)
            .arg(m_settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(deviceSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgDeviceSettings->asJson().toUtf8());
    buffer->seek(0);
    QNetworkReply *reply;

    if (start) {
        reply = m_networkManager->sendCustomRequest(m_networkRequest, "POST", buffer);
    } else {
        reply = m_networkManager->sendCustomRequest(m_networkRequest, "DELETE", buffer);
    }

    buffer->setParent(reply);
    delete swgDeviceSettings;
}
