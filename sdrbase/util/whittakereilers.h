///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2025 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_WHITTAKEREILERS_H
#define INCLUDE_WHITTAKEREILERS_H

#include <QtCore>

#include "export.h"

// Whittaker-Eilers smoother
// https://pubs.acs.org/doi/10.1021/ac034173t
class SDRBASE_API WhittakerEilers
{

public:
    WhittakerEilers();
    ~WhittakerEilers();
    void filter(const double *xi, double *yi, const double *w, const int length, const double lambda=2000);

private:
    void alloc(int length);
    void dealloc();

    double *v1a;
    double *v2a;
    double *da;
    double *dtd;
    double *ca;
    double *za;
    double *zb;
    double *b;

    int m_length;

};

#endif /* INCLUDE_WHITTAKEREILERS_H */
