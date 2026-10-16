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

#ifndef INCLUDE_IEEE_802_15_4_DEMOD_WEBAPIADAPTER_H
#define INCLUDE_IEEE_802_15_4_DEMOD_WEBAPIADAPTER_H

#include "channel/channelwebapiadapter.h"
#include "ieee_802_15_4_demodsettings.h"

/**
 * Standalone API adapter only for the settings
 */
class IEEE_802_15_4_DemodWebAPIAdapter : public ChannelWebAPIAdapter {
public:
    IEEE_802_15_4_DemodWebAPIAdapter();
    virtual ~IEEE_802_15_4_DemodWebAPIAdapter();

    virtual QByteArray serialize() const { return m_settings.serialize(); }
    virtual bool deserialize(const QByteArray& data) { return m_settings.deserialize(data); }

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

private:
    IEEE_802_15_4_DemodSettings m_settings;
};

#endif // INCLUDE_IEEE_802_15_4_DEMOD_WEBAPIADAPTER_H
