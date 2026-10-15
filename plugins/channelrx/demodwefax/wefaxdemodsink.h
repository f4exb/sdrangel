///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_WEFAXDEMODSINK_H
#define INCLUDE_WEFAXDEMODSINK_H

#include <cstdint>
#include <deque>
#include <memory>

#include "dsp/channelsamplesink.h"
#include "dsp/firfilter.h"
#include "dsp/goertzel.h"
#include "dsp/interpolator.h"
#include "dsp/nco.h"
#include "dsp/phasediscri.h"
#include "util/movingaverage.h"
#include "wefaxdecoder.h"
#include "wefaxtonemeter.h"
#include "wefaxdemodsettings.h"

class WefaxDemodSink : public ChannelSampleSink
{
public:
    enum class CompletionReason
    {
        None,
        StopTone,
        ManualStop,
        Rephase,
        ModeChanged,
        RowLimit,
        NewStart,
        SignalLoss,
        SourceChanged,
        Shutdown
    };

    WefaxDemodSink();

    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end) override;
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const QStringList& settingsKeys, const WefaxDemodSettings& settings, bool force = false);

    void startPhasing();
    bool finishPhasing();
    // keepTuning keeps the AFC correction measured during phasing; a manual
    // start may be on a different signal, so it clears it by default.
    bool startReceiving(bool keepTuning = false);
    void stop(CompletionReason reason = CompletionReason::ManualStop);
    WefaxDecoder& decoder() { return m_decoder; }
    const WefaxDecoder& decoder() const { return m_decoder; }
    std::vector<std::vector<std::uint8_t>> takeCompletedLines();
    int internalSampleRate() const { return m_settings.internalSampleRate(); }
    int selectedIOC() const { return m_settings.m_ioc; }
    int selectedLinesPerMinute() const { return m_settings.m_linesPerMinute; }
    CompletionReason completionReason() const { return m_completionReason; }
    static const char *completionReasonText(CompletionReason reason);
    double tuningErrorHz() const { return m_tuningErrorHz; }
    // Measured low (black) and high (white) tone frequencies before AFC.
    double lowToneHz() const { return m_blackFrequencyHz; }
    double highToneHz() const { return m_whiteFrequencyHz; }
    const char *timingSource() const { return m_timingFromPhasing ? "phasing" : "nominal/manual"; }
    double effectiveLinesPerMinute() const;

    double magnitudeSquared() const { return m_magnitudeSquared; }
    void getMagSqLevels(double& average, double& peak, int& sampleCount);

private:
    WefaxDemodSettings m_settings;
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    NCO m_nco;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
    Lowpass<Complex> m_channelFilter;
    PhaseDiscriminators m_phaseDiscriminator;
    Lowpass<Real> m_videoFilter;
    Lowpass<Real> m_controlFilter;
    Lowpass<Real> m_powerFilter;        // Channel power, aligned with the video
    double m_powerReference;            // Slow average of channel power
    double m_powerReferenceAlpha;
    Real m_videoScale;
    WefaxDecoder m_decoder;

    MovingAverageUtil<Real, double, 32> m_movingAverage;
    double m_magnitudeSquared;
    double m_magnitudeSquaredSum;
    double m_magnitudeSquaredPeak;
    int m_magnitudeSquaredCount;

    std::unique_ptr<Goertzel> m_ioc576StartToneDetector;
    std::unique_ptr<Goertzel> m_ioc288StartToneDetector;
    std::unique_ptr<Goertzel> m_stopToneDetector;
    int m_toneWindowSamples;
    int m_startConfirmWindows;
    int m_stopConfirmWindows;
    int m_ioc576StartToneWindows;
    int m_ioc288StartToneWindows;
    int m_stopToneWindows;
    std::uint64_t m_acquisitionSampleCount;
    std::uint64_t m_lastPhasingLineSample;
    int m_lastPhasingLineCount;
    bool m_autoPhasingPreviousWhite;
    bool m_autoPhasingHaveLevel;
    bool m_autoPhasingPulseOpen;
    std::uint64_t m_autoPhasingPulseStart;
    double m_autoPhasingLastPulseCenter;
    bool m_autoPhasingHaveLastPulse;
    std::deque<double> m_autoPhasingIntervals;
    CompletionReason m_completionReason;
    std::deque<std::vector<std::uint8_t>> m_pendingRows;
    double m_signalBlockSum;
    int m_signalBlockSamples;
    double m_signalReference;
    std::uint64_t m_lowSignalSamples;
    std::uint64_t m_signalLossSampleLimit;
    double m_blackFrequencyHz;
    double m_whiteFrequencyHz;
    double m_tuningErrorHz;
    double m_tuningCorrectionHz;        // AFC correction applied to the discriminator
    WefaxToneMeter m_toneMeter;
    bool m_haveBlackFrequency;
    bool m_haveWhiteFrequency;
    bool m_timingFromPhasing;

    void processOneSample(const Complex& sample);
    void processAcquisitionSample(float video, float frequencyHz, double magnitudeSquared);
    void processAutomaticPhasingSample(float video);
    void selectAutomaticIOC(int ioc);
    void selectAutomaticLpm(int linesPerMinute);
    void resetAcquisition();
    void resetAutomaticPhasingDetector();
    void resetSignalLossDetector();
    void resetAfc();
    void updateAfc();
    void configureDsp();
    void configureVideoFilter();
    void configureDecoder();
};

#endif // INCLUDE_WEFAXDEMODSINK_H
