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

#include "dsp/dspengine.h"

#include "radioastronomy.h"
#include "radioastronomysink.h"

RadioAstronomySink::RadioAstronomySink(RadioAstronomy *aisDemod) :
    m_radioAstronomy(aisDemod),
    m_channelSampleRate(1000000),
    m_channelFrequencyOffset(0),
    m_fftSequence(-1),
    m_fft(nullptr),
    m_fftCounter(0),
    m_fftSum(nullptr),
    m_fftTemp(nullptr),
    m_fftSumCount(0),
    m_enabled(false),
    m_cal(false),
    m_magsqSum(0.0f),
    m_magsqPeak(0.0f),
    m_magsqCount(0),
    m_messageQueueToChannel(nullptr)
{
    m_magsq = 0.0;

    applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

RadioAstronomySink::~RadioAstronomySink()
{
    delete[] m_fftSum;
    delete[] m_fftTemp;
}

void RadioAstronomySink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    Complex ci;

    for (SampleVector::const_iterator it = begin; it != end; ++it)
    {
        Complex c(it->real(), it->imag());
        c *= m_nco.nextIQ();

        if (m_interpolatorDistance < 1.0f) // interpolate
        {
            while (!m_interpolator.interpolate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
        else // decimate
        {
            if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
    }
}

void RadioAstronomySink::processOneSample(Complex &ci)
{
    // Calculate power
    double magsqRaw = ci.real()*ci.real() + ci.imag()*ci.imag();
    double magsq = (magsqRaw / (SDR_RX_SCALED*SDR_RX_SCALED));

    // Calculate average and peak levels for level meter
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();
    m_magsqSum += magsq;
    if (magsq > m_magsqPeak)
    {
        m_magsqPeak = magsq;
    }
    m_magsqCount++;

    if (m_enabled || m_cal)
    {
        // Add to FFT input buffer
        m_fft->in()[m_fftCounter] = Complex(ci.real() / SDR_RX_SCALEF, ci.imag() / SDR_RX_SCALEF);
        m_fftCounter++;
        if (m_fftCounter >= m_settings.m_fftSize)
        {
            // Calculate FFT
            m_fftWindow.apply(m_fft->in());
            m_fft->transform();
            m_fftCounter = 0;

            // Calculate power and accumulate
            for (int i = 0; i < m_settings.m_fftSize; i++)
            {
                Complex s = m_fft->out()[i];
                Real v = s.real() * s.real() + s.imag() * s.imag();
                Real enbw = 1.0f;
                /*if (m_settings.m_fftWindow == RadioAstronomySettings::HAN && m_settings.m_fftCorrection == RadioAstronomySettings::POWER) {
                    enbw = 1.5; // FIXME: Small dependence on fftSize in Matlab
                }*/
                m_fftSum[i] += v / (enbw * m_settings.m_fftSize * m_settings.m_fftSize); // Why FFT size here and not Fs?
            }

            m_fftSumCount++;
            if (m_fftSumCount >= m_settings.m_integration)
            {
                // Average
                for (int i = 0; i < m_settings.m_fftSize; i++) {
                    m_fftSum[i] /= m_fftSumCount;
                }

                // Put negative frequencies first
                std::copy(m_fftSum + m_settings.m_fftSize/2, m_fftSum + m_settings.m_fftSize, m_fftTemp);
                std::copy(m_fftSum, m_fftSum + m_settings.m_fftSize/2, m_fftTemp + m_settings.m_fftSize/2);

                // Filter freqs with RFI
                if (m_filterBins.size() > 0)
                {
                    // Find minimum value to use as replacement
                    // Should possibly use an average of the n lowest values or something
                    float minVal = std::numeric_limits<float>::max();
                    for (int i = 0; i < m_settings.m_fftSize; i++) {
                         minVal = std::min(minVal, m_fftTemp[i]);
                    }
                    for (int i = 0; i < m_filterBins.size(); i++)
                    {
                        int bin = m_filterBins[i];
                        if (bin < m_settings.m_fftSize) {
                            m_fftTemp[bin] = minVal;
                        }
                    }
                }

                getMessageQueueToChannel()->push(RadioAstronomy::MsgMeasurementProgress::create(100));

                if (m_cal)
                {
                    // Indicate calibration complete
                    if (getMessageQueueToChannel())
                    {
                        RadioAstronomy::MsgCalComplete *msg = RadioAstronomy::MsgCalComplete::create(m_fftTemp, m_settings.m_fftSize, QDateTime::currentDateTime(), m_hot);
                        getMessageQueueToChannel()->push(msg);
                    }

                    // Cal complete
                    m_cal = false;
                }
                else
                {
                    // Send averaged FFT to channel
                    if (getMessageQueueToChannel())
                    {

                        RadioAstronomy::MsgFFTMeasurement *msg = RadioAstronomy::MsgFFTMeasurement::create(m_fftTemp, m_settings.m_fftSize, QDateTime::currentDateTime());
                        getMessageQueueToChannel()->push(msg);
                    }

                    m_enabled = (m_settings.m_runMode == RadioAstronomySettings::CONTINUOUS);
                    if (m_enabled) {
                        getMessageQueueToChannel()->push(RadioAstronomy::MsgMeasurementProgress::create(0));
                    }
                }

                m_fftSumCount = 0;
                std::fill(m_fftSum, m_fftSum + m_settings.m_fftSize, 0.0f);
            }
            else
            {
                // Don't send more than ~4 updates per second
                int fftsPerSecond = m_settings.m_sampleRate / m_settings.m_fftSize;
                if ((m_fftSumCount % (fftsPerSecond/4)) == 0) {
                    getMessageQueueToChannel()->push(RadioAstronomy::MsgMeasurementProgress::create(100 * m_fftSumCount / m_settings.m_integration));
                }
            }

        }
    }
}

void RadioAstronomySink::startMeasurements()
{
    getMessageQueueToChannel()->push(RadioAstronomy::MsgMeasurementProgress::create(0));
    m_enabled = true;
    m_fftSumCount = 0;
    std::fill(m_fftSum, m_fftSum + m_settings.m_fftSize, 0.0f);
}

void RadioAstronomySink::stopMeasurements()
{
    m_enabled = false;
}

void RadioAstronomySink::startCal(bool hot)
{
    getMessageQueueToChannel()->push(RadioAstronomy::MsgMeasurementProgress::create(0));
    m_cal = true;
    m_hot = hot;
    m_fftSumCount = 0;
    std::fill(m_fftSum, m_fftSum + m_settings.m_fftSize, 0.0f);
}

void RadioAstronomySink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "RadioAstronomySink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((m_channelFrequencyOffset != channelFrequencyOffset) ||
        (m_channelSampleRate != channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((m_channelSampleRate != channelSampleRate) || force)
    {
        m_interpolator.create(16, channelSampleRate, m_settings.m_rfBandwidth / 2.0f);
        m_interpolatorDistance = (Real) channelSampleRate / (Real) m_settings.m_sampleRate;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void RadioAstronomySink::applySettings(const RadioAstronomySettings& settings, bool force)
{
    qDebug() << "RadioAstronomySink::applySettings:"
            << " m_sampleRate: " << settings.m_sampleRate
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_fftSize: " << settings.m_fftSize
            << " m_fftWindow: " << settings.m_fftWindow
            << " m_filterFreqs: " << settings.m_filterFreqs
            << " force: " << force;

    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth)
        || (settings.m_sampleRate != m_settings.m_sampleRate)
        || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_rfBandwidth / 2.0f); // 2.0 rather than 2.2 as in other plugins, to reduce rolloff at edge of band
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) settings.m_sampleRate;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    if ((settings.m_fftSize != m_settings.m_fftSize) || force)
    {
        FFTFactory *fftFactory = DSPEngine::instance()->getFFTFactory();
        if (m_fftSequence >= 0) {
            fftFactory->releaseEngine(m_settings.m_fftSize, false, m_fftSequence);
        }
        m_fftSequence = fftFactory->getEngine(settings.m_fftSize, false, &m_fft);
        m_fftCounter = 0;
        delete[] m_fftSum;
        delete[] m_fftTemp;
        m_fftSum = new Real[settings.m_fftSize]();
        m_fftTemp = new Real[settings.m_fftSize]();
        m_fftSumCount = 0;
   }

    if ((settings.m_fftSize != m_settings.m_fftSize)
        || (settings.m_fftWindow != m_settings.m_fftWindow)
        || force)
    {
        if (settings.m_fftWindow == RadioAstronomySettings::HAN) {
            m_fftWindow.create(FFTWindow::Hanning, settings.m_fftSize);
        } else {
            m_fftWindow.create(FFTWindow::Rectangle, settings.m_fftSize);
        }
    }

    if ((settings.m_filterFreqs != m_settings.m_filterFreqs) || force)
    {
        m_filterBins.clear();
        QStringList filterFreqs = settings.m_filterFreqs.split(" ");
        for (int i = 0; i < filterFreqs.size(); i++)
        {
            bool ok;
            int bin = filterFreqs[i].toInt(&ok);
            if (ok) {
                m_filterBins.append(bin);
            }
        }
    }

    m_settings = settings;
}
