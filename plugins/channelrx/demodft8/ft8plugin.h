///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany     //
// written by Christian Daniel                                                       //
// Copyright (C) 2014 John Greb <hexameron@spam.no>                                  //
// Copyright (C) 2016-2020, 2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>        //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
#ifndef INCLUDE_FT8PLUGIN_H
#define INCLUDE_FT8PLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class DeviceUISet;
class BasebandSampleSink;

class FT8Plugin : public QObject, PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID "sdrangel.channel.ft8demod")

public:
	explicit FT8Plugin(QObject* parent = nullptr);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	virtual void createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const;
	virtual ChannelGUI* createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const;
    virtual ChannelWebAPIAdapter* createChannelWebAPIAdapter() const;

private:
	static const PluginDescriptor m_pluginDescriptor;

	PluginAPI* m_pluginAPI;
};

#endif // INCLUDE_FT8PLUGIN_H
