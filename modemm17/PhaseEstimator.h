// Copyright 2020 Mobilinkd LLC.

#pragma once

#include <array>
#include <algorithm>
#include <cassert>

namespace modemm17
{

/**
 * Estimate the phase of a sample by estimating the
 * tangent of the sample point.  This is done by computing
 * the magnitude difference of the previous and following
 * samples.  We do not correct for 0-crossing errors because
 * these errors have not affected the performance of clock
 * recovery.
 */
struct PhaseEstimator
{
    using float_type = float;
    using samples_t = std::array<float, 3>;    // 3 samples in length

    float_type dx_;

    PhaseEstimator(float sample_rate, float symbol_rate)
    : dx_(2.0 * symbol_rate / sample_rate)
    {}

    /**
     * This performs a rolling estimate of the phase.
     *
     * @param samples are three samples centered around the current sample point
     *  (t-1, t, t+1).
     */
    float_type operator()(const samples_t& samples)
    {
        assert(dx_ > 0.0);

        float ratio = ((samples.at(2) - samples.at(0)) / 3.0) / dx_;
        // Clamp +/-5.
        ratio = std::min(float(5.0), ratio);
        ratio = std::max(float(-5.0), ratio);

        return ratio;
    }
};

} // modemm17
