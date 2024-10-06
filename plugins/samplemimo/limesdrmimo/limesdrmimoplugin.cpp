///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
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

#include <QtPlugin>

#include "plugin/pluginapi.h"
#include "limesdr/devicelimesdr.h"

#ifndef SERVER_MODE
#include "limesdrmimogui.h"
#endif
#include "limesdrmimo.h"
#include "limesdrmimoplugin.h"
#include "limesdrmimowebapiadapter.h"

const PluginDescriptor LimeSDRMIMOPlugin::m_pluginDescriptor = {
    QStringLiteral("LimeSDR"),
	QStringLiteral("LimeSDR MIMO"),
    QStringLiteral("7.22.1"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

static constexpr const char* const m_hardwareID = "LimeSDR";
static constexpr const char* const m_deviceTypeID = LIMESDRMIMO_DEVICE_TYPE_ID;

LimeSDRMIMOPlugin::LimeSDRMIMOPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& LimeSDRMIMOPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void LimeSDRMIMOPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleMIMO(m_deviceTypeID, this);
}

void LimeSDRMIMOPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) { // check if it was done
        return;
    }

    DeviceLimeSDR::enumOriginDevices(m_hardwareID, originDevices);
    listedHwIds.append(m_hardwareID);
}

PluginInterface::SamplingDevices LimeSDRMIMOPlugin::enumSampleMIMO(const OriginDevices& originDevices)
{
    SamplingDevices result;

	for (OriginDevices::const_iterator it = originDevices.begin(); it != originDevices.end(); ++it)
    {
        if (it->hardwareId == m_hardwareID)
        {
            QString displayedName = it->displayableName;
            displayedName.replace(QString(":$1]"), QString("]"));
            result.append(SamplingDevice(
                displayedName,
                m_hardwareID,
                m_deviceTypeID,
                it->serial,
                it->sequence,
                PluginInterface::SamplingDevice::PhysicalDevice,
                PluginInterface::SamplingDevice::StreamMIMO,
                1,
                0
            ));
        }
    }

    return result;
}

#ifdef SERVER_MODE
DeviceGUI* LimeSDRMIMOPlugin::createSampleMIMOPluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    (void) sourceId;
    (void) widget;
    (void) deviceUISet;
    return nullptr;
}
#else
DeviceGUI* LimeSDRMIMOPlugin::createSampleMIMOPluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if (sourceId == m_deviceTypeID)
    {
		LimeSDRMIMOGUI* gui = new LimeSDRMIMOGUI(deviceUISet);
		*widget = gui;
		return gui;
	}
    else
    {
		return nullptr;
	}
}
#endif

DeviceSampleMIMO *LimeSDRMIMOPlugin::createSampleMIMOPluginInstance(const QString& mimoId, DeviceAPI *deviceAPI)
{
    if (mimoId == m_deviceTypeID)
    {
        LimeSDRMIMO* input = new LimeSDRMIMO(deviceAPI);
        return input;
    }
    else
    {
        return nullptr;
    }
}

DeviceWebAPIAdapter *LimeSDRMIMOPlugin::createDeviceWebAPIAdapter() const
{
    return new LimeSDRMIMOWebAPIAdapter();
}
