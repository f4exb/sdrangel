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

#include <QDebug>
#include <QTime>
#include <QDateTime>
#include <QString>
#include <QMessageBox>
#include <QResizeEvent>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include "ui_remoteoutputgui.h"
#include "plugin/pluginapi.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/basicdevicesettingsdialog.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"

#include "mainwindow.h"

#include "device/deviceapi.h"
#include "device/deviceuiset.h"
#include "remoteoutputgui.h"

#include "channel/remotedatablock.h"

RemoteOutputSinkGui::RemoteOutputSinkGui(DeviceUISet *deviceUISet, QWidget* parent) :
	DeviceGUI(parent),
	ui(new Ui::RemoteOutputGui),
	m_settings(),
	m_remoteOutput(0),
	m_deviceCenterFrequency(0),
	m_samplesCount(0),
	m_tickCount(0),
	m_nbSinceLastFlowCheck(0),
	m_lastEngineState(DeviceAPI::StNotStarted),
	m_doApplySettings(true),
	m_forceSettings(true),
    m_remoteAPIConnected(false)
{
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_countUnrecoverable = 0;
    m_countRecovered = 0;
    m_lastCountUnrecoverable = 0;
    m_lastCountRecovered = 0;
    m_lastSampleCount = 0;

    m_paletteGreenText.setColor(QPalette::WindowText, Qt::green);
    m_paletteRedText.setColor(QPalette::WindowText, Qt::red);
    m_paletteWhiteText.setColor(QPalette::WindowText, Qt::white);

    ui->setupUi(getContents());
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    getContents()->setStyleSheet("#RemoteOutputGui { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplesink/remoteoutput/readme.md";

	connect(&(m_deviceUISet->m_deviceAPI->getMasterTimer()), SIGNAL(timeout()), this, SLOT(tick()));
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);

	m_remoteOutput = (RemoteOutput*) m_deviceUISet->m_deviceAPI->getSampleSink();

	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);

	m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);

    m_time.start();
    displayEventCounts();
    displayEventTimer();

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));

    displaySettings();
    sendSettings();
    makeUIConnections();
}

RemoteOutputSinkGui::~RemoteOutputSinkGui()
{
    m_statusTimer.stop();
    m_updateTimer.stop();
	delete ui;
}

void RemoteOutputSinkGui::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void RemoteOutputSinkGui::destroy()
{
	delete this;
}

void RemoteOutputSinkGui::resetToDefaults()
{
    blockApplySettings(true);
	m_settings.resetToDefaults();
	displaySettings();
	blockApplySettings(false);
    m_forceSettings = true;
	sendSettings();
}

QByteArray RemoteOutputSinkGui::serialize() const
{
	return m_settings.serialize();
}

bool RemoteOutputSinkGui::deserialize(const QByteArray& data)
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

void RemoteOutputSinkGui::resizeEvent(QResizeEvent* size)
{
    adjustSize();
    size->accept();
}

bool RemoteOutputSinkGui::handleMessage(const Message& message)
{
    if (RemoteOutput::MsgConfigureRemoteOutput::match(message))
    {
        const RemoteOutput::MsgConfigureRemoteOutput& cfg = (RemoteOutput::MsgConfigureRemoteOutput&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (RemoteOutput::MsgStartStop::match(message))
    {
        RemoteOutput::MsgStartStop& notif = (RemoteOutput::MsgStartStop&) message;
        blockApplySettings(true);
        ui->startStop->setChecked(notif.getStartStop());
        blockApplySettings(false);
        return true;
    }
    else if (RemoteOutput::MsgReportRemoteData::match(message))
    {
        const RemoteOutput::MsgReportRemoteData& report = (const RemoteOutput::MsgReportRemoteData&) message;
        displayRemoteData(report.getData());
        return true;
    }
    else if (RemoteOutput::MsgReportRemoteFixedData::match(message))
    {
        const RemoteOutput::MsgReportRemoteFixedData& report = (const RemoteOutput::MsgReportRemoteFixedData&) message;
        displayRemoteFixedData(report.getData());
        return true;
    }
	else
	{
		return false;
	}
}

void RemoteOutputSinkGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            qDebug("RemoteOutputSinkGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRate();

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

void RemoteOutputSinkGui::updateSampleRate()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    ui->deviceRateText->setText(tr("%1k").arg((float)(m_sampleRate) / 1000));
}

void RemoteOutputSinkGui::displaySettings()
{
    blockApplySettings(true);
    ui->centerFrequency->setText(QString("%L1").arg(m_deviceCenterFrequency));
    ui->nbFECBlocks->setValue(m_settings.m_nbFECBlocks);
    ui->nbTxBytes->setCurrentIndex(log2(m_settings.m_nbTxBytes));

    QString s0 = QString::number(128 + m_settings.m_nbFECBlocks, 'f', 0);
    QString s1 = QString::number(m_settings.m_nbFECBlocks, 'f', 0);
    ui->nominalNbBlocksText->setText(tr("%1/%2").arg(s0).arg(s1));

    ui->deviceIndex->setText(tr("%1").arg(m_settings.m_deviceIndex));
    ui->channelIndex->setText(tr("%1").arg(m_settings.m_channelIndex));
    ui->apiAddress->setText(m_settings.m_apiAddress);
    ui->apiPort->setText(tr("%1").arg(m_settings.m_apiPort));
    ui->dataAddress->setText(m_settings.m_dataAddress);
    ui->dataPort->setText(tr("%1").arg(m_settings.m_dataPort));
    blockApplySettings(false);
}

void RemoteOutputSinkGui::sendSettings()
{
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start(100);
    }
}


void RemoteOutputSinkGui::updateHardware()
{
    qDebug() << "RemoteOutputSinkGui::updateHardware";
    RemoteOutput::MsgConfigureRemoteOutput* message = RemoteOutput::MsgConfigureRemoteOutput::create(m_settings, m_settingsKeys, m_forceSettings);
    m_remoteOutput->getInputMessageQueue()->push(message);
    m_forceSettings = false;
    m_settingsKeys.clear();
    m_updateTimer.stop();
}

void RemoteOutputSinkGui::updateStatus()
{
    int state = m_deviceUISet->m_deviceAPI->state();

    if(m_lastEngineState != state)
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
}

void RemoteOutputSinkGui::on_nbFECBlocks_valueChanged(int value)
{
    m_settings.m_nbFECBlocks = value;
    int nbOriginalBlocks = 128;
    int nbFECBlocks = value;
    QString s = QString::number(nbOriginalBlocks + nbFECBlocks, 'f', 0);
    QString s1 = QString::number(nbFECBlocks, 'f', 0);
    ui->nominalNbBlocksText->setText(tr("%1/%2").arg(s).arg(s1));
    m_settingsKeys.append("nbFECBlocks");
    sendSettings();
}

void RemoteOutputSinkGui::on_deviceIndex_returnPressed()
{
    bool dataOk;
    int deviceIndex = ui->deviceIndex->text().toInt(&dataOk);

    if ((!dataOk) || (deviceIndex < 0)) {
        return;
    } else {
        m_settings.m_deviceIndex = deviceIndex;
    }

    m_settingsKeys.append("deviceIndex");
    sendSettings();
}

void RemoteOutputSinkGui::on_channelIndex_returnPressed()
{
    bool dataOk;
    int channelIndex = ui->channelIndex->text().toInt(&dataOk);

    if ((!dataOk) || (channelIndex < 0)) {
        return;
    } else {
        m_settings.m_channelIndex = channelIndex;
    }

    m_settingsKeys.append("channelIndex");
    sendSettings();
}

void RemoteOutputSinkGui::on_nbTxBytes_currentIndexChanged(int index)
{
    m_settings.m_nbTxBytes = 1 << index;
    m_settingsKeys.append("nbTxBytes");
    sendSettings();
}

void RemoteOutputSinkGui::on_apiAddress_returnPressed()
{
    m_settings.m_apiAddress = ui->apiAddress->text();
    m_settingsKeys.append("apiAddress");
    sendSettings();

    RemoteOutput::MsgRequestFixedData *msg = RemoteOutput::MsgRequestFixedData::create();
    m_remoteOutput->getInputMessageQueue()->push(msg);
}

void RemoteOutputSinkGui::on_apiPort_returnPressed()
{
    bool dataOk;
    int apiPort = ui->apiPort->text().toInt(&dataOk);

    if((!dataOk) || (apiPort < 1024) || (apiPort > 65535)) {
        return;
    } else {
        m_settings.m_apiPort = apiPort;
    }

    m_settingsKeys.append("apiPort");
    sendSettings();

    RemoteOutput::MsgRequestFixedData *msg = RemoteOutput::MsgRequestFixedData::create();
    m_remoteOutput->getInputMessageQueue()->push(msg);
}

void RemoteOutputSinkGui::on_dataAddress_returnPressed()
{
    m_settings.m_dataAddress = ui->dataAddress->text();
    m_settingsKeys.append("dataAddress");
    sendSettings();
}

void RemoteOutputSinkGui::on_dataPort_returnPressed()
{
    bool dataOk;
    int dataPort = ui->dataPort->text().toInt(&dataOk);

    if((!dataOk) || (dataPort < 1024) || (dataPort > 65535)) {
        return;
    } else {
        m_settings.m_dataPort = dataPort;
    }

    m_settingsKeys.append("dataPort");
    sendSettings();
}

void RemoteOutputSinkGui::on_apiApplyButton_clicked(bool checked)
{
    (void) checked;
    m_settings.m_apiAddress = ui->apiAddress->text();
    m_settingsKeys.append("apiAddress");

    bool apiOk;
    int apiPort = ui->apiPort->text().toInt(&apiOk);

    if((apiOk) && (apiPort >= 1024) && (apiPort < 65535))
    {
        m_settings.m_apiPort = apiPort;
        m_settingsKeys.append("apiPort");
    }

    sendSettings();

    RemoteOutput::MsgRequestFixedData *msg = RemoteOutput::MsgRequestFixedData::create();
    m_remoteOutput->getInputMessageQueue()->push(msg);
}

void RemoteOutputSinkGui::on_dataApplyButton_clicked(bool checked)
{
    (void) checked;
    m_settings.m_dataAddress = ui->dataAddress->text();
    m_settingsKeys.append("dataAddress");

    bool dataOk;
    int udpDataPort = ui->dataPort->text().toInt(&dataOk);

    if((dataOk) && (udpDataPort >= 1024) && (udpDataPort < 65535))
    {
        m_settings.m_dataPort = udpDataPort;
        m_settingsKeys.append("dataPort");
    }

    sendSettings();
}

void RemoteOutputSinkGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        RemoteOutput::MsgStartStop *message = RemoteOutput::MsgStartStop::create(checked);
        m_remoteOutput->getInputMessageQueue()->push(message);
    }
}

void RemoteOutputSinkGui::on_eventCountsReset_clicked(bool checked)
{
    (void) checked;
    m_countUnrecoverable = 0;
    m_countRecovered = 0;
    m_time.start();
    displayEventCounts();
    displayEventTimer();
}

void RemoteOutputSinkGui::displayEventCounts()
{
    QString nstr = QString("%1").arg(m_countUnrecoverable, 3, 10, QChar('0'));
    ui->eventUnrecText->setText(nstr);
    nstr = QString("%1").arg(m_countRecovered, 3, 10, QChar('0'));
    ui->eventRecText->setText(nstr);
}

void RemoteOutputSinkGui::displayEventStatus(int recoverableCount, int unrecoverableCount)
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

void RemoteOutputSinkGui::displayEventTimer()
{
    int elapsedTimeMillis = m_time.elapsed();
    QTime recordLength(0, 0, 0, 0);
    recordLength = recordLength.addSecs(elapsedTimeMillis/1000);
    QString s_time = recordLength.toString("HH:mm:ss");
    ui->eventCountsTimeText->setText(s_time);
}

void RemoteOutputSinkGui::tick()
{
    if (++m_tickCount == 20)
    {
        if (m_remoteAPIConnected) {
            ui->apiAddressLabel->setStyleSheet("QLabel { background-color: green; }");
        } else {
            ui->apiAddressLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
        }

        m_remoteAPIConnected = false;
        m_tickCount = 0;
    }
}

void RemoteOutputSinkGui::displayRemoteData(const RemoteOutput::MsgReportRemoteData::RemoteData& remoteData)
{
    m_deviceCenterFrequency = remoteData.m_centerFrequency;
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->centerFrequency->setText(QString("%L1").arg(m_deviceCenterFrequency));
    ui->remoteRateText->setText(tr("%1k").arg((float)(remoteData.m_sampleRate) / 1000));
    QString queueLengthText = QString("%1/%2").arg(remoteData.m_queueLength).arg(remoteData.m_queueSize);
    ui->queueLengthText->setText(queueLengthText);
    int queueLengthPercent = (remoteData.m_queueLength*100)/remoteData.m_queueSize;
    ui->queueLengthGauge->setValue(queueLengthPercent);
    int recoverableCountDelta = remoteData.m_recoverableCount - m_lastCountRecovered;
    int unrecoverableCountDelta = remoteData.m_unrecoverableCount - m_lastCountUnrecoverable;
    displayEventStatus(recoverableCountDelta, unrecoverableCountDelta);
    m_countRecovered += recoverableCountDelta;
    m_countUnrecoverable += unrecoverableCountDelta;
    displayEventCounts();
    displayEventTimer();
    m_remoteAPIConnected = true;

    uint32_t sampleCountDelta;

    if (remoteData.m_sampleCount < m_lastSampleCount) {
        sampleCountDelta = (0xFFFFFFFFU - m_lastSampleCount) + remoteData.m_sampleCount + 1;
    } else {
        sampleCountDelta = remoteData.m_sampleCount - m_lastSampleCount;
    }

    if (sampleCountDelta == 0)
    {
        ui->allFramesDecoded->setStyleSheet("QToolButton { background-color : blue; }");
    }

    double remoteStreamRate = sampleCountDelta*1e6 / (double) (remoteData.m_timestampUs - m_lastTimestampUs);

    if (remoteStreamRate != 0) {
        ui->remoteStreamRateText->setText(QString("%1").arg(remoteStreamRate, 0, 'f', 0));
    }

    m_lastCountRecovered = remoteData.m_recoverableCount;
    m_lastCountUnrecoverable = remoteData.m_unrecoverableCount;
    m_lastSampleCount = remoteData.m_sampleCount;
    m_lastTimestampUs = remoteData.m_timestampUs;
}

void RemoteOutputSinkGui::displayRemoteFixedData(const RemoteOutput::MsgReportRemoteFixedData::RemoteData& remoteData)
{
    QString infoLine = "v" + remoteData.m_version;
    infoLine += " Qt" + remoteData.m_qtVersion;
    infoLine += " " + remoteData.m_architecture;
    infoLine += " " + remoteData.m_os;
    infoLine +=  QString(" %1/%2b").arg(remoteData.m_rxBits).arg(remoteData.m_txBits);
    m_remoteAPIConnected = true;

    if (infoLine.size() > 0) {
        ui->infoText->setText(infoLine);
    }
}

void RemoteOutputSinkGui::openDeviceSettingsDialog(const QPoint& p)
{
    if (m_contextMenuType == ContextMenuDeviceSettings)
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
        m_settingsKeys.append("useReverseAPI");
        m_settingsKeys.append("reverseAPIAddress");
        m_settingsKeys.append("reverseAPIPort");
        m_settingsKeys.append("reverseAPIDeviceIndex");

        sendSettings();
    }

    resetContextMenuType();
}

void RemoteOutputSinkGui::makeUIConnections()
{
    QObject::connect(ui->nbFECBlocks, &QDial::valueChanged, this, &RemoteOutputSinkGui::on_nbFECBlocks_valueChanged);
    QObject::connect(ui->deviceIndex, &QLineEdit::returnPressed, this, &RemoteOutputSinkGui::on_deviceIndex_returnPressed);
    QObject::connect(ui->channelIndex, &QLineEdit::returnPressed, this, &RemoteOutputSinkGui::on_channelIndex_returnPressed);
    QObject::connect(ui->nbTxBytes, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RemoteOutputSinkGui::on_nbTxBytes_currentIndexChanged);
    QObject::connect(ui->apiAddress, &QLineEdit::returnPressed, this, &RemoteOutputSinkGui::on_apiAddress_returnPressed);
    QObject::connect(ui->apiPort, &QLineEdit::returnPressed, this, &RemoteOutputSinkGui::on_apiPort_returnPressed);
    QObject::connect(ui->dataAddress, &QLineEdit::returnPressed, this, &RemoteOutputSinkGui::on_dataAddress_returnPressed);
    QObject::connect(ui->dataPort, &QLineEdit::returnPressed, this, &RemoteOutputSinkGui::on_dataPort_returnPressed);
    QObject::connect(ui->apiApplyButton, &QPushButton::clicked, this, &RemoteOutputSinkGui::on_apiApplyButton_clicked);
    QObject::connect(ui->dataApplyButton, &QPushButton::clicked, this, &RemoteOutputSinkGui::on_dataApplyButton_clicked);
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &RemoteOutputSinkGui::on_startStop_toggled);
    QObject::connect(ui->eventCountsReset, &QPushButton::clicked, this, &RemoteOutputSinkGui::on_eventCountsReset_clicked);
}
