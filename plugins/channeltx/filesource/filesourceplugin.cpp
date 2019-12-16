///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2019 Edouard Griffiths, F4EXB                              //
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
#include "filesourcegui.h"
#endif
#include "filesource.h"
#include "filesourcewebapiadapter.h"
#include "filesourceplugin.h"

const PluginDescriptor FileSourcePlugin::m_pluginDescriptor = {
    FileSource::m_channelId,
    QString("File channel source"),
    QString("4.12.3"),
    QString("(c) Edouard Griffiths, F4EXB"),
    QString("https://github.com/f4exb/sdrangel"),
    true,
    QString("https://github.com/f4exb/sdrangel")
};

FileSourcePlugin::FileSourcePlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(0)
{
}

const PluginDescriptor& FileSourcePlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void FileSourcePlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    // register source
    m_pluginAPI->registerTxChannel(FileSource::m_channelIdURI, FileSource::m_channelId, this);
}

#ifdef SERVER_MODE
PluginInstanceGUI* FileSourcePlugin::createTxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSource *txChannel) const
{
    return 0;
}
#else
PluginInstanceGUI* FileSourcePlugin::createTxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSource *txChannel) const
{
    return FileSourceGUI::create(m_pluginAPI, deviceUISet, txChannel);
}
#endif

BasebandSampleSource* FileSourcePlugin::createTxChannelBS(DeviceAPI *deviceAPI) const
{
    return new FileSource(deviceAPI);
}

ChannelAPI* FileSourcePlugin::createTxChannelCS(DeviceAPI *deviceAPI) const
{
    return new FileSource(deviceAPI);
}

ChannelWebAPIAdapter* FileSourcePlugin::createChannelWebAPIAdapter() const
{
	return new FileSourceWebAPIAdapter();
}
