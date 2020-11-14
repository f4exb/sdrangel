///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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
#include "ieee_802_15_4_mod.h"
#include "ieee_802_15_4_modwebapiadapter.h"

IEEE_802_15_4_ModWebAPIAdapter::IEEE_802_15_4_ModWebAPIAdapter()
{}

IEEE_802_15_4_ModWebAPIAdapter::~IEEE_802_15_4_ModWebAPIAdapter()
{}

int IEEE_802_15_4_ModWebAPIAdapter::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIeee802154ModSettings(new SWGSDRangel::SWGIEEE_802_15_4_ModSettings());
    response.getIeee802154ModSettings()->init();
    IEEE_802_15_4_Mod::webapiFormatChannelSettings(response, m_settings);

    return 200;
}

int IEEE_802_15_4_ModWebAPIAdapter::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) force; // no action
    (void) errorMessage;
    IEEE_802_15_4_Mod::webapiUpdateChannelSettings(m_settings, channelSettingsKeys, response);

    IEEE_802_15_4_Mod::webapiFormatChannelSettings(response, m_settings);
    return 200;
}
