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
#include "ieee_802_15_4_demodgui.h"
#endif
#include "ieee_802_15_4_demod.h"
#include "ieee_802_15_4_demodwebapiadapter.h"
#include "ieee_802_15_4_demodplugin.h"

const PluginDescriptor IEEE_802_15_4_DemodPlugin::m_pluginDescriptor = {
    IEEE_802_15_4_Demod::m_channelId,
    QStringLiteral("802.15.4 Demodulator"),
    QStringLiteral("8.0.0"),
    QStringLiteral("(c) Jon Beniston, M7RCE"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

IEEE_802_15_4_DemodPlugin::IEEE_802_15_4_DemodPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(nullptr)
{
}

const PluginDescriptor& IEEE_802_15_4_DemodPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void IEEE_802_15_4_DemodPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    m_pluginAPI->registerRxChannel(IEEE_802_15_4_Demod::m_channelIdURI, IEEE_802_15_4_Demod::m_channelId, this);
}

void IEEE_802_15_4_DemodPlugin::createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const
{
    if (bs || cs)
    {
        IEEE_802_15_4_Demod *instance = new IEEE_802_15_4_Demod(deviceAPI);

        if (bs) {
            *bs = instance;
        }

        if (cs) {
            *cs = instance;
        }
    }
}

#ifdef SERVER_MODE
ChannelGUI* IEEE_802_15_4_DemodPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
    (void) deviceUISet;
    (void) rxChannel;
    return nullptr;
}
#else
ChannelGUI* IEEE_802_15_4_DemodPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
    return IEEE_802_15_4_DemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

ChannelWebAPIAdapter* IEEE_802_15_4_DemodPlugin::createChannelWebAPIAdapter() const
{
    return new IEEE_802_15_4_DemodWebAPIAdapter();
}
