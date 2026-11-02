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

#include <functional>

enum class UastcTextureSemantic { Color, Data, Normal };

// Writes the narrow KTX2 subset needed by this converter: one non-array,
// non-cubemap 2D UASTC image with an explicit mip chain. The embedded encoder
// uses a compact subset of UASTC modes for each 4x4 tile, avoiding a Basis
// Universal or KTX-Software runtime dependency.
bool writeUastcKtx2(const QVector<QImage> &levels, const QString &outputFile,
                    bool srgb, UastcTextureSemantic semantic, QString *error,
                    const std::function<bool()> &isCancelled = {},
                    bool zstdSupercompression = false);
