///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRBASE_FEATURE_FEATUREWEBAPIADAPER_H_
#define SDRBASE_FEATURE_FEATUREWEBAPIADAPER_H_

#include <QByteArray>
#include <QStringList>

#include "export.h"

namespace SWGSDRangel
{
    class SWGFeatureSettings;
}

class SDRBASE_API FeatureWebAPIAdapter
{
public:
    FeatureWebAPIAdapter() {}
    virtual ~FeatureWebAPIAdapter() {}
    virtual QByteArray serialize() const = 0;
    virtual bool deserialize(const QByteArray& data)  = 0;

    /**
     * API adapter for the channel settings GET requests
     */
    virtual int webapiSettingsGet(
            SWGSDRangel::SWGFeatureSettings& response,
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
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response,
            QString& errorMessage)
    {
        (void) force;
        (void) featureSettingsKeys;
        (void) response;
        errorMessage = "Not implemented"; return 501;
    }
};

#endif // SDRBASE_FEATURE_FEATUREWEBAPIADAPER_H_
