///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
// using LeanSDR Framework (C) 2016 F4DAV                                        //
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

#ifndef INCLUDE_DATVPLUGIN_H
#define INCLUDE_DATVPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class DeviceUISet;
class BasebandSampleSink;

class DATVDemodPlugin : public QObject, PluginInterface
{
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
    Q_PLUGIN_METADATA(IID "sdrangel.channel.demoddatv")

public:
    explicit DATVDemodPlugin(QObject* ptrParent = nullptr);

	const PluginDescriptor& getPluginDescriptor() const;
    void initPlugin(PluginAPI* ptrPluginAPI);

    virtual void createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const;
    virtual ChannelGUI* createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const;
    virtual ChannelWebAPIAdapter* createChannelWebAPIAdapter() const;

private:
    static const PluginDescriptor m_ptrPluginDescriptor;

    PluginAPI* m_ptrPluginAPI;


};

#endif // INCLUDE_DATVPLUGIN_H
