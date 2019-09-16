///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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
#include "util/simpleserializer.h"
#include "bladerf2inputwebapiadapter.h"

#ifdef SERVER_MODE
#include "bladerf2input.h"
#else
#include "bladerf2inputgui.h"
#endif

const PluginDescriptor Blderf2InputPlugin::m_pluginDescriptor = {
    QString("BladeRF2 Input"),
    QString("4.11.10"),
    QString("(c) Edouard Griffiths, F4EXB"),
    QString("https://github.com/f4exb/sdrangel"),
    true,
    QString("https://github.com/f4exb/sdrangel")
};

const QString Blderf2InputPlugin::m_hardwareID = "BladeRF2";
const QString Blderf2InputPlugin::m_deviceTypeID = BLADERF2INPUT_DEVICE_TYPE_ID;

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
                qCritical("Blderf2InputPlugin::enumOriginDevices: No device at index %d", i);
                continue;
            }
            else if (status != 0)
            {
                qCritical("Blderf2InputPlugin::enumOriginDevices: Failed to open device at index %d", i);
                continue;
            }

            const char *boardName = bladerf_get_board_name(dev);

            if (strcmp(boardName, "bladerf2") == 0)
            {
                unsigned int nbRxChannels = bladerf_get_channel_count(dev, BLADERF_RX);
                unsigned int nbTxChannels = bladerf_get_channel_count(dev, BLADERF_TX);
                qDebug("Blderf2InputPlugin::enumOriginDevices: device #%d (%s)", i, devinfo[i].serial);
                QString displayableName(QString("BladeRF2[%1:%2] %3").arg(devinfo[i].instance).arg("%1").arg(devinfo[i].serial));

                originDevices.append(OriginDevice(
                    displayableName,
                    m_hardwareID,
                    devinfo[i].serial,
                    i,
                    2, // nb Rx
                    2  // nb Tx
                ));
            }

            bladerf_close(dev);
        }

        bladerf_free_device_list(devinfo); // Valgrind memcheck
    }

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
                result.append(SamplingDevice(
                    it->displayableName,
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
PluginInstanceGUI* Blderf2InputPlugin::createSampleSourcePluginInstanceGUI(
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
PluginInstanceGUI* Blderf2InputPlugin::createSampleSourcePluginInstanceGUI(
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
