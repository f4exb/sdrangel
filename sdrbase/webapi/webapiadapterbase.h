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
#include "SWGFeatureSetPreset.h"
#include "SWGCommand.h"
#include "settings/preferences.h"
#include "settings/preset.h"
#include "settings/featuresetpreset.h"
#include "settings/mainsettings.h"
#include "commands/command.h"
#include "webapiadapterinterface.h"

class PluginManager;
class ChannelWebAPIAdapter;
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
    void webapiFormatFeatureSetPreset(
        SWGSDRangel::SWGFeatureSetPreset *apiPreset,
        const FeatureSetPreset& preset
    );
    static void webapiFormatCommand(
        SWGSDRangel::SWGCommand *apiCommand,
        const Command& command
    );
    static void webapiInitConfig(
        MainSettings& mainSettings
    );
    static void webapiUpdatePreferences(
        SWGSDRangel::SWGPreferences *apiPreferences,
        const QStringList& preferenceKeys,
        Preferences& preferences
    );
    void webapiUpdatePreset(
        bool force,
        SWGSDRangel::SWGPreset *apiPreset,
        const WebAPIAdapterInterface::PresetKeys& presetKeys,
        Preset *preset
    );
    void webapiUpdateFeatureSetPreset(
        bool force,
        SWGSDRangel::SWGFeatureSetPreset *apiPreset,
        const WebAPIAdapterInterface::FeatureSetPresetKeys& presetKeys,
        FeatureSetPreset *preset
    );
    static void webapiUpdateCommand(
        SWGSDRangel::SWGCommand *apiCommand,
        const WebAPIAdapterInterface::CommandKeys& commandKeys,
        Command& command
    );

private:
    class WebAPIChannelAdapters
    {
    public:
        ChannelWebAPIAdapter *getChannelWebAPIAdapter(const QString& channelURI, const PluginManager *pluginManager);
        void flush();
    private:
        QMap<QString, ChannelWebAPIAdapter*> m_webAPIChannelAdapters;
    };

    class WebAPIDeviceAdapters
    {
    public:
        DeviceWebAPIAdapter *getDeviceWebAPIAdapter(const QString& deviceId, const PluginManager *pluginManager);
        void flush();
    private:
        QMap<QString, DeviceWebAPIAdapter*> m_webAPIDeviceAdapters;
    };

    class WebAPIFeatureAdapters
    {
    public:
        FeatureWebAPIAdapter *getFeatureWebAPIAdapter(const QString& featureURI, const PluginManager *pluginManager);
        void flush();
    private:
        QMap<QString, FeatureWebAPIAdapter*> m_webAPIFeatureAdapters;
    };

    const PluginManager *m_pluginManager;
    WebAPIChannelAdapters m_webAPIChannelAdapters;
    WebAPIDeviceAdapters m_webAPIDeviceAdapters;
    WebAPIFeatureAdapters m_webAPIFeatureAdapters;
};

#endif // SDRBASE_WEBAPI_WEBAPIADAPTERBASE_H_
