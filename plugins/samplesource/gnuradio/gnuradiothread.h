///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2013 by Dimitri Stolnikov <horiz0n@gmx.net>                     //
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

#ifndef INCLUDE_GNURADIOTHREAD_H
#define INCLUDE_GNURADIOTHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include <gnuradio/top_block.h>
#include <osmosdr/source.h>

class SampleFifo;

class GnuradioThread : public QThread {
	Q_OBJECT

public:
	GnuradioThread(QString args, SampleFifo* sampleFifo, QObject* parent = NULL);
	~GnuradioThread();

	void startWork();
	void stopWork();

	osmosdr::source::sptr radio() { return m_src; }

private:
#pragma pack(push, 1)
	struct Sample {
		qint16 i;
		qint16 q;
	};
#pragma pack(pop)

	QMutex m_startWaitMutex;
	QWaitCondition m_startWaiter;
	bool m_running;

	QString m_args;
	SampleFifo* m_sampleFifo;

	gr::top_block_sptr m_top;
	osmosdr::source::sptr m_src;

	void run();
};

#endif // INCLUDE_GNURADIOTHREAD_H
