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

#include <algorithm>

#include "dsp/fftengine.h"
#include "interferometercorr.h"

Sample sAdd(const Sample& a, const Sample& b) { //!< Sample addition
    return Sample{a.real() + b.real(), a.imag() + b.imag()};
}

Sample sMulConj(const Sample& a, const Sample& b) { //!< Sample multiply with conjugate
    Sample s;
    // Integer processing
    int64_t ax = a.real();
    int64_t ay = a.imag();
    int64_t bx = b.real();
    int64_t by = b.imag();
    int64_t x = ax*bx + ay*by;
    int64_t y = ay*bx - ax*by;
    s.setReal(x>>SDR_RX_SAMP_SZ);
    s.setImag(y>>SDR_RX_SAMP_SZ);
    // Floating point processing (in practice there is no significant performance difference)
    // float ax = a.real() / SDR_RX_SCALEF;
    // float ay = a.imag() / SDR_RX_SCALEF;
    // float bx = b.real() / SDR_RX_SCALEF;
    // float by = b.imag() / SDR_RX_SCALEF;
    // float x = ax*bx + ay*by;
    // float y = ay*bx - ax*by;
    // s.setReal(x*SDR_RX_SCALEF);
    // s.setImag(y*SDR_RX_SCALEF);
    return s;
}

Sample cf2s(const std::complex<float>& a) { //!< Complex float to Sample
    Sample s;
    s.setReal(a.real()*SDR_RX_SCALEF);
    s.setImag(a.imag()*SDR_RX_SCALEF);
    return s;
}

Sample invfft2s(const std::complex<float>& a) { //!< Complex float to Sample
    Sample s;
    s.setReal(a.real());
    s.setImag(a.imag());
    return s;
}

InterferometerCorrelator::InterferometerCorrelator(int fftSize) :
    m_corrType(InterferometerSettings::CorrelationAdd),
    m_fftSize(fftSize)
{
    for (int i = 0; i < 2; i++)
    {
        m_fft[i] = FFTEngine::create();
        m_fft[i]->configure(2*fftSize, false); // internally twice the data FFT size
    }

    m_invFFT = FFTEngine::create();
    m_invFFT->configure(2*fftSize, true);

    m_dataj = new std::complex<float>[2*fftSize]; // receives actual FFT result hence twice the data FFT size
    m_scorr.resize(2*fftSize);
    m_tcorr.resize(2*fftSize);
}

InterferometerCorrelator::~InterferometerCorrelator()
{
    delete[] m_dataj;
    delete m_invFFT;

    for (int i = 0; i < 2; i++) {
        delete m_fft[i];
    }
}

bool InterferometerCorrelator::performCorr(
    const SampleVector& data0,
    int size0,
    const SampleVector& data1,
    int size1
)
{
    bool results = false;

    switch (m_corrType)
    {
        case InterferometerSettings::CorrelationAdd:
            results = performOpCorr(data0, size0, data1, size1, sAdd);
            break;
        case InterferometerSettings::CorrelationMultiply:
            results = performOpCorr(data0, size0, data1, size1, sMulConj);
            break;
        case InterferometerSettings::CorrelationFFT:
            results = performFFTCorr(data0, size0, data1, size1);
            break;
        default:
            break;
    }

    return results;
}

bool InterferometerCorrelator::performOpCorr(
    const SampleVector& data0,
    int size0,
    const SampleVector& data1,
    int size1,
    Sample sampleOp(const Sample& a, const Sample& b)
)
{
    unsigned int size = std::min(size0, size1);
    // if (size0 != size1) {
    //     qDebug("InterferometerCorrelator::performOpCorr: size0: %d, size1: %d", size0, size1);
    // }
    adjustTCorrSize(size);

    std::transform(
        data0.begin(),
        data0.begin() + size,
        data1.begin(),
        m_tcorr.begin(),
        sampleOp
    );

    m_processed = size;
    m_remaining[0] = size0 - size;
    m_remaining[1] = size1 - size;
    return true;
}

bool InterferometerCorrelator::performFFTCorr(
    const SampleVector& data0,
    int size0,
    const SampleVector& data1,
    int size1
)
{
    unsigned int size = std::min(size0, size1);
    int nfft = 0;
    SampleVector::const_iterator begin0 = data0.begin();
    SampleVector::const_iterator begin1 = data1.begin();
    adjustSCorrSize(size);
    adjustTCorrSize(size);

    while (size > m_fftSize)
    {
        // FFT[0]
        std::transform(
            begin0,
            begin0 + m_fftSize,
            m_fft[0]->in(),
            [](const Sample& s) -> std::complex<float> {
                return std::complex<float>{s.real() / SDR_RX_SCALEF, s.imag() / SDR_RX_SCALEF};
            }
        );
        std::fill(m_fft[0]->in() + m_fftSize, m_fft[0]->in() + 2*m_fftSize, std::complex<float>{0, 0});
        m_fft[0]->transform();

        // FFT[1]
        std::transform(
            begin1,
            begin1 + m_fftSize,
            m_fft[1]->in(),
            [](const Sample& s) -> std::complex<float> {
                return std::complex<float>{s.real() / SDR_RX_SCALEF, s.imag() / SDR_RX_SCALEF};
            }
        );
        std::fill(m_fft[1]->in() + m_fftSize, m_fft[1]->in() + 2*m_fftSize, std::complex<float>{0, 0});
        m_fft[1]->transform();

        // conjugate FFT[1]
        std::transform(
            m_fft[1]->out(),
            m_fft[1]->out()+2*m_fftSize,
            m_dataj,
            [](const std::complex<float>& c) -> std::complex<float> {
                return std::conj(c);
            }
        );

        // product of FFT[1]* with FFT[0] and store in inverse FFT input
        std::transform(
            m_fft[0]->out(),
            m_fft[0]->out()+2*m_fftSize,
            m_dataj,
            m_invFFT->in(),
            [](std::complex<float>& a, const std::complex<float>& b) -> std::complex<float> {
                return a*b;
            }
        );

        // copy product to correlation spectrum
        std::transform(
            m_invFFT->in(),
            m_invFFT->in() + 2*m_fftSize,
            m_scorr.begin(),
            cf2s
        );

        // do the inverse FFT to get time correlation
        m_invFFT->transform();
        std::transform(
            m_invFFT->out(),
            m_invFFT->out() + 2*m_fftSize,
            m_tcorr.begin(),
            invfft2s
        );

        size -= m_fftSize;
        begin0 += m_fftSize;
        begin1 += m_fftSize;
        nfft++;
    }

    // update the samples counters
    m_processed = nfft*m_fftSize;
    m_remaining[0] = size0 - nfft*m_fftSize;
    m_remaining[1] = size1 - nfft*m_fftSize;

    return nfft > 0;
}

void InterferometerCorrelator::adjustSCorrSize(int size)
{
    if (size > m_scorrSize)
    {
        m_scorr.resize(size);
        m_scorrSize = size;
    }
}

void InterferometerCorrelator::adjustTCorrSize(int size)
{
    if (size > m_tcorrSize)
    {
        m_tcorr.resize(size);
        m_tcorrSize = size;
    }
}
