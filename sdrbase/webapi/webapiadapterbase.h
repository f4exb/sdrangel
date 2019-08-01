///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// Swagger server adapter interface                                              //
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
#ifndef SDRBASE_WEBAPI_WEBAPIADAPTERBASE_H_
#define SDRBASE_WEBAPI_WEBAPIADAPTERBASE_H_

#include "export.h"
#include "SWGPreferences.h"
#include "SWGPreset.h"
#include "settings/preferences.h"
#include "settings/preset.h"

/**
 * Adapter between API and objects in sdrbase library
 */
class SDRBASE_API WebAPIAdapterBase
{
public:
    static void webapiFormatPreferences(
        SWGSDRangel::SWGPreferences *apiPreferences,
        const Preferences& preferences
    );
    static void webapiFormatPreset(
        SWGSDRangel::SWGPreset *apiPreset,
        const Preset& preset
    );
};

#endif // SDRBASE_WEBAPI_WEBAPIADAPTERBASE_H_