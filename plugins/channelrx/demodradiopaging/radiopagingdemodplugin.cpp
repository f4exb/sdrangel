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
#include "radiopagingdemodgui.h"
#endif
#include "radiopagingdemod.h"
#include "radiopagingdemodwebapiadapter.h"
#include "radiopagingdemodplugin.h"

const PluginDescriptor RadioPagingDemodPlugin::m_pluginDescriptor = {
    RadioPagingDemod::m_channelId,
    QStringLiteral("Radio Paging Demodulator"),
    QStringLiteral("8.1.0"),
    QStringLiteral("(c) Vasyl Samoilov"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

RadioPagingDemodPlugin::RadioPagingDemodPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(nullptr)
{
}

const PluginDescriptor& RadioPagingDemodPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void RadioPagingDemodPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    m_pluginAPI->registerRxChannel(RadioPagingDemod::m_channelIdURI, RadioPagingDemod::m_channelId, this);
}

void RadioPagingDemodPlugin::createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const
{
    if (bs || cs)
    {
        RadioPagingDemod *instance = new RadioPagingDemod(deviceAPI);

        if (bs) {
            *bs = instance;
        }

        if (cs) {
            *cs = instance;
        }
    }
}

#ifdef SERVER_MODE
ChannelGUI* RadioPagingDemodPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
    (void) deviceUISet;
    (void) rxChannel;
    return nullptr;
}
#else
ChannelGUI* RadioPagingDemodPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
    return RadioPagingDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

ChannelWebAPIAdapter* RadioPagingDemodPlugin::createChannelWebAPIAdapter() const
{
    return new RadioPagingDemodWebAPIAdapter();
}
