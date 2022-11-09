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

#include <libairspy/airspy.h>

#include "airspygui.h"

#include "device/deviceapi.h"
#include "device/deviceuiset.h"

#include "ui_airspygui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/basicdevicesettingsdialog.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"

AirspyGui::AirspyGui(DeviceUISet *deviceUISet, QWidget* parent) :
	DeviceGUI(parent),
	ui(new Ui::AirspyGui),
	m_doApplySettings(true),
	m_forceSettings(true),
	m_settings(),
	m_sampleSource(0),
	m_lastEngineState(DeviceAPI::StNotStarted)
{
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_sampleSource = (AirspyInput*) m_deviceUISet->m_deviceAPI->getSampleSource();

    ui->setupUi(getContents());
    sizeToContents();
    getContents()->setStyleSheet("#AirspyGui { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplesource/airspy/readme.md";
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
	updateFrequencyLimits();

	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));

	displaySettings();

	m_rates = ((AirspyInput*) m_sampleSource)->getSampleRates();
	displaySampleRates();
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleSource->setMessageQueueToGUI(&m_inputMessageQueue);

    sendSettings();
    makeUIConnections();
}

AirspyGui::~AirspyGui()
{
    m_statusTimer.stop();
    m_updateTimer.stop();
    delete ui;
}

void AirspyGui::destroy()
{
	delete this;
}

void AirspyGui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
    m_forceSettings = true;
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
		m_forceSettings = true;
		sendSettings();
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}

bool AirspyGui::handleMessage(const Message& message)
{
    if (AirspyInput::MsgConfigureAirspy::match(message))
    {
        const AirspyInput::MsgConfigureAirspy& cfg = (AirspyInput::MsgConfigureAirspy&) message;

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
    else if (AirspyInput::MsgStartStop::match(message))
    {
        AirspyInput::MsgStartStop& notif = (AirspyInput::MsgStartStop&) message;
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

void AirspyGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        qDebug("AirspyGui::handleInputMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("AirspyGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
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

void AirspyGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_sampleRate / 1000));
}

void AirspyGui::updateFrequencyLimits()
{
    // values in kHz
    qint64 deltaFrequency = m_settings.m_transverterMode ? m_settings.m_transverterDeltaFrequency/1000 : 0;
    qint64 minLimit = AirspyInput::loLowLimitFreq/1000 + deltaFrequency;
    qint64 maxLimit = AirspyInput::loHighLimitFreq/1000 + deltaFrequency;

    if (m_settings.m_transverterMode)
    {
        minLimit = minLimit < 0 ? 0 : minLimit > 999999999 ? 999999999 : minLimit;
        maxLimit = maxLimit < 0 ? 0 : maxLimit > 999999999 ? 999999999 : maxLimit;
        ui->centerFrequency->setValueRange(9, minLimit, maxLimit);
    }
    else
    {
        minLimit = minLimit < 0 ? 0 : minLimit > 9999999 ? 9999999 : minLimit;
        maxLimit = maxLimit < 0 ? 0 : maxLimit > 9999999 ? 9999999 : maxLimit;
        ui->centerFrequency->setValueRange(7, minLimit, maxLimit);
    }
    qDebug("AirspyGui::updateFrequencyLimits: delta: %lld min: %lld max: %lld", deltaFrequency, minLimit, maxLimit);
}

void AirspyGui::displaySettings()
{
    ui->transverter->setDeltaFrequency(m_settings.m_transverterDeltaFrequency);
    ui->transverter->setDeltaFrequencyActive(m_settings.m_transverterMode);
    ui->transverter->setIQOrder(m_settings.m_iqOrder);
    updateFrequencyLimits();
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
	unsigned int savedIndex = m_settings.m_devSampleRateIndex;
	ui->sampleRate->blockSignals(true);

	if (m_rates.size() > 0)
	{
		ui->sampleRate->clear();

		for (unsigned int i = 0; i < m_rates.size(); i++)
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
    m_settingsKeys.append("centerFrequency");
	sendSettings();
}

void AirspyGui::on_LOppm_valueChanged(int value)
{
	m_settings.m_LOppmTenths = value;
	ui->LOppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));
    m_settingsKeys.append("LOppmTenths");
	sendSettings();
}

void AirspyGui::on_dcOffset_toggled(bool checked)
{
	m_settings.m_dcBlock = checked;
    m_settingsKeys.append("dcBlock");
	sendSettings();
}

void AirspyGui::on_iqImbalance_toggled(bool checked)
{
	m_settings.m_iqCorrection = checked;
    m_settingsKeys.append("iqCorrection");
	sendSettings();
}

void AirspyGui::on_sampleRate_currentIndexChanged(int index)
{
	m_settings.m_devSampleRateIndex = index;
    m_settingsKeys.append("devSampleRateIndex");
	sendSettings();
}

void AirspyGui::on_biasT_stateChanged(int state)
{
	m_settings.m_biasT = (state == Qt::Checked);
    m_settingsKeys.append("biasT");
	sendSettings();
}

void AirspyGui::on_lnaAGC_stateChanged(int state)
{
	m_settings.m_lnaAGC = (state == Qt::Checked);
    m_settingsKeys.append("lnaAGC");
	sendSettings();
}

void AirspyGui::on_mixAGC_stateChanged(int state)
{
	m_settings.m_mixerAGC = (state == Qt::Checked);
    m_settingsKeys.append("mixerAGC");
	sendSettings();
}

void AirspyGui::on_decim_currentIndexChanged(int index)
{
	if ((index <0) || (index > 6))
		return;
	m_settings.m_log2Decim = index;
    m_settingsKeys.append("log2Decim");
	sendSettings();
}

void AirspyGui::on_fcPos_currentIndexChanged(int index)
{
	if (index == 0) {
		m_settings.m_fcPos = AirspySettings::FC_POS_INFRA;
        m_settingsKeys.append("fcPos");
		sendSettings();
	} else if (index == 1) {
		m_settings.m_fcPos = AirspySettings::FC_POS_SUPRA;
        m_settingsKeys.append("fcPos");
		sendSettings();
	} else if (index == 2) {
		m_settings.m_fcPos = AirspySettings::FC_POS_CENTER;
        m_settingsKeys.append("fcPos");
		sendSettings();
	}
}

void AirspyGui::on_lna_valueChanged(int value)
{
	if ((value < 0) || (value > 14))
		return;

	ui->lnaGainText->setText(tr("%1dB").arg(value));
	m_settings.m_lnaGain = value;
    m_settingsKeys.append("lnaGain");
	sendSettings();
}

void AirspyGui::on_mix_valueChanged(int value)
{
	if ((value < 0) || (value > 15))
		return;

	ui->mixText->setText(tr("%1dB").arg(value));
	m_settings.m_mixerGain = value;
    m_settingsKeys.append("mixerGain");
	sendSettings();
}

void AirspyGui::on_vga_valueChanged(int value)
{
	if ((value < 0) || (value > 15))
		return;

	ui->vgaText->setText(tr("%1dB").arg(value));
	m_settings.m_vgaGain = value;
    m_settingsKeys.append("vgaGain");
	sendSettings();
}

void AirspyGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        AirspyInput::MsgStartStop *message = AirspyInput::MsgStartStop::create(checked);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void AirspyGui::on_transverter_clicked()
{
    m_settings.m_transverterMode = ui->transverter->getDeltaFrequencyAcive();
    m_settings.m_transverterDeltaFrequency = ui->transverter->getDeltaFrequency();
    m_settings.m_iqOrder = ui->transverter->getIQOrder();
    qDebug("AirspyGui::on_transverter_clicked: %lld Hz %s", m_settings.m_transverterDeltaFrequency, m_settings.m_transverterMode ? "on" : "off");
    updateFrequencyLimits();
    m_settings.m_centerFrequency = ui->centerFrequency->getValueNew()*1000;
    m_settingsKeys.append("transverterMode");
    m_settingsKeys.append("m_transverterDeltaFrequency");
    m_settingsKeys.append("m_iqOrder");
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}

void AirspyGui::updateHardware()
{
	qDebug() << "AirspyGui::updateHardware";
	AirspyInput::MsgConfigureAirspy* message = AirspyInput::MsgConfigureAirspy::create(m_settings, m_settingsKeys, m_forceSettings);
	m_sampleSource->getInputMessageQueue()->push(message);
    m_settingsKeys.clear();
	m_forceSettings = false;
	m_updateTimer.stop();
}

void AirspyGui::updateStatus()
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

void AirspyGui::openDeviceSettingsDialog(const QPoint& p)
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

        m_settingsKeys.append("useReverseAPI");
        m_settingsKeys.append("reverseAPIAddress");
        m_settingsKeys.append("reverseAPIPort");
        m_settingsKeys.append("reverseAPIDeviceIndex");
        sendSettings();
    }

    resetContextMenuType();
}

void AirspyGui::makeUIConnections()
{
    QObject::connect(ui->centerFrequency, &ValueDial::changed, this, &AirspyGui::on_centerFrequency_changed);
    QObject::connect(ui->LOppm, &QSlider::valueChanged, this, &AirspyGui::on_LOppm_valueChanged);
    QObject::connect(ui->dcOffset, &ButtonSwitch::toggled, this, &AirspyGui::on_dcOffset_toggled);
    QObject::connect(ui->iqImbalance, &ButtonSwitch::toggled, this, &AirspyGui::on_iqImbalance_toggled);
    QObject::connect(ui->sampleRate, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AirspyGui::on_sampleRate_currentIndexChanged);
    QObject::connect(ui->biasT, &QCheckBox::stateChanged, this, &AirspyGui::on_biasT_stateChanged);
    QObject::connect(ui->decim, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AirspyGui::on_decim_currentIndexChanged);
    QObject::connect(ui->fcPos, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AirspyGui::on_fcPos_currentIndexChanged);
    QObject::connect(ui->lna, &QSlider::valueChanged, this, &AirspyGui::on_lna_valueChanged);
    QObject::connect(ui->mix, &QSlider::valueChanged, this, &AirspyGui::on_mix_valueChanged);
    QObject::connect(ui->vga, &QSlider::valueChanged, this, &AirspyGui::on_vga_valueChanged);
    QObject::connect(ui->lnaAGC, &QCheckBox::stateChanged, this, &AirspyGui::on_lnaAGC_stateChanged);
    QObject::connect(ui->mixAGC, &QCheckBox::stateChanged, this, &AirspyGui::on_mixAGC_stateChanged);
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &AirspyGui::on_startStop_toggled);
    QObject::connect(ui->transverter, &TransverterButton::clicked, this, &AirspyGui::on_transverter_clicked);
}
