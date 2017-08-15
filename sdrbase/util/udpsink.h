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

#include <stdint.h>
#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>

#include <cassert>

template<typename T>
class UDPSink
{
public:
	UDPSink(QObject *parent, unsigned int udpSize, unsigned int port) :
		m_udpSize(udpSize),
		m_udpSamples(udpSize/sizeof(T)),
		m_address(QHostAddress::LocalHost),
		m_port(port),
		m_sampleBufferIndex(0)
	{
        assert(m_udpSamples > 0);
		m_sampleBuffer = new T[m_udpSamples];
		m_socket = new QUdpSocket(parent);
	}

	UDPSink (QObject *parent, unsigned int udpSize, QHostAddress& address, unsigned int port) :
		m_udpSize(udpSize),
        m_udpSamples(udpSize/sizeof(T)),
		m_address(address),
		m_port(port),
		m_sampleBufferIndex(0)
	{
		assert(m_udpSamples > 0);
		m_sampleBuffer = new T[m_udpSamples];
		m_socket = new QUdpSocket(parent);
	}

	~UDPSink()
	{
		delete[] m_sampleBuffer;
		delete m_socket;
	}

	void setAddress(QString& address) { m_address.setAddress(address); }
	void setPort(unsigned int port) { m_port = port; }

	void write(T sample)
	{
		if (m_sampleBufferIndex < m_udpSamples)
		{
			m_sampleBuffer[m_sampleBufferIndex] = sample;
			m_sampleBufferIndex++;
		}
		else
		{
			m_socket->writeDatagram((const char*)&m_sampleBuffer[0], (qint64 ) m_udpSize, m_address, m_port);
			m_sampleBuffer[0] = sample;
			m_sampleBufferIndex = 1;
		}
	}

private:
	unsigned int m_udpSize;
    unsigned int m_udpSamples;
	QHostAddress m_address;
	unsigned int m_port;
	QUdpSocket *m_socket;
	T *m_sampleBuffer;;
	uint32_t m_sampleBufferIndex;
};



#endif /* INCLUDE_UTIL_UDPSINK_H_ */
