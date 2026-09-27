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

#ifndef INCLUDE_FEATURE_DENOISER_NVIDIAAUDIOEFFECTS_H_
#define INCLUDE_FEATURE_DENOISER_NVIDIAAUDIOEFFECTS_H_

#include <QString>

// Loads the user-installed NVIDIA Audio Effects redistributable at runtime.
// No NVIDIA headers or binaries are required.
class NvidiaAudioEffects
{
public:
    NvidiaAudioEffects();
    ~NvidiaAudioEffects();

    bool initialize(QString& error, float intensityRatio, bool enableVad);
    void shutdown();
    bool process(const float *input, float *output, QString& error);

private:
    struct Impl;
    Impl *m_impl;
};

#endif
