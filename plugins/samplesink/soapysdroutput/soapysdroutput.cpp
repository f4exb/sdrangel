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

#include "soapysdroutput.h"

SoapySDROutput::SoapySDROutput(DeviceSinkAPI *deviceAPI __attribute__((unused))) :
    m_deviceDescription("SoapySDROutput")
{
}

SoapySDROutput::~SoapySDROutput()
{
}

void SoapySDROutput::destroy()
{
    delete this;
}

void SoapySDROutput::init()
{
}

bool SoapySDROutput::start()
{
    return false;
}

void SoapySDROutput::stop()
{
}

QByteArray SoapySDROutput::serialize() const
{
    SimpleSerializer s(1);
    return s.final();
}

bool SoapySDROutput::deserialize(const QByteArray& data __attribute__((unused)))
{
    return false;
}

const QString& SoapySDROutput::getDeviceDescription() const
{
    return m_deviceDescription;
}

int SoapySDROutput::getSampleRate() const
{
    return 0;
}

quint64 SoapySDROutput::getCenterFrequency() const
{
    return 0;
}

void SoapySDROutput::setCenterFrequency(qint64 centerFrequency __attribute__((unused)))
{
}

bool SoapySDROutput::handleMessage(const Message& message __attribute__((unused)))
{
    return false;
}



