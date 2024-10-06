///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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
#include <mirisdr.h>
#include "plugin/pluginapi.h"

#ifdef SERVER_MODE
#include "sdrplayinput.h"
#else
#include "sdrplaygui.h"
#endif
#include "sdrplayplugin.h"
#include "sdrplaywebapiadapter.h"

const PluginDescriptor SDRPlayPlugin::m_pluginDescriptor = {
    QStringLiteral("SDRPlay"),
    QStringLiteral("SDRPlay RSP1 Input"),
    QStringLiteral("7.22.1"),
    QStringLiteral("(c) Edouard Griffiths, F4EXB"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

static constexpr const char* const m_hardwareID = "SDRplay1";
static constexpr const char* const m_deviceTypeID = SDRPLAY_DEVICE_TYPE_ID;

SDRPlayPlugin::SDRPlayPlugin(QObject* parent) :
    QObject(parent)
{
}

const PluginDescriptor& SDRPlayPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void SDRPlayPlugin::initPlugin(PluginAPI* pluginAPI)
{
    pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

void SDRPlayPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) { // check if it was done
        return;
    }

	int count = mirisdr_get_device_count();

	char vendor[256];
	char product[256];
	char serial[256];

	for(int i = 0; i < count; i++)
	{
		vendor[0] = '\0';
		product[0] = '\0';
		serial[0] = '\0';

		if (mirisdr_get_device_usb_strings((uint32_t)i, vendor, product, serial) != 0)
		{
			continue;
		}

		qDebug("SDRPlayPlugin::enumSampleSources: found %s:%s (%s)", vendor, product, serial);
		QString displayableName(QString("SDRPlay[%1] %2").arg(i).arg(serial));

        originDevices.append(OriginDevice(
            displayableName,
            m_hardwareID,
            serial,
            i, // sequence
            1, // Nb Rx
            0  // Nb Tx
        ));
	}

    listedHwIds.append(m_hardwareID);
}

PluginInterface::SamplingDevices SDRPlayPlugin::enumSampleSources(const OriginDevices& originDevices)
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
            qDebug("SDRPlayPlugin::enumSampleSources: enumerated SDRPlay RSP1 device #%d", it->sequence);
        }
    }

	return result;
}

#ifdef SERVER_MODE
DeviceGUI* SDRPlayPlugin::createSampleSourcePluginInstanceGUI(
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
DeviceGUI* SDRPlayPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    if(sourceId == m_deviceTypeID)
    {
        SDRPlayGui* gui = new SDRPlayGui(deviceUISet);
        *widget = gui;
        return gui;
    }
    else
    {
        return 0;
    }
}
#endif

DeviceSampleSource *SDRPlayPlugin::createSampleSourcePluginInstance(const QString& sourceId, DeviceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
        SDRPlayInput* input = new SDRPlayInput(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}

DeviceWebAPIAdapter *SDRPlayPlugin::createDeviceWebAPIAdapter() const
{
    return new SDRPlayWebAPIAdapter();
}
