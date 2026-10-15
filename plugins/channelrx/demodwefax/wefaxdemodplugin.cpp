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

#include "wefaxdemodplugin.h"

#include <QtPlugin>

#include "plugin/pluginapi.h"
#include "wefaxdemod.h"
#include "wefaxdemodwebapiadapter.h"
#ifndef SERVER_MODE
#include "wefaxdemodgui.h"
#endif

const PluginDescriptor WefaxDemodPlugin::m_pluginDescriptor = {
    WefaxDemod::m_channelId,
    QStringLiteral("WEFAX Demodulator"),
    QStringLiteral("7.28.0"),
    QStringLiteral("(c) Jon Beniston, M7RCE"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

WefaxDemodPlugin::WefaxDemodPlugin(QObject *parent) :
    QObject(parent),
    m_pluginAPI(nullptr)
{}

const PluginDescriptor& WefaxDemodPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void WefaxDemodPlugin::initPlugin(PluginAPI *pluginAPI)
{
    m_pluginAPI = pluginAPI;
    m_pluginAPI->registerRxChannel(
        WefaxDemod::m_channelIdURI,
        WefaxDemod::m_channelId,
        this);
}

void WefaxDemodPlugin::createRxChannel(
    DeviceAPI *deviceAPI,
    BasebandSampleSink **basebandSink,
    ChannelAPI **channelAPI) const
{
    if (!basebandSink && !channelAPI) {
        return;
    }

    auto *instance = new WefaxDemod(deviceAPI);
    if (basebandSink) {
        *basebandSink = instance;
    }
    if (channelAPI) {
        *channelAPI = instance;
    }
}

ChannelGUI *WefaxDemodPlugin::createRxChannelGUI(
    DeviceUISet *deviceUISet,
    BasebandSampleSink *rxChannel) const
{
#ifdef SERVER_MODE
    (void) deviceUISet;
    (void) rxChannel;
    return nullptr;
#else
    return WefaxDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
#endif
}

ChannelWebAPIAdapter *WefaxDemodPlugin::createChannelWebAPIAdapter() const
{
    return new WefaxDemodWebAPIAdapter();
}
