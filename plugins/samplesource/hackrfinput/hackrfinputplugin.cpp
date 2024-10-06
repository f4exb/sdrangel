///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#include "plugin/pluginapi.h"

#ifdef SERVER_MODE
#include "hackrfinput.h"
#else
#include "hackrfinputgui.h"
#endif
#include "hackrfinputplugin.h"
#include "hackrfinputwebapiadapter.h"

const PluginDescriptor HackRFInputPlugin::m_pluginDescriptor = {
    QStringLiteral("HackRF"),
	QStringLiteral("HackRF Input"),
    QStringLiteral("7.22.1"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

static constexpr const char* const m_hardwareID = "HackRF";
static constexpr const char* const m_deviceTypeID = HACKRF_DEVICE_TYPE_ID;

HackRFInputPlugin::HackRFInputPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& HackRFInputPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void HackRFInputPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

void HackRFInputPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) { // check if it was done
        return;
    }

	DeviceHackRF::enumOriginDevices(m_hardwareID, originDevices);
	listedHwIds.append(m_hardwareID);
}

PluginInterface::SamplingDevices HackRFInputPlugin::enumSampleSources(const OriginDevices& originDevices)
{
	SamplingDevices result;

	for (OriginDevices::const_iterator it = originDevices.begin(); it != originDevices.end(); ++it)
    {
        if (it->hardwareId == m_hardwareID)
        {
			result.append(SamplingDevice(
				it->displayableName,
				it->hardwareId,
				m_deviceTypeID,
				it->serial,
				it->sequence,
				PluginInterface::SamplingDevice::PhysicalDevice,
				PluginInterface::SamplingDevice::StreamSingleRx,
				1,
				0
			));
		}
	}

	return result;
}

#ifdef SERVER_MODE
DeviceGUI* HackRFInputPlugin::createSampleSourcePluginInstanceGUI(
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
DeviceGUI* HackRFInputPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if(sourceId == m_deviceTypeID)
	{
		HackRFInputGui* gui = new HackRFInputGui(deviceUISet);
		*widget = gui;
		return gui;
	}
	else
	{
		return 0;
	}
}
#endif

DeviceSampleSource *HackRFInputPlugin::createSampleSourcePluginInstance(const QString& sourceId, DeviceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
        HackRFInput* input = new HackRFInput(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}

DeviceWebAPIAdapter *HackRFInputPlugin::createDeviceWebAPIAdapter() const
{
    return new HackRFInputWebAPIAdapter();
}
