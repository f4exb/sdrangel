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

#include <string.h>
#include "dstar.h"
#include "dsd_decoder.h"

namespace DSDplus
{

const int DSDDstar::dW[72] = {

        // 0-11
        0, 0,
        3, 2,
        1, 1,
        0, 0,
        1, 1,
        0, 0,

        // 12-23
        3, 2,
        1, 1,
        3, 2,
        1, 1,
        0, 0,
        3, 2,

        // 24-35
        0, 0,
        3, 2,
        1, 1,
        0, 0,
        1, 1,
        0, 0,

        // 36-47
        3, 2,
        1, 1,
        3, 2,
        1, 1,
        0, 0,
        3, 2,

        // 48-59
        0, 0,
        3, 2,
        1, 1,
        0, 0,
        1, 1,
        0, 0,

        // 60-71
        3, 2,
        1, 1,
        3, 3,
        2, 1,
        0, 0,
        3, 3,
};

const int DSDDstar::dX[72] = {

        // 0-11
        10, 22,
        11, 9,
        10, 22,
        11, 23,
        8, 20,
        9, 21,

        // 12-23
        10, 8,
        9, 21,
        8, 6,
        7, 19,
        8, 20,
        9, 7,

        // 24-35
        6, 18,
        7, 5,
        6, 18,
        7, 19,
        4, 16,
        5, 17,

        // 36-47
        6, 4,
        5, 17,
        4, 2,
        3, 15,
        4, 16,
        5, 3,

        // 48-59
        2, 14,
        3, 1,
        2, 14,
        3, 15,
        0, 12,
        1, 13,

        // 60-71
        2, 0,
        1, 13,
        0, 12,
        10, 11,
        0, 12,
        1, 13,
};

DSDDstar::DSDDstar(DSDDecoder *dsdDecoder) :
        m_dsdDecoder(dsdDecoder)
{
}

DSDDstar::~DSDDstar()
{
}

void DSDDstar::init()
{
    if (m_dsdDecoder->m_opts.errorbars == 1) {
        fprintf(m_dsdDecoder->m_state.logfile, "e:");
    }

    if (m_dsdDecoder->m_state.synctype == 18) {
        framecount = 0;
        m_dsdDecoder->m_state.synctype = 6;
    } else if (m_dsdDecoder->m_state.synctype == 19) {
        framecount = 0;
        m_dsdDecoder->m_state.synctype = 7;
    } else {
        framecount = 1; //just saw a sync frame; there should be 20 not 21 till the next
    }

    sync_missed = 0;
    bitbuffer = 0;
    m_symbolIndex = 0;
}

void DSDDstar::initVoiceFrame()
{
    memset(ambe_fr, 0, 96);
    // voice frame
    w = dW;
    x = dX;
}

void DSDDstar::process()
{
    if (m_symbolIndex < 72) // voice frame
    {
        if (m_symbolIndex == 0) {
            initVoiceFrame();
        }

        processVoice();
    }
    else if (m_symbolIndex < 97) // data frame
    {

    }

    m_symbolIndex++;
}

void DSDDstar::processHD()
{
}

void DSDDstar::processVoice()
{
    int dibit = m_dsdDecoder->getDibit(); // get dibit from symbol and store it in cache
    m_dibitCache[m_symbolIndex] = dibit;

    if (m_symbolIndex == 72-1) // last dibit in voice frame
    {
        for (int i = 0; i < 72; i++)
        {
            int dibit = m_dibitCache[i];

            bitbuffer <<= 1;

            if (dibit == 1) {
                bitbuffer |= 0x01;
            }

            if ((bitbuffer & 0x00FFFFFF) == 0x00AAB468)
            {
                // we're slipping bits
                fprintf(m_dsdDecoder->m_state.logfile, "sync in voice after i=%d, restarting\n", i);
                //ugh just start over
                i = 0;
                w = dW;
                x = dX;
                framecount = 1;
                continue;
            }

            ambe_fr[*w][*x] = (1 & dibit);
            w++;
            x++;
        }

        m_dsdDecoder->m_mbeDecoder.processFrame(0, ambe_fr, 0);
    }
}

void DSDDstar::processData()
{

}

} // namespace DSDplus
