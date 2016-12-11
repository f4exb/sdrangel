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
#include "ui_nfmmodgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingswidget.h"
#include "dsp/dspengine.h"
#include "mainwindow.h"
#include "nfmmodgui.h"

const QString NFMModGUI::m_channelID = "sdrangel.channeltx.modnfm";

const int NFMModGUI::m_rfBW[] = {
	3000, 4000, 5000, 6250, 8330, 10000, 12500, 15000, 20000, 25000, 40000
};
const int NFMModGUI::m_nbRfBW = 11;

NFMModGUI* NFMModGUI::create(PluginAPI* pluginAPI, DeviceSinkAPI *deviceAPI)
{
    NFMModGUI* gui = new NFMModGUI(pluginAPI, deviceAPI);
	return gui;
}

void NFMModGUI::destroy()
{
}

void NFMModGUI::setName(const QString& name)
{
	setObjectName(name);
}

QString NFMModGUI::getName() const
{
	return objectName();
}

qint64 NFMModGUI::getCenterFrequency() const {
	return m_channelMarker.getCenterFrequency();
}

void NFMModGUI::setCenterFrequency(qint64 centerFrequency)
{
	m_channelMarker.setCenterFrequency(centerFrequency);
	applySettings();
}

void NFMModGUI::resetToDefaults()
{
	blockApplySettings(true);

	ui->rfBW->setCurrentIndex(6);
	ui->afBW->setValue(3);
	ui->fmDev->setValue(50);
	ui->toneFrequency->setValue(100);
	ui->volume->setValue(10);
	ui->deltaFrequency->setValue(0);

	blockApplySettings(false);
	applySettings();
}

QByteArray NFMModGUI::serialize() const
{
	SimpleSerializer s(1);
	s.writeS32(1, m_channelMarker.getCenterFrequency());
	s.writeS32(2, ui->rfBW->currentIndex());
	s.writeS32(3, ui->afBW->value());
	s.writeS32(4, ui->fmDev->value());
	s.writeU32(5, m_channelMarker.getColor().rgb());
	s.writeS32(6, ui->toneFrequency->value());
	s.writeS32(7, ui->volume->value());
	return s.final();
}

bool NFMModGUI::deserialize(const QByteArray& data)
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
		d.readS32(2, &tmp, 6);
		ui->rfBW->setCurrentIndex(tmp);
		d.readS32(3, &tmp, 3);
		ui->afBW->setValue(tmp);
		d.readS32(4, &tmp, 50);
		ui->fmDev->setValue(tmp);

        if(d.readU32(5, &u32tmp))
        {
			m_channelMarker.setColor(u32tmp);
        }

        d.readS32(6, &tmp, 100);
        ui->toneFrequency->setValue(tmp);
        d.readS32(7, &tmp, 10);
        ui->volume->setValue(tmp);

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

bool NFMModGUI::handleMessage(const Message& message)
{
    if (NFMMod::MsgReportFileSourceStreamData::match(message))
    {
        m_recordSampleRate = ((NFMMod::MsgReportFileSourceStreamData&)message).getSampleRate();
        m_recordLength = ((NFMMod::MsgReportFileSourceStreamData&)message).getRecordLength();
        m_samplesCount = 0;
        updateWithStreamData();
        return true;
    }
    else if (NFMMod::MsgReportFileSourceStreamTiming::match(message))
    {
        m_samplesCount = ((NFMMod::MsgReportFileSourceStreamTiming&)message).getSamplesCount();
        updateWithStreamTime();
        return true;
    }
    else
    {
        return false;
    }
}

void NFMModGUI::viewChanged()
{
	applySettings();
}

void NFMModGUI::handleSourceMessages()
{
    Message* message;

    while ((message = m_nfmMod->getOutputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

void NFMModGUI::on_deltaMinus_toggled(bool minus)
{
	int deltaFrequency = m_channelMarker.getCenterFrequency();
	bool minusDelta = (deltaFrequency < 0);

	if (minus ^ minusDelta) // sign change
	{
		m_channelMarker.setCenterFrequency(-deltaFrequency);
	}
}

void NFMModGUI::on_deltaFrequency_changed(quint64 value)
{
	if (ui->deltaMinus->isChecked()) {
		m_channelMarker.setCenterFrequency(-value);
	} else {
		m_channelMarker.setCenterFrequency(value);
	}
}

void NFMModGUI::on_rfBW_currentIndexChanged(int index)
{
	m_channelMarker.setBandwidth(m_rfBW[index]);
	applySettings();
}

void NFMModGUI::on_afBW_valueChanged(int value)
{
	ui->afBWText->setText(QString("%1k").arg(value));
	applySettings();
}

void NFMModGUI::on_fmDev_valueChanged(int value)
{
	ui->fmDevText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
	applySettings();
}

void NFMModGUI::on_volume_valueChanged(int value)
{
	ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
	applySettings();
}

void NFMModGUI::on_toneFrequency_valueChanged(int value)
{
    ui->toneFrequencyText->setText(QString("%1k").arg(value / 100.0, 0, 'f', 2));
    applySettings();
}

void NFMModGUI::on_audioMute_toggled(bool checked)
{
	applySettings();
}

void NFMModGUI::on_playLoop_toggled(bool checked)
{
	applySettings();
}

void NFMModGUI::on_play_toggled(bool checked)
{
    ui->tone->setEnabled(!checked); // release other source inputs
    ui->mic->setEnabled(!checked);
    ui->morseKeyer->setEnabled(!checked);
    m_modAFInput = checked ? NFMMod::NFMModInputFile : NFMMod::NFMModInputNone;
    NFMMod::MsgConfigureAFInput* message = NFMMod::MsgConfigureAFInput::create(m_modAFInput);
    m_nfmMod->getInputMessageQueue()->push(message);
    ui->navTimeSlider->setEnabled(!checked);
    m_enableNavTime = !checked;
}

void NFMModGUI::on_tone_toggled(bool checked)
{
    ui->play->setEnabled(!checked); // release other source inputs
    ui->mic->setEnabled(!checked);
    ui->morseKeyer->setEnabled(!checked);
    m_modAFInput = checked ? NFMMod::NFMModInputTone : NFMMod::NFMModInputNone;
    NFMMod::MsgConfigureAFInput* message = NFMMod::MsgConfigureAFInput::create(m_modAFInput);
    m_nfmMod->getInputMessageQueue()->push(message);
}

void NFMModGUI::on_morseKeyer_toggled(bool checked)
{
    ui->tone->setEnabled(!checked); // release other source inputs
    ui->mic->setEnabled(!checked);
    ui->play->setEnabled(!checked);
    m_modAFInput = checked ? NFMMod::NFMModInputCWTone : NFMMod::NFMModInputNone;
    NFMMod::MsgConfigureAFInput* message = NFMMod::MsgConfigureAFInput::create(m_modAFInput);
    m_nfmMod->getInputMessageQueue()->push(message);
}

void NFMModGUI::on_mic_toggled(bool checked)
{
    ui->play->setEnabled(!checked); // release other source inputs
    ui->tone->setEnabled(!checked); // release other source inputs
    ui->morseKeyer->setEnabled(!checked);
    m_modAFInput = checked ? NFMMod::NFMModInputAudio : NFMMod::NFMModInputNone;
    NFMMod::MsgConfigureAFInput* message = NFMMod::MsgConfigureAFInput::create(m_modAFInput);
    m_nfmMod->getInputMessageQueue()->push(message);
}

void NFMModGUI::on_navTimeSlider_valueChanged(int value)
{
    if (m_enableNavTime && ((value >= 0) && (value <= 100)))
    {
        int t_sec = (m_recordLength * value) / 100;
        QTime t(0, 0, 0, 0);
        t = t.addSecs(t_sec);

        NFMMod::MsgConfigureFileSourceSeek* message = NFMMod::MsgConfigureFileSourceSeek::create(value);
        m_nfmMod->getInputMessageQueue()->push(message);
    }
}

void NFMModGUI::on_showFileDialog_clicked(bool checked)
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

void NFMModGUI::configureFileName()
{
    qDebug() << "FileSourceGui::configureFileName: " << m_fileName.toStdString().c_str();
    NFMMod::MsgConfigureFileSourceName* message = NFMMod::MsgConfigureFileSourceName::create(m_fileName);
    m_nfmMod->getInputMessageQueue()->push(message);
}

void NFMModGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
}

void NFMModGUI::onMenuDoubleClicked()
{
	if(!m_basicSettingsShown) {
		m_basicSettingsShown = true;
		BasicChannelSettingsWidget* bcsw = new BasicChannelSettingsWidget(&m_channelMarker, this);
		bcsw->show();
	}
}

NFMModGUI::NFMModGUI(PluginAPI* pluginAPI, DeviceSinkAPI *deviceAPI, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::NFMModGUI),
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
    m_modAFInput(NFMMod::NFMModInputNone)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);

    blockApplySettings(true);
    ui->rfBW->clear();
    for (int i = 0; i < m_nbRfBW; i++) {
        ui->rfBW->addItem(QString("%1").arg(m_rfBW[i] / 1000.0, 0, 'f', 2));
    }
    ui->rfBW->setCurrentIndex(6);
    blockApplySettings(false);


	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(menuDoubleClickEvent()), this, SLOT(onMenuDoubleClicked()));

	m_nfmMod = new NFMMod();
	m_channelizer = new UpChannelizer(m_nfmMod);
	m_threadedChannelizer = new ThreadedBasebandSampleSource(m_channelizer, this);
	//m_pluginAPI->addThreadedSink(m_threadedChannelizer);
    m_deviceAPI->addThreadedSource(m_threadedChannelizer);

	connect(&m_pluginAPI->getMainWindow()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

	ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));

	//m_channelMarker = new ChannelMarker(this);
	m_channelMarker.setColor(Qt::red);
	m_channelMarker.setBandwidth(12500);
	m_channelMarker.setCenterFrequency(0);
	m_channelMarker.setVisible(true);

	connect(&m_channelMarker, SIGNAL(changed()), this, SLOT(viewChanged()));

	m_deviceAPI->registerChannelInstance(m_channelID, this);
    m_deviceAPI->addChannelMarker(&m_channelMarker);
    m_deviceAPI->addRollupWidget(this);

    ui->play->setEnabled(false);
    ui->play->setChecked(false);
    ui->tone->setChecked(false);
    ui->mic->setChecked(false);

    ui->cwKeyerGUI->setBuddies(m_nfmMod->getInputMessageQueue(), m_nfmMod->getCWKeyer());

	applySettings();

	connect(m_nfmMod->getOutputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
	connect(m_nfmMod, SIGNAL(levelChanged(qreal, qreal, int)), ui->volumeMeter, SLOT(levelChanged(qreal, qreal, int)));
}

NFMModGUI::~NFMModGUI()
{
    m_deviceAPI->removeChannelInstance(this);
	m_deviceAPI->removeThreadedSource(m_threadedChannelizer);
	delete m_threadedChannelizer;
	delete m_channelizer;
	delete m_nfmMod;
	//delete m_channelMarker;
	delete ui;
}

void NFMModGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void NFMModGUI::applySettings()
{
	if (m_doApplySettings)
	{
		setTitleColor(m_channelMarker.getColor());

		m_channelizer->configure(m_channelizer->getInputMessageQueue(),
			48000,
			m_channelMarker.getCenterFrequency());

		ui->deltaFrequency->setValue(abs(m_channelMarker.getCenterFrequency()));
		ui->deltaMinus->setChecked(m_channelMarker.getCenterFrequency() < 0);

		m_nfmMod->configure(m_nfmMod->getInputMessageQueue(),
			m_rfBW[ui->rfBW->currentIndex()],
			ui->afBW->value() * 1000.0,
			ui->fmDev->value() * 100.0f, // value is in '100 Hz
			ui->toneFrequency->value() * 10.0f,
			ui->volume->value() / 10.0f,
			ui->audioMute->isChecked(),
			ui->playLoop->isChecked());
	}
}

void NFMModGUI::leaveEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(false);
	blockApplySettings(false);
}

void NFMModGUI::enterEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(true);
	blockApplySettings(false);
}

void NFMModGUI::tick()
{
	Real powDb = CalcDb::dbPower(m_nfmMod->getMagSq());
	m_channelPowerDbAvg.feed(powDb);
	ui->channelPower->setText(QString::number(m_channelPowerDbAvg.average(), 'f', 1));

    if (((++m_tickCount & 0xf) == 0) && (m_modAFInput == NFMMod::NFMModInputFile))
    {
        NFMMod::MsgConfigureFileSourceStreamTiming* message = NFMMod::MsgConfigureFileSourceStreamTiming::create();
        m_nfmMod->getInputMessageQueue()->push(message);
    }
}

void NFMModGUI::updateWithStreamData()
{
    QTime recordLength(0, 0, 0, 0);
    recordLength = recordLength.addSecs(m_recordLength);
    QString s_time = recordLength.toString("hh:mm:ss");
    ui->recordLengthText->setText(s_time);
    updateWithStreamTime();
}

void NFMModGUI::updateWithStreamTime()
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
