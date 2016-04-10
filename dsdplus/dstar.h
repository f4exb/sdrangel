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

#ifndef DSDPLUS_DSTAR_H_
#define DSDPLUS_DSTAR_H_

namespace DSDplus
{

class DSDDecoder;

class DSDDstar
{
public:
    DSDDstar(DSDDecoder *dsdDecoder);
    ~DSDDstar();

    void init();
    void process();
    void processHD();

private:
    void initVoiceFrame();
    void initDataFrame();
    void processVoice();
    void processData();
    void dstar_header_decode();

    DSDDecoder *m_dsdDecoder;
    int m_symbolIndex;    //!< Current symbol index in non HD sequence
    int m_symbolIndexHD;  //!< Current symbol index in HD sequence
    int m_dibitCache[97]; // has to handle a voice + data frame (97 dibits)
    int m_dibitIndex;     // index in dibit cache

    // DSTAR
    char ambe_fr[4][24];
    unsigned char data[9];
    unsigned int bits[4];
    int framecount;
    int sync_missed;
    unsigned char slowdata[4];
    unsigned int bitbuffer;
    const int *w, *x;

    // DSTAR-HD
    int radioheaderbuffer[660];

    // constants
    static const int dW[72];
    static const int dX[72];
};

} // namespace DSDplus

#endif /* DSDPLUS_DSTAR_H_ */
