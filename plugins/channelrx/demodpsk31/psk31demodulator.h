///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                    //
// Some code by AI                                                               //
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

#ifndef INCLUDE_PSK31DEMODULATOR_H
#define INCLUDE_PSK31DEMODULATOR_H

#include <deque>
#include <functional>
#include <utility>
#include <vector>

#include <QString>

#include "dsp/costasloop.h"
#include "dsp/dsptypes.h"
#include "dsp/firfilter.h"
#include "dsp/interpolator.h"
#include "dsp/nco.h"
#include "util/movingaverage.h"
#include "util/psk31.h"

// PSK31 receiver core: channel filter, carrier acquisition, tracking filter,
// carrier tracking, matched filter, symbol timing recovery, differential
// detection and Varicode decoding. Samples are resampled to an internal rate
// of 1000 samples/s.
class PSK31Demodulator
{
public:
    static constexpr int m_sampleRate = 1000;
    static constexpr int m_samplesPerSymbol = 32;

    PSK31Demodulator();

    // Resamples from channelSampleRate after mixing channelFrequencyOffset to 0 Hz.
    void setChannel(int channelSampleRate, int channelFrequencyOffset);
    void setRFBandwidth(Real rfBandwidth);
    Real getRFBandwidth() const { return m_rfBandwidth; }

    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    // Process one sample at the internal rate, normalized so full scale is 1.0.
    void processSample(const Complex& sample);
    void reset();

    void setCharacterCallback(const std::function<void(QChar)>& callback) { m_characterCallback = callback; }
    // Called once per second of samples.
    void setReportCallback(const std::function<void()>& callback) { m_reportCallback = callback; }
    // Called for every internal sample: real is the normalized matched filter
    // output, imag is 1.0 at the symbol sampling instants and 0.0 otherwise.
    void setScopeCallback(const std::function<void(const Complex&)>& callback) { m_scopeCallback = callback; }

    double getMagSq() const { return m_magsq; }
    void getMagSqLevels(double& avg, double& peak, int& nbSamples) const
    {
        avg = m_magSqLevelStore.m_magsq;
        peak = m_magSqLevelStore.m_magsqPeak;
        nbSamples = m_magSqLevelStore.m_nbSamples;
    }
    float getFrequencyOffset() const;       //!< Carrier offset from channel center in Hz
    float getSNR() const { return m_snr; }  //!< Only valid when isLocked()
    bool isLocked() const { return m_locked; }

private:
    struct MagSqLevelsStore
    {
        MagSqLevelsStore() : m_magsq(1e-12), m_magsqPeak(1e-12), m_nbSamples(1) {}
        double m_magsq;
        double m_magsqPeak;
        int m_nbSamples;
    };

    static const int m_channelFilterTaps = 255;
    static const int m_lockConfirmSymbols = 20;
    static const int m_relockConfirmSymbols = 4;
    static const int m_unlockConfirmSymbols = 20;
    static const int m_characterDelaySymbols = 8;
    static constexpr Real m_lockMetricAlpha = 1.0f / 16.0f;
    static constexpr Real m_lockThreshold = 0.5f;
    static constexpr Real m_signalLossRatio = 0.35f;
    static constexpr Real m_signalReturnRatio = 0.5f;
    static constexpr Real m_lockedLevelAlpha = 1.0f / 32.0f;
    static const int m_afcLength = 512;
    static const int m_afcInterval = 256;
    static constexpr Real m_snrReferenceBandwidth = 100.0f;
    static constexpr Real m_trackingBandwidth = 70.0f;
    static constexpr Real m_costasRange = 15.0f;
    static constexpr Real m_timingGain = 1.0f;
    static constexpr Real m_timingIntegralGain = 0.02f;
    static constexpr Real m_maxTimingError = 2.0f;
    static constexpr Real m_maxTimingRate = 1.0f;
    static constexpr Real m_afcThreshold = 20.0f;
    static constexpr Real m_afcRetuneHz = 2.0f;
    static constexpr Real m_afcIdleToneRatio = 2.0f;
    static constexpr Real m_afcTransferRate = 0.001f;

    Real m_rfBandwidth;
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    NCO m_nco;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
    Lowpass<Complex> m_channelFilter;
    Lowpass<Complex> m_trackingFilter;
    CostasLoop m_costasLoop;
    Real m_maxFrequency;
    Real m_trackingCutoff;
    Real m_trackingNoiseBandwidth;  //!< Hz

    // Levels
    MovingAverageUtil<Real, double, 16> m_movingAverage;
    MovingAverageUtil<Real, double, m_samplesPerSymbol * 2> m_levelAverage;
    MovingAverageUtil<Real, double, m_samplesPerSymbol * 4> m_signalLevelAverage;
    double m_magsq;
    double m_magsqSum;
    double m_magsqPeak;
    int m_magsqCount;
    MagSqLevelsStore m_magSqLevelStore;

    // Carrier acquisition
    std::vector<Complex> m_afcBuffer;
    std::vector<Complex> m_afcTwiddle;
    std::vector<Real> m_afcWindow;
    std::vector<Real> m_afcPowers;
    std::vector<Real> m_afcSorted;
    int m_afcIndex;
    int m_afcCount;
    Real m_afcCandidate;    //!< Hz
    bool m_afcCandidateValid;
    Real m_afcFrequency;    //!< radians/sample
    Real m_afcPhase;

    // Matched filter and symbol timing
    std::vector<Real> m_matchedFilterTaps;
    std::vector<Complex> m_matchedFilterSamples;
    int m_matchedFilterIndex;
    std::vector<Real> m_history;
    int m_historyIndex;
    Real m_symbolPower;
    Real m_clockCount;
    Real m_timingIntegrator;

    // Lock detection
    Real m_lockMetric;
    int m_lockCount;
    int m_presentCount;
    int m_unlockCount;
    Real m_lockedLevel;     //!< Tracked signal power while locked
    bool m_locked;
    std::deque<std::pair<QChar, int>> m_pendingCharacters; //!< Character and symbols until output
    std::deque<bool> m_acquisitionSymbols;

    // SNR
    double m_snrInPhaseSum;
    double m_snrQuadratureSum;
    int m_snrCount;
    double m_snrSignalPower;
    double m_snrNoisePower;
    bool m_snrAverageValid;
    float m_snr;
    int m_reportSampleCount;

    PSK31Decoder m_decoder;

    std::function<void(QChar)> m_characterCallback;
    std::function<void()> m_reportCallback;
    std::function<void(const Complex&)> m_scopeCallback;

    void acquireCarrier(const Complex& sample);
    void checkIdleToneLock(Real current);
    Real afcPower(int bin) const;
    void retune(Real frequency);
    Complex matchedFilter(const Complex& sample);
    void symbol(const Complex& value);
    void decode(bool symbol);
    void emitCharacter(QChar character);
    void unlock();
    void resetSNR();
};

#endif // INCLUDE_PSK31DEMODULATOR_H
