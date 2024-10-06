///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2021 Jon Beniston, M7RCE <jon@beniston.com>                     //
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
#include <sdrplay_api.h>
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"

#ifdef SERVER_MODE
#include "sdrplayv3input.h"
#else
#include "sdrplayv3gui.h"
#endif
#include "sdrplayv3plugin.h"
#include "sdrplayv3webapiadapter.h"

const PluginDescriptor SDRPlayV3Plugin::m_pluginDescriptor = {
    QStringLiteral("SDRPlayV3"),
    QStringLiteral("SDRPlayV3 Input"),
    QStringLiteral("7.22.1"),
    QStringLiteral("(c) Jon Beniston, M7RCE and Edouard Griffiths, F4EXB"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

static constexpr const char* const m_hardwareID = "SDRplayV3";
static constexpr const char* const m_deviceTypeID = SDRPLAYV3_DEVICE_TYPE_ID;

SDRPlayV3Plugin::SDRPlayV3Plugin(QObject* parent) :
    QObject(parent),
    m_opened(false)
{
    sdrplay_api_ErrT err;
    float ver = 0.0f;

    if ((err = sdrplay_api_Open()) == sdrplay_api_Success)
    {
        m_opened = true;

        if ((err = sdrplay_api_ApiVersion(&ver)) == sdrplay_api_Success)
        {
            if (ver != SDRPLAY_API_VERSION) {
                qCritical() << "SDRPlayV3Plugin::SDRPlayV3Plugin: SDRPlay API versions do not match " << ver << " " << SDRPLAY_API_VERSION;
            }
        }
        else
        {
            qCritical() << "SDRPlayV3Plugin::SDRPlayV3Plugin: failed to get SDRPlay API version.";
        }
    }
    else
    {
        qCritical() << "SDRPlayV3Plugin::SDRPlayV3Plugin: sdrplay_api_Open() was unsuccessful. " << sdrplay_api_GetErrorString(err);
    }
}

SDRPlayV3Plugin::~SDRPlayV3Plugin()
{
    if (m_opened) {
        sdrplay_api_Close();
    }
}

const PluginDescriptor& SDRPlayV3Plugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void SDRPlayV3Plugin::initPlugin(PluginAPI* pluginAPI)
{
    pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

void SDRPlayV3Plugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) { // check if it was done
        return;
    }

    sdrplay_api_LockDeviceApi();

    sdrplay_api_ErrT err;
    sdrplay_api_DeviceT devs[SDRPLAY_MAX_DEVICES];
    unsigned int count;
    if ((err = sdrplay_api_GetDevices(devs, &count, sizeof(devs) / sizeof(sdrplay_api_DeviceT))) == sdrplay_api_Success)
    {
        for(unsigned int i = 0; i < count; i++)
        {
            qDebug("SDRPlayV3Plugin::enumSampleSources: found %d - %s", devs[i].hwVer, devs[i].SerNo);
            QString displayableName(QString("SDRPlayV3[%1] %2").arg(i).arg(devs[i].SerNo));

            originDevices.append(OriginDevice(
                displayableName,
                m_hardwareID,
                devs[i].SerNo,
                i, // sequence
                1, // Nb Rx
                0  // Nb Tx
            ));
        }

        listedHwIds.append(m_hardwareID);
    }
    else
        qCritical() << "SDRPlayV3Plugin::enumOriginDevices: Failed to get SDRPlay devices: " << sdrplay_api_GetErrorString(err);

    sdrplay_api_UnlockDeviceApi();
}

PluginInterface::SamplingDevices SDRPlayV3Plugin::enumSampleSources(const OriginDevices& originDevices)
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
            qDebug("SDRPlayV3Plugin::enumSampleSources: enumerated SDRPlayV3 RSP1 device #%d", it->sequence);
        }
    }

    return result;
}

#ifdef SERVER_MODE
DeviceGUI* SDRPlayV3Plugin::createSampleSourcePluginInstanceGUI(
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
DeviceGUI* SDRPlayV3Plugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    if(sourceId == m_deviceTypeID)
    {
        SDRPlayV3Gui* gui = new SDRPlayV3Gui(deviceUISet);
        *widget = gui;
        return gui;
    }
    else
    {
        return 0;
    }
}
#endif

DeviceSampleSource *SDRPlayV3Plugin::createSampleSourcePluginInstance(const QString& sourceId, DeviceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
        SDRPlayV3Input* input = new SDRPlayV3Input(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}

DeviceWebAPIAdapter *SDRPlayV3Plugin::createDeviceWebAPIAdapter() const
{
    return new SDRPlayV3WebAPIAdapter();
}
