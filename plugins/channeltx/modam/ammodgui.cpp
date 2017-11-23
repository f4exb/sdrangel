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
#include "device/deviceuiset.h"
#include "dsp/upchannelizer.h"

#include "ui_ammodgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "dsp/dspengine.h"
#include "mainwindow.h"

AMModGUI* AMModGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
	AMModGUI* gui = new AMModGUI(pluginAPI, deviceUISet, channelTx);
	return gui;
}

void AMModGUI::destroy()
{
    delete this;
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

void AMModGUI::on_showFileDialog_clicked(bool checked __attribute__((unused)))
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

void AMModGUI::onWidgetRolled(QWidget* widget __attribute__((unused)), bool rollDown __attribute__((unused)))
{
}

AMModGUI::AMModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::AMModGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
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

	m_amMod = (AMMod*) channelTx; //new AMMod(m_deviceUISet->m_deviceSinkAPI);
	m_amMod->setMessageQueueToGUI(getInputMessageQueue());

	connect(&MainWindow::getInstance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::yellow);
    m_channelMarker.setBandwidth(5000);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("AM Modulator");
    m_channelMarker.setUDPAddress("127.0.0.1");
    m_channelMarker.setUDPSendPort(9999);
    m_channelMarker.blockSignals(false);
	m_channelMarker.setVisible(true); // activate signal on the last setting only

	m_settings.setChannelMarker(&m_channelMarker);
	m_settings.setCWKeyerGUI(ui->cwKeyerGUI);

	m_deviceUISet->registerTxChannelInstance(AMMod::m_channelIdURI, this);
	m_deviceUISet->addChannelMarker(&m_channelMarker);
	m_deviceUISet->addRollupWidget(this);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));

    ui->play->setEnabled(false);
    ui->play->setChecked(false);
    ui->tone->setChecked(false);
    ui->morseKeyer->setChecked(false);
    ui->mic->setChecked(false);

    ui->cwKeyerGUI->setBuddies(m_amMod->getInputMessageQueue(), m_amMod->getCWKeyer());

	connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
	connect(m_amMod, SIGNAL(levelChanged(qreal, qreal, int)), ui->volumeMeter, SLOT(levelChanged(qreal, qreal, int)));

	displaySettings();
    applySettings(true);
}

AMModGUI::~AMModGUI()
{
    m_deviceUISet->removeTxChannelInstance(this);
	delete m_amMod; // TODO: check this: when the GUI closes it has to delete the modulator
	delete ui;
}

void AMModGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void AMModGUI::applySettings(bool force __attribute((unused)))
{
	if (m_doApplySettings)
	{
		setTitleColor(m_channelMarker.getColor());

		AMMod::MsgConfigureChannelizer *msgConfigure = AMMod::MsgConfigureChannelizer::create(
		        48000, m_channelMarker.getCenterFrequency());
        m_amMod->getInputMessageQueue()->push(msgConfigure);

        AMMod::MsgConfigureAMMod* message = AMMod::MsgConfigureAMMod::create( m_settings, force);
        m_amMod->getInputMessageQueue()->push(message);
	}
}

void AMModGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());

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

    blockApplySettings(false);
}

void AMModGUI::leaveEvent(QEvent*)
{
	m_channelMarker.setHighlighted(false);
}

void AMModGUI::enterEvent(QEvent*)
{
	m_channelMarker.setHighlighted(true);
}

void AMModGUI::tick()
{
    double powDb = CalcDb::dbPower(m_amMod->getMagSq());
	m_channelPowerDbAvg.feed(powDb);
	ui->channelPower->setText(tr("%1 dB").arg(m_channelPowerDbAvg.average(), 0, 'f', 1));

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
