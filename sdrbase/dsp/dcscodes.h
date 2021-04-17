//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Edouard Griffiths, F4EXB                                                              //
//                                                                                                          //
// This program is free software; you can redistribute it and/or modify                                     //
// it under the terms of the GNU General Public License as published by                                     //
// the Free Software Foundation as version 3 of the License, or                                             //
// (at your option) any later version.                                                                      //
//                                                                                                          //
// This program is distributed in the hope that it will be useful,                                          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                                           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                                             //
// GNU General Public License V3 for more details.                                                          //
//                                                                                                          //
// You should have received a copy of the GNU General Public License                                        //
// along with this program. If not, see <http://www.gnu.org/licenses/>.                                     //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_DSP_DCSCODES_H_
#define INCLUDE_DSP_DCSCODES_H_

#include <QList>
#include <QMap>

#include "export.h"

class SDRBASE_API DCSCodes
{
public:
    static void getCanonicalCodes(QList<unsigned int>& codes);
    static const int m_nbCodes;
    static const QMap<unsigned int, unsigned int> m_toCanonicalCode;
    static const QMap<unsigned int, unsigned int> m_signFlip;
};

#endif // INCLUDE_DSP_DCSCODES_H_
