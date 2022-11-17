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

#include <QDockWidget>
#include <QMainWindow>
#include <QDebug>

#include "freqtrackergui.h"

#include "device/deviceuiset.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "ui_freqtrackergui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "dsp/dspengine.h"
#include "gui/crightclickenabler.h"
#include "gui/audioselectdialog.h"
#include "maincore.h"

#include "freqtracker.h"
#include "freqtrackerreport.h"

FreqTrackerGUI* FreqTrackerGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	FreqTrackerGUI* gui = new FreqTrackerGUI(pluginAPI, deviceUISet, rxChannel);
	return gui;
}

void FreqTrackerGUI::destroy()
{
	delete this;
}

void FreqTrackerGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
	applySettings(true);
}

QByteArray FreqTrackerGUI::serialize() const
{
    return m_settings.serialize();
}

bool FreqTrackerGUI::deserialize(const QByteArray& data)
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

bool FreqTrackerGUI::handleMessage(const Message& message)
{
    if (FreqTracker::MsgConfigureFreqTracker::match(message))
    {
        qDebug("FreqTrackerGUI::handleMessage: FreqTracker::MsgConfigureFreqTracker");
        const FreqTracker::MsgConfigureFreqTracker& cfg = (FreqTracker::MsgConfigureFreqTracker&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        ui->spectrumGUI->updateSettings();
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& cfg = (DSPSignalNotification&) message;
        m_deviceCenterFrequency = cfg.getCenterFrequency();
        m_basebandSampleRate = cfg.getSampleRate();
        ui->deltaFrequency->setValueRange(false, 7, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();
        int sinkSampleRate = m_basebandSampleRate / (1<<m_settings.m_log2Decim);
        ui->channelSampleRateText->setText(tr("%1k").arg(QString::number(sinkSampleRate / 1000.0f, 'g', 5)));
        displaySpectrumBandwidth(m_settings.m_spanLog2);
        m_pllChannelMarker.setBandwidth(sinkSampleRate/500);

        if (sinkSampleRate > 1000) {
            ui->rfBW->setMaximum(sinkSampleRate/100);
        }

        return true;
    }
    else if (FreqTrackerReport::MsgSinkFrequencyOffsetNotification::match(message))
    {
        if (!m_settings.m_tracking) {
            qDebug("FreqTrackerGUI::handleMessage: FreqTrackerReport::MsgSinkFrequencyOffsetNotification");
        }
        const FreqTrackerReport::MsgSinkFrequencyOffsetNotification& cfg = (FreqTrackerReport::MsgSinkFrequencyOffsetNotification&) message;
        m_settings.m_inputFrequencyOffset = cfg.getFrequencyOffset();
        m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
        ui->deltaFrequency->setValue(m_settings.m_inputFrequencyOffset);

        return true;
    }

	return false;
}

void FreqTrackerGUI::handleInputMessages()
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

void FreqTrackerGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
	applySettings();
}

void FreqTrackerGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void FreqTrackerGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void FreqTrackerGUI::on_log2Decim_currentIndexChanged(int index)
{
    m_settings.m_log2Decim = index < 0 ? 0 : index > 6 ? 6 : index;
    int sinkSampleRate = m_basebandSampleRate / (1<<m_settings.m_log2Decim);
    ui->channelSampleRateText->setText(tr("%1k").arg(QString::number(sinkSampleRate / 1000.0f, 'g', 5)));
    displaySpectrumBandwidth(m_settings.m_spanLog2);
    m_pllChannelMarker.setBandwidth(sinkSampleRate/500);

    if (sinkSampleRate > 1000) {
        ui->rfBW->setMaximum(sinkSampleRate/100);
    }

    applySettings();
}

void FreqTrackerGUI::on_rfBW_valueChanged(int value)
{
	ui->rfBWText->setText(QString("%1 kHz").arg(value / 10.0, 0, 'f', 1));
	m_channelMarker.setBandwidth(value * 100);
	m_settings.m_rfBandwidth = value * 100;
	applySettings();
}

void FreqTrackerGUI::on_tracking_toggled(bool checked)
{
    if (!checked)
    {
        ui->tracking->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        ui->tracking->setToolTip(tr("PLL for synchronous AM"));
    }

    m_settings.m_tracking = checked;
    applySettings();
}

void FreqTrackerGUI::on_alphaEMA_valueChanged(int value)
{
    m_settings.m_alphaEMA = value / 100.0;
    QString alphaEMAStr = QString::number(m_settings.m_alphaEMA, 'f', 2);
    ui->alphaEMAText->setText(alphaEMAStr);
    applySettings();
}

void FreqTrackerGUI::on_trackerType_currentIndexChanged(int index)
{
    m_settings.m_trackerType = (FreqTrackerSettings::TrackerType) index;
    applySettings();
}

void FreqTrackerGUI::on_pllPskOrder_currentIndexChanged(int index)
{
    if ((index < 0) || (index > 5)) {
        return;
    }

    m_settings.m_pllPskOrder = 1<<index;
    applySettings();
}

void FreqTrackerGUI::on_rrc_toggled(bool checked)
{
    m_settings.m_rrc = checked;
    applySettings();
}

void FreqTrackerGUI::on_rrcRolloff_valueChanged(int value)
{
    m_settings.m_rrcRolloff = value < 0 ? 0 : value > 100 ? 100 : value;
    QString rolloffStr = QString::number(value/100.0, 'f', 2);
    ui->rrcRolloffText->setText(rolloffStr);
    applySettings();
}

void FreqTrackerGUI::on_squelch_valueChanged(int value)
{
	ui->squelchText->setText(QString("%1 dB").arg(value));
	m_settings.m_squelch = value;
	applySettings();
}

void FreqTrackerGUI::on_squelchGate_valueChanged(int value)
{
    ui->squelchGateText->setText(QString("%1").arg(value * 10.0f, 0, 'f', 0));
    m_settings.m_squelchGate = value;
	applySettings();
}

void FreqTrackerGUI::on_spanLog2_valueChanged(int value)
{
    if ((value < 0) || (value > 6)) {
        return;
    }

    applySpectrumBandwidth(ui->spanLog2->value());
}

void FreqTrackerGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void FreqTrackerGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_freqTracker->getNumberOfDeviceStreams());
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

FreqTrackerGUI::FreqTrackerGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
	ChannelGUI(parent),
	ui(new Ui::FreqTrackerGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
    m_pllChannelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(0),
	m_doApplySettings(true),
	m_squelchOpen(false),
	m_tickCount(0)
{
	ui->setupUi(getRollupContents());
    getRollupContents()->arrangeRollups();
    m_helpURL = "plugins/channelrx/freqtracker/readme.md";
	setAttribute(Qt::WA_DeleteOnClose, true);
	connect(getRollupContents(), SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

	m_freqTracker = reinterpret_cast<FreqTracker*>(rxChannel);
	m_freqTracker->setMessageQueueToGUI(getInputMessageQueue());
    m_spectrumVis = m_freqTracker->getSpectrumVis();
	m_spectrumVis->setGLSpectrum(ui->glSpectrum);

	connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
	ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

	m_channelMarker.blockSignals(true);
	m_channelMarker.setColor(Qt::yellow);
	m_channelMarker.setBandwidth(5000);
	m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("Frequency Tracker");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    setTitleColor(m_channelMarker.getColor());
    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setSpectrumGUI(ui->spectrumGUI);
    m_settings.setRollupState(&m_rollupState);

	m_deviceUISet->addChannelMarker(&m_channelMarker);

    ui->glSpectrum->setCenterFrequency(0);
    m_pllChannelMarker.blockSignals(true);
    m_pllChannelMarker.setColor(Qt::white);
    m_pllChannelMarker.setCenterFrequency(0);
    m_pllChannelMarker.setBandwidth(70);
    m_pllChannelMarker.setTitle("Tracker");
    m_pllChannelMarker.setMovable(false);
    m_pllChannelMarker.blockSignals(false);
    m_pllChannelMarker.setVisible(true);
    ui->glSpectrum->addChannelMarker(&m_pllChannelMarker);
    ui->spectrumGUI->setBuddies(m_spectrumVis, ui->glSpectrum);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

	displaySettings();
    makeUIConnections();
	applySettings(true);
}

FreqTrackerGUI::~FreqTrackerGUI()
{
	delete ui;
}

void FreqTrackerGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void FreqTrackerGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
	    FreqTracker::MsgConfigureFreqTracker* message = FreqTracker::MsgConfigureFreqTracker::create( m_settings, force);
	    m_freqTracker->getInputMessageQueue()->push(message);
	}
}

void FreqTrackerGUI::applySpectrumBandwidth(int spanLog2, bool force)
{
    displaySpectrumBandwidth(spanLog2);
    m_settings.m_spanLog2 = spanLog2;
    applySettings(force);
}

void FreqTrackerGUI::displaySettings()
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
    ui->log2Decim->setCurrentIndex(m_settings.m_log2Decim);
    int displayValue = m_settings.m_rfBandwidth/100.0;
    ui->rfBW->setValue(displayValue);
    ui->rfBWText->setText(QString("%1 kHz").arg(displayValue / 10.0, 0, 'f', 1));
    ui->squelch->setValue(m_settings.m_squelch);
    ui->squelchText->setText(QString("%1 dB").arg(m_settings.m_squelch));
    ui->tracking->setChecked(m_settings.m_tracking);
    ui->trackerType->setCurrentIndex((int) m_settings.m_trackerType);
    QString alphaEMAStr = QString::number(m_settings.m_alphaEMA, 'f', 2);
    ui->alphaEMAText->setText(alphaEMAStr);
    ui->alphaEMA->setValue(m_settings.m_alphaEMA*100.0);

    int i = 0;
    for(; ((m_settings.m_pllPskOrder>>i) & 1) == 0; i++);
    ui->pllPskOrder->setCurrentIndex(i);

    ui->rrc->setChecked(m_settings.m_rrc);
    ui->rrcRolloff->setValue(m_settings.m_rrcRolloff);
    QString rolloffStr = QString::number(m_settings.m_rrcRolloff/100.0, 'f', 2);
    ui->rrcRolloffText->setText(rolloffStr);
    ui->squelchGateText->setText(QString("%1").arg(m_settings.m_squelchGate * 10.0f, 0, 'f', 0));
    ui->squelchGate->setValue(m_settings.m_squelchGate);

    displaySpectrumBandwidth(m_settings.m_spanLog2);
    updateIndexLabel();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void FreqTrackerGUI::displaySpectrumBandwidth(int spanLog2)
{
    int spectrumRate = (m_basebandSampleRate / (1<<m_settings.m_log2Decim)) / (1<<spanLog2);

    qDebug() << "FreqTrackerGUI::displaySpectrumBandwidth:"
            << " spanLog2: " << spanLog2
            << " spectrumRate: " << spectrumRate;

    QString spanStr = QString::number(spectrumRate/1000.0, 'f', 1);

    ui->spanLog2->blockSignals(true);
    ui->spanLog2->setValue(spanLog2);
    ui->spanLog2->blockSignals(false);
    ui->spanText->setText(tr("%1k").arg(spanStr));
    ui->glSpectrum->setSampleRate(spectrumRate);
}

void FreqTrackerGUI::leaveEvent(QEvent* event)
{
	m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void FreqTrackerGUI::enterEvent(EnterEventType* event)
{
	m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void FreqTrackerGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_freqTracker->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(QString::number(powDbAvg, 'f', 1));
    }

	bool squelchOpen = m_freqTracker->getSquelchOpen();

    if (squelchOpen) {
        ui->squelchLabel->setStyleSheet("QLabel { background-color : green; }");
    } else {
        ui->squelchLabel->setStyleSheet("QLabel { background:rgb(50,50,50); }");
    }

    if (m_freqTracker->getPllLocked()) {
        ui->tracking->setStyleSheet("QToolButton { background-color : green; }");
    } else {
        ui->tracking->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
    }

    int freq = m_freqTracker->getAvgDeltaFreq();
    QLocale loc;
    ui->trackingFrequencyText->setText(tr("%1 Hz").arg(loc.toString(freq)));
    m_pllChannelMarker.setCenterFrequency(freq);

	if (m_settings.m_tracking) {
        ui->tracking->setToolTip("Tracking on");
	} else {
        ui->tracking->setToolTip("Tracking off");
    }

	m_tickCount++;
}

void FreqTrackerGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &FreqTrackerGUI::on_deltaFrequency_changed);
    QObject::connect(ui->log2Decim, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FreqTrackerGUI::on_log2Decim_currentIndexChanged);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &FreqTrackerGUI::on_rfBW_valueChanged);
    QObject::connect(ui->tracking, &QToolButton::toggled, this, &FreqTrackerGUI::on_tracking_toggled);
    QObject::connect(ui->alphaEMA, &QDial::valueChanged, this, &FreqTrackerGUI::on_alphaEMA_valueChanged);
    QObject::connect(ui->trackerType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FreqTrackerGUI::on_trackerType_currentIndexChanged);
    QObject::connect(ui->pllPskOrder, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FreqTrackerGUI::on_pllPskOrder_currentIndexChanged);
    QObject::connect(ui->rrc, &ButtonSwitch::toggled, this, &FreqTrackerGUI::on_rrc_toggled);
    QObject::connect(ui->rrcRolloff, &QDial::valueChanged, this, &FreqTrackerGUI::on_rrcRolloff_valueChanged);
    QObject::connect(ui->squelch, &QSlider::valueChanged, this, &FreqTrackerGUI::on_squelch_valueChanged);
    QObject::connect(ui->squelchGate, &QDial::valueChanged, this, &FreqTrackerGUI::on_squelchGate_valueChanged);
    QObject::connect(ui->spanLog2, &QSlider::valueChanged, this, &FreqTrackerGUI::on_spanLog2_valueChanged);
}

void FreqTrackerGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
