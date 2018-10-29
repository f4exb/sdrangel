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

#include "util/simpleserializer.h"

#include "soapysdrinput.h"

SoapySDRInput::SoapySDRInput(DeviceSourceAPI *deviceAPI)
{
}

SoapySDRInput::~SoapySDRInput()
{
}

void SoapySDRInput::destroy()
{
    delete this;
}

void SoapySDRInput::init()
{
}

bool SoapySDRInput::start()
{
    return false;
}

void SoapySDRInput::stop()
{
}

QByteArray SoapySDRInput::serialize() const
{
    SimpleSerializer s(1);
    return s.final();
}

bool SoapySDRInput::deserialize(const QByteArray& data)
{
    return false;
}

const QString& SoapySDRInput::getDeviceDescription() const
{
    return m_deviceDescription;
}

int SoapySDRInput::getSampleRate() const
{
    return 0;
}

quint64 SoapySDRInput::getCenterFrequency() const
{
    return 0;
}

void SoapySDRInput::setCenterFrequency(qint64 centerFrequency)
{
}

bool SoapySDRInput::handleMessage(const Message& message)
{
    return false;
}
