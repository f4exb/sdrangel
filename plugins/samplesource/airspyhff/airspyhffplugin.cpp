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
#include <QAction>
#include <libairspyhf/airspyhf.h>

#include <device/devicesourceapi.h>
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "airspyhffplugin.h"
#include "airspyhffgui.h"


const PluginDescriptor AirspyHFFPlugin::m_pluginDescriptor = {
	QString("AirspyHF Input (float)"),
	QString("3.12.0"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

const QString AirspyHFFPlugin::m_hardwareID = "AirspyHFF";
const QString AirspyHFFPlugin::m_deviceTypeID = AIRSPYHFF_DEVICE_TYPE_ID;
const int AirspyHFFPlugin::m_maxDevices = 32;

AirspyHFFPlugin::AirspyHFFPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& AirspyHFFPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void AirspyHFFPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

PluginInterface::SamplingDevices AirspyHFFPlugin::enumSampleSources()
{
	SamplingDevices result;
	int nbDevices;
	uint64_t deviceSerials[m_maxDevices];

	nbDevices = airspyhf_list_devices(deviceSerials, m_maxDevices);

    if (nbDevices < 0)
    {
        qCritical("AirspyHFPlugin::enumSampleSources: failed to list Airspy HF devices");
    }

	for (int i = 0; i < nbDevices; i++)
	{
	    if (deviceSerials[i])
	    {
            QString serial_str = QString::number(deviceSerials[i], 16);
            QString displayedName(QString("AirspyHF(float)[%1] %2").arg(i).arg(serial_str));

            result.append(SamplingDevice(displayedName,
                    m_hardwareID,
                    m_deviceTypeID,
                    serial_str,
                    i,
                    PluginInterface::SamplingDevice::PhysicalDevice,
                    true,
                    1,
                    0));

            qDebug("AirspyHFFPlugin::enumSampleSources: enumerated Airspy HF device #%d", i);
	    }
	    else
	    {
            qDebug("AirspyHFFPlugin::enumSampleSources: finished to enumerate Airspy HF. %d devices found", i);
	        break; // finished
	    }
	}

	return result;
}

PluginInstanceGUI* AirspyHFFPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if (sourceId == m_deviceTypeID)
	{
	    AirspyHFFGui* gui = new AirspyHFFGui(deviceUISet);
		*widget = gui;
		return gui;
	}
	else
	{
		return 0;
	}
}

DeviceSampleSource *AirspyHFFPlugin::createSampleSourcePluginInstanceInput(const QString& sourceId, DeviceSourceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
        AirspyHFFInput* input = new AirspyHFFInput(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}
