// Copyright 2021 Mobilinkd LLC.

#pragma once

#include <vector>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <cassert>

#include "export.h"

namespace modemm17
{

/**
 * Calculate the phase estimates for each sample position.
 *
 * This performs a running calculation of the phase of each bit position.
 * It is very noisy for individual samples, but quite accurate when
 * averaged over an entire M17 frame.
 *
 * It is designed to be used to calculate the best bit position for each
 * frame of data.  Samples are collected and averaged.  When update() is
 * called, the best sample index and clock are estimated, and the counters
 * reset for the next frame.
 *
 * It starts counting bit 0 as the first bit received after a reset.
 *
 * This is very efficient as it only uses addition and subtraction for
 * each bit sample.  And uses one multiply and divide per update (per
 * frame).
 *
 * This will permit a clock error of up to 500ppm.  This allows up to
 * 250ppm error for both transmitter and receiver clocks.  This is
 * less than one sample per frame when the sample rate is 48000 SPS.
 *
 * @inv current_index_ is in the interval [0, SAMPLES_PER_SYMBOL).
 * @inv sample_index_ is in the interval [0, SAMPLES_PER_SYMBOL).
 * @inv clock_ is in the interval [0.9995, 1.0005]
 */
class MODEMM17_API ClockRecovery
{
    std::vector<float> estimates_;
    size_t sample_count_ = 0;
    uint16_t frame_count_ = 0;
    uint8_t sample_index_ = 0;
    uint8_t prev_sample_index_ = 0;
    uint8_t index_ = 0;
    float offset_ = 0.0;
    float clock_ = 1.0;
    float prev_sample_ = 0.0;

    /**
     * Find the sample index.
     *
     * There are @p SAMPLES_PER_INDEX bins.  It is expected that half are
     * positive values and half are negative.  The positive and negative
     * bins will be grouped together such that there is a single transition
     * from positive values to negative values.
     *
     * The best bit position is always the position with the positive value
     * at that transition point.  It will be the bit index with the highest
     * energy.
     *
     * @post sample_index_ contains the best sample point.
     */
    void update_sample_index_()
    {
        uint8_t index = 0;

        // Find falling edge.
        bool is_positive = false;
        for (size_t i = 0; i != samples_per_symbol_; ++i)
        {
            float phase = estimates_[i];

            if (!is_positive && phase > 0)
            {
                is_positive = true;
            }
            else if (is_positive && phase < 0)
            {
                index = i;
                break;
            }
        }

        sample_index_ = index == 0 ? samples_per_symbol_ - 1 : index - 1;
    }

    /**
     * Compute the drift in sample points from the last update.
     *
     * This should never be greater than one.
     */
    float calc_offset_()
    {
        int8_t offset = sample_index_ - prev_sample_index_;

        // When in spec, the clock should drift by less than 1 sample per frame.
        if (offset >= max_offset_)
        {
            offset -= samples_per_symbol_;
        }
        else if (offset <= -max_offset_)
        {
            offset += samples_per_symbol_;
        }

        return offset;
    }

    void update_clock_()
    {
        // update_sample_index_() must be called first.

        if (frame_count_ == 0)
        {
            prev_sample_index_ = sample_index_;
            offset_ = 0.0;
            clock_ = 1.0;
            return;
        }

        offset_ += calc_offset_();
        prev_sample_index_ = sample_index_;
        clock_ = 1.0 + (offset_ / (frame_count_ * sample_count_));
        clock_ = std::min(MAX_CLOCK, std::max(MIN_CLOCK, clock_));
    }

public:
    ClockRecovery(size_t sampleRate, size_t symbolRate) :
        sampleRate_(sampleRate),
        symbolRate_(symbolRate)
    {
        samples_per_symbol_ = sampleRate_ / symbolRate_;
        max_offset_ = samples_per_symbol_ / 2;
        dx_ = 1.0 / samples_per_symbol_;
        estimates_.resize(samples_per_symbol_);
        std::fill(estimates_.begin(), estimates_.end(), 0);
    }

    /**
     * Update clock recovery with the given sample.  This will advance the
     * current sample index by 1.
     */
    void operator()(float sample)
    {
        float dy = (sample - prev_sample_);

        if (sample + prev_sample_ < 0)
        {
            // Invert the phase estimate when sample midpoint is less than 0.
            dy = -dy;
        }

        prev_sample_ = sample;

        estimates_[index_] += dy;
        index_ += 1;
        if (index_ == samples_per_symbol_)
        {
            index_ = 0;
        }
        sample_count_ += 1;
    }

    /**
     * Reset the state of the clock recovery system.  This should be called
     * when a new transmission is detected.
     */
    void reset()
    {
        sample_count_ = 0;
        frame_count_ = 0;
        index_ = 0;
        sample_index_ = 0;
        std::fill(estimates_.begin(), estimates_.end(), 0);
    }

    /**
     * Return the current sample index.  This will always be in the range of
     * [0..SAMPLES_PER_SYMBOL).
     */
    uint8_t current_index() const
    {
        return index_;
    }

    /**
     * Return the estimated sample clock increment based on the last update.
     *
     * The value is only valid after samples have been collected and update()
     * has been called.
     */
    float clock_estimate() const
    {
        return clock_;
    }

    /**
     * Return the estimated "best sample index" based on the last update.
     *
     * The value is only valid after samples have been collected and update()
     * has been called.
     */
    uint8_t sample_index() const
    {
        return sample_index_;
    }

    /**
     * Update the sample index and clock estimates, and reset the state for
     * the next frame of data.
     *
     * @pre index_ = 0
     * @pre sample_count_ > 0
     *
     * After this is called, sample_index() and clock_estimate() will have
     * valid, updated results.
     *
     * The more samples between calls to update, the more accurate the
     * estimates will be.
     *
     * @return true if the preconditions are met and the update has been
     *  performed, otherwise false.
     */
    bool update()
    {
        if (!(sample_count_ != 0 && index_ == 0)) return false;

        update_sample_index_();
        update_clock_();

        frame_count_ = std::min(0x1000, 1 + frame_count_);
        sample_count_ = 0;
        std::fill(estimates_.begin(), estimates_.end(), 0);
        return true;
    }
private:
    size_t sampleRate_;
    size_t symbolRate_;
    size_t samples_per_symbol_;
    int8_t max_offset_;
    float dx_;

    static const float MAX_CLOCK;
    static const float MIN_CLOCK;
};

} // modemm17
