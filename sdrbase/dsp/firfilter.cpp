///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2017, 2019-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
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

#include "firfilter.h"

namespace FirFilterGenerators
{

void generateLowPassFilter(int nTaps, double sampleRate, double cutoff, std::vector<Real> &taps)
{
    if (!(nTaps & 1))
    {
        qDebug("Filter has to have an odd number of taps");
        nTaps++;
    }

    double Wc = (2.0 * M_PI * cutoff) / sampleRate;
    int halfTaps = nTaps / 2 + 1;
    taps.resize(halfTaps);

    for (int i = 0; i < halfTaps; ++i)
    {
        if (i == halfTaps - 1)
        {
            taps[i] = Wc / M_PI;
        }
        else
        {
            int n = i - (nTaps - 1) / 2;
            taps[i] = sin(n * Wc) / (n * M_PI);
        }
    }

    // Blackman window
    for (int i = 0; i < halfTaps; i++)
    {
        int n = i - (nTaps - 1) / 2;
        taps[i] *= 0.42 + 0.5 * cos((2.0 * M_PI * n) / nTaps) + 0.08 * cos((4.0 * M_PI * n) / nTaps);
    }

    Real sum = 0;
    size_t i;

    for (i = 0; i < taps.size() - 1; ++i) {
        sum += taps[i] * 2.0;
    }

    sum += taps[i];

    for (i = 0; i < taps.size(); ++i) {
        taps[i] /= sum;
    }
}

}
