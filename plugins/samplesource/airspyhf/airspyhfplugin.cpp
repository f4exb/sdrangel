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

#include "airspyhfgui.h"
#include "airspyhfplugin.h"

#include <device/devicesourceapi.h>
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"


const PluginDescriptor AirspyHFPlugin::m_pluginDescriptor = {
	QString("AirspyHF Input"),
	QString("3.11.0"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

const QString AirspyHFPlugin::m_hardwareID = "AirspyHF";
const QString AirspyHFPlugin::m_deviceTypeID = AIRSPYHF_DEVICE_TYPE_ID;
const int AirspyHFPlugin::m_maxDevices = 32;

AirspyHFPlugin::AirspyHFPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& AirspyHFPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void AirspyHFPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

PluginInterface::SamplingDevices AirspyHFPlugin::enumSampleSources()
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
            QString displayedName(QString("AirspyHF[%1] %2").arg(i).arg(serial_str));

            result.append(SamplingDevice(displayedName,
                    m_hardwareID,
                    m_deviceTypeID,
                    serial_str,
                    i,
                    PluginInterface::SamplingDevice::PhysicalDevice,
                    true,
                    1,
                    0));

            qDebug("AirspyPlugin::enumSampleSources: enumerated Airspy device #%d", i);
	    }
	    else
	    {
            qDebug("AirspyHFPlugin::enumSampleSources: finished to enumerate Airspy HF. %d devices found", i);
	        break; // finished
	    }
	}

	return result;
}

PluginInstanceGUI* AirspyHFPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if (sourceId == m_deviceTypeID)
	{
	    AirspyHFGui* gui = new AirspyHFGui(deviceUISet);
		*widget = gui;
		return gui;
	}
	else
	{
		return 0;
	}
}

DeviceSampleSource *AirspyHFPlugin::createSampleSourcePluginInstanceInput(const QString& sourceId, DeviceSourceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
        AirspyHFInput* input = new AirspyHFInput(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}
