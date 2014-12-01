/* (C)2015 John Greb
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public Licence version 3.
 */

#include "fcdthread.h"

bool FCDThread::OpenSource(const char* cardname)
{
	snd_pcm_hw_params_t* params;
	//fcd_rate = FCDPP_RATE;
	//fcd_channels =2;
	//fcd_format = SND_PCM_SFMT_U16_LE;
	snd_pcm_stream_t fcd_stream = SND_PCM_STREAM_CAPTURE;

	if (fcd_handle)
		return false;
	if ( snd_pcm_open( &fcd_handle, cardname, fcd_stream, 0 ) < 0 )
		return false;

	snd_pcm_hw_params_alloca(&params);
	if ( snd_pcm_hw_params_any(fcd_handle, params) < 0 )
		qCritical("Funcube Dongle read settings failed");
	else if ( snd_pcm_hw_params(fcd_handle, params) < 0 )
		qCritical("Funcube Dongle write settings failed");
	// TODO: check actual samplerate, may be crippled firmware

	if ( snd_pcm_start(fcd_handle) < 0 )
		qCritical("Funcube Dongle stream start failed");
	else	qDebug("Funcube stream started");
	return true;
}

void FCDThread::CloseSource()
{
	if (fcd_handle)
		snd_pcm_close( fcd_handle );
	fcd_handle = NULL;
}

void FCDThread::set_center_freq(double freq)
{
	//TODO
}

int FCDThread::work(int n_items)
{
	int l;
	SampleVector::iterator it;
	void *out;

	it = m_convertBuffer.begin();
	out = (void *)&it[0];
	l = snd_pcm_mmap_readi(fcd_handle, out, (snd_pcm_uframes_t)n_items);
	if (l > 0)
		m_sampleFifo->write(it, it + l);
	return l;
}


