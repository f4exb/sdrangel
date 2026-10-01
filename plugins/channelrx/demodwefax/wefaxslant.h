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

#ifndef INCLUDE_WEFAXSLANT_H
#define INCLUDE_WEFAXSLANT_H

#include <QImage>

/**
 * Automatic slant correction for a received WEFAX raster.
 *
 * A line-clock error makes every row start slightly early or late, so content
 * drifts horizontally by a constant number of pixels per row. Charts contain
 * straight vertical lines (frames, and meridians on Mercator projections). The
 * drift is measured as the shear that makes the strongest pair of parallel
 * near-vertical lines straightest, and is removed by re-rastering the pixel
 * stream with the corrected line length.
 *
 * Without phasing, the line start is unknown. Stations that transmit a blank
 * (white) dead sector at the start of every line can be aligned by moving the
 * cut to the start of that band.
 */
class WefaxSlant
{
public:
    struct Estimate
    {
        bool valid = false;
        double driftPerRow = 0.0;   // Horizontal drift of vertical lines in pixels per row
        double coverage = 0.0;      // Lower of the line's coverage of the top and bottom halves
    };

    static constexpr int MinRows = 150;
    static constexpr double MaxPpm = 200.0;
    static constexpr double MinCoverage = 0.4;

    // image is Grayscale8 with black lines on white; only the first rows are used.
    static Estimate estimate(const QImage& image, int rows);
    // Returns the raster with the drift removed and each row starting
    // columnOffset pixels later in the stream. Rows remain image.width() wide.
    static QImage correct(const QImage& image, int rows, double driftPerRow, double columnOffset = 0.0);
    // Column at which the white dead sector that some stations send at the
    // start of every line begins, or -1 if there is no clearly blank band.
    // The image should already be slant corrected.
    static int deadSectorStart(const QImage& image, int rows);
    static double driftToPpm(double driftPerRow, int width) { return 1.0e6 * driftPerRow / width; }
};

#endif // INCLUDE_WEFAXSLANT_H
