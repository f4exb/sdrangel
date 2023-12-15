///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2016, 2018-2019, 2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
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

#include "audiocatsisohamlib.h"

AudioCATSISOHamlib::AudioCATSISOHamlib()
{
    rig_load_all_backends();
    int status = rig_list_foreach(hash_model_list, this);
    qDebug("AudioCATSISOHamlib::AudioCATSISOHamlib: status: %d", status);
}

AudioCATSISOHamlib::~AudioCATSISOHamlib()
{
}

int AudioCATSISOHamlib::hash_model_list(HAMLIB_RIG_CAPS *caps, void *data)
{
    AudioCATSISOHamlib *hamlibHandler = (AudioCATSISOHamlib*) data;
    hamlibHandler->m_rigModels[caps->rig_model] = caps->model_name;
    hamlibHandler->m_rigNames[caps->model_name] = caps->rig_model;

    return 1;  /* !=0, we want them all ! */
}
