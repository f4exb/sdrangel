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
#include "wfmmod.h"
#include "wfmmodwebapiadapter.h"

WFMModWebAPIAdapter::WFMModWebAPIAdapter()
{}

WFMModWebAPIAdapter::~WFMModWebAPIAdapter()
{}

int WFMModWebAPIAdapter::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setWfmModSettings(new SWGSDRangel::SWGWFMModSettings());
    response.getWfmModSettings()->init();
    WFMMod::webapiFormatChannelSettings(response, m_settings);

    const CWKeyerSettings& cwKeyerSettings = m_settings.getCWKeyerSettings();
    SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = response.getWfmModSettings()->getCwKeyer();
    apiCwKeyerSettings->init();
    CWKeyer::webapiFormatChannelSettings(apiCwKeyerSettings, cwKeyerSettings);

    return 200;
}

int WFMModWebAPIAdapter::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) force; // no action
    (void) errorMessage;
    WFMMod::webapiUpdateChannelSettings(m_settings, channelSettingsKeys, response);

    if (channelSettingsKeys.contains("cwKeyer"))
    {
        CWKeyerSettings newCWKeyerSettings;
        SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = response.getWfmModSettings()->getCwKeyer();
        CWKeyer::webapiSettingsPutPatch(channelSettingsKeys, newCWKeyerSettings, apiCwKeyerSettings);
        m_settings.setCWKeyerSettings(newCWKeyerSettings);
        const QByteArray& serializedNewSettings = m_settings.serialize(); // effectively update CW keyer settings
    }

    WFMMod::webapiFormatChannelSettings(response, m_settings);
    return 200;
}
