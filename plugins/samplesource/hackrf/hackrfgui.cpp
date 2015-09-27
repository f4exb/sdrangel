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
#include <libhackrf/hackrf.h>

#include "plugin/pluginapi.h"
#include "gui/colormapper.h"
#include "dsp/dspengine.h"
#include "hackrfgui.h"
#include "ui_hackrfgui.h"

HackRFGui::HackRFGui(PluginAPI* pluginAPI, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::HackRFGui),
	m_pluginAPI(pluginAPI),
	m_settings(),
	m_sampleSource(NULL)
{
	ui->setupUi(this);
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));
	ui->centerFrequency->setValueRange(7, 0U, 7250000U);
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	displaySettings();

	m_sampleSource = new HackRFInput();

	displaySampleRates();
	displayBandwidths();

	DSPEngine::instance()->setSource(m_sampleSource);
}

HackRFGui::~HackRFGui()
{
	delete m_sampleSource; // Valgrind memcheck
	delete ui;
}

void HackRFGui::destroy()
{
	delete this;
}

void HackRFGui::setName(const QString& name)
{
	setObjectName(name);
}

QString HackRFGui::getName() const
{
	return objectName();
}

void HackRFGui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

qint64 HackRFGui::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

QByteArray HackRFGui::serialize() const
{
	return m_settings.serialize();
}

bool HackRFGui::deserialize(const QByteArray& data)
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

bool HackRFGui::handleMessage(const Message& message)
{
	return false;
}

void HackRFGui::displaySettings()
{
	ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);

	ui->LOppm->setValue(m_settings.m_LOppmTenths);
	ui->LOppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));

	ui->sampleRate->setCurrentIndex(m_settings.m_devSampleRateIndex);

	ui->biasT->setChecked(m_settings.m_biasT);

	ui->decimText->setText(tr("%1").arg(1<<m_settings.m_log2Decim));
	ui->decim->setValue(m_settings.m_log2Decim);

	ui->fcPos->setCurrentIndex((int) m_settings.m_fcPos);

	ui->lnaExt->setChecked(m_settings.m_lnaExt);
	ui->lnaGainText->setText(tr("%1dB").arg(m_settings.m_lnaGain));
	ui->lna->setValue(m_settings.m_lnaGain);

	ui->bbFilter->setCurrentIndex(m_settings.m_bandwidthIndex);

	ui->vgaText->setText(tr("%1dB").arg(m_settings.m_vgaGain));
	ui->vga->setValue(m_settings.m_vgaGain);
}

void HackRFGui::displaySampleRates()
{
	int savedIndex = m_settings.m_devSampleRateIndex;
	ui->sampleRate->blockSignals(true);
	ui->sampleRate->clear();

	for (int i = 0; i < HackRFSampleRates::m_nb_rates; i++)
	{
		ui->sampleRate->addItem(QString("%1M").arg(QString::number(HackRFSampleRates::m_rates_k[i]/1000.0, 'f', 1)));
	}

	ui->sampleRate->blockSignals(false);

	if (savedIndex < HackRFSampleRates::m_nb_rates)
	{
		ui->sampleRate->setCurrentIndex(savedIndex);
	}
	else
	{
		ui->sampleRate->setCurrentIndex((int) HackRFSampleRates::m_nb_rates-1);
	}
}

void HackRFGui::displayBandwidths()
{
	int savedIndex = m_settings.m_bandwidthIndex;
	ui->bbFilter->blockSignals(true);
	ui->bbFilter->clear();

	for (int i = 0; i < HackRFBandwidths::m_nb_bw; i++)
	{
		ui->bbFilter->addItem(QString("%1M").arg(QString::number(HackRFBandwidths::m_bw_k[i]/1000.0, 'f', 2)));
	}

	ui->bbFilter->blockSignals(false);

	if (savedIndex < HackRFBandwidths::m_nb_bw)
	{
		ui->bbFilter->setCurrentIndex(savedIndex);
	}
	else
	{
		ui->bbFilter->setCurrentIndex((int) HackRFBandwidths::m_nb_bw-1);
	}
}

void HackRFGui::sendSettings()
{
	if(!m_updateTimer.isActive())
		m_updateTimer.start(100);
}

void HackRFGui::on_centerFrequency_changed(quint64 value)
{
	m_settings.m_centerFrequency = value * 1000;
	sendSettings();
}

void HackRFGui::on_LOppm_valueChanged(int value)
{
	m_settings.m_LOppmTenths = value;
	ui->LOppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));
	sendSettings();
}

void HackRFGui::on_sampleRate_currentIndexChanged(int index)
{
	m_settings.m_devSampleRateIndex = index;
	sendSettings();
}

void HackRFGui::on_bbFilter_currentIndexChanged(int index)
{
	m_settings.m_bandwidthIndex = index;
	sendSettings();
}

void HackRFGui::on_biasT_stateChanged(int state)
{
	m_settings.m_biasT = (state == Qt::Checked);
	sendSettings();
}

void HackRFGui::on_lnaExt_stateChanged(int state)
{
	m_settings.m_lnaExt = (state == Qt::Checked);
	sendSettings();
}

void HackRFGui::on_decim_valueChanged(int value)
{
	if ((value <0) || (value > 6))
		return;
	ui->decimText->setText(tr("%1").arg(1<<value));
	m_settings.m_log2Decim = value;
	sendSettings();
}

void HackRFGui::on_fcPos_currentIndexChanged(int index)
{
	if (index == 0) {
		m_settings.m_fcPos = HackRFInput::FC_POS_INFRA;
		sendSettings();
	} else if (index == 1) {
		m_settings.m_fcPos = HackRFInput::FC_POS_SUPRA;
		sendSettings();
	} else if (index == 2) {
		m_settings.m_fcPos = HackRFInput::FC_POS_CENTER;
		sendSettings();
	}
}

void HackRFGui::on_lna_valueChanged(int value)
{
	if ((value < 0) || (value > 40))
		return;

	ui->lnaGainText->setText(tr("%1dB").arg(value));
	m_settings.m_lnaGain = value;
	sendSettings();
}

void HackRFGui::on_vga_valueChanged(int value)
{
	if ((value < 0) || (value > 62))
		return;

	ui->vgaText->setText(tr("%1dB").arg(value));
	m_settings.m_vgaGain = value;
	sendSettings();
}

void HackRFGui::updateHardware()
{
	qDebug() << "HackRFGui::updateHardware";
	HackRFInput::MsgConfigureHackRF* message = HackRFInput::MsgConfigureHackRF::create( m_settings);
	m_sampleSource->getInputMessageQueue()->push(message);
	m_updateTimer.stop();
}

unsigned int HackRFSampleRates::m_rates_k[] = {2400, 3200, 4800, 6400, 9600, 12800, 19200};

unsigned int HackRFSampleRates::getRate(unsigned int rate_index)
{
	if (rate_index < m_nb_rates)
	{
		return m_rates_k[rate_index];
	}
	else
	{
		return m_rates_k[0];
	}
}

unsigned int HackRFSampleRates::getRateIndex(unsigned int rate)
{
	for (unsigned int i=0; i < m_nb_rates; i++)
	{
		if (rate/1000 == m_rates_k[i])
		{
			return i;
		}
	}

	return 0;
}

unsigned int HackRFBandwidths::m_bw_k[] = {1750, 2500, 3500, 5000, 5500, 6000, 7000, 8000, 9000, 10000, 12000, 14000, 15000, 20000, 24000, 28000};

unsigned int HackRFBandwidths::getBandwidth(unsigned int bandwidth_index)
{
	if (bandwidth_index < m_nb_bw)
	{
		return m_bw_k[bandwidth_index];
	}
	else
	{
		return m_bw_k[0];
	}
}

unsigned int HackRFBandwidths::getBandwidthIndex(unsigned int bandwidth)
{
	for (unsigned int i=0; i < m_nb_bw; i++)
	{
		if (bandwidth == m_bw_k[i])
		{
			return i;
		}
	}

	return 0;
}
