///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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
#include <QRegularExpression>

#include "device/deviceuiset.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/scopevisxy.h"
#include "gui/crightclickenabler.h"
#include "gui/audioselectdialog.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "maincore.h"

#include "ui_m17modgui.h"
#include "m17modgui.h"


M17ModGUI* M17ModGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    M17ModGUI* gui = new M17ModGUI(pluginAPI, deviceUISet, channelTx);
	return gui;
}

void M17ModGUI::destroy()
{
    delete this;
}

void M17ModGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(QList<QString>(), true);
}

QByteArray M17ModGUI::serialize() const
{
    return m_settings.serialize();
}

bool M17ModGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data)) {
        displaySettings();
        applySettings(QList<QString>(), true);
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

bool M17ModGUI::handleMessage(const Message& message)
{
    if (M17Mod::MsgReportFileSourceStreamData::match(message))
    {
        m_recordSampleRate = ((M17Mod::MsgReportFileSourceStreamData&)message).getSampleRate();
        m_recordLength = ((M17Mod::MsgReportFileSourceStreamData&)message).getRecordLength();
        m_samplesCount = 0;
        updateWithStreamData();
        return true;
    }
    else if (M17Mod::MsgReportFileSourceStreamTiming::match(message))
    {
        m_samplesCount = ((M17Mod::MsgReportFileSourceStreamTiming&)message).getSamplesCount();
        updateWithStreamTime();
        return true;
    }
    else if (M17Mod::MsgConfigureM17Mod::match(message))
    {
        const M17Mod::MsgConfigureM17Mod& cfg = (M17Mod::MsgConfigureM17Mod&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        blockApplySettings(true);
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
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
    else
    {
        return false;
    }
}

void M17ModGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
	applySettings(QList<QString>{"inputFrequencyOffset"});
}

void M17ModGUI::handleSourceMessages()
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

void M17ModGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings(QList<QString>{"inputFrequencyOffset"});
}

void M17ModGUI::on_rfBW_valueChanged(int value)
{
	ui->rfBWText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
	m_settings.m_rfBandwidth = value * 100.0;
	m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);

    applySettings(QList<QString>{"rfBandwidth"});
}

void M17ModGUI::on_fmDev_valueChanged(int value)
{
	ui->fmDevText->setText(QString("%1%2k").arg(QChar(0xB1, 0x00)).arg(value / 10.0, 0, 'f', 1));
	m_settings.m_fmDeviation = value * 200.0;
	applySettings(QList<QString>{"fmDeviation"});
}

void M17ModGUI::on_volume_valueChanged(int value)
{
	ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
	m_settings.m_volumeFactor = value / 10.0;
	applySettings(QList<QString>{"volumeFactor"});
}

void M17ModGUI::on_toneFrequency_valueChanged(int value)
{
    ui->toneFrequencyText->setText(QString("%1k").arg(value / 100.0, 0, 'f', 2));
    m_settings.m_toneFrequency = value * 10.0;
    applySettings(QList<QString>{"toneFrequency"});
}

void M17ModGUI::on_fmAudio_toggled(bool checked)
{
    m_fmAudioMode = checked;

    if ((checked) && (m_settings.m_m17Mode == M17ModSettings::M17Mode::M17ModeM17Audio))
    {
        m_settings.m_m17Mode = M17ModSettings::M17Mode::M17ModeFMAudio;
        applySettings(QList<QString>{"m17Mode"});
    }
    else if ((!checked) && (m_settings.m_m17Mode == M17ModSettings::M17Mode::M17ModeFMAudio))
    {
        m_settings.m_m17Mode = M17ModSettings::M17Mode::M17ModeM17Audio;
        applySettings(QList<QString>{"m17Mode"});
    }
}

void M17ModGUI::on_channelMute_toggled(bool checked)
{
    m_settings.m_channelMute = checked;
	applySettings(QList<QString>{"channelMute"});
}

void M17ModGUI::on_playLoop_toggled(bool checked)
{
    m_settings.m_playLoop = checked;
	applySettings(QList<QString>{"playLoop"});
}

void M17ModGUI::on_play_toggled(bool checked)
{
    m_settings.m_audioType = checked ? M17ModSettings::AudioFile : M17ModSettings::AudioNone;
    m_settings.m_m17Mode = checked ?
        m_fmAudioMode ?
            M17ModSettings::M17Mode::M17ModeFMAudio
            : M17ModSettings::M17Mode::M17ModeM17Audio
        : M17ModSettings::M17ModeNone;
    displayModes();
    applySettings(QList<QString>{"audioType", "m17Mode"});
    ui->navTimeSlider->setEnabled(!checked);
    m_enableNavTime = !checked;
}

void M17ModGUI::on_tone_toggled(bool checked)
{
    m_settings.m_m17Mode = checked ? M17ModSettings::M17ModeFMTone : M17ModSettings::M17ModeNone;
    displayModes();
    applySettings(QList<QString>{"m17Mode"});
}

void M17ModGUI::on_mic_toggled(bool checked)
{
    m_settings.m_audioType = checked ? M17ModSettings::AudioInput : M17ModSettings::AudioNone;
    m_settings.m_m17Mode = checked ?
        m_fmAudioMode ?
            M17ModSettings::M17Mode::M17ModeFMAudio
            : M17ModSettings::M17Mode::M17ModeM17Audio
        : M17ModSettings::M17ModeNone;
    displayModes();
    applySettings(QList<QString>{"audioType", "m17Mode"});
}

void M17ModGUI::on_feedbackEnable_toggled(bool checked)
{
    m_settings.m_feedbackAudioEnable = checked;
    applySettings(QList<QString>{"feedbackAudioEnable"});
}

void M17ModGUI::on_feedbackVolume_valueChanged(int value)
{
    ui->feedbackVolumeText->setText(QString("%1").arg(value / 100.0, 0, 'f', 2));
    m_settings.m_feedbackVolumeFactor = value / 100.0;
    applySettings(QList<QString>{"feedbackVolumeFactor"});
}

void M17ModGUI::on_navTimeSlider_valueChanged(int value)
{
    if (m_enableNavTime && ((value >= 0) && (value <= 100)))
    {
        int t_sec = (m_recordLength * value) / 100;
        QTime t(0, 0, 0, 0);
        t = t.addSecs(t_sec);

        M17Mod::MsgConfigureFileSourceSeek* message = M17Mod::MsgConfigureFileSourceSeek::create(value);
        m_m17Mod->getInputMessageQueue()->push(message);
    }
}

void M17ModGUI::on_showFileDialog_clicked(bool checked)
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

void M17ModGUI::on_packetMode_toggled(bool checked)
{
    m_settings.m_m17Mode = checked ? M17ModSettings::M17ModeM17Packet : M17ModSettings::M17ModeNone;
    displayModes();
    applySettings(QList<QString>{"m17Mode"});
}

void M17ModGUI::on_bertMode_toggled(bool checked)
{
    m_settings.m_m17Mode = checked ? M17ModSettings::M17ModeM17BERT : M17ModSettings::M17ModeNone;
    displayModes();
    applySettings(QList<QString>{"m17Mode"});
}

void M17ModGUI::on_sendPacket_clicked(bool)
{
    m_m17Mod->sendPacket();
}

void M17ModGUI::on_loopPacket_toggled(bool checked)
{
    m_settings.m_loopPacket = checked;
    applySettings(QList<QString>{"loopPacket"});
}

void M17ModGUI::on_loopPacketInterval_valueChanged(int value)
{
    ui->loopPacketIntervalText->setText(tr("%1").arg(value));
    m_settings.m_loopPacketInterval = value;
    (void) value;
    applySettings(QList<QString>{"loopPacketInterval"});
}

void M17ModGUI::on_packetDataWidget_currentChanged(int index)
{
    m_settings.m_packetType = indexToPacketType(index);
    applySettings(QList<QString>{"packetType"});
}

void M17ModGUI::on_source_editingFinished()
{
    m_settings.m_sourceCall = ui->source->text();
    applySettings(QList<QString>{"sourceCall"});
}

void M17ModGUI::on_destination_editingFinished()
{
    m_settings.m_destCall = ui->destination->text();
    applySettings(QList<QString>{"destCall"});
}

void M17ModGUI::on_insertPosition_toggled(bool checked)
{
    m_settings.m_insertPosition = checked;
    applySettings(QList<QString>{"insertPosition"});
}

void M17ModGUI::on_can_valueChanged(int value)
{
    m_settings.m_can = value;
    applySettings(QList<QString>{"can"});
}

void M17ModGUI::on_smsText_editingFinished()
{
    m_settings.m_smsText = ui->smsText->toPlainText();
    applySettings(QList<QString>{"smsText"});
}

void M17ModGUI::on_aprsFromText_editingFinished()
{
    m_settings.m_aprsCallsign = ui->aprsFromText->text();
    applySettings(QList<QString>{"aprsCallsign"});
}

void M17ModGUI::on_aprsTo_currentTextChanged(const QString &text)
{
    m_settings.m_aprsTo = text;
    applySettings(QList<QString>{"aprsTo"});
}

void M17ModGUI::on_aprsVia_currentTextChanged(const QString &text)
{
    m_settings.m_aprsVia = text;
    applySettings(QList<QString>{"aprsVia"});
}

void M17ModGUI::on_aprsData_editingFinished()
{
    m_settings.m_aprsData = ui->aprsData->text();
    applySettings(QList<QString>{"aprsData"});
}

void M17ModGUI::on_aprsInsertPosition_toggled(bool checked)
{
    m_settings.m_aprsInsertPosition = checked;
    applySettings(QList<QString>{"aprsInsertPosition"});
}

void M17ModGUI::configureFileName()
{
    qDebug() << "M17ModGUI::configureFileName: " << m_fileName.toStdString().c_str();
    M17Mod::MsgConfigureFileSourceName* message = M17Mod::MsgConfigureFileSourceName::create(m_fileName);
    m_m17Mod->getInputMessageQueue()->push(message);
}

void M17ModGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
}

void M17ModGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_m17Mod->getNumberOfDeviceStreams());
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

        QList<QString> settingsKeys({
            "m_rgbColor",
            "title",
            "useReverseAPI",
            "reverseAPIAddress",
            "reverseAPIPort",
            "reverseAPIDeviceIndex",
            "reverseAPIChannelIndex"
        });

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
            settingsKeys.append("streamIndex");
            m_channelMarker.clearStreamIndexes();
            m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
            updateIndexLabel();
        }

        applySettings(settingsKeys);
    }

    resetContextMenuType();
}

M17ModGUI::M17ModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent) :
	ChannelGUI(parent),
	ui(new Ui::M17ModGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
	m_doApplySettings(true),
    m_fmAudioMode(false),
    m_recordLength(0),
    m_recordSampleRate(48000),
    m_samplesCount(0),
    m_audioSampleRate(-1),
    m_feedbackAudioSampleRate(-1),
    m_tickCount(0),
    m_enableNavTime(false),
    m_dcsCodeValidator(QRegularExpression("[0-7]{1,3}"))
{
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channeltx/modm17/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

	m_m17Mod = (M17Mod*) channelTx;
	m_m17Mod->setMessageQueueToGUI(getInputMessageQueue());

	connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    CRightClickEnabler *audioMuteRightClickEnabler = new CRightClickEnabler(ui->mic);
    connect(audioMuteRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect()));

    CRightClickEnabler *feedbackRightClickEnabler = new CRightClickEnabler(ui->feedbackEnable);
    connect(feedbackRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioFeedbackSelect()));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::red);
    m_channelMarker.setBandwidth(12500);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("M17 Modulator");
    m_channelMarker.setSourceOrSinkStream(false);
    m_channelMarker.blockSignals(false);
	m_channelMarker.setVisible(true); // activate signal on the last setting only

	m_deviceUISet->addChannelMarker(&m_channelMarker);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));

    ui->play->setEnabled(false);
    ui->play->setChecked(false);
    ui->tone->setChecked(false);
    ui->mic->setChecked(false);

	connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
    m_m17Mod->setLevelMeter(ui->volumeMeter);

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    displaySettings();
    makeUIConnections();
    applySettings(QList<QString>{"channelMarker", "rollupState"});
    DialPopup::addPopupsToChildDials(this);
}

M17ModGUI::~M17ModGUI()
{
	delete ui;
}

void M17ModGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void M17ModGUI::applySettings(const QList<QString>& settingsKeys, bool force)
{
	if (m_doApplySettings)
	{
		M17Mod::MsgConfigureM17Mod *msg = M17Mod::MsgConfigureM17Mod::create(m_settings, settingsKeys, force);
		m_m17Mod->getInputMessageQueue()->push(msg);
	}
}

void M17ModGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());
    updateIndexLabel();

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->rfBWText->setText(QString("%1k").arg(m_settings.m_rfBandwidth / 1000.0, 0, 'f', 1));
    ui->rfBW->setValue(m_settings.m_rfBandwidth / 100.0);

    ui->fmDevText->setText(QString("%1%2k").arg(QChar(0xB1, 0x00)).arg(m_settings.m_fmDeviation / 2000.0, 0, 'f', 1));
    ui->fmDev->setValue(m_settings.m_fmDeviation / 200.0);

    ui->volumeText->setText(QString("%1").arg(m_settings.m_volumeFactor, 0, 'f', 1));
    ui->volume->setValue(m_settings.m_volumeFactor * 10.0);

    ui->toneFrequencyText->setText(QString("%1k").arg(m_settings.m_toneFrequency / 1000.0, 0, 'f', 2));
    ui->toneFrequency->setValue(m_settings.m_toneFrequency / 10.0);

    ui->channelMute->setChecked(m_settings.m_channelMute);
    ui->playLoop->setChecked(m_settings.m_playLoop);

    displayModes();
    ui->fmAudio->setChecked(m_fmAudioMode);
    ui->packetDataWidget->setCurrentIndex(packetTypeToIndex(m_settings.m_packetType));

    ui->feedbackEnable->setChecked(m_settings.m_feedbackAudioEnable);
    ui->feedbackVolume->setValue(roundf(m_settings.m_feedbackVolumeFactor * 100.0));
    ui->feedbackVolumeText->setText(QString("%1").arg(m_settings.m_feedbackVolumeFactor, 0, 'f', 2));

    ui->source->setText(m_settings.m_sourceCall);
    ui->destination->setText(m_settings.m_destCall);
    ui->insertPosition->setChecked(m_settings.m_insertPosition);
    ui->can->setValue(m_settings.m_can);

    ui->loopPacket->setChecked(m_settings.m_loopPacket);
    ui->loopPacketInterval->setValue(m_settings.m_loopPacketInterval);
    ui->loopPacketIntervalText->setText(tr("%1").arg(m_settings.m_loopPacketInterval));

    ui->smsText->setText(m_settings.m_smsText);

    ui->aprsFromText->setText(m_settings.m_aprsCallsign);
    ui->aprsData->setText(m_settings.m_aprsData);
    ui->aprsTo->lineEdit()->setText(m_settings.m_aprsTo);
    ui->aprsVia->lineEdit()->setText(m_settings.m_aprsVia);
    ui->aprsInsertPosition->setChecked(m_settings.m_aprsInsertPosition);

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void M17ModGUI::displayModes()
{
    qDebug("M17ModGUI::displayModes: m_m17Mode: %d m_audioType: %d",
        (int) m_settings.m_m17Mode, (int) m_settings.m_audioType);

    if (m_settings.m_m17Mode ==  M17ModSettings::M17Mode::M17ModeM17Packet)
    {
        ui->packetMode->setChecked(true);
        ui->packetMode->setEnabled(true);
        ui->bertMode->setChecked(false);
        ui->tone->setChecked(false);
        ui->mic->setChecked(false);
        ui->play->setChecked(false);
        ui->bertMode->setEnabled(false);
        ui->tone->setEnabled(false);
        ui->mic->setEnabled(false);
        ui->play->setEnabled(false);
    }
    if (m_settings.m_m17Mode ==  M17ModSettings::M17Mode::M17ModeM17BERT)
    {
        ui->bertMode->setChecked(true);
        ui->bertMode->setEnabled(true);
        ui->tone->setChecked(false);
        ui->mic->setChecked(false);
        ui->play->setChecked(false);
        ui->tone->setEnabled(false);
        ui->mic->setEnabled(false);
        ui->play->setEnabled(false);
    }
    else if (m_settings.m_m17Mode ==  M17ModSettings::M17Mode::M17ModeFMTone)
    {
        ui->tone->setChecked(true);
        ui->tone->setEnabled(true);
        ui->packetMode->setChecked(false);
        ui->bertMode->setChecked(false);
        ui->mic->setChecked(false);
        ui->play->setChecked(false);
        ui->packetMode->setEnabled(false);
        ui->bertMode->setEnabled(true);
        ui->mic->setEnabled(false);
        ui->play->setEnabled(false);
    }
    else if ((m_settings.m_m17Mode ==  M17ModSettings::M17Mode::M17ModeFMAudio) ||
        (m_settings.m_m17Mode ==  M17ModSettings::M17Mode::M17ModeM17Audio))
    {
        ui->tone->setChecked(false);
        ui->packetMode->setChecked(false);
        ui->bertMode->setChecked(false);
        ui->tone->setEnabled(false);
        ui->packetMode->setEnabled(false);
        ui->bertMode->setEnabled(true);

        if (m_settings.m_audioType == M17ModSettings::AudioType::AudioInput)
        {
            ui->mic->setChecked(true);
            ui->mic->setEnabled(true);
            ui->play->setChecked(false);
            ui->play->setEnabled(false);
        }
        else if (m_settings.m_audioType == M17ModSettings::AudioType::AudioFile)
        {
            ui->play->setChecked(true);
            ui->play->setEnabled(true);
            ui->mic->setChecked(false);
            ui->mic->setEnabled(false);
        }
        else if (m_settings.m_audioType == M17ModSettings::AudioType::AudioNone)
        {
            ui->mic->setChecked(false);
            ui->play->setChecked(false);
            ui->mic->setEnabled(true);
            ui->play->setEnabled(true);
        }
    }
    else if (m_settings.m_m17Mode == M17ModSettings::M17Mode::M17ModeNone)
    {
        ui->packetMode->setChecked(false);
        ui->bertMode->setChecked(false);
        ui->tone->setChecked(false);
        ui->mic->setChecked(false);
        ui->play->setChecked(false);
        ui->packetMode->setEnabled(true);
        ui->bertMode->setEnabled(true);
        ui->tone->setEnabled(true);
        ui->mic->setEnabled(true);
        ui->play->setEnabled(true);
    }
}

void M17ModGUI::leaveEvent(QEvent* event)
{
	m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void M17ModGUI::enterEvent(EnterEventType* event)
{
	m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void M17ModGUI::audioSelect()
{
    qDebug("M17ModGUI::audioSelect");
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName, true); // true for input
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        applySettings(QList<QString>{"audioDeviceName"});
    }
}

void M17ModGUI::audioFeedbackSelect()
{
    qDebug("M17ModGUI::audioFeedbackSelect");
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName, false); // false for output
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_feedbackAudioDeviceName = audioSelect.m_audioDeviceName;
        applySettings(QList<QString>{"feedbackAudioDeviceName"});
    }
}

void M17ModGUI::tick()
{
    double powDb = CalcDb::dbPower(m_m17Mod->getMagSq());
	m_channelPowerDbAvg(powDb);
	ui->channelPower->setText(tr("%1 dB").arg(m_channelPowerDbAvg.asDouble(), 0, 'f', 1));

    int audioSampleRate = m_m17Mod->getAudioSampleRate();

    if (audioSampleRate != m_audioSampleRate)
    {
        if (audioSampleRate < 0) {
            ui->mic->setColor(QColor("red"));
        } else {
            ui->mic->resetColor();
        }

        m_audioSampleRate = audioSampleRate;
    }

    int feedbackAudioSampleRate = m_m17Mod->getFeedbackAudioSampleRate();

    if (feedbackAudioSampleRate != m_feedbackAudioSampleRate)
    {
        if (feedbackAudioSampleRate < 0) {
            ui->feedbackEnable->setStyleSheet("QToolButton { background-color : red; }");
        } else {
            ui->feedbackEnable->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        }

        m_feedbackAudioSampleRate = feedbackAudioSampleRate;
    }

    if (((++m_tickCount & 0xf) == 0) && (m_settings.m_audioType == M17ModSettings::AudioFile))
    {
        M17Mod::MsgConfigureFileSourceStreamTiming* message = M17Mod::MsgConfigureFileSourceStreamTiming::create();
        m_m17Mod->getInputMessageQueue()->push(message);
    }
}

void M17ModGUI::updateWithStreamData()
{
    QTime recordLength(0, 0, 0, 0);
    recordLength = recordLength.addSecs(m_recordLength);
    QString s_time = recordLength.toString("HH:mm:ss");
    ui->recordLengthText->setText(s_time);
    updateWithStreamTime();
}

void M17ModGUI::updateWithStreamTime()
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

void M17ModGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &M17ModGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &M17ModGUI::on_rfBW_valueChanged);
    QObject::connect(ui->fmDev, &QSlider::valueChanged, this, &M17ModGUI::on_fmDev_valueChanged);
    QObject::connect(ui->toneFrequency, &QDial::valueChanged, this, &M17ModGUI::on_toneFrequency_valueChanged);
    QObject::connect(ui->volume, &QDial::valueChanged, this, &M17ModGUI::on_volume_valueChanged);
    QObject::connect(ui->channelMute, &QToolButton::toggled, this, &M17ModGUI::on_channelMute_toggled);
    QObject::connect(ui->tone, &ButtonSwitch::toggled, this, &M17ModGUI::on_tone_toggled);
    QObject::connect(ui->mic, &ButtonSwitch::toggled, this, &M17ModGUI::on_mic_toggled);
    QObject::connect(ui->play, &ButtonSwitch::toggled, this, &M17ModGUI::on_play_toggled);
    QObject::connect(ui->playLoop, &ButtonSwitch::toggled, this, &M17ModGUI::on_playLoop_toggled);
    QObject::connect(ui->navTimeSlider, &QSlider::valueChanged, this, &M17ModGUI::on_navTimeSlider_valueChanged);
    QObject::connect(ui->showFileDialog, &QPushButton::clicked, this, &M17ModGUI::on_showFileDialog_clicked);
    QObject::connect(ui->feedbackEnable, &QToolButton::toggled, this, &M17ModGUI::on_feedbackEnable_toggled);
    QObject::connect(ui->feedbackVolume, &QDial::valueChanged, this, &M17ModGUI::on_feedbackVolume_valueChanged);
    QObject::connect(ui->fmAudio, &ButtonSwitch::toggled, this, &M17ModGUI::on_fmAudio_toggled);
    QObject::connect(ui->packetMode, &ButtonSwitch::toggled, this, &M17ModGUI::on_packetMode_toggled);
    QObject::connect(ui->bertMode, &ButtonSwitch::toggled, this, &M17ModGUI::on_bertMode_toggled);
    QObject::connect(ui->sendPacket, &QPushButton::clicked, this, &M17ModGUI::on_sendPacket_clicked);
    QObject::connect(ui->loopPacket, &ButtonSwitch::toggled, this, &M17ModGUI::on_loopPacket_toggled);
    QObject::connect(ui->loopPacketInterval, &QDial::valueChanged, this, &M17ModGUI::on_loopPacketInterval_valueChanged);
    QObject::connect(ui->packetDataWidget, &QTabWidget::currentChanged, this, &M17ModGUI::on_packetDataWidget_currentChanged);
    QObject::connect(ui->smsText, &CustomTextEdit::editingFinished, this, &M17ModGUI::on_smsText_editingFinished);
    QObject::connect(ui->aprsFromText, &QLineEdit::editingFinished, this, &M17ModGUI::on_aprsFromText_editingFinished);
    QObject::connect(ui->aprsTo, &QComboBox::currentTextChanged, this, &M17ModGUI::on_aprsTo_currentTextChanged);
    QObject::connect(ui->aprsVia, &QComboBox::currentTextChanged, this, &M17ModGUI::on_aprsVia_currentTextChanged);
    QObject::connect(ui->aprsData, &QLineEdit::editingFinished, this, &M17ModGUI::on_aprsData_editingFinished);
    QObject::connect(ui->aprsInsertPosition, &ButtonSwitch::toggled, this, &M17ModGUI::on_aprsInsertPosition_toggled);
    QObject::connect(ui->source, &QLineEdit::editingFinished, this, &M17ModGUI::on_source_editingFinished);
    QObject::connect(ui->destination, &QLineEdit::editingFinished, this, &M17ModGUI::on_destination_editingFinished);
    QObject::connect(ui->can, QOverload<int>::of(&QSpinBox::valueChanged), this, &M17ModGUI::on_can_valueChanged);
    QObject::connect(ui->insertPosition, &ButtonSwitch::toggled, this, &M17ModGUI::on_insertPosition_toggled);
}

void M17ModGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}

M17ModSettings::PacketType M17ModGUI::indexToPacketType(int index)
{
    switch(index)
    {
        case 0:
            return M17ModSettings::PacketType::PacketSMS;
        case 1:
            return M17ModSettings::PacketType::PacketAPRS;
        default:
            return M17ModSettings::PacketType::PacketSMS;
    }
}

int M17ModGUI::packetTypeToIndex(M17ModSettings::PacketType type)
{
    switch(type)
    {
        case M17ModSettings::PacketType::PacketSMS:
            return 0;
        case M17ModSettings::PacketType::PacketAPRS:
            return 1;
        default:
            return 0;
    }
}
