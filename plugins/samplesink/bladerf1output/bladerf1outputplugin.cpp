///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#include <QtPlugin>
#include <libbladeRF.h>
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include <device/devicesourceapi.h>

#include "bladerf1outputplugin.h"

#ifdef SERVER_MODE
#include "bladerf1output.h"
#else
#include "bladerf1outputgui.h"
#endif

const PluginDescriptor Bladerf1OutputPlugin::m_pluginDescriptor = {
	QString("BladeRF1 Output"),
	QString("4.5.2"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

const QString Bladerf1OutputPlugin::m_hardwareID = "BladeRF1";
const QString Bladerf1OutputPlugin::m_deviceTypeID = BLADERF1OUTPUT_DEVICE_TYPE_ID;

Bladerf1OutputPlugin::Bladerf1OutputPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& Bladerf1OutputPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void Bladerf1OutputPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSink(m_deviceTypeID, this);
}

PluginInterface::SamplingDevices Bladerf1OutputPlugin::enumSampleSinks()
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
                qCritical("BladerfOutputPlugin::enumSampleSinks: No device at index %d", i);
                continue;
            }
            else if (status != 0)
            {
                qCritical("BladerfOutputPlugin::enumSampleSinks: Failed to open device at index %d", i);
                continue;
            }

            const char *boardName = bladerf_get_board_name(dev);

            if (strcmp(boardName, "bladerf1") == 0)
            {
                QString displayedName(QString("BladeRF1[%1] %2").arg(devinfo[i].instance).arg(devinfo[i].serial));

                result.append(SamplingDevice(displayedName,
                        m_hardwareID,
                        m_deviceTypeID,
                        QString(devinfo[i].serial),
                        i,
                        PluginInterface::SamplingDevice::PhysicalDevice,
                        false,
                        1,
                        0));

            }

            bladerf_close(dev);
        }

		bladerf_free_device_list(devinfo); // Valgrind memcheck
	}

	return result;
}

#ifdef SERVER_MODE
PluginInstanceGUI* Bladerf1OutputPlugin::createSampleSinkPluginInstanceGUI(
        const QString& sinkId __attribute__((unused)),
        QWidget **widget __attribute__((unused)),
        DeviceUISet *deviceUISet __attribute__((unused)))
{
    return 0;
}
#else
PluginInstanceGUI* Bladerf1OutputPlugin::createSampleSinkPluginInstanceGUI(
        const QString& sinkId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if(sinkId == m_deviceTypeID)
	{
		Bladerf1OutputGui* gui = new Bladerf1OutputGui(deviceUISet);
		*widget = gui;
		return gui;
	}
	else
	{
		return 0;
	}
}
#endif

DeviceSampleSink* Bladerf1OutputPlugin::createSampleSinkPluginInstanceOutput(const QString& sinkId, DeviceSinkAPI *deviceAPI)
{
    if(sinkId == m_deviceTypeID)
    {
        Bladerf1Output* output = new Bladerf1Output(deviceAPI);
        return output;
    }
    else
    {
        return 0;
    }
}
