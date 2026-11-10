///////////////////////////////////////////////////////////////////////////////////
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
#include "dsemodgui.h"
#endif
#include "dsemod.h"
#include "dsemodwebapiadapter.h"
#include "dsemodplugin.h"

const PluginDescriptor DSEModPlugin::m_pluginDescriptor = {
    DSEMod::m_channelId,
    QStringLiteral("DSE Modulator"),
    QStringLiteral("8.2.0"),
    QStringLiteral("(c) Jon Beniston, M7RCE"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

DSEModPlugin::DSEModPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(nullptr)
{
}

const PluginDescriptor& DSEModPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void DSEModPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;
    m_pluginAPI->registerTxChannel(DSEMod::m_channelIdURI, DSEMod::m_channelId, this);
}

void DSEModPlugin::createTxChannel(DeviceAPI *deviceAPI, BasebandSampleSource **bs, ChannelAPI **cs) const
{
    if (bs || cs)
    {
        DSEMod *instance = new DSEMod(deviceAPI);

        if (bs) {
            *bs = instance;
        }

        if (cs) {
            *cs = instance;
        }
    }
}

#ifdef SERVER_MODE
ChannelGUI* DSEModPlugin::createTxChannelGUI(
    DeviceUISet *deviceUISet,
    BasebandSampleSource *txChannel) const
{
    (void) deviceUISet;
    (void) txChannel;
    return nullptr;
}
#else
ChannelGUI* DSEModPlugin::createTxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSource *txChannel) const
{
    return DSEModGUI::create(m_pluginAPI, deviceUISet, txChannel);
}
#endif

ChannelWebAPIAdapter* DSEModPlugin::createChannelWebAPIAdapter() const
{
    return new DSEModWebAPIAdapter();
}
