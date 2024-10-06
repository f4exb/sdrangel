///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2018 Christopher Hewitt <hewitt@ieee.org>                       //
// Copyright (C) 2019 Davide Gerhard <rainbow@irh.it>                            //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
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

#include "bladerf2inputplugin.h"

#include <QtPlugin>
#include <libbladeRF.h>
#include "plugin/pluginapi.h"
#include "bladerf2inputwebapiadapter.h"

#ifdef SERVER_MODE
#include "bladerf2input.h"
#else
#include "bladerf2inputgui.h"
#endif

const PluginDescriptor Blderf2InputPlugin::m_pluginDescriptor = {
    QStringLiteral("BladeRF2"),
    QStringLiteral("BladeRF2 Input"),
    QStringLiteral("7.22.1"),
    QStringLiteral("(c) Edouard Griffiths, F4EXB"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

static constexpr const char* const m_hardwareID = "BladeRF2";
static constexpr const char* const m_deviceTypeID = BLADERF2INPUT_DEVICE_TYPE_ID;

Blderf2InputPlugin::Blderf2InputPlugin(QObject* parent) :
    QObject(parent)
{
}

const PluginDescriptor& Blderf2InputPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void Blderf2InputPlugin::initPlugin(PluginAPI* pluginAPI)
{
    pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

void Blderf2InputPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) { // check if it was done
        return;
    }

    DeviceBladeRF2::enumOriginDevices(m_hardwareID, originDevices);
    listedHwIds.append(m_hardwareID);
}

PluginInterface::SamplingDevices Blderf2InputPlugin::enumSampleSources(const OriginDevices& originDevices)
{
    SamplingDevices result;

	for (OriginDevices::const_iterator it = originDevices.begin(); it != originDevices.end(); ++it)
    {
        if (it->hardwareId == m_hardwareID)
        {
            for (int j=0; j < it->nbRxStreams; j++)
            {
                QString displayedName = it->displayableName;
                displayedName.replace(QString("$1]"), QString("%1]").arg(j));
                result.append(SamplingDevice(
                    displayedName,
                    m_hardwareID,
                    m_deviceTypeID,
                    it->serial,
                    it->sequence,
                    PluginInterface::SamplingDevice::PhysicalDevice,
                    PluginInterface::SamplingDevice::StreamSingleRx,
                    it->nbRxStreams,
                    j));
            }
        }
    }

    return result;
}

#ifdef SERVER_MODE
DeviceGUI* Blderf2InputPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    (void) sourceId;
    (void) widget;
    (void) deviceUISet;
    return 0;
}
#else
DeviceGUI* Blderf2InputPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    if(sourceId == m_deviceTypeID)
    {
        BladeRF2InputGui* gui = new BladeRF2InputGui(deviceUISet);
        *widget = gui;
        return gui;
    }
    else
    {
        return 0;
    }
}
#endif

DeviceSampleSource *Blderf2InputPlugin::createSampleSourcePluginInstance(const QString& sourceId, DeviceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
        BladeRF2Input *input = new BladeRF2Input(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}

DeviceWebAPIAdapter *Blderf2InputPlugin::createDeviceWebAPIAdapter() const
{
    return new BladeRF2InputWebAPIAdapter();
}
