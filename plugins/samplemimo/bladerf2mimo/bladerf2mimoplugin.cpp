///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifdef SERVER_MODE
#include "bladerf2mimo.h"
#else
#include "bladerf2mimogui.h"
#include "bladerf2mimo.h" // TODO
#endif
#include "bladerf2mimoplugin.h"
//#include "testmiwebapiadapter.h"

const PluginDescriptor BladeRF2MIMOPlugin::m_pluginDescriptor = {
	QString("BladeRF2 MIMO"),
	QString("4.12.0"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

const QString BladeRF2MIMOPlugin::m_hardwareID = "BladeRF2";
const QString BladeRF2MIMOPlugin::m_deviceTypeID = BLADERF2MIMO_DEVICE_TYPE_ID;

BladeRF2MIMOPlugin::BladeRF2MIMOPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& BladeRF2MIMOPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void BladeRF2MIMOPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleMIMO(m_deviceTypeID, this);
}

void BladeRF2MIMOPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) { // check if it was done
        return;
    }

    DeviceBladeRF2::enumOriginDevices(m_hardwareID, originDevices);
    listedHwIds.append(m_hardwareID);
}

PluginInterface::SamplingDevices BladeRF2MIMOPlugin::enumSampleMIMO(const OriginDevices& originDevices)
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
PluginInstanceGUI* BladeRF2MIMOPlugin::createSampleMIMOPluginInstanceGUI(
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
PluginInstanceGUI* BladeRF2MIMOPlugin::createSampleMIMOPluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if (sourceId == m_deviceTypeID)
    {
		BladeRF2MIMOGui* gui = new BladeRF2MIMOGui(deviceUISet);
		*widget = gui;
		return gui;
	}
    else
    {
		return nullptr;
	}
}
#endif

DeviceSampleMIMO *BladeRF2MIMOPlugin::createSampleMIMOPluginInstance(const QString& mimoId, DeviceAPI *deviceAPI)
{
    if (mimoId == m_deviceTypeID)
    {
        BladeRF2MIMO* input = new BladeRF2MIMO(deviceAPI);
        return input;
    }
    else
    {
        return nullptr;
    }
}

DeviceWebAPIAdapter *BladeRF2MIMOPlugin::createDeviceWebAPIAdapter() const
{
    // TODO
    //return new BladeRF2MIMOWebAPIAdapter();
    return nullptr;
}
