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
#include <libairspy/airspy.h>

#include "airspygui.h"
#include "ui_airspygui.h"
#include "plugin/pluginapi.h"
#include "gui/colormapper.h"
#include "dsp/dspengine.h"

AirspyGui::AirspyGui(PluginAPI* pluginAPI, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::AirspyGui),
	m_pluginAPI(pluginAPI),
	m_settings(),
	m_sampleSource(NULL)
{
	ui->setupUi(this);
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));
	ui->centerFrequency->setValueRange(7, 24000U, 1900000U);
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	displaySettings();

	m_sampleSource = new AirspyInput();
	m_rates = ((AirspyInput*) m_sampleSource)->getSampleRates();
	displaySampleRates();
	connect(m_sampleSource->getOutputMessageQueueToGUI(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
	DSPEngine::instance()->setSource(m_sampleSource);
}

AirspyGui::~AirspyGui()
{
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

void AirspyGui::displaySettings()
{
	ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);

	ui->LOppm->setValue(m_settings.m_LOppmTenths);
	ui->LOppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));

	ui->sampleRate->setCurrentIndex(m_settings.m_devSampleRateIndex);

	ui->biasT->setChecked(m_settings.m_biasT);

	ui->decimText->setText(tr("%1").arg(1<<m_settings.m_log2Decim));
	ui->decim->setValue(m_settings.m_log2Decim);

	ui->fcPos->setCurrentIndex((int) m_settings.m_fcPos);

	ui->lnaGainText->setText(tr("%1dB").arg(m_settings.m_lnaGain));
	ui->lna->setValue(m_settings.m_lnaGain);

	ui->mixText->setText(tr("%1dB").arg(m_settings.m_mixerGain));
	ui->mix->setValue(m_settings.m_mixerGain);

	ui->vgaText->setText(tr("%1dB").arg(m_settings.m_vgaGain));
	ui->vga->setValue(m_settings.m_vgaGain);
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
			ui->sampleRate->addItem(QString("%1M").arg(QString::number(m_rates[i]/1000000.0, 'f', 3)));
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

void AirspyGui::on_decim_valueChanged(int value)
{
	if ((value <0) || (value > 5))
		return;
	ui->decimText->setText(tr("%1").arg(1<<value));
	m_settings.m_log2Decim = value;
	sendSettings();
}

void AirspyGui::on_fcPos_currentIndexChanged(int index)
{
	if (index == 0) {
		m_settings.m_fcPos = AirspyInput::FC_POS_INFRA;
		sendSettings();
	} else if (index == 1) {
		m_settings.m_fcPos = AirspyInput::FC_POS_SUPRA;
		sendSettings();
	} else if (index == 2) {
		m_settings.m_fcPos = AirspyInput::FC_POS_CENTER;
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
	m_settings.m_lnaGain = value;
	sendSettings();
}

void AirspyGui::on_vga_valueChanged(int value)
{
	if ((value < 0) || (value > 15))
		return;

	ui->vgaText->setText(tr("%1dB").arg(value));
	m_settings.m_lnaGain = value;
	sendSettings();
}

void AirspyGui::updateHardware()
{
	qDebug() << "AirspyGui::updateHardware";
	AirspyInput::MsgConfigureAirspy* message = AirspyInput::MsgConfigureAirspy::create( m_settings);
	m_sampleSource->getInputMessageQueue()->push(message);
	m_updateTimer.stop();
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
