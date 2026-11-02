///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                      //
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

#ifndef INCLUDE_FLIGHTPATHPALETTE_H
#define INCLUDE_FLIGHTPATHPALETTE_H

#include <QHash>
#include <QStringList>
#include <QVariant>

#include "export.h"

// Palettes for colouring flight paths according to the altitude of an aircraft.
// Each palette has one colour per m_altitudeStep feet, with everything at or above
// m_colors*m_altitudeStep drawn in the last colour.
// Colours are QVariants containing QColors, so they can be passed to QML.
// Used by the ADS-B demodulator and the Map feature, so both draw tracks the same way.
class SDRBASE_API FlightPathPalette
{
public:
    static const QStringList& getPaletteNames();
    static const QVariant *getPalette(const QString& name); // Falls back to Rainbow if the name isn't known
    static int getColorIndex(float altitudeFt);

    static const int m_colors = 8;
    static const int m_altitudeStep = 5000; // In feet

private:
    static const QVariant m_rainbowPalette[m_colors];
    static const QVariant m_pastelPalette[m_colors];
    static const QVariant m_spectralPalette[m_colors];
    static const QVariant m_bluePalette[m_colors];
    static const QVariant m_purplePalette[m_colors];
    static const QVariant m_greyPalette[m_colors];

    static const QHash<QString, const QVariant *> m_palettes;
};

#endif // INCLUDE_FLIGHTPATHPALETTE_H
