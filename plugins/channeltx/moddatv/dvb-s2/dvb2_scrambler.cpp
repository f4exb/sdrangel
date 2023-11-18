///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE <jon@beniston.com>                         //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
#include "DVB2.h"

void DVB2::init_bb_randomiser(void)
{
    int sr = 0x4A80;
    for( int i = 0; i < FRAME_SIZE_NORMAL; i++ )
    {
        int b = ((sr)^(sr>>1))&1;
        m_bb_randomise[i] = b;
        sr >>= 1;
        if( b ) sr |= 0x4000;
    }
}
//
// Randomise the data bits
//
void DVB2::bb_randomise(void)
{
    for( int i = 0; i < m_format[0].kbch; i++ )
   {
        m_frame[i] ^= m_bb_randomise[i];
    }
}
