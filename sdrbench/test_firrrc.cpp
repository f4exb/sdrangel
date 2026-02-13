///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include "mainbench.h"
#include "dsp/firfilterrrc.h"
#include <QDebug>

void MainBench::testFIRRRCFilter()
{
    qDebug() << "MainBench::testFIRRRCFilter";
    FIRFilterRRC filter;
    filter.create(0.05f, 0.35f, 8, FIRFilterRRC::Normalization::Gain);  // 2400 baud, 0.35 rolloff

    qDebug() << "MainBench::testFIRRRCFilter: filter created";
    FILE *fd_filter = fopen("test_rrc_filter.txt", "w");
    const std::vector<float>& taps = filter.getTaps();
    for (auto tap : taps) {
        fprintf(fd_filter, "%f\n", std::abs(tap));
    }
    qDebug() << "MainBench::testFIRRRCFilter: filter coefficients written to test_rrc_filter.txt";
    fclose(fd_filter);

    qDebug() << "MainBench::testFIRRRCFilter: running filter";
    FILE *fd = fopen("test_rrc.txt", "w");

    for (int i = 0; i < 1000; i++)
    {
        Real phi = i * (1200.0 / 48000.0) * (2*3.141);
        Real x = sin(phi) > 0.0 ? 1.0f : -1.0f;  // Simulate a BPSK signal at 1200 baud
        Complex rrc = filter.filter(Complex(x, 0.0f));
        fprintf(fd, "%f\n", rrc.real());
    }

    qDebug() << "MainBench::testFIRRRCFilter: output samples written to test_firrrc.txt";
    fclose(fd);
}
