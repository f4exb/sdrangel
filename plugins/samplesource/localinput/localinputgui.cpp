///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include "ui_localinputgui.h"
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
#include "localinputgui.h"

LocalInputGui::LocalInputGui(DeviceUISet *deviceUISet, QWidget *parent) : DeviceGUI(parent),
                                                                          ui(new Ui::LocalInputGui),
                                                                          m_deviceUISet(deviceUISet),
                                                                          m_settings(),
                                                                          m_sampleSource(0),
                                                                          m_acquisition(false),
                                                                          m_streamSampleRate(0),
                                                                          m_streamCenterFrequency(0),
                                                                          m_lastEngineState(DeviceAPI::StNotStarted),
                                                                          m_startingDateTime(QDateTime::currentDateTime()),
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

    m_sampleSource = (LocalInput*) m_deviceUISet->m_deviceAPI->getSampleSource();

	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
	m_sampleSource->setMessageQueueToGUI(&m_inputMessageQueue);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));

    m_forceSettings = true;
    sendSettings();
}

LocalInputGui::~LocalInputGui()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
	delete ui;
}

void LocalInputGui::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void LocalInputGui::destroy()
{
	delete this;
}

void LocalInputGui::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    m_forceSettings = true;
    sendSettings();
}

QByteArray LocalInputGui::serialize() const
{
    return m_settings.serialize();
}

bool LocalInputGui::deserialize(const QByteArray& data)
{
    qDebug("LocalInputGui::deserialize");

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

bool LocalInputGui::handleMessage(const Message& message)
{
    if (LocalInput::MsgConfigureLocalInput::match(message))
    {
        const LocalInput::MsgConfigureLocalInput& cfg = (LocalInput::MsgConfigureLocalInput&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
	else if (LocalInput::MsgStartStop::match(message))
    {
	    LocalInput::MsgStartStop& notif = (LocalInput::MsgStartStop&) message;
        blockApplySettings(true);
        ui->startStop->setChecked(notif.getStartStop());
        blockApplySettings(false);

        return true;
    }
    else if (LocalInput::MsgReportSampleRateAndFrequency::match(message))
    {
        LocalInput::MsgReportSampleRateAndFrequency& notif = (LocalInput::MsgReportSampleRateAndFrequency&) message;
        m_streamSampleRate = notif.getSampleRate();
        m_streamCenterFrequency = notif.getCenterFrequency();
        updateSampleRateAndFrequency();

        return true;
    }
	else
	{
		return false;
	}
}

void LocalInputGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        //qDebug("LocalInputGui::handleInputMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;

            if (notif->getSampleRate() != m_streamSampleRate) {
                m_streamSampleRate = notif->getSampleRate();
            }

            m_streamCenterFrequency = notif->getCenterFrequency();

            qDebug("LocalInputGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());

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

void LocalInputGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_streamSampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_streamCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_streamSampleRate / 1000));
    blockApplySettings(true);
    ui->centerFrequency->setValue(m_streamCenterFrequency / 1000);
    ui->centerFrequencyHz->setValue(m_streamCenterFrequency % 1000);
    blockApplySettings(false);
}

void LocalInputGui::displaySettings()
{
    blockApplySettings(true);

    ui->centerFrequency->setValue(m_streamCenterFrequency / 1000);
    ui->centerFrequencyHz->setValue(m_streamCenterFrequency % 1000);
    ui->deviceRateText->setText(tr("%1k").arg(m_streamSampleRate / 1000.0));

	ui->dcOffset->setChecked(m_settings.m_dcBlock);
	ui->iqImbalance->setChecked(m_settings.m_iqCorrection);

	blockApplySettings(false);
}

void LocalInputGui::sendSettings()
{
    if(!m_updateTimer.isActive())
        m_updateTimer.start(100);
}

void LocalInputGui::on_dcOffset_toggled(bool checked)
{
    m_settings.m_dcBlock = checked;
    sendSettings();
}

void LocalInputGui::on_iqImbalance_toggled(bool checked)
{
    m_settings.m_iqCorrection = checked;
    sendSettings();
}

void LocalInputGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        LocalInput::MsgStartStop *message = LocalInput::MsgStartStop::create(checked);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void LocalInputGui::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "LocalInputGui::updateHardware";
        LocalInput::MsgConfigureLocalInput* message =
                LocalInput::MsgConfigureLocalInput::create(m_settings, m_forceSettings);
        m_sampleSource->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_updateTimer.stop();
    }
}

void LocalInputGui::updateStatus()
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

void LocalInputGui::openDeviceSettingsDialog(const QPoint& p)
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
