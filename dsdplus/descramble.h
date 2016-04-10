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

#ifndef DSDPLUS_DESCRAMBLE_H_
#define DSDPLUS_DESCRAMBLE_H_

namespace DSDplus
{

class Descramble
{
public:
//    Descramble();
//    ~Descramble();

    static void scramble (int * in,int * out);
    static void deinterleave (int * in, int * out);
    static int FECdecoder (int * in, int * out);

private:
    static int traceBack (int * out, int * m_pathMemory0, int * m_pathMemory1, int * m_pathMemory2, int * m_pathMemory3);
    static void viterbiDecode (int n, int *data, int *m_pathMemory0, int *m_pathMemory1, int *m_pathMemory2, int *m_pathMemory3, int *m_pathMetric);

    static const int SCRAMBLER_TABLE_BITS_LENGTH=720;
    static const int SCRAMBLER_TABLE_BITS[];
};


} // namespace DSDplus

#endif /* DSDPLUS_DESCRAMBLE_H_ */
