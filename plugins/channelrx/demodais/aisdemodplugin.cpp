///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2019 Davide Gerhard <rainbow@irh.it>                            //
// Copyright (C) 2020-2021 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
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
#include "aisdemodgui.h"
#endif
#include "aisdemod.h"
#include "aisdemodwebapiadapter.h"
#include "aisdemodplugin.h"

const PluginDescriptor AISDemodPlugin::m_pluginDescriptor = {
    AISDemod::m_channelId,
    QStringLiteral("AIS Demodulator"),
    QStringLiteral("7.22.1"),
    QStringLiteral("(c) Jon Beniston, M7RCE"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

AISDemodPlugin::AISDemodPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(0)
{
}

const PluginDescriptor& AISDemodPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void AISDemodPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    m_pluginAPI->registerRxChannel(AISDemod::m_channelIdURI, AISDemod::m_channelId, this);
}

void AISDemodPlugin::createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const
{
    if (bs || cs)
    {
        AISDemod *instance = new AISDemod(deviceAPI);

        if (bs) {
            *bs = instance;
        }

        if (cs) {
            *cs = instance;
        }
    }
}

#ifdef SERVER_MODE
ChannelGUI* AISDemodPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
    (void) deviceUISet;
    (void) rxChannel;
    return 0;
}
#else
ChannelGUI* AISDemodPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
    return AISDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

ChannelWebAPIAdapter* AISDemodPlugin::createChannelWebAPIAdapter() const
{
    return new AISDemodWebAPIAdapter();
}
