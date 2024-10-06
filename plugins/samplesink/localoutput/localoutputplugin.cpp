///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
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
#include "localoutput.h"
#else
#include "localoutputgui.h"
#endif
#include "localoutputplugin.h"
#include "localoutputwebapiadapter.h"

const PluginDescriptor LocalOutputPlugin::m_pluginDescriptor = {
    QStringLiteral("LocalOutput"),
	QStringLiteral("Local device output"),
    QStringLiteral("7.22.1"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

static constexpr const char* const m_hardwareID = "LocalOutput";
static constexpr const char* const m_deviceTypeID = LOCALOUTPUT_DEVICE_TYPE_ID;

LocalOutputPlugin::LocalOutputPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& LocalOutputPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void LocalOutputPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSink(m_deviceTypeID, this);
}

void LocalOutputPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) { // check if it was done
        return;
    }

    originDevices.append(OriginDevice(
        "LocalOutput",
        m_hardwareID,
        QString(),
        0, // Sequence
        0, // nb Rx
        1  // nb Tx
    ));

    listedHwIds.append(m_hardwareID);
}

PluginInterface::SamplingDevices LocalOutputPlugin::enumSampleSinks(const OriginDevices& originDevices)
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
                PluginInterface::SamplingDevice::BuiltInDevice,
                PluginInterface::SamplingDevice::StreamSingleTx,
                1,
                0
            ));
        }
    }

	return result;
}

#ifdef SERVER_MODE
DeviceGUI* LocalOutputPlugin::createSampleSinkPluginInstanceGUI(
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
DeviceGUI* LocalOutputPlugin::createSampleSinkPluginInstanceGUI(
        const QString& sinkId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if(sinkId == m_deviceTypeID)
	{
		LocalOutputGui* gui = new LocalOutputGui(deviceUISet);
		*widget = gui;
		return gui;
	}
	else
	{
		return 0;
	}
}
#endif

DeviceSampleSink *LocalOutputPlugin::createSampleSinkPluginInstance(const QString& sinkId, DeviceAPI *deviceAPI)
{
    if (sinkId == m_deviceTypeID)
    {
        LocalOutput* output = new LocalOutput(deviceAPI);
        return output;
    }
    else
    {
        return 0;
    }
}

DeviceWebAPIAdapter *LocalOutputPlugin::createDeviceWebAPIAdapter() const
{
    return new LocalOutputWebAPIAdapter();
}
