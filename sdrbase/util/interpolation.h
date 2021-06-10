///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_INTERPOLATION_H
#define INCLUDE_INTERPOLATION_H

#include "export.h"
#include <iterator>

class SDRBASE_API Interpolation {
public:

    // Linear interpolation/extrapolation
    // Assumes x is sorted in increasing order
    template<class Iter, class T>
    static T linear(Iter xBegin, Iter xEnd, Iter yBegin, T x)
    {
        // Find first point in x that target is bigger than
        int i = 0;
        while (xBegin != xEnd)
        {
            if (x < *xBegin) {
                break;
            }
            ++xBegin;
            i++;
        }
        T x0, x1, y0, y1;
        if (i == 0) {
            // Extrapolate left
            x0 = *xBegin;
            ++xBegin;
            x1 = *xBegin;
            y0 = *yBegin;
            ++yBegin;
            y1 = *yBegin;

            return extrapolate(x0, y0, x1, y1, x);
        } else if (xBegin > xEnd) {
            // Extrapolate right
            Iter xCur = xBegin;
            std::advance(xCur, i - 2);
            x0 = *xCur;
            ++xCur;
            x1 = *xCur;

            Iter yCur = yBegin;
            std::advance(yCur, i - 2);
            y0 = *yCur;
            ++yCur;
            y1 = *yCur;

            return extrapolate(x0, y0, x1, y1, x);
        } else {
            // Interpolate
            x1 = *xBegin;
            --xBegin;
            x0 = *xBegin;

            Iter yCur = yBegin;
            std::advance(yCur, i - 1);
            y0 = *yCur;
            ++yCur;
            y1 = *yCur;

            return interpolate(x0, y0, x1, y1, x);
        }
    }

    // Linear extrapolation
    template<class T>
    static T extrapolate(T x0, T y0, T x1, T y1, T x)
    {
        return y0 + ((x-x0)/(x1-x0)) * (y1-y0);
    }

    // Linear interpolation
    template<class T>
    static T interpolate(T x0, T y0, T x1, T y1, T x)
    {
        return (y0*(x1-x) + y1*(x-x0)) / (x1-x0);
    }

};

#endif // INCLUDE_INTERPOLATION_H
