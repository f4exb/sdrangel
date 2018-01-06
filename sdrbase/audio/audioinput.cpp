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

#include <string.h>
#include <QAudioFormat>
#include <QAudioDeviceInfo>
#include <QAudioInput>
#include "audio/audioinput.h"
#include "audio/audiofifo.h"

AudioInput::AudioInput() :
	m_mutex(QMutex::Recursive),
	m_audioInput(0),
	m_audioUsageCount(0),
	m_onExit(false),
	m_volume(0.5f),
	m_audioFifos()
{
}

AudioInput::~AudioInput()
{
	stop();

	QMutexLocker mutexLocker(&m_mutex);

	for (AudioFifos::iterator it = m_audioFifos.begin(); it != m_audioFifos.end(); ++it)
	{
		delete *it;
	}

	m_audioFifos.clear();
}

bool AudioInput::start(int device, int rate)
{
	if (m_audioUsageCount == 0)
	{
        QMutexLocker mutexLocker(&m_mutex);
        QAudioDeviceInfo devInfo;

        if (device < 0)
        {
            devInfo = QAudioDeviceInfo::defaultInputDevice();
            qWarning("AudioInput::start: using default device %s", qPrintable(devInfo.defaultInputDevice().deviceName()));
        }
        else
        {
            QList<QAudioDeviceInfo> devicesInfo = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);

            if (device < devicesInfo.size())
            {
                devInfo = devicesInfo[device];
                qWarning("AudioInput::start: using audio device #%d: %s", device, qPrintable(devInfo.defaultInputDevice().deviceName()));
            }
            else
            {
                devInfo = QAudioDeviceInfo::defaultInputDevice();
                qWarning("AudioInput::start: audio device #%d does not exist. Using default device %s", device, qPrintable(devInfo.defaultInputDevice().deviceName()));
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
            qWarning("AudioInput::start: %d Hz S16_LE audio format not supported. New rate: %d", rate, m_audioFormat.sampleRate());
        }
        else
        {
            qInfo("AudioInput::start: audio format OK");
        }

        if (m_audioFormat.sampleSize() != 16)
        {
            qWarning("AudioInput::start: Audio device ( %s ) failed", qPrintable(devInfo.defaultInputDevice().deviceName()));
            return false;
        }

        m_audioInput = new QAudioInput(devInfo, m_audioFormat);
        m_audioInput->setVolume(m_volume);

        QIODevice::open(QIODevice::ReadWrite);

        m_audioInput->start(this);

        if (m_audioInput->state() != QAudio::ActiveState)
        {
            qWarning("AudioInput::start: cannot start");
        }
	}

	m_audioUsageCount++;

	return true;
}

bool AudioInput::startImmediate(int device, int rate)
{

    if (QIODevice::isOpen())
    {
        qInfo("AudioInput::startImmediate: already started");
        return true;
    }

    QAudioDeviceInfo devInfo;
    QMutexLocker mutexLocker(&m_mutex);

    if (device < 0)
    {
        devInfo = QAudioDeviceInfo::defaultInputDevice();
        qWarning("AudioInput::startImmediate: using default device %s", qPrintable(devInfo.defaultInputDevice().deviceName()));
    }
    else
    {
        QList<QAudioDeviceInfo> devicesInfo = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);

        if (device < devicesInfo.size())
        {
            devInfo = devicesInfo[device];
            qWarning("AudioInput::startImmediate: using audio device #%d: %s", device, qPrintable(devInfo.defaultInputDevice().deviceName()));
        }
        else
        {
            devInfo = QAudioDeviceInfo::defaultInputDevice();
            qWarning("AudioInput::startImmediate: audio device #%d does not exist. Using default device %s", device, qPrintable(devInfo.defaultInputDevice().deviceName()));
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
        qWarning("AudioInput::startImmediate: %d Hz S16_LE audio format not supported. New rate: %d", rate, m_audioFormat.sampleRate());
    }
    else
    {
        qInfo("AudioInput::startImmediate: audio format OK");
    }

    if (m_audioFormat.sampleSize() != 16)
    {
        qWarning("AudioInput::startImmediate: Audio device ( %s ) failed", qPrintable(devInfo.defaultInputDevice().deviceName()));
        return false;
    }

    m_audioInput = new QAudioInput(devInfo, m_audioFormat);
    m_audioInput->setVolume(m_volume);

    QIODevice::open(QIODevice::ReadWrite);

    m_audioInput->start(this);

    if (m_audioInput->state() != QAudio::ActiveState)
    {
        qWarning("AudioInput::startImmediate: cannot start");
    }

    return true;
}

void AudioInput::stop()
{
    qDebug("AudioInput::stop");

    if (m_audioUsageCount > 0)
    {
        m_audioUsageCount--;

        if (m_audioUsageCount == 0)
        {
            qDebug("AudioInput::stop: effectively close QIODevice");
            QMutexLocker mutexLocker(&m_mutex);
            QIODevice::close();

            if (!m_onExit) {
                delete m_audioInput;
            }
        }
    }
}

void AudioInput::stopImmediate()
{
    if (!QIODevice::isOpen())
    {
        qInfo("AudioInput::stopImmediate: already stopped");
    }
    else
    {
        qDebug("AudioInput::stopImmediate");
        QMutexLocker mutexLocker(&m_mutex);
        QIODevice::close();
        delete m_audioInput;
    }
}

void AudioInput::addFifo(AudioFifo* audioFifo)
{
	QMutexLocker mutexLocker(&m_mutex);

	m_audioFifos.push_back(audioFifo);
}

void AudioInput::removeFifo(AudioFifo* audioFifo)
{
	QMutexLocker mutexLocker(&m_mutex);

	m_audioFifos.remove(audioFifo);
}

qint64 AudioInput::readData(char* data, qint64 maxLen)
{
	Q_UNUSED(data);
	Q_UNUSED(maxLen);
	return 0;
}

qint64 AudioInput::writeData(const char *data, qint64 len)
{
    // Study this mutex on OSX, for now deadlocks possible
    // Removed as it may indeed cause lockups and is in fact useless.
//#ifndef __APPLE__
//    QMutexLocker mutexLocker(&m_mutex);
//#endif

    if ((m_audioFormat.sampleSize() != 16)
    		|| (m_audioFormat.sampleType() != QAudioFormat::SignedInt)
			|| (m_audioFormat.byteOrder() != QAudioFormat::LittleEndian))
    {
    	qCritical("AudioInput::writeData: invalid format not S16LE");
    	return 0;
    }

    if (m_audioFormat.channelCount() != 2) {
    	qCritical("AudioInput::writeData: invalid format not stereo");
    	return 0;
    }

	for (AudioFifos::iterator it = m_audioFifos.begin(); it != m_audioFifos.end(); ++it)
	{
		(*it)->write(reinterpret_cast<const quint8*>(data), len/4, 10);
	}

	return len;
}

