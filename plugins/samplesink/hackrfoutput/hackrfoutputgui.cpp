///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include "hackrfoutputgui.h"

#include <QDebug>
#include <QMessageBox>

#include <libhackrf/hackrf.h>

#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/devicesinkapi.h"
#include "device/devicesourceapi.h"
#include "device/deviceuiset.h"
#include "hackrf/devicehackrfvalues.h"
#include "hackrf/devicehackrfshared.h"

#include "ui_hackrfoutputgui.h"

HackRFOutputGui::HackRFOutputGui(DeviceUISet *deviceUISet, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::HackRFOutputGui),
	m_deviceUISet(deviceUISet),
	m_forceSettings(true),
	m_settings(),
	m_deviceSampleSink(0),
	m_lastEngineState((DSPDeviceSinkEngine::State)-1),
	m_doApplySettings(true)
{
    m_deviceSampleSink = (HackRFOutput*) m_deviceUISet->m_deviceSinkAPI->getSampleSink();

    ui->setupUi(this);
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
	ui->centerFrequency->setValueRange(7, 0U, 7250000U);

    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(8, 2400000U, 20000000U);

	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);

	displaySettings();
	displayBandwidths();
	sendSettings();

	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
}

HackRFOutputGui::~HackRFOutputGui()
{
	delete ui;
}

void HackRFOutputGui::destroy()
{
	delete this;
}

void HackRFOutputGui::setName(const QString& name)
{
	setObjectName(name);
}

QString HackRFOutputGui::getName() const
{
	return objectName();
}

void HackRFOutputGui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

qint64 HackRFOutputGui::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void HackRFOutputGui::setCenterFrequency(qint64 centerFrequency)
{
	m_settings.m_centerFrequency = centerFrequency;
	displaySettings();
	sendSettings();
}

QByteArray HackRFOutputGui::serialize() const
{
	return m_settings.serialize();
}

bool HackRFOutputGui::deserialize(const QByteArray& data)
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

void HackRFOutputGui::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}


bool HackRFOutputGui::handleMessage(const Message& message)
{
    if (HackRFOutput::MsgReportHackRF::match(message))
    {
        displaySettings();
        return true;
    }
    else
    {
        return false;
    }
}

void HackRFOutputGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        qDebug("HackRFOutputGui::handleInputMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("HackRFOutputGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();

            delete message;
        }
        else if (DeviceHackRFShared::MsgConfigureFrequencyDelta::match(*message))
        {
            blockApplySettings(true);
            DeviceHackRFShared::MsgConfigureFrequencyDelta* deltaMsg = (DeviceHackRFShared::MsgConfigureFrequencyDelta *) message;
            ui->centerFrequency->setValue(ui->centerFrequency->getValue() + (deltaMsg->getFrequencyDelta()/1000));
            blockApplySettings(false);

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

void HackRFOutputGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(QString("%1k").arg(QString::number(m_sampleRate/1000.0, 'g', 5)));
}

void HackRFOutputGui::displaySettings()
{
    blockApplySettings(true);
	ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);

	ui->LOppm->setValue(m_settings.m_LOppmTenths);
	ui->LOppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));

	ui->biasT->setChecked(m_settings.m_biasT);

	ui->sampleRate->setValue(m_settings.m_devSampleRate);

	ui->interp->setCurrentIndex(m_settings.m_log2Interp);

	ui->lnaExt->setChecked(m_settings.m_lnaExt);
	ui->txvgaGainText->setText(tr("%1dB").arg(m_settings.m_vgaGain));
	ui->txvga->setValue(m_settings.m_vgaGain);

    unsigned int bandwidthIndex = HackRFBandwidths::getBandwidthIndex(m_settings.m_bandwidth/1000);
	ui->bbFilter->setCurrentIndex(bandwidthIndex);
	blockApplySettings(false);
}

void HackRFOutputGui::displayBandwidths()
{
    blockApplySettings(true);
	unsigned int savedIndex = HackRFBandwidths::getBandwidthIndex(m_settings.m_bandwidth/1000);
	ui->bbFilter->blockSignals(true);
	ui->bbFilter->clear();

	for (unsigned int i = 0; i < HackRFBandwidths::m_nb_bw; i++)
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
	blockApplySettings(false);
}

void HackRFOutputGui::sendSettings()
{
	if(!m_updateTimer.isActive())
		m_updateTimer.start(100);
}

void HackRFOutputGui::on_centerFrequency_changed(quint64 value)
{
	m_settings.m_centerFrequency = value * 1000;
	sendSettings();
}

void HackRFOutputGui::on_sampleRate_changed(quint64 value)
{
    m_settings.m_devSampleRate = value;
    sendSettings();
}

void HackRFOutputGui::on_LOppm_valueChanged(int value)
{
	m_settings.m_LOppmTenths = value;
	ui->LOppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));
	sendSettings();
}

void HackRFOutputGui::on_bbFilter_currentIndexChanged(int index)
{
    int newBandwidth = HackRFBandwidths::getBandwidth(index);
	m_settings.m_bandwidth = newBandwidth * 1000;
	sendSettings();
}

void HackRFOutputGui::on_biasT_stateChanged(int state)
{
	m_settings.m_biasT = (state == Qt::Checked);
	sendSettings();
}

void HackRFOutputGui::on_lnaExt_stateChanged(int state)
{
	m_settings.m_lnaExt = (state == Qt::Checked);
	sendSettings();
}

void HackRFOutputGui::on_interp_currentIndexChanged(int index)
{
	if ((index <0) || (index > 6))
		return;
	m_settings.m_log2Interp = index;
	sendSettings();
}

void HackRFOutputGui::on_txvga_valueChanged(int value)
{
	if ((value < 0) || (value > 47))
		return;

	ui->txvgaGainText->setText(tr("%1dB").arg(value));
	m_settings.m_vgaGain = value;
	sendSettings();
}

void HackRFOutputGui::on_startStop_toggled(bool checked)
{
    if (checked)
    {
        // forcibly stop the Rx if present before starting
        if (m_deviceUISet->m_deviceSinkAPI->getSourceBuddies().size() > 0)
        {
            DeviceSourceAPI *buddy = m_deviceUISet->m_deviceSinkAPI->getSourceBuddies()[0];
            buddy->stopAcquisition();
        }

        if (m_deviceUISet->m_deviceSinkAPI->initGeneration())
        {
            m_deviceUISet->m_deviceSinkAPI->startGeneration();
            DSPEngine::instance()->startAudioInput();
        }
    }
    else
    {
        m_deviceUISet->m_deviceSinkAPI->stopGeneration();
        DSPEngine::instance()->startAudioInput();
    }
}

void HackRFOutputGui::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "HackRFOutputGui::updateHardware";
        HackRFOutput::MsgConfigureHackRF* message = HackRFOutput::MsgConfigureHackRF::create(m_settings, m_forceSettings);
        m_deviceSampleSink->getInputMessageQueue()->push(message);
        m_forceSettings = true;
        m_updateTimer.stop();
    }
}

void HackRFOutputGui::updateStatus()
{
    int state = m_deviceUISet->m_deviceSinkAPI->state();

    if(m_lastEngineState != state)
    {
        switch(state)
        {
            case DSPDeviceSinkEngine::StNotStarted:
                ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case DSPDeviceSinkEngine::StIdle:
                ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                ui->startStop->setChecked(false);
                break;
            case DSPDeviceSinkEngine::StRunning:
                ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case DSPDeviceSinkEngine::StError:
                ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::information(this, tr("Message"), m_deviceUISet->m_deviceSinkAPI->errorMessage());
                break;
            default:
                break;
        }

        m_lastEngineState = state;
    }
}
