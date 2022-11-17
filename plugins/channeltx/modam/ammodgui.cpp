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
#include <QFileDialog>
#include <QTime>
#include <QDebug>

#include "device/deviceuiset.h"

#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "dsp/dspengine.h"
#include "dsp/cwkeyer.h"
#include "dsp/dspcommands.h"
#include "gui/crightclickenabler.h"
#include "gui/audioselectdialog.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "maincore.h"

#include "ui_ammodgui.h"
#include "ammodgui.h"

AMModGUI* AMModGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
	AMModGUI* gui = new AMModGUI(pluginAPI, deviceUISet, channelTx);
	return gui;
}

void AMModGUI::destroy()
{
    delete this;
}

void AMModGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray AMModGUI::serialize() const
{
    return m_settings.serialize();
}

bool AMModGUI::deserialize(const QByteArray& data)
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

bool AMModGUI::handleMessage(const Message& message)
{
    if (AMMod::MsgReportFileSourceStreamData::match(message))
    {
        m_recordSampleRate = ((AMMod::MsgReportFileSourceStreamData&)message).getSampleRate();
        m_recordLength = ((AMMod::MsgReportFileSourceStreamData&)message).getRecordLength();
        m_samplesCount = 0;
        updateWithStreamData();
        return true;
    }
    else if (AMMod::MsgReportFileSourceStreamTiming::match(message))
    {
        m_samplesCount = ((AMMod::MsgReportFileSourceStreamTiming&)message).getSamplesCount();
        updateWithStreamTime();
        return true;
    }
    else if (AMMod::MsgConfigureAMMod::match(message))
    {
        const AMMod::MsgConfigureAMMod& cfg = (AMMod::MsgConfigureAMMod&) message;
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

void AMModGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
	applySettings();
}

void AMModGUI::handleSourceMessages()
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

void AMModGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = value;
    updateAbsoluteCenterFrequency();
    applySettings();
}

void AMModGUI::on_rfBW_valueChanged(int value)
{
	ui->rfBWText->setText(QString("%1 kHz").arg(value / 10.0, 0, 'f', 1));
	m_settings.m_rfBandwidth = value * 100.0;
	m_channelMarker.setBandwidth(value * 100);
	applySettings();
}

void AMModGUI::on_modPercent_valueChanged(int value)
{
	ui->modPercentText->setText(QString("%1").arg(value));
	m_settings.m_modFactor = value / 100.0;
	applySettings();
}

void AMModGUI::on_volume_valueChanged(int value)
{
    ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_volumeFactor = value / 10.0;
    applySettings();
}

void AMModGUI::on_toneFrequency_valueChanged(int value)
{
    ui->toneFrequencyText->setText(QString("%1k").arg(value / 100.0, 0, 'f', 2));
    m_settings.m_toneFrequency = value * 10.0;
    applySettings();
}


void AMModGUI::on_channelMute_toggled(bool checked)
{
    m_settings.m_channelMute = checked;
	applySettings();
}

void AMModGUI::on_playLoop_toggled(bool checked)
{
    m_settings.m_playLoop = checked;
	applySettings();
}

void AMModGUI::on_play_toggled(bool checked)
{
    ui->tone->setEnabled(!checked); // release other source inputs
    ui->morseKeyer->setEnabled(!checked);
    ui->mic->setEnabled(!checked);
    m_settings.m_modAFInput = checked ? AMModSettings::AMModInputFile : AMModSettings::AMModInputNone;
    applySettings();
    ui->navTimeSlider->setEnabled(!checked);
    m_enableNavTime = !checked;
}

void AMModGUI::on_tone_toggled(bool checked)
{
    ui->play->setEnabled(!checked); // release other source inputs
    ui->morseKeyer->setEnabled(!checked);
    ui->mic->setEnabled(!checked);
    m_settings.m_modAFInput = checked ? AMModSettings::AMModInputTone : AMModSettings::AMModInputNone;
    applySettings();
}

void AMModGUI::on_morseKeyer_toggled(bool checked)
{
    ui->play->setEnabled(!checked); // release other source inputs
    ui->tone->setEnabled(!checked); // release other source inputs
    ui->mic->setEnabled(!checked);
    m_settings.m_modAFInput = checked ? AMModSettings::AMModInputCWTone : AMModSettings::AMModInputNone;
    applySettings();
}

void AMModGUI::on_mic_toggled(bool checked)
{
    ui->play->setEnabled(!checked); // release other source inputs
    ui->morseKeyer->setEnabled(!checked);
    ui->tone->setEnabled(!checked); // release other source inputs
    m_settings.m_modAFInput = checked ? AMModSettings::AMModInputAudio : AMModSettings::AMModInputNone;
    applySettings();
}

void AMModGUI::on_feedbackEnable_toggled(bool checked)
{
    m_settings.m_feedbackAudioEnable = checked;
    applySettings();
}

void AMModGUI::on_feedbackVolume_valueChanged(int value)
{
    ui->feedbackVolumeText->setText(QString("%1").arg(value / 100.0, 0, 'f', 2));
    m_settings.m_feedbackVolumeFactor = value / 100.0;
    applySettings();
}

void AMModGUI::on_navTimeSlider_valueChanged(int value)
{
    if (m_enableNavTime && ((value >= 0) && (value <= 100)))
    {
        int t_sec = (m_recordLength * value) / 100;
        QTime t(0, 0, 0, 0);
        t = t.addSecs(t_sec);

        AMMod::MsgConfigureFileSourceSeek* message = AMMod::MsgConfigureFileSourceSeek::create(value);
        m_amMod->getInputMessageQueue()->push(message);
    }
}

void AMModGUI::on_showFileDialog_clicked(bool checked)
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

void AMModGUI::configureFileName()
{
    qDebug() << "FileSourceGui::configureFileName: " << m_fileName.toStdString().c_str();
    AMMod::MsgConfigureFileSourceName* message = AMMod::MsgConfigureFileSourceName::create(m_fileName);
    m_amMod->getInputMessageQueue()->push(message);
}

void AMModGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void AMModGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_amMod->getNumberOfDeviceStreams());
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

AMModGUI::AMModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent) :
	ChannelGUI(parent),
	ui(new Ui::AMModGUI),
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
    m_helpURL = "plugins/channeltx/modam/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

	m_amMod = (AMMod*) channelTx;
	m_amMod->setMessageQueueToGUI(getInputMessageQueue());

	connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    CRightClickEnabler *audioMuteRightClickEnabler = new CRightClickEnabler(ui->mic);
    connect(audioMuteRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect()));

    CRightClickEnabler *feedbackRightClickEnabler = new CRightClickEnabler(ui->feedbackEnable);
    connect(feedbackRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioFeedbackSelect()));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::yellow);
    m_channelMarker.setBandwidth(5000);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("AM Modulator");
    m_channelMarker.setSourceOrSinkStream(false);
    m_channelMarker.blockSignals(false);
	m_channelMarker.setVisible(true); // activate signal on the last setting only

	m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);
	m_settings.setCWKeyerGUI(ui->cwKeyerGUI);

	m_deviceUISet->addChannelMarker(&m_channelMarker);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));

    ui->play->setEnabled(false);
    ui->play->setChecked(false);
    ui->tone->setChecked(false);
    ui->morseKeyer->setChecked(false);
    ui->mic->setChecked(false);

    ui->cwKeyerGUI->setCWKeyer(m_amMod->getCWKeyer());

	connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
    m_amMod->setLevelMeter(ui->volumeMeter);

	displaySettings();
    makeUIConnections();
    applySettings(true);
}

AMModGUI::~AMModGUI()
{
	delete ui;
}

void AMModGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void AMModGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
		setTitleColor(m_channelMarker.getColor());
        AMMod::MsgConfigureAMMod* message = AMMod::MsgConfigureAMMod::create( m_settings, force);
        m_amMod->getInputMessageQueue()->push(message);
	}
}

void AMModGUI::displaySettings()
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

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->rfBW->setValue(roundf(m_settings.m_rfBandwidth / 100.0));
    ui->rfBWText->setText(QString("%1 kHz").arg(m_settings.m_rfBandwidth / 1000.0, 0, 'f', 1));

    int modPercent = roundf(m_settings.m_modFactor * 100.0);
    ui->modPercent->setValue(modPercent);
    ui->modPercentText->setText(QString("%1").arg(modPercent));

    ui->toneFrequency->setValue(roundf(m_settings.m_toneFrequency / 10.0));
    ui->toneFrequencyText->setText(QString("%1k").arg(m_settings.m_toneFrequency / 1000.0, 0, 'f', 2));

    ui->volume->setValue(roundf(m_settings.m_volumeFactor * 10.0));
    ui->volumeText->setText(QString("%1").arg(m_settings.m_volumeFactor, 0, 'f', 1));

    ui->channelMute->setChecked(m_settings.m_channelMute);
    ui->playLoop->setChecked(m_settings.m_playLoop);

    ui->tone->setEnabled((m_settings.m_modAFInput == AMModSettings::AMModInputAF::AMModInputTone) || (m_settings.m_modAFInput == AMModSettings::AMModInputAF::AMModInputNone));
    ui->mic->setEnabled((m_settings.m_modAFInput == AMModSettings::AMModInputAF::AMModInputAudio) || (m_settings.m_modAFInput == AMModSettings::AMModInputAF::AMModInputNone));
    ui->play->setEnabled((m_settings.m_modAFInput == AMModSettings::AMModInputAF::AMModInputFile) || (m_settings.m_modAFInput == AMModSettings::AMModInputAF::AMModInputNone));
    ui->morseKeyer->setEnabled((m_settings.m_modAFInput == AMModSettings::AMModInputAF::AMModInputCWTone) || (m_settings.m_modAFInput == AMModSettings::AMModInputAF::AMModInputNone));

    ui->tone->setChecked(m_settings.m_modAFInput == AMModSettings::AMModInputAF::AMModInputTone);
    ui->mic->setChecked(m_settings.m_modAFInput == AMModSettings::AMModInputAF::AMModInputAudio);
    ui->play->setChecked(m_settings.m_modAFInput == AMModSettings::AMModInputAF::AMModInputFile);
    ui->morseKeyer->setChecked(m_settings.m_modAFInput == AMModSettings::AMModInputAF::AMModInputCWTone);

    ui->feedbackEnable->setChecked(m_settings.m_feedbackAudioEnable);
    ui->feedbackVolume->setValue(roundf(m_settings.m_feedbackVolumeFactor * 100.0));
    ui->feedbackVolumeText->setText(QString("%1").arg(m_settings.m_feedbackVolumeFactor, 0, 'f', 2));

    updateIndexLabel();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void AMModGUI::leaveEvent(QEvent* event)
{
	m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void AMModGUI::enterEvent(EnterEventType* event)
{
	m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void AMModGUI::audioSelect()
{
    qDebug("AMModGUI::audioSelect");
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName, true); // true for input
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        applySettings();
    }
}

void AMModGUI::audioFeedbackSelect()
{
    qDebug("AMModGUI::audioFeedbackSelect");
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName, false); // false for output
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_feedbackAudioDeviceName = audioSelect.m_audioDeviceName;
        applySettings();
    }
}

void AMModGUI::tick()
{
    double powDb = CalcDb::dbPower(m_amMod->getMagSq());
	m_channelPowerDbAvg(powDb);
	ui->channelPower->setText(tr("%1 dB").arg(m_channelPowerDbAvg.asDouble(), 0, 'f', 1));

    int audioSampleRate = m_amMod->getAudioSampleRate();

    if (audioSampleRate != m_audioSampleRate)
    {
        if (audioSampleRate < 0) {
            ui->mic->setColor(QColor("red"));
        } else {
            ui->mic->resetColor();
        }

        m_audioSampleRate = audioSampleRate;
    }

    int feedbackAudioSampleRate = m_amMod->getFeedbackAudioSampleRate();

    if (feedbackAudioSampleRate != m_feedbackAudioSampleRate)
    {
        if (feedbackAudioSampleRate < 0) {
            ui->feedbackEnable->setStyleSheet("QToolButton { background-color : red; }");
        } else {
            ui->feedbackEnable->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        }

        m_feedbackAudioSampleRate = feedbackAudioSampleRate;
    }

    if (((++m_tickCount & 0xf) == 0) && (m_settings.m_modAFInput == AMModSettings::AMModInputFile))
    {
        AMMod::MsgConfigureFileSourceStreamTiming* message = AMMod::MsgConfigureFileSourceStreamTiming::create();
        m_amMod->getInputMessageQueue()->push(message);
    }
}

void AMModGUI::updateWithStreamData()
{
    QTime recordLength(0, 0, 0, 0);
    recordLength = recordLength.addSecs(m_recordLength);
    QString s_time = recordLength.toString("HH:mm:ss");
    ui->recordLengthText->setText(s_time);
    updateWithStreamTime();
}

void AMModGUI::updateWithStreamTime()
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

void AMModGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &AMModGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &AMModGUI::on_rfBW_valueChanged);
    QObject::connect(ui->modPercent, &QSlider::valueChanged, this, &AMModGUI::on_modPercent_valueChanged);
    QObject::connect(ui->volume, &QDial::valueChanged, this, &AMModGUI::on_volume_valueChanged);
    QObject::connect(ui->channelMute, &QToolButton::toggled, this, &AMModGUI::on_channelMute_toggled);
    QObject::connect(ui->tone, &ButtonSwitch::toggled, this, &AMModGUI::on_tone_toggled);
    QObject::connect(ui->toneFrequency, &QDial::valueChanged, this, &AMModGUI::on_toneFrequency_valueChanged);
    QObject::connect(ui->mic, &ButtonSwitch::toggled, this, &AMModGUI::on_mic_toggled);
    QObject::connect(ui->play, &ButtonSwitch::toggled, this, &AMModGUI::on_play_toggled);
    QObject::connect(ui->morseKeyer, &ButtonSwitch::toggled, this, &AMModGUI::on_morseKeyer_toggled);
    QObject::connect(ui->playLoop, &ButtonSwitch::toggled, this, &AMModGUI::on_playLoop_toggled);
    QObject::connect(ui->navTimeSlider, &QSlider::valueChanged, this, &AMModGUI::on_navTimeSlider_valueChanged);
    QObject::connect(ui->showFileDialog, &QPushButton::clicked, this, &AMModGUI::on_showFileDialog_clicked);
    QObject::connect(ui->feedbackEnable, &QToolButton::toggled, this, &AMModGUI::on_feedbackEnable_toggled);
    QObject::connect(ui->feedbackVolume, &QDial::valueChanged, this, &AMModGUI::on_feedbackVolume_valueChanged);
}

void AMModGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
