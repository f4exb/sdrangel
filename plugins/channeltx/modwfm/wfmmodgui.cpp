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

#include "device/devicesinkapi.h"
#include "dsp/upchannelizer.h"

#include "dsp/threadedbasebandsamplesource.h"
#include "ui_wfmmodgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingswidget.h"
#include "dsp/dspengine.h"
#include "mainwindow.h"
#include "wfmmodgui.h"

const QString WFMModGUI::m_channelID = "sdrangel.channeltx.modwfm";

WFMModGUI* WFMModGUI::create(PluginAPI* pluginAPI, DeviceSinkAPI *deviceAPI)
{
    WFMModGUI* gui = new WFMModGUI(pluginAPI, deviceAPI);
	return gui;
}

void WFMModGUI::destroy()
{
}

void WFMModGUI::setName(const QString& name)
{
	setObjectName(name);
}

QString WFMModGUI::getName() const
{
	return objectName();
}

qint64 WFMModGUI::getCenterFrequency() const {
	return m_channelMarker.getCenterFrequency();
}

void WFMModGUI::setCenterFrequency(qint64 centerFrequency)
{
	m_channelMarker.setCenterFrequency(centerFrequency);
	applySettings();
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
    else
    {
        return false;
    }
}

void WFMModGUI::viewChanged()
{
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
    m_modAFInput = checked ? WFMMod::WFMModInputFile : WFMMod::WFMModInputNone;
    WFMMod::MsgConfigureAFInput* message = WFMMod::MsgConfigureAFInput::create(m_modAFInput);
    m_wfmMod->getInputMessageQueue()->push(message);
    ui->navTimeSlider->setEnabled(!checked);
    m_enableNavTime = !checked;
}

void WFMModGUI::on_tone_toggled(bool checked)
{
    ui->play->setEnabled(!checked); // release other source inputs
    ui->mic->setEnabled(!checked);
    ui->morseKeyer->setEnabled(!checked);
    m_modAFInput = checked ? WFMMod::WFMModInputTone : WFMMod::WFMModInputNone;
    WFMMod::MsgConfigureAFInput* message = WFMMod::MsgConfigureAFInput::create(m_modAFInput);
    m_wfmMod->getInputMessageQueue()->push(message);
}

void WFMModGUI::on_morseKeyer_toggled(bool checked)
{
    ui->tone->setEnabled(!checked); // release other source inputs
    ui->mic->setEnabled(!checked);
    ui->play->setEnabled(!checked);
    m_modAFInput = checked ? WFMMod::WFMModInputCWTone : WFMMod::WFMModInputNone;
    WFMMod::MsgConfigureAFInput* message = WFMMod::MsgConfigureAFInput::create(m_modAFInput);
    m_wfmMod->getInputMessageQueue()->push(message);
}

void WFMModGUI::on_mic_toggled(bool checked)
{
    ui->play->setEnabled(!checked); // release other source inputs
    ui->tone->setEnabled(!checked); // release other source inputs
    ui->morseKeyer->setEnabled(!checked);
    m_modAFInput = checked ? WFMMod::WFMModInputAudio : WFMMod::WFMModInputNone;
    WFMMod::MsgConfigureAFInput* message = WFMMod::MsgConfigureAFInput::create(m_modAFInput);
    m_wfmMod->getInputMessageQueue()->push(message);
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

void WFMModGUI::on_showFileDialog_clicked(bool checked __attribute__((unused)))
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

void WFMModGUI::configureFileName()
{
    qDebug() << "FileSourceGui::configureFileName: " << m_fileName.toStdString().c_str();
    WFMMod::MsgConfigureFileSourceName* message = WFMMod::MsgConfigureFileSourceName::create(m_fileName);
    m_wfmMod->getInputMessageQueue()->push(message);
}

void WFMModGUI::onWidgetRolled(QWidget* widget __attribute__((unused)), bool rollDown __attribute__((unused)))
{
}

void WFMModGUI::onMenuDoubleClicked()
{
	if(!m_basicSettingsShown) {
		m_basicSettingsShown = true;
		BasicChannelSettingsWidget* bcsw = new BasicChannelSettingsWidget(&m_channelMarker, this);
		bcsw->show();
	}
}

WFMModGUI::WFMModGUI(PluginAPI* pluginAPI, DeviceSinkAPI *deviceAPI, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::WFMModGUI),
	m_pluginAPI(pluginAPI),
	m_deviceAPI(deviceAPI),
	m_channelMarker(this),
	m_basicSettingsShown(false),
	m_doApplySettings(true),
	m_channelPowerDbAvg(20,0),
    m_recordLength(0),
    m_recordSampleRate(48000),
    m_samplesCount(0),
    m_tickCount(0),
    m_enableNavTime(false),
    m_modAFInput(WFMMod::WFMModInputNone)
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
	connect(this, SIGNAL(menuDoubleClickEvent()), this, SLOT(onMenuDoubleClicked()));

	m_wfmMod = new WFMMod();
	m_wfmMod->setMessageQueueToGUI(getInputMessageQueue());
	m_channelizer = new UpChannelizer(m_wfmMod);
	m_threadedChannelizer = new ThreadedBasebandSampleSource(m_channelizer, this);
	//m_pluginAPI->addThreadedSink(m_threadedChannelizer);
    m_deviceAPI->addThreadedSource(m_threadedChannelizer);

	connect(&m_pluginAPI->getMainWindow()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

	//m_channelMarker = new ChannelMarker(this);
//	m_channelMarker.setColor(Qt::blue);
//	m_channelMarker.setBandwidth(m_rfBW[ui->rfBW->currentIndex()]);
//	m_channelMarker.setCenterFrequency(0);
//	m_channelMarker.setVisible(true);

    m_channelMarker.setTitle("WFM Modulator");
    m_channelMarker.setVisible(true);

	connect(&m_channelMarker, SIGNAL(changed()), this, SLOT(viewChanged()));

	m_deviceAPI->registerChannelInstance(m_channelID, this);
    m_deviceAPI->addChannelMarker(&m_channelMarker);
    m_deviceAPI->addRollupWidget(this);

    ui->play->setEnabled(false);
    ui->play->setChecked(false);
    ui->tone->setChecked(false);
    ui->mic->setChecked(false);

    ui->cwKeyerGUI->setBuddies(m_wfmMod->getInputMessageQueue(), m_wfmMod->getCWKeyer());

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setCWKeyerGUI(ui->cwKeyerGUI);

	connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
	connect(m_wfmMod, SIGNAL(levelChanged(qreal, qreal, int)), ui->volumeMeter, SLOT(levelChanged(qreal, qreal, int)));

	displaySettings();
    applySettings(true);
}

WFMModGUI::~WFMModGUI()
{
    m_deviceAPI->removeChannelInstance(this);
	m_deviceAPI->removeThreadedSource(m_threadedChannelizer);
	delete m_threadedChannelizer;
	delete m_channelizer;
	delete m_wfmMod;
	//delete m_channelMarker;
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

		m_channelizer->configure(m_channelizer->getInputMessageQueue(),
		   requiredBW(WFMModSettings::getRFBW(ui->rfBW->currentIndex())),
		   m_channelMarker.getCenterFrequency());

		ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

		WFMMod::MsgConfigureWFMMod *msgConf = WFMMod::MsgConfigureWFMMod::create(m_settings, force);
		m_wfmMod->getInputMessageQueue()->push(msgConf);
	}
}

void WFMModGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    setTitleColor(m_settings.m_rgbColor);
    m_channelMarker.blockSignals(false);

    setWindowTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

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

    blockApplySettings(false);
}

void WFMModGUI::leaveEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(false);
	blockApplySettings(false);
}

void WFMModGUI::enterEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(true);
	blockApplySettings(false);
}

void WFMModGUI::tick()
{
    double powDb = CalcDb::dbPower(m_wfmMod->getMagSq());
	m_channelPowerDbAvg.feed(powDb);
	ui->channelPower->setText(tr("%1 dB").arg(m_channelPowerDbAvg.average(), 0, 'f', 1));

    if (((++m_tickCount & 0xf) == 0) && (m_modAFInput == WFMMod::WFMModInputFile))
    {
        WFMMod::MsgConfigureFileSourceStreamTiming* message = WFMMod::MsgConfigureFileSourceStreamTiming::create();
        m_wfmMod->getInputMessageQueue()->push(message);
    }
}

void WFMModGUI::updateWithStreamData()
{
    QTime recordLength(0, 0, 0, 0);
    recordLength = recordLength.addSecs(m_recordLength);
    QString s_time = recordLength.toString("hh:mm:ss");
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
    QString s_timems = t.toString("hh:mm:ss.zzz");
    QString s_time = t.toString("hh:mm:ss");
    ui->relTimeText->setText(s_timems);

    if (!m_enableNavTime)
    {
        float posRatio = (float) t_sec / (float) m_recordLength;
        ui->navTimeSlider->setValue((int) (posRatio * 100.0));
    }
}
