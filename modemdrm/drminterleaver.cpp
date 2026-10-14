///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include "drminterleaver.h"

#include <map>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace modemdrm
{

std::vector<std::size_t> BitInterleaver::permutation(std::size_t inputSize, int multiplier)
{
    if (inputSize == 0 || multiplier <= 0) {
        return {};
    }

    std::size_t s = 1;
    while (s < inputSize) {
        s <<= 1U;
    }

    const std::size_t q = s / 4U - 1U;
    std::vector<std::size_t> result(inputSize);
    result[0] = 0;
    for (std::size_t i = 1; i < inputSize; ++i)
    {
        std::size_t candidate = (static_cast<std::size_t>(multiplier) * result[i - 1] + q) % s;
        while (candidate >= inputSize) {
            candidate = (static_cast<std::size_t>(multiplier) * candidate + q) % s;
        }

        result[i] = candidate;
    }

    return result;
}

const std::vector<std::size_t>& BitInterleaver::cachedPermutation(std::size_t inputSize, int multiplier)
{
    // DRM uses a small set of sizes; bound the cache in case a caller does not.
    constexpr std::size_t MaximumCachedPermutations = 512;
    static std::mutex mutex;
    static std::map<std::pair<std::size_t, int>, std::vector<std::size_t>> cache;
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto found = cache.find({inputSize, multiplier});
        if (found != cache.end()) {
            return found->second;
        }

        if (cache.size() < MaximumCachedPermutations)
        {
            return cache.emplace(std::make_pair(inputSize, multiplier),
                permutation(inputSize, multiplier)).first->second;
        }
    }

    thread_local std::vector<std::size_t> uncached;
    uncached = permutation(inputSize, multiplier);
    return uncached;
}

std::vector<std::uint8_t> BitInterleaver::interleave(
    const std::vector<std::uint8_t>& input,
    int multiplier
)
{
    const std::vector<std::size_t>& order = cachedPermutation(input.size(), multiplier);
    std::vector<std::uint8_t> result(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        result[i] = input[order[i]];
    }

    return result;
}

std::vector<float> BitInterleaver::deinterleave(
    const std::vector<float>& input,
    int multiplier
)
{
    const std::vector<std::size_t>& order = cachedPermutation(input.size(), multiplier);
    std::vector<float> result(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        result[order[i]] = input[i];
    }

    return result;
}

} // namespace modemdrm
