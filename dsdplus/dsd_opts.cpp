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

#include "dsd_opts.h"

namespace DSDplus
{

DSDOpts::DSDOpts()
{
    onesymbol = 10;
    errorbars = 1;
    datascope = 0;
    symboltiming = 0;
    verbose = 2;
    p25enc = 0;
    p25lc = 0;
    p25status = 0;
    p25tg = 0;
    scoperate = 15;
    split = 0;
    playoffset = 0;
    audio_gain = 0;
    audio_out = 1;
    resume = 0;
    frame_dstar = 0;
    frame_x2tdma = 1;
    frame_p25p1 = 1;
    frame_nxdn48 = 0;
    frame_nxdn96 = 1;
    frame_dmr = 1;
    frame_provoice = 0;
    mod_c4fm = 1;
    mod_qpsk = 1;
    mod_gfsk = 1;
    uvquality = 3;
    inverted_x2tdma = 1; // most transmitter + scanner + sound card combinations show inverted signals for this
    inverted_dmr = 0; // most transmitter + scanner + sound card combinations show non-inverted signals for this
    mod_threshold = 26;
    ssize = 36;
    msize = 15;
    delay = 0;
    use_cosine_filter = 1;
    unmute_encrypted_p25 = 0;
    upsample = 0;
    stereo = 0;
}

DSDOpts::~DSDOpts()
{
}

} // namespace dsdplus

