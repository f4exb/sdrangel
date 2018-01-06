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

#include <string.h>
#include <QAudioFormat>
#include <QAudioDeviceInfo>
#include <QAudioOutput>
#include "audio/audiooutput.h"
#include "audio/audiofifo.h"

AudioOutput::AudioOutput() :
	m_mutex(QMutex::Recursive),
	m_audioOutput(0),
	m_audioUsageCount(0),
	m_onExit(false),
	m_audioFifos()
{
}

AudioOutput::~AudioOutput()
{
	stop();

	QMutexLocker mutexLocker(&m_mutex);

	for (AudioFifos::iterator it = m_audioFifos.begin(); it != m_audioFifos.end(); ++it)
	{
		delete *it;
	}

	m_audioFifos.clear();
}

bool AudioOutput::start(int device, int rate)
{

	if (m_audioUsageCount == 0)
	{
        QMutexLocker mutexLocker(&m_mutex);
        QAudioDeviceInfo devInfo;

        if (device < 0)
        {
            devInfo = QAudioDeviceInfo::defaultOutputDevice();
            qWarning("AudioOutput::start: using default device %s", qPrintable(devInfo.defaultOutputDevice().deviceName()));
        }
        else
        {
            QList<QAudioDeviceInfo> devicesInfo = QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);

            if (device < devicesInfo.size())
            {
                devInfo = devicesInfo[device];
                qWarning("AudioOutput::start: using audio device #%d: %s", device, qPrintable(devInfo.defaultOutputDevice().deviceName()));
            }
            else
            {
                devInfo = QAudioDeviceInfo::defaultOutputDevice();
                qWarning("AudioOutput::start: audio device #%d does not exist. Using default device %s", device, qPrintable(devInfo.defaultOutputDevice().deviceName()));
            }
        }

        //QAudioDeviceInfo devInfo(QAudioDeviceInfo::defaultOutputDevice());

        m_audioFormat.setSampleRate(rate);
        m_audioFormat.setChannelCount(2);
        m_audioFormat.setSampleSize(16);
        m_audioFormat.setCodec("audio/pcm");
        m_audioFormat.setByteOrder(QAudioFormat::LittleEndian);
        m_audioFormat.setSampleType(QAudioFormat::SignedInt);

        if (!devInfo.isFormatSupported(m_audioFormat))
        {
            m_audioFormat = devInfo.nearestFormat(m_audioFormat);
            qWarning("AudioOutput::start: %d Hz S16_LE audio format not supported. New rate: %d", rate, m_audioFormat.sampleRate());
        }
        else
        {
            qInfo("AudioOutput::start: audio format OK");
        }

        if (m_audioFormat.sampleSize() != 16)
        {
            qWarning("AudioOutput::start: Audio device ( %s ) failed", qPrintable(devInfo.defaultOutputDevice().deviceName()));
            return false;
        }

        m_audioOutput = new QAudioOutput(devInfo, m_audioFormat);

        QIODevice::open(QIODevice::ReadOnly);

        m_audioOutput->start(this);

        if (m_audioOutput->state() != QAudio::ActiveState)
        {
            qWarning("AudioOutput::start: cannot start");
        }
	}

	m_audioUsageCount++;

	return true;
}

bool AudioOutput::startImmediate(int device, int rate)
{
    if (QIODevice::isOpen())
    {
        qInfo("AudioOutput::startImmediate: already open");
        return true;
    }

    QMutexLocker mutexLocker(&m_mutex);
    QAudioDeviceInfo devInfo;

    if (device < 0)
    {
        devInfo = QAudioDeviceInfo::defaultOutputDevice();
        qWarning("AudioOutput::startImmediate: using default device %s", qPrintable(devInfo.defaultOutputDevice().deviceName()));
    }
    else
    {
        QList<QAudioDeviceInfo> devicesInfo = QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);

        if (device < devicesInfo.size())
        {
            devInfo = devicesInfo[device];
            qWarning("AudioOutput::startImmediate: using audio device #%d: %s", device, qPrintable(devInfo.defaultOutputDevice().deviceName()));
        }
        else
        {
            devInfo = QAudioDeviceInfo::defaultOutputDevice();
            qWarning("AudioOutput::startImmediate: audio device #%d does not exist. Using default device %s", device, qPrintable(devInfo.defaultOutputDevice().deviceName()));
        }
    }

    //QAudioDeviceInfo devInfo(QAudioDeviceInfo::defaultOutputDevice());

    m_audioFormat.setSampleRate(rate);
    m_audioFormat.setChannelCount(2);
    m_audioFormat.setSampleSize(16);
    m_audioFormat.setCodec("audio/pcm");
    m_audioFormat.setByteOrder(QAudioFormat::LittleEndian);
    m_audioFormat.setSampleType(QAudioFormat::SignedInt);

    if (!devInfo.isFormatSupported(m_audioFormat))
    {
        m_audioFormat = devInfo.nearestFormat(m_audioFormat);
        qWarning("AudioOutput::startImmediate: %d Hz S16_LE audio format not supported. New rate: %d", rate, m_audioFormat.sampleRate());
    }
    else
    {
        qInfo("AudioOutput::startImmediate: audio format OK");
    }

    if (m_audioFormat.sampleSize() != 16)
    {
        qWarning("AudioOutput::startImmediate: Audio device ( %s ) failed", qPrintable(devInfo.defaultOutputDevice().deviceName()));
        return false;
    }

    m_audioOutput = new QAudioOutput(devInfo, m_audioFormat);

    QIODevice::open(QIODevice::ReadOnly);

    m_audioOutput->start(this);

    if (m_audioOutput->state() != QAudio::ActiveState)
    {
        qWarning("AudioOutput::startImmediate: cannot start");
    }

    return true;
}

void AudioOutput::stop()
{
    qDebug("AudioOutput::stop");

    if (m_audioUsageCount > 0)
    {
        m_audioUsageCount--;

        if (m_audioUsageCount == 0)
        {
            QMutexLocker mutexLocker(&m_mutex);
            QIODevice::close();

            if (!m_onExit) {
                delete m_audioOutput;
            }
        }
    }
}

void AudioOutput::stopImmediate()
{
    if (!QIODevice::isOpen())
    {
        qInfo("AudioOutput::stopImmediate");
    }
    else
    {
        qDebug("AudioOutput::stopImmediate");
        QMutexLocker mutexLocker(&m_mutex);
        QIODevice::close();
        delete m_audioOutput;
    }
}

void AudioOutput::addFifo(AudioFifo* audioFifo)
{
	QMutexLocker mutexLocker(&m_mutex);

	m_audioFifos.push_back(audioFifo);
}

void AudioOutput::removeFifo(AudioFifo* audioFifo)
{
	QMutexLocker mutexLocker(&m_mutex);

	m_audioFifos.remove(audioFifo);
}

/*
bool AudioOutput::open(OpenMode mode)
{
	Q_UNUSED(mode);
	return false;
}*/

qint64 AudioOutput::readData(char* data, qint64 maxLen)
{
    //qDebug("AudioOutput::readData: %lld", maxLen);

    // Study this mutex on OSX, for now deadlocks possible
    // Removed as it may indeed cause lockups and is in fact useless.
//#ifndef __APPLE__
//    QMutexLocker mutexLocker(&m_mutex);
//#endif

	unsigned int framesPerBuffer = maxLen / 4;

	if (framesPerBuffer == 0)
	{
		return 0;
	}

	if (m_mixBuffer.size() < framesPerBuffer * 2)
	{
		m_mixBuffer.resize(framesPerBuffer * 2); // allocate 2 qint32 per frame (stereo)

		if (m_mixBuffer.size() != framesPerBuffer * 2)
		{
			return 0;
		}
	}

	memset(&m_mixBuffer[0], 0x00, 2 * framesPerBuffer * sizeof(m_mixBuffer[0])); // start with silence

	// sum up a block from all fifos

	for (AudioFifos::iterator it = m_audioFifos.begin(); it != m_audioFifos.end(); ++it)
	{
		// use outputBuffer as temp - yes, one memcpy could be saved
		uint samples = (*it)->read((quint8*) data, framesPerBuffer, 1);
		const qint16* src = (const qint16*) data;
		std::vector<qint32>::iterator dst = m_mixBuffer.begin();

//		if (samples != framesPerBuffer)
//		{
//            qDebug("AudioOutput::readData: read %d samples vs %d requested", samples, framesPerBuffer);
//		}

		for (uint i = 0; i < samples; i++)
		{
			*dst += *src;
			++src;
			++dst;
			*dst += *src;
			++src;
			++dst;
		}
    }
	// convert to int16

	//std::vector<qint32>::const_iterator src = m_mixBuffer.begin(); // Valgrind optim
	qint16* dst = (qint16*) data;
	qint32 s;

	for (uint i = 0; i < framesPerBuffer; i++)
	{
		// left channel

		//s = *src++; // Valgrind optim
		s = m_mixBuffer[2*i];

		if(s < -32768)
		{
			s = -32768;
		}
		else if (s > 32767)
		{
			s = 32767;
		}

		*dst++ = s;

		// right channel

		//s = *src++; // Valgrind optim
		s = m_mixBuffer[2*i + 1];

		if(s < -32768)
		{
			s = -32768;
		}
		else if (s > 32767)
		{
			s = 32767;
		}

		*dst++ = s;
	}

	return framesPerBuffer * 4;
}

qint64 AudioOutput::writeData(const char* data, qint64 len)
{
	Q_UNUSED(data);
	Q_UNUSED(len);
	return 0;
}
