///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_CRC64_H_
#define INCLUDE_CRC64_H_

#include <stdint.h>

#include "export.h"

class SDRBASE_API CRC64
{
public:
    CRC64();
    ~CRC64();
    uint64_t calculate_crc(uint8_t *stream, int length);

private:
    void build_crc_table();

    uint64_t m_crcTable[256];
    static const uint64_t m_poly;
};



#endif /* INCLUDE_CRC64_H_ */
