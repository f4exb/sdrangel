///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE <jon@beniston.com>                     //
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
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "dsp/dspengine.h"
#include "dsp/fftfactory.h"
#include "util/db.h"

#include "freqscanner.h"
#include "freqscannersink.h"

FreqScannerSink::FreqScannerSink() :
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
        m_averageCount(0),
        m_cepstrumSequenceInverse(-1),
        m_cepstrumSequenceForward(-1),
        m_cepstrumFFTInverse(nullptr),
        m_cepstrumFFTForward(nullptr),
        m_cepstrumSize(0)
{
    applySettings(m_settings, QStringList(), true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, 16, 4, true);
}

FreqScannerSink::~FreqScannerSink()
{
    FFTFactory* fftFactory = DSPEngine::instance()->getFFTFactory();
    
    if (m_fftSequence >= 0) {
        fftFactory->releaseEngine(m_fftSize, false, m_fftSequence);
    }
    
    if (m_cepstrumSequenceInverse >= 0) {
        fftFactory->releaseEngine(m_cepstrumSize, true, m_cepstrumSequenceInverse);
    }
    
    if (m_cepstrumSequenceForward >= 0) {
        fftFactory->releaseEngine(m_cepstrumSize, false, m_cepstrumSequenceForward);
    }
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

        // Accumulate voice activity levels on individual FFT (before averaging)
        // This captures sharp formant structure better than averaged spectrum
        int freqCount = m_settings.m_frequencySettings.size();
        if (m_voiceLevelSum.size() != freqCount) {
            m_voiceLevelSum.resize(freqCount);
            m_voiceLevelCount.resize(freqCount);
            m_voiceLevelSum.fill(0.0);
            m_voiceLevelCount.fill(0);
        }

        for (int i = 0; i < freqCount; i++)
        {
            if (m_settings.m_frequencySettings[i].m_enabled)
            {
                qint64 frequency = m_settings.m_frequencySettings[i].m_frequency;
                qint64 startFrequency = m_centerFrequency - m_scannerSampleRate / 2;
                qint64 diff = frequency - startFrequency;
                float binBW = m_scannerSampleRate / (float)m_fftSize;

                // avoid spectrum edges where there may be aliasing from half-band filters
                if ((diff >= m_scannerSampleRate / 8) && (diff < m_scannerSampleRate * 7 / 8))
                {
                    int bin = std::round(diff / binBW);
                    int channelBins;
                    if (m_settings.m_frequencySettings[i].m_channelBandwidth.isEmpty()) {
                        channelBins = m_binsPerChannel;
                    } else {
                        int channelBW = m_settings.getChannelBandwidth(&m_settings.m_frequencySettings[i]);
                        channelBins = m_fftSize / (m_scannerSampleRate / (float)channelBW);
                    }

                    Real voiceLevel = 0.0;
                    if (m_settings.m_voiceSquelchType == FreqScannerSettings::VoiceLsb) {
                        voiceLevel = voiceActivityLevel(bin, channelBins, true);
                    } else if (m_settings.m_voiceSquelchType == FreqScannerSettings::VoiceUsb) {
                        voiceLevel = voiceActivityLevel(bin, channelBins, false);
                    }

                    if (voiceLevel > 0.0) {
                        m_voiceLevelSum[i] += voiceLevel;
                        m_voiceLevelCount[i]++;
                    }
                }
            }
        }

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

                for (int i = 0; i < m_settings.m_frequencySettings.size(); i++)
                {
                    if (m_settings.m_frequencySettings[i].m_enabled)
                    {
                        qint64 frequency = m_settings.m_frequencySettings[i].m_frequency;
                        qint64 startFrequency = m_centerFrequency - m_scannerSampleRate / 2;
                        qint64 diff = frequency - startFrequency;
                        float binBW = m_scannerSampleRate / (float)m_fftSize;

                        // Ignore results in upper and lower 12.5%, as there may be aliasing here from half-band filters
                        if ((diff >= m_scannerSampleRate / 8) && (diff < m_scannerSampleRate * 7 / 8))
                        {
                            int bin = std::round(diff / binBW); // Bin corresponding to the frequency
                            int channelBins; // Number of bins in the channel containing the frequency.
                                             // This is either the default (m_binsPerChannel)
                                             // or calculated based on the channel bandwidth if specified in settings for this frequency

                            if (m_settings.m_frequencySettings[i].m_channelBandwidth.isEmpty())
                            {
                                channelBins = m_binsPerChannel;
                            }
                            else
                            {
                                int channelBW = m_settings.getChannelBandwidth(&m_settings.m_frequencySettings[i]);
                                channelBins = m_fftSize / (m_scannerSampleRate / (float)channelBW);
                            }

                            // Calculate power at that frequency
                            Real power;
                            if (m_settings.m_measurement == FreqScannerSettings::PEAK) {
                                power = peakPower(bin, channelBins);
                            } else {
                                power = totalPower(bin, channelBins);
                            }

                            // Use averaged voice activity level from individual FFTs
                            Real voiceLevel = 0.0;
                            if ((m_settings.m_voiceSquelchType == FreqScannerSettings::VoiceLsb ||
                                 m_settings.m_voiceSquelchType == FreqScannerSettings::VoiceUsb) &&
                                m_voiceLevelCount[i] > 0) 
                            {
                                voiceLevel = m_voiceLevelSum[i] / m_voiceLevelCount[i];
                                if (voiceLevel > m_settings.m_voiceSquelchThreshold) {
                                    qDebug() << "FreqScannerSink::processOneSample: freq" 
                                        << frequency + (m_settings.m_voiceSquelchType == FreqScannerSettings::VoiceLsb ? 1500 : -1500)
                                        << "voiceLevel" << voiceLevel << "count" << m_voiceLevelCount[i];
                                }
                            }

                            FreqScanner::MsgScanResult::ScanResult result = {frequency, power, voiceLevel};
                            results.append(result);
                        }
                    }
                }
                getMessageQueueToChannel()->push(msg);
            }
            m_averageCount = 0;
            m_fftStartTime = QDateTime::currentDateTime();

            // Reset voice level accumulators for next averaging period
            m_voiceLevelSum.fill(0.0);
            m_voiceLevelCount.fill(0);
        }
        m_fftCounter = 0;
    }
}

// Calculate total power in a channel containing the specified bin (i.e. sums adjacent bins in the same channel)
Real FreqScannerSink::totalPower(int bin, int channelBins) const
{
    // Skip bin between halfway between channels
    // Then skip first and last bins, to avoid spectral leakage (particularly at DC)
    int startBin = bin - channelBins / 2 + 1 + 1;
    Real magSqSum = 0.0f;
    for (int i = 0; i < channelBins - 2 - 1; i++) {
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
Real FreqScannerSink::peakPower(int bin, int channelBins) const
{
    // Skip bin between halfway between channels
    // Then skip first and last bins, to avoid spectral leakage (particularly at DC)
    int startBin = bin - channelBins/2 + 1 + 1;
    Real maxMagSq = std::numeric_limits<Real>::min();
    for (int i = 0; i < channelBins - 2 - 1; i++)
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

// Compute magSq from raw FFT output with reordering (negative frequencies first)
Real FreqScannerSink::magSqFromRawFFT(int bin) const
{
    // m_magSq is reordered: negative freqs first, then positive
    // m_fft->out() is in standard FFT order: DC, positive freqs, negative freqs
    int halfSize = m_fftSize / 2;
    int fftBin;
    if (bin < halfSize) {
        // Negative frequencies: map to second half of FFT output
        fftBin = bin + halfSize;
    } else {
        // Positive frequencies: map to first half of FFT output
        fftBin = bin - halfSize;
    }
    return magSq(fftBin);
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
            fftFactory->releaseEngine(m_fftSize, false, m_fftSequence);
        }
        m_fftSequence = fftFactory->getEngine(fftSize, false, &m_fft);
        m_fftCounter = 0;
        m_fftStartTime = QDateTime::currentDateTime();
        m_fftWindow.create(FFTWindow::Hanning, fftSize);

        int averages = m_settings.m_scanTime * scannerSampleRate / 2 / fftSize;
        m_fftAverage.resize(fftSize, averages);
        m_magSq.resize(fftSize);

        // Resize voice level accumulators to match frequency count
        int freqCount = m_settings.m_frequencySettings.size();
        m_voiceLevelSum.resize(freqCount);
        m_voiceLevelCount.resize(freqCount);
        m_voiceLevelSum.fill(0.0);
        m_voiceLevelCount.fill(0);
        
        // Allocate cepstral FFT engines for formant detection
        // Size needs to be power-of-2 and >= channel bins (typically ~100-200 bins)
        // Use conservative size to handle most SSB channels
        int maxChannelBins = std::max(binsPerChannel * 2, 256);
        int cepstrumSize = 1;
        while (cepstrumSize < maxChannelBins) {
            cepstrumSize <<= 1;
        }
        
        if (m_cepstrumSize != cepstrumSize) {
            // Release old engines if size changed
            if (m_cepstrumSequenceInverse >= 0) {
                fftFactory->releaseEngine(m_cepstrumSize, true, m_cepstrumSequenceInverse);
            }
            if (m_cepstrumSequenceForward >= 0) {
                fftFactory->releaseEngine(m_cepstrumSize, false, m_cepstrumSequenceForward);
            }
            
            // Allocate new engines
            m_cepstrumSequenceInverse = fftFactory->getEngine(cepstrumSize, true, &m_cepstrumFFTInverse);
            m_cepstrumSequenceForward = fftFactory->getEngine(cepstrumSize, false, &m_cepstrumFFTForward);
            m_cepstrumSize = cepstrumSize;
            
            qDebug() << "FreqScannerSink::applyChannelSettings: Allocated cepstral FFT engines, size:" << cepstrumSize;
        }
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

// Voice activity detection for SSB signals
// Detects voice by looking for formant-like structure using spectral smoothing
// Returns a value from 0.0 (no voice) to 1.0 (strong voice signature)
Real FreqScannerSink::voiceActivityLevel(int bin, int channelBins, bool isLSB)
{
    // Voice band in SSB is typically 100-3000 Hz from carrier
    // We look for 2-4 formant peaks in the smoothed spectral envelope

    int startBin = bin - channelBins / 2 + 1;
    int endBin = startBin + channelBins - 1;

    if (startBin < 0 || endBin >= m_fftSize) {
        return 0.0;
    }

    // Calculate bin bandwidth in Hz
    float binBW = m_scannerSampleRate / (float)m_fftSize;

    // For LSB, spectrum is reversed - flip the search direction
    int carrierBin = isLSB ? endBin : startBin;

    // Get formant envelope using spectral smoothing
    // This separates vocal tract resonances from pitch harmonics
    QVector<Real> formantEnvelope;
    Real pitchHz = 0.0;
    getFormantEnvelope(startBin, endBin, formantEnvelope, &pitchHz);

    if (formantEnvelope.isEmpty()) {
        qDebug() << "FreqScannerSink::voiceActivityLevel formantEnvelope is empty!";
        return 0.0;
    }

    // Calculate noise floor from formant envelope
    Real noiseFloor = 0.0;
    Real maxEnv = 0.0;
    for (const Real env : formantEnvelope) {
        noiseFloor += env;
        maxEnv = std::max(maxEnv, env);
    }
    noiseFloor = formantEnvelope.size() > 0 ? noiseFloor / formantEnvelope.size() : 1e-12;
    
    // For voice, we need reasonably high peaks relative to noise
    // Use 4 dB (2.5x) above average as threshold for peak detection
    // Additional validation: peak-to-noise ratio
    // If max is not sufficiently higher than average, signal quality is poor
    float peakToNoiseRatio = maxEnv / noiseFloor;
    
    // Lower threshold: require at least 1.2x peak-to-noise ratio (only ~1.6dB)
    // After heavy smoothing, formants appear as gentle bumps, not sharp peaks
    if (peakToNoiseRatio < 1.2) {
        // Signal is essentially all noise
        return 0.0;
    }
    
    // For peak detection, use much lower threshold since smoothed spectral peaks are gentle
    // Use 1.1x noise floor to catch formant peaks in the smoothed envelope
    Real threshold = noiseFloor * 1.1;

    // Find formant peaks in the smoothed envelope
    QVector<int> formantBins;
    QVector<Real> formantMags;

    // Simple peak detection in formant envelope
    const qsizetype formantEnvelopeSize = formantEnvelope.size();
    for (qsizetype i = 1; i + 1 < formantEnvelopeSize; ++i)
    {
        Real prev = formantEnvelope[i - 1];
        Real curr = formantEnvelope[i];
        Real next = formantEnvelope[i + 1];

        // Local maximum above threshold
        if (curr > prev && curr > next && curr > threshold)
        {
            int absBin = startBin + static_cast<int>(i);
            // Use SIGNED offset to distinguish USB from LSB and reject mistuned signals
            // USB: formants at positive offset (100-3000 Hz above carrier)
            // LSB: formants at negative offset (-3000 to -100 Hz below carrier)
            float freqOffset = (absBin - carrierBin) * binBW;

            // Check appropriate sideband for voice: USB uses positive, LSB uses negative
            // This rejects signals tuned to wrong sideband (e.g., 1kHz offset)
            bool inVoiceBand = isLSB ? (freqOffset <= -100.0 && freqOffset >= -3000.0)
                                     : (freqOffset >= 100.0 && freqOffset <= 3000.0);
            
            if (inVoiceBand)
            {
                formantBins.append(absBin);
                formantMags.append(curr);
            }
        }
    }

    // Voice requires 2-4 formants
    if (formantBins.size() < 2) {
        return 0.0;
    }
    
    // Sort formants by frequency offset (not bin order) to handle LSB reversal
    // Convert to absolute frequencies since F1/F2 validation expects positive values
    QVector<float> formantFreqs;
    QVector<int> formantIndices;
    const qsizetype formantBinsSize = formantBins.size();
    
    for (qsizetype i = 0; i < formantBinsSize; ++i)
    {
        // Use absolute value for formant frequency analysis (F1, F2 ranges are defined as positive)
        float freqOffset = std::abs(formantBins[i] - carrierBin) * binBW;
        formantFreqs.append(freqOffset);
        formantIndices.append(static_cast<int>(i));
    }
    
    // Simple insertion sort by frequency
    const qsizetype formantFreqsSize = formantFreqs.size();
    for (qsizetype i = 1; i < formantFreqsSize; ++i)
    {
        for (qsizetype j = i; j > 0 && formantFreqs[j] < formantFreqs[j - 1]; --j)
        {
            std::swap(formantFreqs[j], formantFreqs[j - 1]);
            std::swap(formantIndices[j], formantIndices[j - 1]);
        }
    }
    
    // Merge peaks that are too close together (within 400 Hz)
    // Real formants have minimum separation of 400-500 Hz in SSB voice
    // So anything closer is ripple within a single formant
    // We keep the peak with highest magnitude and remove others
    QVector<float> mergedFormantFreqs;
    QVector<int> mergedFormantIndices;
    
    const float minFormantSpacing = 400.0; // Hz - minimum spacing between real formants
    const qsizetype sortedFormantFreqsSize = formantFreqs.size();
    
    for (qsizetype i = 0; i < sortedFormantFreqsSize; ++i)
    {
        if (i == 0 || formantFreqs[i] - mergedFormantFreqs.back() >= minFormantSpacing)
        {
            // This is a new formant (far enough from previous)
            mergedFormantFreqs.append(formantFreqs[i]);
            mergedFormantIndices.append(formantIndices[i]);
        }
        else
        {
            // This peak is too close to the previous one - merge by keeping highest magnitude
            int prevIdx = mergedFormantIndices.back();
            int currIdx = formantIndices[i];
            if (formantMags[currIdx] > formantMags[prevIdx])
            {
                // Replace with higher magnitude peak
                mergedFormantFreqs.back() = formantFreqs[i];
                mergedFormantIndices.back() = currIdx;
            }
        }
    }
    
    formantFreqs = mergedFormantFreqs;
    formantIndices = mergedFormantIndices;
    
    // After merging, we should still have at least 2 formants for voice
    if (formantFreqs.size() < 2) {
        return 0.0;
    }

    // Check formant spacing (voice formants should be 400-1500 Hz apart)
    // F1-F2 spacing is typically 600-1200 Hz
    bool goodSpacing = false;
    const qsizetype mergedFormantFreqsSize = formantFreqs.size();
    for (qsizetype i = 0; i + 1 < mergedFormantFreqsSize; ++i)
    {
        float spacing = formantFreqs[i + 1] - formantFreqs[i];
        if (spacing >= 400.0 && spacing <= 1500.0) {
            goodSpacing = true;
            break;
        }
    }

    if (!goodSpacing) {
        return 0.0; // Formants too close or too far apart
    }

    // Check for F1 formant in expected range (300-1000 Hz from carrier)
    // This is critical for voice detection
    // F1 should be the lowest frequency formant
    bool hasF1 = false;
    Real f1Mag = 0.0;
    int f1Idx = -1;
    
    if (formantFreqs.size() > 0 && formantFreqs[0] >= 300.0 && formantFreqs[0] <= 1000.0)
    {
        hasF1 = true;
        f1Idx = formantIndices[0];
        f1Mag = formantMags[f1Idx];
    }

    if (!hasF1) {
        return 0.0; // No F1 formant - not voice or mistuned
    }

    // Check for F2 formant in expected range (900-2500 Hz from carrier)
    // F2 should be higher frequency than F1 AND 400-1500 Hz away from F1
    bool hasF2 = false;
    Real f2Mag = 0.0;
    int f2Idx = -1;
    float f2Freq = 0.0;
    const qsizetype validatedFormantFreqsSize = formantFreqs.size();
    
    for (qsizetype i = 1; i < validatedFormantFreqsSize; ++i)
    {
        float freqOffset = formantFreqs[i];
        float f1ToF2Spacing = freqOffset - formantFreqs[0];
        
        // F2 must be: in range, higher than F1, and properly spaced from F1
        if (freqOffset >= 900.0 && freqOffset <= 2500.0 && 
            f1ToF2Spacing >= 400.0 && f1ToF2Spacing <= 1500.0)
        {
            hasF2 = true;
            f2Idx = formantIndices[i];
            f2Freq = freqOffset;
            f2Mag = formantMags[f2Idx];
            break; // Take the first valid F2
        }
    }

    if (!hasF2) {
        return 0.0; // No F2 formant - not voice or mistuned
    }

    // Reject strongly tonal spectra (typical CW / single-tone signals).
    // Voice in SSB should have broader spectral spread, lower peak dominance,
    // and non-negligible spectral flatness across the 100-3000 Hz voice band.
    Real voiceBandTotalEnergy = 0.0;
    Real voiceBandPeakEnergy = 0.0;
    Real voiceBandSecondPeakEnergy = 0.0;
    Real voiceBandLogEnergySum = 0.0;
    Real voiceBandWeightedFreqSum = 0.0;
    Real voiceBandWeightedFreqSqSum = 0.0;
    QVector<Real> voiceBandEnergies;
    voiceBandEnergies.reserve(std::max(0, endBin - startBin + 1));
    int voiceBandBins = 0;

    for (int absBin = startBin; absBin <= endBin; absBin++)
    {
        float signedOffset = (absBin - carrierBin) * binBW;
        float voiceOffset = isLSB ? -signedOffset : signedOffset;

        if (voiceOffset < 100.0f || voiceOffset > 3000.0f) {
            continue;
        }

        Real binEnergy = magSqFromRawFFT(absBin);
        Real safeEnergy = std::max(binEnergy, (Real) 1e-20);
        voiceBandEnergies.append(safeEnergy);

        voiceBandTotalEnergy += safeEnergy;

        if (safeEnergy > voiceBandPeakEnergy)
        {
            voiceBandSecondPeakEnergy = voiceBandPeakEnergy;
            voiceBandPeakEnergy = safeEnergy;
        }
        else if (safeEnergy > voiceBandSecondPeakEnergy)
        {
            voiceBandSecondPeakEnergy = safeEnergy;
        }

        voiceBandLogEnergySum += std::log(safeEnergy);
        voiceBandWeightedFreqSum += safeEnergy * voiceOffset;
        voiceBandWeightedFreqSqSum += safeEnergy * voiceOffset * voiceOffset;
        voiceBandBins++;
    }

    if (voiceBandBins <= 0 || voiceBandTotalEnergy <= 0.0) {
        return 0.0;
    }

    Real voiceBandMeanEnergy = voiceBandTotalEnergy / voiceBandBins;
    Real voiceBandGeometricMean = std::exp(voiceBandLogEnergySum / voiceBandBins);
    Real voiceBandMeanFreq = voiceBandWeightedFreqSum / voiceBandTotalEnergy;
    Real voiceBandMeanFreqSq = voiceBandWeightedFreqSqSum / voiceBandTotalEnergy;
    Real voiceBandVariance = std::max((Real) 0.0, voiceBandMeanFreqSq - voiceBandMeanFreq * voiceBandMeanFreq);
    float voiceBandRmsSpreadHz = std::sqrt(voiceBandVariance);
    float spectralFlatness = voiceBandMeanEnergy > 0.0 ? (voiceBandGeometricMean / voiceBandMeanEnergy) : 0.0f;
    float peakFraction = voiceBandTotalEnergy > 0.0 ? (voiceBandPeakEnergy / voiceBandTotalEnergy) : 1.0f;
    float peak12Ratio = voiceBandSecondPeakEnergy > 0.0 ? (voiceBandPeakEnergy / voiceBandSecondPeakEnergy) : 1000.0f;

    int significantBins = 0;
    Real significantThreshold = voiceBandMeanEnergy * 1.8;

    for (const Real& binEnergy : voiceBandEnergies)
    {
        if (binEnergy > significantThreshold) {
            significantBins++;
        }
    }

    if ((peakFraction > 0.45f) || (spectralFlatness < 0.025f) || (significantBins < 5)) {
        return 0.0;
    }

    // Additional strong-CW rejection:
    // - very narrow energy spread in voice band
    // - one dominant spectral line overwhelmingly larger than the second line
    if ((voiceBandRmsSpreadHz < 220.0f) || ((peak12Ratio > 8.0f) && (significantBins < 9))) {
        return 0.0;
    }

    float tonalPenalty = 1.0f;
    if (peakFraction > 0.30f) {
        tonalPenalty *= 0.6f;
    }
    if (spectralFlatness < 0.06f) {
        tonalPenalty *= 0.7f;
    }
    if (voiceBandRmsSpreadHz < 350.0f) {
        tonalPenalty *= 0.55f;
    }
    if (peak12Ratio > 4.0f) {
        tonalPenalty *= 0.60f;
    }
    // CW rejection end

    // Additional F1 plausibility checks.
    // Prevent a tiny low-frequency ripple from being accepted as F1 when
    // dominant formant energy is shifted high (e.g. around 2 kHz).
    Real strongestFormantMag = 0.0;
    float strongestFormantFreq = 0.0f;

    for (int i = 0; i < formantFreqs.size(); i++)
    {
        int idx = formantIndices[i];
        if (formantMags[idx] > strongestFormantMag)
        {
            strongestFormantMag = formantMags[idx];
            strongestFormantFreq = formantFreqs[i];
        }
    }

    const float minF1ToStrongestRatio = 0.35f;
    const float dominantHighFormantHz = 1200.0f;
    bool weakF1 = f1Mag < strongestFormantMag * minF1ToStrongestRatio;
    bool dominantIsHigh = strongestFormantFreq >= dominantHighFormantHz;

    if (weakF1 && dominantIsHigh) {
        return 0.0;
    }

    // F1 must have a minimum contrast above noise floor.
    if (f1Mag < noiseFloor * 1.25f) {
        return 0.0;
    }
    // Additional F1 plausibility checks - end

    // Calculate voice activity score based on formant characteristics
    // Voice is indicated by presence of F1 and F2 formants - this is the primary voice signature
    // Harmonics are less reliable in SSB due to spectral properties and noise
    float score = 0.0;

    // Base score from number of formants (2-4 formants typical for voice)
    // Formants are the most reliable voice indicator
    // Use merged formant count, not raw peak count
    float formantScore = std::min(formantFreqs.size() / 3.0f, 1.0f);
    score += formantScore * 0.6; // 60% weight

    // Score from formant magnitude (strong formants = strong voice)
    // Higher magnitudes indicate clearer voice detection
    // Make this threshold appropriately high to prefer strong signals
    float magnitudeScore = std::min((f1Mag + f2Mag) / (noiseFloor * 6.0f), 1.0f);
    score += magnitudeScore * 0.4; // 40% weight

    // Apply a soft pitch-based weighting. Pitch helps detect detuning, but should not gate voice.
    const float minPitchHz = 70.0f;
    const float maxPitchHz = 300.0f;
    const float softMinPitchHz = 50.0f;
    const float softMaxPitchHz = 400.0f;
    float pitchScore = 0.7f;
    float harmonicAlignmentScore = 0.75f;
    float formantIndexScore = 0.8f;

    if (pitchHz > 0.0f) {
        if (pitchHz >= minPitchHz && pitchHz <= maxPitchHz) {
            pitchScore = 1.0f;
        } else if (pitchHz >= softMinPitchHz && pitchHz <= softMaxPitchHz) {
            if (pitchHz < minPitchHz) {
                pitchScore = 0.7f + 0.3f * (pitchHz - softMinPitchHz) / (minPitchHz - softMinPitchHz);
            } else {
                pitchScore = 0.7f + 0.3f * (softMaxPitchHz - pitchHz) / (softMaxPitchHz - maxPitchHz);
            }
        } else {
            pitchScore = 0.6f;
        }

        // Check harmonic-comb alignment against the estimated pitch.
        // A wrong carrier offset shifts all harmonics by a constant frequency,
        // so they no longer align with integer multiples of pitch.
        const float harmonicToleranceHz = std::max(2.0f * binBW, 0.18f * pitchHz);
        Real rawNoiseFloor = 0.0;
        int rawBinCount = 0;

        for (int absBin = startBin; absBin <= endBin; absBin++)
        {
            float signedOffset = (absBin - carrierBin) * binBW;
            float voiceOffset = isLSB ? -signedOffset : signedOffset;

            if (voiceOffset >= 100.0f && voiceOffset <= 3000.0f)
            {
                rawNoiseFloor += magSqFromRawFFT(absBin);
                rawBinCount++;
            }
        }

        rawNoiseFloor = rawBinCount > 0 ? rawNoiseFloor / rawBinCount : 0.0;

        Real alignedEnergy = 0.0;
        Real totalEnergy = 0.0;

        for (int absBin = startBin; absBin <= endBin; absBin++)
        {
            float signedOffset = (absBin - carrierBin) * binBW;
            float voiceOffset = isLSB ? -signedOffset : signedOffset;

            if (voiceOffset < 100.0f || voiceOffset > 3000.0f) {
                continue;
            }

            Real binEnergy = magSqFromRawFFT(absBin);

            if (binEnergy <= rawNoiseFloor * 1.2f) {
                continue;
            }

            float residue = std::fmod(voiceOffset, pitchHz);
            if (residue < 0.0f) {
                residue += pitchHz;
            }

            float harmonicDistance = std::min(residue, pitchHz - residue);
            Real weightedEnergy = std::max(binEnergy - rawNoiseFloor, (Real) 0.0);

            totalEnergy += weightedEnergy;

            if (harmonicDistance <= harmonicToleranceHz) {
                alignedEnergy += weightedEnergy;
            }
        }

        if (totalEnergy > 0.0)
        {
            float harmonicAlignment = alignedEnergy / totalEnergy;
            float normalizedAlignment = (harmonicAlignment - 0.20f) / 0.45f;
            normalizedAlignment = std::max(0.0f, std::min(normalizedAlignment, 1.0f));
            harmonicAlignmentScore = 0.5f + 0.5f * normalizedAlignment;
        }

        // Check formant harmonic-index plausibility.
        // Wrong carrier tuning that shifts spectrum down can make F2/F3 appear as F1/F2.
        // In that case the implied harmonic indices become unusually high.
        float f1HarmonicIndex = formantFreqs[0] / pitchHz;
        float f2HarmonicIndex = f2Freq / pitchHz;
        float harmonicGap = f2HarmonicIndex - f1HarmonicIndex;

        float f1IndexScore = 1.0f;
        if (f1HarmonicIndex < 2.0f) {
            f1IndexScore = std::max(0.0f, (f1HarmonicIndex - 1.0f) / 1.0f);
        } else if (f1HarmonicIndex > 18.0f) {
            f1IndexScore = std::max(0.0f, (26.0f - f1HarmonicIndex) / 8.0f);
        }

        float f2IndexScore = 1.0f;
        if (f2HarmonicIndex < 5.0f) {
            f2IndexScore = std::max(0.0f, (f2HarmonicIndex - 3.0f) / 2.0f);
        } else if (f2HarmonicIndex > 36.0f) {
            f2IndexScore = std::max(0.0f, (44.0f - f2HarmonicIndex) / 8.0f);
        }

        float gapScore = 1.0f;
        if (harmonicGap < 3.0f) {
            gapScore = std::max(0.0f, (harmonicGap - 1.0f) / 2.0f);
        } else if (harmonicGap > 22.0f) {
            gapScore = std::max(0.0f, (28.0f - harmonicGap) / 6.0f);
        }

        formantIndexScore = std::max(0.4f, 0.25f + 0.75f * (0.40f * f1IndexScore + 0.40f * f2IndexScore + 0.20f * gapScore));
    }

    score *= tonalPenalty * pitchScore * harmonicAlignmentScore * formantIndexScore;

    // Clamp to [0, 1]
    score = std::max(0.0f, std::min(score, 1.0f));
    return score;
}

// Compute formant envelope using cepstral liftering
// This separates vocal tract resonances (formants) from pitch harmonics
// Method: log spectrum → IFFT → lifter → FFT → exp
void FreqScannerSink::getFormantEnvelope(int startBin, int endBin, QVector<Real>& envelope, Real *pitchHz)
{
    if (pitchHz) {
        *pitchHz = 0.0;
    }

    if (startBin < 0 || endBin >= m_fftSize || startBin > endBin) {
        envelope.clear();
        return;
    }

    int numBins = endBin - startBin + 1;
    envelope.resize(numBins);

    // Check if cepstral FFT engines are available and large enough
    if (!m_cepstrumFFTInverse || !m_cepstrumFFTForward || numBins > m_cepstrumSize) {
        // Fallback: return simple log/exp without cepstral processing
        for (int i = 0; i < numBins; i++) {
            Real magSq = magSqFromRawFFT(startBin + i);
            envelope[i] = std::sqrt(std::max(magSq, (Real)1e-12));
        }
        return;
    }

    // Step 1: Compute log magnitude spectrum
    QVector<Real> logMag(numBins);
    Real minLog = -10.0; // Floor to avoid log(0)
    
    for (int i = 0; i < numBins; i++)
    {
        Real magSq = magSqFromRawFFT(startBin + i);
        Real mag = std::sqrt(std::max(magSq, (Real)1e-12));
        logMag[i] = std::log(mag);
        if (logMag[i] < minLog) {
            logMag[i] = minLog;
        }
    }
    
    // Step 2: Apply cepstral liftering for better source-filter separation
    // Cepstral analysis separates:
    //   - Voice pitch (high quefrency peak) 
    //   - Formants (low quefrency components)
    // Liftering = low-pass filtering in quefrency domain
    
    // Copy log magnitude to inverse FFT input (real data, symmetric spectrum)
    // For real cepstrum, we need a symmetric spectrum:
    // [DC, positive freqs, Nyquist, negative freqs (mirror)]
    
    // DC component
    m_cepstrumFFTInverse->in()[0] = Complex(logMag[0], 0.0);
    
    // Positive frequencies
    int halfBins = std::min(numBins, m_cepstrumSize / 2);
    for (int i = 1; i < halfBins; i++) {
        m_cepstrumFFTInverse->in()[i] = Complex(logMag[i], 0.0);
    }
    
    // Nyquist (if we have space)
    if (m_cepstrumSize > halfBins) {
        m_cepstrumFFTInverse->in()[halfBins] = Complex(numBins > halfBins ? logMag[halfBins] : logMag[halfBins-1], 0.0);
    }
    
    // Negative frequencies (mirror of positive)
    for (int i = 1; i < halfBins; i++) {
        m_cepstrumFFTInverse->in()[m_cepstrumSize - i] = Complex(logMag[i], 0.0);
    }
    
    // Zero-pad the middle if needed
    for (int i = halfBins + 1; i < m_cepstrumSize - halfBins; i++) {
        m_cepstrumFFTInverse->in()[i] = Complex(0.0, 0.0);
    }
    
    // Step 3: IFFT to get cepstrum (quefrency domain)
    m_cepstrumFFTInverse->transform();
    
    // Step 4: Estimate pitch from the cepstrum before liftering.
    // Pitch appears as a peak at higher quefrencies (around 3-14 ms).
    float binBW = m_scannerSampleRate / (float)m_fftSize;
    float quefrencyResolution = 1.0f / (m_cepstrumSize * binBW); // seconds per bin in quefrency

    if (pitchHz) {
        const float minPitchHz = 70.0f;
        const float maxPitchHz = 300.0f;
        const float minQuefrency = 1.0f / maxPitchHz;
        const float maxQuefrency = 1.0f / minPitchHz;
        const int minBin = std::max(1, (int)std::ceil(minQuefrency / quefrencyResolution));
        const int maxBin = std::min(m_cepstrumSize / 2, (int)std::floor(maxQuefrency / quefrencyResolution));
        Real maxVal = 0.0;
        int maxIdx = -1;

        for (int i = minBin; i <= maxBin; i++) {
            Real val = std::abs(m_cepstrumFFTInverse->out()[i].real());
            if (val > maxVal) {
                maxVal = val;
                maxIdx = i;
            }
        }

        if (maxIdx > 0) {
            *pitchHz = 1.0f / (maxIdx * quefrencyResolution);
        }
    }

    // Step 5: Apply lifter (low-pass filter in quefrency domain)
    // Lifter cutoff: keep low quefrencies (formant envelope), remove high quefrencies (pitch harmonics)
    // Typical pitch periods: 3-10 ms (100-330 Hz F0)
    // We want to remove quefrencies corresponding to pitch harmonics
    
    // Lifter cutoff in seconds (quefrency): keep components below this
    // Use much lower cutoff - we only need to keep very low quefrencies for formant envelope
    // Most formant information is in the first few quefrency bins
    float lifterCutoffQuefrency = 0.002f; // 2 ms (reduced from 8 ms)
    int lifterCutoffBin = std::max(1, (int)(lifterCutoffQuefrency / quefrencyResolution));
    
    // Cap the lifter cutoff to reasonable maximum (1/4 of cepstrum size)
    lifterCutoffBin = std::min(lifterCutoffBin, m_cepstrumSize / 4);
    
    // Apply lifter: keep low quefrencies, zero out high quefrencies
    // Use smooth transition (raised cosine) to reduce artifacts
    int transitionBins = std::max(1, lifterCutoffBin / 4);
    
    for (int i = 0; i < m_cepstrumSize; i++) {
        Real lifterWeight = 1.0;
        
        if (i > lifterCutoffBin + transitionBins) {
            lifterWeight = 0.0; // Zero out high quefrencies
        } else if (i > lifterCutoffBin) {
            // Smooth transition using raised cosine
            float t = (float)(i - lifterCutoffBin) / transitionBins;
            lifterWeight = 0.5 * (1.0 + std::cos(M_PI * t));
        }
        // else: lifterWeight = 1.0 (keep low quefrencies)
        
        m_cepstrumFFTForward->in()[i] = m_cepstrumFFTInverse->out()[i] * lifterWeight;
    }
    
    // Step 6: FFT back to frequency domain (smoothed log spectrum)
    m_cepstrumFFTForward->transform();
    
    // Step 7: Convert back to linear magnitude
    // Take real part of FFT output and exponentiate
    // IMPORTANT: Normalize by FFT size since FFT engines don't auto-normalize
    Real normalization = 1.0 / m_cepstrumSize;
    
    for (int i = 0; i < numBins; i++)
    {
        Real smoothedLog = m_cepstrumFFTForward->out()[i].real() * normalization;
        envelope[i] = std::exp(smoothedLog);
    }
}
