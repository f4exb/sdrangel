///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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
#include "bfmmod.h"
#include "bfmmodwebapiadapter.h"
#ifndef SERVER_MODE
#include "bfmmodgui.h"
#endif
#include "bfmmodplugin.h"

namespace {
const char *channelIdURI = "sdrangel.channeltx.modbfm";
const char *channelId = "BFMMod";
}

const PluginDescriptor BFMModPlugin::m_pluginDescriptor = {
    channelId,
    QStringLiteral("Broadcast FM Modulator"),
    QStringLiteral("7.28.0"),
    QStringLiteral("(c) Jon Beniston, M7RCE"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

BFMModPlugin::BFMModPlugin(QObject *parent) :
    QObject(parent),
    m_pluginAPI(nullptr)
{}

const PluginDescriptor& BFMModPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void BFMModPlugin::initPlugin(PluginAPI *pluginAPI)
{
    m_pluginAPI = pluginAPI;
    m_pluginAPI->registerTxChannel(channelIdURI, channelId, this);
}

void BFMModPlugin::createTxChannel(DeviceAPI *deviceAPI, BasebandSampleSource **bs, ChannelAPI **cs) const
{
    if (!bs && !cs) {
        return;
    }

    BFMMod *instance = new BFMMod(deviceAPI);
    if (bs) {
        *bs = instance;
    }
    if (cs) {
        *cs = instance;
    }
}

#ifdef SERVER_MODE
ChannelGUI* BFMModPlugin::createTxChannelGUI(DeviceUISet *, BasebandSampleSource *) const
{
    return nullptr;
}
#else
ChannelGUI* BFMModPlugin::createTxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSource *txChannel) const
{
    return BFMModGUI::create(m_pluginAPI, deviceUISet, txChannel);
}
#endif

ChannelWebAPIAdapter* BFMModPlugin::createChannelWebAPIAdapter() const
{
    return new BFMModWebAPIAdapter();
}
