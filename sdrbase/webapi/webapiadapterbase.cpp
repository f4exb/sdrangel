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
#include "webapiadapterbase.h"

void WebAPIAdapterBase::webapiFormatPreferences(
    SWGSDRangel::SWGPreferences *apiPreferences,
    const Preferences& preferences
)
{
    apiPreferences->init();
    apiPreferences->setSourceDevice(new QString(preferences.m_sourceDevice));
    apiPreferences->setSourceIndex(preferences.m_sourceIndex);
    apiPreferences->setAudioType(new QString(preferences.m_audioType));
    apiPreferences->setAudioDevice(new QString(preferences.m_audioDevice));
    apiPreferences->setLatitude(preferences.m_latitude);
    apiPreferences->setLongitude(preferences.m_longitude);
    apiPreferences->setConsoleMinLogLevel((int) preferences.m_consoleMinLogLevel);
    apiPreferences->setUseLogFile(preferences.m_useLogFile ? 1 : 0);
    apiPreferences->setLogFileName(new QString(preferences.m_logFileName));
    apiPreferences->setFileMinLogLevel((int) preferences.m_fileMinLogLevel);
}

void WebAPIAdapterBase::webapiFormatPreset(
        SWGSDRangel::SWGPreset *apiPreset,
        const Preset& preset
)
{
    apiPreset->init();
    apiPreset->setSourcePreset(preset.m_sourcePreset ? 1 : 0);
    apiPreset->setGroup(new QString(preset.m_group));
    apiPreset->setDescription(new QString(preset.m_description));
    apiPreset->setCenterFrequency(preset.m_centerFrequency);
    apiPreset->getMSpectrumConfig()->init(); // TODO when spectrum config is extracted to sdrbase
    apiPreset->setDcOffsetCorrection(preset.m_dcOffsetCorrection ? 1 : 0);
    apiPreset->setIqImbalanceCorrection(preset.m_iqImbalanceCorrection ? 1 : 0);
    // TODO: channel configs
    // TODO: device configs
}
