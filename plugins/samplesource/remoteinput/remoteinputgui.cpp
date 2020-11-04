///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#include <stdint.h>
#include <sstream>
#include <iostream>

#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>
#include <QTime>
#include <QDateTime>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonParseError>
#include <QJsonObject>

#include "ui_remoteinputgui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/crightclickenabler.h"
#include "gui/basicdevicesettingsdialog.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "mainwindow.h"
#include "util/simpleserializer.h"
#include "device/deviceapi.h"
#include "device/deviceuiset.h"
#include "remoteinputgui.h"


RemoteInputGui::RemoteInputGui(DeviceUISet *deviceUISet, QWidget* parent) :
	DeviceGUI(parent),
	ui(new Ui::RemoteInputGui),
	m_deviceUISet(deviceUISet),
	m_settings(),
	m_sampleSource(0),
	m_acquisition(false),
	m_streamSampleRate(0),
	m_streamCenterFrequency(0),
	m_lastEngineState(DeviceAPI::StNotStarted),
	m_framesDecodingStatus(0),
	m_bufferLengthInSecs(0.0),
    m_bufferGauge(-50),
	m_nbOriginalBlocks(128),
    m_nbFECBlocks(0),
    m_sampleBits(16), // assume 16 bits to start with
    m_sampleBytes(2),
    m_samplesCount(0),
    m_tickCount(0),
    m_addressEdited(false),
    m_dataPortEdited(false),
	m_countUnrecoverable(0),
	m_countRecovered(0),
    m_doApplySettings(true),
    m_forceSettings(true),
    m_txDelay(0.0)
{
    m_paletteGreenText.setColor(QPalette::WindowText, Qt::green);
    m_paletteWhiteText.setColor(QPalette::WindowText, Qt::white);

	m_startingTimeStampms = 0;
	ui->setupUi(this);

	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
	ui->centerFrequency->setValueRange(7, 0, 9999999U);

	ui->centerFrequencyHz->setColorMapper(ColorMapper(ColorMapper::GrayGold));
	ui->centerFrequencyHz->setValueRange(3, 0, 999U);

    CRightClickEnabler *startStopRightClickEnabler = new CRightClickEnabler(ui->startStop);
    connect(startStopRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));

	displaySettings();

	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);
    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));

    m_sampleSource = (RemoteInput*) m_deviceUISet->m_deviceAPI->getSampleSource();

	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
	m_sampleSource->setMessageQueueToGUI(&m_inputMessageQueue);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));

    m_eventsTime.start();
    displayEventCounts();
    displayEventTimer();

    m_forceSettings = true;
    sendSettings();
}

RemoteInputGui::~RemoteInputGui()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
	delete ui;
}

void RemoteInputGui::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void RemoteInputGui::destroy()
{
	delete this;
}

void RemoteInputGui::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    m_forceSettings = true;
    sendSettings();
}

QByteArray RemoteInputGui::serialize() const
{
    return m_settings.serialize();
}

bool RemoteInputGui::deserialize(const QByteArray& data)
{
    qDebug("RemoteInputGui::deserialize");

    if (m_settings.deserialize(data))
    {
        displaySettings();
        m_forceSettings = true;
        sendSettings();

        return true;
    }
    else
    {
        return false;
    }
}

bool RemoteInputGui::handleMessage(const Message& message)
{
    if (RemoteInput::MsgConfigureRemoteInput::match(message))
    {
        const RemoteInput::MsgConfigureRemoteInput& cfg = (RemoteInput::MsgConfigureRemoteInput&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (RemoteInput::MsgReportRemoteInputAcquisition::match(message))
	{
		m_acquisition = ((RemoteInput::MsgReportRemoteInputAcquisition&)message).getAcquisition();
		updateWithAcquisition();
		return true;
	}
	else if (RemoteInput::MsgReportRemoteInputStreamData::match(message))
	{
        m_startingTimeStampms = ((RemoteInput::MsgReportRemoteInputStreamData&)message).get_tv_msec();

        qDebug() << "RemoteInputGui::handleMessage: RemoteInput::MsgReportRemoteInputStreamData: "
                << " : " << m_startingTimeStampms << " ms";

        updateWithStreamTime();
        return true;
	}
	else if (RemoteInput::MsgReportRemoteInputStreamTiming::match(message))
	{
		m_startingTimeStampms = ((RemoteInput::MsgReportRemoteInputStreamTiming&)message).get_tv_msec();
		m_framesDecodingStatus = ((RemoteInput::MsgReportRemoteInputStreamTiming&)message).getFramesDecodingStatus();
		m_allBlocksReceived = ((RemoteInput::MsgReportRemoteInputStreamTiming&)message).allBlocksReceived();
		m_bufferLengthInSecs = ((RemoteInput::MsgReportRemoteInputStreamTiming&)message).getBufferLengthInSecs();
        m_bufferGauge = ((RemoteInput::MsgReportRemoteInputStreamTiming&)message).getBufferGauge();
        m_minNbBlocks = ((RemoteInput::MsgReportRemoteInputStreamTiming&)message).getMinNbBlocks();
        m_minNbOriginalBlocks = ((RemoteInput::MsgReportRemoteInputStreamTiming&)message).getMinNbOriginalBlocks();
        m_maxNbRecovery = ((RemoteInput::MsgReportRemoteInputStreamTiming&)message).getMaxNbRecovery();
        m_avgNbBlocks = ((RemoteInput::MsgReportRemoteInputStreamTiming&)message).getAvgNbBlocks();
        m_avgNbOriginalBlocks = ((RemoteInput::MsgReportRemoteInputStreamTiming&)message).getAvgNbOriginalBlocks();
        m_avgNbRecovery = ((RemoteInput::MsgReportRemoteInputStreamTiming&)message).getAvgNbRecovery();
        m_nbOriginalBlocks = ((RemoteInput::MsgReportRemoteInputStreamTiming&)message).getNbOriginalBlocksPerFrame();
        m_sampleBits = ((RemoteInput::MsgReportRemoteInputStreamTiming&)message).getSampleBits();
        m_sampleBytes = ((RemoteInput::MsgReportRemoteInputStreamTiming&)message).getSampleBytes();

        int nbFECBlocks = ((RemoteInput::MsgReportRemoteInputStreamTiming&)message).getNbFECBlocksPerFrame();

        if (m_nbFECBlocks != nbFECBlocks) {
            m_nbFECBlocks = nbFECBlocks;
        }

		updateWithStreamTime();
		return true;
	}
	else if (RemoteInput::MsgStartStop::match(message))
    {
	    RemoteInput::MsgStartStop& notif = (RemoteInput::MsgStartStop&) message;
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

void RemoteInputGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        //qDebug("RemoteInputGui::handleInputMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;

            if (notif->getSampleRate() != m_streamSampleRate) {
                m_streamSampleRate = notif->getSampleRate();
            }

            m_streamCenterFrequency = notif->getCenterFrequency();

            qDebug("RemoteInputGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());

            updateSampleRateAndFrequency();
            DSPSignalNotification *fwd = new DSPSignalNotification(*notif);
            m_sampleSource->getInputMessageQueue()->push(fwd);

            delete message;
        }
        else
        {
            if (handleMessage(*message))
            {
                delete message;
            }
        }
    }
}

void RemoteInputGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_streamSampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_streamCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_streamSampleRate / 1000));
    blockApplySettings(true);
    ui->centerFrequency->setValue(m_streamCenterFrequency / 1000);
    ui->centerFrequencyHz->setValue(m_streamCenterFrequency % 1000);
    blockApplySettings(false);
}

void RemoteInputGui::displaySettings()
{
    blockApplySettings(true);

    ui->centerFrequency->setValue(m_streamCenterFrequency / 1000);
    ui->centerFrequencyHz->setValue(m_streamCenterFrequency % 1000);
    ui->deviceRateText->setText(tr("%1k").arg(m_streamSampleRate / 1000.0));

    ui->apiAddress->setText(m_settings.m_apiAddress);
    ui->apiPort->setText(tr("%1").arg(m_settings.m_apiPort));
    ui->dataPort->setText(tr("%1").arg(m_settings.m_dataPort));
    ui->dataAddress->setText(m_settings.m_dataAddress);
    ui->multicastAddress->setText(m_settings.m_multicastAddress);
    ui->multicastJoin->setChecked(m_settings.m_multicastJoin);

    ui->dataApplyButton->setEnabled(false);
    ui->dataApplyButton->setStyleSheet("QPushButton { background:rgb(79,79,79); }");

	ui->dcOffset->setChecked(m_settings.m_dcBlock);
	ui->iqImbalance->setChecked(m_settings.m_iqCorrection);

	blockApplySettings(false);
}

void RemoteInputGui::sendSettings()
{
    if(!m_updateTimer.isActive())
        m_updateTimer.start(100);
}

void RemoteInputGui::on_apiApplyButton_clicked(bool checked)
{
    (void) checked;
    m_settings.m_apiAddress = ui->apiAddress->text();

    bool ctlOk;
    int udpApiPort = ui->apiPort->text().toInt(&ctlOk);

    if((ctlOk) && (udpApiPort >= 1024) && (udpApiPort < 65535)) {
        m_settings.m_apiPort = udpApiPort;
    }

    sendSettings();

    QString infoURL = QString("http://%1:%2/sdrangel").arg(m_settings.m_apiAddress).arg(m_settings.m_apiPort);
    m_networkRequest.setUrl(QUrl(infoURL));
    m_networkManager->get(m_networkRequest);
}

void RemoteInputGui::on_dataApplyButton_clicked(bool checked)
{
    (void) checked;

    ui->dataApplyButton->setEnabled(false);
    ui->dataApplyButton->setStyleSheet("QPushButton { background:rgb(79,79,79); }");

    sendSettings();
}

void RemoteInputGui::on_apiAddress_returnPressed()
{
    m_settings.m_apiAddress = ui->apiAddress->text();

    QString infoURL = QString("http://%1:%2/sdrangel").arg(m_settings.m_apiAddress).arg(m_settings.m_apiPort);
    m_networkRequest.setUrl(QUrl(infoURL));
    m_networkManager->get(m_networkRequest);

    sendSettings();
}

void RemoteInputGui::on_dataAddress_returnPressed()
{
    m_settings.m_dataAddress = ui->dataAddress->text();
    ui->dataApplyButton->setEnabled(true);
    ui->dataApplyButton->setStyleSheet("QPushButton { background-color : green; }");
}

void RemoteInputGui::on_dataPort_returnPressed()
{
    bool ok;
    quint16 udpPort = ui->dataPort->text().toInt(&ok);

    if ((!ok) || (udpPort < 1024)) {
        udpPort = 9998;
    }

    m_settings.m_dataPort = udpPort;
    ui->dataPort->setText(tr("%1").arg(m_settings.m_dataPort));

    ui->dataApplyButton->setEnabled(true);
    ui->dataApplyButton->setStyleSheet("QPushButton { background-color : green; }");
}

void RemoteInputGui::on_multicastAddress_returnPressed()
{
    m_settings.m_multicastAddress = ui->multicastAddress->text();
    ui->dataApplyButton->setEnabled(true);
    ui->dataApplyButton->setStyleSheet("QPushButton { background-color : green; }");
}

void RemoteInputGui::on_multicastJoin_toggled(bool checked)
{
    m_settings.m_multicastJoin = checked;
    ui->dataApplyButton->setEnabled(true);
    ui->dataApplyButton->setStyleSheet("QPushButton { background-color : green; }");
}

void RemoteInputGui::on_apiPort_returnPressed()
{
    bool ctlOk;
    int udpApiPort = ui->apiPort->text().toInt(&ctlOk);

    if((!ctlOk) || (udpApiPort < 1024) || (udpApiPort > 65535))
    {
        return;
    }
    else
    {
        m_settings.m_apiPort = udpApiPort;

        QString infoURL = QString("http://%1:%2/sdrangel").arg(m_settings.m_apiAddress).arg(m_settings.m_apiPort);
        m_networkRequest.setUrl(QUrl(infoURL));
        m_networkManager->get(m_networkRequest);

        sendSettings();
    }
}

void RemoteInputGui::on_dcOffset_toggled(bool checked)
{
    m_settings.m_dcBlock = checked;
    sendSettings();
}

void RemoteInputGui::on_iqImbalance_toggled(bool checked)
{
    m_settings.m_iqCorrection = checked;
    sendSettings();
}

void RemoteInputGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        RemoteInput::MsgStartStop *message = RemoteInput::MsgStartStop::create(checked);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void RemoteInputGui::on_eventCountsReset_clicked(bool checked)
{
    (void) checked;
    m_countUnrecoverable = 0;
    m_countRecovered = 0;
    m_eventsTime.start();
    displayEventCounts();
    displayEventTimer();
}

void RemoteInputGui::displayEventCounts()
{
    QString nstr = QString("%1").arg(m_countUnrecoverable, 3, 10, QChar('0'));
    ui->eventUnrecText->setText(nstr);
    nstr = QString("%1").arg(m_countRecovered, 3, 10, QChar('0'));
    ui->eventRecText->setText(nstr);
}

void RemoteInputGui::displayEventTimer()
{
    int elapsedTimeMillis = m_eventsTime.elapsed();
    QTime recordLength(0, 0, 0, 0);
    recordLength = recordLength.addSecs(elapsedTimeMillis/1000);
    QString s_time = recordLength.toString("HH:mm:ss");
    ui->eventCountsTimeText->setText(s_time);
}

void RemoteInputGui::updateWithAcquisition()
{
}

void RemoteInputGui::updateWithStreamTime()
{
	bool updateEventCounts = false;
    QDateTime dt = QDateTime::fromMSecsSinceEpoch(m_startingTimeStampms);
    QString s_date = dt.toString("yyyy-MM-dd  HH:mm:ss.zzz");
	ui->absTimeText->setText(s_date);

	if (m_framesDecodingStatus == 2)
	{
		ui->allFramesDecoded->setStyleSheet("QToolButton { background-color : green; }");
	}
	else if (m_framesDecodingStatus == 1)
	{
        if (m_countRecovered < 999) m_countRecovered++;
        updateEventCounts = true;
        ui->allFramesDecoded->setStyleSheet("QToolButton { background:rgb(56,56,56); }");
	}
	else
	{
        if (m_countUnrecoverable < 999) m_countUnrecoverable++;
        updateEventCounts = true;
        ui->allFramesDecoded->setStyleSheet("QToolButton { background-color : red; }");
	}

	QString s = QString::number(m_bufferLengthInSecs, 'f', 1);
	ui->bufferLenSecsText->setText(tr("%1").arg(s));

    s = QString::number(m_bufferGauge, 'f', 0);
	ui->bufferRWBalanceText->setText(tr("%1").arg(s));

    ui->bufferGaugeNegative->setValue((m_bufferGauge < 0 ? -m_bufferGauge : 0));
    ui->bufferGaugePositive->setValue((m_bufferGauge < 0 ? 0 : m_bufferGauge));

    s = QString::number(m_minNbBlocks, 'f', 0);
    ui->minNbBlocksText->setText(tr("%1").arg(s));

    s = QString("%1").arg(m_maxNbRecovery, 2, 10, QChar('0'));
    ui->maxNbRecoveryText->setText(tr("%1").arg(s));

    s = QString::number(m_nbOriginalBlocks + m_nbFECBlocks, 'f', 0);
    QString s1 = QString("%1").arg(m_nbFECBlocks, 2, 10, QChar('0'));
    ui->nominalNbBlocksText->setText(tr("%1/%2").arg(s).arg(s1));

    ui->sampleBitsText->setText(tr("%1b").arg(m_sampleBits));

    if (updateEventCounts)
    {
        displayEventCounts();
    }

    displayEventTimer();
}

void RemoteInputGui::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "RemoteInputGui::updateHardware";
        RemoteInput::MsgConfigureRemoteInput* message =
                RemoteInput::MsgConfigureRemoteInput::create(m_settings, m_forceSettings);
        m_sampleSource->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_updateTimer.stop();
    }
}

void RemoteInputGui::updateStatus()
{
    if (m_sampleSource->isStreaming())
    {
        int state = m_deviceUISet->m_deviceAPI->state();

        if (m_lastEngineState != state)
        {
            switch(state)
            {
                case DeviceAPI::StNotStarted:
                    ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                    break;
                case DeviceAPI::StIdle:
                    ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                    break;
                case DeviceAPI::StRunning:
                    ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                    break;
                case DeviceAPI::StError:
                    ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                    QMessageBox::information(this, tr("Message"), m_deviceUISet->m_deviceAPI->errorMessage());
                    break;
                default:
                    break;
            }

            m_lastEngineState = state;
        }

        ui->startStop->setEnabled(true);
    }
    else
    {
        ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        ui->startStop->setChecked(false);
        ui->startStop->setEnabled(false);
    }
}

void RemoteInputGui::networkManagerFinished(QNetworkReply *reply)
{
    if (reply->error())
    {
        ui->apiAddressLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
        ui->statusText->setText(reply->errorString());
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
                ui->apiAddressLabel->setStyleSheet("QLabel { background-color : green; }");
                ui->statusText->setText(QString("API OK"));
                analyzeApiReply(doc.object());
            }
            else
            {
                ui->apiAddressLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
                QString errorMsg = QString("Reply JSON error: ") + error.errorString() + QString(" at offset ") + QString::number(error.offset);
                ui->statusText->setText(QString("JSON error. See log"));
                qInfo().noquote() << "RemoteInputGui::networkManagerFinished" << errorMsg;
            }
        }
        catch (const std::exception& ex)
        {
            ui->apiAddressLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
            QString errorMsg = QString("Error parsing request: ") + ex.what();
            ui->statusText->setText("Error parsing request. See log for details");
            qInfo().noquote() << "RemoteInputGui::networkManagerFinished" << errorMsg;
        }
    }

    reply->deleteLater();
}

void RemoteInputGui::analyzeApiReply(const QJsonObject& jsonObject)
{
    QString infoLine;

    if (jsonObject.contains("version")) {
        infoLine = jsonObject["version"].toString();
    }

    if (jsonObject.contains("qtVersion")) {
        infoLine += " Qt" + jsonObject["qtVersion"].toString();
    }

    if (jsonObject.contains("architecture")) {
        infoLine += " " + jsonObject["architecture"].toString();
    }

    if (jsonObject.contains("os")) {
        infoLine += " " + jsonObject["os"].toString();
    }

    if (jsonObject.contains("dspRxBits") && jsonObject.contains("dspTxBits")) {
        infoLine +=  QString(" %1/%2b").arg(jsonObject["dspRxBits"].toInt()).arg(jsonObject["dspTxBits"].toInt());
    }

    if (infoLine.size() > 0) {
        ui->infoText->setText(infoLine);
    }
}

void RemoteInputGui::openDeviceSettingsDialog(const QPoint& p)
{
    BasicDeviceSettingsDialog dialog(this);
    dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
    dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
    dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
    dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);

    dialog.move(p);
    dialog.exec();

    m_settings.m_useReverseAPI = dialog.useReverseAPI();
    m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
    m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
    m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();

    sendSettings();
}
