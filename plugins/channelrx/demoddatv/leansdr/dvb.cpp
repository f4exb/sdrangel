///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB <f4exb06@gmail.com>                   //
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
#include "dvb.h"

namespace leansdr
{

deconvol_sync_simple *make_deconvol_sync_simple(scheduler *sch,
                                                pipebuf<eucl_ss> &_in,
                                                pipebuf<u8> &_out,
                                                enum code_rate rate)
{
    // EN 300 421, section 4.4.3 Inner coding
    uint32_t pX, pY;
    switch (rate)
    {
    case FEC12:
        pX = 0x1; // 1
        pY = 0x1; // 1
        break;
    case FEC23:
    case FEC46:
        pX = 0xa; // 1010  (Handle as FEC4/6, no half-symbols)
        pY = 0xf; // 1111
        break;
    case FEC34:
        pX = 0x5; // 101
        pY = 0x6; // 110
        break;
    case FEC56:
        pX = 0x15; // 10101
        pY = 0x1a; // 11010
        break;
    case FEC78:
        pX = 0x45; // 1000101
        pY = 0x7a; // 1111010
        break;
    default:
        //fail("Code rate not implemented");
        // For testing DVB-S2 constellations.
        fprintf(stderr, "Code rate not implemented; proceeding anyway\n");
        pX = pY = 1;
    }

    return new deconvol_sync_simple(sch, _in, _out, DVBS_G1, DVBS_G2, pX, pY);
}

} // leansdr
