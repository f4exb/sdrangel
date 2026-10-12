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

#ifndef MODEMDVB_CPUFEATURES_H
#define MODEMDVB_CPUFEATURES_H

// Internal to modemdvb: which of the instruction sets code is built for at run time can be used. Setting DVB_NO_AVX2
// in the environment disables AVX2, to compare it with the SSE code it replaces

namespace modemdvb {

bool cpuHasAVX2();      //!< And FMA, which every AVX2 CPU also has
bool cpuHasSSE41();
//! On a hybrid CPU (Intel's performance and efficiency cores), if the calling thread is on an efficiency core now.
//! Their 128-bit vector units split 256-bit instructions and are slow at moving data across their halves, so some
//! AVX2 code is slower there than the SSE code. False if the CPU isn't hybrid
bool onEfficiencyCore();

//! The detection behind those
class CPUFeatures
{
private:
    friend bool cpuHasAVX2();
    friend bool cpuHasSSE41();
    friend bool onEfficiencyCore();

#if defined(MODEMDVB_X86)
#if defined(_MSC_VER)
#if defined(__clang__)
    __attribute__((target("xsave"))) // As its definition
#endif
    static unsigned long long xcr0();
#endif
    static bool detectAVX2();
    static bool detectSSE41();
    static void cpuid(int leaf, int sub, unsigned int r[4]);
    static bool detectHybrid();
#endif
};

} // namespace modemdvb

#endif // MODEMDVB_CPUFEATURES_H
