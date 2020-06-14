///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_SIGMFFILEWEBAPIADAPTER_H
#define INCLUDE_SIGMFFILEWEBAPIADAPTER_H

#include "device/devicewebapiadapter.h"
#include "sigmffileinputsettings.h"

class SigMFFileInputWebAPIAdapter : public DeviceWebAPIAdapter
{
public:
    SigMFFileInputWebAPIAdapter();
    virtual ~SigMFFileInputWebAPIAdapter();
    virtual QByteArray serialize() { return m_settings.serialize(); }
    virtual bool deserialize(const QByteArray& data) { return m_settings.deserialize(data); }

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGDeviceSettings& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response, // query + response
            QString& errorMessage);

private:
    SigMFFileInputSettings m_settings;
};

#endif // INCLUDE_SIGMFFILEWEBAPIADAPTER_H
