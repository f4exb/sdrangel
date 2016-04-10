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
 * A 0  54   ...    54 <- this one is always skipped
 *   1  12          12 <- cache data
 *   2  36          36 <- AMBE slot
 *   3  18          18 <- AMBE slot
 *   4  24          24 <- sync data
 * B 5  18          18 <- AMBE slot
 *   6  36          36 <- AMBE slot
 *   7  12          12 <- cache data
 *   8  54          54 <- this one is always skipped
 *   9  24          24 <- sync data
 *
 * The A gtoup of the first major block is already in memory and is processed
 * at initialization time
 * Then dibits for each slot are stored in cache and processed right after the
 * last dibit for the slot has been added.
 * For skipped slots the dibits are simply thrown away
 *
 */

#ifndef DSDPLUS_DMR_VOICE_H_
#define DSDPLUS_DMR_VOICE_H_

namespace DSDPlus
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

    DSDDecoder *m_dsdDecoder;
    // extracts AMBE frames from DMR frame
    int m_slotIndex;  //!< Slot index in major block 0..9 //i;
    int m_majorBlock; //!< Major block index 0..5 //j;
    int dibit;
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
    int m_dibitCache[54]; // biggest slot is 54 dibits
    int m_dibitIndex;     // index in dibit cache
};

}

#endif /* DSDPLUS_DMR_VOICE_H_ */
