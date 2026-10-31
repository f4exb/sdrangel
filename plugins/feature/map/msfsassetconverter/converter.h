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
    QString m_outputFileSuffix;     // After "_cesium", e.g. "_lod1", so the model scan skips it
    QString m_sharedTextureDirectory;
    bool m_flipDirectXNormalY = true;
    int m_flightSimIndex = 0;
    int m_lodIndex = 0;
    bool m_strictExtensions = false;
    // Liveries are authored at up to 4096 square. The KTX2 file is small - UASTC and zstd
    // see to that - but once decoded and mipmapped a 4096 costs about 21 MB of GPU memory
    // however far away the aircraft is, and a sky full of them was spending over a
    // gigabyte on liveries alone. 2048 is indistinguishable from 4096 at any distance the
    // map is used at; 1024 visibly softens small markings like door placards
    int m_maximumTextureSize = 2048;    // 0 for no limit
    // MSFS bakes an animation at a keyframe per simulation frame, so a gear leg that is a
    // plain ramp arrives with 100 or 200 of them. Cesium unpacks each keyframe into a
    // Quaternion object, and does it per model instance rather than per file, so a sky
    // full of aircraft had half a million of them live - and a major GC tracing that many
    // is what the map's a-second-and-a-third stutters were. Keyframes are dropped while
    // the animation still reproduces to within this much per component: about a quarter
    // of a degree of rotation, or two millimetres of travel, at the default
    double m_animationTolerance = 0.002;    // 0 to keep every keyframe
    // For a level of detail that is never drawn big enough to show an animation at all,
    // the keyframes go entirely rather than being thinned. The names of the nodes the
    // animations drove are kept in the glTF's extras, because map3d.html hides exactly
    // those nodes in far mode and finds them by reading the animations
    bool m_dropAnimations = false;
    // And the parts those animations drove, with them. A low detail model's gear and flaps
    // only ever showed in their rest pose once the animations went, and beyond
    // farModelDistance map3d.html was hiding them anyway, so the geometry is dead weight:
    // fewer primitives to draw and fewer nodes for Cesium to walk every frame. Needs
    // m_dropAnimations, and leaves the names out of extras since nothing is left to hide
    bool m_dropAnimatedParts = false;
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
    int m_downscaledTextures = 0;
    int m_skippedFallbackDirectories = 0;
    int m_preservedMipLevels = 0;
    int m_flippedTriangles = 0;
    int m_removedExtras = 0;
};

using ConversionLog = std::function<void(const QString &)>;

bool convertMsfsAsset(const ConversionOptions &options,
                      ConversionResult *result, QString *error,
                      const ConversionLog &log = {});
