///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
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

template<typename T>
class UDPSinkUtil
{
public:
	UDPSinkUtil(QObject *parent, unsigned int udpSize) :
		m_udpSize(udpSize),
		m_udpSamples(udpSize/sizeof(T)),
		m_address(QHostAddress::LocalHost),
		m_port(9999),
		m_sampleBufferIndex(0)
	{
		m_sampleBuffer = new T[m_udpSamples];
		m_socket = new QUdpSocket(parent);
	}

    UDPSinkUtil(QObject *parent, unsigned int udpSize, unsigned int port) :
        m_udpSize(udpSize),
        m_udpSamples(udpSize/sizeof(T)),
        m_address(QHostAddress::LocalHost),
        m_port(port),
        m_sampleBufferIndex(0)
    {
        m_sampleBuffer = new T[m_udpSamples];
        m_socket = new QUdpSocket(parent);
    }

	UDPSinkUtil (QObject *parent, unsigned int udpSize, QHostAddress& address, unsigned int port) :
		m_udpSize(udpSize),
        m_udpSamples(udpSize/sizeof(T)),
		m_address(address),
		m_port(port),
		m_sampleBufferIndex(0)
	{
		m_sampleBuffer = new T[m_udpSamples];
		m_socket = new QUdpSocket(parent);
	}

	~UDPSinkUtil()
	{
		delete[] m_sampleBuffer;
		delete m_socket;
	}

	void moveToThread(QThread *thread)
	{
	    m_socket->moveToThread(thread);
	}

	void setAddress(const QString& address) { m_address.setAddress(address); }
	void setPort(unsigned int port) { m_port = port; }

	void setDestination(const QString& address, int port)
	{
	    m_address.setAddress(const_cast<QString&>(address));
	    m_port = port;
	}

	/**
	 * Write one sample
	 */
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

	/**
	 * Write a bunch of samples
	 */
	void write(T *samples, int nbSamples)
	{
	    int samplesIndex = 0;

	    if (m_sampleBufferIndex + nbSamples > m_udpSamples) // fill remainder of buffer and send it
	    {
	        memcpy(&m_sampleBuffer[m_sampleBufferIndex], &samples[samplesIndex], (m_udpSamples - m_sampleBufferIndex)*sizeof(T)); // fill remainder of buffer
	        m_socket->writeDatagram((const char*)&m_sampleBuffer[0], (qint64 ) m_udpSize, m_address, m_port); // send buffer
            samplesIndex += (m_udpSamples - m_sampleBufferIndex);
            nbSamples -= (m_udpSamples - m_sampleBufferIndex);
	        m_sampleBufferIndex = 0;
	    }

	    while (nbSamples > m_udpSamples) // send directly from input without buffering
	    {
	        m_socket->writeDatagram((const char*)&samples[samplesIndex], (qint64 ) m_udpSize, m_address, m_port);
	        samplesIndex += m_udpSamples;
	        nbSamples -= m_udpSamples;
	    }

	    memcpy(&m_sampleBuffer[m_sampleBufferIndex], &samples[samplesIndex], nbSamples*sizeof(T)); // copy remainder of input to buffer
	}

	/**
	 * Write a bunch of samples unbuffered
	 */
	void writeUnbuffered(const T *samples, int nbSamples)
	{
		m_socket->writeDatagram((const char*)samples, (qint64 ) nbSamples, m_address, m_port); // send given samples
	}

private:
	int m_udpSize;
    int m_udpSamples;
	QHostAddress m_address;
	unsigned int m_port;
	QUdpSocket *m_socket;
	T *m_sampleBuffer;;
	int m_sampleBufferIndex;
};



#endif /* INCLUDE_UTIL_UDPSINK_H_ */
