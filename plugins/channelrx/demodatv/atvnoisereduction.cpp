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

#include <algorithm>
#include <cmath>

#include "atvnoisereduction.h"

static const int histogramBinsPerLevel = 4;

ATVNoiseReduction::ATVNoiseReduction() :
    m_width(0),
    m_height(0),
    m_strength(0.0f),
    m_histogramCount(0),
    m_threshold(0.0f),
    m_window(7)
{
    m_histogram.resize(256 * histogramBinsPerLevel);
}

void ATVNoiseReduction::configure(int width, int height)
{
    m_width = std::max(width, 0);
    m_height = std::max(height, 0);
    m_frame.assign((size_t) m_width * m_height * 3, 0.0f);
    m_rowValid.assign(m_height, false);
    m_differenceSum.resize(m_width + 1);
    std::fill(m_histogram.begin(), m_histogram.end(), 0);
    m_histogramCount = 0;
    m_threshold = 0.0f;
}

void ATVNoiseReduction::setStrength(int strength)
{
    // At 100%, still frames are blended 90% with the previous output, which reduces the
    // noise to about a quarter
    m_strength = 0.9f * std::min(std::max(strength, 0), 100) / 100.0f;
}

void ATVNoiseReduction::processRow(int row, unsigned int *rgb)
{
    if ((row < 0) || (row >= m_height) || (m_width == 0)) {
        return;
    }

    float *prev = &m_frame[(size_t) row * m_width * 3];

    if (!m_rowValid[row])
    {
        for (int n = 0; n < m_width; n++)
        {
            prev[3*n] = rgb[n] & 0xff;
            prev[3*n+1] = (rgb[n] >> 8) & 0xff;
            prev[3*n+2] = (rgb[n] >> 16) & 0xff;
        }

        m_rowValid[row] = true;
        return;
    }

    // Luminance difference from the previous frame, signed, with running sums
    m_differenceSum[0] = 0.0f;

    for (int n = 0; n < m_width; n++)
    {
        float r = rgb[n] & 0xff;
        float g = (rgb[n] >> 8) & 0xff;
        float b = (rgb[n] >> 16) & 0xff;
        float difference = 0.299f * (r - prev[3*n]) + 0.587f * (g - prev[3*n+1]) + 0.114f * (b - prev[3*n+2]);
        m_differenceSum[n + 1] = m_differenceSum[n] + difference;
    }

    const float windowScale = 1.0f / m_window;

    for (int n = 0; n < m_width; n++)
    {
        // The signed difference averaged over a few pixels: noise averages towards zero,
        // while motion, a consistent change in brightness, does not
        int a = std::max(n - m_window / 2, 0);
        int b = std::min(n - m_window / 2 + m_window, m_width);
        float d = std::fabs(m_differenceSum[b] - m_differenceSum[a]) * (b - a == m_window ? windowScale : 1.0f / (b - a));
        int bin = std::min((int) (d * histogramBinsPerLevel), (int) m_histogram.size() - 1);
        m_histogram[bin]++;
        m_histogramCount++;

        // Full blend below the threshold, none above twice it
        float k = m_threshold > 0.0f ? m_strength * std::min(std::max(2.0f - d / m_threshold, 0.0f), 1.0f) : 0.0f;
        const unsigned int in = rgb[n];
        unsigned int packed = 0;

        for (int c = 0; c < 3; c++)
        {
            float cur = (in >> (8 * c)) & 0xff;
            float out = cur + k * (prev[3*n+c] - cur);
            prev[3*n+c] = out;
            packed |= ((unsigned int) (out + 0.5f)) << (8 * c); // Between two 8-bit values, so no need to clip
        }

        rgb[n] = packed;
    }
}

void ATVNoiseReduction::endFrame()
{
    if (m_histogramCount == 0) {
        return;
    }

    // The lower quartile of the difference: as long as a quarter of the picture is still,
    // this measures the noise, even when most of it is moving
    int quartile = m_histogramCount / 4;
    int sum = 0;
    int bin = 0;

    for (; bin < (int) m_histogram.size(); bin++)
    {
        sum += m_histogram[bin];

        if (sum >= quartile) {
            break;
        }
    }

    // For noise alone, the quartile of |difference| is 0.32 sigma: threshold at about 1.6 sigma
    float threshold = std::max(5.0f * (bin + 0.5f) / histogramBinsPerLevel, 1.0f);
    m_threshold = m_threshold > 0.0f ? m_threshold + 0.2f * (threshold - m_threshold) : threshold;
    std::fill(m_histogram.begin(), m_histogram.end(), 0);
    m_histogramCount = 0;
}
