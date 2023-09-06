///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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
#include "SWGChannelSettings.h"
#include "SWGRemoteTCPSinkSettings.h"
#include "SWGDeviceState.h"

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

// Instantiate specified sampling source device and create a RemoteTCPSink channel
// on the specified address and port and start the device
static void startRemoteTCPSink(const QString& address, int port, const QString& hwType, const QString& serial)
{
    MainCore *mainCore = MainCore::instance();

    // Delete any existing device sets, in case requested device is already in use
    int initialDeviceSets = mainCore->getDeviceSets().size();
    for (int i = 0; i < initialDeviceSets; i++)
    {
        MainCore::MsgRemoveLastDeviceSet *msg = MainCore::MsgRemoveLastDeviceSet::create();
        mainCore->getMainMessageQueue()->push(msg);
    }

    // Wait until they've been deleted
    if (initialDeviceSets > 0)
    {
        do
        {
            QTime dieTime = QTime::currentTime().addMSecs(100);
            while (QTime::currentTime() < dieTime) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
            }
        }
        while (mainCore->getDeviceSets().size() > 0);
    }

    // Create DeviceSet
    unsigned int deviceSetIndex = mainCore->getDeviceSets().size();
    MainCore::MsgAddDeviceSet *msg = MainCore::MsgAddDeviceSet::create(0);
    mainCore->getMainMessageQueue()->push(msg);

    // Switch to requested device type
    int nbSamplingDevices = DeviceEnumerator::instance()->getNbRxSamplingDevices();
    bool found = false;
    for (int i = 0; i < nbSamplingDevices; i++)
    {
        const PluginInterface::SamplingDevice *samplingDevice;

        samplingDevice = DeviceEnumerator::instance()->getRxSamplingDevice(i);

        if (!hwType.isEmpty() && (hwType != samplingDevice->hardwareId)) {
            continue;
        }
        if (!serial.isEmpty() && (serial != samplingDevice->serial)) {
            continue;
        }

        int direction = 0;
        MainCore::MsgSetDevice *msg = MainCore::MsgSetDevice::create(deviceSetIndex, i, direction);
        mainCore->getMainMessageQueue()->push(msg);
        found = true;
        break;
    }
    if (!found)
    {
        qCritical() << "startRemoteTCPSink: Failed to find device";
        return;
    }

    // Add RemoteTCPSink channel
    PluginAPI::ChannelRegistrations *channelRegistrations = mainCore->getPluginManager()->getRxChannelRegistrations();
    int nbRegistrations = channelRegistrations->size();
    int index = 0;
    for (; index < nbRegistrations; index++)
    {
        if (channelRegistrations->at(index).m_channelId == "RemoteTCPSink") {
            break;
        }
    }

    if (index < nbRegistrations)
    {
        MainCore::MsgAddChannel *msg = MainCore::MsgAddChannel::create(deviceSetIndex, index, 0);
        mainCore->getMainMessageQueue()->push(msg);
    }
    else
    {
        qCritical() << "startRemoteTCPSink: RemoteTCPSink is not available";
        return;
    }
    int channelIndex = 0;

    // Wait until device & channel are created - is there a better way?
    DeviceSet *deviceSet = nullptr;
    ChannelAPI *channelAPI = nullptr;
    do
    {
        QTime dieTime = QTime::currentTime().addMSecs(100);
        while (QTime::currentTime() < dieTime) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        }

        if (mainCore->getDeviceSets().size() > deviceSetIndex)
        {
            deviceSet = mainCore->getDeviceSets()[deviceSetIndex];
            if (deviceSet) {
                channelAPI = deviceSet->m_deviceAPI->getChanelSinkAPIAt(channelIndex);
            }
        }
    }
    while (channelAPI == nullptr);

    // Set TCP settings
    QStringList channelSettingsKeys = {"dataAddress", "dataPort"};
    SWGSDRangel::SWGChannelSettings response;
    response.init();
    SWGSDRangel::SWGRemoteTCPSinkSettings *sinkSettings = response.getRemoteTcpSinkSettings();
    sinkSettings->setDataAddress(new QString(address));
    sinkSettings->setDataPort(port);
    QString errorMessage;
    channelAPI->webapiSettingsPutPatch(false, channelSettingsKeys, response, errorMessage);

    // Wait some time for settings to be applied
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

    // Start the device (use WebAPI so GUI is updated)
    DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();
    QStringList deviceActionsKeys;
    SWGSDRangel::SWGDeviceState state;
    state.init();
    int res = source->webapiRun(true, state, errorMessage);
    if (res != 200) {
        qCritical() << "startRemoteTCPSink: Failed to start device: " << res;
    } else {
        qInfo().nospace().noquote() << "Remote TCP Sink started on " << address << ":" << port;
    }
}

// Start Remote TCP Sink on specified device, with specified address and port
void RemoteTCPSinkStarter::start(const MainParser& parser)
{
    QString remoteTCPSinkAddress = parser.getRemoteTCPSinkAddressOption();
    int remoteTCPSinkPort = parser.getRemoteTCPSinkPortOption();
    QString remoteTCPSinkHWType = parser.getRemoteTCPSinkHWType();
    QString remoteTCPSinkSerial = parser.getRemoteTCPSinkSerial();

    QTimer::singleShot(250, [=] {
        startRemoteTCPSink(
            remoteTCPSinkAddress,
            remoteTCPSinkPort,
            remoteTCPSinkHWType,
            remoteTCPSinkSerial);
    });
}
