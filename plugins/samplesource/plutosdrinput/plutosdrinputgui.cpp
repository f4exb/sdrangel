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

#include <stdio.h>
#include <QDebug>
#include <QMessageBox>

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "gui/glspectrum.h"
#include "device/devicesourceapi.h"
#include "plutosdr/deviceplutosdr.h"
#include "plutosdrinput.h"
#include "ui_plutosdrinputgui.h"
#include "plutosdrinputgui.h"

PlutoSDRInputGui::PlutoSDRInputGui(DeviceSourceAPI *deviceAPI, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::PlutoSDRInputGUI),
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_forceSettings(true),
    m_sampleSource(NULL),
    m_sampleRate(0),
    m_deviceCenterFrequency(0),
    m_lastEngineState((DSPDeviceSourceEngine::State)-1),
    m_doApplySettings(true),
    m_statusCounter(0)
{
    m_sampleSource = new PlutoSDRInput(m_deviceAPI);
    m_deviceAPI->setSource(m_sampleSource);

    ui->setupUi(this);
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->centerFrequency->setValueRange(7, DevicePlutoSDR::loLowLimitFreq/1000, DevicePlutoSDR::loHighLimitFreq/1000);

    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(8, DevicePlutoSDR::srLowLimitFreq, DevicePlutoSDR::srHighLimitFreq);

    ui->lpf->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->lpf->setValueRange(5, DevicePlutoSDR::bbLPRxLowLimitFreq/1000, DevicePlutoSDR::bbLPRxHighLimitFreq/1000);

    ui->lpFIR->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->lpFIR->setValueRange(5, 1U, 56000U); // will be dynamically recalculated

    ui->swDecimLabel->setText(QString::fromUtf8("S\u2193"));

    blockApplySettings(true);
    displaySettings();
    blockApplySettings(false);

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    connect(m_deviceAPI->getDeviceOutputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleDSPMessages()), Qt::QueuedConnection);
}

PlutoSDRInputGui::~PlutoSDRInputGui()
{
    delete ui;
}

void PlutoSDRInputGui::destroy()
{
    delete this;
}

void PlutoSDRInputGui::setName(const QString& name)
{
    setObjectName(name);
}

QString PlutoSDRInputGui::getName() const
{
    return objectName();
}

void PlutoSDRInputGui::resetToDefaults()
{

}

qint64 PlutoSDRInputGui::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void PlutoSDRInputGui::setCenterFrequency(qint64 centerFrequency)
{
    m_settings.m_centerFrequency = centerFrequency;
}

QByteArray PlutoSDRInputGui::serialize() const
{
    return m_settings.serialize();
}

bool PlutoSDRInputGui::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
    {
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        sendSettings(true);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool PlutoSDRInputGui::handleMessage(const Message& message __attribute__((unused)))
{
    return false;
}

void PlutoSDRInputGui::on_startStop_toggled(bool checked)
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

void PlutoSDRInputGui::on_record_toggled(bool checked)
{
    if (checked) {
        ui->record->setStyleSheet("QToolButton { background-color : red; }");
    } else {
        ui->record->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
    }

    PlutoSDRInput::MsgFileRecord* message = PlutoSDRInput::MsgFileRecord::create(checked);
    m_sampleSource->getInputMessageQueue()->push(message);
}

void PlutoSDRInputGui::on_centerFrequency_changed(quint64 value)
{
    m_settings.m_centerFrequency = value * 1000;
    sendSettings();
}

void PlutoSDRInputGui::on_loPPM_valueChanged(int value)
{
    ui->loPPMText->setText(QString("%1").arg(QString::number(value/10.0, 'f', 1)));
    m_settings.m_LOppmTenths = value;
    sendSettings();
}

void PlutoSDRInputGui::on_dcOffset_toggled(bool checked)
{
    m_settings.m_dcBlock = checked;
    sendSettings();
}

void PlutoSDRInputGui::on_iqImbalance_toggled(bool checked)
{
    m_settings.m_iqCorrection = checked;
    sendSettings();
}

void PlutoSDRInputGui::on_swDecim_currentIndexChanged(int index)
{
    m_settings.m_log2Decim = index > 6 ? 6 : index;
    sendSettings();
}

void PlutoSDRInputGui::on_fcPos_currentIndexChanged(int index)
{
    m_settings.m_fcPos = (PlutoSDRInputSettings::fcPos_t) (index > 2 ? 2 : index);
    sendSettings();
}

void PlutoSDRInputGui::on_sampleRate_changed(quint64 value)
{
    m_settings.m_devSampleRate = value;
    sendSettings();
}

void PlutoSDRInputGui::on_lpf_changed(quint64 value)
{
    m_settings.m_lpfBW = value * 1000;
    sendSettings();
}

void PlutoSDRInputGui::on_lpFIREnable_toggled(bool checked)
{
    m_settings.m_lpfFIREnable = checked;
    sendSettings();
}

void PlutoSDRInputGui::on_lpFIR_changed(quint64 value)
{
    m_settings.m_lpfFIRBW = value * 1000;
    sendSettings();
}

void PlutoSDRInputGui::on_lpFIRDecimation_currentIndexChanged(int index)
{
    m_settings.m_lpfFIRlog2Decim = index > 2 ? 2 : index;
    sendSettings();
}

void PlutoSDRInputGui::on_lpFIRGain_currentIndexChanged(int index)
{
    m_settings.m_lpfFIRGain = 6*(index > 1 ? 1 : index) - 6;
    sendSettings();
}

void PlutoSDRInputGui::on_gainMode_currentIndexChanged(int index)
{
    m_settings.m_gainMode = (PlutoSDRInputSettings::GainMode) (index < PlutoSDRInputSettings::GAIN_END ? index : 0);
    sendSettings();
}

void PlutoSDRInputGui::on_gain_valueChanged(int value)
{
    ui->gainText->setText(tr("%1").arg(value));
    m_settings.m_gain = value;
    sendSettings();
}

void PlutoSDRInputGui::on_antenna_currentIndexChanged(int index)
{
    m_settings.m_antennaPath = (PlutoSDRInputSettings::RFPath) (index < PlutoSDRInputSettings::RFPATH_END ? index : 0);
    sendSettings();
}

void PlutoSDRInputGui::displaySettings()
{
    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
    ui->sampleRate->setValue(m_settings.m_devSampleRate);

    ui->dcOffset->setChecked(m_settings.m_dcBlock);
    ui->iqImbalance->setChecked(m_settings.m_iqCorrection);
    ui->loPPMText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));

    ui->swDecim->setCurrentIndex(m_settings.m_log2Decim);

    ui->lpf->setValue(m_settings.m_lpfBW / 1000);

    ui->lpFIREnable->setChecked(m_settings.m_lpfFIREnable);
    ui->lpFIR->setValue(m_settings.m_lpfFIRBW / 1000);
    ui->lpFIRDecimation->setCurrentIndex(m_settings.m_lpfFIRlog2Decim);
    ui->lpFIRGain->setCurrentIndex((m_settings.m_lpfFIRGain + 6)/6);

    ui->gainMode->setCurrentIndex((int) m_settings.m_gainMode);
    ui->gain->setValue(m_settings.m_gain);
    ui->gainText->setText(tr("%1").arg(m_settings.m_gain));

    ui->antenna->setCurrentIndex((int) m_settings.m_antennaPath);
}

void PlutoSDRInputGui::sendSettings(bool forceSettings)
{
    m_forceSettings = forceSettings;
    if(!m_updateTimer.isActive()) { m_updateTimer.start(100); }
}

void PlutoSDRInputGui::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "PlutoSDRInputGui::updateHardware";
        PlutoSDRInput::MsgConfigurePlutoSDR* message = PlutoSDRInput::MsgConfigurePlutoSDR::create(m_settings, m_forceSettings);
        m_sampleSource->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_updateTimer.stop();
    }
}

void PlutoSDRInputGui::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void PlutoSDRInputGui::updateStatus()
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

    if (m_statusCounter % 2 == 0) // 1s
    {
        uint32_t adcRate = ((PlutoSDRInput *) m_sampleSource)->getADCSampleRate();

        if (adcRate < 100000000) {
            ui->adcRateLabel->setText(tr("%1k").arg(QString::number(adcRate / 1000.0f, 'g', 5)));
        } else {
            ui->adcRateLabel->setText(tr("%1M").arg(QString::number(adcRate / 1000000.0f, 'g', 5)));
        }
    }

    if (m_statusCounter % 4 == 0) // 2s
    {
        std::string rssiStr;
        ((PlutoSDRInput *) m_sampleSource)->getRSSI(rssiStr);
        ui->rssiText->setText(QString::fromStdString(rssiStr));
    }

    if (m_statusCounter % 10 == 0) // 5s
    {
        if (m_deviceAPI->isBuddyLeader()) {
            ((PlutoSDRInput *) m_sampleSource)->fetchTemperature();
        }

        ui->temperatureText->setText(tr("%1C").arg(QString::number(((PlutoSDRInput *) m_sampleSource)->getTemperature(), 'f', 0)));
    }

    m_statusCounter++;
}

void PlutoSDRInputGui::handleDSPMessages()
{
    Message* message;

    while ((message = m_deviceAPI->getDeviceOutputMessageQueue()->pop()) != 0)
    {
        if (DSPSignalNotification::match(*message))
        {
            qDebug("LimeSDRInputGUI::handleMessagesToGUI: message: %s", message->getIdentifier());
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("LimeSDRInputGUI::handleMessagesToGUI: SampleRate: %d, CenterFrequency: %llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();

            delete message;
        }
    }
}

void PlutoSDRInputGui::updateSampleRateAndFrequency()
{
    m_deviceAPI->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceAPI->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateLabel->setText(tr("%1k").arg(QString::number(m_sampleRate / 1000.0f, 'g', 5)));
}
