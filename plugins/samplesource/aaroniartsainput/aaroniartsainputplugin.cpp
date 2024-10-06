///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2020, 2022-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
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
#include "aaroniartsainput.h"
#else
#include "aaroniartsainputgui.h"
#endif
#include "aaroniartsainputplugin.h"
#include "aaroniartsainputwebapiadapter.h"

const PluginDescriptor AaroniaRTSAInputPlugin::m_pluginDescriptor = {
    QStringLiteral("AaroniaRTSA"),
	QStringLiteral("AaroniaRTSA input"),
    QStringLiteral("7.22.1"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

static constexpr const char* const m_hardwareID = "AaroniaRTSA";
static constexpr const char* const m_deviceTypeID = AARONIARTSA_DEVICE_TYPE_ID;

AaroniaRTSAInputPlugin::AaroniaRTSAInputPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& AaroniaRTSAInputPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void AaroniaRTSAInputPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

void AaroniaRTSAInputPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) { // check if it was done
        return;
    }

    originDevices.append(OriginDevice(
        "AaroniaRTSAInput",
        m_hardwareID,
        QString(),
        0,
        1, // nb Rx
        0  // nb Tx
    ));

    listedHwIds.append(m_hardwareID);
}

PluginInterface::SamplingDevices AaroniaRTSAInputPlugin::enumSampleSources(const OriginDevices& originDevices)
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
DeviceGUI* AaroniaRTSAInputPlugin::createSampleSourcePluginInstanceGUI(
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
DeviceGUI* AaroniaRTSAInputPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if(sourceId == m_deviceTypeID) {
		AaroniaRTSAInputGui* gui = new AaroniaRTSAInputGui(deviceUISet);
		*widget = gui;
		return gui;
	} else {
		return 0;
	}
}
#endif

DeviceSampleSource *AaroniaRTSAInputPlugin::createSampleSourcePluginInstance(const QString& sourceId, DeviceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
		AaroniaRTSAInput* input = new AaroniaRTSAInput(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}

DeviceWebAPIAdapter *AaroniaRTSAInputPlugin::createDeviceWebAPIAdapter() const
{
    return new AaroniaRTSAInputWebAPIAdapter();
}
