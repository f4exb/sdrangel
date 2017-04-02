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

#include "../hackrfinput/hackrfinputgui.h"

#include <QDebug>
#include <QMessageBox>

#include <libhackrf/hackrf.h>

#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include <device/devicesourceapi.h>
#include <dsp/filerecord.h>
#include "hackrf/devicehackrfvalues.h"

#include "ui_hackrfinputgui.h"

HackRFInputGui::HackRFInputGui(DeviceSourceAPI *deviceAPI, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::HackRFInputGui),
	m_deviceAPI(deviceAPI),
	m_settings(),
	m_sampleSource(NULL),
	m_lastEngineState((DSPDeviceSourceEngine::State)-1)
{
	ui->setupUi(this);
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));
	ui->centerFrequency->setValueRange(7, 0U, 7250000U);

    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::ReverseGreenYellow));
    ui->sampleRate->setValueRange(8, 2400000U, 20000000U);

	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);

	displaySettings();

	m_sampleSource = new HackRFInput(m_deviceAPI);

	displayBandwidths();

	m_deviceAPI->setSource(m_sampleSource);

    char recFileNameCStr[30];
    sprintf(recFileNameCStr, "test_%d.sdriq", m_deviceAPI->getDeviceUID());
    m_fileSink = new FileRecord(std::string(recFileNameCStr));
    m_deviceAPI->addSink(m_fileSink);

    connect(m_deviceAPI->getDeviceOutputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleDSPMessages()), Qt::QueuedConnection);
}

HackRFInputGui::~HackRFInputGui()
{
    m_deviceAPI->removeSink(m_fileSink);
    delete m_fileSink;
	delete m_sampleSource; // Valgrind memcheck
	delete ui;
}

void HackRFInputGui::destroy()
{
	delete this;
}

void HackRFInputGui::setName(const QString& name)
{
	setObjectName(name);
}

QString HackRFInputGui::getName() const
{
	return objectName();
}

void HackRFInputGui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

qint64 HackRFInputGui::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void HackRFInputGui::setCenterFrequency(qint64 centerFrequency)
{
	m_settings.m_centerFrequency = centerFrequency;
	displaySettings();
	sendSettings();
}

QByteArray HackRFInputGui::serialize() const
{
	return m_settings.serialize();
}

bool HackRFInputGui::deserialize(const QByteArray& data)
{
	if(m_settings.deserialize(data))
	{
		displaySettings();
		sendSettings();
		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}

bool HackRFInputGui::handleMessage(const Message& message)
{
    if (HackRFInput::MsgReportHackRF::match(message))
    {
        displaySettings();
        return true;
    }
    else
    {
        return false;
    }
}

void HackRFInputGui::handleDSPMessages()
{
    Message* message;

    while ((message = m_deviceAPI->getDeviceOutputMessageQueue()->pop()) != 0)
    {
        qDebug("HackRFGui::handleDSPMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("HackRFGui::handleDSPMessages: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();
            m_fileSink->handleMessage(*notif); // forward to file sink

            delete message;
        }
    }
}

void HackRFInputGui::updateSampleRateAndFrequency()
{
    m_deviceAPI->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceAPI->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(QString("%1k").arg(QString::number(m_sampleRate/1000.0, 'g', 5)));
}

void HackRFInputGui::displaySettings()
{
	ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);

	ui->LOppm->setValue(m_settings.m_LOppmTenths);
	ui->LOppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));

	ui->dcOffset->setChecked(m_settings.m_dcBlock);
	ui->iqImbalance->setChecked(m_settings.m_iqCorrection);

    ui->sampleRate->setValue(m_settings.m_devSampleRate);

	ui->biasT->setChecked(m_settings.m_biasT);

	ui->decim->setCurrentIndex(m_settings.m_log2Decim);

	ui->fcPos->setCurrentIndex((int) m_settings.m_fcPos);

	ui->lnaExt->setChecked(m_settings.m_lnaExt);
	ui->lnaGainText->setText(tr("%1dB").arg(m_settings.m_lnaGain));
	ui->lna->setValue(m_settings.m_lnaGain);

    unsigned int bandwidthIndex = HackRFBandwidths::getBandwidthIndex(m_settings.m_bandwidth/1000);
	ui->bbFilter->setCurrentIndex(bandwidthIndex);

	ui->vgaText->setText(tr("%1dB").arg(m_settings.m_vgaGain));
	ui->vga->setValue(m_settings.m_vgaGain);
}

void HackRFInputGui::displayBandwidths()
{
	int savedIndex = HackRFBandwidths::getBandwidthIndex(m_settings.m_bandwidth/1000);
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

void HackRFInputGui::sendSettings()
{
	if(!m_updateTimer.isActive())
		m_updateTimer.start(100);
}

void HackRFInputGui::on_LOppm_valueChanged(int value)
{
	m_settings.m_LOppmTenths = value;
	ui->LOppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));
	sendSettings();
}

void HackRFInputGui::on_dcOffset_toggled(bool checked)
{
	m_settings.m_dcBlock = checked;
	sendSettings();
}

void HackRFInputGui::on_iqImbalance_toggled(bool checked)
{
	m_settings.m_iqCorrection = checked;
	sendSettings();
}

void HackRFInputGui::on_bbFilter_currentIndexChanged(int index)
{
    int newBandwidth = HackRFBandwidths::getBandwidth(index);
	m_settings.m_bandwidth = newBandwidth * 1000;
	sendSettings();
}

void HackRFInputGui::on_biasT_stateChanged(int state)
{
	m_settings.m_biasT = (state == Qt::Checked);
	sendSettings();
}

void HackRFInputGui::on_lnaExt_stateChanged(int state)
{
	m_settings.m_lnaExt = (state == Qt::Checked);
	sendSettings();
}

void HackRFInputGui::on_centerFrequency_changed(quint64 value)
{
	m_settings.m_centerFrequency = value * 1000;
	sendSettings();
}

void HackRFInputGui::on_sampleRate_changed(quint64 value)
{
    m_settings.m_devSampleRate = value;
    sendSettings();
}

void HackRFInputGui::on_decim_currentIndexChanged(int index)
{
	if ((index <0) || (index > 6))
		return;
	m_settings.m_log2Decim = index;
	sendSettings();
}

void HackRFInputGui::on_fcPos_currentIndexChanged(int index)
{
	if (index == 0) {
		m_settings.m_fcPos = HackRFInputSettings::FC_POS_INFRA;
		sendSettings();
	} else if (index == 1) {
		m_settings.m_fcPos = HackRFInputSettings::FC_POS_SUPRA;
		sendSettings();
	} else if (index == 2) {
		m_settings.m_fcPos = HackRFInputSettings::FC_POS_CENTER;
		sendSettings();
	}
}

void HackRFInputGui::on_lna_valueChanged(int value)
{
	if ((value < 0) || (value > 40))
		return;

	ui->lnaGainText->setText(tr("%1dB").arg(value));
	m_settings.m_lnaGain = value;
	sendSettings();
}

void HackRFInputGui::on_vga_valueChanged(int value)
{
	if ((value < 0) || (value > 62))
		return;

	ui->vgaText->setText(tr("%1dB").arg(value));
	m_settings.m_vgaGain = value;
	sendSettings();
}

void HackRFInputGui::on_startStop_toggled(bool checked)
{
    if (checked)
    {
        if (m_deviceAPI->initAcquisition())
        {
            m_deviceAPI->startAcquisition();
            DSPEngine::instance()->startAudioOutput();
        }
    }
    else
    {
        m_deviceAPI->stopAcquisition();
        DSPEngine::instance()->stopAudioOutput();
    }
}

void HackRFInputGui::on_record_toggled(bool checked)
{
    if (checked)
    {
        ui->record->setStyleSheet("QToolButton { background-color : red; }");
        m_fileSink->startRecording();
    }
    else
    {
        ui->record->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        m_fileSink->stopRecording();
    }
}

void HackRFInputGui::updateHardware()
{
	qDebug() << "HackRFGui::updateHardware";
	HackRFInput::MsgConfigureHackRF* message = HackRFInput::MsgConfigureHackRF::create(m_settings);
	m_sampleSource->getInputMessageQueue()->push(message);
	m_updateTimer.stop();
}

void HackRFInputGui::updateStatus()
{
    int state = m_deviceAPI->state();

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
                QMessageBox::information(this, tr("Message"), m_deviceAPI->errorMessage());
                break;
            default:
                break;
        }

        m_lastEngineState = state;
    }
}
