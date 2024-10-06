///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2020, 2022-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
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
#include "plutosdr/deviceplutosdr.h"

#ifdef SERVER_MODE
#include "plutosdrinput.h"
#else
#include "plutosdrinputgui.h"
#endif
#include "plutosdrinputplugin.h"
#include "plutosdrinputwebapiadapter.h"

class DeviceAPI;

const PluginDescriptor PlutoSDRInputPlugin::m_pluginDescriptor = {
    QStringLiteral("PlutoSDR"),
	QStringLiteral("PlutoSDR Input"),
    QStringLiteral("7.22.1"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

static constexpr const char* const m_hardwareID = "PlutoSDR";
const char* const PlutoSDRInputPlugin::m_deviceTypeID = PLUTOSDR_DEVICE_TYPE_ID;

PlutoSDRInputPlugin::PlutoSDRInputPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& PlutoSDRInputPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void PlutoSDRInputPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSource(m_deviceTypeID, this);
	DevicePlutoSDR::instance(); // create singleton
}

void PlutoSDRInputPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) { // check if it was done
        return;
    }

    DevicePlutoSDR::instance().enumOriginDevices(m_hardwareID, originDevices);
    listedHwIds.append(m_hardwareID);
}

PluginInterface::SamplingDevices PlutoSDRInputPlugin::enumSampleSources(const OriginDevices& originDevices)
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
            qDebug("PlutoSDRInputPlugin::enumSampleSources: enumerated PlutoSDR device #%d", it->sequence);
        }
    }

	return result;
}

#ifdef SERVER_MODE
DeviceGUI* PlutoSDRInputPlugin::createSampleSourcePluginInstanceGUI(
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
DeviceGUI* PlutoSDRInputPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if(sourceId == m_deviceTypeID)
	{
		PlutoSDRInputGui* gui = new PlutoSDRInputGui(deviceUISet);
		*widget = gui;
		return gui;
	}
	else
	{
		return 0;
	}
}
#endif

DeviceSampleSource *PlutoSDRInputPlugin::createSampleSourcePluginInstance(const QString& sourceId, DeviceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
        PlutoSDRInput* input = new PlutoSDRInput(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}

DeviceWebAPIAdapter *PlutoSDRInputPlugin::createDeviceWebAPIAdapter() const
{
    return new PlutoSDRInputWebAPIAdapter();
}
