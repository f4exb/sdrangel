///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2015 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2018 beta-tester <alpha-beta-release@gmx.net>                   //
// Copyright (C) 2019 Stefan Biereigel <stefan@biereigel.de>                     //
// Copyright (C) 2021-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
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
#include <QFileDialog>
#include <QTime>
#include <QDebug>

#include "ssbmodgui.h"

#include "device/deviceuiset.h"
#include "dsp/spectrumvis.h"
#include "ui_ssbmodgui.h"
#include "plugin/pluginapi.h"
#include "util/db.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/cwkeyer.h"
#include "gui/crightclickenabler.h"
#include "gui/audioselectdialog.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "maincore.h"

SSBModGUI* SSBModGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    SSBModGUI* gui = new SSBModGUI(pluginAPI, deviceUISet, channelTx);
	return gui;
}

void SSBModGUI::destroy()
{
    delete this;
}

void SSBModGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
}

QByteArray SSBModGUI::serialize() const
{
    return m_settings.serialize();
}

bool SSBModGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
    {
        qDebug("SSBModGUI::deserialize");
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

bool SSBModGUI::handleMessage(const Message& message)
{
    if (SSBMod::MsgReportFileSourceStreamData::match(message))
    {
        m_recordSampleRate = ((SSBMod::MsgReportFileSourceStreamData&)message).getSampleRate();
        m_recordLength = ((SSBMod::MsgReportFileSourceStreamData&)message).getRecordLength();
        m_samplesCount = 0;
        updateWithStreamData();
        return true;
    }
    else if (SSBMod::MsgReportFileSourceStreamTiming::match(message))
    {
        m_samplesCount = ((SSBMod::MsgReportFileSourceStreamTiming&)message).getSamplesCount();
        updateWithStreamTime();
        return true;
    }
    else if (DSPConfigureAudio::match(message))
    {
        qDebug("SSBModGUI::handleMessage: DSPConfigureAudio: %d", m_ssbMod->getAudioSampleRate());
        applyBandwidths(5 - ui->spanLog2->value()); // will update spectrum details with new sample rate
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
    else if (SSBMod::MsgConfigureSSBMod::match(message))
    {
        SSBModSettings mod_settings; // different USB/LSB convention between modulator and GUI
        const SSBMod::MsgConfigureSSBMod& cfg = (SSBMod::MsgConfigureSSBMod&) message;
        mod_settings = cfg.getSettings();
        if (mod_settings.m_usb == false) {
            mod_settings.m_bandwidth = -mod_settings.m_bandwidth;
            mod_settings.m_lowCutoff = -mod_settings.m_lowCutoff;
        }
        m_settings = mod_settings;
        blockApplySettings(true);
        ui->spectrumGUI->updateSettings();
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (CWKeyer::MsgConfigureCWKeyer::match(message))
    {
        const CWKeyer::MsgConfigureCWKeyer& cfg = (CWKeyer::MsgConfigureCWKeyer&) message;
        ui->cwKeyerGUI->setSettings(cfg.getSettings());
        ui->cwKeyerGUI->displaySettings();
        return true;
    }
    else
    {
        return false;
    }
}

void SSBModGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void SSBModGUI::channelMarkerUpdate()
{
    m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
    displaySettings();
    applySettings();
}

void SSBModGUI::handleSourceMessages()
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

void SSBModGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void SSBModGUI::on_flipSidebands_clicked(bool checked)
{
    (void) checked;
    int bwValue = ui->BW->value();
    int lcValue = ui->lowCut->value();
    ui->BW->setValue(-bwValue);
    ui->lowCut->setValue(-lcValue);
}

void SSBModGUI::on_dsb_toggled(bool dsb)
{
    ui->flipSidebands->setEnabled(!dsb);
    applyBandwidths(5 - ui->spanLog2->value());
}

void SSBModGUI::on_audioBinaural_toggled(bool checked)
{
    m_settings.m_audioBinaural = checked;
	applySettings();
}

void SSBModGUI::on_audioFlipChannels_toggled(bool checked)
{
    m_settings.m_audioFlipChannels = checked;
	applySettings();
}

void SSBModGUI::on_spanLog2_valueChanged(int value)
{
    if ((value < 0) || (value > 4)) {
        return;
    }

    applyBandwidths(5 - value);
}

void SSBModGUI::on_BW_valueChanged(int value)
{
    (void) value;
    applyBandwidths(5 - ui->spanLog2->value());
}

void SSBModGUI::on_lowCut_valueChanged(int value)
{
    (void) value;
    applyBandwidths(5 - ui->spanLog2->value());
}

void SSBModGUI::on_toneFrequency_valueChanged(int value)
{
    ui->toneFrequencyText->setText(QString("%1k").arg(value / 100.0, 0, 'f', 2));
    m_settings.m_toneFrequency = value * 10.0;
    applySettings();
}

void SSBModGUI::on_volume_valueChanged(int value)
{
    ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_volumeFactor = value / 10.0;
    applySettings();
}

void SSBModGUI::on_audioMute_toggled(bool checked)
{
    m_settings.m_audioMute = checked;
	applySettings();
}

void SSBModGUI::on_playLoop_toggled(bool checked)
{
    m_settings.m_playLoop = checked;
    applySettings();
}

void SSBModGUI::on_play_toggled(bool checked)
{
    ui->tone->setEnabled(!checked); // release other source inputs
    ui->morseKeyer->setEnabled(!checked);
    ui->mic->setEnabled(!checked);
    m_settings.m_modAFInput = checked ? SSBModSettings::SSBModInputFile : SSBModSettings::SSBModInputNone;
    applySettings();
    ui->navTimeSlider->setEnabled(!checked);
    m_enableNavTime = !checked;
}

void SSBModGUI::on_tone_toggled(bool checked)
{
    ui->play->setEnabled(!checked); // release other source inputs
    ui->morseKeyer->setEnabled(!checked);
    ui->mic->setEnabled(!checked);
    m_settings.m_modAFInput = checked ? SSBModSettings::SSBModInputTone : SSBModSettings::SSBModInputNone;
    applySettings();
}

void SSBModGUI::on_morseKeyer_toggled(bool checked)
{
    ui->play->setEnabled(!checked); // release other source inputs
    ui->tone->setEnabled(!checked); // release other source inputs
    ui->mic->setEnabled(!checked);
    m_settings.m_modAFInput = checked ? SSBModSettings::SSBModInputCWTone : SSBModSettings::SSBModInputNone;
    applySettings();
}

void SSBModGUI::on_mic_toggled(bool checked)
{
    ui->play->setEnabled(!checked); // release other source inputs
    ui->morseKeyer->setEnabled(!checked);
    ui->tone->setEnabled(!checked); // release other source inputs
    m_settings.m_modAFInput = checked ? SSBModSettings::SSBModInputAudio : SSBModSettings::SSBModInputNone;
    applySettings();
}

void SSBModGUI::on_feedbackEnable_toggled(bool checked)
{
    m_settings.m_feedbackAudioEnable = checked;
    applySettings();
}

void SSBModGUI::on_feedbackVolume_valueChanged(int value)
{
    ui->feedbackVolumeText->setText(QString("%1").arg(value / 100.0, 0, 'f', 2));
    m_settings.m_feedbackVolumeFactor = value / 100.0;
    applySettings();
}

void SSBModGUI::on_agc_toggled(bool checked)
{
    m_settings.m_agc = checked;
    applySettings();
}

void SSBModGUI::on_cmpPreGain_valueChanged(int value)
{
    m_settings.m_cmpPreGainDB = value;
    ui->cmpPreGainText->setText(QString("%1").arg(value));
    applySettings();
}

void SSBModGUI::on_cmpThreshold_valueChanged(int value)
{
    m_settings.m_cmpThresholdDB = value;
    ui->cmpThresholdText->setText(QString("%1").arg(value));
    applySettings();
}

void SSBModGUI::on_navTimeSlider_valueChanged(int value)
{
    if (m_enableNavTime && ((value >= 0) && (value <= 100)))
    {
        int t_sec = (m_recordLength * value) / 100;
        QTime t(0, 0, 0, 0);
        t = t.addSecs(t_sec);

        SSBMod::MsgConfigureFileSourceSeek* message = SSBMod::MsgConfigureFileSourceSeek::create(value);
        m_ssbMod->getInputMessageQueue()->push(message);
    }
}

void SSBModGUI::on_showFileDialog_clicked(bool checked)
{
    (void) checked;
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Open raw audio file"), ".", tr("Raw audio Files (*.raw)"), 0, QFileDialog::DontUseNativeDialog);

    if (fileName != "")
    {
        m_fileName = fileName;
        ui->recordFileText->setText(m_fileName);
        ui->play->setEnabled(true);
        configureFileName();
    }
}

void SSBModGUI::configureFileName()
{
    qDebug() << "FileSourceGui::configureFileName: " << m_fileName.toStdString().c_str();
    SSBMod::MsgConfigureFileSourceName* message = SSBMod::MsgConfigureFileSourceName::create(m_fileName);
    m_ssbMod->getInputMessageQueue()->push(message);
}

void SSBModGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void SSBModGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuType::ContextMenuChannelSettings)
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
            dialog.setNumberOfStreams(m_ssbMod->getNumberOfDeviceStreams());
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

SSBModGUI::SSBModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent) :
	ChannelGUI(parent),
	ui(new Ui::SSBModGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
	m_doApplySettings(true),
	m_spectrumRate(6000),
    m_recordLength(0),
    m_recordSampleRate(48000),
    m_samplesCount(0),
    m_audioSampleRate(-1),
    m_feedbackAudioSampleRate(-1),
    m_tickCount(0),
    m_enableNavTime(false)
{
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channeltx/modssb/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

	m_ssbMod = (SSBMod*) channelTx;
    m_spectrumVis = m_ssbMod->getSpectrumVis();
	m_spectrumVis->setGLSpectrum(ui->glSpectrum);
	m_ssbMod->setMessageQueueToGUI(getInputMessageQueue());

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

    CRightClickEnabler *audioMuteRightClickEnabler = new CRightClickEnabler(ui->mic);
    connect(audioMuteRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect(const QPoint &)));

    CRightClickEnabler *feedbackRightClickEnabler = new CRightClickEnabler(ui->feedbackEnable);
    connect(feedbackRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioFeedbackSelect(const QPoint &)));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    m_channelMarker.blockSignals(true);
	m_channelMarker.setColor(Qt::green);
	m_channelMarker.setBandwidth(m_spectrumRate);
	m_channelMarker.setSidebands(ChannelMarker::usb);
	m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("SSB Modulator");
    m_channelMarker.setSourceOrSinkStream(false);
    m_channelMarker.blockSignals(false);
	m_channelMarker.setVisible(true);

    setTitleColor(m_channelMarker.getColor());

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));

    ui->cwKeyerGUI->setCWKeyer(m_ssbMod->getCWKeyer());
    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setSpectrumGUI(ui->spectrumGUI);
    m_settings.setCWKeyerGUI(ui->cwKeyerGUI);
    m_settings.setRollupState(&m_rollupState);

	connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
    m_ssbMod->setLevelMeter(ui->volumeMeter);

    m_iconDSBUSB.addPixmap(QPixmap("://dsb.png"), QIcon::Normal, QIcon::On);
    m_iconDSBUSB.addPixmap(QPixmap("://usb.png"), QIcon::Normal, QIcon::Off);
    m_iconDSBLSB.addPixmap(QPixmap("://dsb.png"), QIcon::Normal, QIcon::On);
    m_iconDSBLSB.addPixmap(QPixmap("://lsb.png"), QIcon::Normal, QIcon::Off);

    displaySettings();
    makeUIConnections();
    applyBandwidths(5 - ui->spanLog2->value(), true); // does applySettings(true)
    DialPopup::addPopupsToChildDials(this);
    m_resizer.enableChildMouseTracking();
}

SSBModGUI::~SSBModGUI()
{
	delete ui;
}

bool SSBModGUI::blockApplySettings(bool block)
{
    bool ret = !m_doApplySettings;
    m_doApplySettings = !block;
    return ret;
}

void SSBModGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
        SSBModSettings mod_settings; // different USB/LSB convention between modulator and GUI
        mod_settings = m_settings;

        if (mod_settings.m_bandwidth > 0)
        {
            mod_settings.m_usb = true;
        }
        else
        {
            mod_settings.m_bandwidth = -mod_settings.m_bandwidth;
            mod_settings.m_lowCutoff = -mod_settings.m_lowCutoff;
            mod_settings.m_usb = false;
        }

		SSBMod::MsgConfigureSSBMod *msg = SSBMod::MsgConfigureSSBMod::create(mod_settings, force);
		m_ssbMod->getInputMessageQueue()->push(msg);
	}
}

uint32_t SSBModGUI::getValidAudioSampleRate() const
{
    // When not running, m_ssbDemod->getAudioSampleRate() will return 0, but we
    // want a valid value to initialise the GUI, to allow a user to preselect settings
    int sr = m_ssbMod->getAudioSampleRate();
    if (sr == 0)
    {
        if (m_audioSampleRate > 0) {
            sr = m_audioSampleRate;
        } else {
            sr = 48000;
        }
    }
    return sr;
}

void SSBModGUI::applyBandwidths(int spanLog2, bool force)
{
    bool dsb = ui->dsb->isChecked();
    m_spectrumRate = getValidAudioSampleRate() / (1<<spanLog2);
    int bw = ui->BW->value();
    int lw = ui->lowCut->value();
    int bwMax = getValidAudioSampleRate() / (100*(1<<spanLog2));
    int tickInterval = m_spectrumRate / 1200;
    tickInterval = tickInterval == 0 ? 1 : tickInterval;

    qDebug() << "SSBModGUI::applyBandwidths:"
            << " dsb: " << dsb
            << " spanLog2: " << spanLog2
            << " m_spectrumRate: " << m_spectrumRate
            << " bw: " << bw
            << " lw: " << lw
            << " bwMax: " << bwMax
            << " tickInterval: " << tickInterval;

    ui->BW->setTickInterval(tickInterval);
    ui->lowCut->setTickInterval(tickInterval);

    bw = bw < -bwMax ? -bwMax : bw > bwMax ? bwMax : bw;

    if (bw < 0) {
        lw = lw < bw+1 ? bw+1 : lw < 0 ? lw : 0;
    } else if (bw > 0) {
        lw = lw > bw-1 ? bw-1 : lw < 0 ? 0 : lw;
    } else {
        lw = 0;
    }

    if (dsb)
    {
        bw = bw < 0 ? -bw : bw;
        lw = 0;
    }

    QString spanStr = QString::number(bwMax/10.0, 'f', 1);
    QString bwStr   = QString::number(bw/10.0, 'f', 1);
    QString lwStr   = QString::number(lw/10.0, 'f', 1);

    SpectrumSettings spectrumSettings = m_spectrumVis->getSettings();

    if (dsb)
    {
        ui->BWText->setText(tr("%1%2k").arg(QChar(0xB1, 0x00)).arg(bwStr));
        ui->spanText->setText(tr("%1%2k").arg(QChar(0xB1, 0x00)).arg(spanStr));
        ui->scaleMinus->setText("0");
        ui->scaleCenter->setText("");
        ui->scalePlus->setText(tr("%1").arg(QChar(0xB1, 0x00)));
        ui->lsbLabel->setText("");
        ui->usbLabel->setText("");
        ui->glSpectrum->setCenterFrequency(0);
        ui->glSpectrum->setSampleRate(2*m_spectrumRate);
        spectrumSettings.m_ssb = false;
        ui->glSpectrum->setLsbDisplay(false);
        ui->glSpectrum->setSsbSpectrum(false);
    }
    else
    {
        ui->BWText->setText(tr("%1k").arg(bwStr));
        ui->spanText->setText(tr("%1k").arg(spanStr));
        ui->scaleMinus->setText("-");
        ui->scaleCenter->setText("0");
        ui->scalePlus->setText("+");
        ui->lsbLabel->setText("LSB");
        ui->usbLabel->setText("USB");
        ui->glSpectrum->setCenterFrequency(0);
        ui->glSpectrum->setSampleRate(2*m_spectrumRate);
        spectrumSettings.m_ssb = true;
        ui->glSpectrum->setLsbDisplay(bw < 0);
        ui->glSpectrum->setSsbSpectrum(true);
    }

    SpectrumVis::MsgConfigureSpectrumVis *msg = SpectrumVis::MsgConfigureSpectrumVis::create(spectrumSettings, false);
    m_spectrumVis->getInputMessageQueue()->push(msg);

    ui->lowCutText->setText(tr("%1k").arg(lwStr));


    ui->BW->blockSignals(true);
    ui->lowCut->blockSignals(true);

    ui->BW->setMaximum(bwMax);
    ui->BW->setMinimum(dsb ? 0 : -bwMax);
    ui->BW->setValue(bw);

    ui->lowCut->setMaximum(dsb ? 0 :  bwMax);
    ui->lowCut->setMinimum(dsb ? 0 : -bwMax);
    ui->lowCut->setValue(lw);

    ui->lowCut->blockSignals(false);
    ui->BW->blockSignals(false);


    m_settings.m_dsb = dsb;
    m_settings.m_spanLog2 = spanLog2;
    m_settings.m_bandwidth = bw * 100;
    m_settings.m_lowCutoff = lw * 100;

    applySettings(force);

    bool applySettingsWereBlocked = blockApplySettings(true);
    m_channelMarker.setBandwidth(bw * 200);
    m_channelMarker.setSidebands(dsb ? ChannelMarker::dsb : bw < 0 ? ChannelMarker::lsb : ChannelMarker::usb);
    ui->dsb->setIcon(bw < 0 ? m_iconDSBLSB : m_iconDSBUSB);
    if (!dsb) { m_channelMarker.setLowCutoff(lw * 100); }
    blockApplySettings(applySettingsWereBlocked);
}

void SSBModGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setBandwidth(m_settings.m_bandwidth * 2);
    m_channelMarker.setLowCutoff(m_settings.m_lowCutoff);

    ui->flipSidebands->setEnabled(!m_settings.m_dsb);

    if (m_settings.m_dsb) {
        m_channelMarker.setSidebands(ChannelMarker::dsb);
    } else {
        if (m_settings.m_bandwidth < 0) {
            m_channelMarker.setSidebands(ChannelMarker::lsb);
            ui->dsb->setIcon(m_iconDSBLSB);
        } else {
            m_channelMarker.setSidebands(ChannelMarker::usb);
            ui->dsb->setIcon(m_iconDSBUSB);
        }
    }

    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor);

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());
    updateIndexLabel();

    blockApplySettings(true);

    ui->agc->setChecked(m_settings.m_agc);
    ui->cmpPreGainText->setText(QString("%1").arg(m_settings.m_cmpPreGainDB));
    ui->cmpThresholdText->setText(QString("%1").arg(m_settings.m_cmpThresholdDB));
    ui->audioBinaural->setChecked(m_settings.m_audioBinaural);
    ui->audioFlipChannels->setChecked(m_settings.m_audioFlipChannels);
    ui->audioMute->setChecked(m_settings.m_audioMute);
    ui->playLoop->setChecked(m_settings.m_playLoop);

    // Prevent uncontrolled triggering of applyBandwidths
    ui->spanLog2->blockSignals(true);
    ui->dsb->blockSignals(true);
    ui->BW->blockSignals(true);

    ui->dsb->setChecked(m_settings.m_dsb);
    ui->spanLog2->setValue(5 - m_settings.m_spanLog2);

    ui->BW->setValue(roundf(m_settings.m_bandwidth/100.0));
    QString s = QString::number(m_settings.m_bandwidth/1000.0, 'f', 1);

    if (m_settings.m_dsb)
    {
        ui->BWText->setText(tr("%1%2k").arg(QChar(0xB1, 0x00)).arg(s));
    }
    else
    {
        ui->BWText->setText(tr("%1k").arg(s));
    }

    ui->spanLog2->blockSignals(false);
    ui->dsb->blockSignals(false);
    ui->BW->blockSignals(false);

    // The only one of the four signals triggering applyBandwidths will trigger it once only with all other values
    // set correctly and therefore validate the settings and apply them to dependent widgets
    ui->lowCut->setValue(m_settings.m_lowCutoff / 100.0);
    ui->lowCutText->setText(tr("%1k").arg(m_settings.m_lowCutoff / 1000.0));

    ui->deltaFrequency->setValue(m_settings.m_inputFrequencyOffset);

    ui->toneFrequency->setValue(roundf(m_settings.m_toneFrequency / 10.0));
    ui->toneFrequencyText->setText(QString("%1k").arg(m_settings.m_toneFrequency / 1000.0, 0, 'f', 2));

    ui->volume->setValue(m_settings.m_volumeFactor * 10.0);
    ui->volumeText->setText(QString("%1").arg(m_settings.m_volumeFactor, 0, 'f', 1));

    ui->tone->setEnabled((m_settings.m_modAFInput == SSBModSettings::SSBModInputAF::SSBModInputTone)
            || (m_settings.m_modAFInput == SSBModSettings::SSBModInputAF::SSBModInputNone));
    ui->mic->setEnabled((m_settings.m_modAFInput == SSBModSettings::SSBModInputAF::SSBModInputAudio)
            || (m_settings.m_modAFInput == SSBModSettings::SSBModInputAF::SSBModInputNone));
    ui->play->setEnabled((m_settings.m_modAFInput == SSBModSettings::SSBModInputAF::SSBModInputFile)
            || (m_settings.m_modAFInput == SSBModSettings::SSBModInputAF::SSBModInputNone));
    ui->morseKeyer->setEnabled((m_settings.m_modAFInput == SSBModSettings::SSBModInputAF::SSBModInputCWTone)
            || (m_settings.m_modAFInput == SSBModSettings::SSBModInputAF::SSBModInputNone));

    ui->tone->setChecked(m_settings.m_modAFInput == SSBModSettings::SSBModInputAF::SSBModInputTone);
    ui->mic->setChecked(m_settings.m_modAFInput == SSBModSettings::SSBModInputAF::SSBModInputAudio);
    ui->play->setChecked(m_settings.m_modAFInput == SSBModSettings::SSBModInputAF::SSBModInputFile);
    ui->morseKeyer->setChecked(m_settings.m_modAFInput == SSBModSettings::SSBModInputAF::SSBModInputCWTone);

    ui->feedbackEnable->setChecked(m_settings.m_feedbackAudioEnable);
    ui->feedbackVolume->setValue(roundf(m_settings.m_feedbackVolumeFactor * 100.0));
    ui->feedbackVolumeText->setText(QString("%1").arg(m_settings.m_feedbackVolumeFactor, 0, 'f', 2));

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void SSBModGUI::leaveEvent(QEvent* event)
{
	m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void SSBModGUI::enterEvent(EnterEventType* event)
{
	m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void SSBModGUI::audioSelect(const QPoint& p)
{
    qDebug("SSBModGUI::audioSelect");
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName, true); // true for input
    audioSelect.move(p);
    new DialogPositioner(&audioSelect, false);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        applySettings();
    }
}

void SSBModGUI::audioFeedbackSelect(const QPoint& p)
{
    qDebug("SSBModGUI::audioFeedbackSelect");
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_feedbackAudioDeviceName, false); // false for output
    audioSelect.move(p);
    new DialogPositioner(&audioSelect, false);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_feedbackAudioDeviceName = audioSelect.m_audioDeviceName;
        applySettings();
    }
}

void SSBModGUI::tick()
{
    double powDb = CalcDb::dbPower(m_ssbMod->getMagSq());
	m_channelPowerDbAvg(powDb);
	ui->channelPower->setText(tr("%1 dB").arg(m_channelPowerDbAvg.asDouble(), 0, 'f', 1));

    int audioSampleRate = m_ssbMod->getAudioSampleRate();

    if (audioSampleRate != m_audioSampleRate)
    {
        if (audioSampleRate < 0) {
            ui->mic->setColor(QColor("red"));
        } else {
            ui->mic->resetColor();
        }

        m_audioSampleRate = audioSampleRate;
    }

    int feedbackAudioSampleRate = m_ssbMod->getFeedbackAudioSampleRate();

    if (feedbackAudioSampleRate != m_feedbackAudioSampleRate)
    {
        if (feedbackAudioSampleRate < 0) {
            ui->feedbackEnable->setStyleSheet("QToolButton { background-color : red; }");
        } else {
            ui->feedbackEnable->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        }

        m_feedbackAudioSampleRate = feedbackAudioSampleRate;
    }

    if (((++m_tickCount & 0xf) == 0) && (m_settings.m_modAFInput == SSBModSettings::SSBModInputFile))
    {
        SSBMod::MsgConfigureFileSourceStreamTiming* message = SSBMod::MsgConfigureFileSourceStreamTiming::create();
        m_ssbMod->getInputMessageQueue()->push(message);
    }
}

void SSBModGUI::updateWithStreamData()
{
    QTime recordLength(0, 0, 0, 0);
    recordLength = recordLength.addSecs(m_recordLength);
    QString s_time = recordLength.toString("HH:mm:ss");
    ui->recordLengthText->setText(s_time);
    updateWithStreamTime();
}

void SSBModGUI::updateWithStreamTime()
{
    int t_sec = 0;
    int t_msec = 0;

    if (m_recordSampleRate > 0)
    {
        t_msec = ((m_samplesCount * 1000) / m_recordSampleRate) % 1000;
        t_sec = m_samplesCount / m_recordSampleRate;
    }

    QTime t(0, 0, 0, 0);
    t = t.addSecs(t_sec);
    t = t.addMSecs(t_msec);
    QString s_timems = t.toString("HH:mm:ss.zzz");
    QString s_time = t.toString("HH:mm:ss");
    ui->relTimeText->setText(s_timems);

    if (!m_enableNavTime)
    {
        float posRatio = (float) t_sec / (float) m_recordLength;
        ui->navTimeSlider->setValue((int) (posRatio * 100.0));
    }
}

void SSBModGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &SSBModGUI::on_deltaFrequency_changed);
    QObject::connect(ui->flipSidebands, &QPushButton::clicked, this, &SSBModGUI::on_flipSidebands_clicked);
    QObject::connect(ui->dsb, &QToolButton::toggled, this, &SSBModGUI::on_dsb_toggled);
    QObject::connect(ui->audioBinaural, &QToolButton::toggled, this, &SSBModGUI::on_audioBinaural_toggled);
    QObject::connect(ui->audioFlipChannels, &QToolButton::toggled, this, &SSBModGUI::on_audioFlipChannels_toggled);
    QObject::connect(ui->BW, &TickedSlider::valueChanged, this, &SSBModGUI::on_BW_valueChanged);
    QObject::connect(ui->lowCut, &TickedSlider::valueChanged, this, &SSBModGUI::on_lowCut_valueChanged);
    QObject::connect(ui->volume, &QDial::valueChanged, this, &SSBModGUI::on_volume_valueChanged);
    QObject::connect(ui->audioMute, &QToolButton::toggled, this, &SSBModGUI::on_audioMute_toggled);
    QObject::connect(ui->tone, &ButtonSwitch::toggled, this, &SSBModGUI::on_tone_toggled);
    QObject::connect(ui->toneFrequency, &QDial::valueChanged, this, &SSBModGUI::on_toneFrequency_valueChanged);
    QObject::connect(ui->mic, &ButtonSwitch::toggled, this, &SSBModGUI::on_mic_toggled);
    QObject::connect(ui->agc, &ButtonSwitch::toggled, this, &SSBModGUI::on_agc_toggled);
    QObject::connect(ui->cmpPreGain, &QDial::valueChanged, this, &SSBModGUI::on_cmpPreGain_valueChanged);
    QObject::connect(ui->cmpThreshold, &QDial::valueChanged, this, &SSBModGUI::on_cmpThreshold_valueChanged);
    QObject::connect(ui->play, &ButtonSwitch::toggled, this, &SSBModGUI::on_play_toggled);
    QObject::connect(ui->playLoop, &ButtonSwitch::toggled, this, &SSBModGUI::on_playLoop_toggled);
    QObject::connect(ui->morseKeyer, &ButtonSwitch::toggled, this, &SSBModGUI::on_morseKeyer_toggled);
    QObject::connect(ui->navTimeSlider, &QSlider::valueChanged, this, &SSBModGUI::on_navTimeSlider_valueChanged);
    QObject::connect(ui->showFileDialog, &QPushButton::clicked, this, &SSBModGUI::on_showFileDialog_clicked);
    QObject::connect(ui->feedbackEnable, &QToolButton::toggled, this, &SSBModGUI::on_feedbackEnable_toggled);
    QObject::connect(ui->feedbackVolume, &QDial::valueChanged, this, &SSBModGUI::on_feedbackVolume_valueChanged);
    QObject::connect(ui->spanLog2, &QSlider::valueChanged, this, &SSBModGUI::on_spanLog2_valueChanged);
}

void SSBModGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
