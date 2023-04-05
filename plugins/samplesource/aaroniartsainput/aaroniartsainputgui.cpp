///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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
#include <QFileDialog>

#include "ui_aaroniartsainputgui.h"
#include "plugin/pluginapi.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/basicdevicesettingsdialog.h"
#include "gui/dialogpositioner.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "util/db.h"

#include "mainwindow.h"

#include "aaroniartsainputgui.h"
#include "device/deviceapi.h"
#include "device/deviceuiset.h"

AaroniaRTSAInputGui::AaroniaRTSAInputGui(DeviceUISet *deviceUISet, QWidget* parent) :
    DeviceGUI(parent),
    ui(new Ui::AaroniaRTSAInputGui),
    m_settings(),
    m_doApplySettings(true),
    m_forceSettings(true),
    m_sampleSource(0),
    m_tickCount(0),
    m_lastEngineState(DeviceAPI::StNotStarted)
{
    qDebug("AaroniaRTSAInputGui::AaroniaRTSAInputGui");
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_sampleSource = m_deviceUISet->m_deviceAPI->getSampleSource();

	m_statusTooltips.push_back("Idle");          // 0
	m_statusTooltips.push_back("Unstable");      // 1
	m_statusTooltips.push_back("Connected");     // 2
	m_statusTooltips.push_back("Error");         // 3
	m_statusTooltips.push_back("Disconnected");  // 4

	m_statusColors.push_back("gray");               // Idle
	m_statusColors.push_back("rgb(232, 212, 35)");  // Unstable (yellow)
	m_statusColors.push_back("rgb(35, 138, 35)");   // Connected (green)
	m_statusColors.push_back("rgb(232, 85, 85)");   // Error (red)
	m_statusColors.push_back("rgb(232, 85, 232)");  // Disconnected (magenta)

    ui->setupUi(getContents());
    sizeToContents();
    getContents()->setStyleSheet("#AaroniaRTSAInputGui { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplesource/aaroniartsainput/readme.md";
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->centerFrequency->setValueRange(9, 0, 999999999);

    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(8, 2000U, 20000000U);

    displaySettings();
    makeUIConnections();

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleSource->setMessageQueueToGUI(&m_inputMessageQueue);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));
}

AaroniaRTSAInputGui::~AaroniaRTSAInputGui()
{
    delete ui;
}

void AaroniaRTSAInputGui::destroy()
{
    delete this;
}

void AaroniaRTSAInputGui::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    m_forceSettings = true;
    sendSettings();
}

QByteArray AaroniaRTSAInputGui::serialize() const
{
    return m_settings.serialize();
}

bool AaroniaRTSAInputGui::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data)) {
        displaySettings();
        m_forceSettings = true;
        sendSettings();
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

void AaroniaRTSAInputGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
		AaroniaRTSAInput::MsgStartStop *message = AaroniaRTSAInput::MsgStartStop::create(checked);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void AaroniaRTSAInputGui::on_centerFrequency_changed(quint64 value)
{
    m_settings.m_centerFrequency = value * 1000;
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}

void AaroniaRTSAInputGui::on_sampleRate_changed(quint64 value)
{
    m_settings.m_sampleRate = value;
    m_settingsKeys.append("sampleRate");
    sendSettings();
}

void AaroniaRTSAInputGui::on_serverAddress_returnPressed()
{
	on_serverAddressApplyButton_clicked();
}

void AaroniaRTSAInputGui::on_serverAddressApplyButton_clicked()
{
	QString serverAddress = ui->serverAddress->text();
    QUrl url(serverAddress);

    if (QStringList{"ws", "wss", "http", "https"}.contains(url.scheme())) {
        m_settings.m_serverAddress = QString("%1:%2").arg(url.host()).arg(url.port());
    } else {
        m_settings.m_serverAddress = serverAddress;
    }

    m_settingsKeys.append("serverAddress");
	sendSettings();
}

void AaroniaRTSAInputGui::displaySettings()
{
    blockApplySettings(true);

    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
	ui->serverAddress->setText(m_settings.m_serverAddress);

    blockApplySettings(false);
}

void AaroniaRTSAInputGui::sendSettings()
{
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start(100);
    }
}

void AaroniaRTSAInputGui::updateHardware()
{
    if (m_doApplySettings)
    {
		AaroniaRTSAInput::MsgConfigureAaroniaRTSA* message = AaroniaRTSAInput::MsgConfigureAaroniaRTSA::create(m_settings, m_settingsKeys, m_forceSettings);
        m_sampleSource->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_settingsKeys.clear();
        m_updateTimer.stop();
    }
}

void AaroniaRTSAInputGui::updateStatus()
{
    int state = m_deviceUISet->m_deviceAPI->state();

    if (m_lastEngineState != state)
    {
        switch (state)
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

bool AaroniaRTSAInputGui::handleMessage(const Message& message)
{
    if (AaroniaRTSAInput::MsgConfigureAaroniaRTSA::match(message))
    {
        qDebug("AaroniaRTSAInputGui::handleMessage: MsgConfigureAaroniaRTSA");
        const AaroniaRTSAInput::MsgConfigureAaroniaRTSA& cfg = (AaroniaRTSAInput::MsgConfigureAaroniaRTSA&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        displaySettings();
        return true;
    }
    else if (AaroniaRTSAInput::MsgStartStop::match(message))
    {
        qDebug("AaroniaRTSAInputGui::handleMessage: MsgStartStop");
		AaroniaRTSAInput::MsgStartStop& notif = (AaroniaRTSAInput::MsgStartStop&) message;
        blockApplySettings(true);
        ui->startStop->setChecked(notif.getStartStop());
        blockApplySettings(false);

        return true;
    }
	else if (AaroniaRTSAInput::MsgSetStatus::match(message))
	{
		qDebug("AaroniaRTSAInputGui::handleMessage: MsgSetStatus");
		AaroniaRTSAInput::MsgSetStatus& notif = (AaroniaRTSAInput::MsgSetStatus&) message;
		int status = notif.getStatus();
		ui->statusIndicator->setToolTip(m_statusTooltips[status]);
		ui->statusIndicator->setStyleSheet("QLabel { background-color: " +
			m_statusColors[status] + "; border-radius: 7px; }");
		return true;
	}
    else
    {
        return false;
    }
}

void AaroniaRTSAInputGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_deviceSampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("AaroniaRTSAInputGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu",
                    notif->getSampleRate(),
                    notif->getCenterFrequency());
            updateSampleRateAndFrequency();

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

void AaroniaRTSAInputGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_deviceSampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
	// ui->deviceRateText->setText(tr("%1M").arg((float)m_deviceSampleRate / 1000 / 1000));
    ui->deviceRateText->setText(tr("%1k").arg(QString::number(m_deviceSampleRate / 1000.0f, 'g', 5)));
    blockApplySettings(true);
    ui->centerFrequency->setValue(m_deviceCenterFrequency / 1000);
    ui->sampleRate->setValue(m_deviceSampleRate);
    blockApplySettings(false);
}

void AaroniaRTSAInputGui::openDeviceSettingsDialog(const QPoint& p)
{
    if (m_contextMenuType == ContextMenuDeviceSettings)
    {
        BasicDeviceSettingsDialog dialog(this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);

        dialog.move(p);
        new DialogPositioner(&dialog, false);
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

void AaroniaRTSAInputGui::makeUIConnections()
{
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &AaroniaRTSAInputGui::on_startStop_toggled);
    QObject::connect(ui->centerFrequency, &ValueDial::changed, this, &AaroniaRTSAInputGui::on_centerFrequency_changed);
    QObject::connect(ui->sampleRate, &ValueDial::changed, this, &AaroniaRTSAInputGui::on_sampleRate_changed);
    QObject::connect(ui->serverAddress, &QLineEdit::returnPressed, this, &AaroniaRTSAInputGui::on_serverAddress_returnPressed);
    QObject::connect(ui->serverAddressApplyButton, &QPushButton::clicked, this, &AaroniaRTSAInputGui::on_serverAddressApplyButton_clicked);
}
