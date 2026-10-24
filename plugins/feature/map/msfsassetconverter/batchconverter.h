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
#include <QStringList>

#include <functional>

struct MSFSBatchConversionSummary {
    int m_discovered = 0;
    int m_converted = 0;
    int m_lowDetailModels = 0;      // Converted aircraft that also got a _lod1 model
    int m_failed = 0;
    int m_reusedTextures = 0;
    int m_synthesizedTextures = 0;
    int m_skippedFallbackDirectories = 0;
    int m_removedTextures = 0;
    int m_downscaledTextures = 0;
    qint64 m_removedTextureBytes = 0;
    int m_workerCount = 0;
    bool m_cancelled = false;
    QStringList m_errors;
};

using MSFSBatchProgress = std::function<void(int current, int total, const QString& message)>;
using MSFSBatchCancelled = std::function<bool()>;

// Recursively converts every distinct model/texture variant in each aircraft.cfg
// below aircraftDirectory. Models retain the input directory layout and textures
// use a shared content-addressed cache below outputDirectory.
bool convertMSFSAircraftDirectory(
    const QString& aircraftDirectory,
    const QString& outputDirectory,
    MSFSBatchConversionSummary *summary,
    QString *error,
    const MSFSBatchProgress& progress = {},
    const MSFSBatchCancelled& isCancelled = {},
    int maxThreadCount = 0,
    int maximumTextureSize = 2048,
    // A low detail model is only drawn while the aircraft is small on screen - measured at
    // 33-49 px across, against a threshold of 8% of the window height - so its liveries
    // carry far more detail than can ever be shown. Rendered side by side at 50, 92 and
    // 200 px, 2048 and 512 are indistinguishable. 512 rather than 256 leaves headroom for
    // a 4K window, where the threshold is nearer 173 px, and for a raised 3D min pixel size
    int lowDetailMaximumTextureSize = 512);
