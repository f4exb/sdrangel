///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef SDRBASE_AUDIO_AUDIOG722_H_
#define SDRBASE_AUDIO_AUDIOG722_H_

#include <stdint.h>
#include "export.h"

class SDRBASE_API AudioG722
{
public:
    AudioG722();
    ~AudioG722();
    void init(int rate, int options);
    int encode(uint8_t g722_data[], const int16_t amp[], int len);

private:
    struct g722_encode_state
    {
        /*! TRUE if the operating in the special ITU test mode, with the band split filters
                 disabled. */
        int itu_test_mode;
        /*! TRUE if the G.722 data is packed */
        int packed;
        /*! TRUE if encode from 8k samples/second */
        int eight_k;
        /*! 6 for 48000kbps, 7 for 56000kbps, or 8 for 64000kbps. */
        int bits_per_sample;

        /*! Signal history for the QMF */
        int x[24];

        struct
        {
            int s;
            int sp;
            int sz;
            int r[3];
            int a[3];
            int ap[3];
            int p[3];
            int d[7];
            int b[7];
            int bp[7];
            int sg[7];
            int nb;
            int det;
        } band[2];

        unsigned int in_buffer;
        int in_bits;
        unsigned int out_buffer;
        int out_bits;

        g722_encode_state();
        void init(int rate, int options);
    };

    int16_t saturate(int32_t amp)
    {
        int16_t amp16;

        /* Hopefully this is optimized for the common case - not clipping */
        amp16 = (int16_t) amp;
        if (amp == amp16) {
            return amp16;
        }

        if (amp > INT16_MAX) {
            return  INT16_MAX;
        }

        return  INT16_MIN;
    }

    void block4(int band, int d);

    g722_encode_state state;

    static const int q6[32];
    static const int iln[32];
    static const int ilp[32];
    static const int wl[8];
    static const int rl42[16];
    static const int ilb[32];
    static const int qm4[16];
    static const int qm2[4];
    static const int qmf_coeffs[12];
    static const int ihn[3];
    static const int ihp[3];
    static const int wh[3];
    static const int rh2[4];
};


#endif /* SDRBASE_AUDIO_AUDIOG722_H_ */
