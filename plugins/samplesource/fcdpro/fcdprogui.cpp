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

#include <QMessageBox>

#include "ui_fcdprogui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "fcdprogui.h"

#include <device/devicesourceapi.h>
#include "device/deviceuiset.h"
#include "fcdproconst.h"
#include "fcdtraits.h"

FCDProGui::FCDProGui(DeviceUISet *deviceUISet, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::FCDProGui),
	m_deviceUISet(deviceUISet),
	m_forceSettings(true),
	m_settings(),
	m_sampleSource(NULL),
	m_lastEngineState((DSPDeviceSourceEngine::State)-1)
{
    m_sampleSource = (FCDProInput*) m_deviceUISet->m_deviceSourceAPI->getSampleSource();

	ui->setupUi(this);
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    updateFrequencyLimits();

	ui->lnaGain->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_lna_gain_nb_values(); i++)
	{
		ui->lnaGain->addItem(QString(FCDProConstants::lna_gains[i].label.c_str()), i);
	}

	ui->rfFilter->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_rf_filter_nb_values(); i++)
	{
		ui->rfFilter->addItem(QString(FCDProConstants::rf_filters[i].label.c_str()), i);
	}

	ui->lnaEnhance->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_lna_enhance_nb_values(); i++)
	{
		ui->lnaEnhance->addItem(QString(FCDProConstants::lna_enhances[i].label.c_str()), i);
	}

	ui->band->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_band_nb_values(); i++)
	{
		ui->band->addItem(QString(FCDProConstants::bands[i].label.c_str()), i);
	}

	ui->mixGain->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_mixer_gain_nb_values(); i++)
	{
		ui->mixGain->addItem(QString(FCDProConstants::mixer_gains[i].label.c_str()), i);
	}

	ui->mixFilter->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_mixer_filter_nb_values(); i++)
	{
		ui->mixFilter->addItem(QString(FCDProConstants::mixer_filters[i].label.c_str()), i);
	}

	ui->bias->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_bias_current_nb_values(); i++)
	{
		ui->bias->addItem(QString(FCDProConstants::bias_currents[i].label.c_str()), i);
	}

	ui->mode->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_if_gain_mode_nb_values(); i++)
	{
		ui->mode->addItem(QString(FCDProConstants::if_gain_modes[i].label.c_str()), i);
	}

	ui->gain1->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_if_gain1_nb_values(); i++)
	{
		ui->gain1->addItem(QString(FCDProConstants::if_gains1[i].label.c_str()), i);
	}

	ui->rcFilter->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_if_rc_filter_nb_values(); i++)
	{
		ui->rcFilter->addItem(QString(FCDProConstants::if_rc_filters[i].label.c_str()), i);
	}

	ui->gain2->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_if_gain2_nb_values(); i++)
	{
		ui->gain2->addItem(QString(FCDProConstants::if_gains2[i].label.c_str()), i);
	}

	ui->gain3->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_if_gain3_nb_values(); i++)
	{
		ui->gain3->addItem(QString(FCDProConstants::if_gains3[i].label.c_str()), i);
	}

	ui->gain4->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_if_gain4_nb_values(); i++)
	{
		ui->gain4->addItem(QString(FCDProConstants::if_gains4[i].label.c_str()), i);
	}

	ui->ifFilter->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_if_filter_nb_values(); i++)
	{
		ui->ifFilter->addItem(QString(FCDProConstants::if_filters[i].label.c_str()), i);
	}

	ui->gain5->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_if_gain5_nb_values(); i++)
	{
		ui->gain5->addItem(QString(FCDProConstants::if_gains5[i].label.c_str()), i);
	}

	ui->gain6->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_if_gain6_nb_values(); i++)
	{
		ui->gain6->addItem(QString(FCDProConstants::if_gains6[i].label.c_str()), i);
	}

	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);

	displaySettings();

	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
}

FCDProGui::~FCDProGui()
{
	delete ui;
}

void FCDProGui::destroy()
{
	delete this;
}

void FCDProGui::setName(const QString& name)
{
	setObjectName(name);
}

QString FCDProGui::getName() const
{
	return objectName();
}

void FCDProGui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

qint64 FCDProGui::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void FCDProGui::setCenterFrequency(qint64 centerFrequency)
{
	m_settings.m_centerFrequency = centerFrequency;
	displaySettings();
	sendSettings();
}

QByteArray FCDProGui::serialize() const
{
	return m_settings.serialize();
}

bool FCDProGui::deserialize(const QByteArray& data)
{
	if(m_settings.deserialize(data))
	{
		displaySettings();
		m_forceSettings = true;
		sendSettings();
		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}

bool FCDProGui::handleMessage(const Message& message __attribute__((unused)))
{
    if (FCDProInput::MsgConfigureFCD::match(message))
    {
        const FCDProInput::MsgConfigureFCD& cfg = (FCDProInput::MsgConfigureFCD&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (FCDProInput::MsgStartStop::match(message))
    {
        FCDProInput::MsgStartStop& notif = (FCDProInput::MsgStartStop&) message;
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

void FCDProGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        qDebug("FCDProGui::handleInputMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("FCDProGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();

            delete message;
        }
    }
}

void FCDProGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_sampleRate / 1000));
}

void FCDProGui::updateFrequencyLimits()
{
    // values in kHz
    qint64 deltaFrequency = m_settings.m_transverterMode ? m_settings.m_transverterDeltaFrequency/1000 : 0;
    qint64 minLimit = fcd_traits<Pro>::loLowLimitFreq/1000 + deltaFrequency;
    qint64 maxLimit = fcd_traits<Pro>::loHighLimitFreq/1000 + deltaFrequency;

    minLimit = minLimit < 0 ? 0 : minLimit > 9999999 ? 9999999 : minLimit;
    maxLimit = maxLimit < 0 ? 0 : maxLimit > 9999999 ? 9999999 : maxLimit;

    qDebug("FCDProGui::updateFrequencyLimits: delta: %lld min: %lld max: %lld", deltaFrequency, minLimit, maxLimit);

    ui->centerFrequency->setValueRange(7, minLimit, maxLimit);
}

void FCDProGui::displaySettings()
{
    ui->transverter->setDeltaFrequency(m_settings.m_transverterDeltaFrequency);
    ui->transverter->setDeltaFrequencyActive(m_settings.m_transverterMode);
    updateFrequencyLimits();
	ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
	ui->ppm->setValue(m_settings.m_LOppmTenths);
	ui->ppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));
	ui->dcOffset->setChecked(m_settings.m_dcBlock);
	ui->iqImbalance->setChecked(m_settings.m_iqCorrection);

	ui->lnaGain->setCurrentIndex(m_settings.m_lnaGainIndex);
	ui->rfFilter->setCurrentIndex(m_settings.m_rfFilterIndex);
	ui->lnaEnhance->setCurrentIndex(m_settings.m_lnaEnhanceIndex);
	ui->band->setCurrentIndex(m_settings.m_bandIndex);
	ui->mixGain->setCurrentIndex(m_settings.m_mixerGainIndex);
	ui->mixFilter->setCurrentIndex(m_settings.m_mixerFilterIndex);
	ui->bias->setCurrentIndex(m_settings.m_biasCurrentIndex);
	ui->mode->setCurrentIndex(m_settings.m_modeIndex);
	ui->gain1->setCurrentIndex(m_settings.m_gain1Index);
	ui->gain2->setCurrentIndex(m_settings.m_gain2Index);
	ui->gain3->setCurrentIndex(m_settings.m_gain3Index);
	ui->gain4->setCurrentIndex(m_settings.m_gain4Index);
	ui->gain5->setCurrentIndex(m_settings.m_gain5Index);
	ui->gain6->setCurrentIndex(m_settings.m_gain6Index);
	ui->rcFilter->setCurrentIndex(m_settings.m_rcFilterIndex);
	ui->ifFilter->setCurrentIndex(m_settings.m_ifFilterIndex);
}

void FCDProGui::sendSettings()
{
	if(!m_updateTimer.isActive())
		m_updateTimer.start(100);
}

void FCDProGui::on_centerFrequency_changed(quint64 value)
{
	m_settings.m_centerFrequency = value * 1000;
	sendSettings();
}

void FCDProGui::on_ppm_valueChanged(int value)
{
	m_settings.m_LOppmTenths = value;
	displaySettings();
	sendSettings();
}

void FCDProGui::on_dcOffset_toggled(bool checked)
{
	m_settings.m_dcBlock = checked;
	sendSettings();
}

void FCDProGui::on_iqImbalance_toggled(bool checked)
{
	m_settings.m_iqCorrection = checked;
	sendSettings();
}

void FCDProGui::on_lnaGain_currentIndexChanged(int index)
{
	m_settings.m_lnaGainIndex = index;
	sendSettings();
}

void FCDProGui::on_rfFilter_currentIndexChanged(int index)
{
	m_settings.m_rfFilterIndex = index;
	sendSettings();
}

void FCDProGui::on_lnaEnhance_currentIndexChanged(int index)
{
	m_settings.m_lnaEnhanceIndex = index;
	sendSettings();
}

void FCDProGui::on_band_currentIndexChanged(int index)
{
	m_settings.m_bandIndex = index;
	sendSettings();
}

void FCDProGui::on_mixGain_currentIndexChanged(int index)
{
	m_settings.m_mixerGainIndex = index;
	sendSettings();
}

void FCDProGui::on_mixFilter_currentIndexChanged(int index)
{
	m_settings.m_mixerFilterIndex = index;
	sendSettings();
}

void FCDProGui::on_bias_currentIndexChanged(int index)
{
	m_settings.m_biasCurrentIndex = index;
	sendSettings();
}

void FCDProGui::on_mode_currentIndexChanged(int index)
{
	m_settings.m_modeIndex = index;
	sendSettings();
}

void FCDProGui::on_gain1_currentIndexChanged(int index)
{
	m_settings.m_gain1Index = index;
	sendSettings();
}

void FCDProGui::on_rcFilter_currentIndexChanged(int index)
{
	m_settings.m_rcFilterIndex = index;
	sendSettings();
}

void FCDProGui::on_gain2_currentIndexChanged(int index)
{
	m_settings.m_gain2Index = index;
	sendSettings();
}

void FCDProGui::on_gain3_currentIndexChanged(int index)
{
	m_settings.m_gain3Index = index;
	sendSettings();
}

void FCDProGui::on_gain4_currentIndexChanged(int index)
{
	m_settings.m_gain4Index = index;
	sendSettings();
}

void FCDProGui::on_ifFilter_currentIndexChanged(int index)
{
	m_settings.m_ifFilterIndex = index;
	sendSettings();
}

void FCDProGui::on_gain5_currentIndexChanged(int index)
{
	m_settings.m_gain5Index = index;
	sendSettings();
}

void FCDProGui::on_gain6_currentIndexChanged(int index)
{
	m_settings.m_gain6Index = index;
	sendSettings();
}

void FCDProGui::on_setDefaults_clicked(bool checked __attribute__((unused)))
{
	m_settings.m_lnaGainIndex = 8;        // +15 dB
	//m_settings.rfFilterIndex = 0;
	m_settings.m_mixerGainIndex = 1;      // +12 dB
	m_settings.m_mixerFilterIndex = 8;    // 1.9 MHz
	m_settings.m_gain1Index = 1;          // +6 dB
	m_settings.m_rcFilterIndex = 15;      // 1.0 MHz
	m_settings.m_gain2Index = 1;          // +3 dB
	m_settings.m_gain3Index = 1;          // +3 dB
	m_settings.m_gain4Index = 0;          // 0 dB
	m_settings.m_ifFilterIndex = 31;      // 2.15 MHz
	m_settings.m_gain5Index = 0;          // +3 dB
	m_settings.m_gain6Index = 0;          // +3 dB
	m_settings.m_lnaEnhanceIndex = 0;     // Off
	m_settings.m_biasCurrentIndex = 3;    // V/U band
	m_settings.m_modeIndex = 0;           // Linearity
	displaySettings();
	sendSettings();
}

void FCDProGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        FCDProInput::MsgStartStop *message = FCDProInput::MsgStartStop::create(checked);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void FCDProGui::on_record_toggled(bool checked)
{
    if (checked) {
        ui->record->setStyleSheet("QToolButton { background-color : red; }");
    } else {
        ui->record->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
    }

    FCDProInput::MsgFileRecord* message = FCDProInput::MsgFileRecord::create(checked);
    m_sampleSource->getInputMessageQueue()->push(message);
}

void FCDProGui::on_transverter_clicked()
{
    m_settings.m_transverterMode = ui->transverter->getDeltaFrequencyAcive();
    m_settings.m_transverterDeltaFrequency = ui->transverter->getDeltaFrequency();
    qDebug("FCDProGui::on_transverter_clicked: %lld Hz %s", m_settings.m_transverterDeltaFrequency, m_settings.m_transverterMode ? "on" : "off");
    updateFrequencyLimits();
    m_settings.m_centerFrequency = ui->centerFrequency->getValueNew()*1000;
    sendSettings();
}

void FCDProGui::updateStatus()
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

void FCDProGui::updateHardware()
{
	FCDProInput::MsgConfigureFCD* message = FCDProInput::MsgConfigureFCD::create(m_settings, m_forceSettings);
	m_sampleSource->getInputMessageQueue()->push(message);
	m_forceSettings = false;
	m_updateTimer.stop();
}

