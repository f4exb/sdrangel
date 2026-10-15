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
#include "ieee_802_15_4_demod.h"
#include "ieee_802_15_4_demodwebapiadapter.h"

IEEE_802_15_4_DemodWebAPIAdapter::IEEE_802_15_4_DemodWebAPIAdapter()
{
}

IEEE_802_15_4_DemodWebAPIAdapter::~IEEE_802_15_4_DemodWebAPIAdapter()
{
}

int IEEE_802_15_4_DemodWebAPIAdapter::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIeee802154DemodSettings(new SWGSDRangel::SWGIEEE_802_15_4_DemodSettings());
    response.getIeee802154DemodSettings()->init();
    IEEE_802_15_4_Demod::webapiFormatChannelSettings(response, m_settings);

    return 200;
}

int IEEE_802_15_4_DemodWebAPIAdapter::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) force;
    (void) errorMessage;
    IEEE_802_15_4_Demod::webapiUpdateChannelSettings(m_settings, channelSettingsKeys, response);

    return 200;
}
