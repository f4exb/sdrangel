///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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
#include "morsedecoder.h"
#include "morsedecoderwebapiadapter.h"

MorseDecoderWebAPIAdapter::MorseDecoderWebAPIAdapter()
{}

MorseDecoderWebAPIAdapter::~MorseDecoderWebAPIAdapter()
{}

int MorseDecoderWebAPIAdapter::webapiSettingsGet(
        SWGSDRangel::SWGFeatureSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setMorseDecoderSettings(new SWGSDRangel::SWGMorseDecoderSettings());
    response.getMorseDecoderSettings()->init();
    MorseDecoder::webapiFormatFeatureSettings(response, m_settings);

    return 200;
}

int MorseDecoderWebAPIAdapter::webapiSettingsPutPatch(
        bool force,
        const QStringList& featureSettingsKeys,
        SWGSDRangel::SWGFeatureSettings& response,
        QString& errorMessage)
{
    (void) force; // no action
    (void) errorMessage;
    MorseDecoder::webapiUpdateFeatureSettings(m_settings, featureSettingsKeys, response);

    return 200;
}
