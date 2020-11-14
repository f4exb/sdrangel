///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB.                                  //
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
#include "chirpchatmod.h"
#include "chirpchatmodwebapiadapter.h"

ChirpChatModWebAPIAdapter::ChirpChatModWebAPIAdapter()
{}

ChirpChatModWebAPIAdapter::~ChirpChatModWebAPIAdapter()
{}

int ChirpChatModWebAPIAdapter::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setChirpChatModSettings(new SWGSDRangel::SWGChirpChatModSettings());
    response.getChirpChatModSettings()->init();
    ChirpChatMod::webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int ChirpChatModWebAPIAdapter::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) force; // no action
    (void) errorMessage;
    ChirpChatMod::webapiUpdateChannelSettings(m_settings, channelSettingsKeys, response);
    ChirpChatMod::webapiFormatChannelSettings(response, m_settings);
    return 200;
}
