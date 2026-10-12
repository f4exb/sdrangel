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

#include <cstdlib>

#include "cpufeatures.h"

#if defined(MODEMDVB_X86) && defined(_MSC_VER)
#include <intrin.h>
#elif defined(MODEMDVB_X86)
#include <cpuid.h>
#endif

namespace modemdvb {

#if defined(MODEMDVB_X86)

#if defined(_MSC_VER)
// clang-cl's _xgetbv needs the XSAVE instructions enabled, but only for this, so not for the whole file
#if defined(__clang__)
__attribute__((target("xsave")))
#endif
unsigned long long CPUFeatures::xcr0()
{
    return _xgetbv(0);
}
#endif

bool CPUFeatures::detectAVX2()
{
    if (getenv("DVB_NO_AVX2")) {
        return false;
    }

#if defined(_MSC_VER)
    int r[4];
    __cpuid(r, 0);

    if (r[0] < 7) {
        return false;
    }

    __cpuid(r, 1);
    const bool fma = (r[2] & (1 << 12)) != 0;
    const bool osxsave = (r[2] & (1 << 27)) != 0;
    const bool avx = (r[2] & (1 << 28)) != 0;

    if (!fma || !osxsave || !avx || ((xcr0() & 6) != 6)) { // OS saves the YMM registers
        return false;
    }

    __cpuidex(r, 7, 0);
    return (r[1] & (1 << 5)) != 0;
#else
    return __builtin_cpu_supports("avx2") && __builtin_cpu_supports("fma");
#endif
}

bool CPUFeatures::detectSSE41()
{
#if defined(_MSC_VER)
    int r[4];
    __cpuid(r, 1);
    return (r[2] & (1 << 19)) != 0;
#else
    return __builtin_cpu_supports("sse4.1");
#endif
}

bool cpuHasAVX2()
{
    static const bool avx2 = CPUFeatures::detectAVX2();
    return avx2;
}

bool cpuHasSSE41()
{
    static const bool sse41 = CPUFeatures::detectSSE41();
    return sse41;
}

void CPUFeatures::cpuid(int leaf, int sub, unsigned int r[4])
{
#if defined(_MSC_VER)
    int v[4];
    __cpuidex(v, leaf, sub);

    for (int i = 0; i < 4; i++) {
        r[i] = (unsigned int) v[i];
    }
#else
    __cpuid_count(leaf, sub, r[0], r[1], r[2], r[3]);
#endif
}

// CPUID.07H:EDX bit 15, and leaf 1AH available
bool CPUFeatures::detectHybrid()
{
    unsigned int r[4];
    cpuid(0, 0, r);

    if (r[0] < 0x1a) {
        return false;
    }

    cpuid(7, 0, r);
    return (r[3] & (1u << 15)) != 0;
}

bool onEfficiencyCore()
{
    static const bool hybrid = CPUFeatures::detectHybrid();

    if (!hybrid) {
        return false;
    }

    // CPUID can be slow (in a VM, the hypervisor handles it), and this is asked for each block, so the thread's last
    // answer is kept for a few calls. Should the OS move the thread meanwhile, it only runs slower code till then
    static const unsigned int refresh = 32;
    thread_local unsigned int calls = 0;
    thread_local bool efficiency = false;

    if (calls++ % refresh == 0)
    {
        // CPUID.1AH:EAX bits 31..24: the type of the core it runs on, 0x20 for an efficiency (Atom) core
        unsigned int r[4];
        CPUFeatures::cpuid(0x1a, 0, r);
        efficiency = (r[0] >> 24) == 0x20;
    }

    return efficiency;
}

#else

bool cpuHasAVX2()
{
    return false;
}

bool cpuHasSSE41()
{
    return false;
}

bool onEfficiencyCore()
{
    return false;
}

#endif

} // namespace modemdvb
