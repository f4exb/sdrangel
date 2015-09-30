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
#include <libbladeRF.h>

#include "ui_bladerfgui.h"
#include "plugin/pluginapi.h"
#include "gui/colormapper.h"
#include "dsp/dspengine.h"
#include "bladerfgui.h"

BladerfGui::BladerfGui(PluginAPI* pluginAPI, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::BladerfGui),
	m_pluginAPI(pluginAPI),
	m_settings(),
	m_sampleSource(NULL)
{
	ui->setupUi(this);
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));
	ui->centerFrequency->setValueRange(7, BLADERF_FREQUENCY_MIN_XB200/1000, BLADERF_FREQUENCY_MAX/1000);
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	displaySettings();

	m_sampleSource = new BladerfInput();
	DSPEngine::instance()->setSource(m_sampleSource);
}

BladerfGui::~BladerfGui()
{
	delete m_sampleSource; // Valgrind memcheck
	delete ui;
}

void BladerfGui::destroy()
{
	delete this;
}

void BladerfGui::setName(const QString& name)
{
	setObjectName(name);
}

QString BladerfGui::getName() const
{
	return objectName();
}

void BladerfGui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

qint64 BladerfGui::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void BladerfGui::setCenterFrequency(qint64 centerFrequency)
{
	m_settings.m_centerFrequency = centerFrequency;
	displaySettings();
	sendSettings();
}

QByteArray BladerfGui::serialize() const
{
	return m_settings.serialize();
}

bool BladerfGui::deserialize(const QByteArray& data)
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

bool BladerfGui::handleMessage(const Message& message)
{
	if (BladerfInput::MsgReportBladerf::match(message))
	{
		displaySettings();
		return true;
	}
	else
	{
		return false;
	}
}

void BladerfGui::displaySettings()
{
	ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);

	ui->samplerateText->setText(tr("%1k").arg(m_settings.m_devSampleRate / 1000));
	unsigned int sampleRateIndex = BladerfSampleRates::getRateIndex(m_settings.m_devSampleRate);
	ui->samplerate->setValue(sampleRateIndex);

	ui->bandwidthText->setText(tr("%1k").arg(m_settings.m_bandwidth / 1000));
	unsigned int bandwidthIndex = BladerfBandwidths::getBandwidthIndex(m_settings.m_bandwidth);
	ui->bandwidth->setValue(bandwidthIndex);

	ui->decimText->setText(tr("%1").arg(1<<m_settings.m_log2Decim));
	ui->decim->setValue(m_settings.m_log2Decim);

	ui->fcPos->setCurrentIndex((int) m_settings.m_fcPos);

	ui->lnaGainText->setText(tr("%1dB").arg(m_settings.m_lnaGain*3));
	ui->lna->setValue(m_settings.m_lnaGain);

	ui->vga1Text->setText(tr("%1dB").arg(m_settings.m_vga1));
	ui->vga1->setValue(m_settings.m_vga1);

	ui->vga2Text->setText(tr("%1dB").arg(m_settings.m_vga2));
	ui->vga2->setValue(m_settings.m_vga2);

	ui->xb200->setCurrentIndex(getXb200Index(m_settings.m_xb200, m_settings.m_xb200Path, m_settings.m_xb200Filter));
}

void BladerfGui::sendSettings()
{
	if(!m_updateTimer.isActive())
		m_updateTimer.start(100);
}

void BladerfGui::on_centerFrequency_changed(quint64 value)
{
	m_settings.m_centerFrequency = value * 1000;
	sendSettings();
}

void BladerfGui::on_samplerate_valueChanged(int value)
{
	int newrate = BladerfSampleRates::getRate(value);
	ui->samplerateText->setText(tr("%1k").arg(newrate));
	m_settings.m_devSampleRate = newrate * 1000;
	sendSettings();
}

void BladerfGui::on_bandwidth_valueChanged(int value)
{
	int newbw = BladerfBandwidths::getBandwidth(value);
	ui->bandwidthText->setText(tr("%1k").arg(newbw));
	m_settings.m_bandwidth = newbw * 1000;
	sendSettings();
}

void BladerfGui::on_decim_valueChanged(int value)
{
	if ((value <0) || (value > 5))
		return;
	ui->decimText->setText(tr("%1").arg(1<<value));
	m_settings.m_log2Decim = value;
	sendSettings();
}

void BladerfGui::on_fcPos_currentIndexChanged(int index)
{
	if (index == 0) {
		m_settings.m_fcPos = BladeRFSettings::FC_POS_INFRA;
		sendSettings();
	} else if (index == 1) {
		m_settings.m_fcPos = BladeRFSettings::FC_POS_SUPRA;
		sendSettings();
	} else if (index == 2) {
		m_settings.m_fcPos = BladeRFSettings::FC_POS_CENTER;
		sendSettings();
	}
}

void BladerfGui::on_lna_valueChanged(int value)
{
	qDebug() << "BladerfGui: LNA gain = " << value;

	if ((value < 0) || (value > 2))
		return;

	ui->lnaGainText->setText(tr("%1dB").arg(value*3));
	m_settings.m_lnaGain = value;
	sendSettings();
}

void BladerfGui::on_vga1_valueChanged(int value)
{
	if ((value < BLADERF_RXVGA1_GAIN_MIN) || (value > BLADERF_RXVGA1_GAIN_MAX))
		return;

	ui->vga1Text->setText(tr("%1dB").arg(value));
	m_settings.m_vga1 = value;
	sendSettings();
}

void BladerfGui::on_vga2_valueChanged(int value)
{
	if ((value < BLADERF_RXVGA2_GAIN_MIN) || (value > BLADERF_RXVGA2_GAIN_MAX))
		return;

	ui->vga2Text->setText(tr("%1dB").arg(value));
	m_settings.m_vga2 = value;
	sendSettings();
}

void BladerfGui::on_xb200_currentIndexChanged(int index)
{
	if (index == 1) // bypass
	{
		m_settings.m_xb200 = true;
		m_settings.m_xb200Path = BLADERF_XB200_BYPASS;
	}
	else if (index == 2) // Auto 1dB
	{
		m_settings.m_xb200 = true;
		m_settings.m_xb200Path = BLADERF_XB200_MIX;
		m_settings.m_xb200Filter = BLADERF_XB200_AUTO_1DB;
	}
	else if (index == 3) // Auto 3dB
	{
		m_settings.m_xb200 = true;
		m_settings.m_xb200Path = BLADERF_XB200_MIX;
		m_settings.m_xb200Filter = BLADERF_XB200_AUTO_3DB;
	}
	else if (index == 4) // Custom
	{
		m_settings.m_xb200 = true;
		m_settings.m_xb200Path = BLADERF_XB200_MIX;
		m_settings.m_xb200Filter = BLADERF_XB200_CUSTOM;
	}
	else if (index == 5) // 50 MHz
	{
		m_settings.m_xb200 = true;
		m_settings.m_xb200Path = BLADERF_XB200_MIX;
		m_settings.m_xb200Filter = BLADERF_XB200_50M;
	}
	else if (index == 6) // 144 MHz
	{
		m_settings.m_xb200 = true;
		m_settings.m_xb200Path = BLADERF_XB200_MIX;
		m_settings.m_xb200Filter = BLADERF_XB200_144M;
	}
	else if (index == 7) // 222 MHz
	{
		m_settings.m_xb200 = true;
		m_settings.m_xb200Path = BLADERF_XB200_MIX;
		m_settings.m_xb200Filter = BLADERF_XB200_222M;
	}
	else // no xb200
	{
		m_settings.m_xb200 = false;
	}

	if (m_settings.m_xb200)
	{
		ui->centerFrequency->setValueRange(7, BLADERF_FREQUENCY_MIN_XB200/1000, BLADERF_FREQUENCY_MAX/1000);
	}
	else
	{
		ui->centerFrequency->setValueRange(7, BLADERF_FREQUENCY_MIN/1000, BLADERF_FREQUENCY_MAX/1000);
	}

	sendSettings();
}

void BladerfGui::updateHardware()
{
	qDebug() << "BladerfGui::updateHardware";
	BladerfInput::MsgConfigureBladerf* message = BladerfInput::MsgConfigureBladerf::create( m_settings);
	m_sampleSource->getInputMessageQueue()->push(message);
	m_updateTimer.stop();
}

unsigned int BladerfGui::getXb200Index(bool xb_200, bladerf_xb200_path xb200Path, bladerf_xb200_filter xb200Filter)
{
	if (xb_200)
	{
		if (xb200Path == BLADERF_XB200_BYPASS)
		{
			return 1;
		}
		else
		{
			if (xb200Filter == BLADERF_XB200_AUTO_1DB)
			{
				return 2;
			}
			else if (xb200Filter == BLADERF_XB200_AUTO_3DB)
			{
				return 3;
			}
			else if (xb200Filter == BLADERF_XB200_CUSTOM)
			{
				return 4;
			}
			else if (xb200Filter == BLADERF_XB200_50M)
			{
				return 5;
			}
			else if (xb200Filter == BLADERF_XB200_144M)
			{
				return 6;
			}
			else if (xb200Filter == BLADERF_XB200_222M)
			{
				return 7;
			}
			else
			{
				return 0;
			}
		}
	}
	else
	{
		return 0;
	}
}

unsigned int BladerfSampleRates::m_rates[] = {1536, 2304, 3072, 4608, 6144, 7680, 9216, 10752, 12288, 18432, 24576, 30720, 36864, 39936};
unsigned int BladerfSampleRates::m_nb_rates = 14;

unsigned int BladerfSampleRates::getRate(unsigned int rate_index)
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

unsigned int BladerfSampleRates::getRateIndex(unsigned int rate)
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

unsigned int BladerfBandwidths::m_halfbw[] = {750, 875, 1250, 1375, 1500, 1920, 2500, 2750, 3000, 3500, 4375, 5000, 6000, 7000, 10000, 14000};
unsigned int BladerfBandwidths::m_nb_halfbw = 16;

unsigned int BladerfBandwidths::getBandwidth(unsigned int bandwidth_index)
{
	if (bandwidth_index < m_nb_halfbw)
	{
		return m_halfbw[bandwidth_index] * 2;
	}
	else
	{
		return m_halfbw[0] * 2;
	}
}

unsigned int BladerfBandwidths::getBandwidthIndex(unsigned int bandwidth)
{
	for (unsigned int i=0; i < m_nb_halfbw; i++)
	{
		if (bandwidth/2000 == m_halfbw[i])
		{
			return i;
		}
	}

	return 0;
}
