///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Vort                                   //
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
#include "kiwisdrinput.h"
#else
#include "kiwisdrgui.h"
#endif
#include "kiwisdrplugin.h"
#include "kiwisdrwebapiadapter.h"

const PluginDescriptor KiwiSDRPlugin::m_pluginDescriptor = {
	QString("KiwiSDR input"),
	QString("4.11.6"),
	QString("(c) Vort (c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

const QString KiwiSDRPlugin::m_hardwareID = "KiwiSDR";
const QString KiwiSDRPlugin::m_deviceTypeID = KIWISDR_DEVICE_TYPE_ID;

KiwiSDRPlugin::KiwiSDRPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& KiwiSDRPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void KiwiSDRPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

PluginInterface::SamplingDevices KiwiSDRPlugin::enumSampleSources()
{
	SamplingDevices result;

    result.append(SamplingDevice(
            "KiwiSDR",
            m_hardwareID,
            m_deviceTypeID,
            QString::null,
            0,
            PluginInterface::SamplingDevice::BuiltInDevice,
            PluginInterface::SamplingDevice::StreamSingleRx,
            1,
            0));

	return result;
}

#ifdef SERVER_MODE
PluginInstanceGUI* KiwiSDRPlugin::createSampleSourcePluginInstanceGUI(
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
PluginInstanceGUI* KiwiSDRPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if(sourceId == m_deviceTypeID) {
		KiwiSDRGui* gui = new KiwiSDRGui(deviceUISet);
		*widget = gui;
		return gui;
	} else {
		return 0;
	}
}
#endif

DeviceSampleSource *KiwiSDRPlugin::createSampleSourcePluginInstance(const QString& sourceId, DeviceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
		KiwiSDRInput* input = new KiwiSDRInput(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}

DeviceWebAPIAdapter *KiwiSDRPlugin::createDeviceWebAPIAdapter() const
{
    return new KiwiSDRWebAPIAdapter();
}
