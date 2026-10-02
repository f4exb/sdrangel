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

#include "wefaxdecoder.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace
{

constexpr double kEpsilon = 1.0e-9;
constexpr double kPi = 3.14159265358979323846;

double median(std::vector<double> values)
{
    if (values.empty()) {
        return 0.0;
    }

    const std::size_t middle = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + middle, values.end());
    double result = values[middle];

    if ((values.size() % 2) == 0)
    {
        const auto lower = std::max_element(values.begin(), values.begin() + middle);
        result = (*lower + result) / 2.0;
    }

    return result;
}

double clampLevel(float level)
{
    if (!std::isfinite(level)) {
        return 0.0;
    }

    return std::clamp(static_cast<double>(level), 0.0, 1.0);
}

} // namespace

WefaxDecoder::WefaxDecoder() :
    WefaxDecoder(Config())
{
}

WefaxDecoder::WefaxDecoder(const Config& config) :
    m_config(),
    m_state(State::Idle),
    m_inputSampleIndex(0),
    m_previousWhite(false),
    m_havePreviousLevel(false),
    m_phasingPulseOpen(false),
    m_phasingPulseStart(0.0),
    m_phasingWindowIndex(0),
    m_phasingWindowFill(0),
    m_phasingWindowSum(0.0),
    m_hasPhasingEstimate(false),
    m_measuredSamplesPerLine(0.0),
    m_phasingOriginSample(0.0),
    m_phasingConfidence(0.0),
    m_timingStatus(TimingStatus::None),
    m_appliedSamplesPerLine(0.0),
    m_samplesUntilRasterStart(0.0),
    m_imageWidth(0),
    m_samplesPerPixel(0.0),
    m_nextPixelEnd(0.0),
    m_linePosition(0.0),
    m_pixelIndex(0),
    m_pixelSum(0.0),
    m_pixelWeight(0.0),
    m_pixelConfidenceSum(0.0),
    m_completedLineCount(0)
{
    configure(config);
}

bool WefaxDecoder::configure(const Config& config)
{
    if (!isSupportedIOC(config.ioc)
        || !isSupportedLinesPerMinute(config.linesPerMinute)
        || !std::isfinite(config.nominalSampleRate)
        || (config.nominalSampleRate <= 0.0)
        || (config.minimumPhasingLines < 2))
    {
        return false;
    }

    m_config = config;
    m_imageWidth = rasterWidth(config.ioc);
    reset();
    return true;
}

void WefaxDecoder::reset()
{
    m_state = State::Idle;
    resetPhasingWindow();
    m_inputSampleIndex = 0;
    m_previousWhite = false;
    m_havePreviousLevel = false;
    m_phasingPulseOpen = false;
    m_phasingPulseStart = 0.0;
    m_phasingPoints.clear();
    m_hasPhasingEstimate = false;
    m_measuredSamplesPerLine = 0.0;
    m_phasingOriginSample = 0.0;
    m_phasingConfidence = 0.0;
    m_timingStatus = TimingStatus::None;
    m_appliedSamplesPerLine = 0.0;
    m_samplesUntilRasterStart = 0.0;
    m_completedLines.clear();
    m_completedLineCount = 0;
    resetRaster();
}

void WefaxDecoder::resetPhasingWindow()
{
    // 0.5% of a line, a tenth of the 5% phasing pulse.
    const int window = std::max(1, static_cast<int>(std::lround(0.005 * nominalSamplesPerLine())));
    m_phasingWindow.assign(window, 0.0);
    m_phasingWindowIndex = 0;
    m_phasingWindowFill = 0;
    m_phasingWindowSum = 0.0;
}

void WefaxDecoder::startPhasing()
{
    m_state = State::Phasing;
    resetPhasingWindow();
    m_previousWhite = false;
    m_havePreviousLevel = false;
    m_phasingPulseOpen = false;
    m_phasingPulseStart = 0.0;
    m_phasingPoints.clear();
    m_hasPhasingEstimate = false;
    m_measuredSamplesPerLine = 0.0;
    m_phasingOriginSample = 0.0;
    m_phasingConfidence = 0.0;
    m_timingStatus = TimingStatus::InsufficientEvidence;
    m_completedLines.clear();
    m_completedLineCount = 0;
    resetRaster();
}

bool WefaxDecoder::finishPhasing()
{
    updatePhasingEstimate();

    if (!m_hasPhasingEstimate || (phasingLineCount() < m_config.minimumPhasingLines)) {
        return false;
    }

    return startReceiving(m_measuredSamplesPerLine);
}

bool WefaxDecoder::finishPhasingAligned()
{
    updatePhasingEstimate();

    if (!m_hasPhasingEstimate || (phasingLineCount() < m_config.minimumPhasingLines)) {
        return false;
    }

    const double phase = (static_cast<double>(m_inputSampleIndex) - m_phasingOriginSample)
        / m_measuredSamplesPerLine;
    const double nextLine = std::ceil(phase - 1.0e-9);
    const double nextLineSample = m_phasingOriginSample + nextLine * m_measuredSamplesPerLine;
    const double delay = std::clamp(
        nextLineSample - static_cast<double>(m_inputSampleIndex),
        0.0,
        m_measuredSamplesPerLine);

    if (!startReceiving(m_measuredSamplesPerLine)) {
        return false;
    }

    m_samplesUntilRasterStart = delay;
    return true;
}

bool WefaxDecoder::startReceiving(double samplesPerLine)
{
    if (samplesPerLine <= 0.0) {
        samplesPerLine = m_hasPhasingEstimate ? m_measuredSamplesPerLine : nominalSamplesPerLine();
    }

    if (!std::isfinite(samplesPerLine) || (samplesPerLine <= imageWidth())) {
        return false;
    }

    m_appliedSamplesPerLine = samplesPerLine;
    m_samplesUntilRasterStart = 0.0;
    m_samplesPerPixel = samplesPerLine / m_imageWidth;
    m_state = State::Receiving;
    m_completedLines.clear();
    m_completedLineCount = 0;
    resetRaster();
    return true;
}

void WefaxDecoder::stop()
{
    m_state = State::Idle;
    resetRaster();
}

void WefaxDecoder::feed(const float *samples, std::size_t count)
{
    if ((samples == nullptr) || (count == 0)) {
        return;
    }

    if (m_state == State::Phasing)
    {
        for (std::size_t i = 0; i < count; ++i) {
            processPhasingSample(samples[i]);
            ++m_inputSampleIndex;
        }
    }
    else if (m_state == State::Receiving)
    {
        for (std::size_t i = 0; i < count; ++i) {
            processImageSample(samples[i], 1.0f);
            ++m_inputSampleIndex;
        }
    }
    else
    {
        m_inputSampleIndex += count;
    }
}

int WefaxDecoder::phasingLineCount() const
{
    if (m_phasingPoints.size() < 2) {
        return 0;
    }

    return m_phasingPoints.back().lineIndex - m_phasingPoints.front().lineIndex + 1;
}

double WefaxDecoder::lineClockCorrectionPpm() const
{
    if (m_measuredSamplesPerLine <= 0.0) {
        return 0.0;
    }

    return 1.0e6 * ((m_measuredSamplesPerLine / nominalSamplesPerLine()) - 1.0);
}

double WefaxDecoder::appliedLineClockCorrectionPpm() const
{
    return m_appliedSamplesPerLine > 0.0
        ? 1.0e6 * ((m_appliedSamplesPerLine / nominalSamplesPerLine()) - 1.0)
        : 0.0;
}

const char *WefaxDecoder::timingStatusText(TimingStatus status)
{
    switch (status)
    {
    case TimingStatus::InsufficientEvidence: return "insufficient-evidence";
    case TimingStatus::LowConfidence: return "low-confidence";
    case TimingStatus::OutOfRange: return "out-of-range";
    case TimingStatus::Accepted: return "accepted";
    case TimingStatus::None: default: return "none";
    }
}

std::vector<std::vector<std::uint8_t>> WefaxDecoder::takeCompletedLines()
{
    std::vector<std::vector<std::uint8_t>> lines;
    lines.swap(m_completedLines);
    return lines;
}

int WefaxDecoder::rasterWidth(int ioc)
{
    return isSupportedIOC(ioc) ? static_cast<int>(std::floor(static_cast<double>(ioc) * kPi)) : 0;
}

bool WefaxDecoder::isSupportedIOC(int ioc)
{
    return (ioc == 288) || (ioc == 576);
}

bool WefaxDecoder::isSupportedLinesPerMinute(double linesPerMinute)
{
    static constexpr double rates[] = {60.0, 90.0, 100.0, 120.0, 180.0, 240.0};

    if (!std::isfinite(linesPerMinute)) {
        return false;
    }

    return std::any_of(std::begin(rates), std::end(rates), [linesPerMinute](double rate) {
        return std::abs(linesPerMinute - rate) < 1.0e-6;
    });
}

double WefaxDecoder::nominalSamplesPerLine() const
{
    return m_config.nominalSampleRate * 60.0 / m_config.linesPerMinute;
}

void WefaxDecoder::processPhasingSample(float level)
{
    const double normalized = clampLevel(level);
    const double whiteLevel = m_config.inverted ? 1.0 - normalized : normalized;

    // On a weak signal, noise splits the phasing pulse into fragments and
    // makes its edges chatter. Average over a small fraction of the pulse and
    // apply hysteresis. The average delays both edges equally by half its
    // length, which is removed from the edge times below.
    const int window = static_cast<int>(m_phasingWindow.size());
    m_phasingWindowSum += whiteLevel - m_phasingWindow[m_phasingWindowIndex];
    m_phasingWindow[m_phasingWindowIndex] = whiteLevel;
    m_phasingWindowIndex = (m_phasingWindowIndex + 1) % window;
    if (m_phasingWindowFill < window)
    {
        ++m_phasingWindowFill;
        return;
    }
    const double smoothed = m_phasingWindowSum / window;
    const bool white = (m_havePreviousLevel && m_previousWhite) ? (smoothed > 0.4) : (smoothed >= 0.6);
    const double edgeSample = static_cast<double>(m_inputSampleIndex) - 0.5 * (window - 1);

    if (white && (!m_havePreviousLevel || !m_previousWhite))
    {
        m_phasingPulseOpen = true;
        m_phasingPulseStart = edgeSample;
    }
    else if (!white && m_havePreviousLevel && m_previousWhite && m_phasingPulseOpen)
    {
        const double pulseEnd = edgeSample;
        const double pulseWidth = pulseEnd - m_phasingPulseStart;
        const double nominal = nominalSamplesPerLine();

        if ((pulseWidth >= nominal * 0.01) && (pulseWidth <= nominal * 0.1)) {
            // Time each pulse by its centre, the mean of both edges. On weak
            // signals the leading and trailing edges are displaced unequally,
            // and a leading-edge fit alone can misjudge the line rate by tens
            // of ppm. The raster is still aligned to the leading edge.
            addPhasingPulse(m_phasingPulseStart + 0.5 * pulseWidth, pulseWidth);
        }

        m_phasingPulseOpen = false;
    }

    m_previousWhite = white;
    m_havePreviousLevel = true;
}

void WefaxDecoder::addPhasingPulse(double sampleIndex, double width)
{
    const double nominal = nominalSamplesPerLine();

    if (m_phasingPoints.empty())
    {
        m_phasingPoints.push_back({sampleIndex, 0, width});
        return;
    }

    const PhasingPoint& last = m_phasingPoints.back();
    const double difference = sampleIndex - last.sampleIndex;
    const int elapsedLines = static_cast<int>(std::llround(difference / nominal));

    if ((elapsedLines >= 1)
        && (elapsedLines <= 4)
        && (std::abs((difference / elapsedLines) - nominal) <= nominal * 0.1))
    {
        m_phasingPoints.push_back({sampleIndex, last.lineIndex + elapsedLines, width});

        if (m_phasingPoints.size() > 64) {
            m_phasingPoints.erase(m_phasingPoints.begin());
        }

        updatePhasingEstimate();
    }
    else if (difference > nominal * 4.5)
    {
        m_phasingPoints.clear();
        m_phasingPoints.push_back({sampleIndex, 0, width});
        m_hasPhasingEstimate = false;
        m_phasingConfidence = 0.0;
        m_timingStatus = TimingStatus::InsufficientEvidence;
    }
}

void WefaxDecoder::updatePhasingEstimate()
{
    if (m_phasingPoints.size() < 2)
    {
        m_hasPhasingEstimate = false;
        m_phasingConfidence = 0.0;
        m_timingStatus = TimingStatus::InsufficientEvidence;
        return;
    }

    // Theil-Sen gives a robust first estimate for the short phasing sequence.
    // With at most 64 points, considering all pairs is inexpensive.
    std::vector<double> pairSlopes;
    pairSlopes.reserve(m_phasingPoints.size() * (m_phasingPoints.size() - 1) / 2);

    for (std::size_t i = 0; i < m_phasingPoints.size(); ++i)
    {
        for (std::size_t j = i + 1; j < m_phasingPoints.size(); ++j)
        {
            const int lineDifference = m_phasingPoints[j].lineIndex - m_phasingPoints[i].lineIndex;
            if (lineDifference > 0) {
                pairSlopes.push_back(
                    (m_phasingPoints[j].sampleIndex - m_phasingPoints[i].sampleIndex) / lineDifference);
            }
        }
    }

    const double robustSlope = median(pairSlopes);
    std::vector<double> intercepts;
    intercepts.reserve(m_phasingPoints.size());
    for (const PhasingPoint& point : m_phasingPoints) {
        intercepts.push_back(point.sampleIndex - robustSlope * point.lineIndex);
    }
    const double robustIntercept = median(intercepts);

    std::vector<double> absoluteResiduals;
    absoluteResiduals.reserve(m_phasingPoints.size());
    for (const PhasingPoint& point : m_phasingPoints) {
        absoluteResiduals.push_back(std::abs(point.sampleIndex - (robustIntercept + robustSlope * point.lineIndex)));
    }
    const double residualThreshold = std::max(2.0, 6.0 * median(absoluteResiduals));

    double sumX = 0.0;
    double sumY = 0.0;
    double sumXX = 0.0;
    double sumXY = 0.0;
    int inlierCount = 0;

    for (const PhasingPoint& point : m_phasingPoints)
    {
        const double residual = std::abs(point.sampleIndex - (robustIntercept + robustSlope * point.lineIndex));
        if (residual > residualThreshold) {
            continue;
        }

        const double x = point.lineIndex;
        sumX += x;
        sumY += point.sampleIndex;
        sumXX += x * x;
        sumXY += x * point.sampleIndex;
        ++inlierCount;
    }

    const double count = static_cast<double>(inlierCount);
    const double denominator = count * sumXX - sumX * sumX;

    if ((inlierCount < 2) || (std::abs(denominator) < kEpsilon)) {
        return;
    }

    const double slope = (count * sumXY - sumX * sumY) / denominator;
    const double intercept = (sumY - slope * sumX) / count;

    double squaredResidualSum = 0.0;
    for (const PhasingPoint& point : m_phasingPoints)
    {
        const double residual = point.sampleIndex - (intercept + slope * point.lineIndex);
        if (std::abs(residual) <= residualThreshold) {
            squaredResidualSum += residual * residual;
        }
    }

    const double residualRms = std::sqrt(squaredResidualSum / count);
    const double countConfidence = std::min(
        1.0,
        static_cast<double>(inlierCount) / static_cast<double>(m_config.minimumPhasingLines));
    const double residualConfidence = std::clamp(1.0 - residualRms / (slope * 0.01), 0.0, 1.0);

    m_measuredSamplesPerLine = slope;
    // Points are pulse centres; the line starts at the leading edge.
    std::vector<double> widths;
    widths.reserve(m_phasingPoints.size());
    for (const PhasingPoint& point : m_phasingPoints) {
        widths.push_back(point.width);
    }
    m_phasingOriginSample = intercept - 0.5 * median(widths);
    m_phasingConfidence = countConfidence * residualConfidence;
    const bool validSlope = std::isfinite(slope) && (slope > imageWidth());
    const double correctionPpm = validSlope
        ? 1.0e6 * ((slope / nominalSamplesPerLine()) - 1.0) : 0.0;
    if (!validSlope) {
        m_timingStatus = TimingStatus::InsufficientEvidence;
    } else if (std::abs(correctionPpm) > 1000.0) {
        m_timingStatus = TimingStatus::OutOfRange;
    } else if (m_phasingConfidence < 0.5) {
        m_timingStatus = TimingStatus::LowConfidence;
    } else {
        m_timingStatus = TimingStatus::Accepted;
    }
    m_hasPhasingEstimate = m_timingStatus == TimingStatus::Accepted;
}

void WefaxDecoder::processImageSample(float level, float confidence)
{
    if (m_samplesUntilRasterStart > 0.0)
    {
        m_samplesUntilRasterStart -= 1.0;
        return;
    }

    double remainingWeight = 1.0;
    const double whiteLevel = m_config.inverted ? 1.0 - clampLevel(level) : clampLevel(level);

    while (remainingWeight > kEpsilon)
    {
        const double segmentEnd = std::min(m_nextPixelEnd, m_appliedSamplesPerLine);
        double availableWeight = segmentEnd - m_linePosition;

        if (availableWeight <= kEpsilon)
        {
            if (m_pixelIndex < m_imageWidth) {
                finishPixel();
            }

            if (m_linePosition >= m_appliedSamplesPerLine - kEpsilon) {
                finishLine();
            }

            continue;
        }

        const double weight = std::min(remainingWeight, availableWeight);
        m_pixelSum += whiteLevel * weight;
        m_pixelConfidenceSum += confidence * weight;
        m_pixelWeight += weight;
        m_linePosition += weight;
        remainingWeight -= weight;

        if (m_linePosition >= m_nextPixelEnd - kEpsilon) {
            finishPixel();
        }

        if (m_linePosition >= m_appliedSamplesPerLine - kEpsilon) {
            finishLine();
        }
    }
}

void WefaxDecoder::finishPixel()
{
    if (m_pixelIndex >= m_imageWidth) {
        return;
    }

    const double value = m_pixelWeight > 0.0 ? m_pixelSum / m_pixelWeight : 0.0;
    m_currentLine[m_pixelIndex] = static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0, 1.0) * 255.0));
    m_currentConfidence[m_pixelIndex] = m_pixelWeight > 0.0
        ? static_cast<float>(m_pixelConfidenceSum / m_pixelWeight) : 1.0f;
    ++m_pixelIndex;
    m_nextPixelEnd += m_samplesPerPixel;
    m_pixelSum = 0.0;
    m_pixelConfidenceSum = 0.0;
    m_pixelWeight = 0.0;
}

void WefaxDecoder::fillFades()
{
    // During a deep fade a weak interferer captures the discriminator and
    // draws a short streak. Charts change little from one line to the next,
    // so the pixel above is a better estimate. A line that has mostly faded is
    // a genuine dropout and is left as received.
    if ((m_config.fadeFillThreshold <= 0.0) || (m_previousLine.size() != m_currentLine.size())) {
        return;
    }

    const float threshold = static_cast<float>(m_config.fadeFillThreshold);
    const auto faded = std::count_if(m_currentConfidence.begin(), m_currentConfidence.end(),
        [threshold](float confidence) { return confidence < threshold; });
    if (faded > m_imageWidth / 2) {
        return;
    }

    for (int x = 0; x < m_imageWidth; ++x)
    {
        if (m_currentConfidence[x] < threshold) {
            m_currentLine[x] = m_previousLine[x];
        }
    }
}

void WefaxDecoder::finishLine()
{
    while (m_pixelIndex < m_imageWidth) {
        finishPixel();
    }

    fillFades();
    m_previousLine = m_currentLine;
    m_completedLines.emplace_back(std::move(m_currentLine));
    ++m_completedLineCount;
    m_linePosition -= m_appliedSamplesPerLine;

    if (m_linePosition < 0.0) {
        m_linePosition = 0.0;
    }

    m_pixelIndex = 0;
    m_pixelSum = 0.0;
    m_pixelConfidenceSum = 0.0;
    m_pixelWeight = 0.0;
    m_nextPixelEnd = m_samplesPerPixel;
    m_currentLine.assign(m_imageWidth, 0);
    m_currentConfidence.assign(m_imageWidth, 1.0f);
}

void WefaxDecoder::resetRaster()
{
    m_linePosition = 0.0;
    m_pixelIndex = 0;
    m_pixelSum = 0.0;
    m_pixelConfidenceSum = 0.0;
    m_pixelWeight = 0.0;
    m_nextPixelEnd = m_samplesPerPixel;
    m_currentLine.assign(m_imageWidth, 0);
    m_currentConfidence.assign(m_imageWidth, 1.0f);
    m_previousLine.clear();
}
