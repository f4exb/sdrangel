///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef SDRBASE_AUDIO_AUDIOOPUS_H_
#define SDRBASE_AUDIO_AUDIOOPUS_H_

#include <stdint.h>
#include <QMutex>
#include "export.h"

struct OpusEncoder;

class SDRBASE_API AudioOpus
{
public:
    AudioOpus();
    ~AudioOpus();

    void setEncoder(int32_t fs, int nChannels);
    int encode(int frameSize, int16_t *in, uint8_t *out);

    static const int m_bitrate = 64000; //!< Fixed 64kb/s bitrate (8kB/s)
    static const int m_maxPacketSize = 3*1276;

private:
    OpusEncoder *m_encoderState;
    bool m_encoderOK;
    QMutex m_mutex;
};

#endif /* SDRBASE_AUDIO_AUDIOOPUS_H_ */
