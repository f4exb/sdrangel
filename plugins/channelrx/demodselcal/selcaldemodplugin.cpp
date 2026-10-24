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
#include "selcaldemodgui.h"
#endif
#include "selcaldemod.h"
#include "selcaldemodwebapiadapter.h"
#include "selcaldemodplugin.h"

const PluginDescriptor SELCALDemodPlugin::m_pluginDescriptor = {
    SELCALDemod::m_channelId,
    QStringLiteral("SELCAL Demodulator"),
    QStringLiteral("8.0.0"),
    QStringLiteral("(c) Jon Beniston, M7RCE"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

SELCALDemodPlugin::SELCALDemodPlugin(QObject *parent) :
    QObject(parent),
    m_pluginAPI(0)
{
}

const PluginDescriptor& SELCALDemodPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void SELCALDemodPlugin::initPlugin(PluginAPI *pluginAPI)
{
    m_pluginAPI = pluginAPI;

    m_pluginAPI->registerRxChannel(SELCALDemod::m_channelIdURI, SELCALDemod::m_channelId, this);
}

void SELCALDemodPlugin::createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const
{
    if (bs || cs)
    {
        SELCALDemod *instance = new SELCALDemod(deviceAPI);

        if (bs) {
            *bs = instance;
        }

        if (cs) {
            *cs = instance;
        }
    }
}

#ifdef SERVER_MODE
ChannelGUI *SELCALDemodPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
    (void)deviceUISet;
    (void)rxChannel;
    return 0;
}
#else
ChannelGUI *SELCALDemodPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
    return SELCALDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

ChannelWebAPIAdapter *SELCALDemodPlugin::createChannelWebAPIAdapter() const
{
    return new SELCALDemodWebAPIAdapter();
}
