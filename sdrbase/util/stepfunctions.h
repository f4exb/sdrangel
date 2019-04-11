///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRBASE_UTIL_STEPFUNCTIONS_H_
#define SDRBASE_UTIL_STEPFUNCTIONS_H_

class StepFunctions
{
public:
    static float smootherstep(float x)
    {
        if (x == 1.0f) {
            return 1.0f;
        } else if (x == 0.0f) {
            return 0.0f;
        }

        double x3 = x * x * x;
        double x4 = x * x3;
        double x5 = x * x4;

        return (float) (6.0*x5 - 15.0*x4 + 10.0*x3);
    }
};

#endif /* SDRBASE_UTIL_STEPFUNCTIONS_H_ */
