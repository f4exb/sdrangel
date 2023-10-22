///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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
#include "util/db.h"

#include "freqscanner.h"
#include "freqscannersink.h"

FreqScannerSink::FreqScannerSink(FreqScanner *ilsDemod) :
        m_freqScanner(ilsDemod),
        m_channel(nullptr),
        m_channelSampleRate(48000),
        m_channelFrequencyOffset(0),
        m_scannerSampleRate(33320),
        m_centerFrequency(0),
        m_messageQueueToChannel(nullptr),
        m_fftSequence(-1),
        m_fft(nullptr),
        m_fftCounter(0),
        m_fftSize(1024),
        m_binsPerChannel(16),
        m_averageCount(0)
{
    applySettings(m_settings, QStringList(), true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, 16, 4, true);
}

FreqScannerSink::~FreqScannerSink()
{
}

void FreqScannerSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
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
        else // decimate (and filter)
        {
            if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
    }
}

void FreqScannerSink::processOneSample(Complex &ci)
{
    ci /= SDR_RX_SCALEF;

    m_fft->in()[m_fftCounter] = ci;
    m_fftCounter++;
    if (m_fftCounter == m_fftSize)
    {
        // Apply windowing function
        m_fftWindow.apply(m_fft->in());

        // Perform FFT
        m_fft->transform();

        // Reorder (so negative frequencies are first) and average
        int halfSize = m_fftSize / 2;
        for (int i = 0; i < halfSize; i++) {
            m_fftAverage.storeAndGetAvg(m_magSq[i], magSq(i + halfSize), i);
        }
        for (int i = 0; i < halfSize; i++) {
            m_fftAverage.storeAndGetAvg(m_magSq[i + halfSize], magSq(i), i + halfSize);
        }

        if (m_fftAverage.nextAverage())
        {
            // Send results to channel
            if (getMessageQueueToChannel() && (m_settings.m_channelBandwidth != 0) && (m_binsPerChannel != 0))
            {
                FreqScanner::MsgScanResult* msg = FreqScanner::MsgScanResult::create(m_fftStartTime);
                QList<FreqScanner::MsgScanResult::ScanResult>& results = msg->getScanResults();

                for (int i = 0; i < m_settings.m_frequencies.size(); i++)
                {
                    if (m_settings.m_enabled[i])
                    {
                        qint64 frequency = m_settings.m_frequencies[i];
                        qint64 startFrequency = m_centerFrequency - m_scannerSampleRate / 2;
                        qint64 diff = frequency - startFrequency;
                        float binBW = m_scannerSampleRate / (float)m_fftSize;

                        // Ignore results in uppper and lower 12.5%, as there may be aliasing here from half-band filters
                        if ((diff < m_scannerSampleRate * 0.875f) && (diff >= m_scannerSampleRate * 0.125f))
                        {
                            int bin = std::round(diff / binBW);

                            // Calculate power at that frequency
                            Real power;
                            if (m_settings.m_measurement == FreqScannerSettings::PEAK) {
                                power = peakPower(bin);
                            } else {
                                power = totalPower(bin);
                            }
                            //qDebug() << "startFrequency:" << startFrequency << "m_scannerSampleRate:" << m_scannerSampleRate << "m_centerFrequency:" << m_centerFrequency << "frequency" << frequency << "bin" << bin << "power" << power;
                            FreqScanner::MsgScanResult::ScanResult result = {frequency, power};
                            results.append(result);
                        }
                    }
                }
                getMessageQueueToChannel()->push(msg);
            }
            m_averageCount = 0;
            m_fftStartTime = QDateTime::currentDateTime();
        }
        m_fftCounter = 0;
    }
}

// Calculate total power in a channel containing the specified bin (i.e. sums adjacent bins in the same channel)
Real FreqScannerSink::totalPower(int bin) const
{
    // Skip bin between halfway between channels
    // Then skip first and last bins, to avoid spectral leakage (particularly at DC)
    int startBin = bin - m_binsPerChannel / 2 + 1 + 1;
    Real magSqSum = 0.0f;
    for (int i = 0; i < m_binsPerChannel - 2 - 1; i++) {
        int idx = startBin + i;
        if ((idx < 0) || (idx >= m_fftSize)) {
            continue;
        }
        magSqSum += m_magSq[idx];
    }
    Real db = CalcDb::dbPower(magSqSum);
    return db;
}

// Calculate peak power in a channel containing the specified bin
Real FreqScannerSink::peakPower(int bin) const
{
    // Skip bin between halfway between channels
    // Then skip first and last bins, to avoid spectral leakage (particularly at DC)
    int startBin = bin - m_binsPerChannel/2 + 1 + 1;
    Real maxMagSq = std::numeric_limits<Real>::min();
    for (int i = 0; i < m_binsPerChannel - 2 - 1; i++)
    {
        int idx = startBin + i;
        if ((idx < 0) || (idx >= m_fftSize)) {
            continue;
        }
        //qDebug() << "idx:" << idx << "power:" << CalcDb::dbPower(m_magSq[idx]);
        maxMagSq = std::max(maxMagSq, m_magSq[idx]);
    }
    Real db = CalcDb::dbPower(maxMagSq);
    return db;
}

Real FreqScannerSink::magSq(int bin) const
{
    Complex c = m_fft->out()[bin];
    Real v = c.real() * c.real() + c.imag() * c.imag();
    Real magsq = v / (m_fftSize * m_fftSize);
    return magsq;
}

void FreqScannerSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, int scannerSampleRate, int fftSize, int binsPerChannel, bool force)
{
    qDebug() << "FreqScannerSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset
            << " scannerSampleRate: " << scannerSampleRate
            << " fftSize: " << fftSize
            << " binsPerChannel: " << binsPerChannel;

    if ((m_channelFrequencyOffset != channelFrequencyOffset) ||
        (m_channelSampleRate != channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((m_channelSampleRate != channelSampleRate) || (m_scannerSampleRate != scannerSampleRate) || force)
    {
        m_interpolator.create(16, channelSampleRate, scannerSampleRate / 2.2); // Filter potential aliasing resulting from half-band filters
        m_interpolatorDistance = (Real) channelSampleRate / (Real)scannerSampleRate;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    if ((m_fftSize != fftSize) || force)
    {
        FFTFactory* fftFactory = DSPEngine::instance()->getFFTFactory();
        if (m_fftSequence >= 0) {
            fftFactory->releaseEngine(fftSize, false, m_fftSequence);
        }
        m_fftSequence = fftFactory->getEngine(fftSize, false, &m_fft);
        m_fftCounter = 0;
        m_fftStartTime = QDateTime::currentDateTime();
        m_fftWindow.create(FFTWindow::Hanning, fftSize);

        int averages = m_settings.m_scanTime * scannerSampleRate / 2 / fftSize;
        m_fftAverage.resize(fftSize, averages);
        m_magSq.resize(fftSize);
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
    m_scannerSampleRate = scannerSampleRate;
    m_fftSize = fftSize;
    m_binsPerChannel = binsPerChannel;
}

void FreqScannerSink::applySettings(const FreqScannerSettings& settings, const QStringList& settingsKeys, bool force)
{
    qDebug() << "FreqScannerSink::applySettings:"
             << settings.getDebugString(settingsKeys, force)
             << " force: " << force;

    if (settingsKeys.contains("scanTime") || force)
    {
        int averages = settings.m_scanTime * m_scannerSampleRate / 2 / m_fftSize;
        m_fftAverage.resize(m_fftSize, averages);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}
