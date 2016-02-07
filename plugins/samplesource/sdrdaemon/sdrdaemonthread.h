///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_SDRDAEMONTHREAD_H
#define INCLUDE_SDRDAEMONTHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QTimer>
#include <QHostAddress>

#include <iostream>
#include <cstdlib>
#include "dsp/samplefifo.h"
#include "dsp/inthalfbandfilter.h"
#include "sdrdaemonbuffer.h"

#define SDRDAEMON_THROTTLE_MS 50

class QUdpSocket;

class SDRdaemonThread : public QThread {
	Q_OBJECT

public:
	SDRdaemonThread(SampleFifo* sampleFifo, QObject* parent = NULL);
	~SDRdaemonThread();

	void startWork();
	void stopWork();
	void updateLink(const QString& address, quint16 port);
	bool isRunning() const { return m_running; }
	std::size_t getSamplesCount() const { return m_samplesCount; }

	void connectTimer(const QTimer& timer);

public slots:
	void dataReadyRead();

private:
	QMutex m_startWaitMutex;
	QWaitCondition m_startWaiter;
	bool m_running;

	QUdpSocket *m_dataSocket;
	QHostAddress m_dataAddress;
	quint16 m_dataPort;
	bool m_dataConnected;
	quint8 *m_buf;
	char *m_udpBuf;
	std::size_t m_bufsize;
	std::size_t m_chunksize;
	SampleFifo* m_sampleFifo;
	std::size_t m_samplesCount;

	SDRdaemonBuffer m_sdrDaemonBuffer;

	uint32_t m_samplerate;
	static const int m_rateDivider;
	static const int m_udpPayloadSize;

	void setSamplerate(uint32_t samplerate);
	void run();
private slots:
	void tick();
};

#endif // INCLUDE_SDRDAEMONTHREAD_H
