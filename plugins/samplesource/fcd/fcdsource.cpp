/* (C)2015 John Greb
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public Licence version 3.
 */

#include "fcdthread.h"

bool FCDThread::OpenSource(const char* cardname)
{
	//fcd_rate = FCDPP_RATE;
	//fcd_channels =2;
	//fcd_format = SND_PCM_SFMT_U16_LE;
	fcd_stream = SND_PCM_STREAM_PLAYBACK;

	if (fcd_handle)
		return false;
	if ( snd_pcm_open( &fcd_handle, cardname, fcd_stream, 1 ) < 0 )
		return false;

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

void FCDThread::work(int n_items)
{
	int l;
	SampleVector::iterator it;
	void *out;

	it = m_convertBuffer.begin();
	out = (void *)&it[0];
	l = snd_pcm_readi(fcd_handle, out, (snd_pcm_uframes_t)n_items);
	if (l > 0)
		m_sampleFifo->write(it, it + l);
}


