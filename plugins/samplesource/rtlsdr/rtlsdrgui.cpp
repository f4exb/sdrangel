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
#include <QMessageBox>

#include "rtlsdrgui.h"
#include "ui_rtlsdrgui.h"
#include "plugin/pluginapi.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/filesink.h"

RTLSDRGui::RTLSDRGui(PluginAPI* pluginAPI, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::RTLSDRGui),
	m_pluginAPI(pluginAPI),
	m_settings(),
	m_sampleSource(0),
	m_lastEngineState((DSPDeviceEngine::State)-1)
{
	ui->setupUi(this);
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));
	ui->centerFrequency->setValueRange(7, 24000U, 1900000U);

	ui->sampleRate->clear();

	for (int i = 0; i < RTLSDRSampleRates::getNbRates(); i++)
	{
		ui->sampleRate->addItem(QString::number(RTLSDRSampleRates::getRate(i)));
	}

	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);

	displaySettings();

	m_sampleSource = new RTLSDRInput(m_pluginAPI);
	connect(m_sampleSource->getOutputMessageQueueToGUI(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
	m_pluginAPI->setSource(m_sampleSource);

    char recFileNameCStr[30];
    sprintf(recFileNameCStr, "test_%d.sdriq", m_pluginAPI->getDeviceUID());
    m_fileSink = new FileSink(std::string(recFileNameCStr));
    m_pluginAPI->addSink(m_fileSink);

    connect(m_pluginAPI->getDeviceOutputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleDSPMessages()), Qt::QueuedConnection);
}

RTLSDRGui::~RTLSDRGui()
{
    m_pluginAPI->removeSink(m_fileSink);
    delete m_fileSink;
	delete ui;
}

void RTLSDRGui::destroy()
{
	delete this;
}

void RTLSDRGui::setName(const QString& name)
{
	setObjectName(name);
}

QString RTLSDRGui::getName() const
{
	return objectName();
}

void RTLSDRGui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

qint64 RTLSDRGui::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void RTLSDRGui::setCenterFrequency(qint64 centerFrequency)
{
	m_settings.m_centerFrequency = centerFrequency;
	displaySettings();
	sendSettings();
}

void RTLSDRGui::on_dcOffset_toggled(bool checked)
{
	m_settings.m_dcBlock = checked;
	sendSettings();
}

void RTLSDRGui::on_iqImbalance_toggled(bool checked)
{
	m_settings.m_iqImbalance = checked;
	sendSettings();
}

QByteArray RTLSDRGui::serialize() const
{
	return m_settings.serialize();
}

bool RTLSDRGui::deserialize(const QByteArray& data)
{
	if (m_settings.deserialize(data))
	{
		displaySettings();
		sendSettings();
		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}

bool RTLSDRGui::handleMessage(const Message& message)
{
	if (RTLSDRInput::MsgReportRTLSDR::match(message))
	{
		qDebug() << "RTLSDRGui::handleMessage: MsgReportRTLSDR";
		m_gains = ((RTLSDRInput::MsgReportRTLSDR&) message).getGains();
		displaySettings();
		return true;
	}
	else
	{
		return false;
	}
}

void RTLSDRGui::handleDSPMessages()
{
    Message* message;

    while ((message = m_pluginAPI->getDeviceOutputMessageQueue()->pop()) != 0)
    {
        qDebug("RTLSDRGui::handleDSPMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("RTLSDRGui::handleDSPMessages: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();
            m_fileSink->handleMessage(*notif); // forward to file sink

            delete message;
        }
    }
}

void RTLSDRGui::handleSourceMessages()
{
	Message* message;

	while ((message = m_sampleSource->getOutputMessageQueueToGUI()->pop()) != 0)
	{
		qDebug("RTLSDRGui::HandleSourceMessages: message: %s", message->getIdentifier());

		if (handleMessage(*message))
		{
			delete message;
		}
	}
}

void RTLSDRGui::updateSampleRateAndFrequency()
{
    m_pluginAPI->getSpectrum()->setSampleRate(m_sampleRate);
    m_pluginAPI->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_sampleRate / 1000));
}

void RTLSDRGui::displaySettings()
{
	ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
	ui->dcOffset->setChecked(m_settings.m_dcBlock);
	ui->iqImbalance->setChecked(m_settings.m_iqImbalance);
	unsigned int sampleRateIndex = RTLSDRSampleRates::getRateIndex(m_settings.m_devSampleRate);
	ui->sampleRate->setCurrentIndex(sampleRateIndex);
	ui->ppm->setValue(m_settings.m_loPpmCorrection);
	ui->ppmText->setText(tr("%1").arg(m_settings.m_loPpmCorrection));
	ui->decim->setCurrentIndex(m_settings.m_log2Decim);
	ui->fcPos->setCurrentIndex((int) m_settings.m_fcPos);

	if (m_gains.size() > 0)
	{
		int dist = abs(m_settings.m_gain - m_gains[0]);
		int pos = 0;

		for (uint i = 1; i < m_gains.size(); i++)
		{
			if (abs(m_settings.m_gain - m_gains[i]) < dist)
			{
				dist = abs(m_settings.m_gain - m_gains[i]);
				pos = i;
			}
		}

		ui->gainText->setText(tr("%1.%2").arg(m_gains[pos] / 10).arg(abs(m_gains[pos] % 10)));
		ui->gain->setMaximum(m_gains.size() - 1);
		ui->gain->setEnabled(true);
		ui->gain->setValue(pos);
	}
	else
	{
		ui->gain->setMaximum(0);
		ui->gain->setEnabled(false);
		ui->gain->setValue(0);
	}
}

void RTLSDRGui::sendSettings()
{
	if(!m_updateTimer.isActive())
	{
		m_updateTimer.start(100);
	}
}

void RTLSDRGui::on_centerFrequency_changed(quint64 value)
{
	m_settings.m_centerFrequency = value * 1000;
	sendSettings();
}

void RTLSDRGui::on_decim_currentIndexChanged(int index)
{
	if ((index <0) || (index > 4))
	{
		return;
	}

	m_settings.m_log2Decim = index;

	sendSettings();
}

void RTLSDRGui::on_fcPos_currentIndexChanged(int index)
{
	if (index == 0) {
		m_settings.m_fcPos = RTLSDRSettings::FC_POS_INFRA;
		sendSettings();
	} else if (index == 1) {
		m_settings.m_fcPos = RTLSDRSettings::FC_POS_SUPRA;
		sendSettings();
	} else if (index == 2) {
		m_settings.m_fcPos = RTLSDRSettings::FC_POS_CENTER;
		sendSettings();
	}
}

void RTLSDRGui::on_ppm_valueChanged(int value)
{
	if ((value > 99) || (value < -99))
	{
		return;
	}

	ui->ppmText->setText(tr("%1").arg(value));
	m_settings.m_loPpmCorrection = value;

	sendSettings();
}

void RTLSDRGui::on_gain_valueChanged(int value)
{
	if (value > (int)m_gains.size())
	{
		return;
	}

	int gain = m_gains[value];
	ui->gainText->setText(tr("%1.%2").arg(gain / 10).arg(abs(gain % 10)));
	m_settings.m_gain = gain;

	sendSettings();
}

void RTLSDRGui::on_sampleRate_currentIndexChanged(int index)
{
	int newrate = RTLSDRSampleRates::getRate(index);
	m_settings.m_devSampleRate = newrate * 1000;

	sendSettings();
}

void RTLSDRGui::on_startStop_toggled(bool checked)
{
    if (checked)
    {
        if (m_pluginAPI->initAcquisition())
        {
            m_pluginAPI->startAcquisition();
            DSPEngine::instance()->startAudio();
        }
    }
    else
    {
        m_pluginAPI->stopAcquistion();
        DSPEngine::instance()->stopAudio();
    }
}

void RTLSDRGui::on_record_toggled(bool checked)
{
    if (checked)
    {
        ui->record->setStyleSheet("QToolButton { background-color : red; }");
        m_fileSink->startRecording();
    }
    else
    {
        ui->record->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        m_fileSink->stopRecording();
    }
}

void RTLSDRGui::updateHardware()
{
	RTLSDRInput::MsgConfigureRTLSDR* message = RTLSDRInput::MsgConfigureRTLSDR::create(m_settings);
	m_sampleSource->getInputMessageQueue()->push(message);
	m_updateTimer.stop();
}

void RTLSDRGui::updateStatus()
{
    int state = m_pluginAPI->state();

    if(m_lastEngineState != state)
    {
        switch(state)
        {
            case DSPDeviceEngine::StNotStarted:
                ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case DSPDeviceEngine::StIdle:
                ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case DSPDeviceEngine::StRunning:
                ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case DSPDeviceEngine::StError:
                ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::information(this, tr("Message"), m_pluginAPI->errorMessage());
                break;
            default:
                break;
        }

        m_lastEngineState = state;
    }
}

void RTLSDRGui::on_checkBox_stateChanged(int state)
{
	if (state == Qt::Checked)
	{
		// Direct Modes: 0: off, 1: I, 2: Q, 3: NoMod.
		((RTLSDRInput*)m_sampleSource)->set_ds_mode(3);
		ui->gain->setEnabled(false);
		ui->centerFrequency->setValueRange(7, 1000U, 275000U);
		ui->centerFrequency->setValue(7000);
		m_settings.m_centerFrequency = 7000 * 1000;
	}
	else
	{
		((RTLSDRInput*)m_sampleSource)->set_ds_mode(0);
		ui->gain->setEnabled(true);
		ui->centerFrequency->setValueRange(7, 28500U, 1700000U);
		ui->centerFrequency->setValue(434000);
		ui->gain->setValue(0);
		m_settings.m_centerFrequency = 435000 * 1000;
	}

	sendSettings();
}

unsigned int RTLSDRSampleRates::m_rates[] = {250, 256, 1000, 1152, 1200, 1536, 1600, 2000, 2304, 2400};
unsigned int RTLSDRSampleRates::m_nb_rates = 10;

unsigned int RTLSDRSampleRates::getRate(unsigned int rate_index)
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

unsigned int RTLSDRSampleRates::getRateIndex(unsigned int rate)
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

unsigned int RTLSDRSampleRates::getNbRates()
{
	return RTLSDRSampleRates::m_nb_rates;
}
