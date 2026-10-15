///////////////////////////////////////////////////////////////////////////////////
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

#include "wefaxslant.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{

struct DarkPixel
{
    int x;
    int y;
};

// Column of a dark pixel once the image is sheared by driftPerRow about its
// centre row, wrapped onto the raster width.
int shearedColumn(const DarkPixel& pixel, double driftPerRow, double centreRow, int width)
{
    const int column = static_cast<int>(std::lround(pixel.x - driftPerRow * (pixel.y - centreRow)));
    return ((column % width) + width) % width;
}

int circularDistance(int a, int b, int width)
{
    const int distance = std::abs(a - b) % width;
    return std::min(distance, width - distance);
}

} // namespace

WefaxSlant::Estimate WefaxSlant::estimate(const QImage& image, int rows)
{
    Estimate result;
    const int width = image.width();
    const int height = std::min(rows, image.height());

    if ((height < MinRows) || (width < 16) || (image.format() != QImage::Format_Grayscale8)) {
        return result;
    }

    std::vector<DarkPixel> dark;
    for (int y = 0; y < height; ++y)
    {
        const uchar *line = image.constScanLine(y);
        for (int x = 0; x < width; ++x)
        {
            if (line[x] < 128) {
                dark.push_back({x, y});
            }
        }
    }

    // A mostly dark raster is a photograph or satellite image, not a chart
    // with line work to measure.
    if (dark.empty() || (dark.size() > 0.4 * width * height)) {
        return result;
    }

    // Bound the cost of each trial shear on dense images.
    const std::size_t stride = std::max<std::size_t>(1, dark.size() / 300000);
    const double centreRow = 0.5 * height;
    std::vector<int> histogram(width);

    // Score a shear by its two strongest well-separated columns (3 pixels
    // wide). Requiring a parallel pair favours chart frames and Mercator
    // meridians over a single slanted line such as a polar meridian.
    const auto score = [&](double driftPerRow, int& strongestColumn) {
        std::fill(histogram.begin(), histogram.end(), 0);
        for (std::size_t i = 0; i < dark.size(); i += stride) {
            ++histogram[shearedColumn(dark[i], driftPerRow, centreRow, width)];
        }

        int first = 0;
        int firstColumn = 0;
        for (int x = 0; x < width; ++x)
        {
            const int sum = histogram[(x + width - 1) % width] + histogram[x] + histogram[(x + 1) % width];
            if (sum > first)
            {
                first = sum;
                firstColumn = x;
            }
        }

        int second = 0;
        for (int x = 0; x < width; ++x)
        {
            if (circularDistance(x, firstColumn, width) < width / 8) {
                continue;
            }
            const int sum = histogram[(x + width - 1) % width] + histogram[x] + histogram[(x + 1) % width];
            second = std::max(second, sum);
        }

        strongestColumn = firstColumn;
        return first + second;
    };

    // A coarse step that moves the end rows by one pixel, then a fine search.
    const double maxDrift = MaxPpm * 1.0e-6 * width;
    const double coarseStep = 2.0 / height;
    double bestDrift = 0.0;
    int bestScore = -1;
    int bestColumn = 0;
    for (double drift = -maxDrift; drift <= maxDrift + 1.0e-12; drift += coarseStep)
    {
        int column;
        const int value = score(drift, column);
        if (value > bestScore)
        {
            bestScore = value;
            bestDrift = drift;
            bestColumn = column;
        }
    }

    const double coarseDrift = bestDrift;
    for (double drift = coarseDrift - coarseStep; drift <= coarseDrift + coarseStep + 1.0e-12; drift += coarseStep / 8.0)
    {
        int column;
        const int value = score(drift, column);
        if (value > bestScore)
        {
            bestScore = value;
            bestDrift = drift;
            bestColumn = column;
        }
    }

    // At the edge of the search range the true drift may lie beyond it.
    if (std::abs(bestDrift) > maxDrift - coarseStep) {
        return result;
    }

    // The strongest line must run through most of both halves of the image,
    // otherwise it is a shorter feature or the image is still too short.
    std::vector<char> covered(height, 0);
    for (const DarkPixel& pixel : dark)
    {
        if (circularDistance(shearedColumn(pixel, bestDrift, centreRow, width), bestColumn, width) <= 1) {
            covered[pixel.y] = 1;
        }
    }
    const int half = height / 2;
    const double topCoverage = std::count(covered.begin(), covered.begin() + half, 1) / static_cast<double>(half);
    const double bottomCoverage = std::count(covered.begin() + half, covered.end(), 1)
        / static_cast<double>(height - half);

    result.driftPerRow = bestDrift;
    result.coverage = std::min(topCoverage, bottomCoverage);
    result.valid = result.coverage >= MinCoverage;
    return result;
}

QImage WefaxSlant::correct(const QImage& image, int rows, double driftPerRow, double columnOffset)
{
    const int width = image.width();
    const int height = std::min(rows, image.height());

    if ((height <= 0) || (width <= 0)) {
        return QImage();
    }

    // The raster is one continuous pixel stream cut into rows of width
    // pixels. A drift of d pixels per row means a true line is width + d
    // stream pixels long, so re-cut the stream at that length about the centre
    // row. Content leaving one side of a row correctly enters the next.
    const qint64 streamLength = static_cast<qint64>(width) * height;
    const auto pixel = [&](qint64 position) -> double {
        if ((position < 0) || (position >= streamLength)) {
            return 255.0;
        }
        return image.constScanLine(static_cast<int>(position / width))[position % width];
    };

    const double lineLength = width + driftPerRow;
    const double centreRow = 0.5 * height;
    QImage corrected(width, height, QImage::Format_Grayscale8);

    for (int y = 0; y < height; ++y)
    {
        const double rowStart = (y - centreRow) * lineLength + centreRow * width + columnOffset;
        uchar *line = corrected.scanLine(y);
        for (int x = 0; x < width; ++x)
        {
            const double position = rowStart + x;
            const qint64 index = static_cast<qint64>(std::floor(position));
            const double fraction = position - index;
            const double value = pixel(index) + fraction * (pixel(index + 1) - pixel(index));
            line[x] = static_cast<uchar>(std::clamp(static_cast<int>(std::lround(value)), 0, 255));
        }
    }

    return corrected;
}

int WefaxSlant::deadSectorStart(const QImage& image, int rows)
{
    const int width = image.width();
    const int height = std::min(rows, image.height());

    if ((height < MinRows) || (width < 16) || (image.format() != QImage::Format_Grayscale8)) {
        return -1;
    }

    // Fraction of rows with ink in each column.
    std::vector<double> ink(width, 0.0);
    for (int y = 0; y < height; ++y)
    {
        const uchar *line = image.constScanLine(y);
        for (int x = 0; x < width; ++x)
        {
            if (line[x] < 128) {
                ink[x] += 1.0;
            }
        }
    }
    for (double& value : ink) {
        value /= height;
    }

    // Smooth over 9 columns so isolated noise does not split the band.
    constexpr int smoothing = 9;
    std::vector<double> smoothed(width, 0.0);
    for (int x = 0; x < width; ++x)
    {
        for (int k = -smoothing / 2; k <= smoothing / 2; ++k) {
            smoothed[x] += ink[(x + k + width) % width];
        }
        smoothed[x] /= smoothing;
    }

    // Blank means well below the chart's typical ink and nearly empty. On a
    // weak signal noise inks every column, so no band qualifies and the
    // image is left alone.
    std::vector<double> sorted(ink);
    std::nth_element(sorted.begin(), sorted.begin() + width / 2, sorted.end());
    const double medianInk = sorted[width / 2];
    const double threshold = std::min(0.25 * medianInk, 0.02);

    // Longest circular run of blank columns.
    int bestStart = -1;
    int bestLength = 0;
    int runStart = 0;
    int runLength = 0;
    for (int i = 0; i < 2 * width; ++i)
    {
        if (smoothed[i % width] <= threshold)
        {
            if (runLength == 0) {
                runStart = i;
            }
            ++runLength;
            if ((runLength > bestLength) && (runLength <= width))
            {
                bestLength = runLength;
                bestStart = runStart % width;
            }
        }
        else
        {
            runLength = 0;
        }
    }

    // A dead sector is a few percent of the line, not an empty image or a
    // gap inside the chart.
    if ((bestLength < 0.015 * width) || (bestLength > 0.15 * width)) {
        return -1;
    }
    return bestStart;
}
