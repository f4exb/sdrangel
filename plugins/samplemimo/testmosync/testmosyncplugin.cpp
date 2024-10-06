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

#ifndef SERVER_MODE
#include "testmosyncgui.h"
#endif
#include "testmosync.h"
#include "testmosyncplugin.h"

const PluginDescriptor TestMOSyncPlugin::m_pluginDescriptor = {
    QStringLiteral("TestMOSync"),
	QStringLiteral("Test Synchronous Multiple Output"),
    QStringLiteral("7.22.1"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

static constexpr const char* const m_hardwareID = "TestMOSync";
static constexpr const char* const m_deviceTypeID = TESTMOSYNC_DEVICE_TYPE_ID;

TestMOSyncPlugin::TestMOSyncPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& TestMOSyncPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void TestMOSyncPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleMIMO(m_deviceTypeID, this);
}

void TestMOSyncPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) { // check if it was done
        return;
    }

    originDevices.append(OriginDevice(
        "TestMOSync",         // Displayable name
        m_hardwareID,     // Hardware ID
        QString(),        // Serial
        0,                // Sequence
        0,                // Number of Rx streams
        2                 // Number of Tx streams
    ));

    listedHwIds.append(m_hardwareID);
}

PluginInterface::SamplingDevices TestMOSyncPlugin::enumSampleMIMO(const OriginDevices& originDevices)
{
	SamplingDevices result;

    for (OriginDevices::const_iterator it = originDevices.begin(); it != originDevices.end(); ++it)
    {
        if (it->hardwareId == m_hardwareID)
        {
            result.append(SamplingDevice(
                "TestMOSync",
                m_hardwareID,
                m_deviceTypeID,
                it->serial,
                it->sequence,
                PluginInterface::SamplingDevice::BuiltInDevice,
                PluginInterface::SamplingDevice::StreamMIMO,
                1,    // MIMO is always considered as a single device
                0
            ));
        }
    }

	return result;
}

#ifdef SERVER_MODE
DeviceGUI* TestMOSyncPlugin::createSampleMIMOPluginInstanceGUI(
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
DeviceGUI* TestMOSyncPlugin::createSampleMIMOPluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if (sourceId == m_deviceTypeID)
    {
		TestMOSyncGui* gui = new TestMOSyncGui(deviceUISet);
		*widget = gui;
		return gui;
	}
    else
    {
		return nullptr;
	}
}
#endif

DeviceSampleMIMO *TestMOSyncPlugin::createSampleMIMOPluginInstance(const QString& mimoId, DeviceAPI *deviceAPI)
{
    if (mimoId == m_deviceTypeID)
    {
        TestMOSync* output = new TestMOSync(deviceAPI);
        return output;
    }
    else
    {
        return nullptr;
    }
}

DeviceWebAPIAdapter *TestMOSyncPlugin::createDeviceWebAPIAdapter() const
{
    return nullptr;
}
