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
    m_lastEngineState((DSPDeviceSourceEngine::State)-1)
{
    m_sampleSource = new PlutoSDRInput(m_deviceAPI);
    m_deviceAPI->setSource(m_sampleSource);

    float minF, maxF, stepF;
    // TODO: call device core to get values
    minF = 100000;
    maxF = 4000000;
    stepF = 1000;

    ui->setupUi(this);
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->centerFrequency->setValueRange(7, DevicePlutoSDR::loLowLimitFreq/1000, DevicePlutoSDR::loHighLimitFreq/1000);

    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(8, DevicePlutoSDR::srLowLimitFreq, DevicePlutoSDR::srHighLimitFreq);

    ui->lpf->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->lpf->setValueRange(5, DevicePlutoSDR::bbLPRxLowLimitFreq/1000, DevicePlutoSDR::bbLPRxHighLimitFreq/1000);

    ui->lpFIR->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->lpFIR->setValueRange(5, 1U, 56000U); // will be dynamically recalculated

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
//        displaySettings();
//        m_forceSettings = true;
//        sendSettings();
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool PlutoSDRInputGui::handleMessage(const Message& message)
{
    return false;
}
