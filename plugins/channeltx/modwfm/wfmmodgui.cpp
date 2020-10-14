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
#include <QColor>
#include <QFileDialog>
#include <QTime>
#include <QDebug>

#include "device/deviceuiset.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "dsp/dspengine.h"
#include "dsp/cwkeyer.h"
#include "gui/crightclickenabler.h"
#include "gui/audioselectdialog.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "maincore.h"

#include "ui_wfmmodgui.h"
#include "wfmmodgui.h"

WFMModGUI* WFMModGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    WFMModGUI* gui = new WFMModGUI(pluginAPI, deviceUISet, channelTx);
	return gui;
}

void WFMModGUI::destroy()
{
    delete this;
}

void WFMModGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray WFMModGUI::serialize() const
{
    return m_settings.serialize();
}

bool WFMModGUI::deserialize(const QByteArray& data)
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

bool WFMModGUI::handleMessage(const Message& message)
{
    if (WFMMod::MsgReportFileSourceStreamData::match(message))
    {
        m_recordSampleRate = ((WFMMod::MsgReportFileSourceStreamData&)message).getSampleRate();
        m_recordLength = ((WFMMod::MsgReportFileSourceStreamData&)message).getRecordLength();
        m_samplesCount = 0;
        updateWithStreamData();
        return true;
    }
    else if (WFMMod::MsgReportFileSourceStreamTiming::match(message))
    {
        m_samplesCount = ((WFMMod::MsgReportFileSourceStreamTiming&)message).getSamplesCount();
        updateWithStreamTime();
        return true;
    }
    else if (WFMMod::MsgConfigureWFMMod::match(message))
    {
        const WFMMod::MsgConfigureWFMMod& cfg = (WFMMod::MsgConfigureWFMMod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
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

void WFMModGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
	applySettings();
}

void WFMModGUI::handleSourceMessages()
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

void WFMModGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void WFMModGUI::on_rfBW_currentIndexChanged(int index)
{
    float rfBW = WFMModSettings::getRFBW(index);
	m_channelMarker.setBandwidth(rfBW);
	m_settings.m_rfBandwidth = rfBW;
	applySettings();
}

void WFMModGUI::on_afBW_valueChanged(int value)
{
	ui->afBWText->setText(QString("%1k").arg(value));
	m_settings.m_afBandwidth = value * 1000.0;
	applySettings();
}

void WFMModGUI::on_fmDev_valueChanged(int value)
{
	ui->fmDevText->setText(QString("%1k").arg(value));
	m_settings.m_fmDeviation = value * 1000.0;
	applySettings();
}

void WFMModGUI::on_volume_valueChanged(int value)
{
	ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
	m_settings.m_volumeFactor = value / 10.0;
	applySettings();
}

void WFMModGUI::on_toneFrequency_valueChanged(int value)
{
    ui->toneFrequencyText->setText(QString("%1k").arg(value / 100.0, 0, 'f', 2));
    m_settings.m_toneFrequency = value * 10.0;
    applySettings();
}

void WFMModGUI::on_channelMute_toggled(bool checked)
{
    m_settings.m_channelMute = checked;
	applySettings();
}

void WFMModGUI::on_playLoop_toggled(bool checked)
{
    m_settings.m_playLoop = checked;
	applySettings();
}

void WFMModGUI::on_play_toggled(bool checked)
{
    ui->tone->setEnabled(!checked); // release other source inputs
    ui->mic->setEnabled(!checked);
    ui->morseKeyer->setEnabled(!checked);
    m_settings.m_modAFInput = checked ? WFMModSettings::WFMModInputFile : WFMModSettings::WFMModInputNone;
    applySettings();
    ui->navTimeSlider->setEnabled(!checked);
    m_enableNavTime = !checked;
}

void WFMModGUI::on_tone_toggled(bool checked)
{
    ui->play->setEnabled(!checked); // release other source inputs
    ui->mic->setEnabled(!checked);
    ui->morseKeyer->setEnabled(!checked);
    m_settings.m_modAFInput = checked ? WFMModSettings::WFMModInputTone : WFMModSettings::WFMModInputNone;
    applySettings();
}

void WFMModGUI::on_morseKeyer_toggled(bool checked)
{
    ui->tone->setEnabled(!checked); // release other source inputs
    ui->mic->setEnabled(!checked);
    ui->play->setEnabled(!checked);
    m_settings.m_modAFInput = checked ? WFMModSettings::WFMModInputCWTone : WFMModSettings::WFMModInputNone;
    applySettings();
}

void WFMModGUI::on_mic_toggled(bool checked)
{
    ui->play->setEnabled(!checked); // release other source inputs
    ui->tone->setEnabled(!checked); // release other source inputs
    ui->morseKeyer->setEnabled(!checked);
    m_settings.m_modAFInput = checked ? WFMModSettings::WFMModInputAudio : WFMModSettings::WFMModInputNone;
    applySettings();
}

void WFMModGUI::on_feedbackEnable_toggled(bool checked)
{
    m_settings.m_feedbackAudioEnable = checked;
    applySettings();
}

void WFMModGUI::on_feedbackVolume_valueChanged(int value)
{
    ui->feedbackVolumeText->setText(QString("%1").arg(value / 100.0, 0, 'f', 2));
    m_settings.m_feedbackVolumeFactor = value / 100.0;
    applySettings();
}

void WFMModGUI::on_navTimeSlider_valueChanged(int value)
{
    if (m_enableNavTime && ((value >= 0) && (value <= 100)))
    {
        int t_sec = (m_recordLength * value) / 100;
        QTime t(0, 0, 0, 0);
        t = t.addSecs(t_sec);

        WFMMod::MsgConfigureFileSourceSeek* message = WFMMod::MsgConfigureFileSourceSeek::create(value);
        m_wfmMod->getInputMessageQueue()->push(message);
    }
}

void WFMModGUI::on_showFileDialog_clicked(bool checked)
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

void WFMModGUI::configureFileName()
{
    qDebug() << "FileSourceGui::configureFileName: " << m_fileName.toStdString().c_str();
    WFMMod::MsgConfigureFileSourceName* message = WFMMod::MsgConfigureFileSourceName::create(m_fileName);
    m_wfmMod->getInputMessageQueue()->push(message);
}

void WFMModGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;
}

void WFMModGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
    {
        BasicChannelSettingsDialog dialog(&m_channelMarker, this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
        dialog.setReverseAPIChannelIndex(m_settings.m_reverseAPIChannelIndex);

        dialog.move(p);
        dialog.exec();

        m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
        m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
        m_settings.m_title = m_channelMarker.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settings.m_reverseAPIChannelIndex = dialog.getReverseAPIChannelIndex();

        setWindowTitle(m_settings.m_title);
        setTitleColor(m_settings.m_rgbColor);

        applySettings();
    }
    else if ((m_contextMenuType == ContextMenuStreamSettings) && (m_deviceUISet->m_deviceMIMOEngine))
    {
        DeviceStreamSelectionDialog dialog(this);
        dialog.setNumberOfStreams(m_wfmMod->getNumberOfDeviceStreams());
        dialog.setStreamIndex(m_settings.m_streamIndex);
        dialog.move(p);
        dialog.exec();

        m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
        m_channelMarker.clearStreamIndexes();
        m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
        displayStreamIndex();
        applySettings();
    }

    resetContextMenuType();
}

WFMModGUI::WFMModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent) :
	ChannelGUI(parent),
	ui(new Ui::WFMModGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
	m_doApplySettings(true),
    m_recordLength(0),
    m_recordSampleRate(48000),
    m_samplesCount(0),
    m_audioSampleRate(-1),
    m_feedbackAudioSampleRate(-1),
    m_tickCount(0),
    m_enableNavTime(false)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);

    blockApplySettings(true);

    ui->rfBW->clear();
    for (int i = 0; i < WFMModSettings::m_nbRfBW; i++) {
        ui->rfBW->addItem(QString("%1").arg(WFMModSettings::getRFBW(i) / 1000.0, 0, 'f', 2));
    }
    ui->rfBW->setCurrentIndex(7);

    blockApplySettings(false);

	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

	m_wfmMod = (WFMMod*) channelTx;
	m_wfmMod->setMessageQueueToGUI(getInputMessageQueue());

	connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    CRightClickEnabler *audioMuteRightClickEnabler = new CRightClickEnabler(ui->mic);
    connect(audioMuteRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect()));

    CRightClickEnabler *feedbackRightClickEnabler = new CRightClickEnabler(ui->feedbackEnable);
    connect(feedbackRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioFeedbackSelect()));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 8, -99999999, 99999999);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::blue);
    m_channelMarker.setBandwidth(125000);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("WFM Modulator");
    m_channelMarker.setSourceOrSinkStream(false);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

	m_deviceUISet->addChannelMarker(&m_channelMarker);
	m_deviceUISet->addRollupWidget(this);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));

    ui->play->setEnabled(false);
    ui->play->setChecked(false);
    ui->tone->setChecked(false);
    ui->mic->setChecked(false);

    ui->cwKeyerGUI->setCWKeyer(m_wfmMod->getCWKeyer());

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setCWKeyerGUI(ui->cwKeyerGUI);

	connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
    m_wfmMod->setLevelMeter(ui->volumeMeter);

	displaySettings();
    applySettings(true);
}

WFMModGUI::~WFMModGUI()
{
	delete ui;
}

void WFMModGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void WFMModGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
		setTitleColor(m_channelMarker.getColor());
		ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
		WFMMod::MsgConfigureWFMMod *msgConf = WFMMod::MsgConfigureWFMMod::create(m_settings, force);
		m_wfmMod->getInputMessageQueue()->push(msgConf);
	}
}

void WFMModGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    displayStreamIndex();

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->rfBW->setCurrentIndex(WFMModSettings::getRFBWIndex(m_settings.m_rfBandwidth));

    ui->afBWText->setText(QString("%1k").arg(m_settings.m_afBandwidth / 1000.0));
    ui->afBW->setValue(m_settings.m_afBandwidth / 1000.0);

    ui->fmDevText->setText(QString("%1k").arg(m_settings.m_fmDeviation / 1000.0));
    ui->fmDev->setValue(m_settings.m_fmDeviation / 1000.0);

    ui->volumeText->setText(QString("%1").arg(m_settings.m_volumeFactor, 0, 'f', 1));
    ui->volume->setValue(m_settings.m_volumeFactor * 10.0);

    ui->toneFrequencyText->setText(QString("%1k").arg(m_settings.m_toneFrequency / 1000.0, 0, 'f', 2));
    ui->toneFrequency->setValue(m_settings.m_toneFrequency / 10.0);

    ui->channelMute->setChecked(m_settings.m_channelMute);
    ui->playLoop->setChecked(m_settings.m_playLoop);

    ui->tone->setEnabled((m_settings.m_modAFInput == WFMModSettings::WFMModInputAF::WFMModInputTone) || (m_settings.m_modAFInput == WFMModSettings::WFMModInputAF::WFMModInputNone));
    ui->mic->setEnabled((m_settings.m_modAFInput == WFMModSettings::WFMModInputAF::WFMModInputAudio) || (m_settings.m_modAFInput == WFMModSettings::WFMModInputAF::WFMModInputNone));
    ui->play->setEnabled((m_settings.m_modAFInput == WFMModSettings::WFMModInputAF::WFMModInputFile) || (m_settings.m_modAFInput == WFMModSettings::WFMModInputAF::WFMModInputNone));
    ui->morseKeyer->setEnabled((m_settings.m_modAFInput == WFMModSettings::WFMModInputAF::WFMModInputCWTone) || (m_settings.m_modAFInput == WFMModSettings::WFMModInputAF::WFMModInputNone));

    ui->tone->setChecked(m_settings.m_modAFInput == WFMModSettings::WFMModInputAF::WFMModInputTone);
    ui->mic->setChecked(m_settings.m_modAFInput == WFMModSettings::WFMModInputAF::WFMModInputAudio);
    ui->play->setChecked(m_settings.m_modAFInput == WFMModSettings::WFMModInputAF::WFMModInputFile);
    ui->morseKeyer->setChecked(m_settings.m_modAFInput == WFMModSettings::WFMModInputAF::WFMModInputCWTone);

    ui->feedbackEnable->setChecked(m_settings.m_feedbackAudioEnable);
    ui->feedbackVolume->setValue(roundf(m_settings.m_feedbackVolumeFactor * 100.0));
    ui->feedbackVolumeText->setText(QString("%1").arg(m_settings.m_feedbackVolumeFactor, 0, 'f', 2));

    blockApplySettings(false);
}

void WFMModGUI::displayStreamIndex()
{
    if (m_deviceUISet->m_deviceMIMOEngine) {
        setStreamIndicator(tr("%1").arg(m_settings.m_streamIndex));
    } else {
        setStreamIndicator("S"); // single channel indicator
    }
}

void WFMModGUI::leaveEvent(QEvent*)
{
	m_channelMarker.setHighlighted(false);
}

void WFMModGUI::enterEvent(QEvent*)
{
	m_channelMarker.setHighlighted(true);
}

void WFMModGUI::audioSelect()
{
    qDebug("WFMModGUI::audioSelect");
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName, true); // true for input
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        applySettings();
    }
}

void WFMModGUI::audioFeedbackSelect()
{
    qDebug("WFMModGUI::audioFeedbackSelect");
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName, false); // false for output
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_feedbackAudioDeviceName = audioSelect.m_audioDeviceName;
        applySettings();
    }
}

void WFMModGUI::tick()
{
    double powDb = CalcDb::dbPower(m_wfmMod->getMagSq());
	m_channelPowerDbAvg(powDb);
	ui->channelPower->setText(tr("%1 dB").arg(m_channelPowerDbAvg.asDouble(), 0, 'f', 1));

    int audioSampleRate = m_wfmMod->getAudioSampleRate();

    if (audioSampleRate != m_audioSampleRate)
    {
        if (audioSampleRate < 0) {
            ui->mic->setColor(QColor("red"));
        } else {
            ui->mic->resetColor();
        }

        m_audioSampleRate = audioSampleRate;
    }

    int feedbackAudioSampleRate = m_wfmMod->getFeedbackAudioSampleRate();

    if (feedbackAudioSampleRate != m_feedbackAudioSampleRate)
    {
        if (feedbackAudioSampleRate < 0) {
            ui->feedbackEnable->setStyleSheet("QToolButton { background-color : red; }");
        } else {
            ui->feedbackEnable->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        }

        m_feedbackAudioSampleRate = feedbackAudioSampleRate;
    }

    if (((++m_tickCount & 0xf) == 0) && (m_settings.m_modAFInput == WFMModSettings::WFMModInputFile))
    {
        WFMMod::MsgConfigureFileSourceStreamTiming* message = WFMMod::MsgConfigureFileSourceStreamTiming::create();
        m_wfmMod->getInputMessageQueue()->push(message);
    }
}

void WFMModGUI::updateWithStreamData()
{
    QTime recordLength(0, 0, 0, 0);
    recordLength = recordLength.addSecs(m_recordLength);
    QString s_time = recordLength.toString("HH:mm:ss");
    ui->recordLengthText->setText(s_time);
    updateWithStreamTime();
}

void WFMModGUI::updateWithStreamTime()
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
