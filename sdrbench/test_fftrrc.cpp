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
#include "dsp/fftfilterrrc.h"
#include <QDebug>

void MainBench::testFFTRRCFilter()
{
    qDebug() << "MainBench::testFFTRRCFilter";
    const int RRC_FFT_SIZE = 512;
    std::complex<float>* rrcFilterOut = nullptr;
    FFTFilterRRC filter(RRC_FFT_SIZE);
    filter.create(0.05f, 0.35f);  // 2400 baud, 0.35 rolloff

    qDebug() << "MainBench::testFFTRRCFilter: filter created";
    FILE *fd_filter = fopen("test_fftrrc_filter.txt", "w");

    for (int i = 0; i < RRC_FFT_SIZE; i++) {
        fprintf(fd_filter, "%f\n", std::abs(filter.getFilter()[i]));
    }
    qDebug() << "MainBench::testFFTRRCFilter: filter coefficients written to test_fftrrc_filter.txt";
    fclose(fd_filter);

    qDebug() << "MainBench::testFFTRRCFilter: running filter";
    FILE *fd = fopen("test_fftrrc.txt", "w");
    int outLen = 0;

    for (int i = 0; i < 1024; i++)
    {
        Real phi = i * (48000.0 / 1200.0) * (2*3.141);
        Real x = sin(phi);
        filter.process(Complex(x, 0.0f), &rrcFilterOut);
        outLen++;

        if (outLen % (RRC_FFT_SIZE / 2) == 0)
        {
            // printf("\ni=%d n_out: %d\n", i, n_out);

            for (int j = 0; j < outLen; j++)
            {
                Complex rrc = rrcFilterOut[j];
                fprintf(fd, "%f\n", rrc.real());
            }

            outLen = 0;
        }
    }

    // output any remaining samples
    for (int j = 0; j < outLen; j++)
    {
        Complex rrc = rrcFilterOut[j];
        fprintf(fd, "%f\n", rrc.real());
    }

    qDebug() << "MainBench::testFFTRRCFilter: output samples written to test_fftrrc.txt";
    fclose(fd);
}
