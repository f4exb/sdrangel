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

#include <cstring>
#include <mutex>

#include "dabfft.h"

#ifdef MODEMDAB_HAS_FFTW
#include <fftw3.h>
#ifdef MODEMDAB_FFTW_THREAD_SAFE
// FFTW 3.3.5 on, though not every fftw3.h declares it
extern "C" void fftwf_make_planner_thread_safe(void);
#endif
#else
#include "sdrbase/dsp/kissfft.h"
#endif

namespace modemdab
{

#ifdef MODEMDAB_HAS_FFTW

namespace
{

// FFTW's planner is not thread safe
std::mutex& plannerMutex()
{
    static std::mutex mutex;
    return mutex;
}

} // anonymous namespace

struct DABFFT::Implementation
{
    fftwf_complex *in = nullptr;
    fftwf_complex *out = nullptr;
    fftwf_plan plan = nullptr;
};

DABFFT::DABFFT(int size, bool inverse) :
    m_size(size),
    m_implementation(new Implementation())
{
#ifdef MODEMDAB_FFTW_THREAD_SAFE
    // Other code in the process (SDRangel's FFT engine) plans with its own lock
    static const bool threadSafe = [] { fftwf_make_planner_thread_safe(); return true; }();
    (void) threadSafe;
#endif
    std::lock_guard<std::mutex> lock(plannerMutex());
    m_implementation->in = fftwf_alloc_complex(size);
    m_implementation->out = fftwf_alloc_complex(size);
    // Plans are made once per receiver: measuring gives faster transforms than estimating
    m_implementation->plan = fftwf_plan_dft_1d(size, m_implementation->in, m_implementation->out,
        inverse ? FFTW_BACKWARD : FFTW_FORWARD, FFTW_MEASURE);
}

DABFFT::~DABFFT()
{
    std::lock_guard<std::mutex> lock(plannerMutex());
    fftwf_destroy_plan(m_implementation->plan);
    fftwf_free(m_implementation->in);
    fftwf_free(m_implementation->out);
}

void DABFFT::transform(const std::complex<float> *in, std::complex<float> *out)
{
    // Through the plan's aligned buffers, so any input and output can be used
    std::memcpy(m_implementation->in, in, sizeof(fftwf_complex) * m_size);
    fftwf_execute(m_implementation->plan);
    std::memcpy(static_cast<void*>(out), m_implementation->out, sizeof(fftwf_complex) * m_size);
}

const char *DABFFT::backend()
{
    return "FFTW";
}

#else

struct DABFFT::Implementation
{
    kissfft<float, std::complex<float>> fft;
};

DABFFT::DABFFT(int size, bool inverse) :
    m_size(size),
    m_implementation(new Implementation())
{
    m_implementation->fft.configure(size, inverse);
}

DABFFT::~DABFFT()
{
}

void DABFFT::transform(const std::complex<float> *in, std::complex<float> *out)
{
    m_implementation->fft.transform(in, out);
}

const char *DABFFT::backend()
{
    return "kissfft";
}

#endif

} // namespace modemdab
