///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2019, 2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
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
#ifndef libldpc_h
#define libldpc_h

namespace FT8 {

int ldpc_check(int codeword[]);
void ldpc_decode(float llcodeword[], int iters, int plain[], int *ok);
float fast_tanh(float x);
void ldpc_decode_log(float codeword[], int iters, int plain[], int *ok);
void ft8_crc(int msg1[], int msglen, int out[14]);
void gauss_jordan(int rows, int cols, int m[174][2 * 91], int which[91], int *ok);


} // namespace FT8

#endif // libldpc_h
