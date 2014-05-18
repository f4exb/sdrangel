///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

#ifndef INCLUDE_OSMOSDRTHREAD_H
#define INCLUDE_OSMOSDRTHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <osmosdr.h>

class SampleFifo;

class OsmoSDRThread : public QThread {
	Q_OBJECT

public:
	OsmoSDRThread(osmosdr_dev_t* dev, SampleFifo* sampleFifo, QObject* parent = NULL);
	~OsmoSDRThread();

	void startWork();
	void stopWork();

private:
#pragma pack(push, 1)
	struct Sample {
		qint16 i;
		qint16 q;
	};
#pragma pack(pop)

	qint16 m_nextI;
	qint16 m_nextQ;
	quint64 m_samplePos;

	QMutex m_startWaitMutex;
	QWaitCondition m_startWaiter;
	bool m_running;
	FILE* m_f;

	osmosdr_dev_t* m_dev;
	SampleFifo* m_sampleFifo;

	void run();

	void checkData(const quint8* buf, qint32 len);
	void callback(const quint8* buf, qint32 len);

	static void callbackHelper(unsigned char* buf, uint32_t len, void* ctx);
};

#endif // INCLUDE_OSMOSDRTHREAD_H
