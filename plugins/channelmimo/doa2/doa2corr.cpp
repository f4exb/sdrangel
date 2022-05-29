///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#include "doa2corr.h"

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

Sample invfft2s(const std::complex<float>& a) { //!< Complex float to Sample for 1 side time correlation
    Sample s;
    s.setReal(a.real()/2.0f);
    s.setImag(a.imag()/2.0f);
    return s;
}

DOA2Correlator::DOA2Correlator(int fftSize) :
    m_corrType(DOA2Settings::CorrelationFFT),
    m_fftSize(fftSize)
{
    setPhase(0);
    FFTFactory *fftFactory = DSPEngine::instance()->getFFTFactory();
    m_window.create(FFTWindow::Function::Hanning, fftSize);

    for (int i = 0; i < 2; i++) {
        m_fftSequences[i] = fftFactory->getEngine(fftSize, false, &m_fft[i]);
    }

    m_invFFTSequence = fftFactory->getEngine(fftSize, true, &m_invFFT);

    m_dataj = new std::complex<float>[2*fftSize]; // receives actual FFT result hence twice the data FFT size
    m_tcorr.resize(fftSize);
    m_xcorr.resize(fftSize);
    m_tcorrSize = fftSize;
    m_xcorrSize = fftSize;
}

DOA2Correlator::~DOA2Correlator()
{
    FFTFactory *fftFactory = DSPEngine::instance()->getFFTFactory();
    fftFactory->releaseEngine(m_fftSize, true, m_invFFTSequence);
    delete[] m_dataj;

    for (int i = 0; i < 2; i++) {
        fftFactory->releaseEngine(m_fftSize, false, m_fftSequences[i]);
    }
}

bool DOA2Correlator::performCorr(
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
            case DOA2Settings::Correlation0:
                results = performOpCorr(data0, size0, data1, size1, sFirst);
                break;
            case DOA2Settings::Correlation1:
                results = performOpCorr(data0, size0, data1, size1, sSecond);
                break;
            case DOA2Settings::CorrelationFFT:
                results = performFFTProd(data0, size0, data1, size1);
                break;
            default:
                break;
        }
    }
    else if ((m_phase == -180) || (m_phase == 180))
    {
        if (m_corrType == DOA2Settings::CorrelationFFT)
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
            case DOA2Settings::Correlation0:
                results = performOpCorr(data0, size0, data1, size1, sFirst);
                break;
            case DOA2Settings::Correlation1:
                results = performOpCorr(data0, size0, data1, size1, sSecondInv);
                break;
            case DOA2Settings::CorrelationFFT:
                results = performFFTProd(data0, size0, m_data1p, size1);
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
            case DOA2Settings::Correlation0:
                results = performOpCorr(data0, size0, m_data1p, size1, sFirst);
                break;
            case DOA2Settings::Correlation1:
                results = performOpCorr(data0, size0, m_data1p, size1, sSecond);
                break;
            case DOA2Settings::CorrelationFFT:
                results = performFFTProd(data0, size0, m_data1p, size1);
                break;
            default:
                break;
        }
    }

    return results;
}

bool DOA2Correlator::performOpCorr(
    const SampleVector& data0,
    unsigned int size0,
    const SampleVector& data1,
    unsigned int size1,
    Sample sampleOp(const Sample& a, const Sample& b)
)
{
    unsigned int size = std::min(size0, size1);
    adjustTCorrSize(size);
    adjustXCorrSize(size);

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

bool DOA2Correlator::performFFTProd(
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
    adjustTCorrSize(size);
    adjustXCorrSize(size);

    while (size >= m_fftSize)
    {
        // FFT[0]
        std::transform(
            begin0,
            begin0 + m_fftSize,
            m_fft[0]->in(),
            s2cNorm
        );
        m_window.apply(m_fft[0]->in());
        m_fft[0]->transform();

        // FFT[1]
        std::transform(
            begin1,
            begin1 + m_fftSize,
            m_fft[1]->in(),
            s2cNorm
        );
        m_window.apply(m_fft[1]->in());
        m_fft[1]->transform();

        // conjugate FFT[1]
        std::transform(
            m_fft[1]->out(),
            m_fft[1]->out() + m_fftSize,
            m_dataj,
            [](const std::complex<float>& c) -> std::complex<float> {
                return std::conj(c);
            }
        );

        // product of FFT[1]* with FFT[0] and store in both results
        std::transform(
            m_fft[0]->out(),
            m_fft[0]->out() + m_fftSize,
            m_dataj,
            m_invFFT->in(),
            [this](std::complex<float>& a, const std::complex<float>& b) -> std::complex<float> {
                return (a*b);
            }
        );

        // copy to complex vector for DOA with re-orderong
        std::copy(
            m_invFFT->in(),
            m_invFFT->in() + m_fftSize/2,
            m_xcorr.begin() + nfft*m_fftSize + m_fftSize/2
        );
        std::copy(
            m_invFFT->in() + m_fftSize/2,
            m_invFFT->in() + m_fftSize,
            m_xcorr.begin() + nfft*m_fftSize
        );

        // convert and scale to FFT size for scope time domain display
        std::transform(
            m_xcorr.begin() + nfft*m_fftSize,
            m_xcorr.begin() + nfft*m_fftSize + m_fftSize,
            m_tcorr.begin() + nfft*m_fftSize,
            [](const std::complex<float>& a) -> Sample {
                Sample s;
                s.setReal(a.real()/2.0f);
                s.setImag(a.imag()/2.0f);
                return s;
            }
        );

        // copy product to time domain - re-order, convert and scale to FFT size
        // std::transform(
        //     m_invFFT->in(),
        //     m_invFFT->in() + m_fftSize/2,
        //     m_tcorr.begin() + nfft*m_fftSize + m_fftSize/2,
        //     [](const std::complex<float>& a) -> Sample {
        //         Sample s;
        //         s.setReal(a.real()/2.0f);
        //         s.setImag(a.imag()/2.0f);
        //         return s;
        //     }
        // );
        // std::transform(
        //     m_invFFT->in() + m_fftSize/2,
        //     m_invFFT->in() + m_fftSize,
        //     m_tcorr.begin() + nfft*m_fftSize,
        //     [](const std::complex<float>& a) -> Sample {
        //         Sample s;
        //         s.setReal(a.real()/2.0f);
        //         s.setImag(a.imag()/2.0f);
        //         return s;
        //     }
        // );

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

void DOA2Correlator::adjustTCorrSize(int size)
{
    int nFFTSize = (size/m_fftSize)*m_fftSize;

    if (nFFTSize > m_tcorrSize)
    {
        m_tcorr.resize(nFFTSize);
        m_tcorrSize = nFFTSize;
    }
}

void DOA2Correlator::adjustXCorrSize(int size)
{
    int nFFTSize = (size/m_fftSize)*m_fftSize;

    if (nFFTSize > m_xcorrSize)
    {
        m_xcorr.resize(nFFTSize);
        m_xcorrSize = nFFTSize;
    }
}

void DOA2Correlator::setPhase(int phase)
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
