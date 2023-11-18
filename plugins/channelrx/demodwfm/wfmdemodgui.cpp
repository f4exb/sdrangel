///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany     //
// written by Christian Daniel                                                       //
// Copyright (C) 2014 John Greb <hexameron@spam.no>                                  //
// Copyright (C) 2015-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>        //
// Copyright (C) 2021-2023 Jon Beniston, M7RCE <jon@beniston.com>                    //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
#include "wfmdemodgui.h"

#include "device/deviceuiset.h"

#include <QDockWidget>
#include <QMainWindow>
#include <QDebug>

#include "ui_wfmdemodgui.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/crightclickenabler.h"
#include "gui/dialogpositioner.h"
#include "gui/audioselectdialog.h"
#include "maincore.h"

#include "wfmdemod.h"

WFMDemodGUI* WFMDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	WFMDemodGUI* gui = new WFMDemodGUI(pluginAPI, deviceUISet, rxChannel);
	return gui;
}

void WFMDemodGUI::destroy()
{
	delete this;
}

void WFMDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings();
}

QByteArray WFMDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool WFMDemodGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data)) {
        displaySettings();
        applySettings(true);
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

bool WFMDemodGUI::handleMessage(const Message& message)
{
    if (WFMDemod::MsgConfigureWFMDemod::match(message))
    {
        qDebug("WFMDemodGUI::handleMessage: WFMDemod::MsgConfigureWFMDemod");
        const WFMDemod::MsgConfigureWFMDemod& cfg = (WFMDemod::MsgConfigureWFMDemod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        const DSPSignalNotification& notif = (const DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        ui->deltaFrequency->setValueRange(false, 8, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();
        return true;
    }
    else
    {
        return false;
    }
}

void WFMDemodGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

void WFMDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void WFMDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void WFMDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void WFMDemodGUI::on_rfBW_changed(quint64 value)
{
    m_channelMarker.setBandwidth(value);
    m_settings.m_rfBandwidth = value;
    applySettings();
}

void WFMDemodGUI::on_afBW_valueChanged(int value)
{
    ui->afBWText->setText(QString("%1 kHz").arg(value));
    m_settings.m_afBandwidth = value * 1000.0;
	applySettings();
}

void WFMDemodGUI::on_volume_valueChanged(int value)
{
    ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_volume = value / 10.0;
	applySettings();
}

void WFMDemodGUI::on_squelch_valueChanged(int value)
{
	ui->squelchText->setText(QString("%1 dB").arg(value));
    m_settings.m_squelch = value;
	applySettings();
}

void WFMDemodGUI::on_audioMute_toggled(bool checked)
{
    m_settings.m_audioMute = checked;
    applySettings();
}

void WFMDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void WFMDemodGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
    {
        BasicChannelSettingsDialog dialog(&m_channelMarker, this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
        dialog.setReverseAPIChannelIndex(m_settings.m_reverseAPIChannelIndex);
        dialog.setDefaultTitle(m_displayedName);

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            dialog.setNumberOfStreams(m_wfmDemod->getNumberOfDeviceStreams());
            dialog.setStreamIndex(m_settings.m_streamIndex);
        }

        dialog.move(p);
        new DialogPositioner(&dialog, false);
        dialog.exec();

        m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
        m_settings.m_title = m_channelMarker.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settings.m_reverseAPIChannelIndex = dialog.getReverseAPIChannelIndex();

        setWindowTitle(m_settings.m_title);
        setTitle(m_channelMarker.getTitle());
        setTitleColor(m_settings.m_rgbColor);

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
            m_channelMarker.clearStreamIndexes();
            m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
            updateIndexLabel();
        }

        applySettings();
    }

    resetContextMenuType();
}

WFMDemodGUI::WFMDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
	ChannelGUI(parent),
	ui(new Ui::WFMDemodGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
	m_basicSettingsShown(false),
    m_squelchOpen(false),
    m_audioSampleRate(-1),
    m_recentAudioFifoError(false)
{
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodwfm/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

	m_wfmDemod = (WFMDemod*) rxChannel;
	m_wfmDemod->setMessageQueueToGUI(getInputMessageQueue());

	connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    CRightClickEnabler *audioMuteRightClickEnabler = new CRightClickEnabler(ui->audioMute);
    connect(audioMuteRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect(const QPoint &)));

	ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 8, -99999999, 99999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    ui->rfBW->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->rfBW->setValueRange(WFMDemodSettings::m_rfBWDigits, WFMDemodSettings::m_rfBWMin, WFMDemodSettings::m_rfBWMax);

    m_channelMarker.blockSignals(true);
	m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
	m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("WFM Demodulator");
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.blockSignals(false);
	m_channelMarker.setVisible(true); // activate signal on the last setting only

	setTitleColor(m_channelMarker.getColor());
    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

	m_deviceUISet->addChannelMarker(&m_channelMarker);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));

    displaySettings();
    makeUIConnections();
	applySettings(true);
    m_resizer.enableChildMouseTracking();
}

WFMDemodGUI::~WFMDemodGUI()
{
	delete ui;
}

void WFMDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void WFMDemodGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
        WFMDemod::MsgConfigureWFMDemod* msgConfig = WFMDemod::MsgConfigureWFMDemod::create( m_settings, force);
        m_wfmDemod->getInputMessageQueue()->push(msgConfig);
	}
}

void WFMDemodGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    ui->rfBW->setValue(m_settings.m_rfBandwidth);
    ui->afBW->setValue(m_settings.m_afBandwidth/1000.0);
    ui->afBWText->setText(QString("%1 kHz").arg(m_settings.m_afBandwidth/1000.0));
    ui->volume->setValue(m_settings.m_volume * 10.0);
    ui->volumeText->setText(QString("%1").arg(m_settings.m_volume, 0, 'f', 1));
    ui->squelch->setValue(m_settings.m_squelch);
    ui->squelchText->setText(QString("%1 dB").arg(m_settings.m_squelch));
    ui->audioMute->setChecked(m_settings.m_audioMute);

    updateIndexLabel();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void WFMDemodGUI::leaveEvent(QEvent* event)
{
	m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void WFMDemodGUI::enterEvent(EnterEventType* event)
{
	m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void WFMDemodGUI::audioSelect(const QPoint& p)
{
    qDebug("WFMDemodGUI::audioSelect");
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName);
    audioSelect.move(p);
    new DialogPositioner(&audioSelect, false);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        applySettings();
    }
}

void WFMDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_wfmDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPower->setText(QString::number(powDbAvg, 'f', 1));
    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    int audioSampleRate = m_wfmDemod->getAudioSampleRate();
    bool squelchOpen = m_wfmDemod->getSquelchOpen();
    int secsSinceAudioFifoError = m_wfmDemod->getAudioFifoErrorDateTime().secsTo(QDateTime::currentDateTime());
    bool recentAudioFifoError = (secsSinceAudioFifoError < 1) && squelchOpen;

    if ((audioSampleRate != m_audioSampleRate) || (squelchOpen != m_squelchOpen) || (recentAudioFifoError != m_recentAudioFifoError))
    {
        if (audioSampleRate < 0) {
            ui->audioMute->setStyleSheet("QToolButton { background-color : red; }");
        } else if (recentAudioFifoError) {
            ui->audioMute->setStyleSheet("QToolButton { background-color : rgb(120,120,0); }");
        } else if (squelchOpen) {
            ui->audioMute->setStyleSheet("QToolButton { background-color : green; }");
        } else {
            ui->audioMute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        }

        m_audioSampleRate = audioSampleRate;
        m_squelchOpen = squelchOpen;
        m_recentAudioFifoError = recentAudioFifoError;
    }
}

void WFMDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &WFMDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBW, &ValueDial::changed, this, &WFMDemodGUI::on_rfBW_changed);
    QObject::connect(ui->afBW, &QSlider::valueChanged, this, &WFMDemodGUI::on_afBW_valueChanged);
    QObject::connect(ui->volume, &QSlider::valueChanged, this, &WFMDemodGUI::on_volume_valueChanged);
    QObject::connect(ui->squelch, &QSlider::valueChanged, this, &WFMDemodGUI::on_squelch_valueChanged);
    QObject::connect(ui->audioMute, &QToolButton::toggled, this, &WFMDemodGUI::on_audioMute_toggled);
}

void WFMDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
