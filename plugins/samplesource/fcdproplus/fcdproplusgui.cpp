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
#include <dsp/filerecord.h>
#include "fcdproplusconst.h"

FCDProPlusGui::FCDProPlusGui(DeviceSourceAPI *deviceAPI, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::FCDProPlusGui),
	m_deviceAPI(deviceAPI),
	m_settings(),
	m_sampleSource(NULL),
	m_lastEngineState((DSPDeviceSourceEngine::State)-1)
{
	ui->setupUi(this);

	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));
	ui->centerFrequency->setValueRange(7, 150U, 2000000U);

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

	m_sampleSource = new FCDProPlusInput(m_deviceAPI);
	m_deviceAPI->setSource(m_sampleSource);

	char recFileNameCStr[30];
    sprintf(recFileNameCStr, "test_%d.sdriq", m_deviceAPI->getDeviceUID());
    m_fileSink = new FileRecord(std::string(recFileNameCStr));
    m_deviceAPI->addSink(m_fileSink);

    connect(m_deviceAPI->getDeviceOutputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleDSPMessages()), Qt::QueuedConnection);
}

FCDProPlusGui::~FCDProPlusGui()
{
    m_deviceAPI->removeSink(m_fileSink);
    delete m_fileSink;
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
		sendSettings();
		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}

bool FCDProPlusGui::handleMessage(const Message& message)
{
	return false;
}

void FCDProPlusGui::handleDSPMessages()
{
    Message* message;

    while ((message = m_deviceAPI->getDeviceOutputMessageQueue()->pop()) != 0)
    {
        qDebug("RTLSDRGui::handleDSPMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("RTLSDRGui::handleDSPMessages: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();
            m_fileSink->handleMessage(*notif); // forward to file sink

            delete message;
        }
    }
}

void FCDProPlusGui::updateSampleRateAndFrequency()
{
    m_deviceAPI->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceAPI->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_sampleRate / 1000));
}

void FCDProPlusGui::displaySettings()
{
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
	FCDProPlusInput::MsgConfigureFCD* message = FCDProPlusInput::MsgConfigureFCD::create(m_settings);
	m_sampleSource->getInputMessageQueue()->push(message);
	m_updateTimer.stop();
}

void FCDProPlusGui::updateStatus()
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

void FCDProPlusGui::on_record_toggled(bool checked)
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
