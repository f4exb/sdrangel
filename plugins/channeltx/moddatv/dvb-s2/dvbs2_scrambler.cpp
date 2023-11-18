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
#include "DVBS2.h"

int DVBS2::parity_chk( long a, long b)
{
    int c = 0;
    a = a & b;
    for( int i = 0; i < 18; i++ )
    {
        if( a&(1L<<i)) c++;
    }
    return c&1;
}
//
// This is not time sensitive and is only run at start up
//
void DVBS2::build_symbol_scrambler_table( void )
{
    long x,y;
    int xa,xb,xc,ya,yb,yc;
    int rn,zna,znb;

    // Initialisation
    x = 0x00001;
    y = 0x3FFFF;

    for( int i = 0; i < FRAME_SIZE_NORMAL; i++ )
    {
        xa = parity_chk( x, 0x8050 );
        xb = parity_chk( x, 0x0081 );
        xc = x&1;

        x >>= 1;
        if( xb ) x |= 0x20000;

        ya = parity_chk( y, 0x04A1 );
        yb = parity_chk( y, 0xFF60 );
        yc = y&1;

        y >>= 1;
        if( ya ) y |= 0x20000;

        zna = xc ^ yc;
        znb = xa ^ yb;
        rn = (znb<<1) + zna;
        m_cscram[i] = rn;
    }
}

void DVBS2::pl_scramble_symbols( scmplx *fs, int len )
{
    scmplx x;

    // Start at the end of the PL Header.

    for( int n = 0; n < len; n++ )
    {
        switch( m_cscram[n] )
        {
            case 0:
                // Do nothing
                break;
            case 1:
				x = fs[n];
				fs[n].re = -x.im;
                fs[n].im =  x.re;
                break;
            case 2:
                fs[n].re = -fs[n].re;
                fs[n].im = -fs[n].im;
                break;
            case 03:
                x = fs[n];
                fs[n].re =  x.im;
                fs[n].im = -x.re;
                break;
        }
    }
}
void DVBS2::pl_scramble_dummy_symbols( int len )
{
    scmplx x;
    int p = 0;

    x = m_bpsk[0][0];

    for( int n = 90; n < len; n++ )
    {
        switch( m_cscram[p] )
        {
            case 0:
                // Do nothing
                m_pl_dummy[n].re =  x.re;
                m_pl_dummy[n].im =  x.im;
                break;
            case 1:
                m_pl_dummy[n].re = -x.im;
                m_pl_dummy[n].im =  x.re;
                break;
            case 2:
                m_pl_dummy[n].re = -x.re;
                m_pl_dummy[n].im = -x.im;
                break;
            case 3:
                m_pl_dummy[n].re =  x.im;
                m_pl_dummy[n].im = -x.re;
                break;
        }
        p++;
    }
}
