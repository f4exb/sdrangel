///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
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

#include "SWGChannelSettings.h"
#include "settings/serializable.h"
#include "ssbmod.h"
#include "ssbmodwebapiadapter.h"

SSBModWebAPIAdapter::SSBModWebAPIAdapter() :
    ChannelAPI(SSBMod::m_channelIdURI, ChannelAPI::StreamSingleSource)
{}

SSBModWebAPIAdapter::~SSBModWebAPIAdapter()
{}

int SSBModWebAPIAdapter::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setSsbModSettings(new SWGSDRangel::SWGSSBModSettings());
    response.getSsbModSettings()->init();
    SSBMod::webapiFormatChannelSettings(response, m_settings);
    Serializable *cwKeyerGUI = m_settings.m_cwKeyerGUI;

    if (cwKeyerGUI)
    {
        const QByteArray& serializedCWKeyerSettings = cwKeyerGUI->serialize();
        CWKeyerSettings cwKeyerSettings;

        if (cwKeyerSettings.deserialize(serializedCWKeyerSettings))
        {
            SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = response.getSsbModSettings()->getCwKeyer();
            CWKeyer::webapiFormatChannelSettings(apiCwKeyerSettings, cwKeyerSettings);
        }
    }

    return 200;
}

int SSBModWebAPIAdapter::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    SSBMod::webapiUpdateChannelSettings(m_settings, channelSettingsKeys, response);
    Serializable *cwKeyerGUI = m_settings.m_cwKeyerGUI;

    if (channelSettingsKeys.contains("cwKeyer") && cwKeyerGUI)
    {
        const QByteArray& serializedCWKeyerSettings = cwKeyerGUI->serialize();
        CWKeyerSettings cwKeyerSettings;

        if (cwKeyerSettings.deserialize(serializedCWKeyerSettings))
        {
            SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = response.getSsbModSettings()->getCwKeyer();
            CWKeyer::webapiSettingsPutPatch(channelSettingsKeys, cwKeyerSettings, apiCwKeyerSettings);
            const QByteArray& serializedNewCWKeyerSettings = cwKeyerSettings.serialize();
            cwKeyerGUI->deserialize(serializedNewCWKeyerSettings);
        }
    }

    SSBMod::webapiFormatChannelSettings(response, m_settings);
    return 200;
}
