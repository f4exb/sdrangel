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
#include "nfmtestdemod.h"
#include "nfmtestdemodwebapiadapter.h"

NFMDemodTestWebAPIAdapter::NFMDemodTestWebAPIAdapter()
{}

NFMDemodTestWebAPIAdapter::~NFMDemodTestWebAPIAdapter()
{}

int NFMDemodTestWebAPIAdapter::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setNfmDemodSettings(new SWGSDRangel::SWGNFMDemodSettings());
    response.getNfmDemodSettings()->init();
    NFMDemodTest::webapiFormatChannelSettings(response, m_settings);

    return 200;
}

int NFMDemodTestWebAPIAdapter::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    NFMDemodTest::webapiUpdateChannelSettings(m_settings, channelSettingsKeys, response);

    return 200;
}
