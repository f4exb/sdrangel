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

#include <algorithm>
#include <cmath>

#include <QColor>

#include "util/flightpathpalette.h"

const QVariant FlightPathPalette::m_rainbowPalette[m_colors] = {
    QVariant(QColor(0xff, 0x00, 0x00)),
    QVariant(QColor(0xff, 0x7f, 0x00)),
    QVariant(QColor(0xff, 0xff, 0x00)),
    QVariant(QColor(0xf7, 0xff, 0x00)),
    QVariant(QColor(0x00, 0xff, 0x00)),
    QVariant(QColor(0x00, 0xff, 0x7f)),
    QVariant(QColor(0x00, 0xff, 0xff)),
    QVariant(QColor(0x00, 0x7f, 0xff)),
};

const QVariant FlightPathPalette::m_pastelPalette[m_colors] = {
    QVariant(QColor(0xff, 0xad, 0xad)),
    QVariant(QColor(0xff, 0xd6, 0xa5)),
    QVariant(QColor(0xfd, 0xff, 0xb6)),
    QVariant(QColor(0xca, 0xff, 0xbf)),
    QVariant(QColor(0x9b, 0xf6, 0xff)),
    QVariant(QColor(0xa0, 0xc4, 0xff)),
    QVariant(QColor(0xbd, 0xb2, 0xff)),
    QVariant(QColor(0xff, 0xc6, 0xff)),
};

const QVariant FlightPathPalette::m_spectralPalette[m_colors] = {
    QVariant(QColor(0xd5, 0x3e, 0x4f)),
    QVariant(QColor(0xf4, 0x6d, 0x43)),
    QVariant(QColor(0xfd, 0xae, 0x61)),
    QVariant(QColor(0xfe, 0xe0, 0x8b)),
    QVariant(QColor(0xe6, 0xf5, 0x98)),
    QVariant(QColor(0xab, 0xdd, 0xa4)),
    QVariant(QColor(0x66, 0xc2, 0xa5)),
    QVariant(QColor(0x32, 0x88, 0xbd)),
};

const QVariant FlightPathPalette::m_bluePalette[m_colors] = {
    QVariant(QColor(0xde, 0xeb, 0xf7)),
    QVariant(QColor(0xc6, 0xdb, 0xef)),
    QVariant(QColor(0x9e, 0xca, 0xe1)),
    QVariant(QColor(0x6b, 0xae, 0xd6)),
    QVariant(QColor(0x42, 0x92, 0xc6)),
    QVariant(QColor(0x21, 0x71, 0xb5)),
    QVariant(QColor(0x08, 0x51, 0x9c)),
    QVariant(QColor(0x08, 0x30, 0x6b)),
};

const QVariant FlightPathPalette::m_purplePalette[m_colors] = {
    QVariant(QColor(0xcc, 0xaf, 0xf2)),
    QVariant(QColor(0xb6, 0x99, 0xe0)),
    QVariant(QColor(0xa0, 0x84, 0xcf)),
    QVariant(QColor(0x8a, 0x6f, 0xbd)),
    QVariant(QColor(0x76, 0x5a, 0xac)),
    QVariant(QColor(0x62, 0x45, 0x9a)),
    QVariant(QColor(0x4f, 0x30, 0x89)),
    QVariant(QColor(0x3e, 0x18, 0x78)),
};

const QVariant FlightPathPalette::m_greyPalette[m_colors] = {
    QVariant(QColor(0x80, 0x80, 0x80)),
    QVariant(QColor(0x80, 0x80, 0x80)),
    QVariant(QColor(0x80, 0x80, 0x80)),
    QVariant(QColor(0x80, 0x80, 0x80)),
    QVariant(QColor(0x80, 0x80, 0x80)),
    QVariant(QColor(0x80, 0x80, 0x80)),
    QVariant(QColor(0x80, 0x80, 0x80)),
    QVariant(QColor(0x80, 0x80, 0x80)),
};

const QHash<QString, const QVariant *> FlightPathPalette::m_palettes = {
    {"Rainbow", &FlightPathPalette::m_rainbowPalette[0]},
    {"Pastel", &FlightPathPalette::m_pastelPalette[0]},
    {"Spectral", &FlightPathPalette::m_spectralPalette[0]},
    {"Blues", &FlightPathPalette::m_bluePalette[0]},
    {"Purples", &FlightPathPalette::m_purplePalette[0]},
    {"Grey", &FlightPathPalette::m_greyPalette[0]},
};


const QStringList& FlightPathPalette::getPaletteNames()
{
    // Not m_palettes.keys(), so the order is always the same
    static const QStringList names = {"Rainbow", "Pastel", "Spectral", "Blues", "Purples", "Grey"};

    return names;
}

const QVariant *FlightPathPalette::getPalette(const QString& name)
{
    if (m_palettes.contains(name)) {
        return m_palettes.value(name);
    } else {
        return m_rainbowPalette;
    }
}

int FlightPathPalette::getColorIndex(float altitudeFt)
{
    // An object with no altitude gets the first colour, rather than an index from a NaN
    if (std::isnan(altitudeFt) || (altitudeFt <= 0.0f)) {
        return 0;
    }

    return std::min(m_colors - 1, (int) (altitudeFt / m_altitudeStep));
}
