///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
//                                                                               //
// This is the code from ft8mon: https://github.com/rtmrtmrtmrtm/ft8mon          //
// reformatted and adapted to Qt and SDRangel context                            //
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

#ifndef FT8_ARRAYS_H_
#define FT8_ARRAYS_H_

#include "export.h"

namespace FT8 {

class FT8_API Arrays
{
public:
    //
    // this is the LDPC(174,91) parity check matrix.
    // each row describes one parity check.
    // 83 rows.
    // each number is an index into the codeword (1-origin).
    // the codeword bits mentioned in each row must xor to zero.
    // From WSJT-X's ldpc_174_91_c_reordered_parity.f90
    //
    static const int Nm[][7];

    // Mn from WSJT-X's ldpc_174_91_c_reordered_parity.f90
    // each of the 174 rows corresponds to a codeword bit.
    // the numbers indicate which three parity
    // checks (rows in Nm) refer to the codeword bit.
    // 1-origin.
    static const int Mn[][3];

    // This is the LDPC(174, 91) generator matrix
    // It has 83 rows and 91 columns (zero padded on the right thus 12 bytes)
    // From WSJT-X's ldpc_174_91_c_generator.f90
    static const int Gm[][12];
};

} // namespace FT8

#endif // FT8_ARRAYS_H_
