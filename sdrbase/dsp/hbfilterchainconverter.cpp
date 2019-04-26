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

#include <QString>
#include "hbfilterchainconverter.h"

double HBFilterChainConverter::convertToIndexes(unsigned int log2, unsigned int chainHash, std::vector<unsigned int>& chainIndexes)
{
    chainIndexes.clear();

    if (log2 == 0) {
        return 0.0;
    }

    unsigned int s = 1;
    unsigned int u = chainHash;

    for (unsigned int i = 0; i < log2; i++) {
        s *= 3;
    }

    u %= s; // scale
    unsigned int ix = log2;
    double shift = 0.0;
    double shift_stage = 1.0 / (1<<(log2+1));

    // base3 conversion
    do
    {
        int r = u % 3;
        chainIndexes.push_back(r);
        shift += (r-1)*shift_stage;
        shift_stage *= 2;
        u /= 3;
        ix--;
    } while (u);

    // continue shift with leading zeroes. ix has the number of leading zeroes.
    for (unsigned int i = 0; i < ix; i++)
    {
        chainIndexes.push_back(0);
        shift -= shift_stage;
        shift_stage *= 2;
    }

    return shift;
}

double HBFilterChainConverter::convertToString(unsigned int log2, unsigned int chainHash, QString& chainString)
{
    if (log2 == 0)
    {
        chainString = "C";
        return 0.0;
    }

    unsigned int s = 1;
    unsigned int u = chainHash;
    chainString = "";

    for (unsigned int i = 0; i < log2; i++) {
        s *= 3;
    }

    u %= s; // scale
    unsigned int ix = log2;
    double shift = 0.0;
    double shift_stage = 1.0 / (1<<(log2+1));

    // base3 conversion
    do
    {
        int r = u % 3;

        if (r == 0) {
            chainString = "L" + chainString;
        } else if (r == 1) {
            chainString = "C" + chainString;
        } else if (r == 2) {
            chainString = "H" + chainString;
        }

        shift += (r-1)*shift_stage;
        shift_stage *= 2;
        u /= 3;
        ix--;
    } while (u);

    // continue shift with leading zeroes. ix has the number of leading zeroes.
    for (unsigned int i = 0; i < ix; i++)
    {
        chainString = "L" + chainString;
        shift -= shift_stage;
        shift_stage *= 2;
    }

    return shift;
}

double HBFilterChainConverter::getShiftFactor(unsigned int log2, unsigned int chainHash)
{
    if (log2 == 0)
    {
        return 0.0;
    }

    unsigned int s = 1;
    unsigned int u = chainHash;

    for (unsigned int i = 0; i < log2; i++) {
        s *= 3;
    }

    u %= s; // scale
    unsigned int ix = log2;
    double shift = 0.0;
    double shift_stage = 1.0 / (1<<(log2+1));

    // base3 conversion
    do
    {
        int r = u % 3;
        shift += (r-1)*shift_stage;
        shift_stage *= 2;
        u /= 3;
        ix--;
    } while (u);

    // continue shift with leading zeroes. ix has the number of leading zeroes.
    for (unsigned int i = 0; i < ix; i++)
    {
        shift -= shift_stage;
        shift_stage *= 2;
    }

    return shift;
}