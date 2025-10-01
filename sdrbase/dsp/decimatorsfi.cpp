///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#include "decimatorsfi.h"

template<>
SDRBASE_API void DecimatorsFI<true>::decimate1(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ)
{
    float xreal, yimag;

    for (int pos = 0; pos < nbIAndQ - 1; pos += 2)
    {
        xreal = buf[pos+0];
        yimag = buf[pos+1];
        (**it).setReal(xreal * SDR_RX_SCALEF);
        (**it).setImag(yimag * SDR_RX_SCALEF);
        ++(*it); // Valgrind optim (comment not repeated)
    }
}

// Specialization for 2 channels interleaved I/Q input
template<>
SDRBASE_API void DecimatorsFI<true>::decimate1_2(SampleVector::iterator* it1, SampleVector::iterator* it2, const float* buf, qint32 nbIAndQ)
{
    float xreal1, yimag1, xreal2, yimag2;

    for (int pos = 0; pos < nbIAndQ - 3; pos += 4)
    {
        // First channel
        xreal1 = buf[pos+0];
        yimag1 = buf[pos+1];
        (**it1).setReal(xreal1 * SDR_RX_SCALEF);
        (**it1).setImag(yimag1 * SDR_RX_SCALEF);
        ++(*it1); // Valgrind optim (comment not repeated)

        // Second channel
        xreal2 = buf[pos+2];
        yimag2 = buf[pos+3];
        (**it2).setReal(xreal2 * SDR_RX_SCALEF);
        (**it2).setImag(yimag2 * SDR_RX_SCALEF);
        ++(*it2); // Valgrind optim (comment not repeated)
    }
}

template<>
SDRBASE_API void DecimatorsFI<false>::decimate1(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ)
{
    float xreal, yimag;

    for (int pos = 0; pos < nbIAndQ - 1; pos += 2)
    {
        xreal = buf[pos+1];
        yimag = buf[pos+0];
        (**it).setReal(xreal * SDR_RX_SCALEF);
        (**it).setImag(yimag * SDR_RX_SCALEF);
        ++(*it); // Valgrind optim (comment not repeated)
    }
}

// Specialization for 2 channels interleaved I/Q input
template<>
SDRBASE_API void DecimatorsFI<false>::decimate1_2(SampleVector::iterator* it1, SampleVector::iterator* it2, const float* buf, qint32 nbIAndQ)
{
    float xreal1, yimag1, xreal2, yimag2;

    for (int pos = 0; pos < nbIAndQ - 3; pos += 4)
    {
        // First channel
        xreal1 = buf[pos+1];
        yimag1 = buf[pos+0];
        (**it1).setReal(xreal1 * SDR_RX_SCALEF);
        (**it1).setImag(yimag1 * SDR_RX_SCALEF);
        ++(*it1); // Valgrind optim (comment not repeated)

        // Second channel
        xreal2 = buf[pos+3];
        yimag2 = buf[pos+2];
        (**it2).setReal(xreal2 * SDR_RX_SCALEF);
        (**it2).setImag(yimag2 * SDR_RX_SCALEF);
        ++(*it2); // Valgrind optim (comment not repeated)
    }
}

template<>
SDRBASE_API void DecimatorsFI<true>::decimate2_inf(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ)
{
    float xreal, yimag;

    for (int pos = 0; pos < nbIAndQ - 7; pos += 8)
    {
        xreal = (buf[pos+0] - buf[pos+3]);
        yimag = (buf[pos+1] + buf[pos+2]);
        (**it).setReal(xreal * SDR_RX_SCALED);
        (**it).setImag(yimag * SDR_RX_SCALED);
        ++(*it);

        xreal = (buf[pos+7] - buf[pos+4]);
        yimag = (- buf[pos+5] - buf[pos+6]);
        (**it).setReal(xreal * SDR_RX_SCALED);
        (**it).setImag(yimag * SDR_RX_SCALED);
        ++(*it);
    }
}

template<>
SDRBASE_API void DecimatorsFI<false>::decimate2_inf(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ)
{
    float xreal, yimag;

    for (int pos = 0; pos < nbIAndQ - 7; pos += 8)
    {
        xreal = (buf[pos+1] + buf[pos+2]);
        yimag = (buf[pos+0] - buf[pos+3]);
        (**it).setReal(xreal * SDR_RX_SCALED);
        (**it).setImag(yimag * SDR_RX_SCALED);
        ++(*it);

        xreal = (- buf[pos+5] - buf[pos+6]);
        yimag = (buf[pos+7] - buf[pos+4]);
        (**it).setReal(xreal * SDR_RX_SCALED);
        (**it).setImag(yimag * SDR_RX_SCALED);
        ++(*it);
    }
}

template<>
SDRBASE_API void DecimatorsFI<true>::decimate2_sup(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ)
{
    float xreal, yimag;

    for (int pos = 0; pos < nbIAndQ - 7; pos += 8)
    {
        xreal = (buf[pos+1] - buf[pos+2]);
        yimag = (- buf[pos+0] - buf[pos+3]);
        (**it).setReal(xreal * SDR_RX_SCALED);
        (**it).setImag(yimag * SDR_RX_SCALED);
        ++(*it);

        xreal = (buf[pos+6] - buf[pos+5]);
        yimag = (buf[pos+4] + buf[pos+7]);
        (**it).setReal(xreal * SDR_RX_SCALED);
        (**it).setImag(yimag * SDR_RX_SCALED);
        ++(*it);
    }
}

template<>
SDRBASE_API void DecimatorsFI<false>::decimate2_sup(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ)
{
    float xreal, yimag;

    for (int pos = 0; pos < nbIAndQ - 7; pos += 8)
    {
        xreal = (- buf[pos+0] - buf[pos+3]);
        yimag = (buf[pos+1] - buf[pos+2]);
        (**it).setReal(xreal * SDR_RX_SCALED);
        (**it).setImag(yimag * SDR_RX_SCALED);
        ++(*it);

        xreal = (buf[pos+4] + buf[pos+7]);
        yimag = (buf[pos+6] - buf[pos+5]);
        (**it).setReal(xreal * SDR_RX_SCALED);
        (**it).setImag(yimag * SDR_RX_SCALED);
        ++(*it);
    }
}

template<>
SDRBASE_API void DecimatorsFI<true>::decimate4_inf(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ)
{
    float xreal, yimag;

    for (int pos = 0; pos < nbIAndQ - 7; pos += 8)
    {
        xreal = (buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]);
        yimag = (buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]);

        (**it).setReal(xreal * SDR_RX_SCALED);
        (**it).setImag(yimag * SDR_RX_SCALED);

        ++(*it);
    }
}

template<>
SDRBASE_API void DecimatorsFI<false>::decimate4_inf(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ)
{
    float xreal, yimag;

    for (int pos = 0; pos < nbIAndQ - 7; pos += 8)
    {
        xreal = (buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]);
        yimag = (buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]);

        (**it).setReal(xreal * SDR_RX_SCALED);
        (**it).setImag(yimag * SDR_RX_SCALED);

        ++(*it);
    }
}

template<>
SDRBASE_API void DecimatorsFI<true>::decimate4_sup(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ)
{
    // Sup (USB):
    //            x  y   x  y   x   y  x   y  / x -> 1,-2,-5,6 / y -> -0,-3,4,7
    // [ rotate:  1, 0, -2, 3, -5, -4, 6, -7]
    // Inf (LSB):
    //            x  y   x  y   x   y  x   y  / x -> 0,-3,-4,7 / y -> 1,2,-5,-6
    // [ rotate:  0, 1, -3, 2, -4, -5, 7, -6]
    float xreal, yimag;

    for (int pos = 0; pos < nbIAndQ - 7; pos += 8)
    {
        xreal = (buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6]);
        yimag = (- buf[pos+0] - buf[pos+3] + buf[pos+4] + buf[pos+7]);

        (**it).setReal(xreal * SDR_RX_SCALED);
        (**it).setImag(yimag * SDR_RX_SCALED);

        ++(*it);
    }
}

template<>
SDRBASE_API void DecimatorsFI<false>::decimate4_sup(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ)
{
    // Sup (USB):
    //            x  y   x  y   x   y  x   y  / x -> 1,-2,-5,6 / y -> -0,-3,4,7
    // [ rotate:  1, 0, -2, 3, -5, -4, 6, -7]
    // Inf (LSB):
    //            x  y   x  y   x   y  x   y  / x -> 0,-3,-4,7 / y -> 1,2,-5,-6
    // [ rotate:  0, 1, -3, 2, -4, -5, 7, -6]
    float xreal, yimag;

    for (int pos = 0; pos < nbIAndQ - 7; pos += 8)
    {
        xreal = (- buf[pos+0] - buf[pos+3] + buf[pos+4] + buf[pos+7]);
        yimag = (buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6]);

        (**it).setReal(xreal * SDR_RX_SCALED);
        (**it).setImag(yimag * SDR_RX_SCALED);

        ++(*it);
    }
}
