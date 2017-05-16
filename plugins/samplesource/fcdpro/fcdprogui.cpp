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
#include <dsp/filerecord.h>
#include "fcdproconst.h"

FCDProGui::FCDProGui(DeviceSourceAPI *deviceAPI, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::FCDProGui),
	m_deviceAPI(deviceAPI),
	m_settings(),
	m_sampleSource(NULL),
	m_lastEngineState((DSPDeviceSourceEngine::State)-1)
{
    m_sampleSource = new FCDProInput(m_deviceAPI);
    m_deviceAPI->setSource(m_sampleSource);

	ui->setupUi(this);
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
	ui->centerFrequency->setValueRange(7, 64000U, 1700000U);

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

    char recFileNameCStr[30];
    sprintf(recFileNameCStr, "test_%d.sdriq", m_deviceAPI->getDeviceUID());
    m_fileSink = new FileRecord(std::string(recFileNameCStr));
    m_deviceAPI->addSink(m_fileSink);

    connect(m_deviceAPI->getDeviceOutputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleDSPMessages()), Qt::QueuedConnection);
}

FCDProGui::~FCDProGui()
{
    m_deviceAPI->removeSink(m_fileSink);
    delete m_fileSink;
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
		sendSettings();
		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}

bool FCDProGui::handleMessage(const Message& message)
{
	return false;
}

void FCDProGui::handleDSPMessages()
{
    Message* message;

    while ((message = m_deviceAPI->getDeviceOutputMessageQueue()->pop()) != 0)
    {
        qDebug("FCDProGui::handleDSPMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("FCDProGui::handleDSPMessages: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();
            m_fileSink->handleMessage(*notif); // forward to file sink

            delete message;
        }
    }
}

void FCDProGui::updateSampleRateAndFrequency()
{
    m_deviceAPI->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceAPI->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_sampleRate / 1000));
}

void FCDProGui::displaySettings()
{
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

void FCDProGui::on_setDefaults_clicked(bool checked)
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

void FCDProGui::on_record_toggled(bool checked)
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

void FCDProGui::updateStatus()
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

void FCDProGui::updateHardware()
{
	FCDProInput::MsgConfigureFCD* message = FCDProInput::MsgConfigureFCD::create(m_settings);
	m_sampleSource->getInputMessageQueue()->push(message);
	m_updateTimer.stop();
}

