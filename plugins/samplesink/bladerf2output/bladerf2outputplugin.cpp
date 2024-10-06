///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2020, 2022-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
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

#include <QtPlugin>
#include <libbladeRF.h>
#include "plugin/pluginapi.h"

#include "bladerf2outputplugin.h"
#include "bladerf2outputwebapiadapter.h"

#ifdef SERVER_MODE
#include "bladerf2output.h"
#else
#include "bladerf2outputgui.h"
#endif

const PluginDescriptor BladeRF2OutputPlugin::m_pluginDescriptor = {
    QStringLiteral("BladeRF2"),
    QStringLiteral("BladeRF2 Output"),
    QStringLiteral("7.22.1"),
    QStringLiteral("(c) Edouard Griffiths, F4EXB"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

static constexpr const char* const m_hardwareID = "BladeRF2";
static constexpr const char* const m_deviceTypeID = BLADERF2OUTPUT_DEVICE_TYPE_ID;

BladeRF2OutputPlugin::BladeRF2OutputPlugin(QObject* parent) :
    QObject(parent)
{
}

const PluginDescriptor& BladeRF2OutputPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void BladeRF2OutputPlugin::initPlugin(PluginAPI* pluginAPI)
{
    pluginAPI->registerSampleSink(m_deviceTypeID, this);
}

void BladeRF2OutputPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) { // check if it was done
        return;
    }

    DeviceBladeRF2::enumOriginDevices(m_hardwareID, originDevices);
    listedHwIds.append(m_hardwareID);
}

PluginInterface::SamplingDevices BladeRF2OutputPlugin::enumSampleSinks(const OriginDevices& originDevices)
{
    SamplingDevices result;

    for (OriginDevices::const_iterator it = originDevices.begin(); it != originDevices.end(); ++it)
    {
        if (it->hardwareId == m_hardwareID)
        {
            for (int j = 0; j < it->nbTxStreams; j++)
            {
                QString displayedName = it->displayableName;
                displayedName.replace(QString("$1]"), QString("%1]").arg(j));
                result.append(SamplingDevice(
                    displayedName,
                    it->hardwareId,
                    m_deviceTypeID,
                    it->serial,
                    it->sequence,
                    PluginInterface::SamplingDevice::PhysicalDevice,
                    PluginInterface::SamplingDevice::StreamSingleTx,
                    it->nbTxStreams,
                    j
                ));
            }
        }
    }

    return result;
}

#ifdef SERVER_MODE
DeviceGUI* BladeRF2OutputPlugin::createSampleSinkPluginInstanceGUI(
        const QString& sinkId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    (void) sinkId;
    (void) widget;
    (void) deviceUISet;
    return 0;
}
#else
DeviceGUI* BladeRF2OutputPlugin::createSampleSinkPluginInstanceGUI(
        const QString& sinkId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    if(sinkId == m_deviceTypeID)
    {
        BladeRF2OutputGui* gui = new BladeRF2OutputGui(deviceUISet);
        *widget = gui;
        return gui;
    }
    else
    {
        return 0;
    }
}
#endif

DeviceSampleSink* BladeRF2OutputPlugin::createSampleSinkPluginInstance(const QString& sinkId, DeviceAPI *deviceAPI)
{
    if(sinkId == m_deviceTypeID)
    {
        BladeRF2Output* output = new BladeRF2Output(deviceAPI);
        return output;
    }
    else
    {
        return 0;
    }
}

DeviceWebAPIAdapter *BladeRF2OutputPlugin::createDeviceWebAPIAdapter() const
{
    return new BladeRF2OutputWebAPIAdapter();
}
