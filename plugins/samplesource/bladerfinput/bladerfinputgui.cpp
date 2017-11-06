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

#include "../bladerfinput/bladerfinputgui.h"

#include <QDebug>
#include <QMessageBox>

#include <libbladeRF.h>

#include "ui_bladerfinputgui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include <device/devicesourceapi.h>
#include "device/deviceuiset.h"

BladerfInputGui::BladerfInputGui(DeviceUISet *deviceUISet, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::BladerfInputGui),
	m_deviceUISet(deviceUISet),
	m_forceSettings(true),
	m_doApplySettings(true),
	m_settings(),
	m_sampleSource(NULL),
	m_sampleRate(0),
	m_lastEngineState((DSPDeviceSourceEngine::State)-1)
{
    m_sampleSource = (BladerfInput*) m_deviceUISet->m_deviceSourceAPI->getSampleSource();

	ui->setupUi(this);
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
	ui->centerFrequency->setValueRange(7, BLADERF_FREQUENCY_MIN_XB200/1000, BLADERF_FREQUENCY_MAX/1000);

    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    // BladeRF can go as low as 80 kS/s but because of buffering in practice experience is not good below 330 kS/s
    ui->sampleRate->setValueRange(8, 330000U, BLADERF_SAMPLERATE_REC_MAX);

	ui->bandwidth->clear();
	for (unsigned int i = 0; i < BladerfBandwidths::getNbBandwidths(); i++)
	{
		ui->bandwidth->addItem(QString::number(BladerfBandwidths::getBandwidth(i)));
	}

	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);

	displaySettings();

	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);

	sendSettings();
}

BladerfInputGui::~BladerfInputGui()
{
	delete ui;
}

void BladerfInputGui::destroy()
{
	delete this;
}

void BladerfInputGui::setName(const QString& name)
{
	setObjectName(name);
}

QString BladerfInputGui::getName() const
{
	return objectName();
}

void BladerfInputGui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

qint64 BladerfInputGui::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void BladerfInputGui::setCenterFrequency(qint64 centerFrequency)
{
	m_settings.m_centerFrequency = centerFrequency;
	displaySettings();
	sendSettings();
}

QByteArray BladerfInputGui::serialize() const
{
	return m_settings.serialize();
}

bool BladerfInputGui::deserialize(const QByteArray& data)
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

bool BladerfInputGui::handleMessage(const Message& message __attribute__((unused)))
{
    return false;
}

void BladerfInputGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        qDebug("BladerfGui::handleInputMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("BladerfGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();

            delete message;
        }
    }
}

void BladerfInputGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateLabel->setText(tr("%1k").arg(QString::number(m_sampleRate / 1000.0f, 'g', 5)));
}

void BladerfInputGui::displaySettings()
{
    blockApplySettings(true);

    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
	ui->sampleRate->setValue(m_settings.m_devSampleRate);

	ui->dcOffset->setChecked(m_settings.m_dcBlock);
	ui->iqImbalance->setChecked(m_settings.m_iqCorrection);

	unsigned int bandwidthIndex = BladerfBandwidths::getBandwidthIndex(m_settings.m_bandwidth);
	ui->bandwidth->setCurrentIndex(bandwidthIndex);

	ui->decim->setCurrentIndex(m_settings.m_log2Decim);

	ui->fcPos->setCurrentIndex((int) m_settings.m_fcPos);

	ui->lna->setCurrentIndex(m_settings.m_lnaGain);

	ui->vga1Text->setText(tr("%1dB").arg(m_settings.m_vga1));
	ui->vga1->setValue(m_settings.m_vga1);

	ui->vga2Text->setText(tr("%1dB").arg(m_settings.m_vga2));
	ui->vga2->setValue(m_settings.m_vga2);

	ui->xb200->setCurrentIndex(getXb200Index(m_settings.m_xb200, m_settings.m_xb200Path, m_settings.m_xb200Filter));

	blockApplySettings(false);
}

void BladerfInputGui::sendSettings()
{
	if(!m_updateTimer.isActive())
		m_updateTimer.start(100);
}

void BladerfInputGui::on_centerFrequency_changed(quint64 value)
{
	m_settings.m_centerFrequency = value * 1000;
	sendSettings();
}

void BladerfInputGui::on_sampleRate_changed(quint64 value)
{
    m_settings.m_devSampleRate = value;
    sendSettings();
}

void BladerfInputGui::on_dcOffset_toggled(bool checked)
{
	m_settings.m_dcBlock = checked;
	sendSettings();
}

void BladerfInputGui::on_iqImbalance_toggled(bool checked)
{
	m_settings.m_iqCorrection = checked;
	sendSettings();
}

void BladerfInputGui::on_bandwidth_currentIndexChanged(int index)
{
	int newbw = BladerfBandwidths::getBandwidth(index);
	m_settings.m_bandwidth = newbw * 1000;
	sendSettings();
}

void BladerfInputGui::on_decim_currentIndexChanged(int index)
{
	if ((index <0) || (index > 5))
		return;
	m_settings.m_log2Decim = index;
	sendSettings();
}

void BladerfInputGui::on_fcPos_currentIndexChanged(int index)
{
	if (index == 0) {
		m_settings.m_fcPos = BladeRFInputSettings::FC_POS_INFRA;
		sendSettings();
	} else if (index == 1) {
		m_settings.m_fcPos = BladeRFInputSettings::FC_POS_SUPRA;
		sendSettings();
	} else if (index == 2) {
		m_settings.m_fcPos = BladeRFInputSettings::FC_POS_CENTER;
		sendSettings();
	}
}

void BladerfInputGui::on_lna_currentIndexChanged(int index)
{
	qDebug() << "BladerfGui: LNA gain = " << index * 3 << " dB";

	if ((index < 0) || (index > 2))
		return;

	m_settings.m_lnaGain = index;
	sendSettings();
}

void BladerfInputGui::on_vga1_valueChanged(int value)
{
	if ((value < BLADERF_RXVGA1_GAIN_MIN) || (value > BLADERF_RXVGA1_GAIN_MAX))
		return;

	ui->vga1Text->setText(tr("%1dB").arg(value));
	m_settings.m_vga1 = value;
	sendSettings();
}

void BladerfInputGui::on_vga2_valueChanged(int value)
{
	if ((value < BLADERF_RXVGA2_GAIN_MIN) || (value > BLADERF_RXVGA2_GAIN_MAX))
		return;

	ui->vga2Text->setText(tr("%1dB").arg(value));
	m_settings.m_vga2 = value;
	sendSettings();
}

void BladerfInputGui::on_xb200_currentIndexChanged(int index)
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

void BladerfInputGui::on_startStop_toggled(bool checked)
{
    if (checked)
    {
        if (m_deviceUISet->m_deviceSourceAPI->initAcquisition())
        {
            m_deviceUISet->m_deviceSourceAPI->startAcquisition();
            DSPEngine::instance()->startAudioOutput();
        }
    }
    else
    {
        m_deviceUISet->m_deviceSourceAPI->stopAcquisition();
        DSPEngine::instance()->stopAudioOutput();
    }
}

void BladerfInputGui::on_record_toggled(bool checked)
{
    if (checked) {
        ui->record->setStyleSheet("QToolButton { background-color : red; }");
    } else {
        ui->record->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
    }

    BladerfInput::MsgFileRecord* message = BladerfInput::MsgFileRecord::create(checked);
    m_sampleSource->getInputMessageQueue()->push(message);
}

void BladerfInputGui::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "BladerfGui::updateHardware";
        BladerfInput::MsgConfigureBladerf* message = BladerfInput::MsgConfigureBladerf::create(m_settings, m_forceSettings);
        m_sampleSource->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_updateTimer.stop();
    }
}

void BladerfInputGui::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void BladerfInputGui::updateStatus()
{
    int state = m_deviceUISet->m_deviceSourceAPI->state();

    if(m_lastEngineState != state)
    {
        switch(state)
        {
            case DSPDeviceSourceEngine::StNotStarted:
                ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case DSPDeviceSourceEngine::StIdle:
                ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case DSPDeviceSourceEngine::StRunning:
                ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case DSPDeviceSourceEngine::StError:
                ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::information(this, tr("Message"), m_deviceUISet->m_deviceSourceAPI->errorMessage());
                break;
            default:
                break;
        }

        m_lastEngineState = state;
    }
}

unsigned int BladerfInputGui::getXb200Index(bool xb_200, bladerf_xb200_path xb200Path, bladerf_xb200_filter xb200Filter)
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

