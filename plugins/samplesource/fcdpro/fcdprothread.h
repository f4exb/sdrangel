///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FCDPROTHREAD_H
#define INCLUDE_FCDPROTHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include "dsp/inthalfbandfilter.h"
#include <alsa/asoundlib.h>
#include "dsp/samplesinkfifo.h"

class FCDProThread : public QThread {
	Q_OBJECT

public:
	FCDProThread(SampleSinkFifo* sampleFifo, QObject* parent = NULL);
	~FCDProThread();

	void startWork();
	void stopWork();
	bool OpenSource(const char *filename);
	void CloseSource();

private:
	snd_pcm_t* fcd_handle;

	QMutex m_startWaitMutex;
	QWaitCondition m_startWaiter;
	bool m_running;

	SampleVector m_convertBuffer;
	SampleSinkFifo* m_sampleFifo;

	void run();
	int work(int n_items);
};
#endif // INCLUDE_FCDPROTHREAD_H
