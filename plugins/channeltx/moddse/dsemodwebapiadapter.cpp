///////////////////////////////////////////////////////////////////////////////////
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

#include "SWGChannelSettings.h"
#include "dsp/cwkeyer.h"
#include "dsemod.h"
#include "dsemodwebapiadapter.h"

DSEModWebAPIAdapter::DSEModWebAPIAdapter()
{}

DSEModWebAPIAdapter::~DSEModWebAPIAdapter()
{}

int DSEModWebAPIAdapter::webapiSettingsGet(
    SWGSDRangel::SWGChannelSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setDseModSettings(new SWGSDRangel::SWGDSEModSettings());
    response.getDseModSettings()->init();
    DSEMod::webapiFormatChannelSettings(response, m_settings);

    SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = response.getDseModSettings()->getCwKeyer();
    const CWKeyerSettings& cwKeyerSettings = m_settings.getCWKeyerSettings();
    CWKeyer::webapiFormatChannelSettings(apiCwKeyerSettings, cwKeyerSettings);

    return 200;
}

int DSEModWebAPIAdapter::webapiSettingsPutPatch(
    bool force,
    const QStringList& channelSettingsKeys,
    SWGSDRangel::SWGChannelSettings& response,
    QString& errorMessage)
{
    (void) force; // no action
    (void) errorMessage;
    DSEMod::webapiUpdateChannelSettings(m_settings, channelSettingsKeys, response);

    if (channelSettingsKeys.contains("cwKeyer"))
    {
        SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = response.getDseModSettings()->getCwKeyer();
        CWKeyerSettings cwKeyerSettings = m_settings.getCWKeyerSettings();
        CWKeyer::webapiSettingsPutPatch(channelSettingsKeys, cwKeyerSettings, apiCwKeyerSettings);
        m_settings.setCWKeyerSettings(cwKeyerSettings);
    }

    DSEMod::webapiFormatChannelSettings(response, m_settings);
    return 200;
}
