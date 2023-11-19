///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2017, 2019-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
//                                                                               //
// Interface for static web API adapters used for preset serialization and       //
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

#ifndef SDRBASE_CHANNEL_CHANNELWEBAPIADAPER_H_
#define SDRBASE_CHANNEL_CHANNELWEBAPIADAPER_H_

#include <QByteArray>
#include <QStringList>

#include "export.h"

namespace SWGSDRangel
{
    class SWGChannelSettings;
}

class SDRBASE_API ChannelWebAPIAdapter
{
public:
    ChannelWebAPIAdapter() {}
    virtual ~ChannelWebAPIAdapter() {}
    virtual QByteArray serialize() const = 0;
    virtual bool deserialize(const QByteArray& data)  = 0;

    /**
     * API adapter for the channel settings GET requests
     */
    virtual int webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage)
    {
        (void) response;
        errorMessage = "Not implemented"; return 501;
    }

    /**
     * API adapter for the channel settings PUT and PATCH requests
     */
    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage)
    {
        (void) force;
        (void) channelSettingsKeys;
        (void) response;
        errorMessage = "Not implemented"; return 501;
    }
};

#endif // SDRBASE_CHANNEL_CHANNELWEBAPIADAPER_H_
