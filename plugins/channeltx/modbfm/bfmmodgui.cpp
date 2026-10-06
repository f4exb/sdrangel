///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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
#include <QColor>
#include <QFileDialog>
#include <QFileInfo>
#include <QTime>
#include <QDebug>

#include "device/deviceuiset.h"
#include "plugin/pluginapi.h"
#include "util/db.h"
#include "dsp/dspengine.h"
#include "dsp/cwkeyer.h"
#include "dsp/dspcommands.h"
#include "gui/crightclickenabler.h"
#include "gui/audioselectdialog.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "maincore.h"
#include "util/rds.h"

#include "ui_bfmmodgui.h"
#include "bfmmodgui.h"

BFMModGUI* BFMModGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    BFMModGUI* gui = new BFMModGUI(pluginAPI, deviceUISet, channelTx);
	return gui;
}

void BFMModGUI::destroy()
{
    delete this;
}

void BFMModGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(QStringList(), true);
}

QByteArray BFMModGUI::serialize() const
{
    return m_settings.serialize();
}

bool BFMModGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data)) {
        displaySettings();
        applySettings(QStringList(), true);
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

bool BFMModGUI::handleMessage(const Message& message)
{
    if (BFMMod::MsgReportFileSourceStreamData::match(message))
    {
        m_recordSampleRate = ((BFMMod::MsgReportFileSourceStreamData&)message).getSampleRate();
        m_recordLength = ((BFMMod::MsgReportFileSourceStreamData&)message).getRecordLength();
        m_samplesCount = 0;
        updateWithStreamData();
        return true;
    }
    else if (BFMMod::MsgReportFileSourceStreamTiming::match(message))
    {
        m_samplesCount = ((BFMMod::MsgReportFileSourceStreamTiming&)message).getSamplesCount();
        updateWithStreamTime();
        return true;
    }
    else if (BFMMod::MsgConfigureBFMMod::match(message))
    {
        const BFMMod::MsgConfigureBFMMod& cfg = (BFMMod::MsgConfigureBFMMod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
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
    else if (DSPSignalNotification::match(message))
    {
        const DSPSignalNotification& notif = (const DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        ui->deltaFrequency->setValueRange(false, 8, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();
        updateSampleRateStatus();
        return true;
    }
    else
    {
        return false;
    }
}

void BFMModGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
	applySettings(QStringList("inputFrequencyOffset"));
}

void BFMModGUI::handleSourceMessages()
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

void BFMModGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings(QStringList("inputFrequencyOffset"));
}

void BFMModGUI::on_rfBW_currentIndexChanged(int index)
{
    float rfBW = BFMModSettings::getRFBW(index);
	m_channelMarker.setBandwidth(rfBW);
	m_settings.m_rfBandwidth = rfBW;
	updateSampleRateStatus();
	applySettings(QStringList("rfBandwidth"));
}

// The channel sample rate cannot exceed the device's. When it is below the RF
// bandwidth, the FM sidebands are cut and, below about 120 kHz, the 57 kHz RDS
// subcarrier aliases.
void BFMModGUI::updateSampleRateStatus()
{
    if ((m_basebandSampleRate > 1) && (m_basebandSampleRate < m_settings.m_rfBandwidth)) {
        setStatusText(tr("Sample rate must be >= %1 Hz (currently %2 Hz)").arg(m_settings.m_rfBandwidth, 0, 'f', 0).arg(m_basebandSampleRate));
    } else {
        setStatusText("");
    }
}

void BFMModGUI::on_afBW_valueChanged(int value)
{
	ui->afBWText->setText(QString("%1k").arg(value));
	m_settings.m_afBandwidth = value * 1000.0;
	applySettings(QStringList("afBandwidth"));
}

void BFMModGUI::on_fmDev_valueChanged(int value)
{
	ui->fmDevText->setText(QString("%1%2k").arg(QChar(0xB1, 0x00)).arg(value));
	m_settings.m_fmDeviation = value * 1000.0;
	applySettings(QStringList("fmDeviation"));
}

void BFMModGUI::on_volume_valueChanged(int value)
{
	ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
	m_settings.m_volumeFactor = value / 10.0;
	applySettings(QStringList("volumeFactor"));
}

void BFMModGUI::on_stereo_toggled(bool checked)
{
    m_settings.m_audioStereo = checked;
    applySettings(QStringList("audioStereo"));
}

void BFMModGUI::on_preEmphasis_currentIndexChanged(int index)
{
    m_settings.m_preEmphasis = index;
    applySettings(QStringList("preEmphasis"));
}

void BFMModGUI::on_rdsActive_toggled(bool checked)
{
    m_settings.m_rdsActive = checked;
    applySettings(QStringList("rdsActive"));
}

void BFMModGUI::on_pilotLevel_valueChanged(double value)
{
    m_settings.m_pilotLevel = value / 100.0;
    applySettings(QStringList("pilotLevel"));
}

void BFMModGUI::on_rdsLevel_valueChanged(double value)
{
    m_settings.m_rdsLevel = value / 100.0;
    applySettings(QStringList("rdsLevel"));
}

void BFMModGUI::on_rdsPTY_currentIndexChanged(int index)
{
    m_settings.m_rdsPTY = index;
    applySettings(QStringList("rdsPTY"));
}

void BFMModGUI::on_rdsPI_editingFinished()
{
    // editingFinished is also emitted when focus leaves an unchanged field
    bool ok = false;
    const uint value = ui->rdsPI->text().toUInt(&ok, 16);
    const bool changed = ok && (value <= 0xffffU) && (value != m_settings.m_rdsPI);
    if (changed) {
        m_settings.m_rdsPI = quint16(value);
    }
    ui->rdsPI->setText(QString("%1").arg(m_settings.m_rdsPI, 4, 16, QChar('0')).toUpper());
    if (changed) {
        applySettings(QStringList("rdsPI"));
    }
}

void BFMModGUI::on_rdsPS_editingFinished()
{
    const QString ps = ui->rdsPS->text().left(8);
    if (ps != m_settings.m_rdsPS)
    {
        m_settings.m_rdsPS = ps;
        applySettings(QStringList("rdsPS"));
    }
}

void BFMModGUI::on_rdsRadioText_editingFinished()
{
    const QString radioText = ui->rdsRadioText->text().left(64);
    if (radioText != m_settings.m_rdsRadioText)
    {
        m_settings.m_rdsRadioText = radioText;
        applySettings(QStringList("rdsRadioText"));
    }
}

void BFMModGUI::on_toneFrequency_valueChanged(int value)
{
    ui->toneFrequencyText->setText(QString("%1k").arg(value / 100.0, 0, 'f', 2));
    m_settings.m_toneFrequency = value * 10.0;
    applySettings(QStringList("toneFrequency"));
}

void BFMModGUI::on_channelMute_toggled(bool checked)
{
    m_settings.m_channelMute = checked;
	applySettings(QStringList("channelMute"));
}

void BFMModGUI::on_playLoop_toggled(bool checked)
{
    m_settings.m_playLoop = checked;
	applySettings(QStringList("playLoop"));
}

void BFMModGUI::on_play_toggled(bool checked)
{
    selectInput(BFMModSettings::BFMModInputFile, checked);
}

void BFMModGUI::on_tone_toggled(bool checked)
{
    selectInput(BFMModSettings::BFMModInputTone, checked);
}

void BFMModGUI::on_morseKeyer_toggled(bool checked)
{
    selectInput(BFMModSettings::BFMModInputCWTone, checked);
}

void BFMModGUI::on_mic_toggled(bool checked)
{
    selectInput(BFMModSettings::BFMModInputAudio, checked);
}

// The input buttons act as a radio group: selecting one deselects the others.
// Unlike a QButtonGroup, the selected input can also be switched off, leaving none.
void BFMModGUI::selectInput(BFMModSettings::BFMModInputAF input, bool checked)
{
    m_settings.m_modAFInput = checked ? input : BFMModSettings::BFMModInputNone;
    displayInput();
    applySettings(QStringList("modAFInput"));
}

void BFMModGUI::displayInput()
{
    const BFMModSettings::BFMModInputAF input = m_settings.m_modAFInput;
    const std::pair<ButtonSwitch*, BFMModSettings::BFMModInputAF> buttons[] = {
        {ui->mic, BFMModSettings::BFMModInputAudio},
        {ui->play, BFMModSettings::BFMModInputFile},
        {ui->tone, BFMModSettings::BFMModInputTone},
        {ui->morseKeyer, BFMModSettings::BFMModInputCWTone}
    };

    for (const auto& button : buttons)
    {
        button.first->blockSignals(true);
        button.first->setChecked(input == button.second);
        button.first->blockSignals(false);
    }

    // While the file is playing, the slider shows its position rather than seeking
    const bool playing = input == BFMModSettings::BFMModInputFile;
    ui->navTimeSlider->setEnabled(!playing);
    m_enableNavTime = !playing;

    // Show the controls for the selected input
    if (playing) {
        ui->tabWidget->setCurrentWidget(ui->tabPlay);
    } else if (input == BFMModSettings::BFMModInputCWTone) {
        ui->tabWidget->setCurrentWidget(ui->tabMorseKeyer);
    }
}

void BFMModGUI::on_feedbackEnable_toggled(bool checked)
{
    m_settings.m_feedbackAudioEnable = checked;
    applySettings(QStringList("feedbackAudioEnable"));
}

void BFMModGUI::on_feedbackVolume_valueChanged(int value)
{
    ui->feedbackVolumeText->setText(QString("%1").arg(value / 100.0, 0, 'f', 2));
    m_settings.m_feedbackVolumeFactor = value / 100.0;
    applySettings(QStringList("feedbackVolumeFactor"));
}

void BFMModGUI::on_navTimeSlider_valueChanged(int value)
{
    if (m_enableNavTime && ((value >= 0) && (value <= 100)))
    {
        BFMMod::MsgConfigureFileSourceSeek* message = BFMMod::MsgConfigureFileSourceSeek::create(value);
        m_bfmMod->getInputMessageQueue()->push(message);
    }
}

void BFMModGUI::on_showFileDialog_clicked(bool checked)
{
    (void) checked;
    // Start in the directory of the current file, if any
    const QString directory = m_settings.m_fileName.isEmpty() ? QString(".") : QFileInfo(m_settings.m_fileName).absolutePath();
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Open audio file"), directory, tr("Audio files (*.wav *.raw)"), 0, QFileDialog::DontUseNativeDialog);

    if (fileName != "")
    {
        m_settings.m_fileName = fileName;
        displayFileName();
        applySettings(QStringList("fileName"));
    }
}

void BFMModGUI::displayFileName()
{
    ui->recordFileText->setText(m_settings.m_fileName.isEmpty() ? QString("...") : m_settings.m_fileName);
    ui->recordFileText->setToolTip(m_settings.m_fileName);
}

void BFMModGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings(QStringList());
}

void BFMModGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_bfmMod->getNumberOfDeviceStreams());
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

        applySettings(QStringList({"title", "rgbColor", "useReverseAPI",
            "reverseAPIAddress", "reverseAPIPort", "reverseAPIDeviceIndex",
            "reverseAPIChannelIndex", "streamIndex"}));
    }

    resetContextMenuType();
}

BFMModGUI::BFMModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent) :
	ChannelGUI(parent),
	ui(new Ui::BFMModGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
	m_doApplySettings(true),
    m_recordLength(0),
    m_recordSampleRate(48000),
    m_samplesCount(0),
    m_audioSampleRate(-1),
    m_feedbackAudioSampleRate(-1),
    m_tickCount(0),
    m_enableNavTime(false)
{
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channeltx/modbfm/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    blockApplySettings(true);

    ui->rfBW->clear();
    for (int i = 0; i < BFMModSettings::m_nbRfBW; i++) {
        ui->rfBW->addItem(QString("%1").arg(BFMModSettings::getRFBW(i) / 1000.0, 0, 'f', 2));
    }
    ui->rfBW->setCurrentIndex(BFMModSettings::getRFBWIndex(m_settings.m_rfBandwidth));

    // Same names as the programme type shown by the BFM demodulator. Index is the PTY code.
    for (int i = 0; i < RDS::m_nbProgrammeTypes; i++) {
        ui->rdsPTY->addItem(QString::fromUtf8(RDS::m_programmeTypes[i]));
    }

    blockApplySettings(false);

	connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

	m_bfmMod = (BFMMod*) channelTx;
	m_bfmMod->setMessageQueueToGUI(getInputMessageQueue());

	connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    CRightClickEnabler *audioMuteRightClickEnabler = new CRightClickEnabler(ui->mic);
    connect(audioMuteRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect(const QPoint &)));

    CRightClickEnabler *feedbackRightClickEnabler = new CRightClickEnabler(ui->feedbackEnable);
    connect(feedbackRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioFeedbackSelect(const QPoint &)));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 8, -99999999, 99999999);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setSourceOrSinkStream(false);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

	m_deviceUISet->addChannelMarker(&m_channelMarker);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));

    ui->cwKeyerGUI->setCWKeyer(m_bfmMod->getCWKeyer());

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setCWKeyerGUI(ui->cwKeyerGUI);
    m_settings.setRollupState(&m_rollupState);

	connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
    m_bfmMod->setLevelMeter(ui->volumeMeter);

	displaySettings();
    makeUIConnections();
    applySettings(QStringList(), true);
    DialPopup::addPopupsToChildDials(this);
    m_resizer.enableChildMouseTracking();
}

BFMModGUI::~BFMModGUI()
{
	delete ui;
}

void BFMModGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void BFMModGUI::applySettings(const QStringList& settingsKeys, bool force)
{
	if (m_doApplySettings)
	{
		BFMMod::MsgConfigureBFMMod *msgConf = BFMMod::MsgConfigureBFMMod::create(settingsKeys, m_settings, force);
		m_bfmMod->getInputMessageQueue()->push(msgConf);
	}
}

void BFMModGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());
    updateIndexLabel();

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->rfBW->setCurrentIndex(BFMModSettings::getRFBWIndex(m_settings.m_rfBandwidth));

    ui->afBWText->setText(QString("%1k").arg(m_settings.m_afBandwidth / 1000.0));
    ui->afBW->setValue(m_settings.m_afBandwidth / 1000.0);

    ui->fmDevText->setText(QString("%1%2k").arg(QChar(0xB1, 0x00)).arg(m_settings.m_fmDeviation / 1000.0));
    ui->fmDev->setValue(m_settings.m_fmDeviation / 1000.0);

    ui->volumeText->setText(QString("%1").arg(m_settings.m_volumeFactor, 0, 'f', 1));
    ui->volume->setValue(m_settings.m_volumeFactor * 10.0);
    ui->stereo->setChecked(m_settings.m_audioStereo);
    ui->preEmphasis->setCurrentIndex(m_settings.m_preEmphasis);
    ui->rdsActive->setChecked(m_settings.m_rdsActive);
    ui->pilotLevel->setValue(m_settings.m_pilotLevel * 100.0);
    ui->rdsLevel->setValue(m_settings.m_rdsLevel * 100.0);
    ui->rdsPTY->setCurrentIndex(m_settings.m_rdsPTY);
    ui->rdsPI->setText(QString("%1").arg(m_settings.m_rdsPI, 4, 16, QChar('0')).toUpper());
    ui->rdsPS->setText(m_settings.m_rdsPS);
    ui->rdsRadioText->setText(m_settings.m_rdsRadioText);

    ui->toneFrequencyText->setText(QString("%1k").arg(m_settings.m_toneFrequency / 1000.0, 0, 'f', 2));
    ui->toneFrequency->setValue(m_settings.m_toneFrequency / 10.0);

    ui->channelMute->setChecked(m_settings.m_channelMute);
    ui->playLoop->setChecked(m_settings.m_playLoop);
    displayFileName();

    displayInput();

    ui->feedbackEnable->setChecked(m_settings.m_feedbackAudioEnable);
    ui->feedbackVolume->setValue(roundf(m_settings.m_feedbackVolumeFactor * 100.0));
    ui->feedbackVolumeText->setText(QString("%1").arg(m_settings.m_feedbackVolumeFactor, 0, 'f', 2));

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    updateSampleRateStatus();
    blockApplySettings(false);
}

void BFMModGUI::leaveEvent(QEvent* event)
{
	m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void BFMModGUI::enterEvent(EnterEventType* event)
{
	m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void BFMModGUI::audioSelect(const QPoint& p)
{
    qDebug("BFMModGUI::audioSelect");
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName, true); // true for input
    audioSelect.move(p);
    new DialogPositioner(&audioSelect, false);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        applySettings(QStringList("audioDeviceName"));
    }
}

void BFMModGUI::audioFeedbackSelect(const QPoint& p)
{
    qDebug("BFMModGUI::audioFeedbackSelect");
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_feedbackAudioDeviceName, false); // false for output
    audioSelect.move(p);
    new DialogPositioner(&audioSelect, false);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_feedbackAudioDeviceName = audioSelect.m_audioDeviceName;
        applySettings(QStringList("feedbackAudioDeviceName"));
    }
}

void BFMModGUI::tick()
{
    double powDb = CalcDb::dbPower(m_bfmMod->getMagSq());
	m_channelPowerDbAvg(powDb);
	ui->channelPower->setText(tr("%1 dB").arg(m_channelPowerDbAvg.asDouble(), 0, 'f', 1));

    int audioSampleRate = m_bfmMod->getAudioSampleRate();

    if (audioSampleRate != m_audioSampleRate)
    {
        if (audioSampleRate < 0) {
            ui->mic->setColor(QColor("red"));
        } else {
            ui->mic->resetColor();
        }

        m_audioSampleRate = audioSampleRate;
    }

    int feedbackAudioSampleRate = m_bfmMod->getFeedbackAudioSampleRate();

    if (feedbackAudioSampleRate != m_feedbackAudioSampleRate)
    {
        if (feedbackAudioSampleRate < 0) {
            ui->feedbackEnable->setStyleSheet("QToolButton { background-color : red; }");
        } else {
            ui->feedbackEnable->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        }

        m_feedbackAudioSampleRate = feedbackAudioSampleRate;
    }

    if (((++m_tickCount & 0xf) == 0) && (m_settings.m_modAFInput == BFMModSettings::BFMModInputFile))
    {
        BFMMod::MsgConfigureFileSourceStreamTiming* message = BFMMod::MsgConfigureFileSourceStreamTiming::create();
        m_bfmMod->getInputMessageQueue()->push(message);
    }
}

void BFMModGUI::updateWithStreamData()
{
    QTime recordLength(0, 0, 0, 0);
    recordLength = recordLength.addSecs(m_recordLength);
    QString s_time = recordLength.toString("HH:mm:ss");
    ui->recordLengthText->setText(s_time);
    updateWithStreamTime();
}

void BFMModGUI::updateWithStreamTime()
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

    if (!m_enableNavTime && (m_recordLength > 0))
    {
        float posRatio = (float) t_sec / (float) m_recordLength;
        ui->navTimeSlider->setValue((int) (posRatio * 100.0));
    }
}

void BFMModGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &BFMModGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBW, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BFMModGUI::on_rfBW_currentIndexChanged);
    QObject::connect(ui->afBW, &QSlider::valueChanged, this, &BFMModGUI::on_afBW_valueChanged);
    QObject::connect(ui->fmDev, &QSlider::valueChanged, this, &BFMModGUI::on_fmDev_valueChanged);
    QObject::connect(ui->toneFrequency, &QDial::valueChanged, this, &BFMModGUI::on_toneFrequency_valueChanged);
    QObject::connect(ui->volume, &QDial::valueChanged, this, &BFMModGUI::on_volume_valueChanged);
    QObject::connect(ui->stereo, &ButtonSwitch::toggled, this, &BFMModGUI::on_stereo_toggled);
    QObject::connect(ui->preEmphasis, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BFMModGUI::on_preEmphasis_currentIndexChanged);
    QObject::connect(ui->rdsActive, &ButtonSwitch::toggled, this, &BFMModGUI::on_rdsActive_toggled);
    QObject::connect(ui->pilotLevel, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BFMModGUI::on_pilotLevel_valueChanged);
    QObject::connect(ui->rdsLevel, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BFMModGUI::on_rdsLevel_valueChanged);
    QObject::connect(ui->rdsPTY, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BFMModGUI::on_rdsPTY_currentIndexChanged);
    QObject::connect(ui->rdsPI, &QLineEdit::editingFinished, this, &BFMModGUI::on_rdsPI_editingFinished);
    QObject::connect(ui->rdsPS, &QLineEdit::editingFinished, this, &BFMModGUI::on_rdsPS_editingFinished);
    QObject::connect(ui->rdsRadioText, &QLineEdit::editingFinished, this, &BFMModGUI::on_rdsRadioText_editingFinished);
    QObject::connect(ui->channelMute, &QToolButton::toggled, this, &BFMModGUI::on_channelMute_toggled);
    QObject::connect(ui->tone, &ButtonSwitch::toggled, this, &BFMModGUI::on_tone_toggled);
    QObject::connect(ui->morseKeyer, &ButtonSwitch::toggled, this, &BFMModGUI::on_morseKeyer_toggled);
    QObject::connect(ui->mic, &ButtonSwitch::toggled, this, &BFMModGUI::on_mic_toggled);
    QObject::connect(ui->play, &ButtonSwitch::toggled, this, &BFMModGUI::on_play_toggled);
    QObject::connect(ui->playLoop, &ButtonSwitch::toggled, this, &BFMModGUI::on_playLoop_toggled);
    QObject::connect(ui->navTimeSlider, &QSlider::valueChanged, this, &BFMModGUI::on_navTimeSlider_valueChanged);
    QObject::connect(ui->showFileDialog, &QPushButton::clicked, this, &BFMModGUI::on_showFileDialog_clicked);
    QObject::connect(ui->feedbackEnable, &QToolButton::toggled, this, &BFMModGUI::on_feedbackEnable_toggled);
    QObject::connect(ui->feedbackVolume, &QDial::valueChanged, this, &BFMModGUI::on_feedbackVolume_valueChanged);
}

void BFMModGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
