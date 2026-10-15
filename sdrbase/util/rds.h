///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2019, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2016 Ziga S <ziga.svetina@gmail.com>                            //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_UTIL_RDS_H
#define INCLUDE_UTIL_RDS_H

#include "export.h"

// Radio Data System definitions shared by the broadcast FM modulator and demodulator
struct SDRBASE_API RDS
{
    static const int m_nbProgrammeTypes = 32;
    static const char * const m_programmeTypes[m_nbProgrammeTypes]; //!< Programme type (PTY) names, indexed by code. UTF-8.
};

#endif // INCLUDE_UTIL_RDS_H
