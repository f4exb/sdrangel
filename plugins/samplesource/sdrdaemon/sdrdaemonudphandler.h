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

#ifndef PLUGINS_SAMPLESOURCE_SDRDAEMON_SDRDAEMONUDPHANDLER_H_
#define PLUGINS_SAMPLESOURCE_SDRDAEMON_SDRDAEMONUDPHANDLER_H_

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include <QMutex>
#include "sdrdaemonbuffer.h"

#define SDRDAEMON_THROTTLE_MS 50

class SampleFifo;
class MessageQueue;
class QTimer;

class SDRdaemonUDPHandler : public QObject
{
	Q_OBJECT
public:
	SDRdaemonUDPHandler(SampleFifo* sampleFifo, MessageQueue *outputMessageQueueToGUI);
	~SDRdaemonUDPHandler();
	void connectTimer(const QTimer* timer);
	void start();
	void stop();
	void configureUDPLink(const QString& address, quint16 port);

public slots:
	void dataReadyRead();

private:
	SDRdaemonBuffer m_sdrDaemonBuffer;
	QUdpSocket *m_dataSocket;
	QHostAddress m_dataAddress;
	quint16 m_dataPort;
	bool m_dataConnected;
	char *m_udpBuf;
	qint64 m_udpReadBytes;
	std::size_t m_chunksize;
	SampleFifo *m_sampleFifo;
	uint32_t m_samplerate;
	uint32_t m_centerFrequency;
	uint32_t m_tv_sec;
	uint32_t m_tv_usec;
	MessageQueue *m_outputMessageQueueToGUI;
	uint32_t m_tickCount;
	std::size_t m_samplesCount;
	const QTimer *m_timer;

	static const int m_rateDivider;

	void setSamplerate(uint32_t samplerate);
	void processData();

private slots:
	void tick();
};



#endif /* PLUGINS_SAMPLESOURCE_SDRDAEMON_SDRDAEMONUDPHANDLER_H_ */
