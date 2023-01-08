///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// This is the code from ft8mon: https://github.com/rtmrtmrtmrtm/ft8mon          //
// written by Robert Morris, AB1HL                                               //
// reformatted and adapted to Qt and SDRangel context                            //
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
#ifndef ft8_h
#define ft8_h

namespace FT8 {
// Callback function to get the results
typedef int (*cb_t)(
    int *a91,
    float hz0,
    float hz1,
    float off,
    const char *,
    float snr,
    int pass,
    int correct_bits
);
// same as Python class CDECODE
//
struct cdecode
{
    float hz0;
    float hz1;
    float off;
    int *bits; // 174
};

void entry(
    float xsamples[],
    int nsamples,
    int start,
    int rate,
    float min_hz,
    float max_hz,
    int hints1[],
    int hints2[],
    float time_left,
    float total_time_left,
    cb_t cb,
    int,
    struct cdecode *
);

float set(char *param, char *val);
} // namespace FT8

#endif
