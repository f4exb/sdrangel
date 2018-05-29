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

#include <QtPlugin>
#include "perseus-sdr.h"

#include <device/devicesourceapi.h>
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "perseus/deviceperseus.h"
#include "perseusplugin.h"
#ifdef SERVER_MODE
#include "perseusinput.h"
#else
#include "perseusgui.h"
#endif

const PluginDescriptor PerseusPlugin::m_pluginDescriptor = {
	QString("Perseus Input"),
	QString("4.0.0"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

const QString PerseusPlugin::m_hardwareID = "Perseus";
const QString PerseusPlugin::m_deviceTypeID = PERSEUS_DEVICE_TYPE_ID;
const int PerseusPlugin::m_maxDevices = 32;

PerseusPlugin::PerseusPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& PerseusPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void PerseusPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

PluginInterface::SamplingDevices PerseusPlugin::enumSampleSources()
{
    DevicePerseus::instance().scan();
    std::vector<std::string> serials;
    DevicePerseus::instance().getSerials(serials);

    std::vector<std::string>::const_iterator it = serials.begin();
    int i;
	SamplingDevices result;

	for (i = 0; it != serials.end(); ++it, ++i)
	{
	    QString serial_str = QString::fromLocal8Bit(it->c_str());
	    QString displayedName(QString("Perseus[%1] %2").arg(i).arg(serial_str));

        result.append(SamplingDevice(displayedName,
                m_hardwareID,
                m_deviceTypeID,
                serial_str,
                i,
                PluginInterface::SamplingDevice::PhysicalDevice,
                true,
                1,
                0));

        qDebug("PerseusPlugin::enumSampleSources: enumerated PlutoSDR device #%d", i);
	}

	return result;
}

#ifdef SERVER_MODE
PluginInstanceGUI* PerseusPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId __attribute__((unused)),
        QWidget **widget __attribute__((unused)),
        DeviceUISet *deviceUISet __attribute__((unused)))
{
    return 0;
}
#else
PluginInstanceGUI* PerseusPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if (sourceId == m_deviceTypeID)
	{
	    PerseusGui* gui = new PerseusGui(deviceUISet);
		*widget = gui;
		return gui;
	}
	else
	{
		return 0;
	}
}
#endif

DeviceSampleSource *PerseusPlugin::createSampleSourcePluginInstanceInput(const QString& sourceId, DeviceSourceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
        PerseusInput* input = new PerseusInput(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}
