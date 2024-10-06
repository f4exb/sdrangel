///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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
#include "radioastronomygui.h"
#endif
#include "radioastronomy.h"
#include "radioastronomywebapiadapter.h"
#include "radioastronomyplugin.h"

const PluginDescriptor RadioAstronomyPlugin::m_pluginDescriptor = {
    RadioAstronomy::m_channelId,
    QStringLiteral("Radio Astronomy"),
    QStringLiteral("7.22.1"),
    QStringLiteral("(c) Jon Beniston, M7RCE"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

RadioAstronomyPlugin::RadioAstronomyPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(0)
{
}

const PluginDescriptor& RadioAstronomyPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void RadioAstronomyPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    m_pluginAPI->registerRxChannel(RadioAstronomy::m_channelIdURI, RadioAstronomy::m_channelId, this);
}

void RadioAstronomyPlugin::createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const
{
    if (bs || cs)
    {
        RadioAstronomy *instance = new RadioAstronomy(deviceAPI);

        if (bs) {
            *bs = instance;
        }

        if (cs) {
            *cs = instance;
        }
    }
}

#ifdef SERVER_MODE
ChannelGUI* RadioAstronomyPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
    (void) deviceUISet;
    (void) rxChannel;
    return 0;
}
#else
ChannelGUI* RadioAstronomyPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
    return RadioAstronomyGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

ChannelWebAPIAdapter* RadioAstronomyPlugin::createChannelWebAPIAdapter() const
{
    return new RadioAstronomyWebAPIAdapter();
}
