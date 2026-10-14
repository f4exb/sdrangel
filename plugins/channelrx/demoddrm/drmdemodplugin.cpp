///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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
#include "drmdemodgui.h"
#endif
#include "drmdemod.h"
#include "drmdemodwebapiadapter.h"
#include "drmdemodplugin.h"

const PluginDescriptor DRMDemodPlugin::m_pluginDescriptor = {
    DRMDemod::m_channelId,
    QStringLiteral("DRM Demodulator"),
    QStringLiteral("7.28.0"),
    QStringLiteral("(c) Jon Beniston, M7RCE"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

DRMDemodPlugin::DRMDemodPlugin(QObject *parent) :
    QObject(parent),
    m_pluginAPI(nullptr)
{
}

const PluginDescriptor& DRMDemodPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void DRMDemodPlugin::initPlugin(PluginAPI *pluginAPI)
{
    m_pluginAPI = pluginAPI;
    m_pluginAPI->registerRxChannel(DRMDemod::m_channelIdURI, DRMDemod::m_channelId, this);
}

void DRMDemodPlugin::createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **basebandSink, ChannelAPI **channelAPI) const
{
    if (!basebandSink && !channelAPI) {
        return;
    }

    DRMDemod *instance = new DRMDemod(deviceAPI);
    if (basebandSink) {
        *basebandSink = instance;
    }

    if (channelAPI) {
        *channelAPI = instance;
    }
}

#ifdef SERVER_MODE
ChannelGUI* DRMDemodPlugin::createRxChannelGUI(
    DeviceUISet *deviceUISet,
    BasebandSampleSink *rxChannel
) const
{
    (void) deviceUISet;
    (void) rxChannel;
    return nullptr;
}
#else
ChannelGUI* DRMDemodPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
    return DRMDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

ChannelWebAPIAdapter* DRMDemodPlugin::createChannelWebAPIAdapter() const
{
    return new DRMDemodWebAPIAdapter();
}
