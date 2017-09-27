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

#include <QDockWidget>
#include <QMainWindow>

#include "amdemodgui.h"

#include "device/devicesourceapi.h"
#include "dsp/downchannelizer.h"

#include "dsp/threadedbasebandsamplesink.h"
#include "ui_amdemodgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "dsp/dspengine.h"
#include "mainwindow.h"

#include "amdemod.h"

const QString AMDemodGUI::m_channelID = "de.maintech.sdrangelove.channel.am";

AMDemodGUI* AMDemodGUI::create(PluginAPI* pluginAPI, DeviceSourceAPI *deviceAPI)
{
	AMDemodGUI* gui = new AMDemodGUI(pluginAPI, deviceAPI);
	return gui;
}

void AMDemodGUI::destroy()
{
	delete this; // TODO: is this really useful?
}

void AMDemodGUI::setName(const QString& name)
{
	setObjectName(name);
}

QString AMDemodGUI::getName() const
{
	return objectName();
}

qint64 AMDemodGUI::getCenterFrequency() const {
	return m_channelMarker.getCenterFrequency();
}

void AMDemodGUI::setCenterFrequency(qint64 centerFrequency)
{
	m_channelMarker.setCenterFrequency(centerFrequency);
	applySettings();
}

void AMDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
	applySettings(true);
}

QByteArray AMDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool AMDemodGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data)) {
        updateChannelMarker();
        displaySettings();
        applySettings(true);
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

bool AMDemodGUI::handleMessage(const Message& message __attribute__((unused)))
{
	return false;
}

void AMDemodGUI::channelMarkerChanged()
{
    this->setWindowTitle(m_channelMarker.getTitle());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    m_settings.m_udpAddress = m_channelMarker.getUDPAddress(),
    m_settings.m_udpPort =  m_channelMarker.getUDPSendPort(),
    m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
    displayUDPAddress();
    m_settings.m_channelMarkerBytes = m_channelMarker.serialize();
	applySettings();
}

void AMDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void AMDemodGUI::on_bandpassEnable_toggled(bool checked)
{
    m_settings.m_bandpassEnable = checked;
    applySettings();
}

void AMDemodGUI::on_rfBW_valueChanged(int value)
{
	ui->rfBWText->setText(QString("%1 kHz").arg(value / 10.0, 0, 'f', 1));
	m_channelMarker.setBandwidth(value * 100);
	m_settings.m_rfBandwidth = value * 100;
	applySettings();
}

void AMDemodGUI::on_volume_valueChanged(int value)
{
	ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
	m_settings.m_volume = value / 10.0;
	applySettings();
}

void AMDemodGUI::on_squelch_valueChanged(int value)
{
	ui->squelchText->setText(QString("%1 dB").arg(value));
	m_settings.m_squelch = value;
	applySettings();
}

void AMDemodGUI::on_audioMute_toggled(bool checked)
{
    m_settings.m_audioMute = checked;
	applySettings();
}

void AMDemodGUI::on_copyAudioToUDP_toggled(bool checked)
{
    m_settings.m_copyAudioToUDP = checked;
    applySettings();
}

void AMDemodGUI::onWidgetRolled(QWidget* widget __attribute__((unused)), bool rollDown __attribute__((unused)))
{
	/*
	if((widget == ui->spectrumContainer) && (m_nfmDemod != NULL))
		m_nfmDemod->setSpectrum(m_threadedSampleSink->getMessageQueue(), rollDown);
	*/
}

void AMDemodGUI::onMenuDialogCalled(const QPoint &p)
{
    BasicChannelSettingsDialog dialog(&m_channelMarker, this);
    dialog.move(p);
    dialog.exec();
}

AMDemodGUI::AMDemodGUI(PluginAPI* pluginAPI, DeviceSourceAPI *deviceAPI, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::AMDemodGUI),
	m_pluginAPI(pluginAPI),
	m_deviceAPI(deviceAPI),
	m_channelMarker(this),
	m_doApplySettings(true),
	m_squelchOpen(false),
	m_tickCount(0)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

	m_amDemod = new AMDemod(m_deviceAPI);

	connect(&m_pluginAPI->getMainWindow()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
	ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

	m_channelMarker.setColor(Qt::yellow);
	m_channelMarker.setBandwidth(5000);
	m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("AM Demodulator");
    m_channelMarker.setUDPAddress("127.0.0.1");
    m_channelMarker.setUDPSendPort(9999);
    m_channelMarker.setVisible(true);
    setTitleColor(m_channelMarker.getColor());

	connect(&m_channelMarker, SIGNAL(changed()), this, SLOT(channelMarkerChanged()));

	m_deviceAPI->registerChannelInstance(m_channelID, this);
    m_deviceAPI->addChannelMarker(&m_channelMarker);
    m_deviceAPI->addRollupWidget(this);

    displaySettings();
	applySettings(true);
}

AMDemodGUI::~AMDemodGUI()
{
    m_deviceAPI->removeChannelInstance(this);
	delete m_amDemod;
	delete ui;
}

void AMDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void AMDemodGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
		setTitleColor(m_channelMarker.getColor());

		AMDemod::MsgConfigureChannelizer* channelConfigMsg = AMDemod::MsgConfigureChannelizer::create(
		        48000, m_channelMarker.getCenterFrequency());
		m_amDemod->getInputMessageQueue()->push(channelConfigMsg);

		ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

	    AMDemod::MsgConfigureAMDemod* message = AMDemod::MsgConfigureAMDemod::create( m_settings, force);
	    m_amDemod->getInputMessageQueue()->push(message);
	}
}

void AMDemodGUI::displaySettings()
{
    blockApplySettings(true);

    int displayValue = m_settings.m_rfBandwidth/100.0;
    ui->rfBW->setValue(displayValue);
    ui->rfBWText->setText(QString("%1 kHz").arg(displayValue / 10.0, 0, 'f', 1));
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);

    ui->volume->setValue(m_settings.m_volume * 10.0);
    ui->volumeText->setText(QString("%1").arg(m_settings.m_volume, 0, 'f', 1));

    ui->squelch->setValue(m_settings.m_squelch);
    ui->squelchText->setText(QString("%1 dB").arg(m_settings.m_squelch));

    ui->audioMute->setChecked(m_settings.m_audioMute);
    ui->bandpassEnable->setChecked(m_settings.m_bandpassEnable);
    ui->copyAudioToUDP->setChecked(m_settings.m_copyAudioToUDP);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setUDPAddress(m_settings.m_udpAddress);
    m_channelMarker.setUDPSendPort(m_settings.m_udpPort);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    setTitleColor(m_settings.m_rgbColor);
    m_channelMarker.blockSignals(false);

    blockApplySettings(false);
}

void AMDemodGUI::displayUDPAddress()
{
    ui->copyAudioToUDP->setToolTip(QString("Copy audio output to UDP %1:%2").arg(m_channelMarker.getUDPAddress()).arg(m_channelMarker.getUDPSendPort()));
}

void AMDemodGUI::updateChannelMarker()
{
    m_channelMarker.blockSignals(true);

    m_channelMarker.deserialize(m_settings.m_channelMarkerBytes);
    this->setWindowTitle(m_channelMarker.getTitle());

    m_channelMarker.blockSignals(false);
}

void AMDemodGUI::leaveEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(false);
	blockApplySettings(false);
}

void AMDemodGUI::enterEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(true);
	blockApplySettings(false);
}

void AMDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_amDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(QString::number(powDbAvg, 'f', 1));
    }

	bool squelchOpen = m_amDemod->getSquelchOpen();

	if (squelchOpen != m_squelchOpen)
	{
		m_squelchOpen = squelchOpen;

		if (m_squelchOpen) {
			ui->audioMute->setStyleSheet("QToolButton { background-color : green; }");
		} else {
			ui->audioMute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
		}
	}

	m_tickCount++;
}

