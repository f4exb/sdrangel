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

#include "drmcellmap.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <mutex>
#include <stdexcept>

namespace modemdrm
{

namespace
{

using Positions = std::vector<CellPosition>;

void add(Positions& positions, int symbol, std::initializer_list<int> carriers)
{
    for (int carrier : carriers) {
        positions.push_back({symbol, carrier});
    }
}

Positions makeFacA()
{
    Positions p;
    add(p, 2, {26,46,66,86});
    add(p, 3, {10,30,50,70,90});
    add(p, 4, {14,22,34,62,74,94});
    add(p, 5, {26,38,58,66,78});
    add(p, 6, {22,30,42,62,70,82});
    add(p, 7, {26,34,46,66,74,86});
    add(p, 8, {10,30,38,50,58,70,78,90});
    add(p, 9, {14,22,34,42,62,74,82,94});
    add(p, 10, {26,38,46,66,86});
    add(p, 11, {10,30,50,70,90});
    add(p, 12, {14,34,74,94});
    add(p, 13, {38,58,78});
    return p;
}

Positions makeFacB()
{
    Positions p;
    add(p, 2, {13,25,43,55,67});
    add(p, 3, {15,27,45,57,69});
    add(p, 4, {17,29,47,59,71});
    add(p, 5, {19,31,49,61,73});
    add(p, 6, {9,21,33,51,63,75});
    add(p, 7, {11,23,35,53,65,77});
    add(p, 8, {13,25,37,55,67,79});
    add(p, 9, {15,27,39,57,69,81});
    add(p, 10, {17,29,41,59,71,83});
    add(p, 11, {19,31,43,61,73});
    add(p, 12, {21,33,45,63,75});
    add(p, 13, {23,35,47,65,77});
    return p;
}

Positions makeFacC()
{
    Positions p;
    add(p, 3, {9,21,45,57});
    add(p, 4, {23,35,47});
    add(p, 5, {13,25,37,49});
    add(p, 6, {15,27,39,51});
    add(p, 7, {5,17,29,41,53});
    add(p, 8, {7,19,31,43,55});
    add(p, 9, {9,21,45,57});
    add(p, 10, {23,35,47});
    add(p, 11, {13,25,37,49});
    add(p, 12, {15,27,39,51});
    add(p, 13, {5,17,29,41,53});
    add(p, 14, {7,19,31,43,55});
    add(p, 15, {9,21,45,57});
    add(p, 16, {23,35,47});
    add(p, 17, {13,25,37,49});
    add(p, 18, {15,27,39,51});
    return p;
}

Positions makeFacD()
{
    Positions p;
    add(p, 3, {9,18,27});
    add(p, 4, {10,19});
    add(p, 5, {11,20,29});
    add(p, 6, {12,30});
    add(p, 7, {13,22,31});
    add(p, 8, {5,14,23,32});
    add(p, 9, {6,15,24,33});
    add(p, 10, {16,25,34});
    add(p, 11, {8,17,26,35});
    add(p, 12, {9,18,27,36});
    add(p, 13, {10,19,37});
    add(p, 14, {11,20,29});
    add(p, 15, {12,30});
    add(p, 16, {13,22,31});
    add(p, 17, {5,14,23,32});
    add(p, 18, {6,15,24,33});
    add(p, 19, {16,25,34});
    add(p, 20, {8,17,26,35});
    add(p, 21, {9,18,27,36});
    add(p, 22, {10,19,37});
    return p;
}

Positions makeFacE()
{
    Positions p;
    const std::initializer_list<int> groups[] = {
        {-82,-66,-50,-34,-18,-2,14,30,46,62,78},
        {-78,-62,-46,-30,-14,2,18,34,50,66,82},
        {-90,-74,-58,-42,-26,-10,6,22,38,54,70,86},
        {-86,-70,-54,-38,-22,-6,10,26,42,58,74,90}
    };
    for (int symbol = 5; symbol <= 25; ++symbol) {
        add(p, symbol, groups[symbol % 4]);
    }

    add(p, 26, {-90,-74,-58});
    return p;
}

const std::array<Positions, 5> g_fac {{
    makeFacA(), makeFacB(), makeFacC(), makeFacD(), makeFacE()
}};

const std::array<std::vector<ReferenceCell>, 5> g_timeReferences {{
    {{0,17,973},{0,18,205},{0,19,717},{0,21,264},{0,28,357},{0,29,357},
     {0,32,952},{0,33,440},{0,39,856},{0,40,88},{0,41,88},{0,53,68},
     {0,54,836},{0,55,836},{0,56,836},{0,60,1008},{0,61,1008},{0,63,752},
     {0,71,215},{0,72,215},{0,73,727}},
    {{0,14,304},{0,16,331},{0,18,108},{0,20,620},{0,24,192},{0,26,704},
     {0,32,44},{0,36,432},{0,42,588},{0,44,844},{0,48,651},{0,49,651},
     {0,50,651},{0,54,460},{0,56,460},{0,62,944},{0,64,555},{0,66,940},{0,68,428}},
    {{0,8,722},{0,10,466},{0,11,214},{0,12,214},{0,14,479},{0,16,516},
     {0,18,260},{0,22,577},{0,24,662},{0,28,3},{0,30,771},{0,32,392},
     {0,33,392},{0,36,37},{0,38,37},{0,42,474},{0,44,242},{0,45,242},{0,46,754}},
    {{0,5,636},{0,6,124},{0,7,788},{0,8,788},{0,9,200},{0,11,688},
     {0,12,152},{0,14,920},{0,15,920},{0,17,644},{0,18,388},{0,20,652},
     {0,21,1014},{0,23,176},{0,24,176},{0,26,752},{0,27,496},{0,28,332},
     {0,29,432},{0,30,964},{0,32,452}},
    {{0,-80,219},{0,-79,475},{0,-77,987},{0,-53,652},{0,-52,652},{0,-51,140},
     {0,-32,819},{0,-31,819},{0,12,907},{0,13,907},{0,14,651},{0,21,903},
     {0,22,391},{0,23,903},{0,40,203},{0,41,203},{0,42,203},{0,67,797},
     {0,68,29},{0,79,508},{0,80,508}}
}};

int modeIndex(Mode mode)
{
    const int index = static_cast<int>(mode);
    if (index < 0 || index >= static_cast<int>(g_fac.size())) {
        throw std::invalid_argument("Invalid DRM robustness mode");
    }

    return index;
}

bool contains(std::initializer_list<int> values, int value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

int positiveModulo(int value, int modulus)
{
    const int result = value % modulus;
    return result < 0 ? result + modulus : result;
}

int frequencyPhase(Mode mode, int symbol, int carrier)
{
    switch (mode)
    {
    case Mode::A:
        if (carrier == 18) {
            return 205;
        }

        if (carrier == 54) {
            return 836;
        }

        if (carrier == 72) {
            return 215;
        }

        break;
    case Mode::B:
        if (carrier == 16) {
            return 331;
        }

        if (carrier == 48) {
            return 651;
        }

        if (carrier == 64) {
            return 555;
        }

        break;
    case Mode::C:
        if (carrier == 11) {
            return 214;
        }

        if (carrier == 33) {
            return 392;
        }

        if (carrier == 44) {
            return 242;
        }

        break;
    case Mode::D:
        if (carrier == 7) {
            return positiveModulo(788 + (symbol % 2) * 512, 1024);
        }

        if (carrier == 21) {
            return positiveModulo(1014 + (symbol % 2) * 512, 1024);
        }

        if (carrier == 28) {
            return 332;
        }

        break;
    case Mode::E:
        break;
    }

    throw std::invalid_argument("Carrier is not a DRM frequency reference");
}

constexpr std::array<int, 54> g_afsCarriers {{
    -106,-102,-98,-94,-90,-86,-82,-78,-74,-70,-66,-62,-58,-54,-50,-46,-42,-38,
    -34,-30,-26,-22,-18,-14,-10,-6,-2,2,6,10,14,18,22,26,30,34,38,42,46,50,54,
    58,62,66,70,74,78,82,86,90,94,98,102,106
}};

constexpr std::array<int, 54> g_afsPhaseSymbol4 {{
    134,866,588,325,77,868,649,445,256,82,946,801,671,556,455,369,298,242,
    200,173,161,164,181,213,260,322,398,489,595,716,851,1001,142,322,516,725,
    949,164,417,685,968,242,554,881,199,556,927,289,690,82,512,957,393,868
}};

constexpr std::array<int, 54> g_afsPhaseSymbol39 {{
    115,135,194,293,431,608,825,57,353,688,38,452,905,373,905,452,39,689,
    354,59,827,610,433,295,197,138,118,138,197,295,433,610,827,59,354,689,
    39,452,905,373,905,452,38,688,353,57,825,608,431,293,194,135,115,134
}};

bool afsPhase(int symbol, int frameInSuperframe, int carrier, int& phase)
{
    const bool symbol4 = frameInSuperframe == 0 && symbol == 4;
    const bool symbol39 = frameInSuperframe == 3 && symbol == 39;
    if (!symbol4 && !symbol39) {
        return false;
    }

    const auto it = std::find(g_afsCarriers.begin(), g_afsCarriers.end(), carrier);
    if (it == g_afsCarriers.end()) {
        return false;
    }

    const std::size_t index = static_cast<std::size_t>(it - g_afsCarriers.begin());
    phase = symbol4 ? g_afsPhaseSymbol4[index] : g_afsPhaseSymbol39[index];
    return true;
}

} // namespace

std::vector<int> CellMap::unusedCarriers(Mode mode)
{
    if (mode == Mode::A) {
        return {-1, 0, 1};
    }

    if (mode == Mode::E) {
        return {};
    }

    modeIndex(mode);
    return {0};
}

std::vector<int> CellMap::frequencyReferenceCarriers(Mode mode)
{
    switch (mode)
    {
    case Mode::A:
        return {18, 54, 72};
    case Mode::B:
        return {16, 48, 64};
    case Mode::C:
        return {11, 33, 44};
    case Mode::D:
        return {7, 21, 28};
    case Mode::E:
        return {};
    }

    throw std::invalid_argument("Invalid DRM robustness mode");
}

std::vector<ReferenceCell> CellMap::timeReferences(Mode mode)
{
    return g_timeReferences[static_cast<std::size_t>(modeIndex(mode))];
}

namespace
{

std::vector<int> computeGainReferenceCarriers(Mode mode, int occupancy, int symbol)
{
    const SpectrumParameters* spectrum = Parameters::forSpectrum(mode, occupancy);
    if (!spectrum || symbol < 0 || symbol >= Parameters::forMode(mode).symbolsPerFrame) {
        return {};
    }

    int x = 0;
    int y = 0;
    int k0 = 0;
    switch (mode)
    {
    case Mode::A:
        x = 4;
        y = 5;
        k0 = 2;
        break;
    case Mode::B:
        x = 2;
        y = 3;
        k0 = 1;
        break;
    case Mode::C:
        x = 2;
        y = 2;
        k0 = 1;
        break;
    case Mode::D:
        x = 1;
        y = 3;
        k0 = 1;
        break;
    case Mode::E:
        x = 4;
        y = 4;
        k0 = 2;
        break;
    }

    const int base = k0 + x * (symbol % y);
    const int period = x * y;
    // Table 58 places a mode-D gain reference on k = 0 when s mod 3 = 2, but
    // table 50 leaves the DC carrier unused, so nothing is transmitted there.
    const std::vector<int> unused = CellMap::unusedCarriers(mode);
    std::vector<int> carriers;
    for (int carrier = spectrum->minimumCarrier; carrier <= spectrum->maximumCarrier; ++carrier)
    {
        int remainder = (carrier - base) % period;
        if (remainder < 0) {
            remainder += period;
        }

        if (remainder == 0
            && std::find(unused.begin(), unused.end(), carrier) == unused.end()) {
            carriers.push_back(carrier);
        }
    }

    return carriers;
}

} // namespace

int CellMap::gainReferencePhase1024(Mode mode, int symbol, int carrier)
{
    const int symbolsPerFrame = Parameters::forMode(mode).symbolsPerFrame;
    if (symbol < 0 || symbol >= symbolsPerFrame) {
        throw std::invalid_argument("Invalid DRM symbol index");
    }

    int x = 0;
    int y = 0;
    int k0 = 0;
    switch (mode)
    {
    case Mode::A:
        x = 4;
        y = 5;
        k0 = 2;
        break;
    case Mode::B:
        x = 2;
        y = 3;
        k0 = 1;
        break;
    case Mode::C:
        x = 2;
        y = 2;
        k0 = 1;
        break;
    case Mode::D:
        x = 1;
        y = 3;
        k0 = 1;
        break;
    case Mode::E:
        x = 4;
        y = 4;
        k0 = 2;
        break;
    }

    const int n = symbol % y;
    const int m = symbol / y;
    const int numerator = carrier - k0 - n * x;
    if (numerator % (x * y) != 0) {
        throw std::invalid_argument("Carrier is not on the DRM gain-reference lattice");
    }

    const int p = numerator / (x * y);

    static constexpr int wA[5][3] = {
        {228,341,455},{455,569,683},{683,796,910},{910,0,114},{114,228,341}
    };
    static constexpr int zA[5][3] = {
        {0,81,248},{18,106,106},{122,116,31},{129,129,39},{33,32,111}
    };
    static constexpr int wB[3][5] = {
        {512,0,512,0,512},{0,512,0,512,0},{512,0,512,0,512}
    };
    static constexpr int zB[3][5] = {
        {0,57,164,64,12},{168,255,161,106,118},{25,232,132,233,38}
    };
    static constexpr int wC[2][10] = {
        {465,372,279,186,93,0,931,838,745,652},
        {931,838,745,652,559,465,372,279,186,93}
    };
    static constexpr int zC[2][10] = {
        {0,76,29,76,9,190,161,248,33,108},
        {179,178,83,253,127,105,101,198,250,145}
    };
    static constexpr int wD[3][8] = {
        {366,439,512,585,658,731,805,878},
        {731,805,878,951,0,73,146,219},
        {73,146,219,293,366,439,512,585}
    };
    static constexpr int zD[3][8] = {
        {0,240,17,60,220,38,151,101},
        {110,7,78,82,175,150,106,25},
        {165,7,252,124,253,177,197,142}
    };
    static constexpr int rE[4][10] = {
        {39,118,197,276,354,433,39,118,197,276},
        {37,183,402,37,183,402,37,183,402,37},
        {110,329,475,110,329,475,110,329,475,110},
        {79,158,236,315,394,473,79,158,236,315}
    };
    static constexpr int zE[4][10] = {
        {473,394,315,236,158,79,0,0,0,0},
        {183,914,402,37,475,841,768,768,987,183},
        {549,622,475,110,37,622,256,768,329,549},
        {79,158,236,315,394,473,158,315,473,630}
    };
    static constexpr int qE[4][10] = {
        {329,489,894,419,607,519,1020,942,817,939},
        {824,1023,74,319,225,207,348,422,395,92},
        {959,379,7,738,500,920,440,727,263,733},
        {907,946,924,91,189,133,910,804,1022,433}
    };

    int phase = 0;
    switch (mode)
    {
    case Mode::A:
        phase = 4 * zA[n][m] + p * wA[n][m] + p * p * (1 + symbol) * 36;
        break;
    case Mode::B:
        phase = 4 * zB[n][m] + p * wB[n][m] + p * p * (1 + symbol) * 12;
        break;
    case Mode::C:
        phase = 4 * zC[n][m] + p * wC[n][m] + p * p * (1 + symbol) * 12;
        break;
    case Mode::D:
        phase = 4 * zD[n][m] + p * wD[n][m] + p * p * (1 + symbol) * 14;
        break;
    case Mode::E:
        phase = p * p * rE[n][m] + p * zE[n][m] + qE[n][m];
        break;
    }

    return positiveModulo(phase, 1024);
}

bool CellMap::isBoostedGainReference(Mode mode, int occupancy, int carrier)
{
    switch (mode)
    {
    case Mode::A:
        switch (occupancy) {
        case 0:
            return contains({2,6,98,102}, carrier);
        case 1:
            return contains({2,6,110,114}, carrier);
        case 2:
            return contains({-102,-98,98,102}, carrier);
        case 3:
            return contains({-114,-110,110,114}, carrier);
        case 4:
            return contains({-98,-94,310,314}, carrier);
        case 5:
            return contains({-110,-106,346,350}, carrier);
        }

        break;
    case Mode::B:
        switch (occupancy) {
        case 0:
            return contains({1,3,89,91}, carrier);
        case 1:
            return contains({1,3,101,103}, carrier);
        case 2:
            return contains({-91,-89,89,91}, carrier);
        case 3:
            return contains({-103,-101,101,103}, carrier);
        case 4:
            return contains({-87,-85,277,279}, carrier);
        case 5:
            return contains({-99,-97,309,311}, carrier);
        }

        break;
    case Mode::C:
        if (occupancy == 3) {
            return contains({-69,-67,67,69}, carrier);
        }

        if (occupancy == 5) {
            return contains({-67,-65,211,213}, carrier);
        }

        break;
    case Mode::D:
        if (occupancy == 3) {
            return contains({-44,-43,43,44}, carrier);
        }

        if (occupancy == 5) {
            return contains({-43,-42,134,135}, carrier);
        }

        break;
    case Mode::E:
        if (occupancy == 0) {
            return contains({-106,-102,102,106}, carrier);
        }

        break;
    }

    return false;
}

namespace
{

std::vector<PilotCell> computeReferenceCells(
    Mode mode,
    int occupancy,
    int symbol,
    int frameInSuperframe
)
{
    const SpectrumParameters* spectrum = Parameters::forSpectrum(mode, occupancy);
    const ModeParameters& modeParameters = Parameters::forMode(mode);
    if (!spectrum || symbol < 0 || symbol >= modeParameters.symbolsPerFrame
        || frameInSuperframe < 0 || frameInSuperframe >= modeParameters.framesPerSuperframe) {
        return {};
    }

    const float rootTwo = std::sqrt(2.0f);
    std::map<int, PilotCell> cells;
    for (int carrier : CellMap::gainReferenceCarriers(mode, occupancy, symbol))
    {
        cells[carrier] = {
            carrier,
            CellMap::gainReferencePhase1024(mode, symbol, carrier),
            CellMap::isBoostedGainReference(mode, occupancy, carrier) ? 2.0f : rootTwo,
            ReferenceKind::Gain
        };
    }

    for (int carrier : CellMap::frequencyReferenceCarriers(mode))
    {
        if (carrier >= spectrum->minimumCarrier && carrier <= spectrum->maximumCarrier) {
            cells[carrier] = {carrier, frequencyPhase(mode, symbol, carrier), rootTwo, ReferenceKind::Frequency};
        }
    }

    if (symbol == 0)
    {
        for (const ReferenceCell& reference : CellMap::timeReferences(mode))
        {
            if (reference.carrier >= spectrum->minimumCarrier && reference.carrier <= spectrum->maximumCarrier)
            {
                cells[reference.carrier] = {
                    reference.carrier, reference.phase1024, rootTwo, ReferenceKind::Time
                };
            }
        }
    }

    if (mode == Mode::E)
    {
        for (int carrier : g_afsCarriers)
        {
            int phase = 0;
            if (afsPhase(symbol, frameInSuperframe, carrier, phase))
            {
                const auto existing = cells.find(carrier);
                const float amplitude = existing == cells.end() ? 1.0f : existing->second.amplitude;
                cells[carrier] = {carrier, phase, amplitude, ReferenceKind::AFS};
            }
        }
    }

    std::vector<PilotCell> result;
    result.reserve(cells.size());
    for (const auto& entry : cells) {
        result.push_back(entry.second);
    }

    return result;
}

} // namespace

const std::vector<CellPosition>& CellMap::facCells(Mode mode)
{
    return g_fac[static_cast<std::size_t>(modeIndex(mode))];
}

int CellMap::sdcSymbolCount(Mode mode)
{
    switch (mode)
    {
    case Mode::A:
    case Mode::B:
        return 2;
    case Mode::C:
    case Mode::D:
        return 3;
    case Mode::E:
        return 5;
    }

    throw std::invalid_argument("Invalid DRM robustness mode");
}

namespace
{

std::vector<CellPosition> computeSDCCells(Mode mode, int occupancy)
{
    const SpectrumParameters* spectrum = Parameters::forSpectrum(mode, occupancy);
    if (!spectrum) {
        return {};
    }

    const std::vector<int> unused = CellMap::unusedCarriers(mode);
    std::vector<CellPosition> result;
    for (int symbol = 0; symbol < CellMap::sdcSymbolCount(mode); ++symbol)
    {
        const std::vector<PilotCell> references = CellMap::referenceCells(
            mode, occupancy, symbol, 0
        );
        for (int carrier = spectrum->minimumCarrier; carrier <= spectrum->maximumCarrier; ++carrier)
        {
            const bool isUnused = std::find(unused.begin(), unused.end(), carrier) != unused.end();
            const bool isReference = std::find_if(
                references.begin(), references.end(),
                [carrier](const PilotCell& cell) { return cell.carrier == carrier; }
            ) != references.end();
            if (!isUnused && !isReference) {
                result.push_back({symbol, carrier});
            }
        }
    }

    return result;
}

} // namespace

namespace
{

std::vector<CellPosition> computeMSCCells(
    Mode mode,
    int occupancy,
    int frameInSuperframe
)
{
    const SpectrumParameters* spectrum = Parameters::forSpectrum(mode, occupancy);
    const ModeParameters& parameters = Parameters::forMode(mode);
    if (!spectrum || frameInSuperframe < 0
        || frameInSuperframe >= parameters.framesPerSuperframe) {
        return {};
    }

    const std::vector<int> unused = CellMap::unusedCarriers(mode);
    const std::vector<CellPosition>& fac = CellMap::facCells(mode);
    const std::vector<CellPosition> sdc = frameInSuperframe == 0
        ? CellMap::sdcCells(mode, occupancy) : std::vector<CellPosition>();
    std::vector<CellPosition> result;
    for (int symbol = 0; symbol < parameters.symbolsPerFrame; ++symbol)
    {
        const std::vector<PilotCell> references = CellMap::referenceCells(
            mode, occupancy, symbol, frameInSuperframe
        );
        for (int carrier = spectrum->minimumCarrier; carrier <= spectrum->maximumCarrier; ++carrier)
        {
            const CellPosition position {symbol, carrier};
            const bool isUnused = std::find(unused.begin(), unused.end(), carrier) != unused.end();
            const bool isReference = std::find_if(
                references.begin(), references.end(),
                [carrier](const PilotCell& cell) { return cell.carrier == carrier; }
            ) != references.end();
            const bool isFAC = std::find(fac.begin(), fac.end(), position) != fac.end();
            const bool isSDC = std::find(sdc.begin(), sdc.end(), position) != sdc.end();
            if (!isUnused && !isReference && !isFAC && !isSDC) {
                result.push_back(position);
            }
        }
    }

    return result;
}

// The tables are pure functions of small indexes, needed for every symbol of
// every superframe; compute each once. Map nodes never move, so references
// stay valid. Out-of-range indexes are computed directly (they may throw) and
// yield an empty table.
template <typename T>
class TableCache
{
public:
    template <typename Compute>
    const std::vector<T>& get(int key, Compute compute)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto found = m_tables.find(key);
        if (found == m_tables.end()) {
            found = m_tables.emplace(key, compute()).first;
        }

        return found->second;
    }

private:
    std::mutex m_mutex;
    std::map<int, std::vector<T>> m_tables;
};

bool cacheable(Mode mode, int occupancy, int symbol, int frameInSuperframe)
{
    const int index = static_cast<int>(mode);
    return index >= 0 && index <= static_cast<int>(Mode::E)
        && occupancy >= 0 && occupancy < 8
        && symbol >= 0 && symbol < 64
        && frameInSuperframe >= 0 && frameInSuperframe < 8;
}

int cacheKey(Mode mode, int occupancy, int symbol, int frameInSuperframe)
{
    return ((static_cast<int>(mode) * 8 + occupancy) * 64 + symbol) * 8 + frameInSuperframe;
}

template <typename T, typename Compute>
const std::vector<T>& cached(
    TableCache<T>& cache, Mode mode, int occupancy, int symbol, int frameInSuperframe, Compute compute)
{
    if (!cacheable(mode, occupancy, symbol, frameInSuperframe))
    {
        static const std::vector<T> empty;
        (void) compute();
        return empty;
    }

    return cache.get(cacheKey(mode, occupancy, symbol, frameInSuperframe), compute);
}

} // namespace

const std::vector<int>& CellMap::gainReferenceCarriers(Mode mode, int occupancy, int symbol)
{
    static TableCache<int> cache;
    return cached(cache, mode, occupancy, symbol, 0,
        [=] { return computeGainReferenceCarriers(mode, occupancy, symbol); });
}

const std::vector<PilotCell>& CellMap::referenceCells(
    Mode mode,
    int occupancy,
    int symbol,
    int frameInSuperframe
)
{
    static TableCache<PilotCell> cache;
    return cached(cache, mode, occupancy, symbol, frameInSuperframe,
        [=] { return computeReferenceCells(mode, occupancy, symbol, frameInSuperframe); });
}

const std::vector<CellPosition>& CellMap::sdcCells(Mode mode, int occupancy)
{
    static TableCache<CellPosition> cache;
    return cached(cache, mode, occupancy, 0, 0,
        [=] { return computeSDCCells(mode, occupancy); });
}

const std::vector<CellPosition>& CellMap::mscCells(
    Mode mode,
    int occupancy,
    int frameInSuperframe
)
{
    static TableCache<CellPosition> cache;
    return cached(cache, mode, occupancy, 0, frameInSuperframe,
        [=] { return computeMSCCells(mode, occupancy, frameInSuperframe); });
}

} // namespace modemdrm
