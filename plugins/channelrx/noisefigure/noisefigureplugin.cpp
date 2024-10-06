///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2019 Davide Gerhard <rainbow@irh.it>                            //
// Copyright (C) 2020-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
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
#include "noisefiguregui.h"
#endif
#include "noisefigure.h"
#include "noisefigurewebapiadapter.h"
#include "noisefigureplugin.h"

const PluginDescriptor NoiseFigurePlugin::m_pluginDescriptor = {
    NoiseFigure::m_channelId,
    QStringLiteral("Noise Figure"),
    QStringLiteral("7.22.1"),
    QStringLiteral("(c) Jon Beniston, M7RCE"),
    QStringLiteral("https://github.com/f4exb/sdrangel"),
    true,
    QStringLiteral("https://github.com/f4exb/sdrangel")
};

NoiseFigurePlugin::NoiseFigurePlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(0)
{
}

const PluginDescriptor& NoiseFigurePlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void NoiseFigurePlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    m_pluginAPI->registerRxChannel(NoiseFigure::m_channelIdURI, NoiseFigure::m_channelId, this);
}

void NoiseFigurePlugin::createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const
{
    if (bs || cs)
    {
        NoiseFigure *instance = new NoiseFigure(deviceAPI);

        if (bs) {
            *bs = instance;
        }

        if (cs) {
            *cs = instance;
        }
    }
}

#ifdef SERVER_MODE
ChannelGUI* NoiseFigurePlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
    (void) deviceUISet;
    (void) rxChannel;
    return 0;
}
#else
ChannelGUI* NoiseFigurePlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
    return NoiseFigureGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

ChannelWebAPIAdapter* NoiseFigurePlugin::createChannelWebAPIAdapter() const
{
    return new NoiseFigureWebAPIAdapter();
}
