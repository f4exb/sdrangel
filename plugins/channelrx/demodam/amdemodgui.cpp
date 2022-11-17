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

#include <QDockWidget>
#include <QMainWindow>
#include <QDebug>

#include "amdemodgui.h"
#include "amdemodssbdialog.h"

#include "device/deviceuiset.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/crightclickenabler.h"
#include "gui/audioselectdialog.h"
#include "maincore.h"

#include "ui_amdemodgui.h"
#include "amdemod.h"

AMDemodGUI* AMDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	AMDemodGUI* gui = new AMDemodGUI(pluginAPI, deviceUISet, rxChannel);
	return gui;
}

void AMDemodGUI::destroy()
{
	delete this;
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
        displaySettings();
        applySettings(true);
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

bool AMDemodGUI::handleMessage(const Message& message)
{
    if (AMDemod::MsgConfigureAMDemod::match(message))
    {
        qDebug("AMDemodGUI::handleMessage: AMDemod::MsgConfigureAMDemod");
        const AMDemod::MsgConfigureAMDemod& cfg = (AMDemod::MsgConfigureAMDemod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        ui->deltaFrequency->setValueRange(false, 7, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();

        return true;
    }

	return false;
}

void AMDemodGUI::handleInputMessages()
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

void AMDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
	applySettings();
}

void AMDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void AMDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void AMDemodGUI::on_pll_toggled(bool checked)
{
    if (!checked)
    {
        ui->pll->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        ui->pll->setToolTip(tr("PLL for synchronous AM"));
    }

    m_settings.m_pll = checked;
    applySettings();
}

void AMDemodGUI::on_ssb_toggled(bool checked)
{
    m_settings.m_syncAMOperation = checked ? m_samUSB ? AMDemodSettings::SyncAMUSB : AMDemodSettings::SyncAMLSB : AMDemodSettings::SyncAMDSB;
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

void AMDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;
	/*
	if((widget == ui->spectrumContainer) && (m_nfmDemod != NULL))
		m_nfmDemod->setSpectrum(m_threadedSampleSink->getMessageQueue(), rollDown);
	*/

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void AMDemodGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_amDemod->getNumberOfDeviceStreams());
            dialog.setStreamIndex(m_settings.m_streamIndex);
        }

        dialog.move(p);
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

AMDemodGUI::AMDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
	ChannelGUI(parent),
	ui(new Ui::AMDemodGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
	m_doApplySettings(true),
	m_squelchOpen(false),
    m_audioSampleRate(-1),
	m_samUSB(true),
	m_tickCount(0)
{
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodam/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

	m_amDemod = reinterpret_cast<AMDemod*>(rxChannel);
	m_amDemod->setMessageQueueToGUI(getInputMessageQueue());

	connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

	CRightClickEnabler *audioMuteRightClickEnabler = new CRightClickEnabler(ui->audioMute);
	connect(audioMuteRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect()));

	CRightClickEnabler *samSidebandRightClickEnabler = new CRightClickEnabler(ui->ssb);
    connect(samSidebandRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(samSSBSelect()));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
	ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

	m_channelMarker.blockSignals(true);
	m_channelMarker.setColor(Qt::yellow);
	m_channelMarker.setBandwidth(5000);
	m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("AM Demodulator");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    setTitleColor(m_channelMarker.getColor());
    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

	m_deviceUISet->addChannelMarker(&m_channelMarker);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    m_iconDSBUSB.addPixmap(QPixmap("://dsb.png"), QIcon::Normal, QIcon::Off);
    m_iconDSBUSB.addPixmap(QPixmap("://usb.png"), QIcon::Normal, QIcon::On);
    m_iconDSBLSB.addPixmap(QPixmap("://dsb.png"), QIcon::Normal, QIcon::Off);
    m_iconDSBLSB.addPixmap(QPixmap("://lsb.png"), QIcon::Normal, QIcon::On);

	displaySettings();
    makeUIConnections();
	applySettings(true);
}

AMDemodGUI::~AMDemodGUI()
{
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
	    AMDemod::MsgConfigureAMDemod* message = AMDemod::MsgConfigureAMDemod::create( m_settings, force);
	    m_amDemod->getInputMessageQueue()->push(message);
	}
}

void AMDemodGUI::displaySettings()
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

    int displayValue = m_settings.m_rfBandwidth/100.0;
    ui->rfBW->setValue(displayValue);
    ui->rfBWText->setText(QString("%1 kHz").arg(displayValue / 10.0, 0, 'f', 1));

    ui->volume->setValue(m_settings.m_volume * 10.0);
    ui->volumeText->setText(QString("%1").arg(m_settings.m_volume, 0, 'f', 1));

    ui->squelch->setValue(m_settings.m_squelch);
    ui->squelchText->setText(QString("%1 dB").arg(m_settings.m_squelch));

    ui->audioMute->setChecked(m_settings.m_audioMute);
    ui->bandpassEnable->setChecked(m_settings.m_bandpassEnable);
    ui->pll->setChecked(m_settings.m_pll);

    qDebug() << "AMDemodGUI::displaySettings:"
            << " m_pll: " << m_settings.m_pll
            << " m_syncAMOperation: " << m_settings.m_syncAMOperation;

    if (m_settings.m_pll)
    {
        if (m_settings.m_syncAMOperation == AMDemodSettings::SyncAMLSB)
        {
            m_samUSB = false;
            ui->ssb->setChecked(true);
            ui->ssb->setIcon(m_iconDSBLSB);
        }
        else if (m_settings.m_syncAMOperation == AMDemodSettings::SyncAMUSB)
        {
            m_samUSB = true;
            ui->ssb->setChecked(true);
            ui->ssb->setIcon(m_iconDSBUSB);
        }
        else
        {
            ui->ssb->setChecked(false);
        }
    }
    else
    {
        ui->ssb->setChecked(false);
        ui->ssb->setIcon(m_iconDSBUSB);
    }

    updateIndexLabel();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void AMDemodGUI::leaveEvent(QEvent* event)
{
	m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void AMDemodGUI::enterEvent(EnterEventType* event)
{
	m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void AMDemodGUI::audioSelect()
{
    qDebug("AMDemodGUI::audioSelect");
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        applySettings();
    }
}

void AMDemodGUI::samSSBSelect()
{
    AMDemodSSBDialog ssbSelect(m_samUSB);
    ssbSelect.exec();

    ui->ssb->setIcon(ssbSelect.isUsb() ? m_iconDSBUSB : m_iconDSBLSB);

    if (ssbSelect.isUsb() != m_samUSB)
    {
        qDebug("AMDemodGUI::samSSBSelect: %s", ssbSelect.isUsb() ? "usb" : "lsb");
        m_samUSB = ssbSelect.isUsb();

        if (m_settings.m_syncAMOperation != AMDemodSettings::SyncAMDSB)
        {
            m_settings.m_syncAMOperation = m_samUSB ? AMDemodSettings::SyncAMUSB : AMDemodSettings::SyncAMLSB;
            applySettings();
        }
    }
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

    int audioSampleRate = m_amDemod->getAudioSampleRate();
	bool squelchOpen = m_amDemod->getSquelchOpen();

    if ((audioSampleRate != m_audioSampleRate) || (squelchOpen != m_squelchOpen))
    {
        if (audioSampleRate < 0) {
            ui->audioMute->setStyleSheet("QToolButton { background-color : red; }");
        } else if (squelchOpen) {
            ui->audioMute->setStyleSheet("QToolButton { background-color : green; }");
        } else {
            ui->audioMute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        }

        m_audioSampleRate = audioSampleRate;
		m_squelchOpen = squelchOpen;
    }

	if (m_settings.m_pll)
	{
	    if (m_amDemod->getPllLocked()) {
	        ui->pll->setStyleSheet("QToolButton { background-color : green; }");
	    } else {
	        ui->pll->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
	    }

        int freq = (m_amDemod->getPllFrequency() * audioSampleRate) / (2.0*M_PI);
        ui->pll->setToolTip(tr("PLL for synchronous AM. Freq = %1 Hz").arg(freq));
	}

	m_tickCount++;
}

void AMDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &AMDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->pll, &QToolButton::toggled, this, &AMDemodGUI::on_pll_toggled);
    QObject::connect(ui->ssb, &QToolButton::toggled, this, &AMDemodGUI::on_ssb_toggled);
    QObject::connect(ui->bandpassEnable, &ButtonSwitch::toggled, this, &AMDemodGUI::on_bandpassEnable_toggled);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &AMDemodGUI::on_rfBW_valueChanged);
    QObject::connect(ui->volume, &QSlider::valueChanged, this, &AMDemodGUI::on_volume_valueChanged);
    QObject::connect(ui->squelch, &QSlider::valueChanged, this, &AMDemodGUI::on_squelch_valueChanged);
    QObject::connect(ui->audioMute, &QToolButton::toggled, this, &AMDemodGUI::on_audioMute_toggled);
}

void AMDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
