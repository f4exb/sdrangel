///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_UTIL_FITS_H
#define INCLUDE_UTIL_FITS_H

#include <QString>
#include <QByteArray>

#include "export.h"

// Flexible Image Transport System
// Specification: https://fits.gsfc.nasa.gov/standard40/fits_standard40aa-le.pdf
// This just implements the minimum functionality used by the files
// used by the StarTracker plugin
class SDRBASE_API FITS {

    bool m_valid;
    int m_width;                // In pixels
    int m_height;               // In pixels
    int m_bitsPerPixel;         // BITPIX=-32 means single precision floating point
    int m_bytesPerPixel;
    int m_bzero;
    double m_bscale;
    QString m_buint;            // "K TB" "MILLIK TB" "K"
    float m_uintScale;
    double m_cdelta1;           // How many degrees RA per horizontal pixel
    double m_cdelta2;           // How many degrees Declination per vertical pixel

    int m_dataStart;
    QByteArray m_data;

public:

    FITS(QString resourceName);

    float value(int x, int y) const;
    float scaledValue(int x, int y) const;
    float scaledWrappedValue(int x, int y) const;

    double degreesPerPixelH() const { return m_cdelta1; }
    double degreesPerPixelV() const { return m_cdelta2; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    bool valid() const { return m_valid; }

protected:

    int mod(int a, int b) const;
};

#endif // INCLUDE_UTIL_FITS_H
