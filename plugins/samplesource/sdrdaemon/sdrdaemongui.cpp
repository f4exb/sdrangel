///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#include <QDebug>

#include <QTime>
#include <QDateTime>
#include <QString>
#include <QFileDialog>
#include "ui_sdrdaemongui.h"
#include "plugin/pluginapi.h"
#include "gui/colormapper.h"
#include "dsp/dspengine.h"
#include "mainwindow.h"

#include "sdrdaemongui.h"

SDRdaemonGui::SDRdaemonGui(PluginAPI* pluginAPI, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::SDRdaemonGui),
	m_pluginAPI(pluginAPI),
	m_settings(),
	m_sampleSource(NULL),
	m_acquisition(false),
	m_fileName("..."),
	m_sampleRate(0),
	m_centerFrequency(0),
	m_startingTimeStamp(0),
	m_samplesCount(0),
	m_tickCount(0)
{
	ui->setupUi(this);
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));
	ui->centerFrequency->setValueRange(7, 0, pow(10,7));
	ui->fileNameText->setText(m_fileName);
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	connect(&(m_pluginAPI->getMainWindow()->getMasterTimer()), SIGNAL(timeout()), this, SLOT(tick()));
	displaySettings();

	m_sampleSource = new SDRdaemonInput(m_pluginAPI->getMainWindow()->getMasterTimer());
	connect(m_sampleSource->getOutputMessageQueueToGUI(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
	DSPEngine::instance()->setSource(m_sampleSource);
}

SDRdaemonGui::~SDRdaemonGui()
{
	delete ui;
}

void SDRdaemonGui::destroy()
{
	delete this;
}

void SDRdaemonGui::setName(const QString& name)
{
	setObjectName(name);
}

QString SDRdaemonGui::getName() const
{
	return objectName();
}

void SDRdaemonGui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

qint64 SDRdaemonGui::getCenterFrequency() const
{
	return m_centerFrequency;
}

void SDRdaemonGui::setCenterFrequency(qint64 centerFrequency)
{
	m_centerFrequency = centerFrequency;
	displaySettings();
	sendSettings();
}

QByteArray SDRdaemonGui::serialize() const
{
	return m_settings.serialize();
}

bool SDRdaemonGui::deserialize(const QByteArray& data)
{
	if(m_settings.deserialize(data)) {
		displaySettings();
		sendSettings();
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}

bool SDRdaemonGui::handleMessage(const Message& message)
{
	if (SDRdaemonInput::MsgReportSDRdaemonAcquisition::match(message))
	{
		m_acquisition = ((SDRdaemonInput::MsgReportSDRdaemonAcquisition&)message).getAcquisition();
		updateWithAcquisition();
		return true;
	}
	else if (SDRdaemonInput::MsgReportSDRdaemonStreamData::match(message))
	{
		m_sampleRate = ((SDRdaemonInput::MsgReportSDRdaemonStreamData&)message).getSampleRate();
		m_centerFrequency = ((SDRdaemonInput::MsgReportSDRdaemonStreamData&)message).getCenterFrequency();
		m_startingTimeStamp = ((SDRdaemonInput::MsgReportSDRdaemonStreamData&)message).getStartingTimeStamp();
		updateWithStreamData();
		return true;
	}
	else if (SDRdaemonInput::MsgReportSDRdaemonStreamTiming::match(message))
	{
		m_samplesCount = ((SDRdaemonInput::MsgReportSDRdaemonStreamTiming&)message).getSamplesCount();
		updateWithStreamTime();
		return true;
	}
	else
	{
		return false;
	}
}

void SDRdaemonGui::handleSourceMessages()
{
	Message* message;

	while ((message = m_sampleSource->getOutputMessageQueueToGUI()->pop()) != 0)
	{
		qDebug("SDRdaemonGui::handleSourceMessages: message: %s", message->getIdentifier());

		if (handleMessage(*message))
		{
			delete message;
		}
	}
}

void SDRdaemonGui::displaySettings()
{
}

void SDRdaemonGui::sendSettings()
{
}

void SDRdaemonGui::updateHardware()
{
}

void SDRdaemonGui::on_play_toggled(bool checked)
{
	SDRdaemonInput::MsgConfigureSDRdaemonWork* message = SDRdaemonInput::MsgConfigureSDRdaemonWork::create(checked);
	m_sampleSource->getInputMessageQueue()->push(message);
}

void SDRdaemonGui::on_showFileDialog_clicked(bool checked)
{
	QString fileName = QFileDialog::getOpenFileName(this,
	    tr("Open I/Q record file"), ".", tr("SDR I/Q Files (*.sdriq)"));

	if (fileName != "")
	{
		m_fileName = fileName;
		ui->fileNameText->setText(m_fileName);
		configureFileName();
	}
}

void SDRdaemonGui::configureFileName()
{
	qDebug() << "SDRdaemonGui::configureFileName: " << m_fileName.toStdString().c_str();
	SDRdaemonInput::MsgConfigureSDRdaemonName* message = SDRdaemonInput::MsgConfigureSDRdaemonName::create(m_fileName);
	m_sampleSource->getInputMessageQueue()->push(message);
}

void SDRdaemonGui::updateWithAcquisition()
{
	ui->play->setEnabled(m_acquisition);
	ui->play->setChecked(m_acquisition);
	ui->showFileDialog->setEnabled(!m_acquisition);
}

void SDRdaemonGui::updateWithStreamData()
{
	ui->centerFrequency->setValue(m_centerFrequency/1000);
	QString s = QString::number(m_sampleRate/1000.0, 'f', 0);
	ui->sampleRateText->setText(tr("%1k").arg(s));
	ui->play->setEnabled(m_acquisition);
	updateWithStreamTime(); // TODO: remove when time data is implemented
}

void SDRdaemonGui::updateWithStreamTime()
{
	int t_sec = 0;
	int t_msec = 0;

	if (m_sampleRate > 0){
		t_msec = ((m_samplesCount * 1000) / m_sampleRate) % 1000;
		t_sec = m_samplesCount / m_sampleRate;
	}

	QTime t(0, 0, 0, 0);
	t = t.addSecs(t_sec);
	t = t.addMSecs(t_msec);
	QString s_time = t.toString("hh:mm:ss.zzz");
	ui->relTimeText->setText(s_time);

	quint64 startingTimeStampMsec = m_startingTimeStamp * 1000;
	QDateTime dt = QDateTime::fromMSecsSinceEpoch(startingTimeStampMsec);
	dt = dt.addSecs(t_sec);
	dt = dt.addMSecs(t_msec);
	QString s_date = dt.toString("yyyyMMdd hh.mm.ss.zzz");
	ui->absTimeText->setText(s_date);
}

void SDRdaemonGui::tick()
{
	if ((++m_tickCount & 0xf) == 0) {
		SDRdaemonInput::MsgConfigureSDRdaemonStreamTiming* message = SDRdaemonInput::MsgConfigureSDRdaemonStreamTiming::create();
		m_sampleSource->getInputMessageQueue()->push(message);
	}
}
