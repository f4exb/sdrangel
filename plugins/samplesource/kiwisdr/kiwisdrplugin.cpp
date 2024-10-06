///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2019 Vort <vvort@yandex.ru>                                     //
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
#include "kiwisdrinput.h"
#else
#include "kiwisdrgui.h"
#endif
#include "kiwisdrplugin.h"
#include "kiwisdrwebapiadapter.h"

const PluginDescriptor KiwiSDRPlugin::m_pluginDescriptor = {
    QStringLiteral("KiwiSDR"),
	QStringLiteral("KiwiSDR input"),
    QStringLiteral("7.22.1"),
	QStringLiteral("(c) Vort (c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

static constexpr const char* const m_hardwareID = "KiwiSDR";
static constexpr const char* const m_deviceTypeID = KIWISDR_DEVICE_TYPE_ID;

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

void KiwiSDRPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) { // check if it was done
        return;
    }

    originDevices.append(OriginDevice(
        "KiwiSDR",
        m_hardwareID,
        QString(),
        0,
        1, // nb Rx
        0  // nb Tx
    ));

    listedHwIds.append(m_hardwareID);
}

PluginInterface::SamplingDevices KiwiSDRPlugin::enumSampleSources(const OriginDevices& originDevices)
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
                PluginInterface::SamplingDevice::BuiltInDevice,
                PluginInterface::SamplingDevice::StreamSingleRx,
                1,
                0
            ));
        }
    }

	return result;
}

#ifdef SERVER_MODE
DeviceGUI* KiwiSDRPlugin::createSampleSourcePluginInstanceGUI(
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
DeviceGUI* KiwiSDRPlugin::createSampleSourcePluginInstanceGUI(
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
