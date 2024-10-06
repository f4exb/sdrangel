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

#include "bladerf1inputplugin.h"

#include <QtPlugin>
#include <libbladeRF.h>
#include "plugin/pluginapi.h"
#include "bladerf1inputwebapiadapter.h"

#ifdef SERVER_MODE
#include "bladerf1input.h"
#else
#include "bladerf1inputgui.h"
#endif

const PluginDescriptor Blderf1InputPlugin::m_pluginDescriptor = {
    QStringLiteral("BladeRF1"),
	QStringLiteral("BladeRF1 Input"),
    QStringLiteral("7.22.1"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

static constexpr const char* const m_hardwareID = "BladeRF1";
static constexpr const char* const m_deviceTypeID = BLADERF1INPUT_DEVICE_TYPE_ID;

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

    DeviceBladeRF1::enumOriginDevices(m_hardwareID, originDevices);
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
DeviceGUI* Blderf1InputPlugin::createSampleSourcePluginInstanceGUI(
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
DeviceGUI* Blderf1InputPlugin::createSampleSourcePluginInstanceGUI(
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
