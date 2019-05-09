///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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
#include "util/simpleserializer.h"

#include "bladerf2outputplugin.h"

#ifdef SERVER_MODE
#include "bladerf2output.h"
#else
#include "bladerf2outputgui.h"
#endif

const PluginDescriptor BladeRF2OutputPlugin::m_pluginDescriptor = {
    QString("BladeRF2 Output"),
    QString("4.5.4"),
    QString("(c) Edouard Griffiths, F4EXB"),
    QString("https://github.com/f4exb/sdrangel"),
    true,
    QString("https://github.com/f4exb/sdrangel")
};

const QString BladeRF2OutputPlugin::m_hardwareID = "BladeRF2";
const QString BladeRF2OutputPlugin::m_deviceTypeID = BLADERF2OUTPUT_DEVICE_TYPE_ID;

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

PluginInterface::SamplingDevices BladeRF2OutputPlugin::enumSampleSinks()
{
    SamplingDevices result;
    struct bladerf_devinfo *devinfo = 0;

    int count = bladerf_get_device_list(&devinfo);

    if (devinfo)
    {
        for(int i = 0; i < count; i++)
        {
            struct bladerf *dev;

            int status = bladerf_open_with_devinfo(&dev, &devinfo[i]);

            if (status == BLADERF_ERR_NODEV)
            {
                qCritical("Bladerf2OutputPlugin::enumSampleSinks: No device at index %d", i);
                continue;
            }
            else if (status != 0)
            {
                qCritical("Bladerf2OutputPlugin::enumSampleSinks: Failed to open device at index %d", i);
                continue;
            }

            const char *boardName = bladerf_get_board_name(dev);

            if (strcmp(boardName, "bladerf2") == 0)
            {
                unsigned int nbTxChannels = bladerf_get_channel_count(dev, BLADERF_TX);

                for (unsigned int j = 0; j < nbTxChannels; j++)
                {
                    qDebug("Blderf2InputPlugin::enumSampleSinks: device #%d (%s) channel %u", i, devinfo[i].serial, j);
                    QString displayedName(QString("BladeRF2[%1:%2] %3").arg(devinfo[i].instance).arg(j).arg(devinfo[i].serial));
                    result.append(SamplingDevice(displayedName,
                            m_hardwareID,
                            m_deviceTypeID,
                            QString(devinfo[i].serial),
                            i,
                            PluginInterface::SamplingDevice::PhysicalDevice,
                            PluginInterface::SamplingDevice::StreamSingleTx,
                            nbTxChannels,
                            j));
                }
            }

            bladerf_close(dev);
        }

        bladerf_free_device_list(devinfo); // Valgrind memcheck
    }

    return result;
}

#ifdef SERVER_MODE
PluginInstanceGUI* BladeRF2OutputPlugin::createSampleSinkPluginInstanceGUI(
        const QString& sinkId __attribute__((unused)),
        QWidget **widget __attribute__((unused)),
        DeviceUISet *deviceUISet __attribute__((unused)))
{
    return 0;
}
#else
PluginInstanceGUI* BladeRF2OutputPlugin::createSampleSinkPluginInstanceGUI(
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

DeviceSampleSink* BladeRF2OutputPlugin::createSampleSinkPluginInstanceOutput(const QString& sinkId, DeviceAPI *deviceAPI)
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




