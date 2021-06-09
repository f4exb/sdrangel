///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include <QDebug>

#include <complex.h>

#include "dsp/dspengine.h"

#include "noisefigure.h"
#include "noisefiguresink.h"

NoiseFigureSink::NoiseFigureSink(NoiseFigure *noiseFigure) :
        m_noiseFigure(noiseFigure),
        m_channelSampleRate(48000),
        m_fftSequence(-1),
        m_fft(nullptr),
        m_fftCounter(0),
        m_magsqSum(0.0f),
        m_magsqPeak(0.0f),
        m_magsqCount(0),
        m_messageQueueToChannel(nullptr),
        m_powerSum(0.0),
        m_count(0),
        m_enabled(false)
{
    m_magsq = 0.0;

    applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, 0, true);
}

NoiseFigureSink::~NoiseFigureSink()
{
}

void NoiseFigureSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    for (SampleVector::const_iterator it = begin; it != end; ++it)
    {
        Complex c(it->real(), it->imag());
        processOneSample(c);
    }
}

void NoiseFigureSink::processOneSample(Complex &ci)
{
    // Add to FFT input buffer
    m_fft->in()[m_fftCounter] = Complex(ci.real() / SDR_RX_SCALEF, ci.imag() / SDR_RX_SCALEF);
    m_fftCounter++;
    if (m_fftCounter == m_settings.m_fftSize)
    {
        // Calculate FFT (note no windowing as input should be broadband noise)
        m_fft->transform();
        m_fftCounter = 0;

        // Calculate power in FFT bin selected by input frequency offset
        double frequencyResolution = m_channelSampleRate / (double)m_settings.m_fftSize;
        int bin;
        if (m_settings.m_inputFrequencyOffset >= 0) {
            bin = m_settings.m_inputFrequencyOffset / frequencyResolution;
        } else {
            bin = m_settings.m_fftSize + m_settings.m_inputFrequencyOffset / frequencyResolution;
        }
        Complex c = m_fft->out()[bin];
        Real v = c.real() * c.real() + c.imag() * c.imag();

        // Calculate average and peak levels for level meter
        Real magsq = v / (m_settings.m_fftSize*m_settings.m_fftSize);
        m_movingAverage(magsq);
        m_magsq = m_movingAverage.asDouble();
        m_magsqSum += magsq;
        if (magsq > m_magsqPeak)
        {
            m_magsqPeak = magsq;
        }
        m_magsqCount++;

        if (m_enabled)
        {
            // Average power for measurement
            m_powerSum += v;
            m_count++;
            if (m_count == m_settings.m_fftCount)
            {
                // Convert average to dB
                // This is 10*log10(p/(1/fftSize)^2) optimised to not use log10 in the loop
                const Real mult = (10.0f / log2(10.0f));   // ~3.01
                Real ofs = 20.0f * log10f(1.0f / m_settings.m_fftSize);
                Real avg = mult * log2f(m_powerSum / m_count) + ofs;

                // Send NF results to channel
                if (getMessageQueueToChannel())
                {
                    NoiseFigure::MsgPowerMeasurement *msg = NoiseFigure::MsgPowerMeasurement::create(avg);
                    getMessageQueueToChannel()->push(msg);
                }

                // Prepare for a new measurement
                m_powerSum = 0.0;
                m_count = 0;
                m_enabled = false;
            }
        }
    }
}

void NoiseFigureSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    (void) force;
    qDebug() << "NoiseFigureSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    m_channelSampleRate = channelSampleRate;
}

void NoiseFigureSink::applySettings(const NoiseFigureSettings& settings, bool force)
{
    qDebug() << "NoiseFigureSink::applySettings:"
            << " force: " << force;

    if ((settings.m_fftSize != m_settings.m_fftSize) || force)
    {
        FFTFactory *fftFactory = DSPEngine::instance()->getFFTFactory();
        if (m_fftSequence >= 0) {
            fftFactory->releaseEngine(m_settings.m_fftSize, false, m_fftSequence);
        }
        m_fftSequence = fftFactory->getEngine(settings.m_fftSize, false, &m_fft);
        m_fftCounter = 0;
   }

    if ((settings.m_fftCount != m_settings.m_fftCount) || force)
    {
        m_powerSum = 0.0;
        m_count = 0;
    }

    m_settings = settings;
}
