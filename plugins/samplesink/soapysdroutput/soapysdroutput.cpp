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
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "device/devicesinkapi.h"
#include "device/devicesourceapi.h"
#include "soapysdr/devicesoapysdr.h"

#include "soapysdroutput.h"

SoapySDROutput::SoapySDROutput(DeviceSinkAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_deviceDescription("SoapySDROutput"),
    m_running(false)
{
}

SoapySDROutput::~SoapySDROutput()
{
}

void SoapySDROutput::destroy()
{
    delete this;
}

bool SoapySDROutput::openDevice()
{
    m_sampleSourceFifo.resize(m_settings.m_devSampleRate/(1<<(m_settings.m_log2Interp <= 4 ? m_settings.m_log2Interp : 4)));

    // look for Tx buddies and get reference to the device object
    if (m_deviceAPI->getSinkBuddies().size() > 0) // look sink sibling first
    {
        qDebug("SoapySDROutput::openDevice: look in Tx buddies");

        DeviceSinkAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
        DeviceSoapySDRShared *deviceSoapySDRShared = (DeviceSoapySDRShared*) sinkBuddy->getBuddySharedPtr();

        if (deviceSoapySDRShared == 0)
        {
            qCritical("SoapySDROutput::openDevice: the sink buddy shared pointer is null");
            return false;
        }

        SoapySDR::Device *device = deviceSoapySDRShared->m_device;

        if (device == 0)
        {
            qCritical("SoapySDROutput::openDevice: cannot get device pointer from Tx buddy");
            return false;
        }

        m_deviceShared.m_device = device;
    }
    // look for Rx buddies and get reference to the device object
    else if (m_deviceAPI->getSourceBuddies().size() > 0) // then source
    {
        qDebug("SoapySDROutput::openDevice: look in Rx buddies");

        DeviceSourceAPI *sourceBuddy = m_deviceAPI->getSourceBuddies()[0];
        DeviceSoapySDRShared *deviceSoapySDRShared = (DeviceSoapySDRShared*) sourceBuddy->getBuddySharedPtr();

        if (deviceSoapySDRShared == 0)
        {
            qCritical("SoapySDROutput::openDevice: the source buddy shared pointer is null");
            return false;
        }

        SoapySDR::Device *device = deviceSoapySDRShared->m_device;

        if (device == 0)
        {
            qCritical("SoapySDROutput::openDevice: cannot get device pointer from Rx buddy");
            return false;
        }

        m_deviceShared.m_device = device;
    }
    // There are no buddies then create the first BladeRF2 device
    else
    {
        qDebug("SoapySDROutput::openDevice: open device here");
        DeviceSoapySDR& deviceSoapySDR = DeviceSoapySDR::instance();
        m_deviceShared.m_device = deviceSoapySDR.openSoapySDR(m_deviceAPI->getSampleSinkSequence());

        if (!m_deviceShared.m_device)
        {
            qCritical("SoapySDROutput::openDevice: cannot open SoapySDR device");
            return false;
        }
    }

    m_deviceShared.m_channel = m_deviceAPI->getItemIndex(); // publicly allocate channel
    m_deviceShared.m_sink = this;
    m_deviceAPI->setBuddySharedPtr(&m_deviceShared); // propagate common parameters to API
    return true;
}

void SoapySDROutput::closeDevice()
{
    if (m_deviceShared.m_device == 0) { // was never open
        return;
    }

    if (m_running) {
        stop();
    }

//    if (m_thread) { // stills own the thread => transfer to a buddy
//        moveThreadToBuddy();
//    }

    m_deviceShared.m_channel = -1; // publicly release channel
    m_deviceShared.m_sink = 0;

    // No buddies so effectively close the device

    if ((m_deviceAPI->getSinkBuddies().size() == 0) && (m_deviceAPI->getSourceBuddies().size() == 0))
    {
        DeviceSoapySDR& deviceSoapySDR = DeviceSoapySDR::instance();
        deviceSoapySDR.closeSoapySdr(m_deviceShared.m_device);
        m_deviceShared.m_device = 0;
    }
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



