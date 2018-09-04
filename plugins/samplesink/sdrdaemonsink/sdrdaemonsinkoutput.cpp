///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <sys/time.h>
#include <string.h>
#include <errno.h>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonParseError>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGSDRdaemonSinkReport.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/filerecord.h"

#include "device/devicesinkapi.h"

#include "sdrdaemonsinkoutput.h"
#include "sdrdaemonsinkthread.h"

MESSAGE_CLASS_DEFINITION(SDRdaemonSinkOutput::MsgConfigureSDRdaemonSink, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSinkOutput::MsgConfigureSDRdaemonSinkWork, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSinkOutput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSinkOutput::MsgConfigureSDRdaemonSinkChunkCorrection, Message)

const uint32_t SDRdaemonSinkOutput::NbSamplesForRateCorrection = 5000000;

SDRdaemonSinkOutput::SDRdaemonSinkOutput(DeviceSinkAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
    m_sdrDaemonSinkThread(0),
	m_deviceDescription("SDRdaemonSink"),
    m_startingTimeStamp(0),
	m_masterTimer(deviceAPI->getMasterTimer()),
	m_tickCount(0),
    m_tickMultiplier(20),
	m_lastRemoteSampleCount(0),
	m_lastSampleCount(0),
	m_lastRemoteTimestampRateCorrection(0),
	m_lastTimestampRateCorrection(0),
	m_nbRemoteSamplesSinceRateCorrection(0),
	m_nbSamplesSinceRateCorrection(0),
	m_chunkSizeCorrection(0)
{
    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    connect(&m_masterTimer, SIGNAL(timeout()), this, SLOT(tick()));
}

SDRdaemonSinkOutput::~SDRdaemonSinkOutput()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
	stop();
	delete m_networkManager;
}

void SDRdaemonSinkOutput::destroy()
{
    delete this;
}

bool SDRdaemonSinkOutput::start()
{
	QMutexLocker mutexLocker(&m_mutex);
	qDebug() << "SDRdaemonSinkOutput::start";

	m_sdrDaemonSinkThread = new SDRdaemonSinkThread(&m_sampleSourceFifo);
	m_sdrDaemonSinkThread->setDataAddress(m_settings.m_dataAddress, m_settings.m_dataPort);
	m_sdrDaemonSinkThread->setCenterFrequency(m_settings.m_centerFrequency);
	m_sdrDaemonSinkThread->setSamplerate(m_settings.m_sampleRate);
	m_sdrDaemonSinkThread->setNbBlocksFEC(m_settings.m_nbFECBlocks);
	m_sdrDaemonSinkThread->connectTimer(m_masterTimer);
	m_sdrDaemonSinkThread->startWork();

    double delay = ((127*127*m_settings.m_txDelay) / m_settings.m_sampleRate)/(128 + m_settings.m_nbFECBlocks);
    m_sdrDaemonSinkThread->setTxDelay((int) (delay*1e6));

	mutexLocker.unlock();
	//applySettings(m_generalSettings, m_settings, true);
	qDebug("SDRdaemonSinkOutput::start: started");

	return true;
}

void SDRdaemonSinkOutput::init()
{
    applySettings(m_settings, true);
}

void SDRdaemonSinkOutput::stop()
{
	qDebug() << "SDRdaemonSinkOutput::stop";
	QMutexLocker mutexLocker(&m_mutex);

	if(m_sdrDaemonSinkThread != 0)
	{
	    m_sdrDaemonSinkThread->stopWork();
		delete m_sdrDaemonSinkThread;
		m_sdrDaemonSinkThread = 0;
	}
}

QByteArray SDRdaemonSinkOutput::serialize() const
{
    return m_settings.serialize();
}

bool SDRdaemonSinkOutput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureSDRdaemonSink* message = MsgConfigureSDRdaemonSink::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureSDRdaemonSink* messageToGUI = MsgConfigureSDRdaemonSink::create(m_settings, true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& SDRdaemonSinkOutput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int SDRdaemonSinkOutput::getSampleRate() const
{
	return m_settings.m_sampleRate;
}

quint64 SDRdaemonSinkOutput::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void SDRdaemonSinkOutput::setCenterFrequency(qint64 centerFrequency)
{
    SDRdaemonSinkSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureSDRdaemonSink* message = MsgConfigureSDRdaemonSink::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureSDRdaemonSink* messageToGUI = MsgConfigureSDRdaemonSink::create(settings, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

std::time_t SDRdaemonSinkOutput::getStartingTimeStamp() const
{
	return m_startingTimeStamp;
}

bool SDRdaemonSinkOutput::handleMessage(const Message& message)
{

    if (MsgConfigureSDRdaemonSink::match(message))
    {
        qDebug() << "SDRdaemonSinkOutput::handleMessage:" << message.getIdentifier();
	    MsgConfigureSDRdaemonSink& conf = (MsgConfigureSDRdaemonSink&) message;
        applySettings(conf.getSettings(), conf.getForce());
        return true;
    }
	else if (MsgConfigureSDRdaemonSinkWork::match(message))
	{
		MsgConfigureSDRdaemonSinkWork& conf = (MsgConfigureSDRdaemonSinkWork&) message;
		bool working = conf.isWorking();

		if (m_sdrDaemonSinkThread != 0)
		{
			if (working)
			{
			    m_sdrDaemonSinkThread->startWork();
			}
			else
			{
			    m_sdrDaemonSinkThread->stopWork();
			}
		}

		return true;
	}
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "SDRdaemonSinkOutput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initGeneration())
            {
                m_deviceAPI->startGeneration();
            }
        }
        else
        {
            m_deviceAPI->stopGeneration();
        }

        return true;
    }
	else if (MsgConfigureSDRdaemonSinkChunkCorrection::match(message))
	{
	    MsgConfigureSDRdaemonSinkChunkCorrection& conf = (MsgConfigureSDRdaemonSinkChunkCorrection&) message;

	    if (m_sdrDaemonSinkThread != 0)
        {
	        m_sdrDaemonSinkThread->setChunkCorrection(conf.getChunkCorrection());
        }

	    return true;
	}
	else
	{
		return false;
	}
}

void SDRdaemonSinkOutput::applySettings(const SDRdaemonSinkSettings& settings, bool force)
{
    QMutexLocker mutexLocker(&m_mutex);
    bool forwardChange = false;
    bool changeTxDelay = false;

    if (force || (m_settings.m_dataAddress != settings.m_dataAddress) || (m_settings.m_dataPort != settings.m_dataPort))
    {
        if (m_sdrDaemonSinkThread != 0) {
            m_sdrDaemonSinkThread->setDataAddress(settings.m_dataAddress, settings.m_dataPort);
        }
    }

    if (force || (m_settings.m_centerFrequency != settings.m_centerFrequency))
    {
        if (m_sdrDaemonSinkThread != 0) {
            m_sdrDaemonSinkThread->setCenterFrequency(settings.m_centerFrequency);
        }

        forwardChange = true;
    }

    if (force || (m_settings.m_sampleRate != settings.m_sampleRate))
    {
        if (m_sdrDaemonSinkThread != 0) {
            m_sdrDaemonSinkThread->setSamplerate(settings.m_sampleRate);
        }

        m_tickMultiplier = (21*NbSamplesForRateCorrection) / (2*settings.m_sampleRate); // two times per sample filling period plus small extension
        m_tickMultiplier = m_tickMultiplier < 20 ? 20 : m_tickMultiplier; // not below half a second

        forwardChange = true;
        changeTxDelay = true;
    }

    if (force || (m_settings.m_nbFECBlocks != settings.m_nbFECBlocks))
    {
        if (m_sdrDaemonSinkThread != 0) {
            m_sdrDaemonSinkThread->setNbBlocksFEC(settings.m_nbFECBlocks);
        }

        changeTxDelay = true;
    }

    if (force || (m_settings.m_txDelay != settings.m_txDelay))
    {
        changeTxDelay = true;
    }

    if (changeTxDelay)
    {
        double delay = ((127*127*settings.m_txDelay) / settings.m_sampleRate)/(128 + settings.m_nbFECBlocks);
        qDebug("SDRdaemonSinkOutput::applySettings: Tx delay: %f us", delay*1e6);

        if (m_sdrDaemonSinkThread != 0)
        {
            // delay is calculated as a fraction of the nominal UDP block process time
            // frame size: 127 * 127 samples
            // divided by sample rate gives the frame process time
            // divided by the number of actual blocks including FEC blocks gives the block (i.e. UDP block) process time
            m_sdrDaemonSinkThread->setTxDelay((int) (delay*1e6));
        }
    }

    mutexLocker.unlock();

    qDebug() << "SDRdaemonSinkOutput::applySettings:"
            << " m_centerFrequency: " << settings.m_centerFrequency
            << " m_sampleRate: " << settings.m_sampleRate
            << " m_txDelay: " << settings.m_txDelay
            << " m_nbFECBlocks: " << settings.m_nbFECBlocks
            << " m_apiAddress: " << settings.m_apiAddress
            << " m_apiPort: " << settings.m_apiPort
            << " m_dataAddress: " << settings.m_dataAddress
            << " m_dataPort: " << settings.m_dataPort;

    if (forwardChange)
    {
        DSPSignalNotification *notif = new DSPSignalNotification(settings.m_sampleRate, settings.m_centerFrequency);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    m_settings = settings;
}

int SDRdaemonSinkOutput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int SDRdaemonSinkOutput::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
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

int SDRdaemonSinkOutput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage __attribute__((unused)))
{
    response.setSdrDaemonSinkSettings(new SWGSDRangel::SWGSDRdaemonSinkSettings());
    response.getSdrDaemonSinkSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int SDRdaemonSinkOutput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage __attribute__((unused)))
{
    SDRdaemonSinkSettings settings = m_settings;

    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getSdrDaemonSinkSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("sampleRate")) {
        settings.m_sampleRate = response.getSdrDaemonSinkSettings()->getSampleRate();
    }
    if (deviceSettingsKeys.contains("txDelay")) {
        settings.m_txDelay = response.getSdrDaemonSinkSettings()->getTxDelay();
    }
    if (deviceSettingsKeys.contains("nbFECBlocks")) {
        settings.m_nbFECBlocks = response.getSdrDaemonSinkSettings()->getNbFecBlocks();
    }
    if (deviceSettingsKeys.contains("apiAddress")) {
        settings.m_apiAddress = *response.getSdrDaemonSinkSettings()->getApiAddress();
    }
    if (deviceSettingsKeys.contains("apiPort")) {
        settings.m_apiPort = response.getSdrDaemonSinkSettings()->getApiPort();
    }
    if (deviceSettingsKeys.contains("dataAddress")) {
        settings.m_dataAddress = *response.getSdrDaemonSinkSettings()->getDataAddress();
    }
    if (deviceSettingsKeys.contains("dataPort")) {
        settings.m_dataPort = response.getSdrDaemonSinkSettings()->getDataPort();
    }
    if (deviceSettingsKeys.contains("deviceIndex")) {
        settings.m_deviceIndex = response.getSdrDaemonSinkSettings()->getDeviceIndex();
    }
    if (deviceSettingsKeys.contains("channelIndex")) {
        settings.m_channelIndex = response.getSdrDaemonSinkSettings()->getChannelIndex();
    }

    MsgConfigureSDRdaemonSink *msg = MsgConfigureSDRdaemonSink::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureSDRdaemonSink *msgToGUI = MsgConfigureSDRdaemonSink::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

int SDRdaemonSinkOutput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage __attribute__((unused)))
{
    response.setSdrDaemonSinkReport(new SWGSDRangel::SWGSDRdaemonSinkReport());
    response.getSdrDaemonSinkReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void SDRdaemonSinkOutput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const SDRdaemonSinkSettings& settings)
{
    response.getSdrDaemonSinkSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getSdrDaemonSinkSettings()->setSampleRate(settings.m_sampleRate);
    response.getSdrDaemonSinkSettings()->setTxDelay(settings.m_txDelay);
    response.getSdrDaemonSinkSettings()->setNbFecBlocks(settings.m_nbFECBlocks);
    response.getSdrDaemonSinkSettings()->setApiAddress(new QString(settings.m_apiAddress));
    response.getSdrDaemonSinkSettings()->setApiPort(settings.m_apiPort);
    response.getSdrDaemonSinkSettings()->setDataAddress(new QString(settings.m_dataAddress));
    response.getSdrDaemonSinkSettings()->setDataPort(settings.m_dataPort);
    response.getSdrDaemonSinkSettings()->setDeviceIndex(settings.m_deviceIndex);
    response.getSdrDaemonSinkSettings()->setChannelIndex(settings.m_channelIndex);
}

void SDRdaemonSinkOutput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    struct timeval tv;
    response.getSdrDaemonSinkReport()->setBufferRwBalance(m_sampleSourceFifo.getRWBalance());
    response.getSdrDaemonSinkReport()->setSampleCount(m_sdrDaemonSinkThread ? (int) m_sdrDaemonSinkThread->getSamplesCount(tv) : 0);
}

void SDRdaemonSinkOutput::tick()
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

void SDRdaemonSinkOutput::networkManagerFinished(QNetworkReply *reply)
{
    if (reply->error())
    {
        qInfo("SDRdaemonSinkOutput::networkManagerFinished: error: %s", qPrintable(reply->errorString()));
        return;
    }

    QString answer = reply->readAll();

    try
    {
        QByteArray jsonBytes(answer.toStdString().c_str());
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &error);

        if (error.error == QJsonParseError::NoError)
        {
            analyzeApiReply(doc.object());
        }
        else
        {
            QString errorMsg = QString("Reply JSON error: ") + error.errorString() + QString(" at offset ") + QString::number(error.offset);
            qInfo().noquote() << "SDRdaemonSinkOutput::networkManagerFinished" << errorMsg;
        }
    }
    catch (const std::exception& ex)
    {
        QString errorMsg = QString("Error parsing request: ") + ex.what();
        qInfo().noquote() << "SDRdaemonSinkOutput::networkManagerFinished" << errorMsg;
    }
}

void SDRdaemonSinkOutput::analyzeApiReply(const QJsonObject& jsonObject)
{
    if (!m_sdrDaemonSinkThread) {
        return;
    }

    if (jsonObject.contains("DaemonSourceReport"))
    {
        QJsonObject report = jsonObject["DaemonSourceReport"].toObject();
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
        struct timeval tv;
        sampleCount = m_sdrDaemonSinkThread->getSamplesCount(tv);

        if (sampleCount < m_lastSampleCount) {
            sampleCountDelta = (0xFFFFFFFFU - m_lastSampleCount) + sampleCount + 1;
        } else {
            sampleCountDelta = sampleCount - m_lastSampleCount;
        }

        uint64_t timestampUs = tv.tv_sec*1000000ULL + tv.tv_usec;

        if (m_lastRemoteTimestampRateCorrection == 0)
        {
            m_lastRemoteTimestampRateCorrection = remoteTimestampUs;
            m_lastTimestampRateCorrection = timestampUs;
        }
        else
        {
            m_nbRemoteSamplesSinceRateCorrection += remoteSampleCountDelta;
            m_nbSamplesSinceRateCorrection += sampleCountDelta;

            qDebug("SDRdaemonSinkOutput::analyzeApiReply: queueLengthPercent: %d m_nbSamplesSinceRateCorrection: %u",
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
    }
}

void SDRdaemonSinkOutput::sampleRateCorrection(double remoteTimeDeltaUs, double timeDeltaUs, uint32_t remoteSampleCount, uint32_t sampleCount)
{
    double deltaSR = (remoteSampleCount/remoteTimeDeltaUs) - (sampleCount/timeDeltaUs);
    double chunkCorr = 50000 * deltaSR; // for 50ms chunk intervals (50000us)
    m_chunkSizeCorrection += roundf(chunkCorr);

    qDebug("SDRdaemonSinkOutput::sampleRateCorrection: %d (%f) samples", m_chunkSizeCorrection, chunkCorr);

    MsgConfigureSDRdaemonSinkChunkCorrection* message = MsgConfigureSDRdaemonSinkChunkCorrection::create(m_chunkSizeCorrection);
    getInputMessageQueue()->push(message);
}
