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

#ifndef MODEMDAB_DABPROFILE_H_
#define MODEMDAB_DABPROFILE_H_

#include <chrono>

#include "export.h"

namespace modemdab
{

// Time spent in each stage of the receiver, for performance work. Off unless the
// MODEMDAB_PROFILE environment variable is set; then each scope costs two clock reads.
class MODEMDAB_API DABProfile
{
public:
    enum Stage
    {
        BandFilter,
        Acquisition,
        Mixing,
        SymbolFFT,
        Demodulation,
        FIC,
        MSC,
        AudioFraming,
        AudioDecoding,
        Data,
        StageCount
    };

    static bool enabled();
    static void add(Stage stage, double seconds);
    static double seconds(Stage stage);
    static const char *name(Stage stage);
    static void reset();
};

class DABProfileScope
{
public:
    explicit DABProfileScope(DABProfile::Stage stage) :
        m_stage(stage),
        m_enabled(DABProfile::enabled())
    {
        if (m_enabled) {
            m_start = std::chrono::steady_clock::now();
        }
    }

    ~DABProfileScope()
    {
        if (m_enabled) {
            DABProfile::add(m_stage, std::chrono::duration<double>(std::chrono::steady_clock::now() - m_start).count());
        }
    }

private:
    DABProfile::Stage m_stage;
    bool m_enabled;
    std::chrono::steady_clock::time_point m_start;
};

} // namespace modemdab

#endif // MODEMDAB_DABPROFILE_H_
