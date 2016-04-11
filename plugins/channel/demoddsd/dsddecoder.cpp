///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include <QtGlobal>
#include "dsddecoder.h"
#include "audio/audiofifo.h"


DSDDecoder::DSDDecoder()
{
    DSDplus::DSDOpts  *dsdopts = m_decoder.getOpts();
    DSDplus::DSDState *dsdstate = m_decoder.getState();

    dsdopts->split = 1;
    dsdopts->upsample = 1; // force upsampling of audio to 48k
    dsdopts->playoffset = 0;
    dsdopts->delay = 0;

    // Initialize with auto-detect:
    dsdopts->frame_dstar = 1;
    dsdopts->frame_x2tdma = 1;
    dsdopts->frame_p25p1 = 1;
    dsdopts->frame_nxdn48 = 0;
    dsdopts->frame_nxdn96 = 1;
    dsdopts->frame_dmr = 1;
    dsdopts->frame_provoice = 0;

    dsdopts->uvquality = 3; // This is gr-dsd default
    dsdopts->verbose = 2;   // This is gr-dsd default
    dsdopts->errorbars = 1; // This is gr-dsd default

    // Initialize with auto detection of modulation optimization:
    dsdopts->mod_c4fm = 1;
    dsdopts->mod_qpsk = 1;
    dsdopts->mod_gfsk = 1;
    dsdstate->rf_mod = 0;

    dsdstate->output_offset = 0;
    dsdopts->upsample = 1;
    dsdopts->stereo = 1;
}

DSDDecoder::~DSDDecoder()
{
}
