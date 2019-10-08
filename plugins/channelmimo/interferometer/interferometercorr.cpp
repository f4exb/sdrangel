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
#include <functional>

#include "dsp/fftengine.h"
#include "interferometercorr.h"

Sample sFirst(const Sample& a, const Sample& b) {
    return a;
}

Sample sSecond(const Sample& a, const Sample& b) {
    return b;
}

Sample sSecondInv(const Sample& a, const Sample& b) {
    return Sample{-b.real(), -b.imag()};
}

Sample sAdd(const Sample& a, const Sample& b) { //!< Sample addition
    return Sample{a.real() + b.real(), a.imag() + b.imag()};
}

Sample sAddInv(const Sample& a, const Sample& b) { //!< Sample addition
    return Sample{a.real() - b.real(), a.imag() + b.imag()};
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
    s.setReal(x>>(SDR_RX_SAMP_SZ-1));
    s.setImag(y>>(SDR_RX_SAMP_SZ-1));
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

Sample sMulConjInv(const Sample& a, const Sample& b) { //!< Sample multiply with conjugate
    Sample s;
    // Integer processing
    int64_t ax = a.real();
    int64_t ay = a.imag();
    int64_t bx = -b.real();
    int64_t by = -b.imag();
    int64_t x = ax*bx + ay*by;
    int64_t y = ay*bx - ax*by;
    s.setReal(x>>(SDR_RX_SAMP_SZ-1));
    s.setImag(y>>(SDR_RX_SAMP_SZ-1));
    return s;
}

Sample invfft2s(const std::complex<float>& a) { //!< Complex float to Sample for 1 side time correlation
    Sample s;
    s.setReal(a.real()/2.0f);
    s.setImag(a.imag()/2.0f);
    return s;
}

Sample invfft2s2(const std::complex<float>& a) { //!< Complex float to Sample for 2 sides time correlation
    Sample s;
    s.setReal(a.real());
    s.setImag(a.imag());
    return s;
}

InterferometerCorrelator::InterferometerCorrelator(int fftSize) :
    m_corrType(InterferometerSettings::CorrelationAdd),
    m_fftSize(fftSize)
{
    setPhase(0);
    m_window.create(FFTWindow::Function::Hanning, fftSize);
    m_data0w.resize(m_fftSize);
    m_data1w.resize(m_fftSize);

    for (int i = 0; i < 2; i++)
    {
        m_fft[i] = FFTEngine::create();
        m_fft[i]->configure(2*fftSize, false); // internally twice the data FFT size
        m_fft2[i] = FFTEngine::create();
        m_fft2[i]->configure(fftSize, false);
    }

    m_invFFT = FFTEngine::create();
    m_invFFT->configure(2*fftSize, true);
    m_invFFT2 = FFTEngine::create();
    m_invFFT2->configure(fftSize, true);

    m_dataj = new std::complex<float>[2*fftSize]; // receives actual FFT result hence twice the data FFT size
    m_scorr.resize(fftSize);
    m_tcorr.resize(fftSize);
    m_scorrSize = fftSize;
    m_tcorrSize = fftSize;
}

InterferometerCorrelator::~InterferometerCorrelator()
{
    delete[] m_dataj;
    delete m_invFFT;
    delete m_invFFT2;

    for (int i = 0; i < 2; i++)
    {
        delete m_fft[i];
        delete m_fft2[i];
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

    if (m_phase == 0)
    {
        switch (m_corrType)
        {
            case InterferometerSettings::Correlation0:
                results = performOpCorr(data0, size0, data1, size1, sFirst);
                break;
            case InterferometerSettings::Correlation1:
                results = performOpCorr(data0, size0, data1, size1, sSecond);
                break;
            case InterferometerSettings::CorrelationAdd:
                results = performOpCorr(data0, size0, data1, size1, sAdd);
                break;
            case InterferometerSettings::CorrelationMultiply:
                results = performOpCorr(data0, size0, data1, size1, sMulConj);
                break;
            case InterferometerSettings::CorrelationFFT:
                results = performFFTCorr(data0, size0, data1, size1);
                break;
            case InterferometerSettings::CorrelationFFT2:
                results = performFFT2Corr(data0, size0, data1, size1);
                break;
            default:
                break;
        }
    }
    else if ((m_phase == -180) || (m_phase == 180))
    {
        switch (m_corrType)
        {
            case InterferometerSettings::Correlation0:
                results = performOpCorr(data0, size0, data1, size1, sFirst);
                break;
            case InterferometerSettings::Correlation1:
                results = performOpCorr(data0, size0, data1, size1, sSecondInv);
                break;
            case InterferometerSettings::CorrelationAdd:
                results = performOpCorr(data0, size0, data1, size1, sAddInv);
                break;
            case InterferometerSettings::CorrelationMultiply:
                results = performOpCorr(data0, size0, data1, size1, sMulConjInv);
                break;
            case InterferometerSettings::CorrelationFFT:
                results = performFFTCorr(data0, size0, m_data1p, size1);
                break;
            case InterferometerSettings::CorrelationFFT2:
                results = performFFT2Corr(data0, size0, m_data1p, size1);
                break;
            default:
                break;
        }
    }
    else
    {
        if (size1 > m_data1p.size()) {
            m_data1p.resize(size1);
        }

        std::transform(
            data1.begin(),
            data1.begin() + size1,
            m_data1p.begin(),
            [this](const Sample& s) -> Sample {
                Sample t;
                int64_t sx = s.real();
                int64_t sy = s.imag();
                int64_t x = sx*m_cos + sy*m_sin;
                int64_t y = sy*m_cos - sx*m_sin;
                t.setReal(x>>(SDR_RX_SAMP_SZ-1));
                t.setImag(y>>(SDR_RX_SAMP_SZ-1));
                return t;
            }
        );

        switch (m_corrType)
        {
            case InterferometerSettings::Correlation0:
                results = performOpCorr(data0, size0, m_data1p, size1, sFirst);
                break;
            case InterferometerSettings::Correlation1:
                results = performOpCorr(data0, size0, m_data1p, size1, sSecond);
                break;
            case InterferometerSettings::CorrelationAdd:
                results = performOpCorr(data0, size0, m_data1p, size1, sAdd);
                break;
            case InterferometerSettings::CorrelationMultiply:
                results = performOpCorr(data0, size0, m_data1p, size1, sMulConj);
                break;
            case InterferometerSettings::CorrelationFFT:
                results = performFFTCorr(data0, size0, m_data1p, size1);
                break;
            case InterferometerSettings::CorrelationFFT2:
                results = performFFT2Corr(data0, size0, m_data1p, size1);
                break;
            default:
                break;
        }
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

    while (size >= m_fftSize)
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
        m_window.apply(m_fft[0]->in());
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
        m_window.apply(m_fft[1]->in());
        std::fill(m_fft[1]->in() + m_fftSize, m_fft[1]->in() + 2*m_fftSize, std::complex<float>{0, 0});
        m_fft[1]->transform();

        // conjugate FFT[1]
        std::transform(
            m_fft[1]->out(),
            m_fft[1]->out() + 2*m_fftSize,
            m_dataj,
            [](const std::complex<float>& c) -> std::complex<float> {
                return std::conj(c);
            }
        );

        // product of FFT[1]* with FFT[0] and store in inverse FFT input
        std::transform(
            m_fft[0]->out(),
            m_fft[0]->out() + 2*m_fftSize,
            m_dataj,
            m_invFFT->in(),
            [](std::complex<float>& a, const std::complex<float>& b) -> std::complex<float> {
                return (a*b);
            }
        );

        // copy product to correlation spectrum - convert and scale to FFT size
        std::transform(
            m_invFFT->in(),
            m_invFFT->in() + m_fftSize,
            m_scorr.begin() + nfft*m_fftSize,
            [this](const std::complex<float>& a) -> Sample {
                Sample s;
                s.setReal(a.real()*(SDR_RX_SCALEF/m_fftSize));
                s.setImag(a.imag()*(SDR_RX_SCALEF/m_fftSize));
                return s;
            }
        );

        // do the inverse FFT to get time correlation
        m_invFFT->transform();
        std::transform(
            m_invFFT->out(),
            m_invFFT->out() + m_fftSize,
            m_tcorr.begin() + nfft*m_fftSize,
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

bool InterferometerCorrelator::performFFT2Corr(
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

    while (size >= m_fftSize)
    {
        // FFT[0]
        std::transform(
            begin0,
            begin0 + m_fftSize,
            m_fft2[0]->in(),
            [](const Sample& s) -> std::complex<float> {
                return std::complex<float>{s.real() / SDR_RX_SCALEF, s.imag() / SDR_RX_SCALEF};
            }
        );
        m_window.apply(m_fft2[0]->in());
        m_fft2[0]->transform();

        // FFT[1]
        std::transform(
            begin1,
            begin1 + m_fftSize,
            m_fft2[1]->in(),
            [](const Sample& s) -> std::complex<float> {
                return std::complex<float>{s.real() / SDR_RX_SCALEF, s.imag() / SDR_RX_SCALEF};
            }
        );
        m_window.apply(m_fft2[1]->in());
        m_fft2[1]->transform();

        // conjugate FFT[1]
        std::transform(
            m_fft2[1]->out(),
            m_fft2[1]->out() + m_fftSize,
            m_dataj,
            [](const std::complex<float>& c) -> std::complex<float> {
                return std::conj(c);
            }
        );

        // product of FFT[1]* with FFT[0] and store in inverse FFT input
        std::transform(
            m_fft2[0]->out(),
            m_fft2[0]->out() + m_fftSize,
            m_dataj,
            m_invFFT2->in(),
            [](std::complex<float>& a, const std::complex<float>& b) -> std::complex<float> {
                return (a*b);
            }
        );

        // copy product to correlation spectrum - convert and scale to FFT size
        std::transform(
            m_invFFT2->in(),
            m_invFFT2->in() + m_fftSize,
            m_scorr.begin() + nfft*m_fftSize,
            [this](const std::complex<float>& a) -> Sample {
                Sample s;
                s.setReal(a.real()*(SDR_RX_SCALEF/m_fftSize));
                s.setImag(a.imag()*(SDR_RX_SCALEF/m_fftSize));
                return s;
            }
        );

        // do the inverse FFT to get time correlation
        m_invFFT2->transform();
        std::transform(
            m_invFFT2->out() + m_fftSize/2 + 1,
            m_invFFT2->out() + m_fftSize,
            m_tcorr.begin() + nfft*m_fftSize,
            invfft2s2
        );
        std::transform(
            m_invFFT2->out(),
            m_invFFT2->out() + m_fftSize/2,
            m_tcorr.begin() + nfft*m_fftSize + m_fftSize/2,
            invfft2s2
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
    int nFFTSize = (size/m_fftSize)*m_fftSize;

    if (nFFTSize > m_scorrSize)
    {
        m_scorr.resize(nFFTSize);
        m_scorrSize = nFFTSize;
    }
}

void InterferometerCorrelator::adjustTCorrSize(int size)
{
    int nFFTSize = (size/m_fftSize)*m_fftSize;

    if (nFFTSize > m_tcorrSize)
    {
        m_tcorr.resize(nFFTSize);
        m_tcorrSize = nFFTSize;
    }
}

void InterferometerCorrelator::setPhase(int phase)
{
    m_phase = phase;

    if (phase == 0)
    {
        m_sin = 0;
        m_cos = 1<<(SDR_RX_SAMP_SZ-1);
    }
    else if (phase == 90)
    {
        m_sin = 1<<(SDR_RX_SAMP_SZ-1);
        m_cos = 0;
    }
    else if (phase == -90)
    {
        m_sin = -(1<<(SDR_RX_SAMP_SZ-1));
        m_cos = 0;
    }
    else if ((phase == -180) || (phase == 180))
    {
        m_sin = 0;
        m_cos = -(1<<(SDR_RX_SAMP_SZ-1));
    }
    else
    {
        m_phase = phase % 180;
        double d_sin = sin(M_PI*(m_phase/180.0)) * (1<<(SDR_RX_SAMP_SZ-1));
        double d_cos = cos(M_PI*(m_phase/180.0)) * (1<<(SDR_RX_SAMP_SZ-1));
        m_sin = d_sin;
        m_cos = d_cos;
    }
}