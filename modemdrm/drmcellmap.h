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

#ifndef MODEMDRM_DRMCELLMAP_H_
#define MODEMDRM_DRMCELLMAP_H_

#include <vector>

#include "export.h"
#include "drmparameters.h"

namespace modemdrm
{

struct MODEMDRM_API CellPosition
{
    int symbol;
    int carrier;

    bool operator==(const CellPosition& other) const
    {
        return symbol == other.symbol && carrier == other.carrier;
    }
};

struct MODEMDRM_API ReferenceCell
{
    int symbol;
    int carrier;
    int phase1024;
};

enum class ReferenceKind
{
    Gain,
    Frequency,
    Time,
    AFS
};

struct MODEMDRM_API PilotCell
{
    int carrier;
    int phase1024;
    float amplitude;
    ReferenceKind kind;
};

class MODEMDRM_API CellMap
{
public:
    static std::vector<int> unusedCarriers(Mode mode);
    static std::vector<int> frequencyReferenceCarriers(Mode mode);
    static std::vector<ReferenceCell> timeReferences(Mode mode);
    // The cell tables below are computed once and cached; the references stay valid for the process.
    static const std::vector<int>& gainReferenceCarriers(Mode mode, int occupancy, int symbol);
    static int gainReferencePhase1024(Mode mode, int symbol, int carrier);
    static bool isBoostedGainReference(Mode mode, int occupancy, int carrier);
    static const std::vector<PilotCell>& referenceCells(Mode mode, int occupancy, int symbol, int frameInSuperframe);
    static const std::vector<CellPosition>& facCells(Mode mode);
    static int sdcSymbolCount(Mode mode);
    static const std::vector<CellPosition>& sdcCells(Mode mode, int occupancy);
    static const std::vector<CellPosition>& mscCells(Mode mode, int occupancy, int frameInSuperframe);
};

} // namespace modemdrm

#endif // MODEMDRM_DRMCELLMAP_H_
