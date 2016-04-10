///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
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

#ifndef PLUGINS_CHANNEL_DEMODDSD_DSDDECODER_H_
#define PLUGINS_CHANNEL_DEMODDSD_DSDDECODER_H_

#include "dsd_decoder.h"

class AudioFifo;

class DSDDecoder
{
public:
    DSDDecoder();
    ~DSDDecoder();

    void pushSamples(const short *samples, int nbSamples); // Push this amount of samples to the DSD decoder thread
    void popAudioSamples(AudioFifo *audioFifo, bool audioMute);

private:
    DSDplus::DSDDecoder m_decoder;
    short *m_zeroBuffer;
};

#endif /* PLUGINS_CHANNEL_DEMODDSD_DSDDECODER_H_ */
