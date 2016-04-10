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

#ifndef DSDPLUS_DMR_DATA_H_
#define DSDPLUS_DMR_DATA_H_

namespace DSDplus
{

class DSDDecoder;

class DSDDMRData
{
public:
    DSDDMRData(DSDDecoder *dsdDecoder);
    ~DSDDMRData();

    void init();
    void process();

private:
    void preProcess();  //!< process the 90 in memory dibits in initial phase

    DSDDecoder *m_dsdDecoder;
    int m_symbolIndex; //!< Current absolute symbol index
    int *dibit_p;
    char sync[25];
    char syncdata[25];
    char cachdata[13];
    char cc[5];
    char bursttype[5];
};

}

#endif /* DSDPLUS_DMR_DATA_H_ */
