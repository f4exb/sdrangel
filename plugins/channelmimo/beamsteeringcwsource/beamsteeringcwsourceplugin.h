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

#ifndef INCLUDE_BEAMSTEERINGCWSOURCEPLUGIN_H_
#define INCLUDE_BEAMSTEERINGCWSOURCEPLUGIN_H_


#include <QObject>
#include "plugin/plugininterface.h"

class DeviceUISet;
class MIMOChannel;

class BeamSteeringCWSourcePlugin : public QObject, PluginInterface {
    Q_OBJECT
    Q_INTERFACES(PluginInterface)
    Q_PLUGIN_METADATA(IID "sdrangel.channelmimo.beamsteeringcwsource")

public:
    explicit BeamSteeringCWSourcePlugin(QObject* parent = nullptr);

    const PluginDescriptor& getPluginDescriptor() const;
    void initPlugin(PluginAPI* pluginAPI);

    virtual PluginInstanceGUI* createMIMOChannelGUI(DeviceUISet *deviceUISet, MIMOChannel *mimoChannel) const;
    virtual MIMOChannel* createMIMOChannelBS(DeviceAPI *deviceAPI) const;
    virtual ChannelAPI* createMIMOChannelCS(DeviceAPI *deviceAPI) const;
    virtual ChannelWebAPIAdapter* createChannelWebAPIAdapter() const;

private:
    static const PluginDescriptor m_pluginDescriptor;

    PluginAPI* m_pluginAPI;
};

#endif /* INCLUDE_BEAMSTEERINGCWSOURCEPLUGIN_H_ */
