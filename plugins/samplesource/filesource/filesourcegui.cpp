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
#include "ui_filesourcegui.h"
#include "plugin/pluginapi.h"
#include "gui/colormapper.h"
#include "dsp/dspengine.h"
#include "mainwindow.h"

#include "filesourcegui.h"

FileSourceGui::FileSourceGui(PluginAPI* pluginAPI, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::FileSourceGui),
	m_pluginAPI(pluginAPI),
	m_settings(),
	m_sampleSource(NULL),
	m_acquisition(false),
	m_fileName("..."),
	m_sampleRate(0),
	m_centerFrequency(0),
	m_recordLength(0),
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
	ui->navTimeSlider->setEnabled(false);
	ui->playLoop->setChecked(true); // FIXME: always play in a loop
	ui->playLoop->setEnabled(false);

	m_sampleSource = new FileSourceInput(m_pluginAPI->getMainWindow()->getMasterTimer());
	connect(m_sampleSource->getOutputMessageQueueToGUI(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
	DSPEngine::instance()->setSource(m_sampleSource);
}

FileSourceGui::~FileSourceGui()
{
	delete ui;
}

void FileSourceGui::destroy()
{
	delete this;
}

void FileSourceGui::setName(const QString& name)
{
	setObjectName(name);
}

QString FileSourceGui::getName() const
{
	return objectName();
}

void FileSourceGui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

qint64 FileSourceGui::getCenterFrequency() const
{
	return m_centerFrequency;
}

void FileSourceGui::setCenterFrequency(qint64 centerFrequency)
{
	m_centerFrequency = centerFrequency;
	displaySettings();
	sendSettings();
}

QByteArray FileSourceGui::serialize() const
{
	return m_settings.serialize();
}

bool FileSourceGui::deserialize(const QByteArray& data)
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

bool FileSourceGui::handleMessage(const Message& message)
{
	if (FileSourceInput::MsgReportFileSourceAcquisition::match(message))
	{
		m_acquisition = ((FileSourceInput::MsgReportFileSourceAcquisition&)message).getAcquisition();
		updateWithAcquisition();
		return true;
	}
	else if (FileSourceInput::MsgReportFileSourceStreamData::match(message))
	{
		m_sampleRate = ((FileSourceInput::MsgReportFileSourceStreamData&)message).getSampleRate();
		m_centerFrequency = ((FileSourceInput::MsgReportFileSourceStreamData&)message).getCenterFrequency();
		m_startingTimeStamp = ((FileSourceInput::MsgReportFileSourceStreamData&)message).getStartingTimeStamp();
		m_recordLength = ((FileSourceInput::MsgReportFileSourceStreamData&)message).getRecordLength();
		updateWithStreamData();
		return true;
	}
	else if (FileSourceInput::MsgReportFileSourceStreamTiming::match(message))
	{
		m_samplesCount = ((FileSourceInput::MsgReportFileSourceStreamTiming&)message).getSamplesCount();
		updateWithStreamTime();
		return true;
	}
	else
	{
		return false;
	}
}

void FileSourceGui::handleSourceMessages()
{
	Message* message;

	while ((message = m_sampleSource->getOutputMessageQueueToGUI()->pop()) != 0)
	{
		qDebug("FileSourceGui::handleSourceMessages: message: %s", message->getIdentifier());

		if (handleMessage(*message))
		{
			delete message;
		}
	}
}

void FileSourceGui::displaySettings()
{
}

void FileSourceGui::sendSettings()
{
}

void FileSourceGui::updateHardware()
{
}

void FileSourceGui::on_playLoop_toggled(bool checked)
{
	// TODO: do something about it!
}

void FileSourceGui::on_play_toggled(bool checked)
{
	FileSourceInput::MsgConfigureFileSourceWork* message = FileSourceInput::MsgConfigureFileSourceWork::create(checked);
	m_sampleSource->getInputMessageQueue()->push(message);
}

void FileSourceGui::on_showFileDialog_clicked(bool checked)
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

void FileSourceGui::configureFileName()
{
	qDebug() << "FileSourceGui::configureFileName: " << m_fileName.toStdString().c_str();
	FileSourceInput::MsgConfigureFileSourceName* message = FileSourceInput::MsgConfigureFileSourceName::create(m_fileName);
	m_sampleSource->getInputMessageQueue()->push(message);
}

void FileSourceGui::updateWithAcquisition()
{
	ui->play->setEnabled(m_acquisition);
	ui->play->setChecked(m_acquisition);
	ui->showFileDialog->setEnabled(!m_acquisition);
}

void FileSourceGui::updateWithStreamData()
{
	ui->centerFrequency->setValue(m_centerFrequency/1000);
	QString s = QString::number(m_sampleRate/1000.0, 'f', 0);
	ui->sampleRateText->setText(tr("%1k").arg(s));
	ui->play->setEnabled(m_acquisition);
	QTime recordLength(0, 0, 0, 0);
	recordLength = recordLength.addSecs(m_recordLength);
	QString s_time = recordLength.toString("hh:mm:ss");
	ui->recordLengthText->setText(s_time);
	updateWithStreamTime(); // TODO: remove when time data is implemented
}

void FileSourceGui::updateWithStreamTime()
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
	QString s_timems = t.toString("hh:mm:ss.zzz");
	QString s_time = t.toString("hh:mm:ss");
	ui->relTimeText->setText(s_timems);

	quint64 startingTimeStampMsec = m_startingTimeStamp * 1000;
	QDateTime dt = QDateTime::fromMSecsSinceEpoch(startingTimeStampMsec);
	dt = dt.addSecs(t_sec);
	dt = dt.addMSecs(t_msec);
	QString s_date = dt.toString("yyyy-MM-dd hh:mm:ss.zzz");
	ui->absTimeText->setText(s_date);
	float posRatio = (float) t_sec / (float) m_recordLength;
	ui->navTimeSlider->setValue((int) (posRatio * 100.0));
	ui->navTimeText->setText(s_time);
}

void FileSourceGui::tick()
{
	if ((++m_tickCount & 0xf) == 0) {
		FileSourceInput::MsgConfigureFileSourceStreamTiming* message = FileSourceInput::MsgConfigureFileSourceStreamTiming::create();
		m_sampleSource->getInputMessageQueue()->push(message);
	}
}
