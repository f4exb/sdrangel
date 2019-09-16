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

#include "bladerf1inputplugin.h"

#include <QtPlugin>
#include <libbladeRF.h>
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "bladerf1inputwebapiadapter.h"

#ifdef SERVER_MODE
#include "bladerf1input.h"
#else
#include "bladerf1inputgui.h"
#endif

const PluginDescriptor Blderf1InputPlugin::m_pluginDescriptor = {
	QString("BladeRF1 Input"),
	QString("4.11.10"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

const QString Blderf1InputPlugin::m_hardwareID = "BladeRF1";
const QString Blderf1InputPlugin::m_deviceTypeID = BLADERF1INPUT_DEVICE_TYPE_ID;

Blderf1InputPlugin::Blderf1InputPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& Blderf1InputPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void Blderf1InputPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

void Blderf1InputPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) { // check if it was done
        return;
    }

	struct bladerf_devinfo *devinfo = nullptr;
	int count = bladerf_get_device_list(&devinfo);

    if (devinfo)
    {
        for(int i = 0; i < count; i++)
        {
            struct bladerf *dev;

            int status = bladerf_open_with_devinfo(&dev, &devinfo[i]);

            if (status == BLADERF_ERR_NODEV)
            {
                qCritical("BlderfInputPlugin::enumSampleSources: No device at index %d", i);
                continue;
            }
            else if (status != 0)
            {
                qCritical("BlderfInputPlugin::enumSampleSources: Failed to open device at index %d", i);
                continue;
            }

            const char *boardName = bladerf_get_board_name(dev);

            if (strcmp(boardName, "bladerf1") == 0)
            {
                QString displayableName(QString("BladeRF1[%1] %2").arg(devinfo[i].instance).arg(devinfo[i].serial));

                originDevices.append(OriginDevice(
                    displayableName,
                    m_hardwareID,
                    devinfo[i].serial,
                    i,
                    1, // nb Rx
                    1  // nb Tx
                ));
            }

            bladerf_close(dev);
        }

		bladerf_free_device_list(devinfo); // Valgrind memcheck
	}

    listedHwIds.append(m_hardwareID);
}

PluginInterface::SamplingDevices Blderf1InputPlugin::enumSampleSources(const OriginDevices& originDevices)
{
	SamplingDevices result;

	for (OriginDevices::const_iterator it = originDevices.begin(); it != originDevices.end(); ++it)
    {
        if (it->hardwareId == m_hardwareID)
        {
            result.append(SamplingDevice(
                it->displayableName,
                m_hardwareID,
                m_deviceTypeID,
                it->serial,
                it->sequence,
                PluginInterface::SamplingDevice::PhysicalDevice,
                PluginInterface::SamplingDevice::StreamSingleRx,
                1,
                0));
        }
    }

	return result;
}

#ifdef SERVER_MODE
PluginInstanceGUI* Blderf1InputPlugin::createSampleSourcePluginInstanceGUI(
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
PluginInstanceGUI* Blderf1InputPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if(sourceId == m_deviceTypeID)
	{
		Bladerf1InputGui* gui = new Bladerf1InputGui(deviceUISet);
		*widget = gui;
		return gui;
	}
	else
	{
		return 0;
	}
}
#endif

DeviceSampleSource *Blderf1InputPlugin::createSampleSourcePluginInstance(const QString& sourceId, DeviceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
        Bladerf1Input *input = new Bladerf1Input(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}

DeviceWebAPIAdapter *Blderf1InputPlugin::createDeviceWebAPIAdapter() const
{
    return new BladeRF1InputWebAPIAdapter();
}
