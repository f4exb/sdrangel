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

#ifndef DSDPLUS_DSD_MBE_H_
#define DSDPLUS_DSD_MBE_H_

namespace DSDplus
{

class DSDDecoder;

class DSDMBEDecoder
{
public:
    DSDMBEDecoder(DSDDecoder *dsdDecoder);
    ~DSDMBEDecoder();

    void processFrame(char imbe_fr[8][23], char ambe_fr[4][24], char imbe7100_fr[7][24]);

private:
    void processAudio();
    void upsample(float invalue);

    DSDDecoder *m_dsdDecoder;
    char imbe_d[88];
    char ambe_d[49];

};

}

#endif /* DSDPLUS_DSD_MBE_H_ */
