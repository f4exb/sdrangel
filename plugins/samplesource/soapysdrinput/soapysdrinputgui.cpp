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

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/devicesourceapi.h"
#include "device/deviceuiset.h"
#include "util/simpleserializer.h"

#include "ui_soapysdrinputgui.h"
#include "soapysdrinputgui.h"

SoapySDRInputGui::SoapySDRInputGui(DeviceUISet *deviceUISet, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::SoapySDRInputGui),
    m_deviceUISet(deviceUISet),
    m_forceSettings(true),
    m_doApplySettings(true),
    m_sampleSource(0),
    m_sampleRate(0),
    m_deviceCenterFrequency(0),
    m_lastEngineState(DSPDeviceSourceEngine::StNotStarted)
{
    m_sampleSource = (SoapySDRInput*) m_deviceUISet->m_deviceSourceAPI->getSampleSource();
    ui->setupUi(this);
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
}

SoapySDRInputGui::~SoapySDRInputGui()
{
    delete ui;
}

void SoapySDRInputGui::destroy()
{
    delete this;
}

void SoapySDRInputGui::setName(const QString& name)
{
    setObjectName(name);
}

QString SoapySDRInputGui::getName() const
{
    return objectName();
}

void SoapySDRInputGui::resetToDefaults()
{
}

qint64 SoapySDRInputGui::getCenterFrequency() const
{
    return 0;
}

void SoapySDRInputGui::setCenterFrequency(qint64 centerFrequency __attribute__((unused)))
{
}

QByteArray SoapySDRInputGui::serialize() const
{
    SimpleSerializer s(1);
    return s.final();
}

bool SoapySDRInputGui::deserialize(const QByteArray& data __attribute__((unused)))
{
    return false;
}


bool SoapySDRInputGui::handleMessage(const Message& message __attribute__((unused)))
{
    return false;
}


