///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include <cmath>
#include <algorithm>
#include <QDebug>

#include "audiofilter.h"

// f(-3dB) = 3.6 kHz @ 48000 Hz SR (w = 0.0375):
const float AudioFilter::m_lpa[3] = {1.0,           1.392667E+00, -5.474446E-01};
const float AudioFilter::m_lpb[3] = {3.869430E-02,  7.738860E-02,  3.869430E-02};
// f(-3dB) = 300 Hz @ 8000 Hz SR (w = 0.0375):
const float AudioFilter::m_hpa[3] = {1.000000e+00,  1.667871e+00, -7.156964e-01};
const float AudioFilter::m_hpb[3] = {8.459039e-01, -1.691760e+00,  8.459039e-01};

AudioFilter::AudioFilter() :
        m_filterLP(m_lpa, m_lpb),
        m_filterHP(m_hpa, m_hpb),
        m_useHP(false)
{}

AudioFilter::~AudioFilter()
{}


void AudioFilter::setDecimFilters(int srHigh, int srLow, float fcHigh, float fcLow, float fgain)
{
    double fcNormHigh = fcHigh / srHigh;
    double fcNormLow = fcLow / srLow;

    calculate2(false, fcNormHigh, m_lpva, m_lpvb, fgain);
    calculate2(true, fcNormLow, m_hpva, m_hpvb, fgain);

    m_filterLP.setCoeffs(m_lpva, m_lpvb);
    m_filterHP.setCoeffs(m_hpva, m_hpvb);
}

void AudioFilter::calculate2(bool highPass, double fc, float *va, float *vb, float fgain)
{
    double a[22], b[22];

    cheby(highPass, fc, 0.5, 2, a, b, fgain); // low-pass, 0.5% ripple, 2 pole filter

    // Copy to the 2-pole filter coefficients
    for (int i=0; i<3; i++) {
        vb[i] = a[i];
        va[i] = b[i];
    }

    va[0] = 1.0;

    qDebug() << "AudioFilter::calculate2:"
        << " highPass: " << highPass
        << " fc: " << fc
        << " a0: " << va[0]
        << " a1: " << va[1]
        << " a2: " << va[2]
        << " b0: " << vb[0]
        << " b1: " << vb[1]
        << " b2: " << vb[2];
}

/*
 * Adapted from BASIC program in table 20-4 of
 * https://www.analog.com/media/en/technical-documentation/dsp-book/dsp_book_Ch20.pdf
 */
void AudioFilter::cheby(bool highPass, double fc, float pr, int np, double *a, double *b, float fgain)
{
    double a0, a1, a2, b1, b2;
    double ta[22], tb[22];

    std::fill(a, a+22, 0.0);
    std::fill(b, b+22, 0.0);
    a[2] = 1.0;
    b[2] = 1.0;

    for (int p = 1; p <= np/2; p++)
    {
        cheby_sub(highPass, fc, pr, np, p, a0, a1, a2, b1, b2);

        // Add coefficients to the cascade
        for (int i=0; i<22; i++)
        {
            ta[i] = a[i];
            tb[i] = b[i];
        }

        for (int i=2; i<22; i++)
        {
            a[i] = a0*ta[i] + a1*ta[i-1] + a2*ta[i-2];
            b[i] = tb[i] - b1*tb[i-1] - b2*tb[i-2];
        }
    }

    // Finish combining coefficients
    b[2] = 0;

    for (int i=0; i<20; i++)
    {
        a[i] = a[i+2];
        b[i] = -b[i+2];
    }

    // Normalize the gain
    double sa = 0.0;
    double sb = 0.0;

    for (int i=0; i<20; i++)
    {
        if (highPass)
        {
            sa += i%2 == 0 ? a[i] : -a[i];
            sb += i%2 == 0 ? b[i] : -b[i];
        }
        else
        {
            sa += a[i];
            sb += b[i];
        }
    }

    double gain = sa/(1.0 -sb);
    gain /= fgain;

    for (int i=0; i<20; i++) {
        a[i] /= gain;
    }
}

/*
 * Adapted from BASIC subroutine in table 20-5 of
 * https://www.analog.com/media/en/technical-documentation/dsp-book/dsp_book_Ch20.pdf
 */
void AudioFilter::cheby_sub(bool highPass, double fc, float pr, int np, int stage, double& a0, double& a1, double& a2, double& b1, double& b2)
{
    double rp = -cos((M_PI/(np*2)) + (stage-1)*(M_PI/np));
    double ip = sin((M_PI/(np*2)) + (stage-1)*(M_PI/np));

    // Warp from a circle to an ellipse
    double esx = 100.0 / (100.0 - pr);
    double es = sqrt(esx*esx -1.0);
    double vx = (1.0/np) * log((1.0/es) + sqrt((1.0/(es*es)) + 1.0));
    double kx = (1.0/np) * log((1.0/es) + sqrt((1.0/(es*es)) - 1.0));
    kx = (exp(kx) + exp(-kx))/2.0;
    rp = rp * ((exp(vx) - exp(-vx))/2.0) / kx;
    ip = ip * ((exp(vx) + exp(-vx))/2.0) / kx;

    double t = 2.0 * tan(0.5);
    double w = 2.0 * M_PI * fc;
    double m = rp*rp + ip*ip;
    double d = 4.0 - 4.0*rp*t + m*t*t;
    double x0 = (t*t)/d;
    double x1 = (2.0*t*t)/d;
    double x2 = (t*t)/d;
    double y1 = (8.0 - 2.0*m*t*t)/d;
    double y2 = (-4.0 - 4.0*rp*t - m*t*t)/d;
    double k;

    if (highPass) {
        k = -cos(w/2.0 + 0.5) / cos(w/2.0 - 0.5);
    } else {
        k = sin(0.5 - w/2.0) / sin(0.5 + w/2.0);
    }

    d = 1.0 + y1*k - y2*k*k;

    a0 = (x0 - x1*k + x2*k*k)/d;
    a1 = (-2.0*x0*k + x1 + x1*k*k - 2.0*x2*k)/d;
    a2 = (x0*k*k - x1*k + x2)/d;
    b1 = (2.0*k + y1 + y1*k*k - 2.0*y2*k)/d;
    b2 = (-(k*k) - y1*k + y2)/d;

    if (highPass)
    {
        a1 = -a1;
        b1 = -b1;
    }
}

float AudioFilter::run(const float& sample)
{
    return m_useHP ? m_filterLP.run(m_filterHP.run(sample)) : m_filterLP.run(sample);
}

float AudioFilter::runHP(const float& sample)
{
    return m_filterHP.run(sample);
}

float AudioFilter::runLP(const float& sample)
{
    return m_filterLP.run(sample);
}
