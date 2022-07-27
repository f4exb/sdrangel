// Copyright 2021 Rob Riggs <rob@mobilinkd.com>
// All rights reserved.

#pragma once

#include "IirFilter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

#include "export.h"

namespace modemm17 {

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
class MODEMM17_API FreqDevEstimator
{
public:
    using sample_filter_t = BaseIirFilter<3>;

	void reset()
	{
		min_est_ = 0.0f;
		max_est_ = 0.0f;
		min_var_ = 0.0f;
		max_var_ = 0.0f;
		min_count_ = 0;
		max_count_ = 0;
		min_cutoff_ = 0.0f;
		max_cutoff_ = 0.0f;
	}

	void sample(float sample)
	{
		if (sample < 1.5 * min_est_)
		{
			min_count_ = 1;
			min_est_ = sample;
			min_var_ = 0.0f;
			min_cutoff_ = min_est_ * 0.666666f;
		}
		else if (sample < min_cutoff_)
		{
			min_count_ += 1;
			min_est_ += sample;
			float var = (min_est_ / min_count_) - sample;
			min_var_ += var * var;
		}
		else if (sample > 1.5f * max_est_)
		{
			max_count_ = 1;
			max_est_ = sample;
			max_var_ = 0.0f;
			max_cutoff_ = max_est_ * 0.666666f;
		}
		else if (sample > max_cutoff_)
		{
			max_count_ += 1;
			max_est_ += sample;
			float var = (max_est_ / max_count_) - sample;
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
		if (max_count_ < 2 || min_count_ < 2) {
            return;
        }

		float max_ = max_est_ / max_count_;
		float min_ = min_est_ / min_count_;
		deviation_ = (max_ - min_) / 6.0;
		offset_ =  dc_filter_(std::max(std::min(max_ + min_, deviation_ * MAX_DC_ERROR), deviation_ * -MAX_DC_ERROR));
		error_ = (std::sqrt(max_var_ / (max_count_ - 1)) + std::sqrt(min_var_ / (min_count_ - 1))) * 0.5;

        if (deviation_ > 0) {
            idev_ = 1.0 / deviation_;
        }

        min_cutoff_ = offset_ - deviation_ * 2;
		max_cutoff_ = offset_ + deviation_ * 2;
		max_est_ = max_;
		min_est_ = min_;
		max_count_ = 1;
		min_count_ = 1;
		max_var_ = 0.0f;
		min_var_ = 0.0f;
	}

	float deviation() const { return deviation_; }
	float offset() const { return offset_; }
	float error() const { return error_; }
	float idev() const { return idev_; }

private:
    // IIR with Nyquist of 1/4.
    static const std::array<float, 3> dc_b;
    static const std::array<float, 3> dc_a;
    static const float MAX_DC_ERROR;

    float min_est_ = 0.0f;
	float max_est_ = 0.0f;
	float min_cutoff_ = 0.0f;
	float max_cutoff_ = 0.0f;
	float min_var_ = 0.0f;
	float max_var_ = 0.0f;
	size_t min_count_ = 0;
	size_t max_count_ = 0;
	float deviation_ = 0.0f;
	float offset_ = 0.0f;
	float error_ = 0.0f;
	float idev_ = 1.0f;
    sample_filter_t dc_filter_{dc_b, dc_a};
};

} // modemm17
