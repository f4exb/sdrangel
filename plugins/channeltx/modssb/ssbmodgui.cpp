///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
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

#include "device/devicesinkapi.h"
#include "dsp/spectrumvis.h"
#include "ui_ssbmodgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingswidget.h"
#include "dsp/dspengine.h"
#include "mainwindow.h"

const QString SSBModGUI::m_channelID = "sdrangel.channeltx.modssb";

SSBModGUI* SSBModGUI::create(PluginAPI* pluginAPI, DeviceSinkAPI *deviceAPI)
{
    SSBModGUI* gui = new SSBModGUI(pluginAPI, deviceAPI);
	return gui;
}

void SSBModGUI::destroy()
{
}

void SSBModGUI::setName(const QString& name)
{
	setObjectName(name);
}

QString SSBModGUI::getName() const
{
	return objectName();
}

qint64 SSBModGUI::getCenterFrequency() const {
	return m_channelMarker.getCenterFrequency();
}

void SSBModGUI::setCenterFrequency(qint64 centerFrequency)
{
	m_channelMarker.setCenterFrequency(centerFrequency);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
	applySettings();
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
        displaySettings();
        applySettings(true); // will have true
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        displaySettings();
        applySettings(true); // will have true
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
    else
    {
        return false;
    }
}

void SSBModGUI::channelMarkerChanged()
{
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    displaySettings();
    applySettings();
}

void SSBModGUI::channelMarkerUpdate()
{
    m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
    m_settings.m_udpAddress = m_channelMarker.getUDPAddress();
    m_settings.m_udpPort = m_channelMarker.getUDPReceivePort();
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
    applySettings();
}

void SSBModGUI::on_dsb_toggled(bool checked)
{
    m_settings.m_dsb = checked;

    if (checked)
    {
        if (ui->BW->value() < 0) {
            ui->BW->setValue(-ui->BW->value());
        }

        m_channelMarker.setSidebands(ChannelMarker::dsb);

        QString bwStr = QString::number(ui->BW->value()/10.0, 'f', 1);
        ui->BWText->setText(tr("%1%2k").arg(QChar(0xB1, 0x00)).arg(bwStr));
        ui->lowCut->setValue(0);
        ui->lowCut->setEnabled(false);

        m_settings.m_bandwidth = ui->BW->value() * 100.0;
        m_settings.m_lowCutoff = 0;

        applySettings();
    }
    else
    {
        if (ui->BW->value() < 0) {
            m_channelMarker.setSidebands(ChannelMarker::lsb);
        } else {
            m_channelMarker.setSidebands(ChannelMarker::usb);
        }

        QString bwStr = QString::number(ui->BW->value()/10.0, 'f', 1);
        ui->BWText->setText(tr("%1k").arg(bwStr));
        ui->lowCut->setEnabled(true);
        m_settings.m_bandwidth = ui->BW->value() * 100.0;

        on_lowCut_valueChanged(m_channelMarker.getLowCutoff()/100);
    }

    setNewRate(m_settings.m_spanLog2);
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
    if (setNewRate(value))
    {
        m_settings.m_spanLog2 = value;
        applySettings();
    }

}

void SSBModGUI::on_BW_valueChanged(int value)
{
    QString s = QString::number(value/10.0, 'f', 1);
    m_channelMarker.setBandwidth(value * 200);

    if (ui->dsb->isChecked())
    {
        ui->BWText->setText(tr("%1%2k").arg(QChar(0xB1, 0x00)).arg(s));
    }
    else
    {
        ui->BWText->setText(tr("%1k").arg(s));
    }

    m_settings.m_bandwidth = value * 100;
    on_lowCut_valueChanged(m_channelMarker.getLowCutoff()/100);
	setNewRate(m_settings.m_spanLog2);
}

void SSBModGUI::on_lowCut_valueChanged(int value)
{
    int lowCutoff = getEffectiveLowCutoff(value * 100);
    m_channelMarker.setLowCutoff(lowCutoff);
    QString s = QString::number(lowCutoff/1000.0, 'f', 1);
    ui->lowCutText->setText(tr("%1k").arg(s));
    ui->lowCut->setValue(lowCutoff/100);
    m_settings.m_lowCutoff = ui->lowCut->value() * 100.0;
    applySettings();
}

int SSBModGUI::getEffectiveLowCutoff(int lowCutoff)
{
    int ssbBW = m_channelMarker.getBandwidth() / 2;
    int effectiveLowCutoff = lowCutoff;
    const int guard = 100;

    if (ssbBW < 0)
    {
        if (effectiveLowCutoff < ssbBW + guard)
        {
            effectiveLowCutoff = ssbBW + guard;
        }
        if (effectiveLowCutoff > 0)
        {
            effectiveLowCutoff = 0;
        }
    }
    else
    {
        if (effectiveLowCutoff > ssbBW - guard)
        {
            effectiveLowCutoff = ssbBW - guard;
        }
        if (effectiveLowCutoff < 0)
        {
            effectiveLowCutoff = 0;
        }
    }

    return effectiveLowCutoff;
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
    m_modAFInput = checked ? SSBMod::SSBModInputFile : SSBMod::SSBModInputNone;
    SSBMod::MsgConfigureAFInput* message = SSBMod::MsgConfigureAFInput::create(m_modAFInput);
    m_ssbMod->getInputMessageQueue()->push(message);
    ui->navTimeSlider->setEnabled(!checked);
    m_enableNavTime = !checked;
}

void SSBModGUI::on_tone_toggled(bool checked)
{
    ui->play->setEnabled(!checked); // release other source inputs
    ui->morseKeyer->setEnabled(!checked);
    ui->mic->setEnabled(!checked);
    m_modAFInput = checked ? SSBMod::SSBModInputTone : SSBMod::SSBModInputNone;
    SSBMod::MsgConfigureAFInput* message = SSBMod::MsgConfigureAFInput::create(m_modAFInput);
    m_ssbMod->getInputMessageQueue()->push(message);
}

void SSBModGUI::on_morseKeyer_toggled(bool checked)
{
    ui->play->setEnabled(!checked); // release other source inputs
    ui->tone->setEnabled(!checked); // release other source inputs
    ui->mic->setEnabled(!checked);
    m_modAFInput = checked ? SSBMod::SSBModInputCWTone : SSBMod::SSBModInputNone;
    SSBMod::MsgConfigureAFInput* message = SSBMod::MsgConfigureAFInput::create(m_modAFInput);
    m_ssbMod->getInputMessageQueue()->push(message);
}

void SSBModGUI::on_mic_toggled(bool checked)
{
    ui->play->setEnabled(!checked); // release other source inputs
    ui->morseKeyer->setEnabled(!checked);
    ui->tone->setEnabled(!checked); // release other source inputs
    m_modAFInput = checked ? SSBMod::SSBModInputAudio : SSBMod::SSBModInputNone;
    SSBMod::MsgConfigureAFInput* message = SSBMod::MsgConfigureAFInput::create(m_modAFInput);
    m_ssbMod->getInputMessageQueue()->push(message);
}

void SSBModGUI::on_agc_toggled(bool checked)
{
    m_settings.m_agc = checked;
    applySettings();
}

void SSBModGUI::on_agcOrder_valueChanged(int value){
    QString s = QString::number(value / 100.0, 'f', 2);
    ui->agcOrderText->setText(s);
    m_settings.m_agcOrder = value / 100.0;
    applySettings();
}

void SSBModGUI::on_agcTime_valueChanged(int value){
    QString s = QString::number(SSBModSettings::getAGCTimeConstant(value), 'f', 0);
    ui->agcTimeText->setText(s);
    m_settings.m_agcTime = SSBModSettings::getAGCTimeConstant(value) * 48;
    applySettings();
}

void SSBModGUI::on_agcThreshold_valueChanged(int value)
{
    m_settings.m_agcThreshold = value; // dB
    displayAGCPowerThreshold();
    applySettings();
}

void SSBModGUI::on_agcThresholdGate_valueChanged(int value)
{
    QString s = QString::number(value, 'f', 0);
    ui->agcThresholdGateText->setText(s);
    m_settings.m_agcThresholdGate = value * 48;
    applySettings();
}

void SSBModGUI::on_agcThresholdDelay_valueChanged(int value)
{
    QString s = QString::number(value * 10, 'f', 0);
    ui->agcThresholdDelayText->setText(s);
    m_settings.m_agcThresholdDelay = value * 480;
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

void SSBModGUI::on_showFileDialog_clicked(bool checked __attribute__((unused)))
{
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Open raw audio file"), ".", tr("Raw audio Files (*.raw)"));

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

void SSBModGUI::onWidgetRolled(QWidget* widget __attribute__((unused)), bool rollDown __attribute__((unused)))
{
}

void SSBModGUI::onMenuDoubleClicked()
{
	if (!m_basicSettingsShown)
	{
		m_basicSettingsShown = true;
		BasicChannelSettingsWidget* bcsw = new BasicChannelSettingsWidget(&m_channelMarker, this);
		bcsw->show();

		if (bcsw->getHasChanged())
		{
		    channelMarkerUpdate();
		}
	}
}

SSBModGUI::SSBModGUI(PluginAPI* pluginAPI, DeviceSinkAPI *deviceAPI, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::SSBModGUI),
	m_pluginAPI(pluginAPI),
	m_deviceAPI(deviceAPI),
	m_channelMarker(this),
	m_basicSettingsShown(false),
	m_doApplySettings(true),
	m_rate(6000),
	m_channelPowerDbAvg(20,0),
    m_recordLength(0),
    m_recordSampleRate(48000),
    m_samplesCount(0),
    m_tickCount(0),
    m_enableNavTime(false),
    m_modAFInput(SSBMod::SSBModInputNone)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(menuDoubleClickEvent()), this, SLOT(onMenuDoubleClicked()));

	m_spectrumVis = new SpectrumVis(ui->glSpectrum);
	m_ssbMod = new SSBMod(m_deviceAPI, m_spectrumVis);
	m_ssbMod->setMessageQueueToGUI(getInputMessageQueue());

    resetToDefaults();

	ui->glSpectrum->setCenterFrequency(m_rate/2);
	ui->glSpectrum->setSampleRate(m_rate);
	ui->glSpectrum->setDisplayWaterfall(true);
	ui->glSpectrum->setDisplayMaxHold(true);
	ui->glSpectrum->setSsbSpectrum(true);
	ui->glSpectrum->connectTimer(m_pluginAPI->getMainWindow()->getMasterTimer());

	connect(&m_pluginAPI->getMainWindow()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

	m_channelMarker.setColor(Qt::green);
	m_channelMarker.setBandwidth(m_rate);
	m_channelMarker.setSidebands(ChannelMarker::usb);
	m_channelMarker.setCenterFrequency(0);
	m_channelMarker.setVisible(true);

    connect(&m_channelMarker, SIGNAL(changed()), this, SLOT(channelMarkerChanged()));

	m_deviceAPI->registerChannelInstance(m_channelID, this);
    m_deviceAPI->addChannelMarker(&m_channelMarker);
    m_deviceAPI->addRollupWidget(this);

    ui->cwKeyerGUI->setBuddies(m_ssbMod->getInputMessageQueue(), m_ssbMod->getCWKeyer());
    ui->spectrumGUI->setBuddies(m_spectrumVis->getInputMessageQueue(), m_spectrumVis, ui->glSpectrum);

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setSpectrumGUI(ui->spectrumGUI);
    m_settings.setCWKeyerGUI(ui->cwKeyerGUI);

    displaySettings();
	applySettings(true);
	setNewRate(m_settings.m_spanLog2);

	connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
	connect(m_ssbMod, SIGNAL(levelChanged(qreal, qreal, int)), ui->volumeMeter, SLOT(levelChanged(qreal, qreal, int)));
}

SSBModGUI::~SSBModGUI()
{
    m_deviceAPI->removeChannelInstance(this);
	delete m_ssbMod;
	delete m_spectrumVis;
	delete ui;
}

bool SSBModGUI::setNewRate(int spanLog2)
{
	if ((spanLog2 < 1) || (spanLog2 > 5))
	{
		return false;
	}

	m_settings.m_spanLog2 = spanLog2;
	m_rate = 48000 / (1<<spanLog2);

	if (ui->BW->value() < -m_rate/100)
	{
		ui->BW->setValue(-m_rate/100);
		m_channelMarker.setBandwidth(-m_rate*2);
	}
	else if (ui->BW->value() > m_rate/100)
	{
		ui->BW->setValue(m_rate/100);
		m_channelMarker.setBandwidth(m_rate*2);
	}

	if (ui->lowCut->value() < -m_rate/100)
	{
		ui->lowCut->setValue(-m_rate/100);
		m_channelMarker.setLowCutoff(-m_rate);
	}
	else if (ui->lowCut->value() > m_rate/100)
	{
		ui->lowCut->setValue(m_rate/100);
		m_channelMarker.setLowCutoff(m_rate);
	}

	QString s = QString::number(m_rate/1000.0, 'f', 1);

	if (ui->dsb->isChecked())
	{
        ui->BW->setMinimum(0);
        ui->BW->setMaximum(m_rate/100);
        ui->lowCut->setMinimum(0);
        ui->lowCut->setMaximum(m_rate/100);

        m_channelMarker.setSidebands(ChannelMarker::dsb);

        ui->spanText->setText(tr("%1%2k").arg(QChar(0xB1, 0x00)).arg(s));
        ui->glSpectrum->setCenterFrequency(0);
        ui->glSpectrum->setSampleRate(2*m_rate);
        ui->glSpectrum->setSsbSpectrum(false);
        ui->glSpectrum->setLsbDisplay(false);
	}
	else
	{
        ui->BW->setMinimum(-m_rate/100);
        ui->BW->setMaximum(m_rate/100);
        ui->lowCut->setMinimum(-m_rate/100);
        ui->lowCut->setMaximum(m_rate/100);

        if (ui->BW->value() < 0)
        {
            m_channelMarker.setSidebands(ChannelMarker::lsb);
            ui->glSpectrum->setLsbDisplay(true);
        }
        else
        {
            m_channelMarker.setSidebands(ChannelMarker::usb);
            ui->glSpectrum->setLsbDisplay(false);
        }

        ui->spanText->setText(tr("%1k").arg(s));
        ui->glSpectrum->setCenterFrequency(m_rate/2);
        ui->glSpectrum->setSampleRate(m_rate);
        ui->glSpectrum->setSsbSpectrum(true);
	}

	return true;
}

void SSBModGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void SSBModGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
		SSBMod::MsgConfigureChannelizer *msgChan = SSBMod::MsgConfigureChannelizer::create(
		        48000, m_settings.m_inputFrequencyOffset);
        m_ssbMod->getInputMessageQueue()->push(msgChan);

		SSBMod::MsgConfigureSSBMod *msg = SSBMod::MsgConfigureSSBMod::create(m_settings, force);
		m_ssbMod->getInputMessageQueue()->push(msg);
	}
}

void SSBModGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(m_settings.m_bandwidth * 2);
    m_channelMarker.setLowCutoff(m_settings.m_lowCutoff);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    setTitleColor(m_settings.m_rgbColor);

    if (m_settings.m_dsb) {
        m_channelMarker.setSidebands(ChannelMarker::dsb);
    } else {
        if (m_settings.m_bandwidth < 0) {
            m_channelMarker.setSidebands(ChannelMarker::lsb);
        } else {
            m_channelMarker.setSidebands(ChannelMarker::usb);
        }
    }

    m_channelMarker.blockSignals(false);

    setWindowTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    QString s = QString::number(m_settings.m_agcTime / 48, 'f', 0);
    ui->agcTimeText->setText(s);
    ui->agcTime->setValue(SSBModSettings::getAGCTimeConstantIndex(m_settings.m_agcTime / 48));
    displayAGCPowerThreshold();
    s = QString::number(m_settings.m_agcThresholdGate / 48, 'f', 0);
    ui->agcThresholdGateText->setText(s);
    ui->agcThresholdGate->setValue(m_settings.m_agcThresholdGate / 48);
    s = QString::number(m_settings.m_agcThresholdDelay / 48, 'f', 0);
    ui->agcThresholdDelayText->setText(s);
    ui->agcThresholdDelay->setValue(m_settings.m_agcThresholdDelay / 480);
    s = QString::number(m_settings.m_agcOrder, 'f', 2);
    ui->agcOrderText->setText(s);
    ui->agcOrder->setValue(roundf(m_settings.m_agcOrder * 100.0));

    ui->agc->setChecked(m_settings.m_agc);
    ui->audioBinaural->setChecked(m_settings.m_audioBinaural);
    ui->audioFlipChannels->setChecked(m_settings.m_audioFlipChannels);
    ui->audioMute->setChecked(m_settings.m_audioMute);
    ui->dsb->setChecked(m_settings.m_dsb);
    ui->playLoop->setChecked(m_settings.m_playLoop);

    ui->BW->setValue(roundf(m_settings.m_bandwidth/100.0));
    s = QString::number(m_settings.m_bandwidth/1000.0, 'f', 1);

    if (m_settings.m_dsb)
    {
        ui->BWText->setText(tr("%1%2k").arg(QChar(0xB1, 0x00)).arg(s));
    }
    else
    {
        ui->BWText->setText(tr("%1k").arg(s));
    }

    ui->deltaFrequency->setValue(m_settings.m_inputFrequencyOffset);

    ui->lowCut->setValue(m_settings.m_lowCutoff / 100.0);
    ui->lowCutText->setText(tr("%1k").arg(m_settings.m_lowCutoff / 1000.0));

    ui->spanLog2->setValue(m_settings.m_spanLog2);

    ui->toneFrequency->setValue(roundf(m_settings.m_toneFrequency / 10.0));
    ui->toneFrequencyText->setText(QString("%1k").arg(m_settings.m_toneFrequency / 1000.0, 0, 'f', 2));

    ui->volume->setValue(m_settings.m_volumeFactor * 10.0);
    ui->volumeText->setText(QString("%1").arg(m_settings.m_volumeFactor, 0, 'f', 1));

    blockApplySettings(false);
}

void SSBModGUI::displayAGCPowerThreshold()
{
    if (m_settings.m_agcThreshold == -99)
    {
        ui->agcThresholdText->setText("---");
    }
    else
    {
        QString s = QString::number(m_settings.m_agcThreshold, 'f', 0);
        ui->agcThresholdText->setText(s);
    }

    ui->agcThreshold->setValue(m_settings.m_agcThreshold);
}

void SSBModGUI::leaveEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(false);
	blockApplySettings(false);
}

void SSBModGUI::enterEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(true);
	blockApplySettings(false);
}

void SSBModGUI::tick()
{
    double powDb = CalcDb::dbPower(m_ssbMod->getMagSq());
	m_channelPowerDbAvg.feed(powDb);
	ui->channelPower->setText(tr("%1 dB").arg(m_channelPowerDbAvg.average(), 0, 'f', 1));

    if (((++m_tickCount & 0xf) == 0) && (m_modAFInput == SSBMod::SSBModInputFile))
    {
        SSBMod::MsgConfigureFileSourceStreamTiming* message = SSBMod::MsgConfigureFileSourceStreamTiming::create();
        m_ssbMod->getInputMessageQueue()->push(message);
    }
}

void SSBModGUI::updateWithStreamData()
{
    QTime recordLength(0, 0, 0, 0);
    recordLength = recordLength.addSecs(m_recordLength);
    QString s_time = recordLength.toString("hh:mm:ss");
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
    QString s_timems = t.toString("hh:mm:ss.zzz");
    QString s_time = t.toString("hh:mm:ss");
    ui->relTimeText->setText(s_timems);

    if (!m_enableNavTime)
    {
        float posRatio = (float) t_sec / (float) m_recordLength;
        ui->navTimeSlider->setValue((int) (posRatio * 100.0));
    }
}
