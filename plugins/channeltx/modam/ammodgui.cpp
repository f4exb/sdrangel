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

#include "ammodgui.h"

#include "device/devicesinkapi.h"
#include "dsp/upchannelizer.h"

#include "dsp/threadedbasebandsamplesource.h"
#include "ui_ammodgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingswidget.h"
#include "dsp/dspengine.h"
#include "mainwindow.h"

const QString AMModGUI::m_channelID = "sdrangel.channeltx.modam";

AMModGUI* AMModGUI::create(PluginAPI* pluginAPI, DeviceSinkAPI *deviceAPI)
{
	AMModGUI* gui = new AMModGUI(pluginAPI, deviceAPI);
	return gui;
}

void AMModGUI::destroy()
{
}

void AMModGUI::setName(const QString& name)
{
	setObjectName(name);
}

QString AMModGUI::getName() const
{
	return objectName();
}

qint64 AMModGUI::getCenterFrequency() const {
	return m_channelMarker.getCenterFrequency();
}

void AMModGUI::setCenterFrequency(qint64 centerFrequency)
{
	m_channelMarker.setCenterFrequency(centerFrequency);
	applySettings();
}

void AMModGUI::resetToDefaults()
{
	blockApplySettings(true);

	ui->rfBW->setValue(50);
	ui->modPercent->setValue(20);
	ui->volume->setValue(10);
	ui->toneFrequency->setValue(100);
	ui->deltaFrequency->setValue(0);

	blockApplySettings(false);
	applySettings();
}

QByteArray AMModGUI::serialize() const
{
	SimpleSerializer s(1);

	s.writeS32(1, m_channelMarker.getCenterFrequency());
	s.writeS32(2, ui->rfBW->value());
	s.writeS32(3, ui->toneFrequency->value());
	s.writeS32(4, ui->modPercent->value());
	s.writeU32(5, m_channelMarker.getColor().rgb());
	s.writeS32(6, ui->volume->value());
	s.writeBlob(7, ui->cwKeyerGUI->serialize());

	return s.final();
}

bool AMModGUI::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if(!d.isValid())
    {
		resetToDefaults();
		return false;
	}

	if(d.getVersion() == 1)
    {
		QByteArray bytetmp;
		quint32 u32tmp;
		qint32 tmp;

		blockApplySettings(true);
		m_channelMarker.blockSignals(true);

		d.readS32(1, &tmp, 0);
		m_channelMarker.setCenterFrequency(tmp);
		d.readS32(2, &tmp, 4);
		ui->rfBW->setValue(tmp);
		d.readS32(3, &tmp, 100);
		ui->toneFrequency->setValue(tmp);
		d.readS32(4, &tmp, 20);
		ui->modPercent->setValue(tmp);

        if(d.readU32(5, &u32tmp))
        {
			m_channelMarker.setColor(u32tmp);
        }

        d.readS32(6, &tmp, 10);
        ui->volume->setValue(tmp);

        d.readBlob(7, &bytetmp);
        ui->cwKeyerGUI->deserialize(bytetmp);

        blockApplySettings(false);
		m_channelMarker.blockSignals(false);

		applySettings();
		return true;
	}
    else
    {
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
    else
    {
        return false;
    }
}

void AMModGUI::viewChanged()
{
	applySettings();
}

void AMModGUI::handleSourceMessages()
{
    Message* message;

    while ((message = m_amMod->getOutputMessageQueue()->pop()) != 0)
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
}

void AMModGUI::on_rfBW_valueChanged(int value)
{
	ui->rfBWText->setText(QString("%1 kHz").arg(value / 10.0, 0, 'f', 1));
	m_channelMarker.setBandwidth(value * 100);
	applySettings();
}

void AMModGUI::on_modPercent_valueChanged(int value)
{
	ui->modPercentText->setText(QString("%1").arg(value));
	applySettings();
}

void AMModGUI::on_volume_valueChanged(int value)
{
    ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
    applySettings();
}

void AMModGUI::on_toneFrequency_valueChanged(int value)
{
    ui->toneFrequencyText->setText(QString("%1k").arg(value / 100.0, 0, 'f', 2));
    applySettings();
}


void AMModGUI::on_channelMute_toggled(bool checked)
{
	applySettings();
}

void AMModGUI::on_playLoop_toggled(bool checked)
{
	applySettings();
}

void AMModGUI::on_play_toggled(bool checked)
{
    ui->tone->setEnabled(!checked); // release other source inputs
    ui->morseKeyer->setEnabled(!checked);
    ui->mic->setEnabled(!checked);
    m_modAFInput = checked ? AMMod::AMModInputFile : AMMod::AMModInputNone;
    AMMod::MsgConfigureAFInput* message = AMMod::MsgConfigureAFInput::create(m_modAFInput);
    m_amMod->getInputMessageQueue()->push(message);
    ui->navTimeSlider->setEnabled(!checked);
    m_enableNavTime = !checked;
}

void AMModGUI::on_tone_toggled(bool checked)
{
    ui->play->setEnabled(!checked); // release other source inputs
    ui->morseKeyer->setEnabled(!checked);
    ui->mic->setEnabled(!checked);
    m_modAFInput = checked ? AMMod::AMModInputTone : AMMod::AMModInputNone;
    AMMod::MsgConfigureAFInput* message = AMMod::MsgConfigureAFInput::create(m_modAFInput);
    m_amMod->getInputMessageQueue()->push(message);
}

void AMModGUI::on_morseKeyer_toggled(bool checked)
{
    ui->play->setEnabled(!checked); // release other source inputs
    ui->tone->setEnabled(!checked); // release other source inputs
    ui->mic->setEnabled(!checked);
    m_modAFInput = checked ? AMMod::AMModInputCWTone : AMMod::AMModInputNone;
    AMMod::MsgConfigureAFInput* message = AMMod::MsgConfigureAFInput::create(m_modAFInput);
    m_amMod->getInputMessageQueue()->push(message);
}

void AMModGUI::on_mic_toggled(bool checked)
{
    ui->play->setEnabled(!checked); // release other source inputs
    ui->morseKeyer->setEnabled(!checked);
    ui->tone->setEnabled(!checked); // release other source inputs
    m_modAFInput = checked ? AMMod::AMModInputAudio : AMMod::AMModInputNone;
    AMMod::MsgConfigureAFInput* message = AMMod::MsgConfigureAFInput::create(m_modAFInput);
    m_amMod->getInputMessageQueue()->push(message);
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

void AMModGUI::configureFileName()
{
    qDebug() << "FileSourceGui::configureFileName: " << m_fileName.toStdString().c_str();
    AMMod::MsgConfigureFileSourceName* message = AMMod::MsgConfigureFileSourceName::create(m_fileName);
    m_amMod->getInputMessageQueue()->push(message);
}

void AMModGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
}

void AMModGUI::onMenuDoubleClicked()
{
	if(!m_basicSettingsShown) {
		m_basicSettingsShown = true;
		BasicChannelSettingsWidget* bcsw = new BasicChannelSettingsWidget(&m_channelMarker, this);
		bcsw->show();
	}
}

AMModGUI::AMModGUI(PluginAPI* pluginAPI, DeviceSinkAPI *deviceAPI, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::AMModGUI),
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
    m_modAFInput(AMMod::AMModInputNone)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(menuDoubleClickEvent()), this, SLOT(onMenuDoubleClicked()));

	m_amMod = new AMMod();
	m_channelizer = new UpChannelizer(m_amMod);
	m_threadedChannelizer = new ThreadedBasebandSampleSource(m_channelizer, this);
	//m_pluginAPI->addThreadedSink(m_threadedChannelizer);
    m_deviceAPI->addThreadedSource(m_threadedChannelizer);

	connect(&m_pluginAPI->getMainWindow()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

	//m_channelMarker = new ChannelMarker(this);
	m_channelMarker.setColor(Qt::yellow);
	m_channelMarker.setBandwidth(5000);
	m_channelMarker.setCenterFrequency(0);
	m_channelMarker.setVisible(true);

	connect(&m_channelMarker, SIGNAL(changed()), this, SLOT(viewChanged()));

	m_deviceAPI->registerChannelInstance(m_channelID, this);
    m_deviceAPI->addChannelMarker(&m_channelMarker);
    m_deviceAPI->addRollupWidget(this);

    ui->play->setEnabled(false);
    ui->play->setChecked(false);
    ui->tone->setChecked(false);
    ui->morseKeyer->setChecked(false);
    ui->mic->setChecked(false);

    ui->cwKeyerGUI->setBuddies(m_amMod->getInputMessageQueue(), m_amMod->getCWKeyer());

	applySettings();

	connect(m_amMod->getOutputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
	connect(m_amMod, SIGNAL(levelChanged(qreal, qreal, int)), ui->volumeMeter, SLOT(levelChanged(qreal, qreal, int)));
}

AMModGUI::~AMModGUI()
{
    m_deviceAPI->removeChannelInstance(this);
	m_deviceAPI->removeThreadedSource(m_threadedChannelizer);
	delete m_threadedChannelizer;
	delete m_channelizer;
	delete m_amMod;
	//delete m_channelMarker;
	delete ui;
}

void AMModGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void AMModGUI::applySettings()
{
	if (m_doApplySettings)
	{
		setTitleColor(m_channelMarker.getColor());

		m_channelizer->configure(m_channelizer->getInputMessageQueue(),
			48000,
			m_channelMarker.getCenterFrequency());

		ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

		m_amMod->configure(m_amMod->getInputMessageQueue(),
			ui->rfBW->value() * 100.0,
			ui->modPercent->value() / 100.0f,
			ui->toneFrequency->value() * 10.0f,
			ui->volume->value() / 10.0f ,
			ui->channelMute->isChecked(),
			ui->playLoop->isChecked());
	}
}

void AMModGUI::leaveEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(false);
	blockApplySettings(false);
}

void AMModGUI::enterEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(true);
	blockApplySettings(false);
}

void AMModGUI::tick()
{
    double powDb = CalcDb::dbPower(m_amMod->getMagSq());
	m_channelPowerDbAvg.feed(powDb);
	ui->channelPower->setText(QString::number(m_channelPowerDbAvg.average(), 'f', 1));

    if (((++m_tickCount & 0xf) == 0) && (m_modAFInput == AMMod::AMModInputFile))
    {
        AMMod::MsgConfigureFileSourceStreamTiming* message = AMMod::MsgConfigureFileSourceStreamTiming::create();
        m_amMod->getInputMessageQueue()->push(message);
    }
}

void AMModGUI::updateWithStreamData()
{
    QTime recordLength(0, 0, 0, 0);
    recordLength = recordLength.addSecs(m_recordLength);
    QString s_time = recordLength.toString("hh:mm:ss");
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
    QString s_timems = t.toString("hh:mm:ss.zzz");
    QString s_time = t.toString("hh:mm:ss");
    ui->relTimeText->setText(s_timems);

    if (!m_enableNavTime)
    {
        float posRatio = (float) t_sec / (float) m_recordLength;
        ui->navTimeSlider->setValue((int) (posRatio * 100.0));
    }
}
