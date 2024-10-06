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
#include "plutosdroutput.h"
#else
#include "plutosdroutputgui.h"
#endif
#include "plutosdroutputplugin.h"
#include "plutosdroutputwebapiadapter.h"

const PluginDescriptor PlutoSDROutputPlugin::m_pluginDescriptor = {
    QStringLiteral("PlutoSDR"),
	QStringLiteral("PlutoSDR Output"),
    QStringLiteral("7.22.1"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

static constexpr const char* const m_hardwareID = "PlutoSDR";
const char* const PlutoSDROutputPlugin::m_deviceTypeID = PLUTOSDR_DEVICE_TYPE_ID;

PlutoSDROutputPlugin::PlutoSDROutputPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& PlutoSDROutputPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void PlutoSDROutputPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSink(m_deviceTypeID, this);
	DevicePlutoSDR::instance(); // create singleton
}

void PlutoSDROutputPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) { // check if it was done
        return;
    }

    DevicePlutoSDR::instance().enumOriginDevices(m_hardwareID, originDevices);
    listedHwIds.append(m_hardwareID);
}

PluginInterface::SamplingDevices PlutoSDROutputPlugin::enumSampleSinks(const OriginDevices& originDevices)
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
                PluginInterface::SamplingDevice::StreamSingleTx,
                1,
                0
            ));
            qDebug("PlutoSDROutputPlugin::enumSampleSources: enumerated PlutoSDR device #%d", it->sequence);
        }
    }

	return result;
}

#ifdef SERVER_MODE
DeviceGUI* PlutoSDROutputPlugin::createSampleSinkPluginInstanceGUI(
        const QString& sinkId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    (void) sinkId;
    (void) widget;
    (void) deviceUISet;
    return 0;
}
#else
DeviceGUI* PlutoSDROutputPlugin::createSampleSinkPluginInstanceGUI(
        const QString& sinkId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if(sinkId == m_deviceTypeID)
	{
        PlutoSDROutputGUI* gui = new PlutoSDROutputGUI(deviceUISet);
		*widget = gui;
		return gui;
	}
	else
	{
		return 0;
	}
}
#endif

DeviceSampleSink *PlutoSDROutputPlugin::createSampleSinkPluginInstance(const QString& sinkId, DeviceAPI *deviceAPI)
{
    if (sinkId == m_deviceTypeID)
    {
        PlutoSDROutput* output = new PlutoSDROutput(deviceAPI);
        return output;
    }
    else
    {
        return 0;
    }
}

DeviceWebAPIAdapter *PlutoSDROutputPlugin::createDeviceWebAPIAdapter() const
{
    return new PlutoSDROutputWebAPIAdapter();
}
