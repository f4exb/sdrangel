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

#include "dmr_voice.h"
#include "dsd_decoder.h"

namespace DSDplus
{

/*
 * DMR AMBE interleave schedule
 */
const int DSDDMRVoice::rW[36] = {
  0, 1, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 2,
  0, 2, 0, 2, 0, 2,
  0, 2, 0, 2, 0, 2
};

const int DSDDMRVoice::rX[36] = {
  23, 10, 22, 9, 21, 8,
  20, 7, 19, 6, 18, 5,
  17, 4, 16, 3, 15, 2,
  14, 1, 13, 0, 12, 10,
  11, 9, 10, 8, 9, 7,
  8, 6, 7, 5, 6, 4
};

const int DSDDMRVoice::rY[36] = {
  0, 2, 0, 2, 0, 2,
  0, 2, 0, 3, 0, 3,
  1, 3, 1, 3, 1, 3,
  1, 3, 1, 3, 1, 3,
  1, 3, 1, 3, 1, 3,
  1, 3, 1, 3, 1, 3
};

const int DSDDMRVoice::rZ[36] = {
  5, 3, 4, 2, 3, 1,
  2, 0, 1, 13, 0, 12,
  22, 11, 21, 10, 20, 9,
  19, 8, 18, 7, 17, 6,
  16, 5, 15, 4, 14, 3,
  13, 2, 12, 1, 11, 0
};


DSDDMRVoice::DSDDMRVoice(DSDDecoder *dsdDecoder) :
        m_dsdDecoder(dsdDecoder),
        m_symbolIndex(0),
        m_dibitIndex(0),
        mutecurrentslot(0),
        m_slotIndex(-1)
{
}

DSDDMRVoice::~DSDDMRVoice()
{
}

void DSDDMRVoice::init()
{
    mutecurrentslot = 0;
    msMode = 0;
    dibit_p = m_dsdDecoder->m_state.dibit_buf_p - 144;
    m_symbolIndex = 0;

    preProcess();

    m_symbolIndex = 144; // reposition symbol index in frame
}

void DSDDMRVoice::process()
{
    m_majorBlock = m_symbolIndex / 288;           // frame (major block) index
    m_dibitIndex = m_symbolIndex % 288;           // index of symbol in frame
    int symbolIndex = getSlotIndex(m_dibitIndex); // returns symbol index in current slot. Updates m_slotIndex.

    switch(m_slotIndex)
    {
    case 0:
        processSlot0(symbolIndex);
        break;
    case 1:
        processSlot1(symbolIndex);
        break;
    case 2:
        processSlot2(symbolIndex);
        break;
    case 3:
        processSlot3(symbolIndex);
        break;
    case 4:
        processSlot4(symbolIndex);
        break;
    case 5:
        processSlot5(symbolIndex);
        break;
    case 6:
        processSlot6(symbolIndex);
        break;
    case 7:
        processSlot7(symbolIndex);
        break;
    case 8:
        processSlot8(symbolIndex);
        break;
    case 9:
        processSlot9(symbolIndex);
        break;
    case 10: // this is the post-process case
        postProcess(symbolIndex);
        break;
    default:
        m_dsdDecoder->m_fsmState = DSDDecoder::DSDLookForSync;
        break;
    }

    m_symbolIndex++;
}

void DSDDMRVoice::preProcess()
{
    // copy in memory to dibit cache
    memcpy((void *) m_dibitCache, (const void *) dibit_p, 144 * sizeof(int));

    m_dibitIndex = 54-1; // set cache pointer - skip slot 0

    m_dibitIndex += 12; // advance cache pointer
    processSlot1(12-1);

    m_dibitIndex += 36; // advance cache pointer
    processSlot2(36-1);

    m_dibitIndex += 18; // advance cache pointer
    processSlot3(18-1);

    m_dibitIndex += 24; // advance cache pointer
    processSlot4(24-1);

    m_dibitIndex += 18; // advance cache pointer
    processSlot5(18-1);

    m_dibitIndex += 36; // advance cache pointer
    processSlot6(36-1);

    m_dibitIndex += 12; // advance cache pointer
    processSlot7(12-1);

    m_dibitIndex += 54; // advance cache pointer - skip slot 8

    processSlot9(24-1);
    m_dibitIndex += 24; // advance cache pointer
    processSlot9(24-1);

    m_symbolIndex = 144; // advance the main symbol index
}

void DSDDMRVoice::postProcess(int symbolIndex)
{
    //fprintf(stderr, "DSDDMRVoice::postProcess: m_symbolIndex: %d", m_symbolIndex);
    m_dsdDecoder->getDibit(); // get dibit from symbol but do nothing with it

    if (symbolIndex == 54+12+54-1) // very last symbol -> go back to search sync state
    {
        fprintf(stderr, "DSDDMRVoice::postProcess: end of frame\n");
        m_dsdDecoder->m_fsmState = DSDDecoder::DSDLookForSync;
    }
}

void DSDDMRVoice::processSlot0(int symbolIndex) // Slot0 is a 54 symbol slot
{
    m_dsdDecoder->getDibit(); // get dibit from symbol but do nothing with it
}

void DSDDMRVoice::processSlot8(int symbolIndex) // Slot8 is a 54 symbol slot
{
    m_dsdDecoder->getDibit(); // get dibit from symbol but do nothing with it
}

void DSDDMRVoice::processSlot1(int symbolIndex) // Slot1 is a 12 symbol slot
{
    if (m_majorBlock > 0) // 0:1 reads from 144 in memory dibits. Handled by upper layer.
    {
        int dibit = m_dsdDecoder->getDibit(); // get dibit from symbol and store it in cache
        m_dibitCache[m_dibitIndex] = dibit;
    }

    if (symbolIndex == 12-1) // last symbol -> launch process
    {
        int *dibitCache = &m_dibitCache[m_dibitIndex - symbolIndex]; // move back to start of corresponding cache section
        // CACH
        for (int i = 0; i < 12; i++)
        {
            int dibit = dibitCache[i];
            cachdata[i] = dibit;

            if (i == 2)
            {
                m_dsdDecoder->m_state.currentslot = (1 & (dibit >> 1));  // bit 1

                if (m_dsdDecoder->m_state.currentslot == 0)
                {
                    m_dsdDecoder->m_state.slot0light[0] = '[';
                    m_dsdDecoder->m_state.slot0light[6] = ']';
                    m_dsdDecoder->m_state.slot1light[0] = ' ';
                    m_dsdDecoder->m_state.slot1light[6] = ' ';
                }
                else
                {
                    m_dsdDecoder->m_state.slot1light[0] = '[';
                    m_dsdDecoder->m_state.slot1light[6] = ']';
                    m_dsdDecoder->m_state.slot0light[0] = ' ';
                    m_dsdDecoder->m_state.slot0light[6] = ' ';
                }
            }
        }

        cachdata[12] = 0;
    }
}

void DSDDMRVoice::processSlot2(int symbolIndex) // Slot2 is a 36 symbol slot
{
    if (m_majorBlock > 0) // 0:2 reads from 144 in memory dibits. Handled by upper layer.
    {
        int dibit = m_dsdDecoder->getDibit(); // get dibit from symbol and store it in cache
        m_dibitCache[m_dibitIndex] = dibit;
    }

    if (symbolIndex == 36-1) // last symbol -> launch process
    {
        int *dibitCache = &m_dibitCache[m_dibitIndex - symbolIndex]; // move back to start of corresponding cache section

        w = rW;
        x = rX;
        y = rY;
        z = rZ;

        for (int i = 0; i < 36; i++)
        {
            int dibit = dibitCache[i];

            ambe_fr[*w][*x] = (1 & (dibit >> 1)); // bit 1
            ambe_fr[*y][*z] = (1 & dibit);        // bit 0
            w++;
            x++;
            y++;
            z++;
        }
    }
}

void DSDDMRVoice::processSlot3(int symbolIndex) // Slot3 is a 18 symbol slot
{
    if (m_majorBlock > 0) // 0:3 reads from 144 in memory dibits. Handled by upper layer.
    {
        int dibit = m_dsdDecoder->getDibit(); // get dibit from symbol and store it in cache
        m_dibitCache[m_dibitIndex] = dibit;
    }

    if (symbolIndex == 18-1) // last symbol -> launch process
    {
        int *dibitCache = &m_dibitCache[m_dibitIndex - symbolIndex]; // move back to start of corresponding cache section

        w = rW;
        x = rX;
        y = rY;
        z = rZ;

        for (int i = 0; i < 18; i++)
        {
            int dibit = dibitCache[i];

            ambe_fr2[*w][*x] = (1 & (dibit >> 1)); // bit 1
            ambe_fr2[*y][*z] = (1 & dibit);        // bit 0
            w++;
            x++;
            y++;
            z++;
        }
    }
}

void DSDDMRVoice::processSlot4(int symbolIndex) // Slot4 is a 24 symbol slot
{
    if (m_majorBlock > 0) // 0:3 reads from 144 in memory dibits. Handled by upper layer.
    {
        int dibit = m_dsdDecoder->getDibit(); // get dibit from symbol and store it in cache
        m_dibitCache[m_dibitIndex] = dibit;
    }

    if (symbolIndex == 24-1) // last symbol -> launch process
    {
        int *dibitCache = &m_dibitCache[m_dibitIndex - symbolIndex]; // move back to start of corresponding cache section

        for (int i = 0; i < 24; i++)
        {
            int dibit = dibitCache[i];

            syncdata[i] = dibit;
            sync[i] = (dibit | 1) + 48;
        }

        sync[24] = 0;
        syncdata[24] = 0;

        if ((strcmp(sync, DMR_BS_DATA_SYNC) == 0)
         || (strcmp(sync, DMR_MS_DATA_SYNC) == 0))
        {
            mutecurrentslot = 1;
            if (m_dsdDecoder->m_state.currentslot == 0)
            {
                sprintf(m_dsdDecoder->m_state.slot0light, "[slot0]");
            }
            else
            {
                sprintf(m_dsdDecoder->m_state.slot1light, "[slot1]");
            }
        }
        else if ((strcmp(sync, DMR_BS_VOICE_SYNC) == 0)
              || (strcmp(sync, DMR_MS_VOICE_SYNC) == 0))
        {
            mutecurrentslot = 0;
            if (m_dsdDecoder->m_state.currentslot == 0)
            {
                sprintf(m_dsdDecoder->m_state.slot0light, "[SLOT0]");
            }
            else
            {
                sprintf(m_dsdDecoder->m_state.slot1light, "[SLOT1]");
            }
        }
        if ((strcmp(sync, DMR_MS_VOICE_SYNC) == 0)
         || (strcmp(sync, DMR_MS_DATA_SYNC) == 0))
        {
            msMode = 1;
        }

        if ((m_majorBlock == 0) && (m_dsdDecoder->m_opts.errorbars == 1))
        {
            fprintf(stderr, "%s %s  VOICE e:", m_dsdDecoder->m_state.slot0light, m_dsdDecoder->m_state.slot1light);
        }
    }
}

void DSDDMRVoice::processSlot5(int symbolIndex) // Slot5 is a 18 symbol slot
{
    int dibit = m_dsdDecoder->getDibit(); // get dibit from symbol and store it in cache
    m_dibitCache[m_dibitIndex] = dibit;

    if (symbolIndex == 18-1) // last symbol -> launch process
    {
        int *dibitCache = &m_dibitCache[m_dibitIndex - symbolIndex]; // move back to start of corresponding cache section

        for (int i = 0; i < 18; i++)
        {
            int dibit = dibitCache[i];

            ambe_fr2[*w][*x] = (1 & (dibit >> 1)); // bit 1
            ambe_fr2[*y][*z] = (1 & dibit);        // bit 0
            w++;
            x++;
            y++;
            z++;
        }

        if (mutecurrentslot == 0)
        {
            if (m_dsdDecoder->m_state.firstframe == 1)
            { // we don't know if anything received before the first sync after no carrier is valid
                m_dsdDecoder->m_state.firstframe = 0;
            }
            else
            {
                if (m_dsdDecoder->m_opts.errorbars == 1) {
                    fprintf(stderr, "\nMBE: ");
                }

                m_dsdDecoder->m_mbeDecoder.processFrame(0, ambe_fr, 0);
                if (m_dsdDecoder->m_opts.errorbars == 1) {
                    fprintf(stderr, ".");
                }

                m_dsdDecoder->m_mbeDecoder.processFrame(0, ambe_fr2, 0);
                if (m_dsdDecoder->m_opts.errorbars == 1) {
                    fprintf(stderr, ".");
                }
            }
        }
    }
}

void DSDDMRVoice::processSlot6(int symbolIndex) // Slot6 is a 36 symbol slot
{
    int dibit = m_dsdDecoder->getDibit(); // get dibit from symbol and store it in cache
    m_dibitCache[m_dibitIndex] = dibit;

    if (symbolIndex == 36-1) // last symbol -> launch process
    {
        int *dibitCache = &m_dibitCache[m_dibitIndex - symbolIndex]; // move back to start of corresponding cache section

        // current slot frame 3
        w = rW;
        x = rX;
        y = rY;
        z = rZ;

        for (int i = 0; i < 36; i++)
        {
            int dibit = dibitCache[i];

            ambe_fr3[*w][*x] = (1 & (dibit >> 1)); // bit 1
            ambe_fr3[*y][*z] = (1 & dibit);        // bit 0
            w++;
            x++;
            y++;
            z++;
        }

        if (mutecurrentslot == 0)
        {
            m_dsdDecoder->m_mbeDecoder.processFrame(0, ambe_fr3, 0);

            if (m_dsdDecoder->m_opts.errorbars == 1) {
                fprintf(stderr, "\n");
            }
        }
    }
}

void DSDDMRVoice::processSlot7(int symbolIndex) // Slot7 is a 12 symbol slot
{
    int dibit = m_dsdDecoder->getDibit(); // get dibit from symbol and store it in cache
    m_dibitCache[m_dibitIndex] = dibit;

    if (symbolIndex == 12-1) // last symbol -> launch process
    {
        int *dibitCache = &m_dibitCache[m_dibitIndex - symbolIndex]; // move back to start of corresponding cache section
        // CACH
        for (int i = 0; i < 12; i++)
        {
            int dibit = dibitCache[i];
            cachdata[i] = dibit;
        }

        cachdata[12] = 0;
    }
}

void DSDDMRVoice::processSlot9(int symbolIndex) // Slot9 is a 24 symbol slot
{
    int dibit = m_dsdDecoder->getDibit(); // get dibit from symbol and store it in cache
    m_dibitCache[m_dibitIndex] = dibit;

    if (symbolIndex == 24-1) // last symbol -> launch process
    {
        int *dibitCache = &m_dibitCache[m_dibitIndex - symbolIndex]; // move back to start of corresponding cache section

        for (int i = 0; i < 24; i++)
        {
            int dibit = dibitCache[i];

            syncdata[i] = dibit;
            sync[i] = (dibit | 1) + 48;
        }

        sync[24] = 0;
        syncdata[24] = 0;
    }
}

} // namespace dsdplus
