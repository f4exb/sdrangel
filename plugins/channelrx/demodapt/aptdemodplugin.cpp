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
#include "aptdemodgui.h"
#endif
#include "aptdemod.h"
#include "aptdemodwebapiadapter.h"
#include "aptdemodplugin.h"

const PluginDescriptor APTDemodPlugin::m_pluginDescriptor = {
    APTDemod::m_channelId,
    QStringLiteral("APT Demodulator"),
    QStringLiteral("7.22.1"),
    QStringLiteral("(c) Jon Beniston, M7RCE and Aptdec authors"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

APTDemodPlugin::APTDemodPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(0)
{
}

const PluginDescriptor& APTDemodPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void APTDemodPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    m_pluginAPI->registerRxChannel(APTDemod::m_channelIdURI, APTDemod::m_channelId, this);
}

void APTDemodPlugin::createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const
{
    if (bs || cs)
    {
        APTDemod *instance = new APTDemod(deviceAPI);

        if (bs) {
            *bs = instance;
        }

        if (cs) {
            *cs = instance;
        }
    }
}

#ifdef SERVER_MODE
ChannelGUI* APTDemodPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
    (void) deviceUISet;
    (void) rxChannel;
    return 0;
}
#else
ChannelGUI* APTDemodPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
    return APTDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

ChannelWebAPIAdapter* APTDemodPlugin::createChannelWebAPIAdapter() const
{
    return new APTDemodWebAPIAdapter();
}
