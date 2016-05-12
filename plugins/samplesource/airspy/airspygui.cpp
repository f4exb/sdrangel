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

#include <libairspy/airspy.h>

#include "airspygui.h"
#include "ui_airspygui.h"
#include "plugin/pluginapi.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/filesink.h"

AirspyGui::AirspyGui(PluginAPI* pluginAPI, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::AirspyGui),
	m_pluginAPI(pluginAPI),
	m_settings(),
	m_sampleSource(0),
	m_lastEngineState((DSPDeviceEngine::State)-1)
{
	ui->setupUi(this);
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));
	ui->centerFrequency->setValueRange(7, 24000U, 1900000U);

	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);

	displaySettings();

	m_sampleSource = new AirspyInput();
	m_rates = ((AirspyInput*) m_sampleSource)->getSampleRates();
	displaySampleRates();
	connect(m_sampleSource->getOutputMessageQueueToGUI(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
	DSPEngine::instance()->setSource(m_sampleSource);

    char recFileNameCStr[30];
    sprintf(recFileNameCStr, "test_%d.sdriq", m_pluginAPI->getDeviceUID());
    m_fileSink = new FileSink(std::string(recFileNameCStr));
    m_pluginAPI->addSink(m_fileSink);

    connect(m_pluginAPI->getDeviceOutputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleDSPMessages()), Qt::QueuedConnection);
}

AirspyGui::~AirspyGui()
{
    m_pluginAPI->removeSink(m_fileSink);
    delete m_fileSink;
	delete m_sampleSource; // Valgrind memcheck
	delete ui;
}

void AirspyGui::destroy()
{
	delete this;
}

void AirspyGui::setName(const QString& name)
{
	setObjectName(name);
}

QString AirspyGui::getName() const
{
	return objectName();
}

void AirspyGui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

qint64 AirspyGui::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void AirspyGui::setCenterFrequency(qint64 centerFrequency)
{
	m_settings.m_centerFrequency = centerFrequency;
	displaySettings();
	sendSettings();
}

QByteArray AirspyGui::serialize() const
{
	return m_settings.serialize();
}

bool AirspyGui::deserialize(const QByteArray& data)
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

bool AirspyGui::handleMessage(const Message& message)
{
	if (AirspyInput::MsgReportAirspy::match(message))
	{
		qDebug() << "AirspyGui::handleMessage: MsgReportAirspy";
		m_rates = ((AirspyInput::MsgReportAirspy&) message).getSampleRates();
		displaySampleRates();
		return true;
	}
	else
	{
		return false;
	}
}

void AirspyGui::handleDSPMessages()
{
    Message* message;

    while ((message = m_pluginAPI->getDeviceOutputMessageQueue()->pop()) != 0)
    {
        qDebug("AirspyGui::handleDSPMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("AirspyGui::handleDSPMessages: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();
            m_fileSink->handleMessage(*notif); // forward to file sink

            delete message;
        }
    }
}

void AirspyGui::handleSourceMessages()
{
	Message* message;

	while ((message = m_sampleSource->getOutputMessageQueueToGUI()->pop()) != 0)
	{
		qDebug("AirspyGui::HandleSourceMessages: message: %s", message->getIdentifier());

		if (handleMessage(*message))
		{
			delete message;
		}
	}
}

void AirspyGui::updateSampleRateAndFrequency()
{
    m_pluginAPI->getSpectrum()->setSampleRate(m_sampleRate);
    m_pluginAPI->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_sampleRate / 1000));
}

void AirspyGui::displaySettings()
{
	ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);

	ui->LOppm->setValue(m_settings.m_LOppmTenths);
	ui->LOppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));

	ui->sampleRate->setCurrentIndex(m_settings.m_devSampleRateIndex);

	ui->biasT->setChecked(m_settings.m_biasT);
	ui->dcOffset->setChecked(m_settings.m_dcBlock);
	ui->iqImbalance->setChecked(m_settings.m_iqCorrection);

	ui->decim->setCurrentIndex(m_settings.m_log2Decim);

	ui->fcPos->setCurrentIndex((int) m_settings.m_fcPos);

	ui->lnaGainText->setText(tr("%1dB").arg(m_settings.m_lnaGain));
	ui->lna->setValue(m_settings.m_lnaGain);

	ui->mixText->setText(tr("%1dB").arg(m_settings.m_mixerGain));
	ui->mix->setValue(m_settings.m_mixerGain);

	ui->vgaText->setText(tr("%1dB").arg(m_settings.m_vgaGain));
	ui->vga->setValue(m_settings.m_vgaGain);

	ui->lnaAGC->setChecked(m_settings.m_lnaAGC);
	ui->mixAGC->setChecked(m_settings.m_mixerAGC);
}

void AirspyGui::displaySampleRates()
{
	int savedIndex = m_settings.m_devSampleRateIndex;
	ui->sampleRate->blockSignals(true);

	if (m_rates.size() > 0)
	{
		ui->sampleRate->clear();

		for (int i = 0; i < m_rates.size(); i++)
		{
			ui->sampleRate->addItem(QString("%1").arg(QString::number(m_rates[i]/1000000.0, 'f', 1)));
		}
	}

	ui->sampleRate->blockSignals(false);

	if (savedIndex < m_rates.size())
	{
		ui->sampleRate->setCurrentIndex(savedIndex);
	}
	else
	{
		ui->sampleRate->setCurrentIndex((int) m_rates.size()-1);
	}
}

void AirspyGui::sendSettings()
{
	if(!m_updateTimer.isActive())
		m_updateTimer.start(100);
}

void AirspyGui::on_centerFrequency_changed(quint64 value)
{
	m_settings.m_centerFrequency = value * 1000;
	sendSettings();
}

void AirspyGui::on_LOppm_valueChanged(int value)
{
	m_settings.m_LOppmTenths = value;
	ui->LOppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));
	sendSettings();
}

void AirspyGui::on_dcOffset_toggled(bool checked)
{
	m_settings.m_dcBlock = checked;
	sendSettings();
}

void AirspyGui::on_iqImbalance_toggled(bool checked)
{
	m_settings.m_iqCorrection = checked;
	sendSettings();
}

void AirspyGui::on_sampleRate_currentIndexChanged(int index)
{
	m_settings.m_devSampleRateIndex = index;
	sendSettings();
}

void AirspyGui::on_biasT_stateChanged(int state)
{
	m_settings.m_biasT = (state == Qt::Checked);
	sendSettings();
}

void AirspyGui::on_lnaAGC_stateChanged(int state)
{
	m_settings.m_lnaAGC = (state == Qt::Checked);
	sendSettings();
}

void AirspyGui::on_mixAGC_stateChanged(int state)
{
	m_settings.m_mixerAGC = (state == Qt::Checked);
	sendSettings();
}

void AirspyGui::on_decim_currentIndexChanged(int index)
{
	if ((index <0) || (index > 6))
		return;
	m_settings.m_log2Decim = index;
	sendSettings();
}

void AirspyGui::on_fcPos_currentIndexChanged(int index)
{
	if (index == 0) {
		m_settings.m_fcPos = AirspySettings::FC_POS_INFRA;
		sendSettings();
	} else if (index == 1) {
		m_settings.m_fcPos = AirspySettings::FC_POS_SUPRA;
		sendSettings();
	} else if (index == 2) {
		m_settings.m_fcPos = AirspySettings::FC_POS_CENTER;
		sendSettings();
	}
}

void AirspyGui::on_lna_valueChanged(int value)
{
	if ((value < 0) || (value > 14))
		return;

	ui->lnaGainText->setText(tr("%1dB").arg(value));
	m_settings.m_lnaGain = value;
	sendSettings();
}

void AirspyGui::on_mix_valueChanged(int value)
{
	if ((value < 0) || (value > 15))
		return;

	ui->mixText->setText(tr("%1dB").arg(value));
	m_settings.m_mixerGain = value;
	sendSettings();
}

void AirspyGui::on_vga_valueChanged(int value)
{
	if ((value < 0) || (value > 15))
		return;

	ui->vgaText->setText(tr("%1dB").arg(value));
	m_settings.m_vgaGain = value;
	sendSettings();
}

void AirspyGui::on_startStop_toggled(bool checked)
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

void AirspyGui::on_record_toggled(bool checked)
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

void AirspyGui::updateHardware()
{
	qDebug() << "AirspyGui::updateHardware";
	AirspyInput::MsgConfigureAirspy* message = AirspyInput::MsgConfigureAirspy::create( m_settings);
	m_sampleSource->getInputMessageQueue()->push(message);
	m_updateTimer.stop();
}

void AirspyGui::updateStatus()
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

uint32_t AirspyGui::getDevSampleRate(unsigned int rate_index)
{
	if (rate_index < m_rates.size())
	{
		return m_rates[rate_index];
	}
	else
	{
		return m_rates[0];
	}
}

int AirspyGui::getDevSampleRateIndex(uint32_t sampeRate)
{
	for (unsigned int i=0; i < m_rates.size(); i++)
	{
		if (sampeRate == m_rates[i])
		{
			return i;
		}
	}

	return -1;
}
