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

#pragma once

#include <QImage>
#include <QString>
#include <QVector>

struct DdsDecodeResult {
    QVector<QImage> m_levels;
    QString m_format;
};

// Decodes every mip of the DDS variants used by the sample MSFS aircraft.
// When normalMap is true, signed BC5 XY values are converted to an RGB tangent
// space normal. flipNormalY changes the DirectX convention to glTF/OpenGL.
bool decodeDds(const QString &fileName, bool normalMap, bool flipNormalY,
               DdsDecodeResult *result, QString *error);
