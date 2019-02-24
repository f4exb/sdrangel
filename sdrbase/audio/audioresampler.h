///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
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

#ifndef SDRBASE_AUDIO_AUDIORESAMPLER_H_
#define SDRBASE_AUDIO_AUDIORESAMPLER_H_

#include "dsp/dsptypes.h"
#include "audiofilter.h"

class AudioResampler
{
public:
    AudioResampler();
    ~AudioResampler();

    void setDecimation(uint32_t decimation);
    void setAudioFilters(int srHigh, int srLow, int fcLow, int fcHigh);
    bool downSample(qint16 sampleIn, qint16& sampleOut);
    qint16 upSample(qint16 sampleIn, bool& consumed);

private:
    AudioFilter m_audioFilter;
    uint32_t m_decimation;
    uint32_t m_decimationCount;
};

#endif /* SDRBASE_AUDIO_AUDIORESAMPLER_H_ */
