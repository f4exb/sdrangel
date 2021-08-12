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

#include <QDebug>

#include <QTime>
#include <QDateTime>
#include <QString>
#include <QFileDialog>
#include <QMessageBox>

#include "ui_fileoutputgui.h"
#include "plugin/pluginapi.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"

#include "mainwindow.h"

#include "device/deviceapi.h"
#include "device/deviceuiset.h"
#include "fileoutputgui.h"

FileOutputGui::FileOutputGui(DeviceUISet *deviceUISet, QWidget *parent) : DeviceGUI(parent),
                                                                          ui(new Ui::FileOutputGui),
                                                                          m_deviceUISet(deviceUISet),
                                                                          m_doApplySettings(true),
                                                                          m_forceSettings(true),
                                                                          m_settings(),
                                                                          m_deviceSampleSink(0),
                                                                          m_sampleRate(0),
                                                                          m_generation(false),
                                                                          m_startingDateTime(QDateTime::currentDateTime()),
                                                                          m_samplesCount(0),
                                                                          m_tickCount(0),
                                                                          m_lastEngineState(DeviceAPI::StNotStarted)
{
	ui->setupUi(this);

	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
	ui->centerFrequency->setValueRange(7, 0, pow(10,7));

    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(8, 32000U, 90000000U);

	ui->fileNameText->setText(m_settings.m_fileName);

	connect(&(m_deviceUISet->m_deviceAPI->getMasterTimer()), SIGNAL(timeout()), this, SLOT(tick()));
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);

	displaySettings();

    m_deviceSampleSink = (FileOutput*) m_deviceUISet->m_deviceAPI->getSampleSink();
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
}

FileOutputGui::~FileOutputGui()
{
	delete ui;
}

void FileOutputGui::destroy()
{
	delete this;
}

void FileOutputGui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

QByteArray FileOutputGui::serialize() const
{
	return m_settings.serialize();
}

bool FileOutputGui::deserialize(const QByteArray& data)
{
	if(m_settings.deserialize(data)) {
		displaySettings();
		m_forceSettings = true;
		sendSettings();
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}

bool FileOutputGui::handleMessage(const Message& message)
{
    if (FileOutput::MsgConfigureFileOutput::match(message))
    {
        qDebug("FileOutputGui::handleMessage: message: MsgConfigureFileOutput");
        const FileOutput::MsgConfigureFileOutput& cfg = (FileOutput::MsgConfigureFileOutput&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (FileOutput::MsgReportFileOutputGeneration::match(message))
	{
		m_generation = ((FileOutput::MsgReportFileOutputGeneration&)message).getAcquisition();
        qDebug("FileOutputGui::handleMessage: message: MsgReportFileOutputGeneration: %s", m_generation ? "start" : "stop");
		updateWithGeneration();
		return true;
	}
	else if (FileOutput::MsgReportFileOutputStreamTiming::match(message))
	{
		m_samplesCount = ((FileOutput::MsgReportFileOutputStreamTiming&)message).getSamplesCount();
		updateWithStreamTime();
		return true;
	}
	else if (FileOutput::MsgStartStop::match(message))
	{
	    FileOutput::MsgStartStop& notif = (FileOutput::MsgStartStop&) message;
        qDebug("FileOutputGui::handleMessage: message: MsgStartStop: %s", notif.getStartStop() ? "start" : "stop");
	    blockApplySettings(true);
	    ui->startStop->setChecked(notif.getStartStop());
	    blockApplySettings(false);
	    return true;
	}
	else
	{
		return false;
	}
}

void FileOutputGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            qDebug("FileOutputGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            updateSampleRateAndFrequency();

            delete message;
        }
        else
        {
            if (handleMessage(*message))
            {
                delete message;
            }
        }
    }
}

void FileOutputGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)(m_sampleRate*(1<<m_settings.m_log2Interp)) / 1000));
}

void FileOutputGui::displaySettings()
{
    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
    ui->sampleRate->setValue(m_settings.m_sampleRate);
    ui->fileNameText->setText(m_settings.m_fileName);
}

void FileOutputGui::sendSettings()
{
    if(!m_updateTimer.isActive())
        m_updateTimer.start(100);
}


void FileOutputGui::updateHardware()
{
    qDebug() << "FileOutputGui::updateHardware";
    FileOutput::MsgConfigureFileOutput* message = FileOutput::MsgConfigureFileOutput::create(m_settings, m_forceSettings);
    m_deviceSampleSink->getInputMessageQueue()->push(message);
    m_forceSettings = false;
    m_updateTimer.stop();
}

void FileOutputGui::updateStatus()
{
    int state = m_deviceUISet->m_deviceAPI->state();

    if(m_lastEngineState != state)
    {
        switch(state)
        {
            case DeviceAPI::StNotStarted:
                ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case DeviceAPI::StIdle:
                ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case DeviceAPI::StRunning:
                ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case DeviceAPI::StError:
                ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::information(this, tr("Message"), m_deviceUISet->m_deviceAPI->errorMessage());
                break;
            default:
                break;
        }

        m_lastEngineState = state;
    }
}

void FileOutputGui::on_centerFrequency_changed(quint64 value)
{
    m_settings.m_centerFrequency = value * 1000;
    sendSettings();
}

void FileOutputGui::on_sampleRate_changed(quint64 value)
{
    m_settings.m_sampleRate = value;
    sendSettings();
}

void FileOutputGui::on_interp_currentIndexChanged(int index)
{
    if (index < 0) {
        return;
    }

    m_settings.m_log2Interp = index;
    updateSampleRateAndFrequency();
    sendSettings();
}

void FileOutputGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        FileOutput::MsgStartStop *message = FileOutput::MsgStartStop::create(checked);
        m_deviceSampleSink->getInputMessageQueue()->push(message);
    }
}

void FileOutputGui::on_showFileDialog_clicked(bool checked)
{
    (void) checked;
    QString fileName = QFileDialog::getSaveFileName(this,
        tr("Save I/Q record file"), ".", tr("SDR I/Q Files (*.sdriq)"), 0, QFileDialog::DontUseNativeDialog);

	if (fileName != "")
	{
		m_settings.m_fileName = fileName;
		ui->fileNameText->setText(m_settings.m_fileName);
		configureFileName();
	}
}

void FileOutputGui::configureFileName()
{
	qDebug() << "FileOutputGui::configureFileName: " << m_settings.m_fileName.toStdString().c_str();
	FileOutput::MsgConfigureFileOutputName* message = FileOutput::MsgConfigureFileOutputName::create(m_settings.m_fileName);
	m_deviceSampleSink->getInputMessageQueue()->push(message);
}

void FileOutputGui::updateWithGeneration()
{
	ui->showFileDialog->setEnabled(!m_generation);
}

void FileOutputGui::updateWithStreamTime()
{
	int t_sec = 0;
	int t_msec = 0;

	if (m_settings.m_sampleRate > 0){
		t_msec = ((m_samplesCount * 1000) / m_settings.m_sampleRate) % 1000;
		t_sec = m_samplesCount / m_settings.m_sampleRate;
	}

	QTime t(0, 0, 0, 0);
	t = t.addSecs(t_sec);
	t = t.addMSecs(t_msec);
	QString s_timems = t.toString("HH:mm:ss.zzz");
	ui->relTimeText->setText(s_timems);
}

void FileOutputGui::tick()
{
	if ((++m_tickCount & 0xf) == 0)
	{
		FileOutput::MsgConfigureFileOutputStreamTiming* message = FileOutput::MsgConfigureFileOutputStreamTiming::create();
		m_deviceSampleSink->getInputMessageQueue()->push(message);
	}
}
