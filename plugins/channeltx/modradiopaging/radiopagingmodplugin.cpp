///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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
#include "radiopagingmodgui.h"
#endif
#include "radiopagingmod.h"
#include "radiopagingmodwebapiadapter.h"
#include "radiopagingmodplugin.h"

const PluginDescriptor RadioPagingModPlugin::m_pluginDescriptor = {
    RadioPagingMod::m_channelId,
    QStringLiteral("Radio Paging Modulator"),
    QStringLiteral("8.1.0"),
    QStringLiteral("(c) Vasyl Samoilov"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

RadioPagingModPlugin::RadioPagingModPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(nullptr)
{
}

const PluginDescriptor& RadioPagingModPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void RadioPagingModPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;
    m_pluginAPI->registerTxChannel(RadioPagingMod::m_channelIdURI, RadioPagingMod::m_channelId, this);
}

void RadioPagingModPlugin::createTxChannel(DeviceAPI *deviceAPI, BasebandSampleSource **bs, ChannelAPI **cs) const
{
    if (bs || cs)
    {
        RadioPagingMod *instance = new RadioPagingMod(deviceAPI);

        if (bs) {
            *bs = instance;
        }

        if (cs) {
            *cs = instance;
        }
    }
}

#ifdef SERVER_MODE
ChannelGUI* RadioPagingModPlugin::createTxChannelGUI(
    DeviceUISet *deviceUISet,
    BasebandSampleSource *txChannel) const
{
    (void) deviceUISet;
    (void) txChannel;
    return nullptr;
}
#else
ChannelGUI* RadioPagingModPlugin::createTxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSource *txChannel) const
{
    return RadioPagingModGUI::create(m_pluginAPI, deviceUISet, txChannel);
}
#endif

ChannelWebAPIAdapter* RadioPagingModPlugin::createChannelWebAPIAdapter() const
{
    return new RadioPagingModWebAPIAdapter();
}
