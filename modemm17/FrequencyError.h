// Copyright 2020 Mobilinkd LLC.

#pragma once

#include "IirFilter.h"

#include <array>
#include <algorithm>
#include <numeric>

namespace modemm17
{

template <size_t N = 32>
struct FrequencyError
{
    FrequencyError() :
    {
        samples_.fill(0.0f);
    }

    auto operator()(float sample)
    {
        float evm = 0;
        bool use = true;

        if (sample > 2)
        {
            evm = sample - 3;
        }
        else if (sample >= -2)
        {
            use = false;
        }
        else
        {
            evm = sample + 3;
        }

        if (use)
        {
            accum_ = accum_ - samples_[index_] + evm;
            samples_[index_++] = evm;
            if (index_ == N) index_ = 0;
        }

        return filter_(accum_ / N);
    }

private:
    static constexpr std::array<float, 3> evm_b{0.02008337, 0.04016673, 0.02008337};
    static constexpr std::array<float, 3> evm_a{1.0, -1.56101808, 0.64135154};

    std::array<float, N> samples_;
    size_t index_ = 0;
    float accum_ = 0.0f;
    BaseIirFilter<3> filter_{makeIirFilter(evm_b, evm_a)};

    const float ZERO = 0.0f;
};

} // modemm17
