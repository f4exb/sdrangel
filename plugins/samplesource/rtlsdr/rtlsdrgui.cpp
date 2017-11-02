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

#include <device/devicesourceapi.h>
#include "device/deviceuiset.h"
#include <dsp/filerecord.h>

#include "ui_rtlsdrgui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"


RTLSDRGui::RTLSDRGui(DeviceUISet *deviceUISet, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::RTLSDRGui),
	m_deviceUISet(deviceUISet),
	m_forceSettings(true),
	m_settings(),
	m_sampleSource(0),
	m_lastEngineState((DSPDeviceSourceEngine::State)-1)
{
    m_sampleSource = (RTLSDRInput*) m_deviceUISet->m_deviceSourceAPI->getSampleSource();

    ui->setupUi(this);
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
	updateFrequencyLimits();

    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(7, RTLSDRInput::sampleRateHighRangeMin, RTLSDRInput::sampleRateHighRangeMax);

	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);

	displaySettings();

	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
}

RTLSDRGui::~RTLSDRGui()
{
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
    if(m_settings.deserialize(data))
    {
        displayGains();
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

bool RTLSDRGui::handleMessage(const Message& message)
{
	if (RTLSDRInput::MsgReportRTLSDR::match(message))
	{
		m_gains = ((RTLSDRInput::MsgReportRTLSDR&) message).getGains();
		displayGains();
		return true;
	}
	else
	{
		return false;
	}
}

void RTLSDRGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        qDebug("RTLSDRGui::handleInputMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("RTLSDRGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
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

void RTLSDRGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg(QString::number(m_sampleRate / 1000.0f, 'g', 5)));
}

void RTLSDRGui::updateFrequencyLimits()
{
    // values in kHz
    qint64 deltaFrequency = m_settings.m_transverterMode ? m_settings.m_transverterDeltaFrequency/1000 : 0;
    qint64 minLimit = (m_settings.m_noModMode ? RTLSDRInput::frequencyLowRangeMin : RTLSDRInput::frequencyHighRangeMin) + deltaFrequency;
    qint64 maxLimit = (m_settings.m_noModMode ? RTLSDRInput::frequencyLowRangeMax : RTLSDRInput::frequencyHighRangeMax) + deltaFrequency;

    minLimit = minLimit < 0 ? 0 : minLimit > 9999999 ? 9999999 : minLimit;
    maxLimit = maxLimit < 0 ? 0 : maxLimit > 9999999 ? 9999999 : maxLimit;

    qDebug("RTLSDRGui::updateFrequencyLimits: delta: %lld min: %lld max: %lld", deltaFrequency, minLimit, maxLimit);

    ui->centerFrequency->setValueRange(7, minLimit, maxLimit);
}

void RTLSDRGui::displayGains()
{
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

void RTLSDRGui::displaySettings()
{
    ui->transverter->setDeltaFrequency(m_settings.m_transverterDeltaFrequency);
    ui->transverter->setDeltaFrequencyActive(m_settings.m_transverterMode);
    updateFrequencyLimits();
	ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
	ui->sampleRate->setValue(m_settings.m_devSampleRate);
	ui->dcOffset->setChecked(m_settings.m_dcBlock);
	ui->iqImbalance->setChecked(m_settings.m_iqImbalance);
	ui->ppm->setValue(m_settings.m_loPpmCorrection);
	ui->ppmText->setText(tr("%1").arg(m_settings.m_loPpmCorrection));
	ui->decim->setCurrentIndex(m_settings.m_log2Decim);
	ui->fcPos->setCurrentIndex((int) m_settings.m_fcPos);
	ui->checkBox->setChecked(m_settings.m_noModMode);
	ui->agc->setChecked(m_settings.m_agc);
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
	if ((index <0) || (index > 5))
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

void RTLSDRGui::on_startStop_toggled(bool checked)
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

void RTLSDRGui::on_record_toggled(bool checked)
{
    if (checked) {
        ui->record->setStyleSheet("QToolButton { background-color : red; }");
    } else {
        ui->record->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
    }

    RTLSDRInput::MsgFileRecord* message = RTLSDRInput::MsgFileRecord::create(checked);
    m_sampleSource->getInputMessageQueue()->push(message);
}

void RTLSDRGui::on_transverter_clicked()
{
    m_settings.m_transverterMode = ui->transverter->getDeltaFrequencyAcive();
    m_settings.m_transverterDeltaFrequency = ui->transverter->getDeltaFrequency();
    qDebug("RTLSDRGui::on_transverter_clicked: %lld Hz %s", m_settings.m_transverterDeltaFrequency, m_settings.m_transverterMode ? "on" : "off");
    updateFrequencyLimits();
    m_settings.m_centerFrequency = ui->centerFrequency->getValueNew()*1000;
    sendSettings();
}

void RTLSDRGui::updateHardware()
{
	RTLSDRInput::MsgConfigureRTLSDR* message = RTLSDRInput::MsgConfigureRTLSDR::create(m_settings, m_forceSettings);
	m_sampleSource->getInputMessageQueue()->push(message);
	m_forceSettings = false;
	m_updateTimer.stop();
}

void RTLSDRGui::updateStatus()
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

void RTLSDRGui::on_checkBox_stateChanged(int state)
{
	if (state == Qt::Checked)
	{
		ui->gain->setEnabled(false);
        m_settings.m_noModMode = true;
	    updateFrequencyLimits();
		ui->centerFrequency->setValue(7000);
		m_settings.m_centerFrequency = 7000 * 1000;
	}
	else
	{
		ui->gain->setEnabled(true);
        m_settings.m_noModMode = false;
	    updateFrequencyLimits();
		ui->centerFrequency->setValue(434000);
		ui->gain->setValue(0);
		m_settings.m_centerFrequency = 435000 * 1000;
	}

	sendSettings();
}

void RTLSDRGui::on_agc_stateChanged(int state)
{
    m_settings.m_agc = (state == Qt::Checked);
    sendSettings();
}

void RTLSDRGui::on_sampleRate_changed(quint64 value)
{
    m_settings.m_devSampleRate = value;
    sendSettings();
}

void RTLSDRGui::on_lowSampleRate_toggled(bool checked)
{
    if (checked) {
        ui->sampleRate->setValueRange(7, RTLSDRInput::sampleRateLowRangeMin, RTLSDRInput::sampleRateLowRangeMax);
    } else {
        ui->sampleRate->setValueRange(7, RTLSDRInput::sampleRateHighRangeMin, RTLSDRInput::sampleRateHighRangeMax);
    }

    m_settings.m_devSampleRate = ui->sampleRate->getValueNew();
    qDebug("RTLSDRGui::on_lowSampleRate_toggled: %d S/s", m_settings.m_devSampleRate);
    sendSettings();
}
