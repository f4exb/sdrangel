// Copyright 2020 Mobilinkd LLC.

#pragma once

#include "IirFilter.h"

#include <array>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <tuple>

namespace modemm17
{

struct CarrierDetect
{
    using result_t = std::tuple<bool, float>;

    BaseIirFilter<3> filter_;
    float lock_;
    float unlock_;
    bool locked_ = false;

    CarrierDetect(std::array<float, 3> const& b, std::array<float, 3> const& a, float lock_level, float unlock_level)
    : filter_(b, a), lock_(lock_level), unlock_(unlock_level)
    {
    }

    result_t operator()(float value)
    {
        auto filtered = filter_(std::abs(value));
        if (locked_ && (filtered > unlock_)) locked_ = false;
        else if (!locked_ && (filtered < lock_)) locked_ = true;

        return std::make_tuple(locked_, filtered);
    }
};

} // modemm17
