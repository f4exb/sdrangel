///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include "SWGFeatureSettings.h"
#include "gs232controller.h"
#include "gs232controllerwebapiadapter.h"

GS232ControllerWebAPIAdapter::GS232ControllerWebAPIAdapter()
{}

GS232ControllerWebAPIAdapter::~GS232ControllerWebAPIAdapter()
{}

int GS232ControllerWebAPIAdapter::webapiSettingsGet(
        SWGSDRangel::SWGFeatureSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setGs232ControllerSettings(new SWGSDRangel::SWGGS232ControllerSettings());
    response.getGs232ControllerSettings()->init();
    GS232Controller::webapiFormatFeatureSettings(response, m_settings);

    return 200;
}

int GS232ControllerWebAPIAdapter::webapiSettingsPutPatch(
        bool force,
        const QStringList& featureSettingsKeys,
        SWGSDRangel::SWGFeatureSettings& response,
        QString& errorMessage)
{
    (void) force; // no action
    (void) errorMessage;
    GS232Controller::webapiUpdateFeatureSettings(m_settings, featureSettingsKeys, response);

    return 200;
}
