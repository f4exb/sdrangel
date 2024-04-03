///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
//                                                                               //
// This is the code from ft8mon: https://github.com/rtmrtmrtmrtm/ft8mon          //
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

#ifndef _ft8stats_h_
#define _ft8stats_h_

#include <vector>

#include "export.h"

namespace FT8 {
//
// manage statistics for soft decoding, to help
// decide how likely each symbol is to be correct,
// to drive LDPC decoding.
//
// meaning of the how (problt_how) parameter:
// 0: gaussian
// 1: index into the actual distribution
// 2: do something complex for the tails.
// 3: index into the actual distribution plus gaussian for tails.
// 4: similar to 3.
// 5: laplace
//
class FT8_API Stats
{
public:
    std::vector<float> a_;
    float sum_;
    bool finalized_;
    float mean_;   // cached
    float stddev_; // cached
    float b_;      // cached
    int how_;

public:
    Stats(int how, float log_tail, float log_rate);
    void add(float x);
    void finalize();
    float mean();
    float stddev();

    // fraction of distribution that's less than x.
    // assumes normal distribution.
    // this is PHI(x), or the CDF at x,
    // or the integral from -infinity
    // to x of the PDF.
    float gaussian_problt(float x);
    // https://en.wikipedia.org/wiki/Laplace_distribution
    // m and b from page 116 of Mark Owen's Practical Signal Processing.
    float laplace_problt(float x);
    // look into the actual distribution.
    float problt(float x);

private:
    float log_tail_;
    float log_rate_;
};

} //namespace FT8

#endif // _ft8stats_h_
