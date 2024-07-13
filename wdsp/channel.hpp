/*  channel.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013 Warren Pratt, NR0V
Copyright (C) 2024 Edouard Griffiths, F4EXB Adapted to SDRangel

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

warren@wpratt.com

*/

#ifndef wdsp_channel_h
#define wdsp_channel_h

#include <atomic>

#include <QThread>

#include "export.h"

class WDSP_API Channel
{
public:
    int type;
    bool run;                   // thread running
    int in_rate;                // input samplerate
    int out_rate;               // output samplerate
    int in_size;                // input buffsize (complex samples) in a fexchange() operation
    int dsp_rate;               // sample rate for mainstream dsp processing
    int dsp_size;               // number complex samples processed per buffer in mainstream dsp processing
    int dsp_insize;             // size (complex samples) of the output of the r1 (input) buffer
    int dsp_outsize;            // size (complex samples) of the input of the r2 (output) buffer
    int out_size;               // output buffsize (complex samples) in a fexchange() operation
    int state;                  // 0 for channel OFF; 1 for channel ON
    float tdelayup;
    float tslewup;
    float tdelaydown;
    float tslewdown;
    int bfo;                    // 'block_for_output', block fexchange until output is available
    volatile long flushflag;
    QThread channelThread;
    std::atomic<long> upslew;
    // struct  //io buffers
    // {
    //     IOB pc, pd, pe, pf;     // copies for console calls, dsp, exchange, and flush thread
    //     volatile long ch_upslew;
    // } iob;

    static void create_channel (
        int channel,
        int in_size,
        int dsp_size,
        int input_samplerate,
        int dsp_rate,
        int output_samplerate,
        int type,
        int state,
        float tdelayup,
        float tslewup,
        float tdelaydown,
        float tslewdown,
        int bfo
    );
    static void destroy_channel (int channel);
    static void flush_channel (int channel);
    // static void set_type (int channel, int type);
    // static void SetInputBuffsize (int channel, int in_size);
    // static void SetDSPBuffsize (int channel, int dsp_size);
    // static void SetInputSamplerate  (int channel, int samplerate);
    // static void SetDSPSamplerate (int channel, int samplerate);
    // static void SetOutputSamplerate (int channel, int samplerate);
    // static void SetAllRates (int channel, int in_rate, int dsp_rate, int out_rate);
    // static int SetChannelState (int channel, int state, int dmode);
};

#endif
