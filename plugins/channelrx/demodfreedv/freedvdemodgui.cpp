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

#include <QPixmap>

#include "freedvdemodgui.h"

#include "device/deviceuiset.h"

#include "dsp/spectrumvis.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "gui/glspectrum.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/crightclickenabler.h"
#include "gui/audioselectdialog.h"
#include "maincore.h"

#include "ui_freedvdemodgui.h"
#include "freedvdemod.h"

FreeDVDemodGUI* FreeDVDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    FreeDVDemodGUI* gui = new FreeDVDemodGUI(pluginAPI, deviceUISet, rxChannel);
	return gui;
}

void FreeDVDemodGUI::destroy()
{
	delete this;
}

void FreeDVDemodGUI::resetToDefaults()
{
	m_settings.resetToDefaults();
}

QByteArray FreeDVDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool FreeDVDemodGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
    {
        displaySettings();
        applyBandwidths(5 - ui->spanLog2->value(), true); // does applySettings(true)
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        displaySettings();
        applyBandwidths(5 - ui->spanLog2->value(), true); // does applySettings(true)
        return false;
    }
}

bool FreeDVDemodGUI::handleMessage(const Message& message)
{
    if (FreeDVDemod::MsgConfigureFreeDVDemod::match(message))
    {
        qDebug("FreeDVDemodGUI::handleMessage: FreeDVDemodGUI::MsgConfigureFreeDVDemod");
        const FreeDVDemod::MsgConfigureFreeDVDemod& cfg = (FreeDVDemod::MsgConfigureFreeDVDemod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        ui->spectrumGUI->updateSettings();
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (DSPConfigureAudio::match(message))
    {
        qDebug("FreeDVDemodGUI::handleMessage: DSPConfigureAudio: %d", m_freeDVDemod->getAudioSampleRate());
        applyBandwidths(5 - ui->spanLog2->value()); // will update spectrum details with new sample rate
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
    else
    {
        return false;
    }
}

void FreeDVDemodGUI::handleInputMessages()
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

void FreeDVDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void FreeDVDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void FreeDVDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void FreeDVDemodGUI::on_reSync_clicked(bool checked)
{
    (void) checked;
    FreeDVDemod::MsgResyncFreeDVDemod* message = FreeDVDemod::MsgResyncFreeDVDemod::create();
    m_freeDVDemod->getInputMessageQueue()->push(message);
}

void FreeDVDemodGUI::on_freeDVMode_currentIndexChanged(int index)
{
    m_settings.m_freeDVMode = (FreeDVDemodSettings::FreeDVMode) index;
    m_channelMarker.setBandwidth(FreeDVDemodSettings::getHiCutoff(m_settings.m_freeDVMode) * 2);
    m_channelMarker.setLowCutoff(FreeDVDemodSettings::getLowCutoff(m_settings.m_freeDVMode));
    m_channelMarker.setSidebands(ChannelMarker::usb);
    applySettings();
}

void FreeDVDemodGUI::on_volume_valueChanged(int value)
{
	ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
	m_settings.m_volume = value / 10.0;
	applySettings();
}

void FreeDVDemodGUI::on_volumeIn_valueChanged(int value)
{
	ui->volumeInText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
	m_settings.m_volumeIn = value / 10.0;
	applySettings();
}

void FreeDVDemodGUI::on_agc_toggled(bool checked)
{
    m_settings.m_agc = checked;
    applySettings();
}

void FreeDVDemodGUI::on_audioMute_toggled(bool checked)
{
	m_audioMute = checked;
	m_settings.m_audioMute = checked;
	applySettings();
}

void FreeDVDemodGUI::on_spanLog2_valueChanged(int value)
{
    if ((value < 0) || (value > 4)) {
        return;
    }

    applyBandwidths(5 - ui->spanLog2->value());
}

void FreeDVDemodGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_freeDVDemod->getNumberOfDeviceStreams());
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

void FreeDVDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

FreeDVDemodGUI::FreeDVDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
	ChannelGUI(parent),
	ui(new Ui::FreeDVDemodGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
	m_doApplySettings(true),
    m_spectrumRate(6000),
	m_audioBinaural(false),
	m_audioFlipChannels(false),
    m_audioMute(false),
	m_squelchOpen(false),
    m_audioSampleRate(-1)
{
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodfreedv/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

	m_freeDVDemod = (FreeDVDemod*) rxChannel;
    m_spectrumVis = m_freeDVDemod->getSpectrumVis();
	m_spectrumVis->setGLSpectrum(ui->glSpectrum);
	m_freeDVDemod->setMessageQueueToGUI(getInputMessageQueue());

	resetToDefaults();

	ui->spectrumGUI->setBuddies(m_spectrumVis, ui->glSpectrum);

    ui->glSpectrum->setCenterFrequency(m_spectrumRate/2);
    ui->glSpectrum->setSampleRate(m_spectrumRate);

    SpectrumSettings spectrumSettings = m_spectrumVis->getSettings();
    spectrumSettings.m_displayWaterfall = true;
    spectrumSettings.m_displayMaxHold = false;
    spectrumSettings.m_ssb = true;
    SpectrumVis::MsgConfigureSpectrumVis *msg = SpectrumVis::MsgConfigureSpectrumVis::create(spectrumSettings, false);
    m_spectrumVis->getInputMessageQueue()->push(msg);

	connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

	CRightClickEnabler *audioMuteRightClickEnabler = new CRightClickEnabler(ui->audioMute);
    connect(audioMuteRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect(const QPoint &)));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
	ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);
    ui->snrMeter->setColorTheme(LevelMeterSignalDB::ColorCyanAndBlue);
    ui->snrMeter->setRange(-10, 30);
    ui->snrMeter->setAverageSmoothing(2);

    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setSpectrumGUI(ui->spectrumGUI);
    m_settings.setRollupState(&m_rollupState);

	m_deviceUISet->addChannelMarker(&m_channelMarker);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_freeDVDemod->setLevelMeter(ui->volumeInMeter);

	m_iconDSBUSB.addPixmap(QPixmap("://dsb.png"), QIcon::Normal, QIcon::On);
    m_iconDSBUSB.addPixmap(QPixmap("://usb.png"), QIcon::Normal, QIcon::Off);
	m_iconDSBLSB.addPixmap(QPixmap("://dsb.png"), QIcon::Normal, QIcon::On);
    m_iconDSBLSB.addPixmap(QPixmap("://lsb.png"), QIcon::Normal, QIcon::Off);

	displaySettings();
    makeUIConnections();
	applyBandwidths(5 - ui->spanLog2->value(), true); // does applySettings(true)
    DialPopup::addPopupsToChildDials(this);
}

FreeDVDemodGUI::~FreeDVDemodGUI()
{
	delete ui;
}

bool FreeDVDemodGUI::blockApplySettings(bool block)
{
    bool ret = !m_doApplySettings;
    m_doApplySettings = !block;
    return ret;
}

void FreeDVDemodGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
        FreeDVDemod::MsgConfigureFreeDVDemod* message = FreeDVDemod::MsgConfigureFreeDVDemod::create( m_settings, force);
        m_freeDVDemod->getInputMessageQueue()->push(message);
	}
}

void FreeDVDemodGUI::applyBandwidths(int spanLog2, bool force)
{
    displayBandwidths(spanLog2);
    m_settings.m_spanLog2 = spanLog2;
    applySettings(force);
}

void FreeDVDemodGUI::displayBandwidths(int spanLog2)
{
    m_spectrumRate = m_freeDVDemod->getModemSampleRate() / (1<<spanLog2);
    int bwMax = m_freeDVDemod->getModemSampleRate() / (100*(1<<spanLog2));

    qDebug() << "FreeDVDemodGUI::displayBandwidths:"
            << " spanLog2: " << spanLog2
            << " m_spectrumRate: " << m_spectrumRate
            << " bwMax: " << bwMax;

    QString spanStr = QString::number(bwMax/10.0, 'f', 1);

    ui->spanText->setText(tr("%1k").arg(spanStr));
    ui->glSpectrum->setCenterFrequency(0);
    ui->glSpectrum->setSampleRate(2*m_spectrumRate);
    ui->glSpectrum->setLsbDisplay(false);
    ui->glSpectrum->setSsbSpectrum(true);
}

void FreeDVDemodGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setBandwidth(FreeDVDemodSettings::getHiCutoff(m_settings.m_freeDVMode) * 2);
    m_channelMarker.setLowCutoff(FreeDVDemodSettings::getLowCutoff(m_settings.m_freeDVMode));
    m_channelMarker.setSidebands(ChannelMarker::usb);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->freeDVMode->setCurrentIndex((int) m_settings.m_freeDVMode);
    ui->agc->setChecked(m_settings.m_agc);
    ui->audioMute->setChecked(m_settings.m_audioMute);
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    // Prevent uncontrolled triggering of applyBandwidths
    ui->spanLog2->blockSignals(true);
    ui->spanLog2->setValue(5 - m_settings.m_spanLog2);
    displayBandwidths(m_settings.m_spanLog2);
    ui->spanLog2->blockSignals(false);

    ui->volume->setValue(m_settings.m_volume * 10.0);
    ui->volumeText->setText(QString("%1").arg(m_settings.m_volume, 0, 'f', 1));

    ui->volumeIn->setValue(m_settings.m_volumeIn * 10.0);
    ui->volumeInText->setText(QString("%1").arg(m_settings.m_volumeIn, 0, 'f', 1));

    updateIndexLabel();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void FreeDVDemodGUI::leaveEvent(QEvent* event)
{
	m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void FreeDVDemodGUI::enterEvent(EnterEventType* event)
{
	m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void FreeDVDemodGUI::audioSelect(const QPoint& p)
{
    qDebug("FreeDVDemodGUI::audioSelect");
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName);
    audioSelect.move(p);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        applySettings();
    }
}

void FreeDVDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_freeDVDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (FreeDVDemodSettings::m_mminPowerThresholdDBf + powDbAvg) / FreeDVDemodSettings::m_mminPowerThresholdDBf,
            (FreeDVDemodSettings::m_mminPowerThresholdDBf + powDbPeak) / FreeDVDemodSettings::m_mminPowerThresholdDBf,
            nbMagsqSamples);

    double snrAvg, snrPeak;
    int nbSNRSamples;
    m_freeDVDemod->getSNRLevels(snrAvg, snrPeak, nbSNRSamples);

    ui->snrMeter->levelChanged(
        (10.0f + snrAvg) / 40.0f,
        (10.0f + snrPeak) / 40.0f,
        nbSNRSamples
    );

    ui->berText->setText(tr("%1").arg(m_freeDVDemod->getBER()));
    float freqOffset = m_freeDVDemod->getFrequencyOffset();
    int freqOffsetInt = freqOffset < -999 ? -999 : freqOffset > 999 ? 999 : freqOffset;
    ui->freqOffset->setText(tr("%1Hz").arg(freqOffsetInt));

    if (m_freeDVDemod->isSync()) {
        ui->syncLabel->setStyleSheet("QLabel { background-color : green; }");
    } else {
        ui->syncLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
    }

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(tr("%1 dB").arg(powDbAvg, 0, 'f', 1));
        ui->snrText->setText(tr("%1 dB").arg(snrAvg < -90 ? -90 : snrAvg > 90 ? 90 : snrAvg, 0, 'f', 1));
    }

    int audioSampleRate = m_freeDVDemod->getAudioSampleRate();
    bool squelchOpen = m_freeDVDemod->getAudioActive();

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

    m_tickCount++;
}

void FreeDVDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &FreeDVDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->reSync, &QPushButton::clicked, this, &FreeDVDemodGUI::on_reSync_clicked);
    QObject::connect(ui->freeDVMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FreeDVDemodGUI::on_freeDVMode_currentIndexChanged);
    QObject::connect(ui->volume, &QDial::valueChanged, this, &FreeDVDemodGUI::on_volume_valueChanged);
    QObject::connect(ui->volumeIn, &QDial::valueChanged, this, &FreeDVDemodGUI::on_volumeIn_valueChanged);
    QObject::connect(ui->agc, &ButtonSwitch::toggled, this, &FreeDVDemodGUI::on_agc_toggled);
    QObject::connect(ui->audioMute, &QToolButton::toggled, this, &FreeDVDemodGUI::on_audioMute_toggled);
    QObject::connect(ui->spanLog2, &QSlider::valueChanged, this, &FreeDVDemodGUI::on_spanLog2_valueChanged);
}

void FreeDVDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
