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

#include "dsd.h"

class AudioFifo;

class DSDDecoder
{
public:
    DSDDecoder();
    ~DSDDecoder();

    void setInBuffer(const short *inBuffer);
    void pushSample(short sample);
    void pushSamples(int nbSamples); // Push this amount of samples to the DSD decoder thread
    void popAudioSamples(AudioFifo *audioFifo, bool audioMute);

    void start();
    void stop();

private:
    typedef struct
    {
      dsd_opts opts;
      dsd_state state;
    } dsd_params;

    static void* run_dsd(void *arg);

    dsd_params m_dsdParams;
    short *m_zeroBuffer;
    int m_lastNbSamples;
};

#endif /* PLUGINS_CHANNEL_DEMODDSD_DSDDECODER_H_ */
