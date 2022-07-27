// Copyright 2020 Mobilinkd LLC.

#pragma once

#include <array>
#include <algorithm>
#include <numeric>

namespace modemm17
{

template <size_t N = 10>
struct DeviationError
{
    DeviationError()
    {
        minima_.fill(0.0f);
        maxima_.fill(0.0f);
    }

    float operator()(float sample)
    {
        if (sample > ZERO)
        {
            if (sample > max_estimate_ * 0.67 or max_count_ == 5)
            {
                max_count_ = 0;
                maxima_[max_index_++] = sample;
                if (max_index_ == N)
                {
                    max_rolled_ = true;
                    max_index_ = 0;
                }
                if (max_rolled_)
                {
                    max_estimate_ = std::accumulate(std::begin(maxima_), std::end(maxima_), ZERO) / N;
                }
                else
                {
                    max_estimate_ = std::accumulate(std::begin(maxima_), std::begin(maxima_) + max_index_, ZERO) / max_index_;
                }
            }
            else
            {
                ++max_count_;
            }
        }
        else if (sample < 0)
        {
            if (sample < min_estimate_ * 0.67 or min_count_ == 5)
            {
                min_count_ = 0;
                minima_[min_index_++] = sample;
                if (min_index_ == N)
                {
                    min_rolled_ = true;
                    min_index_ = 0;
                }
                if (min_rolled_)
                {
                    min_estimate_ = std::accumulate(std::begin(minima_), std::end(minima_), ZERO) / N;
                }
                else
                {
                    min_estimate_ = std::accumulate(std::begin(minima_), std::begin(minima_) + min_index_, ZERO) / min_index_;
                }
            }
            else
            {
                ++min_count_;
            }
        }

        float deviation = max_estimate_ - min_estimate_;
        float deviation_error = std::min(6.0f / deviation, 100.0f);
        return deviation_error;
    }

private:
    std::array<float, N> minima_;
    std::array<float, N> maxima_;
    size_t min_index_ = 0;
    size_t max_index_ = 0;
    bool min_rolled_ = false;
    bool max_rolled_ = false;
    size_t min_count_ = 0;
    size_t max_count_ = 0;
    float min_estimate_ = 0.0f;
    float max_estimate_ = 0.0f;

    const float ZERO = 0.0f;
};

} // modemm17
