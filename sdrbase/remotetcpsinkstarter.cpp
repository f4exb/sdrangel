///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE <jon@beniston.com>                     //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
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

#include <QTimer>

#include "remotetcpsinkstarter.h"

#include "maincore.h"
#include "device/deviceset.h"
#include "device/deviceapi.h"
#include "device/deviceenumerator.h"
#include "dsp/devicesamplesource.h"
#include "channel/channelapi.h"
#include "channel/channelwebapiutils.h"
#include "SWGChannelSettings.h"
#include "SWGRemoteTCPSinkSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceSettings.h"

// Lists available physical devices to stdout
void RemoteTCPSinkStarter::listAvailableDevices()
{
    int nbSamplingDevices = DeviceEnumerator::instance()->getNbRxSamplingDevices();

    printf("Available devices:\n");
    for (int i = 0; i < nbSamplingDevices; i++)
    {
        const PluginInterface::SamplingDevice *samplingDevice;

        samplingDevice = DeviceEnumerator::instance()->getRxSamplingDevice(i);
        if (samplingDevice->type == PluginInterface::SamplingDevice::PhysicalDevice)
        {
            printf(" HWType: %s", qPrintable(samplingDevice->hardwareId));
            if (!samplingDevice->serial.isEmpty()) {
                printf(" Serial: %s", qPrintable(samplingDevice->serial));
            }
            printf("\n");
        }
    }
}

RemoteTCPSinkStarter::RemoteTCPSinkStarter(const QString& address, int port, const QString& hwType, const QString& serial) :
    m_dataAddress(address),
    m_dataPort(port),
    m_deviceSet(nullptr)
{
    // Add device of requested type
    SWGSDRangel::SWGDeviceSettings *response = new SWGSDRangel::SWGDeviceSettings();
    response->init();
    ChannelWebAPIUtils::addDevice(hwType, 0, QStringList(), response, this, SLOT(deviceOpened(int)));
}

void RemoteTCPSinkStarter::deviceOpened(int deviceSetIndex)
{
    m_deviceSet = MainCore::instance()->getDeviceSets()[deviceSetIndex];
    // Add RemoteTCPSink channel
    connect(MainCore::instance(), &MainCore::channelAdded, this, &RemoteTCPSinkStarter::channelAdded);
    ChannelWebAPIUtils::addChannel(deviceSetIndex, "sdrangel.channel.remotetcpsink", 0);
}

void RemoteTCPSinkStarter::channelAdded(int deviceSetIndex, ChannelAPI *channel)
{
    (void) deviceSetIndex;

    // Set RemoteTCPSink settings
    ChannelWebAPIUtils::patchChannelSetting(channel, "dataAddress", m_dataAddress);
    ChannelWebAPIUtils::patchChannelSetting(channel, "dataPort", m_dataPort);

    // Wait for settings to be applied, then start the device
    QTimer::singleShot(250, [=] {
        startDevice();
    });
}

void RemoteTCPSinkStarter::startDevice()
{
    // Start the device
    ChannelWebAPIUtils::run(m_deviceSet->getIndex());

    // Delete object as we have finished
    delete this;
}

// Start Remote TCP Sink on specified device, with specified address and port
void RemoteTCPSinkStarter::start(const MainParser& parser)
{
    QString remoteTCPSinkAddress = parser.getRemoteTCPSinkAddressOption();
    int remoteTCPSinkPort = parser.getRemoteTCPSinkPortOption();
    QString remoteTCPSinkHWType = parser.getRemoteTCPSinkHWType();
    QString remoteTCPSinkSerial = parser.getRemoteTCPSinkSerial();

    QTimer::singleShot(250, [=] {
        new RemoteTCPSinkStarter(remoteTCPSinkAddress,
            remoteTCPSinkPort,
            remoteTCPSinkHWType,
            remoteTCPSinkSerial);
    });
}
