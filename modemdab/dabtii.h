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

#ifndef MODEMDAB_DABTII_H_
#define MODEMDAB_DABTII_H_

#include <vector>

#include "export.h"

namespace modemdab
{

struct MODEMDAB_API DABTIITransmitter
{
    int mainId = 0;         // Pattern, 0..69
    int subId = 0;          // Comb, 0..23
    float levelDB = 0.0f;   // Relative to the strongest transmitter
};

// Transmitter Identification Information in the null symbol (EN 300 401 clause 14.8)
class MODEMDAB_API DABTIIDecoder
{
public:
    DABTIIDecoder();

    void reset();
    // The power spectrum (FFT bin order) of a null symbol that carries TII: the frames whose
    // first CIF count is 0 modulo 8.
    void push(const std::vector<float>& nullSpectrum);
    // Transmitters found in the last averaging period, strongest first
    const std::vector<DABTIITransmitter>& transmitters() const { return m_transmitters; }
    // Enough has been analysed (two averaging periods, about 3 s) that no transmitters
    // means the ensemble sends no TII
    bool analysed() const { return m_detections >= 2; }

private:
    static constexpr int Average = 8;
    // Carriers this close to DC are not used: a receiver's DC offset or LO leakage would look
    // like TII carriers
    static constexpr int DCExclusion = 2;

    int m_detections;

    std::vector<double> m_sum;
    int m_count;
    std::vector<DABTIITransmitter> m_transmitters;

    void detect();
};

} // namespace modemdab

#endif // MODEMDAB_DABTII_H_
