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

std::complex<float> fcAdd(std::complex<float>& a, const std::complex<float>& b) {
    return a + b;
}

std::complex<float> fcMul(std::complex<float>& a, const std::complex<float>& b) {
    return a * b;
}

Sample cf2sAdd(std::complex<float>& a, const std::complex<float>& b)
{
    std::complex<float> c = a + b;
    return Sample{c.real(), c.imag()};
}

Sample cf2sMul(std::complex<float>& a, const std::complex<float>& b)
{
    std::complex<float> c = a * b;
    return Sample{c.real(), c.imag()};
}

Sample cf2s(std::complex<float>& a)
{
    return Sample{a.real(), a.imag()};
}

const unsigned int InterferometerCorrelator::m_nbFFTBlocks = 128;

InterferometerCorrelator::InterferometerCorrelator(int fftSize) :
    m_corrType(InterferometerSettings::CorrelationAdd),
    m_fftSize(fftSize)
{
    for (int i = 0; i < 2; i++)
    {
        m_fft[i] = FFTEngine::create();
        m_fft[i]->configure(2*fftSize, false); // internally twice the data FFT size
        m_data[i] = new std::complex<float>[fftSize*m_nbFFTBlocks];
        m_dataIndex[i] = 0;
    }

    m_invFFT = FFTEngine::create();
    m_invFFT->configure(2*fftSize, true);

    m_dataj = new std::complex<float>[2*fftSize]; // receives actual FFT result hence twice the data FFT size
    m_scorr.resize(2*fftSize*m_nbFFTBlocks);      // same size multiplied by the number of buffered FFT blocks
    m_tcorr.resize(2*fftSize*m_nbFFTBlocks);      // same size multiplied by the number of buffered FFT blocks
}

InterferometerCorrelator::~InterferometerCorrelator()
{
    for (int i = 0; i < 2; i++)
    {
        delete[] m_data[i];
        delete[] m_fft[i];
    }

    delete[] m_dataj;
}

void InterferometerCorrelator::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, unsigned int argIndex)
{
    if (argIndex > 1) {
        return;
    }

    switch (m_corrType)
    {
        case InterferometerSettings::CorrelationAdd:
            feedOp(begin, end, argIndex, cf2sAdd);
            break;
        case InterferometerSettings::CorrelationMultiply:
            feedOp(begin, end, argIndex, cf2sMul);
            break;
        case InterferometerSettings::CorrelationCorrelation:
            feedCorr(begin, end, argIndex);
            break;
        default:
            break;
    }
}

void InterferometerCorrelator::feedOp(
        const SampleVector::const_iterator& begin,
        const SampleVector::const_iterator& end,
        unsigned int argIndex,
        Sample complexOp(std::complex<float>& a, const std::complex<float>& b)
)
{
    int size = (end - begin);
    int fill = m_fftSize*m_nbFFTBlocks - m_dataIndex[argIndex];
    int first = std::min(fill, size);

    std::transform(begin, begin + first, m_data[argIndex] + m_dataIndex[argIndex], [](const Sample& s) -> std::complex<float> {
        return std::complex<float>{s.real() / SDR_RX_SCALEF, s.imag() / SDR_RX_SCALEF};
    });

    if (argIndex == 1)
    {
        std::transform(
            m_data[0] + m_dataIndex[0], m_data[0] + m_dataIndex[0] + first, m_data[1] + m_dataIndex[1],
            m_tcorr.begin() + m_dataIndex[0],
            complexOp
        );

        emit dataReady(m_dataIndex[0], m_dataIndex[0] + first);
    }

    if (size > fill)
    {
        std::transform(begin, begin + size - fill , m_data[argIndex], [](const Sample& s) -> std::complex<float> {
            return std::complex<float>{s.real() / SDR_RX_SCALEF, s.imag() / SDR_RX_SCALEF};
        });

        if (argIndex == 1)
        {
            std::transform(
                m_data[0], m_data[0] + size - fill, m_data[1],
                m_tcorr.begin(),
                complexOp
            );

            emit dataReady(0, size - fill);
        }

        m_dataIndex[argIndex] = size - fill;
    }
    else
    {
        m_dataIndex[argIndex] += size;
    }
}

void InterferometerCorrelator::feedCorr(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, unsigned int argIndex)
{
    int size = (end - begin);
    int fill = m_fftSize*m_nbFFTBlocks - m_dataIndex[argIndex];
    int first = std::min(fill, size);

    std::transform(begin, begin + first, m_data[argIndex] + m_dataIndex[argIndex], [](const Sample& s) -> std::complex<float> {
        return std::complex<float>{s.real() / SDR_RX_SCALEF, s.imag() / SDR_RX_SCALEF};
    });
    processFFTBlocks(argIndex, m_dataIndex[argIndex], first);

    if (size > fill)
    {
        std::transform(begin, begin + size - fill, m_data[argIndex], [](const Sample& s) -> std::complex<float> {
            return std::complex<float>{s.real() / SDR_RX_SCALEF, s.imag() / SDR_RX_SCALEF};
        });
        processFFTBlocks(argIndex, 0, size - fill);
        m_dataIndex[argIndex] = size - fill;
    }
    else
    {
        m_dataIndex[argIndex] += size;
    }
}

void InterferometerCorrelator::processFFTBlocks(unsigned int argIndex, unsigned int dataIndex, int length)
{
    int start = dataIndex / m_fftSize;
    int stop = (dataIndex + length) / m_fftSize;

    for (int i = start; i < stop; i++)
    {
        m_window.apply(&m_data[argIndex][start*m_fftSize], m_fft[argIndex]->in());
        std::fill(m_fft[argIndex]->in() + m_fftSize, m_fft[argIndex]->in() + 2*m_fftSize, std::complex<float>{0,0});
        m_fft[argIndex]->transform();

        if (argIndex == m_corrIndex)
        {
            // conjugate
            std::transform(m_fft[argIndex]->out(), m_fft[argIndex]->out()+2*m_fftSize, m_dataj, []
                (const std::complex<float>& c) -> std::complex<float> { return std::conj(c); }
            );
            // product with FFT[0] store in inverse FFT input
            std::transform(m_fft[0]->out(), m_fft[0]->out()+2*m_fftSize, m_dataj, m_invFFT->in(), []
                (std::complex<float>& a, const std::complex<float>& b) -> std::complex<float> { return a*b; }
            );
            // copy to correlation spectrum and do the inverse FFT to get time correlation
            std::transform(m_invFFT->in(), m_invFFT->in() + 2*m_fftSize, m_scorr.begin() + 2*i*m_fftSize, cf2s);
            m_invFFT->transform();
            std::transform(m_invFFT->out(), m_invFFT->out() + 2*m_fftSize, m_tcorr.begin() + 2*i*m_fftSize, cf2s);
        }
    }

    if (start != stop) {
        emit dataReady(start, stop);
    }
}