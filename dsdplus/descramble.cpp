///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

/* descramble.h */

// Functions for processing the radio-header:
// descramble
// deinterleave
// FECdecoder

// (C) 2011 Jonathan Naylor G4KLX

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

// This code was originally written by JOnathan Naylor, G4KLX, as part
// of the "pcrepeatercontroller" project
// More info:
// http://groups.yahoo.com/group/pcrepeatercontroller



// Changes:
// Convert C++ to C

// Version 20111106: initial release

// F4EXB: Convert back to C++ !

#include <string.h>
#include "descramble.h"

namespace DSDplus
{

const int Descramble::SCRAMBLER_TABLE_BITS[] = {
        0,0,0,0,1,1,1,0,1,1,1,1,0,0,1,0,1,1,0,0,1,0,0,1,0,0,0,0,0,0,1,0,
        0,0,1,0,0,1,1,0,0,0,1,0,1,1,1,0,1,0,1,1,0,1,1,0,0,0,0,0,1,1,0,0,
        1,1,0,1,0,1,0,0,1,1,1,0,0,1,1,1,1,0,1,1,0,1,0,0,0,0,1,0,1,0,1,0,
        1,1,1,1,1,0,1,0,0,1,0,1,0,0,0,1,1,0,1,1,1,0,0,0,1,1,1,1,1,1,1,0,
        0,0,0,1,1,1,0,1,1,1,1,0,0,1,0,1,1,0,0,1,0,0,1,0,0,0,0,0,0,1,0,0,
        0,1,0,0,1,1,0,0,0,1,0,1,1,1,0,1,0,1,1,0,1,1,0,0,0,0,0,1,1,0,0,1,
        1,0,1,0,1,0,0,1,1,1,0,0,1,1,1,1,0,1,1,0,1,0,0,0,0,1,0,1,0,1,0,1,
        1,1,1,1,0,1,0,0,1,0,1,0,0,0,1,1,0,1,1,1,0,0,0,1,1,1,1,1,1,1,0,0,
        0,0,1,1,1,0,1,1,1,1,0,0,1,0,1,1,0,0,1,0,0,1,0,0,0,0,0,0,1,0,0,0,
        1,0,0,1,1,0,0,0,1,0,1,1,1,0,1,0,1,1,0,1,1,0,0,0,0,0,1,1,0,0,1,1,
        0,1,0,1,0,0,1,1,1,0,0,1,1,1,1,0,1,1,0,1,0,0,0,0,1,0,1,0,1,0,1,1,
        1,1,1,0,1,0,0,1,0,1,0,0,0,1,1,0,1,1,1,0,0,0,1,1,1,1,1,1,1,0,0,0,
        0,1,1,1,0,1,1,1,1,0,0,1,0,1,1,0,0,1,0,0,1,0,0,0,0,0,0,1,0,0,0,1,
        0,0,1,1,0,0,0,1,0,1,1,1,0,1,0,1,1,0,1,1,0,0,0,0,0,1,1,0,0,1,1,0,
        1,0,1,0,0,1,1,1,0,0,1,1,1,1,0,1,1,0,1,0,0,0,0,1,0,1,0,1,0,1,1,1,
        1,1,0,1,0,0,1,0,1,0,0,0,1,1,0,1,1,1,0,0,0,1,1,1,1,1,1,1,0,0,0,0,
        1,1,1,0,1,1,1,1,0,0,1,0,1,1,0,0,1,0,0,1,0,0,0,0,0,0,1,0,0,0,1,0,
        0,1,1,0,0,0,1,0,1,1,1,0,1,0,1,1,0,1,1,0,0,0,0,0,1,1,0,0,1,1,0,1,
        0,1,0,0,1,1,1,0,0,1,1,1,1,0,1,1,0,1,0,0,0,0,1,0,1,0,1,0,1,1,1,1,
        1,0,1,0,0,1,0,1,0,0,0,1,1,0,1,1,1,0,0,0,1,1,1,1,1,1,1,0,0,0,0,1,
        1,1,0,1,1,1,1,0,0,1,0,1,1,0,0,1,0,0,1,0,0,0,0,0,0,1,0,0,0,1,0,0,
        1,1,0,0,0,1,0,1,1,1,0,1,0,1,1,0,1,1,0,0,0,0,0,1,1,0,0,1,1,0,1,0,
        1,0,0,1,1,1,0,0,1,1,1,1,0,1,1,0};

int Descramble::traceBack(int * out, int * m_pathMemory0, int * m_pathMemory1,
        int * m_pathMemory2, int * m_pathMemory3)
{
    enum FEC_STATE
    {
        S0, S1, S2, S3
    } state;
    int loop;
    int length = 0;

    state = S0;

    for (loop = 329; loop >= 0; loop--, length++)
    {

        switch (state)
        {
        case S0: // if state S0
            if (m_pathMemory0[loop])
            {
                state = S2; // lower path
            }
            else
            {
                state = S0; // upper path
            }
            ; // end else - if
            out[loop] = 0;
            break;

        case S1: // if state S1
            if (m_pathMemory1[loop])
            {
                state = S2; // lower path
            }
            else
            {
                state = S0; // upper path
            }
            ; // end else - if
            out[loop] = 1;
            break;

        case S2: // if state S2
            if (m_pathMemory2[loop])
            {
                state = S3; // lower path
            }
            else
            {
                state = S1; // upper path
            }
            ; // end else - if
            out[loop] = 0;
            break;

        case S3: // if state S3
            if (m_pathMemory3[loop])
            {
                state = S3; // lower path
            }
            else
            {
                state = S1; // upper path
            }
            ; // end else - if
            out[loop] = 1;
            break;

        }; // end switch
    }; // end for

    return (length);
} // end function

void Descramble::viterbiDecode(int n, int *data, int *m_pathMemory0, int *m_pathMemory1,
        int *m_pathMemory2, int *m_pathMemory3, int *m_pathMetric)
{
    int tempMetric[4];
    int metric[8];
    int loop;

    int m1;
    int m2;

    metric[0] = (data[1] ^ 0) + (data[0] ^ 0);
    metric[1] = (data[1] ^ 1) + (data[0] ^ 1);
    metric[2] = (data[1] ^ 1) + (data[0] ^ 0);
    metric[3] = (data[1] ^ 0) + (data[0] ^ 1);
    metric[4] = (data[1] ^ 1) + (data[0] ^ 1);
    metric[5] = (data[1] ^ 0) + (data[0] ^ 0);
    metric[6] = (data[1] ^ 0) + (data[0] ^ 1);
    metric[7] = (data[1] ^ 1) + (data[0] ^ 0);

    // Pres. state = S0, Prev. state = S0 & S2
    m1 = metric[0] + m_pathMetric[0];
    m2 = metric[4] + m_pathMetric[2];
    if (m1 < m2)
    {
        m_pathMemory0[n] = 0;
        tempMetric[0] = m1;
    }
    else
    {
        m_pathMemory0[n] = 1;
        tempMetric[0] = m2;
    }; // end else - if

    // Pres. state = S1, Prev. state = S0 & S2
    m1 = metric[1] + m_pathMetric[0];
    m2 = metric[5] + m_pathMetric[2];
    if (m1 < m2)
    {
        m_pathMemory1[n] = 0;
        tempMetric[1] = m1;
    }
    else
    {
        m_pathMemory1[n] = 1;
        tempMetric[1] = m2;
    }; // end else - if

    // Pres. state = S2, Prev. state = S2 & S3
    m1 = metric[2] + m_pathMetric[1];
    m2 = metric[6] + m_pathMetric[3];
    if (m1 < m2)
    {
        m_pathMemory2[n] = 0;
        tempMetric[2] = m1;
    }
    else
    {
        m_pathMemory2[n] = 1;
        tempMetric[2] = m2;
    }

    // Pres. state = S3, Prev. state = S1 & S3
    m1 = metric[3] + m_pathMetric[1];
    m2 = metric[7] + m_pathMetric[3];
    if (m1 < m2)
    {
        m_pathMemory3[n] = 0;
        tempMetric[3] = m1;
    }
    else
    {
        m_pathMemory3[n] = 1;
        tempMetric[3] = m2;
    }; // end else - if

    for (loop = 0; loop < 4; loop++)
    {
        m_pathMetric[loop] = tempMetric[loop];
    }; // end for

} // end function ViterbiDecode

int Descramble::FECdecoder(int * in, int * out)
{
    int outLen;

    int m_pathMemory0[330];
    memset(m_pathMemory0, 0, 330 * sizeof(int));
    int m_pathMemory1[330];
    memset(m_pathMemory1, 0, 330 * sizeof(int));
    int m_pathMemory2[330];
    memset(m_pathMemory2, 0, 330 * sizeof(int));
    int m_pathMemory3[330];
    memset(m_pathMemory3, 0, 330 * sizeof(int));
    int m_pathMetric[4];

    int loop, loop2;

    int n = 0;

    for (loop = 0; loop < 4; loop++)
    {
        m_pathMetric[loop] = 0;
    }; // end for

    for (loop2 = 0; loop2 < 660; loop2 += 2, n++)
    {
        int data[2];

        if (in[loop2])
        {
            data[1] = 1;
        }
        else
        {
            data[1] = 0;
        }; // end else - if

        if (in[loop2 + 1])
        {
            data[0] = 1;
        }
        else
        {
            data[0] = 0;
        }; // end else - if

        viterbiDecode(n, data, m_pathMemory0, m_pathMemory1, m_pathMemory2,
                m_pathMemory3, m_pathMetric);
    }; // end for

    outLen = traceBack(out, m_pathMemory0, m_pathMemory1, m_pathMemory2,
            m_pathMemory3);

// Swap endian-ness
// code removed (done converting bits into octets), done in main program

//for (loop=0;loop<330;loop+=8) {
//  int temp;
//  temp=out[loop];out[loop]=out[loop+7];out[loop+7]=temp;
//  temp=out[loop+1];out[loop+1]=out[loop+6];out[loop+6]=temp;
//  temp=out[loop+2];out[loop+2]=out[loop+5];out[loop+5]=temp;
//  temp=out[loop+3];out[loop+3]=out[loop+4];out[loop+4]=temp;
//}

    return (outLen);
} // end function FECdecoder

void Descramble::deinterleave(int * in, int * out)
{

    int k = 0;
    int loop = 0;
// function starts here

// init vars
    k = 0;

    for (loop = 0; loop < 660; loop++)
    {
        out[k] = in[loop];

        k += 24;

        if (k >= 672)
        {
            k -= 671;
        }
        else if (k >= 660)
        {
            k -= 647;
        }; // end elsif - if
    }; // end for
} // end function deinterleave

void Descramble::scramble (int * in,int * out)
{
    int loop = 0;
    int m_count = 0;

    for (loop = 0; loop < 660; loop++)
    {
        out[loop] = in[loop] ^ SCRAMBLER_TABLE_BITS[m_count++];

        if (m_count >= SCRAMBLER_TABLE_BITS_LENGTH)
        {
            m_count = 0U;
        }; // end if
    }; // end for
}

} // namespace DSDplus


