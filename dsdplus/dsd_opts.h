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

#ifndef DSDPLUS_DSD_OPTS_H_
#define DSDPLUS_DSD_OPTS_H_

namespace DSDplus
{

class DSDOpts
{
public:
    DSDOpts();
    ~DSDOpts();

    int onesymbol;
    int errorbars;
    int datascope;
    int symboltiming;
    int verbose;
    int p25enc;
    int p25lc;
    int p25status;
    int p25tg;
    int scoperate;
    int split;
    int playoffset;
    float audio_gain;
    int audio_out;
    int resume;
    int frame_dstar;
    int frame_x2tdma;
    int frame_p25p1;
    int frame_nxdn48;
    int frame_nxdn96;
    int frame_dmr;
    int frame_provoice;
    int mod_c4fm;
    int mod_qpsk;
    int mod_gfsk;
    int uvquality;
    int inverted_x2tdma;
    int inverted_dmr;
    int mod_threshold;
    int ssize;
    int msize;
    int delay;
    int use_cosine_filter;
    int unmute_encrypted_p25;
    int upsample; //!< force audio upsampling to 48k
    int stereo;   //!< double each audio sample to produce L+R channels
};

} // namespace dsdplus

#endif /* DSDPLUS_DSD_OPTS_H_ */
