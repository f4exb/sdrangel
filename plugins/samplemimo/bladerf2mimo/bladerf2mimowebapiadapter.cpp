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
#include "bladerf2mimo.h"
#include "bladerf2mimowebapiadapter.h"

BladeRF2MIMOWebAPIAdapter::BladeRF2MIMOWebAPIAdapter()
{}

BladeRF2MIMOWebAPIAdapter::~BladeRF2MIMOWebAPIAdapter()
{}

int BladeRF2MIMOWebAPIAdapter::webapiSettingsGet(
        SWGSDRangel::SWGDeviceSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setBladeRf2MimoSettings(new SWGSDRangel::SWGBladeRF2MIMOSettings());
    response.getBladeRf2MimoSettings()->init();
    BladeRF2MIMO::webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int BladeRF2MIMOWebAPIAdapter::webapiSettingsPutPatch(
        bool force,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response, // query + response
        QString& errorMessage)
{
    (void) force; // no action
    (void) errorMessage;
    BladeRF2MIMO::webapiUpdateDeviceSettings(m_settings, deviceSettingsKeys, response);
    return 200;
}
