///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "audiocompressor.h"

AudioCompressor::AudioCompressor()
{
    fillLUT2();
}

AudioCompressor::~AudioCompressor()
{}

void AudioCompressor::fillLUT()
{
    for (int i=0; i<8192; i++) {
        m_lut[i] = (24576/8192)*i;
    }

    for (int i=8192; i<2*8192; i++) {
        m_lut[i] = 24576 + 0.5f*(i-8192);
    }

    for (int i=2*8192; i<3*8192; i++) {
        m_lut[i] = 24576 + 4096 + 0.25f*(i-2*8192);
    }

    for (int i=3*8192; i<4*8192; i++) {
        m_lut[i] = 24576 + 4096 + 2048 + 0.125f*(i-3*8192);
    }
}

void AudioCompressor::fillLUT2()
{
    for (int i=0; i<4096; i++) {
        m_lut[i] = (24576/4096)*i;
    }

    for (int i=4096; i<2*4096; i++) {
        m_lut[i] = 24576 + 0.5f*(i-4096);
    }

    for (int i=2*4096; i<3*4096; i++) {
        m_lut[i] = 24576 + 2048 + 0.25f*(i-2*4096);
    }

    for (int i=3*4096; i<4*4096; i++) {
        m_lut[i] = 24576 + 2048 + 1024 + 0.125f*(i-3*4096);
    }

    for (int i=4*4096; i<5*4096; i++) {
        m_lut[i] = 24576 + 2048 + 1024 + 512 + 0.0625f*(i-4*4096);
    }

    for (int i=5*4096; i<6*4096; i++) {
        m_lut[i] = 24576 + 2048 + 1024 + 512 + 256 + 0.03125f*(i-5*4096);
    }

    for (int i=6*4096; i<7*4096; i++) {
        m_lut[i] = 24576 + 2048 + 1024 + 512 + 256 + 128 + 0.015625f*(i-6*4096);
    }

    for (int i=7*4096; i<8*4096; i++) {
        m_lut[i] = 24576 + 2048 + 1024 + 512 + 256 + 128 + 64 + 0.0078125f*(i-7*4096);
    }
}

int16_t AudioCompressor::compress(int16_t sample)
{
    int16_t sign = sample < 0 ? -1 : 1;
    int16_t abs = sample < 0 ? -sample : sample;
    return sign * m_lut[abs];
}
