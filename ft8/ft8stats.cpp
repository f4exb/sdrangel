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

#include <math.h>
#include <algorithm>

#include "ft8stats.h"

namespace FT8 {

Stats::Stats(int how, float log_tail, float log_rate) :
    sum_(0),
    finalized_(false),
    how_(how),
    log_tail_(log_tail),
    log_rate_(log_rate)
{}

void Stats::add(float x)
{
    a_.push_back(x);
    sum_ += x;
    finalized_ = false;
}

void Stats::finalize()
{
    finalized_ = true;

    int n = a_.size();
    mean_ = sum_ / n;
    float var = 0;
    float bsum = 0;

    for (int i = 0; i < n; i++)
    {
        float y = a_[i] - mean_;
        var += y * y;
        bsum += fabs(y);
    }

    var /= n;
    stddev_ = sqrt(var);
    b_ = bsum / n;

    // prepare for binary search to find where values lie
    // in the distribution.
    if (how_ != 0 && how_ != 5) {
        std::sort(a_.begin(), a_.end());
    }
}

float Stats::mean()
{
    if (!finalized_) {
        finalize();
    }

    return mean_;
}

float Stats::stddev()
{
    if (!finalized_) {
        finalize();
    }

    return stddev_;
}

// fraction of distribution that's less than x.
// assumes normal distribution.
// this is PHI(x), or the CDF at x,
// or the integral from -infinity
// to x of the PDF.
float Stats::gaussian_problt(float x)
{
    float SDs = (x - mean()) / stddev();
    float frac = 0.5 * (1.0 + erf(SDs / sqrt(2.0)));
    return frac;
}

// https://en.wikipedia.org/wiki/Laplace_distribution
// m and b from page 116 of Mark Owen's Practical Signal Processing.
float Stats::laplace_problt(float x)
{
    float m = mean();
    float cdf;

    if (x < m) {
        cdf = 0.5 * exp((x - m) / b_);
    } else {
        cdf = 1.0 - 0.5 * exp(-(x - m) / b_);
    }

    return cdf;
}

// look into the actual distribution.
float Stats::problt(float x)
{
    if (!finalized_) {
        finalize();
    }

    if (how_ == 0) {
        return gaussian_problt(x);
    }

    if (how_ == 5) {
        return laplace_problt(x);
    }

    // binary search.
    auto it = std::lower_bound(a_.begin(), a_.end(), x);
    int i = it - a_.begin();
    int n = a_.size();

    if (how_ == 1)
    {
        // index into the distribution.
        // works poorly for values that are off the ends
        // of the distribution, since those are all
        // mapped to 0.0 or 1.0, regardless of magnitude.
        return i / (float)n;
    }

    if (how_ == 2)
    {
        // use a kind of logistic regression for
        // values near the edges of the distribution.
        if (i < log_tail_ * n)
        {
            float x0 = a_[(int)(log_tail_ * n)];
            float y = 1.0 / (1.0 + exp(-log_rate_ * (x - x0)));
            // y is 0..0.5
            y /= 5;
            return y;
        }
        else if (i > (1 - log_tail_) * n)
        {
            float x0 = a_[(int)((1 - log_tail_) * n)];
            float y = 1.0 / (1.0 + exp(-log_rate_ * (x - x0)));
            // y is 0.5..1
            // we want (1-log_tail)..1
            y -= 0.5;
            y *= 2;
            y *= log_tail_;
            y += (1 - log_tail_);
            return y;
        }
        else
        {
            return i / (float)n;
        }
    }

    if (how_ == 3)
    {
        // gaussian for values near the edge of the distribution.
        if (i < log_tail_ * n) {
            return gaussian_problt(x);
        } else if (i > (1 - log_tail_) * n) {
            return gaussian_problt(x);
        } else {
            return i / (float)n;
        }
    }

    if (how_ == 4)
    {
        // gaussian for values outside the distribution.
        if (x < a_[0] || x > a_.back()) {
            return gaussian_problt(x);
        } else {
            return i / (float)n;
        }
    }

    return 0;
}

} // namespace FT8
