///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRBASE_PIPES_ELEMNTPIPESCOMMON_H_
#define SDRBASE_PIPES_ELEMNTPIPESCOMMON_H_

namespace ElementPipesCommon {

template<typename T>
struct RegistrationKey
{
    const T *m_key;
    int m_typeId;

    RegistrationKey() = default;
    RegistrationKey(const RegistrationKey&) = default;
    RegistrationKey& operator=(const RegistrationKey&) = default;

    bool operator<(const RegistrationKey& other) const
    {
        if (m_key !=  other.m_key) {
            return m_key < other.m_key;
        } else {
            return m_typeId < other.m_typeId;
        }
    }
};

} // ElementPipesCommon

#endif // SDRBASE_PIPES_ELEMNTPIPESCOMMON_H_
