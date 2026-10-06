///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#ifndef INCLUDE_BFMMODPLUGIN_H
#define INCLUDE_BFMMODPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class BFMModPlugin : public QObject, PluginInterface
{
    Q_OBJECT
    Q_INTERFACES(PluginInterface)
    Q_PLUGIN_METADATA(IID "sdrangel.channeltx.bfmmod")

public:
    explicit BFMModPlugin(QObject *parent = nullptr);
    const PluginDescriptor& getPluginDescriptor() const override;
    void initPlugin(PluginAPI *pluginAPI) override;
    void createTxChannel(DeviceAPI *deviceAPI, BasebandSampleSource **bs, ChannelAPI **cs) const override;
    ChannelGUI* createTxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSource *txChannel) const override;
    ChannelWebAPIAdapter* createChannelWebAPIAdapter() const override;

private:
    static const PluginDescriptor m_pluginDescriptor;
    PluginAPI *m_pluginAPI;
};

#endif // INCLUDE_BFMMODPLUGIN_H
