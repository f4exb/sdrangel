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
#include "beamsteeringcwsource.h"
#include "beamsteeringcwsourcewebapiadapter.h"

BeamSteeringCWSourceWebAPIAdapter::BeamSteeringCWSourceWebAPIAdapter()
{}

BeamSteeringCWSourceWebAPIAdapter::~BeamSteeringCWSourceWebAPIAdapter()
{}

int BeamSteeringCWSourceWebAPIAdapter::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setBeamSteeringCwSourceSettings(new SWGSDRangel::SWGBeamSteeringCWSourceSettings());
    response.getBeamSteeringCwSourceSettings()->init();
    BeamSteeringCWSource::webapiFormatChannelSettings(response, m_settings);

    return 200;
}

int BeamSteeringCWSourceWebAPIAdapter::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    BeamSteeringCWSource::webapiUpdateChannelSettings(m_settings, channelSettingsKeys, response);

    return 200;
}
