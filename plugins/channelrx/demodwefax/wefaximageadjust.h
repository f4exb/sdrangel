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

#ifndef INCLUDE_WEFAXIMAGEADJUST_H
#define INCLUDE_WEFAXIMAGEADJUST_H

#include <QImage>

struct WefaxDemodSettings;

/**
 * The user's image adjustments: horizontal alignment, manual slant
 * correction, contrast, inversion, threshold and rotation. Shared by the
 * display and the saved PNG so both show the same image. Zoom is display
 * only and is not applied here.
 */
class WefaxImageAdjust
{
public:
    // True when any adjustment would change the image.
    static bool active(const WefaxDemodSettings& settings);
    // Returns a Grayscale8 copy of image with the adjustments applied.
    static QImage apply(const QImage& image, const WefaxDemodSettings& settings);
};

#endif // INCLUDE_WEFAXIMAGEADJUST_H
