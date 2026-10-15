///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef MODEMDVB_DVBT2DEMAP_H
#define MODEMDVB_DVBT2DEMAP_H

#include <cstdint>

#include "dvbt2fec.h"

namespace dvbt2 {

//! The helpers of demapCells() and demapCellsAxes() (declared in dvbt2fec.h)
class CellDemapper
{
private:
    static float normalisation(int eta);
    //! With axes, each axis has its own scale (scalesQ for the imaginary part's), else scales is for both
    template <bool axes>
    static void demap(const float *re, const float *im, float *scales, float *scalesQ, int nbCells, int eta, float outlierThreshold,
        const int *bitMap, int8_t *llrs, float *errors);

    friend void demapCells(const float *re, const float *im, float *scales, int nbCells, int eta, float outlierThreshold,
        const int *bitMap, int8_t *llrs, float *errors);
    friend void demapCellsAxes(const float *re, const float *im, float *scalesI, float *scalesQ, int nbCells, int eta,
        float outlierThreshold, const int *bitMap, int8_t *llrs, float *errors);
};

} // namespace dvbt2

#endif // MODEMDVB_DVBT2DEMAP_H
