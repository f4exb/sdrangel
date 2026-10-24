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

#include <QString>

#include <functional>

struct ConversionOptions {
    QString m_inputGltf;
    QString m_outputDirectory;
    QString m_outputNameSuffix;
    QString m_sharedTextureDirectory;
    bool m_flipDirectXNormalY = true;
    int m_flightSimIndex = 0;
    int m_lodIndex = 0;
    bool m_strictExtensions = false;
#ifdef MSFS_CONVERTER_WITH_ZSTD
    // Lossless, and about 8x smaller for liveries, which are mostly flat colour
    bool m_zstdSupercompression = true;
#else
    bool m_zstdSupercompression = false;
#endif
    std::function<bool()> m_isCancelled;
};

struct ConversionResult {
    QString m_outputGltf;
    int m_convertedAccessors = 0;
    int m_convertedTextures = 0;
    int m_synthesizedTextures = 0;
    int m_reusedTextures = 0;
    int m_skippedFallbackDirectories = 0;
    int m_preservedMipLevels = 0;
    int m_flippedTriangles = 0;
    int m_removedExtras = 0;
};

using ConversionLog = std::function<void(const QString &)>;

bool convertMsfsAsset(const ConversionOptions &options,
                      ConversionResult *result, QString *error,
                      const ConversionLog &log = {});
