// Copyright 2021 Rob Riggs <rob@mobilinkd.com>
// All rights reserved.

#pragma once

#include "IirFilter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace mobilinkd {

/**
 * Deviation and zero-offset estimator.
 *
 * Accepts samples which are periodically used to update estimates of the
 * input signal deviation and zero offset.
 *
 * Samples must be provided at the ideal sample point (the point with the
 * peak bit energy).
 *
 * Estimates are expected to be updated at each sync word.  But they can
 * be updated more frequently, such as during the preamble.
 */
template <typename FloatType>
class FreqDevEstimator
{
    using sample_filter_t = BaseIirFilter<FloatType, 3>;

    // IIR with Nyquist of 1/4.
    static const std::array<FloatType, 3> dc_b;
    static const std::array<FloatType, 3> dc_a;

    static constexpr FloatType MAX_DC_ERROR = 0.2;

    FloatType min_est_ = 0.0;
	FloatType max_est_ = 0.0;
	FloatType min_cutoff_ = 0.0;
	FloatType max_cutoff_ = 0.0;
	FloatType min_var_ = 0.0;
	FloatType max_var_ = 0.0;
	size_t min_count_ = 0;
	size_t max_count_ = 0;
	FloatType deviation_ = 0.0;
	FloatType offset_ = 0.0;
	FloatType error_ = 0.0;
	FloatType idev_ = 1.0;
    sample_filter_t dc_filter_{dc_b, dc_a};

public:

	void reset()
	{
		min_est_ = 0.0;
		max_est_ = 0.0;
		min_var_ = 0.0;
		max_var_ = 0.0;
		min_count_ = 0;
		max_count_ = 0;
		min_cutoff_ = 0.0;
		max_cutoff_ = 0.0;
	}

	void sample(FloatType sample)
	{
		if (sample < 1.5 * min_est_)
		{
			min_count_ = 1;
			min_est_ = sample;
			min_var_ = 0.0;
			min_cutoff_ = min_est_ * 0.666666;
		}
		else if (sample < min_cutoff_)
		{
			min_count_ += 1;
			min_est_ += sample;
			FloatType var = (min_est_ / min_count_) - sample;
			min_var_ += var * var;
		}
		else if (sample > 1.5 * max_est_)
		{
			max_count_ = 1;
			max_est_ = sample;
			max_var_ = 0.0;
			max_cutoff_ = max_est_ * 0.666666;
		}
		else if (sample > max_cutoff_)
		{
			max_count_ += 1;
			max_est_ += sample;
			FloatType var = (max_est_ / max_count_) - sample;
			max_var_ += var * var;
		}
	}

	/**
	 * Update the estimates for deviation, offset, and EVM (error).  Note
	 * that the estimates for error are using a sloppy implementation for
	 * calculating variance to reduce the memory requirements.  This is
	 * because this is designed for embedded use.
	 */
	void update()
	{
		if (max_count_ < 2 || min_count_ < 2) return;
		FloatType max_ = max_est_ / max_count_;
		FloatType min_ = min_est_ / min_count_;
		deviation_ = (max_ - min_) / 6.0;
		offset_ =  dc_filter_(std::max(std::min(max_ + min_, deviation_ * MAX_DC_ERROR), deviation_ * -MAX_DC_ERROR));
		error_ = (std::sqrt(max_var_ / (max_count_ - 1)) + std::sqrt(min_var_ / (min_count_ - 1))) * 0.5;
		if (deviation_ > 0) idev_ = 1.0 / deviation_;
		min_cutoff_ = offset_ - deviation_ * 2;
		max_cutoff_ = offset_ + deviation_ * 2;
		max_est_ = max_;
		min_est_ = min_;
		max_count_ = 1;
		min_count_ = 1;
		max_var_ = 0.0;
		min_var_ = 0.0;
	}

	FloatType deviation() const { return deviation_; }
	FloatType offset() const { return offset_; }
	FloatType error() const { return error_; }
	FloatType idev() const { return idev_; }
};

} // mobilinkd
