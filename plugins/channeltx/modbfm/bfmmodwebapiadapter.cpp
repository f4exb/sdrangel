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

#include "SWGChannelSettings.h"
#include "dsp/cwkeyer.h"
#include "bfmmod.h"
#include "bfmmodwebapiadapter.h"

BFMModWebAPIAdapter::BFMModWebAPIAdapter()
{}

BFMModWebAPIAdapter::~BFMModWebAPIAdapter()
{}

bool BFMModWebAPIAdapter::deserialize(const QByteArray& data)
{
    return m_settings.deserialize(data);
}

int BFMModWebAPIAdapter::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setBfmModSettings(new SWGSDRangel::SWGBFMModSettings());
    response.getBfmModSettings()->init();
    BFMMod::webapiFormatChannelSettings(response, m_settings);

    const CWKeyerSettings& cwKeyerSettings = m_settings.getCWKeyerSettings();
    SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = response.getBfmModSettings()->getCwKeyer();
    apiCwKeyerSettings->init();
    CWKeyer::webapiFormatChannelSettings(apiCwKeyerSettings, cwKeyerSettings);

    return 200;
}

int BFMModWebAPIAdapter::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) force; // no action
    (void) errorMessage;
    BFMMod::webapiUpdateChannelSettings(m_settings, channelSettingsKeys, response);

    if (channelSettingsKeys.contains("cwKeyer"))
    {
        CWKeyerSettings newCWKeyerSettings;
        SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = response.getBfmModSettings()->getCwKeyer();
        CWKeyer::webapiSettingsPutPatch(channelSettingsKeys, newCWKeyerSettings, apiCwKeyerSettings);
        m_settings.setCWKeyerSettings(newCWKeyerSettings);
    }

    BFMMod::webapiFormatChannelSettings(response, m_settings);
    return 200;
}

