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

#include "wefaximageadjust.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <QTransform>

#include "wefaxdemodsettings.h"
#include "wefaxslant.h"

namespace
{

bool geometryActive(const WefaxDemodSettings& settings)
{
    return (settings.m_horizontalAlignment != 0) || (std::abs(settings.m_displaySlantCorrectionPpm) > 0.001);
}

bool toneActive(const WefaxDemodSettings& settings)
{
    return settings.m_displayInverted || (settings.m_displayContrast != 0) || (settings.m_displayThreshold >= 0);
}

} // namespace

bool WefaxImageAdjust::active(const WefaxDemodSettings& settings)
{
    return geometryActive(settings) || toneActive(settings) || (settings.m_displayRotation != 0);
}

QImage WefaxImageAdjust::apply(const QImage& image, const WefaxDemodSettings& settings)
{
    QImage adjusted = image.convertToFormat(QImage::Format_Grayscale8);
    if (adjusted.isNull()) {
        return adjusted;
    }
    const int width = adjusted.width();

    // Alignment and manual slant are a line-start offset and a line-clock
    // error, so they use the same stream re-cut as automatic correction:
    // pixels moved past one edge continue on the adjacent line. Positive
    // alignment moves content right, positive slant moves later rows left.
    // WefaxSlant::correct shears about the centre row; the offset keeps the
    // first row fixed instead, so the view does not move as rows arrive.
    if (geometryActive(settings))
    {
        const int height = adjusted.height();
        const double drift = width * settings.m_displaySlantCorrectionPpm / 1.0e6;
        const double offset = -settings.m_horizontalAlignment + 0.5 * drift * height;
        adjusted = WefaxSlant::correct(adjusted, height, drift, offset);
    }

    // Contrast about mid-grey, then inversion, then the black/white threshold.
    if (toneActive(settings))
    {
        const double contrast = settings.m_displayContrast / 100.0;
        std::array<uchar, 256> lut;
        for (int source = 0; source < 256; ++source)
        {
            const double value = 128.0 + (source - 128.0) * (1.0 + contrast);
            int pixel = std::clamp(static_cast<int>(std::lround(value)), 0, 255);
            if (settings.m_displayInverted) {
                pixel = 255 - pixel;
            }
            if (settings.m_displayThreshold >= 0) {
                pixel = pixel >= settings.m_displayThreshold ? 255 : 0;
            }
            lut[source] = static_cast<uchar>(pixel);
        }

        for (int y = 0; y < adjusted.height(); ++y)
        {
            uchar *row = adjusted.scanLine(y);
            for (int x = 0; x < width; ++x) {
                row[x] = lut[row[x]];
            }
        }
    }

    // Rotation last, so alignment and slant act along the received lines.
    if (settings.m_displayRotation != 0) {
        adjusted = adjusted.transformed(QTransform().rotate(settings.m_displayRotation));
    }

    return adjusted;
}
