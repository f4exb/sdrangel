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

#ifndef INCLUDE_ATVNOISEREDUCTION_H
#define INCLUDE_ATVNOISEREDUCTION_H

#include <vector>

/**
 * Motion adaptive temporal noise reduction.
 *
 * Each pixel is blended with the same pixel of the previous frame (recursively), except
 * where its luminance has changed by more than the noise, which is taken to be motion. The
 * noise level is estimated each frame from the lower quartile of the frame to frame
 * difference, so the threshold follows the signal as long as some of the picture is still.
 */
class ATVNoiseReduction
{
public:
    ATVNoiseReduction();

    void configure(int width, int height); //!< Pixels per row and rows; clears the frame store
    void setStrength(int strength);        //!< 0 (off) to 100%
    bool isEnabled() const { return m_strength > 0.0f; }

    /** Filter one row in place. Pixels are packed 0x00BBGGRR */
    void processRow(int row, unsigned int *rgb);
    void endFrame(); //!< Update the noise estimate from the frame just finished

    float getThreshold() const { return m_threshold; } //!< Motion threshold, 8-bit levels

private:
    int m_width;
    int m_height;
    float m_strength;                 //!< Maximum blend with the previous frame
    std::vector<float> m_frame;       //!< Previous output, 3 floats per pixel
    std::vector<bool> m_rowValid;
    std::vector<float> m_differenceSum; //!< Running sum of the luminance difference along the current row
    std::vector<int> m_histogram;     //!< Of luminance differences over the frame, for the noise estimate
    int m_histogramCount;
    float m_threshold;
    int m_window;                     //!< Pixels the difference is averaged over
};

#endif // INCLUDE_ATVNOISEREDUCTION_H
