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

#ifndef MODEMDAB_DABFFT_H_
#define MODEMDAB_DABFFT_H_

#include <complex>
#include <memory>

#include "export.h"

namespace modemdab
{

// A complex FFT of fixed size: FFTW when it is built in, kissfft otherwise. Unnormalized in
// both directions.
class MODEMDAB_API DABFFT
{
public:
    DABFFT(int size, bool inverse);
    ~DABFFT();
    DABFFT(const DABFFT&) = delete;
    DABFFT& operator=(const DABFFT&) = delete;

    void transform(const std::complex<float> *in, std::complex<float> *out);
    int size() const { return m_size; }
    static const char *backend();

private:
    struct Implementation;

    int m_size;
    std::unique_ptr<Implementation> m_implementation;
};

} // namespace modemdab

#endif // MODEMDAB_DABFFT_H_
