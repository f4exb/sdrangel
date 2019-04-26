///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef SDRBASE_DSP_HBFILTERCHAINCONVERTER_H
#define SDRBASE_DSP_HBFILTERCHAINCONVERTER_H

#include <vector>
#include "export.h"

class QString;

class SDRBASE_API HBFilterChainConverter
{
public:
    // Converts the chain hash as a base3 number each digit representing a filter stage from lower (LSD) to upper level (MSD)
    // The corresponding log2 of decimation or interpolation factor is also the number of filter stages
    // A vector of indexes as base3 digits is filled in (0: low band, 1: center band, : high band)
    // The shift factor of center frequency is returned. The actual shift is obtained by multiplying this factor by the sample rate.
    static double convertToIndexes(unsigned int log2, unsigned int chainHash, std::vector<unsigned int>& chainIndexes);
    // Same but used only for display giving a string representation of the filter chain
    static double convertToString(unsigned int log2, unsigned int chainHash, QString& chainString);
    // Just calculate the frequency shift factor relative to sample rate
    static double getShiftFactor(unsigned int log2, unsigned int chainHash);
};

#endif // SDRBASE_DSP_HBFILTERCHAINCONVERTER_H