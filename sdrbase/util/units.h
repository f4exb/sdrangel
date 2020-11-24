///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_UNITS_H
#define INCLUDE_UNITS_H

#include <cmath>

#include "export.h"

// Unit conversions
class SDRBASE_API Units
{
public:

    static inline float feetToMetres(float feet)
    {
        return feet * 0.3048f;
    }

    static inline int feetToIntegerMetres(float feet)
    {
        return (int)std::round(feetToMetres(feet));
    }

    static inline float nauticalMilesToMetres(float nauticalMiles)
    {
        return nauticalMiles * 1855.0f;
    }

    static inline int nauticalMilesToIntegerMetres(float nauticalMiles)
    {
        return (int)std::round(nauticalMilesToMetres(nauticalMiles));
    }

    static float knotsToKPH(float knots)
    {
        return knots * 1.852f;
    }

    static int knotsToIntegerKPH(float knots)
    {
        return (int)std::round(knotsToKPH(knots));
    }

    static float feetPerMinToMetresPerSecond(float fpm)
    {
        return fpm * 0.00508f;
    }

    static int feetPerMinToIntegerMetresPerSecond(float fpm)
    {
        return (int)std::round(feetPerMinToMetresPerSecond(fpm));
    }

    static float degreesToRadians(float degrees)
    {
        return degrees * ((float)M_PI) / 180.0f;
    }

    static float radiansToDegress(float radians)
    {
        return radians * 180.0f / ((float)M_PI);
    }

};

#endif // INCLUDE_UNITS_H
