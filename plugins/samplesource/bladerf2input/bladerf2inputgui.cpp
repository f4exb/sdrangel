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

#include <libbladeRF.h>

#include "ui_bladerf2inputgui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/devicesourceapi.h"
#include "device/deviceuiset.h"

#include "bladerf2inputgui.h"

BladeRF2InputGui::BladeRF2InputGui(DeviceUISet *deviceUISet, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::Bladerf2InputGui),
    m_deviceUISet(deviceUISet),
    m_forceSettings(true),
    m_doApplySettings(true),
    m_settings(),
    m_sampleSource(0),
    m_sampleRate(0),
    m_lastEngineState(DSPDeviceSourceEngine::StNotStarted)
{
    m_sampleSource = (BladeRF2Input*) m_deviceUISet->m_deviceSourceAPI->getSampleSource();
    int max, min, step;
    uint64_t f_min, f_max;

    ui->setupUi(this);

    m_sampleSource->getFrequencyRange(f_min, f_max, step);
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->centerFrequency->setValueRange(7, f_min/1000, f_max/1000);

    m_sampleSource->getSampleRateRange(min, max, step);
    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(8, min, max);

    m_sampleSource->getBandwidthRange(min, max, step);
    ui->bandwidth->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->bandwidth->setValueRange(6, min/1000, max/1000);

    m_gainModes = m_sampleSource->getGainModes(m_nbGainModes);

    if (m_gainModes)
    {
        for (int i = 0; i < m_nbGainModes; i++) {
            ui->gainMode->addItem(tr("%1").arg(m_gainModes[i].name));
        }
    }

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    displaySettings();

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleSource->setMessageQueueToGUI(&m_inputMessageQueue);

    sendSettings();
}

BladeRF2InputGui::~BladeRF2InputGui()
{
    delete ui;
}

void BladeRF2InputGui::destroy()
{
    delete this;
}

void BladeRF2InputGui::setName(const QString& name)
{
    setObjectName(name);
}

QString BladeRF2InputGui::getName() const
{
    return objectName();
}

void BladeRF2InputGui::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    sendSettings();
}

qint64 BladeRF2InputGui::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void BladeRF2InputGui::setCenterFrequency(qint64 centerFrequency)
{
    m_settings.m_centerFrequency = centerFrequency;
    displaySettings();
    sendSettings();
}

QByteArray BladeRF2InputGui::serialize() const
{
    return m_settings.serialize();
}

bool BladeRF2InputGui::deserialize(const QByteArray& data)
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

bool BladeRF2InputGui::handleMessage(const Message& message)
{
    if (BladeRF2Input::MsgConfigureBladeRF2::match(message))
    {
        const BladeRF2Input::MsgConfigureBladeRF2& cfg = (BladeRF2Input::MsgConfigureBladeRF2&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (BladeRF2Input::MsgStartStop::match(message))
    {
        BladeRF2Input::MsgStartStop& notif = (BladeRF2Input::MsgStartStop&) message;
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

void BladeRF2InputGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        qDebug("BladeRF2InputGui::handleInputMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("BladeRF2InputGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
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

void BladeRF2InputGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateLabel->setText(tr("%1k").arg(QString::number(m_sampleRate / 1000.0f, 'g', 5)));
}

void BladeRF2InputGui::displaySettings()
{
    blockApplySettings(true);

    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
    ui->sampleRate->setValue(m_settings.m_devSampleRate);
    ui->bandwidth->setValue(m_settings.m_bandwidth / 1000);

    ui->dcOffset->setChecked(m_settings.m_dcBlock);
    ui->iqImbalance->setChecked(m_settings.m_iqCorrection);

    ui->decim->setCurrentIndex(m_settings.m_log2Decim);
    ui->fcPos->setCurrentIndex((int) m_settings.m_fcPos);

    ui->gain->setValue(m_settings.m_globalGain);

    blockApplySettings(false);
}

void BladeRF2InputGui::sendSettings()
{
    if(!m_updateTimer.isActive())
        m_updateTimer.start(100);
}

void BladeRF2InputGui::on_centerFrequency_changed(quint64 value)
{
    m_settings.m_centerFrequency = value * 1000;
    sendSettings();
}

void BladeRF2InputGui::on_sampleRate_changed(quint64 value)
{
    m_settings.m_devSampleRate = value;
    sendSettings();
}

void BladeRF2InputGui::on_dcOffset_toggled(bool checked)
{
    m_settings.m_dcBlock = checked;
    sendSettings();
}

void BladeRF2InputGui::on_iqImbalance_toggled(bool checked)
{
    m_settings.m_iqCorrection = checked;
    sendSettings();
}

void BladeRF2InputGui::on_bandwidth_changed(quint64 value)
{
    m_settings.m_bandwidth = value * 1000;
    sendSettings();
}

void BladeRF2InputGui::on_decim_currentIndexChanged(int index)
{
    if ((index <0) || (index > 6))
        return;
    m_settings.m_log2Decim = index;
    sendSettings();
}

void BladeRF2InputGui::on_fcPos_currentIndexChanged(int index)
{
    if (index == 0) {
        m_settings.m_fcPos = BladeRF2InputSettings::FC_POS_INFRA;
        sendSettings();
    } else if (index == 1) {
        m_settings.m_fcPos = BladeRF2InputSettings::FC_POS_SUPRA;
        sendSettings();
    } else if (index == 2) {
        m_settings.m_fcPos = BladeRF2InputSettings::FC_POS_CENTER;
        sendSettings();
    }
}

void BladeRF2InputGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        BladeRF2Input::MsgStartStop *message = BladeRF2Input::MsgStartStop::create(checked);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void BladeRF2InputGui::on_record_toggled(bool checked)
{
    if (checked) {
        ui->record->setStyleSheet("QToolButton { background-color : red; }");
    } else {
        ui->record->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
    }

    BladeRF2Input::MsgFileRecord* message = BladeRF2Input::MsgFileRecord::create(checked);
    m_sampleSource->getInputMessageQueue()->push(message);
}

void BladeRF2InputGui::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "BladeRF2InputGui::updateHardware";
        BladeRF2Input::MsgConfigureBladeRF2* message = BladeRF2Input::MsgConfigureBladeRF2::create(m_settings, m_forceSettings);
        m_sampleSource->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_updateTimer.stop();
    }
}

void BladeRF2InputGui::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void BladeRF2InputGui::updateStatus()
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
