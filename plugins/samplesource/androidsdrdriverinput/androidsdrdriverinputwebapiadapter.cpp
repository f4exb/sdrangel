///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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
#include "androidsdrdriverinput.h"
#include "androidsdrdriverinputwebapiadapter.h"

AndroidSDRDriverInputWebAPIAdapter::AndroidSDRDriverInputWebAPIAdapter()
{}

AndroidSDRDriverInputWebAPIAdapter::~AndroidSDRDriverInputWebAPIAdapter()
{}

int AndroidSDRDriverInputWebAPIAdapter::webapiSettingsGet(
        SWGSDRangel::SWGDeviceSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setAndroidSdrDriverInputSettings(new SWGSDRangel::SWGAndroidSDRDriverInputSettings());
    response.getAndroidSdrDriverInputSettings()->init();
    AndroidSDRDriverInput::webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int AndroidSDRDriverInputWebAPIAdapter::webapiSettingsPutPatch(
        bool force,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response, // query + response
        QString& errorMessage)
{
    (void) force; // no action
    (void) errorMessage;
    AndroidSDRDriverInput::webapiUpdateDeviceSettings(m_settings, deviceSettingsKeys, response);
    return 200;
}
