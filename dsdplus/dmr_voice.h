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

/*
 * DMR voice frames
 * 6 Major blocks of 10 slots. 2 groups of slots of 144 bytes each.
 * Map is as follows in number of bytes
 *
 *      0    ...    5
 * A 0  54   ...    54 <- AMBE DMR slot 0 frame 2 1/2 + frame 3: skipped
 *   1  12          12 <- CACH data
 *   2  36          36 <- AMBE DMR slot 1 frame 1
 *   3  18          18 <- AMBE DMR slot 1 frame 2 1/2
 *   4  24          24 <- SYNC data
 * B 5  18          18 <- AMBE DMR slot 1 frame 2 1/2
 *   6  36          36 <- AMBE DMR slot 1 frame 3
 *   7  12          12 <- CACH data
 *   8  54          54 <- AMBE DMR slot 0 frame 1 + frame 2 1/2: skipped
 *   9  24          24 <- SYNC data
 *
 * The A group of the first major block is already in memory and is processed
 * at initialization time
 * Then dibits for each slot are stored in cache and processed right after the
 * last dibit for the slot has been added.
 * For skipped slots the dibits are simply thrown away
 *
 * The DMR slot 0 is ignored. Only the DMR slot 1 is processed (listening to one conversation at a time)
 *
 */

#ifndef DSDPLUS_DMR_VOICE_H_
#define DSDPLUS_DMR_VOICE_H_

namespace DSDplus
{

class DSDDecoder;

class DSDDMRVoice
{
public:
    DSDDMRVoice(DSDDecoder *dsdDecoder);
    ~DSDDMRVoice();

    void init();
    void process();

private:
    int getSlotIndex(int symbolInMajorBlockIndex) //!< calculates current slot index and returns symbol index in the slot
    {
        if (m_majorBlock > 5) // this is the post-process case
        {
            m_slotIndex = 10;
            return symbolInMajorBlockIndex;
        }

        if (symbolInMajorBlockIndex < 54)
        {
            m_slotIndex = 0;
            return symbolInMajorBlockIndex;
        }
        else if (symbolInMajorBlockIndex < 54+12)
        {
            m_slotIndex = 1;
            return symbolInMajorBlockIndex - 54;
        }
        else if (symbolInMajorBlockIndex < 54+12+36)
        {
            m_slotIndex = 2;
            return symbolInMajorBlockIndex - 54+12;
        }
        else if (symbolInMajorBlockIndex < 54+12+36+18)
        {
            m_slotIndex = 3;
            return symbolInMajorBlockIndex - 54+12+36;
        }
        else if (symbolInMajorBlockIndex < 54+12+36+18+24)
        {
            m_slotIndex = 4;
            return symbolInMajorBlockIndex - 54+12+36+18;
        }
        else if (symbolInMajorBlockIndex < 54+12+36+18+24+18)
        {
            m_slotIndex = 5;
            return symbolInMajorBlockIndex - 54+12+36+18+24;
        }
        else if (symbolInMajorBlockIndex < 54+12+36+18+24+18+36)
        {
            m_slotIndex = 6;
            return symbolInMajorBlockIndex - 54+12+36+18+24+18;
        }
        else if (symbolInMajorBlockIndex < 54+12+36+18+24+18+36+12)
        {
            m_slotIndex = 7;
            return symbolInMajorBlockIndex - 54+12+36+18+24+18+36;
        }
        else if (symbolInMajorBlockIndex < 54+12+36+18+24+18+36+12+54)
        {
            m_slotIndex = 8;
            return symbolInMajorBlockIndex - 54+12+36+18+24+18+36+12;
        }
        else if (symbolInMajorBlockIndex < 54+12+36+18+24+18+36+12+54+24)
        {
            m_slotIndex = 9;
            return symbolInMajorBlockIndex - 54+12+36+18+24+18+36+12+54;
        }
        else // cannot go there if using this function in its valid context (input is a remainder of division by 288)
        {
            m_slotIndex = -1; // invalid slot
            return 0;
        }
    }

    void preProcess();  //!< process the 144 in memory dibits in initial phase
    void postProcess(int symbolIndex); //!< skip 54 + 12 + 54 symbols at the end of the last slot (block 5 slot 9)
    void processSlot0(int symbolIndex);
    void processSlot1(int symbolIndex);
    void processSlot2(int symbolIndex);
    void processSlot3(int symbolIndex);
    void processSlot4(int symbolIndex);
    void processSlot5(int symbolIndex);
    void processSlot6(int symbolIndex);
    void processSlot7(int symbolIndex);
    void processSlot8(int symbolIndex);
    void processSlot9(int symbolIndex);

    DSDDecoder *m_dsdDecoder;
    // extracts AMBE frames from DMR frame
    int m_symbolIndex; //!< Current absolute symbol index
    int m_slotIndex;   //!< Slot index in major block 0..9 //i;
    int m_majorBlock;  //!< Major block index 0..5 //j;
    int *dibit_p;
    char ambe_fr[4][24];
    char ambe_fr2[4][24];
    char ambe_fr3[4][24];
    const int *w, *x, *y, *z;
    char sync[25];
    char syncdata[25];
    char cachdata[13];
    int mutecurrentslot;
    int msMode;
    int m_dibitCache[288]; // has to handle a complete frame (288 dibits)
    int m_dibitIndex;      // index in dibit cache

    static const int rW[36];
    static const int rX[36];
    static const int rY[36];
    static const int rZ[36];
};

}

#endif /* DSDPLUS_DMR_VOICE_H_ */
