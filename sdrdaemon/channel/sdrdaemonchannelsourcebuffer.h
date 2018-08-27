///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// SDRdaemon source channel (Tx). Samples buffer                                 //
//                                                                               //
// SDRdaemon is a detached SDR front end that handles the interface with a       //
// physical device and sends or receives the I/Q samples stream to or from a     //
// SDRangel instance via UDP. It is controlled via a Web REST API.               //
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

#include <stdint.h>

#include "dsp/dsptypes.h"

class SDRDaemonChannelSourceBuffer
{
public:
    SDRDaemonChannelSourceBuffer(uint32_t nbSamples);
    ~SDRDaemonChannelSourceBuffer();
    void resize(uint32_t nbSamples);
    void write(Sample *begin, uint32_t nbSamples);
    void readOne(Sample& sample);
    int getRWBalancePercent(); //!< positive write leads, negative read leads, balance = 0, percentage of buffer size

private:
    Sample *m_buffer;
    uint32_t m_size;
    uint32_t m_rp;
    uint32_t m_wp;
};
