///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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
#include <QMessageBox>
#include <QFileDialog>

#include <libbladeRF.h>

#include "ui_bladerf1inputgui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/basicdevicesettingsdialog.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "device/deviceuiset.h"

#include "bladerf1inputgui.h"

Bladerf1InputGui::Bladerf1InputGui(DeviceUISet *deviceUISet, QWidget* parent) :
	DeviceGUI(parent),
	ui(new Ui::Bladerf1InputGui),
	m_forceSettings(true),
	m_doApplySettings(true),
	m_settings(),
    m_sampleRateMode(true),
	m_sampleSource(NULL),
	m_sampleRate(0),
	m_lastEngineState(DeviceAPI::StNotStarted)
{
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_sampleSource = (Bladerf1Input*) m_deviceUISet->m_deviceAPI->getSampleSource();

    ui->setupUi(getContents());
    sizeToContents();
    getContents()->setStyleSheet("#Bladerf1InputGui { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplesource/bladerf1input/readme.md";
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

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));

	displaySettings();

	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleSource->setMessageQueueToGUI(&m_inputMessageQueue);

	sendSettings();
    makeUIConnections();
}

Bladerf1InputGui::~Bladerf1InputGui()
{
    m_statusTimer.stop();
    m_updateTimer.stop();
	delete ui;
}

void Bladerf1InputGui::destroy()
{
	delete this;
}

void Bladerf1InputGui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
    m_forceSettings = true;
	sendSettings();
}

QByteArray Bladerf1InputGui::serialize() const
{
	return m_settings.serialize();
}

bool Bladerf1InputGui::deserialize(const QByteArray& data)
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

bool Bladerf1InputGui::handleMessage(const Message& message)
{
    if (Bladerf1Input::MsgConfigureBladerf1::match(message))
    {
        const Bladerf1Input::MsgConfigureBladerf1& cfg = (Bladerf1Input::MsgConfigureBladerf1&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (Bladerf1Input::MsgStartStop::match(message))
    {
        Bladerf1Input::MsgStartStop& notif = (Bladerf1Input::MsgStartStop&) message;
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

void Bladerf1InputGui::handleInputMessages()
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
        else
        {
            if (handleMessage(*message))
            {
                delete message;
            }
        }
    }
}

void Bladerf1InputGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    displaySampleRate();
}

void Bladerf1InputGui::displaySampleRate()
{
    ui->sampleRate->blockSignals(true);
    displayFcTooltip();

    if (m_sampleRateMode)
    {
        ui->sampleRateMode->setStyleSheet("QToolButton { background:rgb(60,60,60); }");
        ui->sampleRateMode->setText("SR");
        // BladeRF can go as low as 80 kS/s but because of buffering in practice experience is not good below 330 kS/s
        ui->sampleRate->setValueRange(8, 330000U, BLADERF_SAMPLERATE_REC_MAX);
        ui->sampleRate->setValue(m_settings.m_devSampleRate);
        ui->sampleRate->setToolTip("Device to host sample rate (S/s)");
        ui->deviceRateText->setToolTip("Baseband sample rate (S/s)");
        uint32_t basebandSampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2Decim);
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(basebandSampleRate / 1000.0f, 'g', 5)));
    }
    else
    {
        ui->sampleRateMode->setStyleSheet("QToolButton { background:rgb(50,50,50); }");
        ui->sampleRateMode->setText("BB");
        // BladeRF can go as low as 80 kS/s but because of buffering in practice experience is not good below 330 kS/s
        ui->sampleRate->setValueRange(8, 330000U/(1<<m_settings.m_log2Decim), BLADERF_SAMPLERATE_REC_MAX/(1<<m_settings.m_log2Decim));
        ui->sampleRate->setValue(m_settings.m_devSampleRate/(1<<m_settings.m_log2Decim));
        ui->sampleRate->setToolTip("Baseband sample rate (S/s)");
        ui->deviceRateText->setToolTip("Device to host sample rate (S/s)");
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(m_settings.m_devSampleRate / 1000.0f, 'g', 5)));
    }

    ui->sampleRate->blockSignals(false);
}

void Bladerf1InputGui::displayFcTooltip()
{
    int32_t fShift = DeviceSampleSource::calculateFrequencyShift(
        m_settings.m_log2Decim,
        (DeviceSampleSource::fcPos_t) m_settings.m_fcPos,
        m_settings.m_devSampleRate,
        DeviceSampleSource::FrequencyShiftScheme::FSHIFT_STD
    );
    ui->fcPos->setToolTip(tr("Relative position of device center frequency: %1 kHz").arg(QString::number(fShift / 1000.0f, 'g', 5)));
}

void Bladerf1InputGui::displaySettings()
{
    blockApplySettings(true);

    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
	displaySampleRate();

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

void Bladerf1InputGui::sendSettings()
{
	if(!m_updateTimer.isActive())
		m_updateTimer.start(100);
}

void Bladerf1InputGui::on_centerFrequency_changed(quint64 value)
{
	m_settings.m_centerFrequency = value * 1000;
    m_settingsKeys.append("centerFrequency");
	sendSettings();
}

void Bladerf1InputGui::on_sampleRate_changed(quint64 value)
{
    if (m_sampleRateMode) {
        m_settings.m_devSampleRate = value;
    } else {
        m_settings.m_devSampleRate = value * (1 << m_settings.m_log2Decim);
    }

    displayFcTooltip();
    m_settingsKeys.append("devSampleRate");
    sendSettings();
}

void Bladerf1InputGui::on_dcOffset_toggled(bool checked)
{
	m_settings.m_dcBlock = checked;
    m_settingsKeys.append("dcBlock");
	sendSettings();
}

void Bladerf1InputGui::on_iqImbalance_toggled(bool checked)
{
	m_settings.m_iqCorrection = checked;
    m_settingsKeys.append("iqCorrection");
	sendSettings();
}

void Bladerf1InputGui::on_bandwidth_currentIndexChanged(int index)
{
	int newbw = BladerfBandwidths::getBandwidth(index);
	m_settings.m_bandwidth = newbw * 1000;
    m_settingsKeys.append("bandwidth");
	sendSettings();
}

void Bladerf1InputGui::on_decim_currentIndexChanged(int index)
{
	if ((index <0) || (index > 6)) {
		return;
    }

	m_settings.m_log2Decim = index;
    displaySampleRate();

    if (m_sampleRateMode) {
        m_settings.m_devSampleRate = ui->sampleRate->getValueNew();
    } else {
        m_settings.m_devSampleRate = ui->sampleRate->getValueNew() * (1 << m_settings.m_log2Decim);
    }

    m_settingsKeys.append("log2Decim");
    m_settingsKeys.append("devSampleRate");
	sendSettings();
}

void Bladerf1InputGui::on_fcPos_currentIndexChanged(int index)
{
    m_settings.m_fcPos = (BladeRF1InputSettings::fcPos_t) (index < 0 ? 0 : index > 2 ? 2 : index);
    displayFcTooltip();
    m_settingsKeys.append("fcPos");
    sendSettings();
}

void Bladerf1InputGui::on_lna_currentIndexChanged(int index)
{
	qDebug() << "BladerfGui: LNA gain = " << index * 3 << " dB";

	if ((index < 0) || (index > 2))
		return;

	m_settings.m_lnaGain = index;
    m_settingsKeys.append("lnaGain");
	sendSettings();
}

void Bladerf1InputGui::on_vga1_valueChanged(int value)
{
	if ((value < BLADERF_RXVGA1_GAIN_MIN) || (value > BLADERF_RXVGA1_GAIN_MAX))
		return;

	ui->vga1Text->setText(tr("%1dB").arg(value));
	m_settings.m_vga1 = value;
    m_settingsKeys.append("vga1");
	sendSettings();
}

void Bladerf1InputGui::on_vga2_valueChanged(int value)
{
	if ((value < BLADERF_RXVGA2_GAIN_MIN) || (value > BLADERF_RXVGA2_GAIN_MAX))
		return;

	ui->vga2Text->setText(tr("%1dB").arg(value));
	m_settings.m_vga2 = value;
    m_settingsKeys.append("vga2");
	sendSettings();
}

void Bladerf1InputGui::on_xb200_currentIndexChanged(int index)
{
	if (index == 1) // bypass
	{
		m_settings.m_xb200 = true;
		m_settings.m_xb200Path = BLADERF_XB200_BYPASS;
        m_settingsKeys.append("xb200");
        m_settingsKeys.append("xb200Path");
	}
	else if (index == 2) // Auto 1dB
	{
		m_settings.m_xb200 = true;
		m_settings.m_xb200Path = BLADERF_XB200_MIX;
		m_settings.m_xb200Filter = BLADERF_XB200_AUTO_1DB;
        m_settingsKeys.append("xb200");
        m_settingsKeys.append("xb200Path");
        m_settingsKeys.append("xb200Filter");
	}
	else if (index == 3) // Auto 3dB
	{
		m_settings.m_xb200 = true;
		m_settings.m_xb200Path = BLADERF_XB200_MIX;
		m_settings.m_xb200Filter = BLADERF_XB200_AUTO_3DB;
        m_settingsKeys.append("xb200");
        m_settingsKeys.append("xb200Path");
        m_settingsKeys.append("xb200Filter");
	}
	else if (index == 4) // Custom
	{
		m_settings.m_xb200 = true;
		m_settings.m_xb200Path = BLADERF_XB200_MIX;
		m_settings.m_xb200Filter = BLADERF_XB200_CUSTOM;
        m_settingsKeys.append("xb200");
        m_settingsKeys.append("xb200Path");
        m_settingsKeys.append("xb200Filter");
	}
	else if (index == 5) // 50 MHz
	{
		m_settings.m_xb200 = true;
		m_settings.m_xb200Path = BLADERF_XB200_MIX;
		m_settings.m_xb200Filter = BLADERF_XB200_50M;
        m_settingsKeys.append("xb200");
        m_settingsKeys.append("xb200Path");
        m_settingsKeys.append("xb200Filter");
	}
	else if (index == 6) // 144 MHz
	{
		m_settings.m_xb200 = true;
		m_settings.m_xb200Path = BLADERF_XB200_MIX;
		m_settings.m_xb200Filter = BLADERF_XB200_144M;
        m_settingsKeys.append("xb200");
        m_settingsKeys.append("xb200Path");
        m_settingsKeys.append("xb200Filter");
	}
	else if (index == 7) // 222 MHz
	{
		m_settings.m_xb200 = true;
		m_settings.m_xb200Path = BLADERF_XB200_MIX;
		m_settings.m_xb200Filter = BLADERF_XB200_222M;
        m_settingsKeys.append("xb200");
        m_settingsKeys.append("xb200Path");
        m_settingsKeys.append("xb200Filter");
	}
	else // no xb200
	{
		m_settings.m_xb200 = false;
        m_settingsKeys.append("xb200");
	}

	if (m_settings.m_xb200) {
        ui->centerFrequency->setValueRange(7, BLADERF_FREQUENCY_MIN_XB200/1000, BLADERF_FREQUENCY_MAX/1000);
	} else {
        ui->centerFrequency->setValueRange(7, BLADERF_FREQUENCY_MIN/1000, BLADERF_FREQUENCY_MAX/1000);
	}

	sendSettings();
}

void Bladerf1InputGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        Bladerf1Input::MsgStartStop *message = Bladerf1Input::MsgStartStop::create(checked);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void Bladerf1InputGui::on_sampleRateMode_toggled(bool checked)
{
    m_sampleRateMode = checked;
    displaySampleRate();
}

void Bladerf1InputGui::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "BladerfGui::updateHardware";
        Bladerf1Input::MsgConfigureBladerf1* message = Bladerf1Input::MsgConfigureBladerf1::create(m_settings, m_settingsKeys, m_forceSettings);
        m_sampleSource->getInputMessageQueue()->push(message);
        m_settingsKeys.clear();
        m_forceSettings = false;
        m_updateTimer.stop();
    }
}

void Bladerf1InputGui::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void Bladerf1InputGui::updateStatus()
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

unsigned int Bladerf1InputGui::getXb200Index(bool xb_200, bladerf_xb200_path xb200Path, bladerf_xb200_filter xb200Filter)
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
			else // xb200Filter == BLADERF_XB200_222M
			{
				return 7;
			}
		}
	}
	else
	{
		return 0;
	}
}

void Bladerf1InputGui::openDeviceSettingsDialog(const QPoint& p)
{
    if (m_contextMenuType == ContextMenuDeviceSettings)
    {
        BasicDeviceSettingsDialog dialog(this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);

        dialog.move(p);
        dialog.exec();

        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settingsKeys.append("transverterMode");
        m_settingsKeys.append("m_transverterDeltaFrequency");
        m_settingsKeys.append("m_iqOrder");
        m_settingsKeys.append("centerFrequency");

        sendSettings();
    }

    resetContextMenuType();
}

void Bladerf1InputGui::makeUIConnections()
{
    QObject::connect(ui->centerFrequency, &ValueDial::changed, this, &Bladerf1InputGui::on_centerFrequency_changed);
    QObject::connect(ui->sampleRate, &ValueDial::changed, this, &Bladerf1InputGui::on_sampleRate_changed);
    QObject::connect(ui->dcOffset, &ButtonSwitch::toggled, this, &Bladerf1InputGui::on_dcOffset_toggled);
    QObject::connect(ui->iqImbalance, &ButtonSwitch::toggled, this, &Bladerf1InputGui::on_iqImbalance_toggled);
    QObject::connect(ui->bandwidth, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Bladerf1InputGui::on_bandwidth_currentIndexChanged);
    QObject::connect(ui->decim, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Bladerf1InputGui::on_decim_currentIndexChanged);
    QObject::connect(ui->lna, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Bladerf1InputGui::on_lna_currentIndexChanged);
    QObject::connect(ui->fcPos, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Bladerf1InputGui::on_fcPos_currentIndexChanged);
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &Bladerf1InputGui::on_startStop_toggled);
    QObject::connect(ui->sampleRateMode, &QToolButton::toggled, this, &Bladerf1InputGui::on_sampleRateMode_toggled);
}
