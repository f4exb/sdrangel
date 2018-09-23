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

#include "SWGDeviceSettings.h"
#include "SWGBladeRF2InputSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGBladeRF2InputReport.h"

#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"
#include "dsp/dspcommands.h"
#include "dsp/filerecord.h"
#include "dsp/dspengine.h"

#include "bladerf2/devicebladerf2shared.h"
#include "bladerf2/devicebladerf2.h"
#include "bladerf2input.h"


MESSAGE_CLASS_DEFINITION(BladeRF2Input::MsgConfigureBladeRF2, Message)
MESSAGE_CLASS_DEFINITION(BladeRF2Input::MsgFileRecord, Message)
MESSAGE_CLASS_DEFINITION(BladeRF2Input::MsgStartStop, Message)

BladeRF2Input::BladeRF2Input(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_deviceDescription("BladeRF2Input"),
    m_running(false)
{
    openDevice();

    m_fileSink = new FileRecord(QString("test_%1.sdriq").arg(m_deviceAPI->getDeviceUID()));
    m_deviceAPI->addSink(m_fileSink);
}

BladeRF2Input::~BladeRF2Input()
{
    if (m_running) {
        stop();
    }

    m_deviceAPI->removeSink(m_fileSink);
    delete m_fileSink;
    closeDevice();
}

void BladeRF2Input::destroy()
{
    delete this;
}

bool BladeRF2Input::openDevice()
{
    if (!m_sampleFifo.setSize(96000 * 4))
    {
        qCritical("BladeRF2Input::openDevice: could not allocate SampleFifo");
        return false;
    }
    else
    {
        qDebug("BladeRF2Input::openDevice: allocated SampleFifo");
    }

    // look for Rx buddies and get reference to the device object
    // if there is a channel left take the first available
    if (m_deviceAPI->getSourceBuddies().size() > 0) // look source sibling first
    {
        qDebug("BladeRF2Input::openDevice: look in Rx buddies");

        DeviceSourceAPI *sourceBuddy = m_deviceAPI->getSourceBuddies()[0];
        DeviceBladeRF2Shared *deviceBladeRF2Shared = (DeviceBladeRF2Shared*) sourceBuddy->getBuddySharedPtr();

        if (deviceBladeRF2Shared == 0)
        {
            qCritical("BladeRF2Input::openDevice: the source buddy shared pointer is null");
            return false;
        }

        DeviceBladeRF2 *device = deviceBladeRF2Shared->m_dev;

        if (device == 0)
        {
            qCritical("BladeRF2Input::openDevice: cannot get device pointer from Rx buddy");
            return false;
        }

        m_deviceShared.m_dev = device;
        int requestedChannel = m_deviceAPI->getItemIndex();

        if (requestedChannel == deviceBladeRF2Shared->m_channel)
        {
            qCritical("BladeRF2Input::openDevice: channel %u already in use", requestedChannel);
            return false;
        }

        if (!device->openRx(requestedChannel))
        {
            qCritical("BladeRF2Input::openDevice: channel %u cannot be enabled", requestedChannel);
            return false;
        }
        else
        {
            m_deviceShared.m_channel = requestedChannel;
            qDebug("BladeRF2Input::openDevice: channel %u enabled", requestedChannel);
        }
    }
    // look for Tx buddies and get reference to the device object
    // allocate the Rx channel unconditionally
    else if (m_deviceAPI->getSinkBuddies().size() > 0) // then sink
    {
        qDebug("BladeRF2Input::openDevice: look in Tx buddies");

        DeviceSinkAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
        DeviceBladeRF2Shared *deviceBladeRF2Shared = (DeviceBladeRF2Shared*) sinkBuddy->getBuddySharedPtr();

        if (deviceBladeRF2Shared == 0)
        {
            qCritical("BladeRF2Input::openDevice: the sink buddy shared pointer is null");
            return false;
        }

        DeviceBladeRF2 *device = deviceBladeRF2Shared->m_dev;

        if (device == 0)
        {
            qCritical("BladeRF2Input::openDevice: cannot get device pointer from Rx buddy");
            return false;
        }

        m_deviceShared.m_dev = device;
        int requestedChannel = m_deviceAPI->getItemIndex();

        if (!device->openRx(requestedChannel))
        {
            qCritical("BladeRF2Input::openDevice: channel %u cannot be enabled", requestedChannel);
            return false;
        }
        else
        {
            m_deviceShared.m_channel = requestedChannel;
            qDebug("BladeRF2Input::openDevice: channel %u enabled", requestedChannel);
        }
    }
    // There are no buddies then create the first BladeRF2 device
    // allocate the Rx channel unconditionally
    else
    {
        qDebug("BladeRF2Input::openDevice: open device here");

        m_deviceShared.m_dev = new DeviceBladeRF2();
        char serial[256];
        strcpy(serial, qPrintable(m_deviceAPI->getSampleSourceSerial()));

        if (!m_deviceShared.m_dev->open(serial))
        {
            qCritical("BladeRF2Input::openDevice: cannot open BladeRF2 device");
            return false;
        }

        int requestedChannel = m_deviceAPI->getItemIndex();

        if (!m_deviceShared.m_dev->openRx(requestedChannel))
        {
            qCritical("BladeRF2Input::openDevice: channel %u cannot be enabled", requestedChannel);
            return false;
        }
        else
        {
            m_deviceShared.m_channel = requestedChannel;
            qDebug("BladeRF2Input::openDevice: channel %u enabled", requestedChannel);
        }
    }

    m_deviceAPI->setBuddySharedPtr(&m_deviceShared); // propagate common parameters to API
    return true;
}

void BladeRF2Input::closeDevice()
{
    if (m_deviceShared.m_dev == 0) { // was never open
        return;
    }

    if (m_running) {
        stop();
    }

    m_deviceShared.m_channel = -1;

    // No buddies so effectively close the device

    if ((m_deviceAPI->getSinkBuddies().size() == 0) && (m_deviceAPI->getSourceBuddies().size() == 0))
    {
        m_deviceShared.m_dev->close();
        delete m_deviceShared.m_dev;
        m_deviceShared.m_dev = 0;
    }
}

void BladeRF2Input::init()
{
    applySettings(m_settings, true);
}

