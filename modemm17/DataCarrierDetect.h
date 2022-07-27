// Copyright 2021 Mobilinkd LLC.

#pragma once

#include "SlidingDFT.h"

#include <array>
#include <complex>
#include <cstddef>

namespace modemm17 {

/**
 * Data carrier detection using the difference of two DFTs, one in-band and
 * one out-of-band.  The first frequency is the in-band frequency and the
 * second one is the out-of-band Frequency.  The second frequency must be
 * within the normal passband of the receiver, but beyond the normal roll-off
 * frequency of the data carrier.
 *
 * This version uses the NSlidingDFT implementation to reduce the memory
 * footprint.
 *
 * As an example, the cut-off for 4.8k symbol/sec 4-FSK is 2400Hz, so 3000Hz
 * is a reasonable out-of-band frequency to use.
 *
 * Note: the input to this DCD must be unfiltered (raw) baseband input.
 */
template <size_t SampleRate, size_t Accuracy = 1000>
struct DataCarrierDetect
{
    NSlidingDFT<SampleRate, SampleRate / Accuracy, 2> dft_;
    float ltrigger_;
    float htrigger_;
    float level_1 = 0.0;
    float level_2 = 0.0;
    float level_ = 0.0;
    bool triggered_ = false;

    DataCarrierDetect(
        size_t freq1,
        size_t freq2,
        float ltrigger = 2.0,
        float htrigger = 5.0
    ) :
        dft_({freq1, freq2}),
        ltrigger_(ltrigger),
        htrigger_(htrigger)
    {
    }

    /**
     * Accept unfiltered baseband input and output a decision on whether
     * a carrier has been detected after every @tparam BlockSize inputs.
     */
    void operator()(float sample)
    {
        auto result = dft_(sample);
        level_1 += std::norm(result[0]);
        level_2 += std::norm(result[1]);
    }

    /**
     * Update the data carrier detection level.
     */
    void update()
    {
    	level_ = level_ * 0.8 + 0.2 * (level_1 / level_2);
    	level_1 = 0.0;
    	level_2 = 0.0;
        triggered_ = triggered_ ? level_ > ltrigger_ : level_ > htrigger_;
    }


    float level() const { return level_; }
    bool dcd() const { return triggered_; }
};

} // modemm17
