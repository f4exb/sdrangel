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
#include "device/devicesinkapi.h"
#include "device/deviceuiset.h"
#include "util/simpleserializer.h"

#include "soapysdroutputgui.h"

SoapySDROutputGui::SoapySDROutputGui(DeviceUISet *deviceUISet, QWidget* parent) :
    QWidget(parent),
    ui(0),
    m_deviceUISet(deviceUISet),
    m_forceSettings(true),
    m_doApplySettings(true),
    m_sampleSink(0),
    m_sampleRate(0),
    m_lastEngineState(DSPDeviceSinkEngine::StNotStarted)
{
}

SoapySDROutputGui::~SoapySDROutputGui()
{
}

void SoapySDROutputGui::destroy()
{
    delete this;
}

void SoapySDROutputGui::setName(const QString& name)
{
    setObjectName(name);
}

QString SoapySDROutputGui::getName() const
{
    return objectName();
}

void SoapySDROutputGui::resetToDefaults()
{
}

qint64 SoapySDROutputGui::getCenterFrequency() const
{
    return 0;
}

void SoapySDROutputGui::setCenterFrequency(qint64 centerFrequency __attribute__((unused)))
{
}

QByteArray SoapySDROutputGui::serialize() const
{
    SimpleSerializer s(1);
    return s.final();
}

bool SoapySDROutputGui::deserialize(const QByteArray& data __attribute__((unused)))
{
    return false;
}


bool SoapySDROutputGui::handleMessage(const Message& message __attribute__((unused)))
{
    return false;
}





