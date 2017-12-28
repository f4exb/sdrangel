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

#include "ui_fcdproplusgui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "fcdproplusgui.h"

#include <device/devicesourceapi.h>
#include "device/deviceuiset.h"
#include "fcdproplusconst.h"
#include "fcdtraits.h"

FCDProPlusGui::FCDProPlusGui(DeviceUISet *deviceUISet, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::FCDProPlusGui),
	m_deviceUISet(deviceUISet),
	m_forceSettings(true),
	m_settings(),
	m_sampleSource(NULL),
	m_lastEngineState((DSPDeviceSourceEngine::State)-1)
{
    m_sampleSource = (FCDProPlusInput*) m_deviceUISet->m_deviceSourceAPI->getSampleSource();

	ui->setupUi(this);

	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
	updateFrequencyLimits();

	ui->filterIF->clear();
	for (int i = 0; i < FCDProPlusConstants::fcdproplus_if_filter_nb_values(); i++)
	{
		ui->filterIF->addItem(QString(FCDProPlusConstants::if_filters[i].label.c_str()), i);
	}

	ui->filterRF->clear();
	for (int i = 0; i < FCDProPlusConstants::fcdproplus_rf_filter_nb_values(); i++)
	{
		ui->filterRF->addItem(QString(FCDProPlusConstants::rf_filters[i].label.c_str()), i);
	}

	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);

	displaySettings();

	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
}

FCDProPlusGui::~FCDProPlusGui()
{
	delete ui;
}

void FCDProPlusGui::destroy()
{
	delete this;
}

void FCDProPlusGui::setName(const QString& name)
{
	setObjectName(name);
}

QString FCDProPlusGui::getName() const
{
	return objectName();
}

void FCDProPlusGui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

qint64 FCDProPlusGui::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void FCDProPlusGui::setCenterFrequency(qint64 centerFrequency)
{
	m_settings.m_centerFrequency = centerFrequency;
	displaySettings();
	sendSettings();
}

QByteArray FCDProPlusGui::serialize() const
{
	return m_settings.serialize();
}

bool FCDProPlusGui::deserialize(const QByteArray& data)
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

bool FCDProPlusGui::handleMessage(const Message& message __attribute__((unused)))
{
    if (FCDProPlusInput::MsgConfigureFCD::match(message))
    {
        const FCDProPlusInput::MsgConfigureFCD& cfg = (FCDProPlusInput::MsgConfigureFCD&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (FCDProPlusInput::MsgStartStop::match(message))
    {
        FCDProPlusInput::MsgStartStop& notif = (FCDProPlusInput::MsgStartStop&) message;
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

void FCDProPlusGui::handleInputMessages()
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
    }
}

void FCDProPlusGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_sampleRate / 1000));
}

void FCDProPlusGui::updateFrequencyLimits()
{
    // values in kHz
    qint64 deltaFrequency = m_settings.m_transverterMode ? m_settings.m_transverterDeltaFrequency/1000 : 0;
    qint64 minLimit = fcd_traits<ProPlus>::loLowLimitFreq/1000 + deltaFrequency;
    qint64 maxLimit = fcd_traits<ProPlus>::loHighLimitFreq/1000 + deltaFrequency;

    minLimit = minLimit < 0 ? 0 : minLimit > 9999999 ? 9999999 : minLimit;
    maxLimit = maxLimit < 0 ? 0 : maxLimit > 9999999 ? 9999999 : maxLimit;

    qDebug("FCDProPlusGui::updateFrequencyLimits: delta: %lld min: %lld max: %lld", deltaFrequency, minLimit, maxLimit);

    ui->centerFrequency->setValueRange(7, minLimit, maxLimit);
}

void FCDProPlusGui::displaySettings()
{
    ui->transverter->setDeltaFrequency(m_settings.m_transverterDeltaFrequency);
    ui->transverter->setDeltaFrequencyActive(m_settings.m_transverterMode);
    updateFrequencyLimits();
	ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
	ui->dcOffset->setChecked(m_settings.m_dcBlock);
	ui->iqImbalance->setChecked(m_settings.m_iqImbalance);
	ui->checkBoxG->setChecked(m_settings.m_lnaGain);
	ui->checkBoxB->setChecked(m_settings.m_biasT);
	ui->mixGain->setChecked(m_settings.m_mixGain);
	ui->ifGain->setValue(m_settings.m_ifGain);
	ui->ifGainText->setText(QString("%1dB").arg(m_settings.m_ifGain));
	ui->filterIF->setCurrentIndex(m_settings.m_ifFilterIndex);
	ui->filterRF->setCurrentIndex(m_settings.m_rfFilterIndex);
	ui->ppm->setValue(m_settings.m_LOppmTenths);
	ui->ppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));
}

void FCDProPlusGui::sendSettings()
{
	if(!m_updateTimer.isActive())
		m_updateTimer.start(100);
}

void FCDProPlusGui::on_centerFrequency_changed(quint64 value)
{
	m_settings.m_centerFrequency = value * 1000;
	sendSettings();
}

void FCDProPlusGui::on_dcOffset_toggled(bool checked)
{
	m_settings.m_dcBlock = checked;
	sendSettings();
}

void FCDProPlusGui::on_iqImbalance_toggled(bool checked)
{
	m_settings.m_iqImbalance = checked;
	sendSettings();
}

void FCDProPlusGui::updateHardware()
{
	FCDProPlusInput::MsgConfigureFCD* message = FCDProPlusInput::MsgConfigureFCD::create(m_settings, m_forceSettings);
	m_sampleSource->getInputMessageQueue()->push(message);
	m_forceSettings = false;
	m_updateTimer.stop();
}

void FCDProPlusGui::updateStatus()
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

void FCDProPlusGui::on_checkBoxG_stateChanged(int state)
{
	m_settings.m_lnaGain = (state == Qt::Checked);
	sendSettings();
}

void FCDProPlusGui::on_checkBoxB_stateChanged(int state)
{
	m_settings.m_biasT = (state == Qt::Checked);
	sendSettings();
}

void FCDProPlusGui::on_mixGain_stateChanged(int state)
{
	m_settings.m_mixGain = (state == Qt::Checked);
	sendSettings();
}

void FCDProPlusGui::on_filterIF_currentIndexChanged(int index)
{
	m_settings.m_ifFilterIndex = index;
	sendSettings();
}

void FCDProPlusGui::on_filterRF_currentIndexChanged(int index)
{
	m_settings.m_rfFilterIndex = index;
	sendSettings();
}

void FCDProPlusGui::on_ifGain_valueChanged(int value)
{
	m_settings.m_ifGain = value;
	displaySettings();
	sendSettings();
}

void FCDProPlusGui::on_ppm_valueChanged(int value)
{
	m_settings.m_LOppmTenths = value;
	displaySettings();
	sendSettings();
}

void FCDProPlusGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        FCDProPlusInput::MsgStartStop *message = FCDProPlusInput::MsgStartStop::create(checked);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void FCDProPlusGui::on_record_toggled(bool checked)
{
    if (checked) {
        ui->record->setStyleSheet("QToolButton { background-color : red; }");
    } else {
        ui->record->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
    }

    FCDProPlusInput::MsgFileRecord* message = FCDProPlusInput::MsgFileRecord::create(checked);
    m_sampleSource->getInputMessageQueue()->push(message);
}

void FCDProPlusGui::on_transverter_clicked()
{
    m_settings.m_transverterMode = ui->transverter->getDeltaFrequencyAcive();
    m_settings.m_transverterDeltaFrequency = ui->transverter->getDeltaFrequency();
    qDebug("FCDProPlusGui::on_transverter_clicked: %lld Hz %s", m_settings.m_transverterDeltaFrequency, m_settings.m_transverterMode ? "on" : "off");
    updateFrequencyLimits();
    m_settings.m_centerFrequency = ui->centerFrequency->getValueNew()*1000;
    sendSettings();
}
