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

    m_dsdParams.state.input_length = 0;
    m_dsdParams.state.input_offset = 0;

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

}

DSDDecoder::~DSDDecoder()
{
    free(m_dsdParams.state.output_samples);
    free(m_dsdParams.state.output_buffer);
}

void DSDDecoder::setInBuffer(const short *inBuffer)
{
    m_dsdParams.state.input_samples = inBuffer;
}

void DSDDecoder::pushSamples(int nbSamples)
{
    m_dsdParams.state.input_offset = 0;
    m_dsdParams.state.input_length = nbSamples;

    if (pthread_cond_signal(&m_dsdParams.state.input_ready)) {
      printf("DSDDecoder::pushSamples: Unable to signal input ready");
    }
}

void DSDDecoder::start()
{
    qDebug("DSDDecoder::start: starting");
    m_dsdParams.state.dsd_running = 1;

    if (pthread_create(&m_dsdParams.state.dsd_thread, NULL, &run_dsd, &m_dsdParams))
    {
        qCritical("DSDDecoder::start: Unable to spawn thread");
        m_dsdParams.state.dsd_running = 0;
    }

    qDebug("DSDDecoder::start: started");
}

void DSDDecoder::stop()
{
    if (m_dsdParams.state.dsd_running)
    {
        qDebug("DSDDecoder::stop: stopping");
        m_dsdParams.state.dsd_running = 0;
        char *b;

        if (pthread_cond_signal(&m_dsdParams.state.input_ready)) {
          printf("DSDDecoder::pushSamples: Unable to signal input ready");
        }

//        if (pthread_join(m_dsdParams.state.dsd_thread, (void**) &b)) {
//            qCritical("DSDDecoder::stop: cannot join dsd thread");
//        }

        qDebug("DSDDecoder::stop: stopped");
    }
    else
    {
        qDebug("DSDDecoder::stop: not running");
    }
}

void* DSDDecoder::run_dsd(void *arg)
{
  dsd_params *params = (dsd_params *) arg;
  liveScanner (&params->opts, &params->state);
  return NULL;
}




