///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include <libairspyhf/airspyhf.h>

#include <device/devicesourceapi.h>
#include "device/deviceuiset.h"
#include <dsp/filerecord.h>

#include "ui_airspyhffgui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "airspyhffgui.h"

AirspyHFFGui::AirspyHFFGui(DeviceUISet *deviceUISet, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::AirspyHFFGui),
	m_deviceUISet(deviceUISet),
	m_doApplySettings(true),
	m_forceSettings(true),
	m_settings(),
	m_sampleSource(0),
	m_lastEngineState((DSPDeviceSourceEngine::State)-1)
{
    m_sampleSource = (AirspyHFFInput*) m_deviceUISet->m_deviceSourceAPI->getSampleSource();

    ui->setupUi(this);
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
	updateFrequencyLimits();

	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);

	displaySettings();

	m_rates = ((AirspyHFFInput*) m_sampleSource)->getSampleRates();
	displaySampleRates();
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);

    sendSettings();
}

AirspyHFFGui::~AirspyHFFGui()
{
	delete ui;
}

void AirspyHFFGui::destroy()
{
	delete this;
}

void AirspyHFFGui::setName(const QString& name)
{
	setObjectName(name);
}

QString AirspyHFFGui::getName() const
{
	return objectName();
}

void AirspyHFFGui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

qint64 AirspyHFFGui::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void AirspyHFFGui::setCenterFrequency(qint64 centerFrequency)
{
	m_settings.m_centerFrequency = centerFrequency;
	displaySettings();
	sendSettings();
}

QByteArray AirspyHFFGui::serialize() const
{
	return m_settings.serialize();
}

bool AirspyHFFGui::deserialize(const QByteArray& data)
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

bool AirspyHFFGui::handleMessage(const Message& message)
{
    if (AirspyHFFInput::MsgConfigureAirspyHF::match(message))
    {
        const AirspyHFFInput::MsgConfigureAirspyHF& cfg = (AirspyHFFInput::MsgConfigureAirspyHF&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (AirspyHFFInput::MsgStartStop::match(message))
    {
        AirspyHFFInput::MsgStartStop& notif = (AirspyHFFInput::MsgStartStop&) message;
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

void AirspyHFFGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        qDebug("AirspyHFGui::handleInputMessages: message: %s", message->getIdentifier());

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

void AirspyHFFGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_sampleRate / 1000));
}

void AirspyHFFGui::updateFrequencyLimits()
{
    // values in kHz
    qint64 deltaFrequency = m_settings.m_transverterMode ? m_settings.m_transverterDeltaFrequency/1000 : 0;

    qint64 minLimit;
    qint64 maxLimit;

    switch(m_settings.m_bandIndex)
    {
    case 1:
        minLimit = AirspyHFFInput::loLowLimitFreqVHF/1000 + deltaFrequency;
        maxLimit = AirspyHFFInput::loHighLimitFreqVHF/1000 + deltaFrequency;
        break;
    case 0:
    default:
        minLimit = AirspyHFFInput::loLowLimitFreqHF/1000 + deltaFrequency;
        maxLimit = AirspyHFFInput::loHighLimitFreqHF/1000 + deltaFrequency;
        break;
    }

    minLimit = minLimit < 0 ? 0 : minLimit > 9999999 ? 9999999 : minLimit;
    maxLimit = maxLimit < 0 ? 0 : maxLimit > 9999999 ? 9999999 : maxLimit;

    qDebug("AirspyHFGui::updateFrequencyLimits: delta: %lld min: %lld max: %lld", deltaFrequency, minLimit, maxLimit);

    ui->centerFrequency->setValueRange(7, minLimit, maxLimit);
}

void AirspyHFFGui::displaySettings()
{
    blockApplySettings(true);
    ui->band->blockSignals(true);
    m_settings.m_bandIndex = m_settings.m_centerFrequency <= 31000000UL ? 0 : 1; // override
    ui->band->setCurrentIndex(m_settings.m_bandIndex);
    updateFrequencyLimits();
    ui->transverter->setDeltaFrequency(m_settings.m_transverterDeltaFrequency);
    ui->transverter->setDeltaFrequencyActive(m_settings.m_transverterMode);
	ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
	ui->LOppm->setValue(m_settings.m_LOppmTenths);
	ui->LOppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));
	ui->sampleRate->setCurrentIndex(m_settings.m_devSampleRateIndex);
	ui->decim->setCurrentIndex(m_settings.m_log2Decim);
    ui->band->blockSignals(false);
    blockApplySettings(false);
}

void AirspyHFFGui::displaySampleRates()
{
	unsigned int savedIndex = m_settings.m_devSampleRateIndex;
	ui->sampleRate->blockSignals(true);

	if (m_rates.size() > 0)
	{
		ui->sampleRate->clear();

		for (unsigned int i = 0; i < m_rates.size(); i++)
		{
		    int sampleRate = m_rates[i]/1000;
			ui->sampleRate->addItem(QString("%1").arg(QString("%1").arg(sampleRate, 5, 10, QChar(' '))));
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

void AirspyHFFGui::sendSettings()
{
	if(!m_updateTimer.isActive())
		m_updateTimer.start(100);
}

void AirspyHFFGui::on_centerFrequency_changed(quint64 value)
{
	m_settings.m_centerFrequency = value * 1000;
	sendSettings();
}

void AirspyHFFGui::on_LOppm_valueChanged(int value)
{
    m_settings.m_LOppmTenths = value;
    ui->LOppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));
    sendSettings();
}

void AirspyHFFGui::on_resetLOppm_clicked()
{
    ui->LOppm->setValue(0);
}

void AirspyHFFGui::on_sampleRate_currentIndexChanged(int index)
{
	m_settings.m_devSampleRateIndex = index;
	sendSettings();
}

void AirspyHFFGui::on_decim_currentIndexChanged(int index)
{
	if ((index < 0) || (index > 5))
		return;
	m_settings.m_log2Decim = index;
	sendSettings();
}

void AirspyHFFGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        AirspyHFFInput::MsgStartStop *message = AirspyHFFInput::MsgStartStop::create(checked);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void AirspyHFFGui::on_record_toggled(bool checked)
{
    if (checked) {
        ui->record->setStyleSheet("QToolButton { background-color : red; }");
    } else {
        ui->record->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
    }

    AirspyHFFInput::MsgFileRecord* message = AirspyHFFInput::MsgFileRecord::create(checked);
    m_sampleSource->getInputMessageQueue()->push(message);
}

void AirspyHFFGui::on_transverter_clicked()
{
    m_settings.m_transverterMode = ui->transverter->getDeltaFrequencyAcive();
    m_settings.m_transverterDeltaFrequency = ui->transverter->getDeltaFrequency();
    qDebug("AirspyHFGui::on_transverter_clicked: %lld Hz %s", m_settings.m_transverterDeltaFrequency, m_settings.m_transverterMode ? "on" : "off");
    updateFrequencyLimits();
    m_settings.m_centerFrequency = ui->centerFrequency->getValueNew()*1000;
    sendSettings();
}

void AirspyHFFGui::on_band_currentIndexChanged(int index)
{
    if ((index < 0) || (index > 1)) {
        return;
    }

    m_settings.m_bandIndex = index;
    updateFrequencyLimits();
    qDebug("AirspyHFGui::on_band_currentIndexChanged: freq: %llu", ui->centerFrequency->getValueNew() * 1000);
    m_settings.m_centerFrequency = ui->centerFrequency->getValueNew() * 1000;
    sendSettings();
}

void AirspyHFFGui::updateHardware()
{
	qDebug() << "AirspyHFGui::updateHardware";
	AirspyHFFInput::MsgConfigureAirspyHF* message = AirspyHFFInput::MsgConfigureAirspyHF::create(m_settings, m_forceSettings);
	m_sampleSource->getInputMessageQueue()->push(message);
	m_forceSettings = false;
	m_updateTimer.stop();
}

void AirspyHFFGui::updateStatus()
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

uint32_t AirspyHFFGui::getDevSampleRate(unsigned int rate_index)
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

int AirspyHFFGui::getDevSampleRateIndex(uint32_t sampeRate)
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
