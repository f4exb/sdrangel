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

#include <QDebug>

#include <QTime>
#include <QDateTime>
#include <QString>
#include <QFileDialog>
#include <QMessageBox>

#include "ui_filesinkgui.h"
#include "plugin/pluginapi.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"

#include "mainwindow.h"

#include "device/devicesinkapi.h"
#include "filesinkgui.h"

FileSinkGui::FileSinkGui(DeviceSinkAPI *deviceAPI, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::FileSinkGui),
	m_deviceAPI(deviceAPI),
	m_settings(),
	m_deviceSampleSink(0),
	m_generation(false),
	m_fileName("./test.sdriq"),
	m_startingTimeStamp(0),
	m_samplesCount(0),
	m_tickCount(0),
	m_lastEngineState((DSPDeviceSinkEngine::State)-1)
{
	ui->setupUi(this);
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));
	ui->centerFrequency->setValueRange(7, 0, pow(10,7));
	ui->fileNameText->setText(m_fileName);

	ui->samplerate->clear();
	for (int i = 0; i < FileSinkSampleRates::getNbRates(); i++)
	{
		ui->samplerate->addItem(QString::number(FileSinkSampleRates::getRate(i)));
	}

	connect(&(m_deviceAPI->getMainWindow()->getMasterTimer()), SIGNAL(timeout()), this, SLOT(tick()));
	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);

	displaySettings();

	m_deviceSampleSink = new FileSinkOutput(m_deviceAPI->getMainWindow()->getMasterTimer());
	connect(m_deviceSampleSink->getOutputMessageQueueToGUI(), SIGNAL(messageEnqueued()), this, SLOT(handleSinkMessages()));
	m_deviceAPI->setSink(m_deviceSampleSink);
}

FileSinkGui::~FileSinkGui()
{
	delete ui;
}

void FileSinkGui::destroy()
{
	delete this;
}

void FileSinkGui::setName(const QString& name)
{
	setObjectName(name);
}

QString FileSinkGui::getName() const
{
	return objectName();
}

void FileSinkGui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

qint64 FileSinkGui::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void FileSinkGui::setCenterFrequency(qint64 centerFrequency)
{
    m_settings.m_centerFrequency = centerFrequency;
	displaySettings();
	sendSettings();
}

QByteArray FileSinkGui::serialize() const
{
	return m_settings.serialize();
}

bool FileSinkGui::deserialize(const QByteArray& data)
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

bool FileSinkGui::handleMessage(const Message& message)
{
	if (FileSinkOutput::MsgReportFileSinkGeneration::match(message))
	{
		m_generation = ((FileSinkOutput::MsgReportFileSinkGeneration&)message).getAcquisition();
		updateWithGeneration();
		return true;
	}
	else if (FileSinkOutput::MsgReportFileSinkStreamTiming::match(message))
	{
		m_samplesCount = ((FileSinkOutput::MsgReportFileSinkStreamTiming&)message).getSamplesCount();
		updateWithStreamTime();
		return true;
	}
	else
	{
		return false;
	}
}

void FileSinkGui::handleSinkMessages()
{
	Message* message;

	while ((message = m_deviceSampleSink->getOutputMessageQueueToGUI()->pop()) != 0)
	{
		//qDebug("FileSourceGui::handleSourceMessages: message: %s", message->getIdentifier());

		if (handleMessage(*message))
		{
			delete message;
		}
	}
}

void FileSinkGui::displaySettings()
{
    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
    unsigned int sampleRateIndex = FileSinkSampleRates::getRateIndex(m_settings.m_sampleRate);
    ui->samplerate->setCurrentIndex(sampleRateIndex);
}

void FileSinkGui::sendSettings()
{
    FileSinkOutput::MsgConfigureFileSink* message = FileSinkOutput::MsgConfigureFileSink::create(m_settings);
    m_deviceSampleSink->getInputMessageQueue()->push(message);
}

void FileSinkGui::updateStatus()
{
    int state = m_deviceAPI->state();

    if(m_lastEngineState != state)
    {
        switch(state)
        {
            case DSPDeviceSinkEngine::StNotStarted:
                ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case DSPDeviceSinkEngine::StIdle:
                ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case DSPDeviceSinkEngine::StRunning:
                ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case DSPDeviceSinkEngine::StError:
                ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::information(this, tr("Message"), m_deviceAPI->errorMessage());
                break;
            default:
                break;
        }

        m_lastEngineState = state;
    }
}

void FileSinkGui::on_centerFrequency_changed(quint64 value)
{
    m_settings.m_centerFrequency = value * 1000;
    sendSettings();
}

void FileSinkGui::on_sampleRate_currentIndexChanged(int index)
{
    int newrate = FileSinkSampleRates::getRate(index);
    m_settings.m_sampleRate = newrate * 1000;
    sendSettings();
}

void FileSinkGui::on_startStop_toggled(bool checked)
{
    if (checked)
    {
        if (m_deviceAPI->initGeneration())
        {
            m_deviceAPI->startGeneration();
            DSPEngine::instance()->startAudio();
        }
    }
    else
    {
        m_deviceAPI->stopGeneration();
        DSPEngine::instance()->stopAudio();
    }
}

void FileSinkGui::on_showFileDialog_clicked(bool checked)
{
    QString fileName = QFileDialog::getSaveFileName(this,
        tr("Save I/Q record file"), ".", tr("SDR I/Q Files (*.sdriq)"));

	if (fileName != "")
	{
		m_fileName = fileName;
		ui->fileNameText->setText(m_fileName);
		configureFileName();
	}
}

void FileSinkGui::configureFileName()
{
	qDebug() << "FileSinkGui::configureFileName: " << m_fileName.toStdString().c_str();
	FileSinkOutput::MsgConfigureFileSinkName* message = FileSinkOutput::MsgConfigureFileSinkName::create(m_fileName);
	m_deviceSampleSink->getInputMessageQueue()->push(message);
}

void FileSinkGui::updateWithGeneration()
{
	ui->showFileDialog->setEnabled(!m_generation);
}

void FileSinkGui::updateWithStreamTime()
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
	QString s_timems = t.toString("hh:mm:ss.zzz");
	ui->relTimeText->setText(s_timems);
}

void FileSinkGui::tick()
{
	if ((++m_tickCount & 0xf) == 0) {
		FileSinkOutput::MsgConfigureFileSinkStreamTiming* message = FileSinkOutput::MsgConfigureFileSinkStreamTiming::create();
		m_deviceSampleSink->getInputMessageQueue()->push(message);
	}
}

unsigned int FileSinkSampleRates::m_rates[] = {48};
unsigned int FileSinkSampleRates::m_nb_rates = 1;

unsigned int FileSinkSampleRates::getRate(unsigned int rate_index)
{
	if (rate_index < m_nb_rates)
	{
		return m_rates[rate_index];
	}
	else
	{
		return m_rates[0];
	}
}

unsigned int FileSinkSampleRates::getRateIndex(unsigned int rate)
{
	for (unsigned int i=0; i < m_nb_rates; i++)
	{
		if (rate/1000 == m_rates[i])
		{
			return i;
		}
	}

	return 0;
}

unsigned int FileSinkSampleRates::getNbRates()
{
	return FileSinkSampleRates::m_nb_rates;
}

