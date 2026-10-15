///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include <QtPlugin>

#include "plugin/pluginapi.h"
#include "usrp/deviceusrp.h"

#ifndef SERVER_MODE
#include "usrpmimogui.h"
#endif
#include "usrpmimo.h"
#include "usrpmimoplugin.h"
#include "usrpmimowebapiadapter.h"

const PluginDescriptor USRPMIMOPlugin::m_pluginDescriptor = {
    QStringLiteral("USRP"),
    QStringLiteral("USRP MIMO"),
    QStringLiteral("7.28.0"),
    QStringLiteral("(c) Jon Beniston, M7RCE"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

static constexpr const char* const m_hardwareID = "USRP";
static constexpr const char* const m_deviceTypeID = USRPMIMO_DEVICE_TYPE_ID;

USRPMIMOPlugin::USRPMIMOPlugin(QObject* parent) :
    QObject(parent)
{
}

const PluginDescriptor& USRPMIMOPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void USRPMIMOPlugin::initPlugin(PluginAPI* pluginAPI)
{
    pluginAPI->registerSampleMIMO(m_deviceTypeID, this);
}

void USRPMIMOPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) { // check if it was done
        return;
    }

    DeviceUSRP::enumOriginDevices(m_hardwareID, originDevices);
    listedHwIds.append(m_hardwareID);
}

PluginInterface::SamplingDevices USRPMIMOPlugin::enumSampleMIMO(const OriginDevices& originDevices)
{
    SamplingDevices result;

    for (OriginDevices::const_iterator it = originDevices.begin(); it != originDevices.end(); ++it)
    {
        // Only offer MIMO for devices with more than one Rx or Tx channel
        if ((it->hardwareId == m_hardwareID) && ((it->nbRxStreams > 1) || (it->nbTxStreams > 1)))
        {
            QString displayedName = it->displayableName;
            displayedName.replace(QString(":$1]"), QString("]"));
            result.append(SamplingDevice(
                displayedName,
                m_hardwareID,
                m_deviceTypeID,
                it->serial,
                it->sequence,
                PluginInterface::SamplingDevice::PhysicalDevice,
                PluginInterface::SamplingDevice::StreamMIMO,
                1,
                0
            ));
        }
    }

    return result;
}

#ifdef SERVER_MODE
DeviceGUI* USRPMIMOPlugin::createSampleMIMOPluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    (void) sourceId;
    (void) widget;
    (void) deviceUISet;
    return nullptr;
}
#else
DeviceGUI* USRPMIMOPlugin::createSampleMIMOPluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    if (sourceId == m_deviceTypeID)
    {
        USRPMIMOGUI* gui = new USRPMIMOGUI(deviceUISet);
        *widget = gui;
        return gui;
    }
    else
    {
        return nullptr;
    }
}
#endif

DeviceSampleMIMO *USRPMIMOPlugin::createSampleMIMOPluginInstance(const QString& mimoId, DeviceAPI *deviceAPI)
{
    if (mimoId == m_deviceTypeID)
    {
        USRPMIMO* mimo = new USRPMIMO(deviceAPI);
        return mimo;
    }
    else
    {
        return nullptr;
    }
}

DeviceWebAPIAdapter *USRPMIMOPlugin::createDeviceWebAPIAdapter() const
{
    return new USRPMIMOWebAPIAdapter();
}
