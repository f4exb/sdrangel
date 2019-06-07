///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Vort                                                       //
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QBuffer>

#include "kiwisdrinput.h"
#include "device/deviceapi.h"
#include "kiwisdrworker.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/filerecord.h"

MESSAGE_CLASS_DEFINITION(KiwiSDRInput::MsgConfigureKiwiSDR, Message)
MESSAGE_CLASS_DEFINITION(KiwiSDRInput::MsgFileRecord, Message)
MESSAGE_CLASS_DEFINITION(KiwiSDRInput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(KiwiSDRInput::MsgSetStatus, Message)


KiwiSDRInput::KiwiSDRInput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_kiwiSDRWorker(nullptr),
	m_deviceDescription(),
	m_running(false),
	m_masterTimer(deviceAPI->getMasterTimer())
{
	m_kiwiSDRWorkerThread.start();

    m_fileSink = new FileRecord();
    m_deviceAPI->setNbSourceStreams(1);
    m_deviceAPI->addAncillarySink(m_fileSink);

    if (!m_sampleFifo.setSize(getSampleRate() * 2)) {
        qCritical("KiwiSDRInput::KiwiSDRInput: Could not allocate SampleFifo");
    }

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

KiwiSDRInput::~KiwiSDRInput()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;

    if (m_running) {
        stop();
    }

	m_kiwiSDRWorkerThread.quit();
	m_kiwiSDRWorkerThread.wait();

    m_deviceAPI->removeAncillarySink(m_fileSink);
    delete m_fileSink;
}

void KiwiSDRInput::destroy()
{
    delete this;
}

void KiwiSDRInput::init()
{
    applySettings(m_settings, true);
}

bool KiwiSDRInput::start()
{
	QMutexLocker mutexLocker(&m_mutex);

    if (m_running) stop();

	m_kiwiSDRWorker = new KiwiSDRWorker(&m_sampleFifo);
	m_kiwiSDRWorker->moveToThread(&m_kiwiSDRWorkerThread);

	connect(this, &KiwiSDRInput::setWorkerCenterFrequency, m_kiwiSDRWorker, &KiwiSDRWorker::onCenterFrequencyChanged);
	connect(this, &KiwiSDRInput::setWorkerServerAddress, m_kiwiSDRWorker, &KiwiSDRWorker::onServerAddressChanged);
	connect(this, &KiwiSDRInput::setWorkerGain, m_kiwiSDRWorker, &KiwiSDRWorker::onGainChanged);
	connect(m_kiwiSDRWorker, &KiwiSDRWorker::updateStatus, this, &KiwiSDRInput::setWorkerStatus);

	mutexLocker.unlock();

	applySettings(m_settings, true);
	m_running = true;

	return true;
}

void KiwiSDRInput::stop()
{
	QMutexLocker mutexLocker(&m_mutex);

	setWorkerStatus(0);

	if (m_kiwiSDRWorker != 0)
	{
		m_kiwiSDRWorker->deleteLater();
		m_kiwiSDRWorker = 0;
	}

	m_running = false;
}

QByteArray KiwiSDRInput::serialize() const
{
    return m_settings.serialize();
}

bool KiwiSDRInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureKiwiSDR* message = MsgConfigureKiwiSDR::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureKiwiSDR* messageToGUI = MsgConfigureKiwiSDR::create(m_settings, true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& KiwiSDRInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int KiwiSDRInput::getSampleRate() const
{
	return 12000;
}

quint64 KiwiSDRInput::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void KiwiSDRInput::setCenterFrequency(qint64 centerFrequency)
{
	KiwiSDRSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureKiwiSDR* message = MsgConfigureKiwiSDR::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureKiwiSDR* messageToGUI = MsgConfigureKiwiSDR::create(settings, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

void KiwiSDRInput::setWorkerStatus(int status)
{
	if (m_guiMessageQueue)
		m_guiMessageQueue->push(MsgSetStatus::create(status));
}

bool KiwiSDRInput::handleMessage(const Message& message)
{
    if (MsgConfigureKiwiSDR::match(message))
    {
        MsgConfigureKiwiSDR& conf = (MsgConfigureKiwiSDR&) message;
        qDebug() << "KiwiSDRInput::handleMessage: MsgConfigureKiwiSDR";

        bool success = applySettings(conf.getSettings(), conf.getForce());

        if (!success)
        {
            qDebug("KiwiSDRInput::handleMessage: config error");
        }

        return true;
    }
    else if (MsgFileRecord::match(message))
    {
        MsgFileRecord& conf = (MsgFileRecord&) message;
        qDebug() << "KiwiSDRInput::handleMessage: MsgFileRecord: " << conf.getStartStop();

        if (conf.getStartStop())
        {
            m_fileSink->genUniqueFileName(m_deviceAPI->getDeviceUID());
            m_fileSink->startRecording();
        }
        else
        {
            m_fileSink->stopRecording();
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "KiwiSDRInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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
    else
    {
        return false;
    }
}

bool KiwiSDRInput::applySettings(const KiwiSDRSettings& settings, bool force)
{
    QList<QString> reverseAPIKeys;

	if (m_settings.m_serverAddress != settings.m_serverAddress || force)
		emit setWorkerServerAddress(settings.m_serverAddress);

	if (m_settings.m_gain != settings.m_gain ||
		m_settings.m_useAGC != settings.m_useAGC || force)
	{
		emit setWorkerGain(settings.m_gain, settings.m_useAGC);
	}

    if (m_settings.m_centerFrequency != settings.m_centerFrequency || force)
    {
        reverseAPIKeys.append("centerFrequency");

        emit setWorkerCenterFrequency(settings.m_centerFrequency);

		DSPSignalNotification *notif = new DSPSignalNotification(
			getSampleRate(), settings.m_centerFrequency);
		m_fileSink->handleMessage(*notif); // forward to file sink
		m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
	}

    if (settings.m_useReverseAPI)
    {
        qDebug("KiwiSDRInput::applySettings: call webapiReverseSendSettings");
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    m_settings = settings;
    return true;
}

int KiwiSDRInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    return 404;
}

int KiwiSDRInput::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
	return 404;
}

int KiwiSDRInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    return 404;
}

int KiwiSDRInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    return 404;
}

void KiwiSDRInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const KiwiSDRSettings& settings)
{
}

void KiwiSDRInput::webapiReverseSendSettings(QList<QString>& deviceSettingsKeys, const KiwiSDRSettings& settings, bool force)
{
}

void KiwiSDRInput::webapiReverseSendStartStop(bool start)
{
}

void KiwiSDRInput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "KiwiSDRInput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
        return;
    }

    QString answer = reply->readAll();
    answer.chop(1); // remove last \n
    qDebug("KiwiSDRInput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
}
