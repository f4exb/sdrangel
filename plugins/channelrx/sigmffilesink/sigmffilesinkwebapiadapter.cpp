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
#include "sigmffilesink.h"
#include "sigmffilesinkwebapiadapter.h"

SigMFFileSinkWebAPIAdapter::SigMFFileSinkWebAPIAdapter()
{}

SigMFFileSinkWebAPIAdapter::~SigMFFileSinkWebAPIAdapter()
{}

int SigMFFileSinkWebAPIAdapter::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    (void) response;
    response.setSigMfFileSinkSettings(new SWGSDRangel::SWGSigMFFileSinkSettings());
    response.getSigMfFileSinkSettings()->init();
    SigMFFileSink::webapiFormatChannelSettings(response, m_settings);

    return 200;
}

int SigMFFileSinkWebAPIAdapter::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) force; // no action
    (void) errorMessage;
    SigMFFileSink::webapiUpdateChannelSettings(m_settings, channelSettingsKeys, response);

    return 200;
}
