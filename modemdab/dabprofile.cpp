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

#include <atomic>
#include <cstdlib>

#include "dabprofile.h"

namespace modemdab
{

namespace
{

// Nanoseconds, as atomics: stages may run on different threads
std::atomic<long long> totals[DABProfile::StageCount];

} // anonymous namespace

bool DABProfile::enabled()
{
    static const bool on = std::getenv("MODEMDAB_PROFILE") != nullptr;
    return on;
}

void DABProfile::add(Stage stage, double seconds)
{
    totals[stage] += static_cast<long long>(seconds * 1.0e9);
}

double DABProfile::seconds(Stage stage)
{
    return totals[stage] * 1.0e-9;
}

const char *DABProfile::name(Stage stage)
{
    static const char *names[StageCount] = {
        "band filter", "acquisition", "mixing", "symbol FFTs", "demodulation",
        "FIC", "MSC", "audio framing", "audio decoding", "data"
    };
    return names[stage];
}

void DABProfile::reset()
{
    for (auto& t : totals) {
        t = 0;
    }
}

} // namespace modemdab
