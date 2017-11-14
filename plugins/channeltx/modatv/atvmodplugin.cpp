///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
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
#include <QAction>
#include "plugin/pluginapi.h"

#include "atvmodgui.h"
#include "atvmod.h"
#include "atvmodplugin.h"

const PluginDescriptor ATVModPlugin::m_pluginDescriptor = {
    QString("ATV Modulator"),
    QString("3.8.4"),
    QString("(c) Edouard Griffiths, F4EXB"),
    QString("https://github.com/f4exb/sdrangel"),
    true,
    QString("https://github.com/f4exb/sdrangel")
};

ATVModPlugin::ATVModPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(0)
{
}

const PluginDescriptor& ATVModPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void ATVModPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    // register ATV modulator
    m_pluginAPI->registerTxChannel(ATVMod::m_channelID, this);
}

PluginInstanceGUI* ATVModPlugin::createTxChannelGUI(const QString& channelName, DeviceUISet *deviceUISet, BasebandSampleSource *txChannel)
{
    if(channelName == ATVMod::m_channelID)
    {
        ATVModGUI* gui = ATVModGUI::create(m_pluginAPI, deviceUISet, txChannel);
        return gui;
    } else {
        return 0;
    }
}

BasebandSampleSource* ATVModPlugin::createTxChannel(const QString& channelName, DeviceSinkAPI *deviceAPI)
{
    if(channelName == ATVMod::m_channelID)
    {
        ATVMod* source = new ATVMod(deviceAPI);
        return source;
    } else {
        return 0;
    }
}


