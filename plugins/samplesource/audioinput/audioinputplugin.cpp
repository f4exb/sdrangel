///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2020, 2022-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
// Copyright (C) 2019 Davide Gerhard <rainbow@irh.it>                            //
// Copyright (C) 2020, 2022 Jon Beniston, M7RCE <jon@beniston.com>               //
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
#include "audioinputplugin.h"
#include "audioinputwebapiadapter.h"

#ifdef SERVER_MODE
#include "audioinput.h"
#else
#include "audioinputgui.h"
#endif

const PluginDescriptor AudioInputPlugin::m_pluginDescriptor = {
    QStringLiteral("AudioInput"),
    QStringLiteral("Audio Input"),
    QStringLiteral("7.22.1"),
    QStringLiteral("(c) Jon Beniston, M7RCE and Edouard Griffiths, F4EXB"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

static constexpr const char* const m_hardwareID = "AudioInput";
static constexpr const char* const m_deviceTypeID = AUDIOINPUT_DEVICE_TYPE_ID;

AudioInputPlugin::AudioInputPlugin(QObject* parent) :
    QObject(parent)
{
}

const PluginDescriptor& AudioInputPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void AudioInputPlugin::initPlugin(PluginAPI* pluginAPI)
{
    pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

void AudioInputPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) { // check if it was done
        return;
    }

    // We could list all input audio devices here separately
    // but I thought it makes it simpler to switch between inputs
    // if they are in the AudioInput GUI
    originDevices.append(OriginDevice(
        "AudioInput",
        m_hardwareID,
        "0",
        0,
        1, // nb Rx
        0  // nb Tx
    ));
    listedHwIds.append(m_hardwareID);
}

PluginInterface::SamplingDevices AudioInputPlugin::enumSampleSources(const OriginDevices& originDevices)
{
    SamplingDevices result;

    for (OriginDevices::const_iterator it = originDevices.begin(); it != originDevices.end(); ++it)
    {
        if (it->hardwareId == m_hardwareID)
        {
            for (int j = 0; j < it->nbRxStreams; j++)
            {
                result.append(SamplingDevice(
                    it->displayableName,
                    it->hardwareId,
                    m_deviceTypeID,
                    it->serial,
                    it->sequence,
                    PluginInterface::SamplingDevice::BuiltInDevice,
                    PluginInterface::SamplingDevice::StreamSingleRx,
                    it->nbRxStreams,
                    j));
            }
        }
    }

    return result;
}

#ifdef SERVER_MODE
DeviceGUI* AudioInputPlugin::createSampleSourcePluginInstanceGUI(
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
DeviceGUI* AudioInputPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    if(sourceId == m_deviceTypeID)
    {
        AudioInputGui* gui = new AudioInputGui(deviceUISet);
        *widget = gui;
        return gui;
    }
    else
    {
        return 0;
    }
}
#endif

DeviceSampleSource *AudioInputPlugin::createSampleSourcePluginInstance(const QString& sourceId, DeviceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
        AudioInput* input = new AudioInput(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}

DeviceWebAPIAdapter *AudioInputPlugin::createDeviceWebAPIAdapter() const
{
    return new AudioInputWebAPIAdapter();
}
