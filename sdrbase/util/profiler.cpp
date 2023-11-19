//////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                  //
// Copyright (C) 2015-2019 Edouard Griffiths, F4EXB <f4exb06@gmail.com>         //
// Copyright (C) 2023 Jon Beniston, M7RCE <jon@beniston.com>                    //
//                                                                              //
// This program is free software; you can redistribute it and/or modify         //
// it under the terms of the GNU General Public License as published by         //
// the Free Software Foundation as version 3 of the License, or                 //
// (at your option) any later version.                                          //
//                                                                              //
// This program is distributed in the hope that it will be useful,              //
// but WITHOUT ANY WARRANTY; without even the implied warranty of               //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                 //
// GNU General Public License V3 for more details.                              //
//                                                                              //
// You should have received a copy of the GNU General Public License            //
// along with this program. If not, see <http://www.gnu.org/licenses/>.         //
//////////////////////////////////////////////////////////////////////////////////

#include "profiler.h"

QHash<QString, ProfileData> GlobalProfileData::m_profileData;
QMutex GlobalProfileData::m_mutex;

QHash<QString, ProfileData>& GlobalProfileData::getProfileData()
{
    m_mutex.lock();
    return m_profileData;
}

void GlobalProfileData::releaseProfileData()
{
    m_mutex.unlock();
}

void GlobalProfileData::resetProfileData()
{
    m_mutex.lock();
    m_profileData.clear();
    m_mutex.unlock();
}
