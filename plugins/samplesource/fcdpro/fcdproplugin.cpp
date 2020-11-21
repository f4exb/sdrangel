///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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
#include "util/simpleserializer.h"
#include "fcdproplugin.h"
#include "fcdprowebapiadapter.h"

#ifdef SERVER_MODE
#include "fcdproinput.h"
#else
#include "fcdprogui.h"
#endif
#include "fcdtraits.h"

const PluginDescriptor FCDProPlugin::m_pluginDescriptor = {
    QStringLiteral("FCDPro"),
	fcd_traits<Pro>::pluginDisplayedName,
	fcd_traits<Pro>::pluginVersion,
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

FCDProPlugin::FCDProPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& FCDProPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void FCDProPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSource(fcd_traits<Pro>::interfaceIID, this);
}

void FCDProPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(fcd_traits<Pro>::hardwareID)) { // check if it was done
        return;
    }

	int i = 0;
	struct hid_device_info *device_info = hid_enumerate(fcd_traits<Pro>::vendorId, fcd_traits<Pro>::productId);

	while (device_info != 0)
	{
		QString serialNumber = QString::fromWCharArray(device_info->serial_number);
		QString displayableName(QString("%1[%2] %3").arg(fcd_traits<Pro>::displayedName).arg(i).arg(serialNumber));

        originDevices.append(OriginDevice(
            displayableName,
            fcd_traits<Pro>::hardwareID,
            serialNumber,
            i,
            1, // nb Rx
            0  // nb Tx
        ));

		device_info = device_info->next;
		i++;
	}

    listedHwIds.append(fcd_traits<Pro>::hardwareID);
}

PluginInterface::SamplingDevices FCDProPlugin::enumSampleSources(const OriginDevices& originDevices)
{
	SamplingDevices result;

	for (OriginDevices::const_iterator it = originDevices.begin(); it != originDevices.end(); ++it)
    {
        if (it->hardwareId == fcd_traits<Pro>::hardwareID)
        {
            result.append(SamplingDevice(
                it->displayableName,
                fcd_traits<Pro>::hardwareID,
                fcd_traits<Pro>::interfaceIID,
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
DeviceGUI* FCDProPlugin::createSampleSourcePluginInstanceGUI(
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
DeviceGUI* FCDProPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if(sourceId == fcd_traits<Pro>::interfaceIID)
	{
		FCDProGui* gui = new FCDProGui(deviceUISet);
		*widget = gui;
		return gui;
	}
	else
	{
		return 0;
	}
}
#endif

DeviceSampleSource *FCDProPlugin::createSampleSourcePluginInstance(const QString& sourceId, DeviceAPI *deviceAPI)
{
    if (sourceId == fcd_traits<Pro>::interfaceIID)
    {
        FCDProInput* input = new FCDProInput(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}

DeviceWebAPIAdapter *FCDProPlugin::createDeviceWebAPIAdapter() const
{
    return new FCDProWebAPIAdapter();
}
