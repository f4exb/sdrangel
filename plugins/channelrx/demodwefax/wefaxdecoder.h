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

#ifndef INCLUDE_WEFAXDECODER_H
#define INCLUDE_WEFAXDECODER_H

#include <cstddef>
#include <cstdint>
#include <vector>

/**
 * Protocol-independent WEFAX line timing and raster assembler.
 *
 * Input samples are normalized video levels: 0.0 is black and 1.0 is white.
 * Start/stop tone detection and FM discrimination are deliberately outside this
 * class, allowing deterministic tests to exercise timing without RF processing.
 */
class WefaxDecoder
{
public:
    enum class State
    {
        Idle,
        Phasing,
        Receiving
    };

    enum class TimingStatus
    {
        None,
        InsufficientEvidence,
        LowConfidence,
        OutOfRange,
        Accepted
    };

    struct Config
    {
        int ioc = 576;
        double linesPerMinute = 120.0;
        double nominalSampleRate = 12000.0;
        int minimumPhasingLines = 8;
        bool inverted = false;
        // Pixels whose signal confidence (channel power relative to its
        // long-term level) is below this are deep fades, where a weak
        // interferer captures the FM discriminator. They are replaced with the
        // pixel above. 0 disables.
        double fadeFillThreshold = 0.25;
    };

    WefaxDecoder();
    explicit WefaxDecoder(const Config& config);

    bool configure(const Config& config);
    void reset();
    void startPhasing();
    bool finishPhasing();
    bool finishPhasingAligned();
    bool startReceiving(double samplesPerLine = 0.0);
    void stop();

    void feed(const float *samples, std::size_t count);
    // confidence is the channel power relative to its long-term level.
    void feedSample(float sample, float confidence = 1.0f)
    {
        if (m_state == State::Phasing) {
            processPhasingSample(sample);
        } else if (m_state == State::Receiving) {
            processImageSample(sample, confidence);
        }
        ++m_inputSampleIndex;
    }

    State state() const { return m_state; }
    const Config& config() const { return m_config; }
    int imageWidth() const { return m_imageWidth; }
    std::size_t completedLineCount() const { return m_completedLineCount; }
    int phasingLineCount() const;
    bool hasPhasingEstimate() const { return m_hasPhasingEstimate; }
    double measuredSamplesPerLine() const { return m_measuredSamplesPerLine; }
    double appliedSamplesPerLine() const { return m_appliedSamplesPerLine; }
    double lineClockCorrectionPpm() const;
    double appliedLineClockCorrectionPpm() const;
    double phasingConfidence() const { return m_phasingConfidence; }
    TimingStatus timingStatus() const { return m_timingStatus; }
    static const char *timingStatusText(TimingStatus status);

    std::vector<std::vector<std::uint8_t>> takeCompletedLines();

    static int rasterWidth(int ioc);
    static bool isSupportedIOC(int ioc);
    static bool isSupportedLinesPerMinute(double linesPerMinute);

private:
    struct PhasingPoint
    {
        double sampleIndex;     // Pulse centre
        int lineIndex;
        double width;           // Pulse width in samples
    };

    Config m_config;
    State m_state;
    std::uint64_t m_inputSampleIndex;
    bool m_previousWhite;
    bool m_havePreviousLevel;
    bool m_phasingPulseOpen;
    double m_phasingPulseStart;
    std::vector<double> m_phasingWindow;
    int m_phasingWindowIndex;
    int m_phasingWindowFill;
    double m_phasingWindowSum;
    std::vector<PhasingPoint> m_phasingPoints;
    bool m_hasPhasingEstimate;
    double m_measuredSamplesPerLine;
    double m_phasingOriginSample;
    double m_phasingConfidence;
    TimingStatus m_timingStatus;

    double m_appliedSamplesPerLine;
    double m_samplesUntilRasterStart;
    int m_imageWidth;
    double m_samplesPerPixel;
    double m_nextPixelEnd;
    double m_linePosition;
    int m_pixelIndex;
    double m_pixelSum;
    double m_pixelWeight;
    double m_pixelConfidenceSum;
    std::vector<std::uint8_t> m_currentLine;
    std::vector<float> m_currentConfidence;
    std::vector<std::uint8_t> m_previousLine;   // Last completed line, after fade fill
    std::vector<std::vector<std::uint8_t>> m_completedLines;
    std::size_t m_completedLineCount;

    double nominalSamplesPerLine() const;
    void resetPhasingWindow();
    void processPhasingSample(float level);
    void addPhasingPulse(double sampleIndex, double width);
    void updatePhasingEstimate();
    void processImageSample(float level, float confidence);
    void finishPixel();
    void fillFades();
    void finishLine();
    void resetRaster();
};

#endif // INCLUDE_WEFAXDECODER_H
