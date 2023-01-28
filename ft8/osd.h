///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// This is the code from ft8mon: https://github.com/rtmrtmrtmrtm/ft8mon          //
// written by Robert Morris, AB1HL                                               //
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
#ifndef osd_h
#define osd_h

namespace FT8 {

class OSD
{
public:
    static const int gen_sys[174][91];
    static int check_crc(const int a91[91]);
    static void ldpc_encode(int plain[91], int codeword[174]);
    static float osd_score(int xplain[91], float ll174[174]);
    static int osd_check(const int plain[91]);
    static void matmul(int a[91][91], int b[91], int c[91]);
    static int osd_decode(float codeword[174], int depth, int out[91], int *out_depth);
};

} // namepsace FT8

#endif // osd_h

