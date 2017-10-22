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

#include <QDebug>
#include <QAudio>
#include <QAudioDeviceInfo>
#include <QAudioInput>
#include <QAudioFormat>
#include <QIODevice>
#include <QList>

#include <stdio.h>
#include <errno.h>
#include "fcdproplusreader.h"

#include "dsp/samplesinkfifo.h"
#include "fcdtraits.h"

FCDProPlusReader::FCDProPlusReader(SampleSinkFifo* sampleFifo, QObject* parent) :
	QObject(parent),
	m_fcdAudioInput(0),
	m_fcdInput(0),
	m_running(false),
	m_convertBuffer(fcd_traits<ProPlus>::convBufSize),
	m_fcdBuffer(fcd_traits<ProPlus>::fcdBufSize, 0),
	m_sampleFifo(sampleFifo)
{
}

FCDProPlusReader::~FCDProPlusReader()
{
	if (m_fcdAudioInput) {
		delete m_fcdAudioInput;
	}
}

void FCDProPlusReader::startWork()
{
	QList<QAudioDeviceInfo> audioDevices = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);

	QList<QAudioDeviceInfo>::iterator audioDeviceIt, fcdDeviceIt = audioDevices.end();

	for (audioDeviceIt = audioDevices.begin(); audioDeviceIt != audioDevices.end(); ++audioDeviceIt)
	{
		QAudioFormat fcdAudioFormat = audioDeviceIt->preferredFormat();
		int sampleRate = fcdAudioFormat.sampleRate();
		int sampleBits = fcdAudioFormat.sampleSize();

		if ((sampleRate == 192000) && (sampleBits == 16))
		{
			qDebug() << "FCDProPlusReader::startWork: found: " << audioDeviceIt->deviceName()
				<< " sampleRate: " << fcdAudioFormat.sampleRate()
				<< " sampleBits: " << fcdAudioFormat.sampleSize();
			fcdDeviceIt = audioDeviceIt;
			break;
		}
	}

	if (fcdDeviceIt == audioDevices.end())
	{
		qCritical() << "FCDProPlusReader::startWork: FCD Pro+ sound card not found";
		return;
	}

	openFcdAudio(*fcdDeviceIt);

	if (!m_fcdAudioInput)
	{
		qCritical() << "FCDProPlusReader::startWork: cannot open FCD Pro+ sound card";
		return;
	}

	m_fcdAudioInput->stop();
	m_fcdInput  = m_fcdAudioInput->start();

	if (!m_fcdInput)
	{
		qCritical() << "FCDProPlusReader::startWork: cannot start FCD Pro+ sound card";
		return;
	}

	connect(m_fcdInput, SIGNAL(readyRead()), this, SLOT(readFcdAudio()));
	m_running = true;

	qDebug() << "FCDProPlusReader::startWork: started";
}

void FCDProPlusReader::stopWork()
{
	m_running = false;
	disconnect(m_fcdInput, SIGNAL(readyRead()), this, SLOT(readFcdAudio()));
	m_fcdAudioInput->stop();

	qDebug() << "FCDProPlusReader::stopWork: stopped";
}

void FCDProPlusReader::openFcdAudio(const QAudioDeviceInfo& fcdAudioDeviceInfo)
{
	QAudioFormat fcdAudioFormat = fcdAudioDeviceInfo.preferredFormat();

	qDebug() << "FCDProPlusReader::openFcdAudio: device: " << fcdAudioDeviceInfo.deviceName()
		<< " sampleRate: " << fcdAudioFormat.sampleRate()
		<< " sampleBits: " << fcdAudioFormat.sampleSize();

	m_fcdAudioInput = new QAudioInput(fcdAudioDeviceInfo, fcdAudioFormat, this);
	//m_fcdAudioInput->setBufferSize(1<<14);
	//m_fcdAudioInput->setNotifyInterval(50);
}

void FCDProPlusReader::readFcdAudio()
{
    if (!m_fcdAudioInput) {
        return;
    }

    int len = m_fcdAudioInput->bytesReady();

//    qDebug() << "FCDProPlusReader::readFcdAudio:"
//		<< " buffer size: " << m_fcdAudioInput->bufferSize()
//		<< " interval: " << m_fcdAudioInput->notifyInterval()
//    	<< " len: " << len;

    if (len > fcd_traits<ProPlus>::fcdBufSize) {
        len = fcd_traits<ProPlus>::fcdBufSize;
    }

    int readLen = m_fcdInput->read(m_fcdBuffer.data(), len);

    if (readLen > 0) {
    	m_sampleFifo->write((const quint8*) m_fcdBuffer.constData(), (uint) readLen);
    }
}

