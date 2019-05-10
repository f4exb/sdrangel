///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include "localsourceplugin.h"

#include <QtPlugin>
#include "plugin/pluginapi.h"

#ifndef SERVER_MODE
#include "localsourcegui.h"
#endif
#include "localsource.h"

const PluginDescriptor LocalSourcePlugin::m_pluginDescriptor = {
    QString("Local channel source"),
    QString("4.8.0"),
    QString("(c) Edouard Griffiths, F4EXB"),
    QString("https://github.com/f4exb/sdrangel"),
    true,
    QString("https://github.com/f4exb/sdrangel")
};

LocalSourcePlugin::LocalSourcePlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(0)
{
}

const PluginDescriptor& LocalSourcePlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void LocalSourcePlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    // register channel Source
    m_pluginAPI->registerTxChannel(LocalSource::m_channelIdURI, LocalSource::m_channelId, this);
}

#ifdef SERVER_MODE
PluginInstanceGUI* LocalSourcePlugin::createTxChannelGUI(
        DeviceUISet *deviceUISet __attribute__((unused)),
        BasebandSampleSource *txChannel __attribute__((unused)))
{
    return 0;
}
#else
PluginInstanceGUI* LocalSourcePlugin::createTxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSource *txChannel)
{
    return LocalSourceGUI::create(m_pluginAPI, deviceUISet, txChannel);
}
#endif

BasebandSampleSource* LocalSourcePlugin::createTxChannelBS(DeviceAPI *deviceAPI)
{
    return new LocalSource(deviceAPI);
}

ChannelAPI* LocalSourcePlugin::createTxChannelCS(DeviceAPI *deviceAPI)
{
    return new LocalSource(deviceAPI);
}
