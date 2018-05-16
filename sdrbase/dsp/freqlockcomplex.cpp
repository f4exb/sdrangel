///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// See: http://liquidsdr.org/blog/pll-howto/                                     //
// Fixed filter registers saturation                                             //
// Added order for PSK locking. This brilliant idea actually comes from this     //
// post: https://www.dsprelated.com/showthread/comp.dsp/36356-1.php              //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "freqlockcomplex.h"
#include "IIRFilterCode.h"

FreqLockComplex::FreqLockComplex() :
    m_a0(1.0),
    m_a1(1.0),
    m_a2(1.0),
    m_b0(1.0),
    m_b1(1.0),
    m_b2(1.0),
    m_v0(0.0),
    m_v1(0.0),
    m_v2(0.0),
    m_y(1.0, 0.0),
    m_prod(1.0, 0.0),
    m_yRe(1.0),
    m_yIm(0.0),
    m_freq(0.0),
    m_phi(0.0),
    m_iir(0)
{
}

FreqLockComplex::~FreqLockComplex()
{
    if (m_iir) {
        delete m_iir;
    }
}

void FreqLockComplex::reset()
{
    m_v0 = 0.0f;
    m_v1 = 0.0f;
    m_v2 = 0.0f;
    m_y.real(1.0);
    m_y.imag(0.0);
    m_prod.real(1.0);
    m_prod.imag(0.0);
    m_yRe = 1.0f;
    m_yIm = 0.0f;
    m_freq = 0.0f;
    m_phi = 0.0f;
}

// wn is in terms of Nyquist. For example, if the sampling frequency = 20 kHz
// and the 3 dB corner frequency is 1.5 kHz, then OmegaC = 0.15
// i.e. 100.0 / (SR/2) or 200 / SR for 100 Hz
void FreqLockComplex::computeCoefficients(float wn)
{
    kitiirfir::TIIRFilterParams params;

    params.BW = 0.0;         // For band pass and notch filters - unused here
    params.Gamma = 0.0;      // For Adjustable Gauss. - unused here
    params.IIRPassType = kitiirfir::iirLPF;
    params.NumPoles = 1;
    params.OmegaC = wn;
    params.ProtoType = kitiirfir::BUTTERWORTH;
    params.Ripple = 0.0;     // For Elliptic and Chebyshev - unused here
    params.StopBanddB = 0.0; // For Elliptic and Inverse Chebyshev - unused here
    params.dBGain = 0.0;

    kitiirfir::TIIRCoeff coeff = kitiirfir::CalcIIRFilterCoeff(params);
    float a[3], b[3];

    a[0] = coeff.a0[0];
    a[1] = coeff.a1[0];
    a[2] = coeff.a2[0];
    b[0] = coeff.b0[0];
    b[1] = coeff.b1[0];
    b[2] = coeff.b2[0];

    qDebug("FreqLockComplex::computeCoefficients: b: %f %f %f", b[0], b[1], b[2]);
    qDebug("FreqLockComplex::computeCoefficients: a: %f %f %f", a[0], a[1], a[2]);

    m_iir = new IIRFilter<float, 2>(a, b);
}

void FreqLockComplex::feed(float re, float im)
{
    m_yRe = cos(m_phi);
    m_yIm = sin(m_phi);
    std::complex<float> y(m_yRe, m_yIm);
    std::complex<float> x(re, im);

    std::complex<float> prod = x * m_y;

    // Discriminator: cross * sign(dot) / dt
    float cross = m_prod.real()*prod.imag() - prod.real()*m_prod.imag();
    float dot = m_prod.real()*prod.real() + m_prod.imag()*prod.imag();
    float eF = cross * (dot < 0 ? -1 : 1); // frequency error

    // LPF section
    float efHat = m_iir->run(eF);

    m_freq = efHat; // correct instantaneous frequency
    m_phi += efHat; // advance phase with instantaneous frequency
    m_prod = prod;  // store previous product
}

