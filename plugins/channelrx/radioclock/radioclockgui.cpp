///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include <limits>
#include <ctype.h>
#include <QDockWidget>
#include <QDebug>

#include "radioclockgui.h"

#include "device/deviceuiset.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "ui_radioclockgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "dsp/dspengine.h"
#include "dsp/glscopesettings.h"
#include "gui/crightclickenabler.h"
#include "maincore.h"

#include "radioclock.h"
#include "radioclocksink.h"

RadioClockGUI* RadioClockGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    RadioClockGUI* gui = new RadioClockGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void RadioClockGUI::destroy()
{
    delete this;
}

void RadioClockGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray RadioClockGUI::serialize() const
{
    return m_settings.serialize();
}

bool RadioClockGUI::deserialize(const QByteArray& data)
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

void RadioClockGUI::displayDateTime()
{
    QDateTime dateTime = m_dateTime;
    if (m_settings.m_timezone == RadioClockSettings::UTC) {
        dateTime = dateTime.toUTC();
    } else if (m_settings.m_timezone == RadioClockSettings::LOCAL) {
        dateTime = dateTime.toLocalTime();
    }
    ui->date->setText(dateTime.date().toString());
    ui->time->setText(dateTime.time().toString());
}

bool RadioClockGUI::handleMessage(const Message& message)
{
    if (RadioClock::MsgConfigureRadioClock::match(message))
    {
        qDebug("RadioClockGUI::handleMessage: RadioClock::MsgConfigureRadioClock");
        const RadioClock::MsgConfigureRadioClock& cfg = (RadioClock::MsgConfigureRadioClock&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        ui->scopeGUI->updateSettings();
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (RadioClock::MsgDateTime::match(message))
    {
        RadioClock::MsgDateTime& report = (RadioClock::MsgDateTime&) message;
        m_dateTime = report.getDateTime();
        displayDateTime();
        switch (report.getDST())
        {
        case RadioClockSettings::UNKNOWN:
            ui->dst->setText("");
            break;
        case RadioClockSettings::NOT_IN_EFFECT:
            ui->dst->setText("Not in effect");
            break;
        case RadioClockSettings::IN_EFFECT:
            ui->dst->setText("In effect");
            break;
        case RadioClockSettings::ENDING:
            ui->dst->setText("Ending");
            break;
        case RadioClockSettings::STARTING:
            ui->dst->setText("Starting");
            break;
        }
        return true;
    }
    else if (RadioClock::MsgStatus::match(message))
    {
        RadioClock::MsgStatus& report = (RadioClock::MsgStatus&) message;
        ui->status->setText(report.getStatus());
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        const DSPSignalNotification& notif = (const DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        ui->deltaFrequency->setValueRange(false, 7, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();
        return true;
    }

    return false;
}

void RadioClockGUI::handleInputMessages()
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

void RadioClockGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void RadioClockGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void RadioClockGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void RadioClockGUI::on_rfBW_valueChanged(int value)
{
    ui->rfBWText->setText(QString("%1 Hz").arg(value));
    m_channelMarker.setBandwidth(value);
    m_settings.m_rfBandwidth = value;
    applySettings();
}

void RadioClockGUI::on_threshold_valueChanged(int value)
{
    ui->thresholdText->setText(QString("%1 dB").arg(value));
    m_settings.m_threshold = value;
    applySettings();
}

void RadioClockGUI::on_modulation_currentIndexChanged(int index)
{
    m_settings.m_modulation = (RadioClockSettings::Modulation)index;
    applySettings();
}

void RadioClockGUI::on_timezone_currentIndexChanged(int index)
{
    m_settings.m_timezone = (RadioClockSettings::DisplayTZ)index;
    displayDateTime();
    applySettings();
}

void RadioClockGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void RadioClockGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_radioClock->getNumberOfDeviceStreams());
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

RadioClockGUI::RadioClockGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::RadioClockGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
    m_doApplySettings(true),
    m_tickCount(0)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/radioclock/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_radioClock = reinterpret_cast<RadioClock*>(rxChannel);
    m_radioClock->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    m_scopeVis = m_radioClock->getScopeSink();
    m_scopeVis->setGLScope(ui->glScope);
    m_scopeVis->setNbStreams(RadioClockSettings::m_scopeStreams);
    m_scopeVis->setLiveRate(RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE);
    ui->glScope->connectTimer(MainCore::instance()->getMasterTimer());
    ui->scopeGUI->setBuddies(m_scopeVis->getInputMessageQueue(), m_scopeVis, ui->glScope);
    ui->scopeGUI->setStreams(QStringList({"IQ", "MagSq", "TH", "FM", "Data", "Samp", "GotMM", "GotM"}));
    ui->scopeGUI->setSampleRate(RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE);

    ui->status->setText("Looking for minute marker");

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::yellow);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle("Radio Clock");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    setTitleColor(m_channelMarker.getColor());
    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);
    m_settings.setScopeGUI(ui->scopeGUI);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    ui->scopeContainer->setVisible(false);

    displaySettings();
    makeUIConnections();
    applySettings(true);
    DialPopup::addPopupsToChildDials(this);
}

RadioClockGUI::~RadioClockGUI()
{
    delete ui;
}

void RadioClockGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void RadioClockGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        RadioClock::MsgConfigureRadioClock* message = RadioClock::MsgConfigureRadioClock::create( m_settings, force);
        m_radioClock->getInputMessageQueue()->push(message);
    }
}

void RadioClockGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->rfBWText->setText(QString("%1 Hz").arg((int)m_settings.m_rfBandwidth));
    ui->rfBW->setValue(m_settings.m_rfBandwidth);

    ui->thresholdText->setText(QString("%1 dB").arg(m_settings.m_threshold));
    ui->threshold->setValue(m_settings.m_threshold);

    ui->modulation->setCurrentIndex((int)m_settings.m_modulation);
    ui->timezone->setCurrentIndex((int)m_settings.m_timezone);

    updateIndexLabel();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void RadioClockGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void RadioClockGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void RadioClockGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_radioClock->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(QString::number(powDbAvg, 'f', 1));
    }

    m_tickCount++;
}

void RadioClockGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &RadioClockGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &RadioClockGUI::on_rfBW_valueChanged);
    QObject::connect(ui->threshold, &QDial::valueChanged, this, &RadioClockGUI::on_threshold_valueChanged);
    QObject::connect(ui->modulation, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioClockGUI::on_modulation_currentIndexChanged);
    QObject::connect(ui->timezone, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioClockGUI::on_timezone_currentIndexChanged);
}

void RadioClockGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
