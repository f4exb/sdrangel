///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2019 Davide Gerhard <rainbow@irh.it>                            //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
// Copyright (C) 2017 Sergey Kostanbaev, Fairwaves Inc.                          //
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

#include "xtrxinputplugin.h"
#include "xtrxinputwebapiadapter.h"

#include <QtPlugin>

#include <regex>
#include <string>

#include "xtrx_api.h"
#include "plugin/pluginapi.h"
#include "xtrx/devicextrx.h"

#ifdef SERVER_MODE
#include "xtrxinput.h"
#else
#include "xtrxinputgui.h"
#endif

const PluginDescriptor XTRXInputPlugin::m_pluginDescriptor = {
    QStringLiteral("XTRX"),
    QStringLiteral("XTRX Input"),
    QStringLiteral("7.22.1"),
    QStringLiteral("(c) Edouard Griffiths, F4EXB"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

static constexpr const char* const m_hardwareID = "XTRX";
static constexpr const char* const m_deviceTypeID = XTRX_DEVICE_TYPE_ID;

XTRXInputPlugin::XTRXInputPlugin(QObject* parent) :
    QObject(parent)
{
}

const PluginDescriptor& XTRXInputPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void XTRXInputPlugin::initPlugin(PluginAPI* pluginAPI)
{
    pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

void XTRXInputPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) { // check if it was done
        return;
    }

    DeviceXTRX::enumOriginDevices(m_hardwareID, originDevices);
    listedHwIds.append(m_hardwareID);
}

PluginInterface::SamplingDevices XTRXInputPlugin::enumSampleSources(const OriginDevices& originDevices)
{
    SamplingDevices result;

	for (OriginDevices::const_iterator it = originDevices.begin(); it != originDevices.end(); ++it)
    {
        if (it->hardwareId == m_hardwareID)
        {
            for (int j = 0; j < it->nbRxStreams; j++)
            {
                qDebug("XTRXInputPlugin::enumSampleSources: device #%d channel %u: %s", it->sequence, j, qPrintable(it->serial));
                QString displayedName = it->displayableName;
                displayedName.replace(QString("$1]"), QString("%1]").arg(j));
                result.append(SamplingDevice(
                    displayedName,
                    it->hardwareId,
                    m_deviceTypeID,
                    it->serial,
                    it->sequence,
                    PluginInterface::SamplingDevice::PhysicalDevice,
                    PluginInterface::SamplingDevice::StreamSingleRx,
                    it->nbRxStreams,
                    j
                ));
            }
        }
    }

    return result;
}

#ifdef SERVER_MODE
DeviceGUI* XTRXInputPlugin::createSampleSourcePluginInstanceGUI(
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
DeviceGUI* XTRXInputPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    if(sourceId == m_deviceTypeID)
    {
        XTRXInputGUI* gui = new XTRXInputGUI(deviceUISet);
        *widget = gui;
        return gui;
    }
    else
    {
        return 0;
    }
}
#endif

DeviceSampleSource *XTRXInputPlugin::createSampleSourcePluginInstance(const QString& sourceId, DeviceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
        XTRXInput* input = new XTRXInput(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}

DeviceWebAPIAdapter *XTRXInputPlugin::createDeviceWebAPIAdapter() const
{
    return new XTRXInputWebAPIAdapter();
}
