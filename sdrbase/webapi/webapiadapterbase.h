///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// Swagger server adapter interface                                              //
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
#ifndef SDRBASE_WEBAPI_WEBAPIADAPTERBASE_H_
#define SDRBASE_WEBAPI_WEBAPIADAPTERBASE_H_

#include <QMap>

#include "export.h"
#include "SWGPreferences.h"
#include "SWGPreset.h"
#include "SWGCommand.h"
#include "settings/preferences.h"
#include "settings/preset.h"
#include "commands/command.h"

class PluginManager;
class ChannelAPI;
class DeviceWebAPIAdapter;

/**
 * Adapter between API and objects in sdrbase library
 */
class SDRBASE_API WebAPIAdapterBase
{
public:
    WebAPIAdapterBase();
    ~WebAPIAdapterBase();

    void setPluginManager(const PluginManager *pluginManager) { m_pluginManager = pluginManager; }

    static void webapiFormatPreferences(
        SWGSDRangel::SWGPreferences *apiPreferences,
        const Preferences& preferences
    );
    void webapiFormatPreset(
        SWGSDRangel::SWGPreset *apiPreset,
        const Preset& preset
    );
    static void webapiFormatCommand(
        SWGSDRangel::SWGCommand *apiCommand,
        const Command& command
    );

private:
    class WebAPIChannelAdapters
    {
    public:
        ChannelAPI *getChannelAPI(const QString& channelURI, const PluginManager *pluginManager);
        void flush();
    private:
        QMap<QString, ChannelAPI*> m_webAPIChannelAdapters;
    };

    class WebAPIDeviceAdapters
    {
    public:
        DeviceWebAPIAdapter *getDeviceWebAPIAdapter(const QString& deviceId, const PluginManager *pluginManager);
        void flush();
    private:
        QMap<QString, DeviceWebAPIAdapter*> m_webAPIDeviceAdapters;
    };

    const PluginManager *m_pluginManager;
    WebAPIChannelAdapters m_webAPIChannelAdapters;
    WebAPIDeviceAdapters m_webAPIDeviceAdapters;
};

#endif // SDRBASE_WEBAPI_WEBAPIADAPTERBASE_H_