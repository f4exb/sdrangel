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

#include <QtGlobal>
#include "dsddecoder.h"
#include "dsd_livescanner.h"

DSDDecoder::DSDDecoder()
{
    initOpts(&m_dsdParams.opts);
    initState(&m_dsdParams.state);

    m_dsdParams.opts.split = 1;
    m_dsdParams.opts.playoffset = 0;
    m_dsdParams.opts.delay = 0;
    m_dsdParams.opts.audio_in_type = 0;
    m_dsdParams.opts.audio_out_type = 0;

    // Initialize with auto-detect:
    m_dsdParams.opts.frame_dstar = 1;
    m_dsdParams.opts.frame_x2tdma = 1;
    m_dsdParams.opts.frame_p25p1 = 1;
    m_dsdParams.opts.frame_nxdn48 = 0;
    m_dsdParams.opts.frame_nxdn96 = 1;
    m_dsdParams.opts.frame_dmr = 1;
    m_dsdParams.opts.frame_provoice = 0;

    m_dsdParams.opts.uvquality = 3; // This is gr-dsd default
    m_dsdParams.opts.verbose = 2;   // This is gr-dsd default
    m_dsdParams.opts.errorbars = 1; // This is gr-dsd default

    // Initialize with auto detection of modulation optimization:
    m_dsdParams.opts.mod_c4fm = 1;
    m_dsdParams.opts.mod_qpsk = 1;
    m_dsdParams.opts.mod_gfsk = 1;
    m_dsdParams.state.rf_mod = 0;

    // Initialize the conditions
    if(pthread_cond_init(&m_dsdParams.state.input_ready, NULL))
    {
        qCritical("DSDDecoder::DSDDecoder: Unable to initialize input condition");
    }
    if(pthread_cond_init(&m_dsdParams.state.output_ready, NULL))
    {
        qCritical("DSDDecoder::DSDDecoder: Unable to initialize output condition");
    }

    m_dsdParams.state.input_length = 0;
    m_dsdParams.state.input_offset = 0;

    // Lock output mutex
    if (pthread_mutex_lock(&m_dsdParams.state.output_mutex))
    {
        qCritical("DSDDecoder::DSDDecoder: Unable to lock output mutex");
    }

    m_dsdParams.state.output_buffer = (short *) malloc(1<<18); // Raw output buffer with single S16LE samples @ 8k (max: 128 kS)
    m_dsdParams.state.output_offset = 0;

    if (m_dsdParams.state.output_buffer == NULL)
    {
        qCritical("DSDDecoder::DSDDecoder: Unable to allocate output raw buffer.");
    }

    m_dsdParams.state.output_samples = (short *) malloc(1<<19); // Audio output buffer with L+R S16LE samples (max: 128 kS)
    m_dsdParams.state.output_length = 1<<19; // the buffer size fixed

    if (m_dsdParams.state.output_samples == NULL)
    {
        qCritical("DSDDecoder::DSDDecoder: Unable to allocate audio L+R buffer.");
    }

    m_dsdThread = pthread_self();      // dummy initialization
    m_dsdParams.state.dsd_running = 0; // wait for start()
}

DSDDecoder::~DSDDecoder()
{
    stop();

    // Unlock output mutex
    if (pthread_mutex_unlock(&m_dsdParams.state.output_mutex))
    {
        qCritical("DSDDecoder::~DSDDecoder: Unable to unlock output mutex");
    }

    free(m_dsdParams.state.output_samples);
    free(m_dsdParams.state.output_buffer);
}

void DSDDecoder::setInBuffer(const short *inBuffer)
{
    m_dsdParams.state.input_samples = inBuffer;
}

void DSDDecoder::pushSamples(int nbSamples)
{
    if (pthread_mutex_lock(&m_dsdParams.state.input_mutex))
    {
        qCritical("DSDDecoder::pushSamples: Unable to lock input mutex");
    }

    m_dsdParams.state.input_length = nbSamples;
    m_dsdParams.state.input_offset = 0;

    if (pthread_cond_signal(&m_dsdParams.state.input_ready))
    {
        qCritical("DSDDecoder::pushSamples: Unable to signal input ready");
    }

    if (pthread_mutex_unlock(&m_dsdParams.state.input_mutex))
    {
        qCritical("DSDDecoder::pushSamples: Unable to unlock input mutex");
    }
}

void DSDDecoder::start()
{
    if (m_dsdParams.state.dsd_running == 1)
    {
        m_dsdParams.state.dsd_running = 0;

        if (pthread_join(m_dsdThread, NULL)) {
            qCritical("DSDDecoder::start: error joining DSD thread. Not starting");
            return;
        }
    }

    m_dsdParams.state.dsd_running = 1;

    if (pthread_create(&m_dsdThread, NULL, &run_dsd, &m_dsdParams))
    {
        qCritical("DSDDecoder::start: Unable to spawn DSD thread");
    }
}

void DSDDecoder::stop()
{
    if (m_dsdParams.state.dsd_running == 1)
    {
        m_dsdParams.state.dsd_running = 0;

        if (pthread_join(m_dsdThread, NULL)) {
            qCritical("DSDDecoder::stop: error joining DSD thread. Not starting");
        }
    }
}

void* DSDDecoder::run_dsd (void *arg)
{
  dsd_params *params = (dsd_params *) arg;
  liveScanner(&params->opts, &params->state);
  return NULL;
}

