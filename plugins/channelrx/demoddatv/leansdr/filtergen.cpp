///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019, 2021 Edouard Griffiths, F4EXB <f4exb06@gmail.com>             //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>

#include "filtergen.h"

namespace leansdr
{
namespace filtergen
{

void dump_filter(const char *name, int ncoeffs, float *coeffs)
{
    fprintf(stderr, "%s = [", name);

    for (int i = 0; i < ncoeffs; ++i) {
        fprintf(stderr, "%s %f", (i ? "," : ""), coeffs[i]);
    }

    fprintf(stderr, " ];\n");
}

} // filtergen
} // leansdr
