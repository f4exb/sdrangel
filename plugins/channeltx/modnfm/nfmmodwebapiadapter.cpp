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
#include "dsp/cwkeyer.h"
#include "nfmmod.h"
#include "nfmmodwebapiadapter.h"

NFMModWebAPIAdapter::NFMModWebAPIAdapter()
{}

NFMModWebAPIAdapter::~NFMModWebAPIAdapter()
{}

int NFMModWebAPIAdapter::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setNfmModSettings(new SWGSDRangel::SWGNFMModSettings());
    response.getNfmModSettings()->init();
    NFMMod::webapiFormatChannelSettings(response, m_settings);

    const CWKeyerSettings& cwKeyerSettings = m_settings.getCWKeyerSettings();
    SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = response.getNfmModSettings()->getCwKeyer();
    apiCwKeyerSettings->init();
    CWKeyer::webapiFormatChannelSettings(apiCwKeyerSettings, cwKeyerSettings);

    return 200;
}

int NFMModWebAPIAdapter::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) force; // no action
    (void) errorMessage;
    NFMMod::webapiUpdateChannelSettings(m_settings, channelSettingsKeys, response);

    if (channelSettingsKeys.contains("cwKeyer"))
    {
        CWKeyerSettings newCWKeyerSettings;
        SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = response.getNfmModSettings()->getCwKeyer();
        CWKeyer::webapiSettingsPutPatch(channelSettingsKeys, newCWKeyerSettings, apiCwKeyerSettings);
        m_settings.setCWKeyerSettings(newCWKeyerSettings);
        const QByteArray& serializedNewSettings = m_settings.serialize(); // effectively update CW keyer settings
    }

    NFMMod::webapiFormatChannelSettings(response, m_settings);
    return 200;
}
