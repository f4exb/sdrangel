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

#include "dsp/dspengine.h"
#include "dsp/fftfactory.h"
#include "dsp/fftengine.h"

#include "interferometercorr.h"

std::complex<float> s2c(const Sample& s)
{
    return std::complex<float>{s.real() / SDR_RX_SCALEF, s.imag() / SDR_RX_SCALEF};
}

std::complex<float> s2cNorm(const Sample& s)
{
    float x = s.real() / SDR_RX_SCALEF;
    float y = s.imag() / SDR_RX_SCALEF;
    float m = sqrt(x*x + y*y);
    return std::complex<float>{x/m, y/m};
}

Sample sFirst(const Sample& a, const Sample& b) {
    (void) b;
    return a;
}

Sample sSecond(const Sample& a, const Sample& b) {
    (void) a;
    return b;
}

Sample sSecondInv(const Sample& a, const Sample& b) {
    (void) a;
    return Sample{-b.real(), -b.imag()};
}

Sample sAdd(const Sample& a, const Sample& b) { //!< Sample addition
    return Sample{(a.real()+b.real())/2, (a.imag()+b.imag())/2};
}

Sample sAddInv(const Sample& a, const Sample& b) { //!< Sample addition
    return Sample{(a.real()-b.real())/2, (a.imag()+b.imag())/2};
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

Sample invfft2star(const std::complex<float>& a) { //!< Complex float to Sample for 1 side time correlation
    Sample s;
    s.setReal(a.real()/2.82842712475f); // 2*sqrt(2)
    s.setImag(a.imag()/2.82842712475f);
    return s;
}

InterferometerCorrelator::InterferometerCorrelator(int fftSize) :
    m_corrType(InterferometerSettings::CorrelationAdd),
    m_fftSize(fftSize)
{
    setPhase(0);
    FFTFactory *fftFactory = DSPEngine::instance()->getFFTFactory();
    m_window.create(FFTWindow::Function::Hanning, fftSize);
    m_data0w.resize(m_fftSize);
    m_data1w.resize(m_fftSize);

    for (int i = 0; i < 2; i++)
    {
        m_fftSequences[i] = fftFactory->getEngine(2*fftSize, false, &m_fft[i]); // internally twice the data FFT size
        m_fft2Sequences[i] = fftFactory->getEngine(fftSize, false, &m_fft2[i]);
    }

    m_invFFTSequence = fftFactory->getEngine(2*fftSize, true, &m_invFFT);
    m_invFFT2Sequence = fftFactory->getEngine(fftSize, true, &m_invFFT2);

    m_dataj = new std::complex<float>[2*fftSize]; // receives actual FFT result hence twice the data FFT size
    m_scorr.resize(fftSize);
    m_tcorr.resize(fftSize);
    m_scorrSize = fftSize;
    m_tcorrSize = fftSize;
}

InterferometerCorrelator::~InterferometerCorrelator()
{
    FFTFactory *fftFactory = DSPEngine::instance()->getFFTFactory();
    fftFactory->releaseEngine(2*m_fftSize, true, m_invFFTSequence);
    fftFactory->releaseEngine(m_fftSize, true, m_invFFT2Sequence);
    delete[] m_dataj;

    for (int i = 0; i < 2; i++)
    {
        fftFactory->releaseEngine(2*m_fftSize, false, m_fftSequences[i]);
        fftFactory->releaseEngine(m_fftSize, false, m_fft2Sequences[i]);
    }
}

bool InterferometerCorrelator::performCorr(
    const SampleVector& data0,
    unsigned int size0,
    const SampleVector& data1,
    unsigned int size1
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
            case InterferometerSettings::CorrelationIFFT:
                results = performIFFTCorr(data0, size0, data1, size1);
                break;
            case InterferometerSettings::CorrelationIFFTStar:
                results = performIFFTCorr(data0, size0, data1, size1, true);
                break;
            case InterferometerSettings::CorrelationFFT:
                results = performFFTProd(data0, size0, data1, size1);
                break;
            case InterferometerSettings::CorrelationIFFT2:
                results = performIFFT2Corr(data0, size0, data1, size1);
                break;
            default:
                break;
        }
    }
    else if ((m_phase == -180) || (m_phase == 180))
    {
        if ((m_corrType == InterferometerSettings::CorrelationIFFT)
         || (m_corrType == InterferometerSettings::CorrelationIFFT2)
         || (m_corrType == InterferometerSettings::CorrelationIFFTStar)
         || (m_corrType == InterferometerSettings::CorrelationFFT))
        {
            if (size1 > m_data1p.size()) {
                m_data1p.resize(size1);
            }

            std::transform(
                data1.begin(),
                data1.begin() + size1,
                m_data1p.begin(),
                [](const Sample& s) -> Sample {
                    return Sample{-s.real(), -s.imag()};
                }
            );
        }

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
            case InterferometerSettings::CorrelationIFFT:
                results = performIFFTCorr(data0, size0, m_data1p, size1);
                break;
            case InterferometerSettings::CorrelationIFFTStar:
                results = performIFFTCorr(data0, size0, m_data1p, size1, true);
                break;
            case InterferometerSettings::CorrelationFFT:
                results = performFFTProd(data0, size0, m_data1p, size1);
                break;
            case InterferometerSettings::CorrelationIFFT2:
                results = performIFFT2Corr(data0, size0, m_data1p, size1);
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
            case InterferometerSettings::CorrelationIFFT:
                results = performIFFTCorr(data0, size0, m_data1p, size1);
                break;
            case InterferometerSettings::CorrelationIFFTStar:
                results = performIFFTCorr(data0, size0, m_data1p, size1, true);
                break;
            case InterferometerSettings::CorrelationFFT:
                results = performFFTProd(data0, size0, m_data1p, size1);
                break;
            case InterferometerSettings::CorrelationIFFT2:
                results = performIFFT2Corr(data0, size0, m_data1p, size1);
                break;
            default:
                break;
        }
    }

    return results;
}

bool InterferometerCorrelator::performOpCorr(
    const SampleVector& data0,
    unsigned int size0,
    const SampleVector& data1,
    unsigned int size1,
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

bool InterferometerCorrelator::performIFFTCorr(
    const SampleVector& data0,
    unsigned int size0,
    const SampleVector& data1,
    unsigned int size1,
    bool star
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
            s2c
        );
        m_window.apply(m_fft[0]->in());
        std::fill(m_fft[0]->in() + m_fftSize, m_fft[0]->in() + 2*m_fftSize, std::complex<float>{0, 0});
        m_fft[0]->transform();

        // FFT[1]
        std::transform(
            begin1,
            begin1 + m_fftSize,
            m_fft[1]->in(),
            s2c
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

        // copy product to correlation spectrum - convert and scale to FFT size and Hanning window
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

        if (star)
        {
            // sum first half with the reversed second half as one is the conjugate of the other this should yield constant phase
            *m_tcorr.begin() = invfft2star(m_invFFT->out()[0]); // t = 0
            std::reverse(m_invFFT->out() + m_fftSize, m_invFFT->out() + 2*m_fftSize);
            std::transform(
                m_invFFT->out() + 1,
                m_invFFT->out() + m_fftSize,
                m_invFFT->out() + m_fftSize,
                m_tcorr.begin() + nfft*m_fftSize,
                [](const std::complex<float>& a, const std::complex<float>& b) -> Sample {
                    Sample s;
                    std::complex<float> sum = a + b;
                    s.setReal(sum.real()/12.0f);
                    s.setImag(sum.imag()/12.0f);
                    return s;
                }
            );
        }
        else
        {
            std::transform(
                m_invFFT->out(),
                m_invFFT->out() + m_fftSize,
                m_tcorr.begin() + nfft*m_fftSize,
                [](const std::complex<float>& a) -> Sample {
                    Sample s;
                    s.setReal(a.real()/6.0f);
                    s.setImag(a.imag()/6.0f);
                    return s;
                }
            );
        }

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

bool InterferometerCorrelator::performIFFT2Corr(
    const SampleVector& data0,
    unsigned int size0,
    const SampleVector& data1,
    unsigned int size1
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
            s2c
        );
        m_window.apply(m_fft2[0]->in());
        m_fft2[0]->transform();

        // FFT[1]
        std::transform(
            begin1,
            begin1 + m_fftSize,
            m_fft2[1]->in(),
            s2c
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
            m_invFFT2->out() + m_fftSize/2,
            m_invFFT2->out() + m_fftSize,
            m_tcorr.begin() + nfft*m_fftSize,
            [](const std::complex<float>& a) -> Sample {
                Sample s;
                s.setReal(a.real()/3.0f);
                s.setImag(a.imag()/3.0f);
                return s;
            }
        );
        std::transform(
            m_invFFT2->out(),
            m_invFFT2->out() + m_fftSize/2,
            m_tcorr.begin() + nfft*m_fftSize + m_fftSize/2,
            [](const std::complex<float>& a) -> Sample {
                Sample s;
                s.setReal(a.real()/3.0f);
                s.setImag(a.imag()/3.0f);
                return s;
            }
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

bool InterferometerCorrelator::performFFTProd(
    const SampleVector& data0,
    unsigned int size0,
    const SampleVector& data1,
    unsigned int size1
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
            s2cNorm
        );
        m_window.apply(m_fft2[0]->in());
        m_fft2[0]->transform();

        // FFT[1]
        std::transform(
            begin1,
            begin1 + m_fftSize,
            m_fft2[1]->in(),
            s2cNorm
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

        // product of FFT[1]* with FFT[0] and store in both results
        std::transform(
            m_fft2[0]->out(),
            m_fft2[0]->out() + m_fftSize,
            m_dataj,
            m_invFFT2->in(),
            [this](std::complex<float>& a, const std::complex<float>& b) -> std::complex<float> {
                return (a*b);
            }
        );

        // copy product to time domain - re-order, convert and scale to FFT size
        std::transform(
            m_invFFT2->in(),
            m_invFFT2->in() + m_fftSize/2,
            m_tcorr.begin() + nfft*m_fftSize + m_fftSize/2,
            [](const std::complex<float>& a) -> Sample {
                Sample s;
                s.setReal(a.real()/2.0f);
                s.setImag(a.imag()/2.0f);
                return s;
            }
        );
        std::transform(
            m_invFFT2->in() + m_fftSize/2,
            m_invFFT2->in() + m_fftSize,
            m_tcorr.begin() + nfft*m_fftSize,
            [](const std::complex<float>& a) -> Sample {
                Sample s;
                s.setReal(a.real()/2.0f);
                s.setImag(a.imag()/2.0f);
                return s;
            }
        );

        // feed spectrum with the sum
        std::transform(
            begin0,
            begin0 + m_fftSize,
            begin1,
            m_scorr.begin() + nfft*m_fftSize,
            sAdd
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
