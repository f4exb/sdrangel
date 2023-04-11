///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// Implementation of static web API adapters used for preset serialization and   //
// deserialization                                                               //
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

#include "SWGDeviceSettings.h"
#include "aaroniartsaoutput.h"
#include "aaroniartsaoutputwebapiadapter.h"

AaroniaRTSAOutputWebAPIAdapter::AaroniaRTSAOutputWebAPIAdapter()
{}

AaroniaRTSAOutputWebAPIAdapter::~AaroniaRTSAOutputWebAPIAdapter()
{}

int AaroniaRTSAOutputWebAPIAdapter::webapiSettingsGet(
        SWGSDRangel::SWGDeviceSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setLocalOutputSettings(new SWGSDRangel::SWGLocalOutputSettings());
    response.getLocalOutputSettings()->init();
    AaroniaRTSAOutput::webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int AaroniaRTSAOutputWebAPIAdapter::webapiSettingsPutPatch(
        bool force,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response, // query + response
        QString& errorMessage)
{
    (void) force; // no action
    (void) errorMessage;
    AaroniaRTSAOutput::webapiUpdateDeviceSettings(m_settings, deviceSettingsKeys, response);
    return 200;
}
