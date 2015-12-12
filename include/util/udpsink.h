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

#ifndef INCLUDE_UTIL_UDPSINK_H_
#define INCLUDE_UTIL_UDPSINK_H_

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>

template<typename T>
class UDPSink
{
public:
	UDPSink(QObject *parent, unsigned int udpSize, unsigned int port) :
		m_udpSize(udpSize),
		m_address(QHostAddress::LocalHost),
		m_port(port),
		m_sampleBufferIndex(0)
	{
		m_sampleBuffer = new T[m_udpSize];
		m_socket = new QUdpSocket(parent);
	}

	UDPSink (QObject *parent, unsigned int udpSize, QHostAddress& address, unsigned int port) :
		m_udpSize(udpSize),
		m_address(address),
		m_port(port),
		m_sampleBufferIndex(0)
	{
		m_sampleBuffer = new T[m_udpSize];
		m_socket = new QUdpSocket(parent);
	}

	~UDPSink()
	{
		delete[] m_sampleBuffer;
		delete m_socket;
	}

	void write(T sample)
	{
		m_sampleBuffer[m_sampleBufferIndex] = sample;

		if (m_sampleBufferIndex < m_udpSize)
		{
			m_sampleBufferIndex++;
		}
		else
		{
			m_socket->writeDatagram((const char*)&m_sampleBuffer[0], (qint64 ) (m_udpSize * sizeof(T)), m_address, m_port);
			m_sampleBufferIndex = 0;
		}
	}

private:
	unsigned int m_udpSize;
	QHostAddress m_address;
	unsigned int m_port;
	QUdpSocket *m_socket;
	T *m_sampleBuffer;;
	int m_sampleBufferIndex;
};



#endif /* INCLUDE_UTIL_UDPSINK_H_ */
