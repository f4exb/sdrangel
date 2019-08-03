///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRBASE_DEVICE_DEVICEWEBAPIADAPER_H_
#define SDRBASE_DEVICE_DEVICEWEBAPIADAPER_H_

#include <QByteArray>
#include <QStringList>

#include "export.h"

namespace SWGSDRangel
{
    class SWGDeviceSettings;
}

class SDRBASE_API DeviceWebAPIAdapter
{
public:
    DeviceWebAPIAdapter() {}
    virtual ~DeviceWebAPIAdapter() {}
    virtual QByteArray serialize() = 0;
    virtual bool deserialize(const QByteArray& data)  = 0;

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGDeviceSettings& response,
            QString& errorMessage) = 0;

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response, // query + response
            QString& errorMessage) = 0;
};


#endif // SDRBASE_DEVICE_DEVICEWEBAPIADAPER_H_