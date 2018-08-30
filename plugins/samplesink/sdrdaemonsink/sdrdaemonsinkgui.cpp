///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#include <QDebug>

#include <QTime>
#include <QDateTime>
#include <QString>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonParseError>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include "ui_sdrdaemonsinkgui.h"
#include "plugin/pluginapi.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"

#include "mainwindow.h"

#include "device/devicesinkapi.h"
#include "device/deviceuiset.h"
#include "sdrdaemonsinkgui.h"

SDRdaemonSinkGui::SDRdaemonSinkGui(DeviceUISet *deviceUISet, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::SDRdaemonSinkGui),
	m_deviceUISet(deviceUISet),
	m_settings(),
	m_deviceSampleSink(0),
	m_samplesCount(0),
	m_tickCount(0),
	m_nbSinceLastFlowCheck(0),
	m_lastEngineState(DSPDeviceSinkEngine::StNotStarted),
	m_doApplySettings(true),
	m_forceSettings(true)
{
    m_countUnrecoverable = 0;
    m_countRecovered = 0;
    m_lastCountUnrecoverable = 0;
    m_lastCountRecovered = 0;
    m_resetCounts = true;

    m_paletteGreenText.setColor(QPalette::WindowText, Qt::green);
    m_paletteRedText.setColor(QPalette::WindowText, Qt::red);
    m_paletteWhiteText.setColor(QPalette::WindowText, Qt::white);

    ui->setupUi(this);

	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
	ui->centerFrequency->setValueRange(7, 0, pow(10,7));

    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(7, 32000U, 9000000U);

    ui->apiAddressLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");

	connect(&(m_deviceUISet->m_deviceSinkAPI->getMasterTimer()), SIGNAL(timeout()), this, SLOT(tick()));
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);

	m_deviceSampleSink = (SDRdaemonSinkOutput*) m_deviceUISet->m_deviceSinkAPI->getSampleSink();

	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);

	m_networkManager = new QNetworkAccessManager();
	connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));

    m_time.start();
    displayEventCounts();
    displayEventTimer();

    displaySettings();
    sendSettings();
}

SDRdaemonSinkGui::~SDRdaemonSinkGui()
{
    delete m_networkManager;
	delete ui;
}

void SDRdaemonSinkGui::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void SDRdaemonSinkGui::destroy()
{
	delete this;
}

void SDRdaemonSinkGui::setName(const QString& name)
{
	setObjectName(name);
}

QString SDRdaemonSinkGui::getName() const
{
	return objectName();
}

void SDRdaemonSinkGui::resetToDefaults()
{
    blockApplySettings(true);
	m_settings.resetToDefaults();
	displaySettings();
	blockApplySettings(false);
	sendSettings();
}

qint64 SDRdaemonSinkGui::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void SDRdaemonSinkGui::setCenterFrequency(qint64 centerFrequency)
{
    m_settings.m_centerFrequency = centerFrequency;
	displaySettings();
	sendSettings();
}

QByteArray SDRdaemonSinkGui::serialize() const
{
	return m_settings.serialize();
}

bool SDRdaemonSinkGui::deserialize(const QByteArray& data)
{
    blockApplySettings(true);

	if(m_settings.deserialize(data))
	{
		displaySettings();
	    blockApplySettings(false);
		m_forceSettings = true;
		sendSettings();
		return true;
	}
	else
	{
        blockApplySettings(false);
		return false;
	}
}

bool SDRdaemonSinkGui::handleMessage(const Message& message)
{
    if (SDRdaemonSinkOutput::MsgConfigureSDRdaemonSink::match(message))
    {
        const SDRdaemonSinkOutput::MsgConfigureSDRdaemonSink& cfg = (SDRdaemonSinkOutput::MsgConfigureSDRdaemonSink&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (SDRdaemonSinkOutput::MsgReportSDRdaemonSinkStreamTiming::match(message))
	{
		m_samplesCount = ((SDRdaemonSinkOutput::MsgReportSDRdaemonSinkStreamTiming&)message).getSamplesCount();
		updateWithStreamTime();
		return true;
	}
    else if (SDRdaemonSinkOutput::MsgStartStop::match(message))
    {
        SDRdaemonSinkOutput::MsgStartStop& notif = (SDRdaemonSinkOutput::MsgStartStop&) message;
        blockApplySettings(true);
        ui->startStop->setChecked(notif.getStartStop());
        blockApplySettings(false);
        return true;
    }
	else
	{
		return false;
	}
}

void SDRdaemonSinkGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("SDRdaemonSinkGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();

            delete message;
        }
        else
        {
            if (handleMessage(*message)) {
                delete message;
            }
        }
    }
}

void SDRdaemonSinkGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)(m_sampleRate) / 1000));
}

void SDRdaemonSinkGui::updateTxDelayTooltip()
{
    double delay = ((127*127*m_settings.m_txDelay) / m_settings.m_sampleRate)/(128 + m_settings.m_nbFECBlocks);
    ui->txDelayText->setToolTip(tr("%1 us").arg(QString::number(delay*1e6, 'f', 0)));
}

void SDRdaemonSinkGui::displaySettings()
{
    blockApplySettings(true);
    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
    ui->sampleRate->setValue(m_settings.m_sampleRate);
    ui->txDelay->setValue(m_settings.m_txDelay*100);
    ui->txDelayText->setText(tr("%1").arg(m_settings.m_txDelay*100));
    ui->nbFECBlocks->setValue(m_settings.m_nbFECBlocks);

    QString s0 = QString::number(128 + m_settings.m_nbFECBlocks, 'f', 0);
    QString s1 = QString::number(m_settings.m_nbFECBlocks, 'f', 0);
    ui->nominalNbBlocksText->setText(tr("%1/%2").arg(s0).arg(s1));

    ui->apiAddress->setText(m_settings.m_apiAddress);
    ui->apiPort->setText(tr("%1").arg(m_settings.m_apiPort));
    ui->dataAddress->setText(m_settings.m_dataAddress);
    ui->dataPort->setText(tr("%1").arg(m_settings.m_dataPort));
    blockApplySettings(false);
}

void SDRdaemonSinkGui::sendSettings()
{
    if(!m_updateTimer.isActive())
        m_updateTimer.start(100);
}


void SDRdaemonSinkGui::updateHardware()
{
    qDebug() << "SDRdaemonSinkGui::updateHardware";
    SDRdaemonSinkOutput::MsgConfigureSDRdaemonSink* message = SDRdaemonSinkOutput::MsgConfigureSDRdaemonSink::create(m_settings, m_forceSettings);
    m_deviceSampleSink->getInputMessageQueue()->push(message);
    m_forceSettings = false;
    m_updateTimer.stop();
}

void SDRdaemonSinkGui::updateStatus()
{
    int state = m_deviceUISet->m_deviceSinkAPI->state();

    if(m_lastEngineState != state)
    {
        switch(state)
        {
            case DSPDeviceSinkEngine::StNotStarted:
                ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case DSPDeviceSinkEngine::StIdle:
                ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case DSPDeviceSinkEngine::StRunning:
                ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case DSPDeviceSinkEngine::StError:
                ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::information(this, tr("Message"), m_deviceUISet->m_deviceSinkAPI->errorMessage());
                break;
            default:
                break;
        }

        m_lastEngineState = state;
    }
}

void SDRdaemonSinkGui::on_centerFrequency_changed(quint64 value)
{
    m_settings.m_centerFrequency = value * 1000;
    sendSettings();
}

void SDRdaemonSinkGui::on_sampleRate_changed(quint64 value)
{
    m_settings.m_sampleRate = value;
    updateTxDelayTooltip();
    sendSettings();
}

void SDRdaemonSinkGui::on_txDelay_valueChanged(int value)
{
    m_settings.m_txDelay = value / 100.0;
    ui->txDelayText->setText(tr("%1").arg(value));
    updateTxDelayTooltip();
    sendSettings();
}

void SDRdaemonSinkGui::on_nbFECBlocks_valueChanged(int value)
{
    m_settings.m_nbFECBlocks = value;
    int nbOriginalBlocks = 128;
    int nbFECBlocks = value;
    QString s = QString::number(nbOriginalBlocks + nbFECBlocks, 'f', 0);
    QString s1 = QString::number(nbFECBlocks, 'f', 0);
    ui->nominalNbBlocksText->setText(tr("%1/%2").arg(s).arg(s1));
    updateTxDelayTooltip();
    sendSettings();
}

void SDRdaemonSinkGui::on_apiAddress_returnPressed()
{
    m_settings.m_apiAddress = ui->apiAddress->text();
    sendSettings();
}

void SDRdaemonSinkGui::on_apiPort_returnPressed()
{
    bool dataOk;
    int apiPort = ui->apiPort->text().toInt(&dataOk);

    if((!dataOk) || (apiPort < 1024) || (apiPort > 65535))
    {
        return;
    }
    else
    {
        m_settings.m_apiPort = apiPort;
    }

    sendSettings();
}

void SDRdaemonSinkGui::on_dataAddress_returnPressed()
{
    m_settings.m_dataAddress = ui->dataAddress->text();
    sendSettings();
}

void SDRdaemonSinkGui::on_dataPort_returnPressed()
{
    bool dataOk;
    int dataPort = ui->dataPort->text().toInt(&dataOk);

    if((!dataOk) || (dataPort < 1024) || (dataPort > 65535))
    {
        return;
    }
    else
    {
        m_settings.m_dataPort = dataPort;
    }

    sendSettings();
}

void SDRdaemonSinkGui::on_apiApplyButton_clicked(bool checked __attribute__((unused)))
{
    m_settings.m_apiAddress = ui->apiAddress->text();

    bool apiOk;
    int apiPort = ui->apiPort->text().toInt(&apiOk);

    if((apiOk) && (apiPort >= 1024) && (apiPort < 65535))
    {
        m_settings.m_apiPort = apiPort;
    }

    sendSettings();
}

void SDRdaemonSinkGui::on_dataApplyButton_clicked(bool checked __attribute__((unused)))
{
    m_settings.m_dataAddress = ui->dataAddress->text();

    bool dataOk;
    int udpDataPort = ui->dataPort->text().toInt(&dataOk);

    if((dataOk) && (udpDataPort >= 1024) && (udpDataPort < 65535))
    {
        m_settings.m_dataPort = udpDataPort;
    }

    sendSettings();
}

void SDRdaemonSinkGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        SDRdaemonSinkOutput::MsgStartStop *message = SDRdaemonSinkOutput::MsgStartStop::create(checked);
        m_deviceSampleSink->getInputMessageQueue()->push(message);
    }
}

void SDRdaemonSinkGui::on_eventCountsReset_clicked(bool checked __attribute__((unused)))
{
    m_countUnrecoverable = 0;
    m_countRecovered = 0;
    m_time.start();
    displayEventCounts();
    displayEventTimer();
}

void SDRdaemonSinkGui::displayEventCounts()
{
    QString nstr = QString("%1").arg(m_countUnrecoverable, 3, 10, QChar('0'));
    ui->eventUnrecText->setText(nstr);
    nstr = QString("%1").arg(m_countRecovered, 3, 10, QChar('0'));
    ui->eventRecText->setText(nstr);
}

void SDRdaemonSinkGui::displayEventStatus(int recoverableCount, int unrecoverableCount)
{

    if (unrecoverableCount == 0)
    {
        if (recoverableCount == 0) {
            ui->allFramesDecoded->setStyleSheet("QToolButton { background-color : green; }");
        } else {
            ui->allFramesDecoded->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        }
    }
    else
    {
        ui->allFramesDecoded->setStyleSheet("QToolButton { background-color : red; }");
    }
}

void SDRdaemonSinkGui::displayEventTimer()
{
    int elapsedTimeMillis = m_time.elapsed();
    QTime recordLength(0, 0, 0, 0);
    recordLength = recordLength.addSecs(elapsedTimeMillis/1000);
    QString s_time = recordLength.toString("HH:mm:ss");
    ui->eventCountsTimeText->setText(s_time);
}

void SDRdaemonSinkGui::updateWithStreamTime()
{
	int t_sec = 0;
	int t_msec = 0;

	if (m_settings.m_sampleRate > 0){
		t_msec = ((m_samplesCount * 1000) / m_settings.m_sampleRate) % 1000;
		t_sec = m_samplesCount / m_settings.m_sampleRate;
	}

	QTime t(0, 0, 0, 0);
	t = t.addSecs(t_sec);
	t = t.addMSecs(t_msec);
	QString s_timems = t.toString("HH:mm:ss.zzz");
	//ui->relTimeText->setText(s_timems); TODO with absolute time
}

void SDRdaemonSinkGui::tick()
{
	if ((++m_tickCount & 0xf) == 0) // 16*50ms ~800ms
	{
	    QString reportURL = QString("http://%1:%2/sdrdaemon/channel/report").arg(m_settings.m_apiAddress).arg(m_settings.m_apiPort);
	    m_networkRequest.setUrl(QUrl(reportURL));
	    m_networkManager->get(m_networkRequest);

//		SDRdaemonSinkOutput::MsgConfigureSDRdaemonSinkStreamTiming* message = SDRdaemonSinkOutput::MsgConfigureSDRdaemonSinkStreamTiming::create();
//		m_deviceSampleSink->getInputMessageQueue()->push(message);

        displayEventTimer();
	}
}

void SDRdaemonSinkGui::networkManagerFinished(QNetworkReply *reply)
{
    if (reply->error())
    {
        ui->apiAddressLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
        qDebug() << "SDRdaemonSinkGui::networkManagerFinished" << reply->errorString();
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
            ui->apiAddressLabel->setStyleSheet("QLabel { background-color : green; }");
            analyzeChannelReport(doc.object());
        }
        else
        {
            ui->apiAddressLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
            QString errorMsg = QString("Reply JSON error: ") + error.errorString() + QString(" at offset ") + QString::number(error.offset);
            qDebug().noquote() << "SDRdaemonSinkGui::networkManagerFinished" << errorMsg;
        }
    }
    catch (const std::exception& ex)
    {
        ui->apiAddressLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
        QString errorMsg = QString("Error parsing request: ") + ex.what();
        qDebug().noquote() << "SDRdaemonSinkGui::networkManagerFinished" << errorMsg;
    }
}

void SDRdaemonSinkGui::analyzeChannelReport(const QJsonObject& jsonObject)
{
    if (jsonObject.contains("SDRDaemonChannelSourceReport"))
    {
        QJsonObject report = jsonObject["SDRDaemonChannelSourceReport"].toObject();
        int queueSize = report["queueSize"].toInt();
        queueSize = queueSize == 0 ? 10 : queueSize;
        int queueLength = report["queueLength"].toInt();
        QString queueLengthText = QString("%1/%2").arg(queueLength).arg(queueSize);
        ui->queueLengthText->setText(queueLengthText);
        ui->queueLengthGauge->setValue((queueLength*100)/queueSize);
        int unrecoverableCount = report["uncorrectableErrorsCount"].toInt();
        int recoverableCount = report["correctableErrorsCount"].toInt();

        if (!m_resetCounts)
        {
            int recoverableCountDelta = recoverableCount - m_lastCountRecovered;
            int unrecoverableCountDelta = unrecoverableCount - m_lastCountUnrecoverable;
            displayEventStatus(recoverableCountDelta, unrecoverableCountDelta);
            m_countRecovered += recoverableCountDelta;
            m_countUnrecoverable += unrecoverableCountDelta;
            displayEventCounts();
        }

        m_resetCounts = false;
        m_lastCountRecovered = recoverableCount;
        m_lastCountUnrecoverable = unrecoverableCount;
    }
}
