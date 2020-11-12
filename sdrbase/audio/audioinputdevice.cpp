///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#include <string.h>
#include <QAudioFormat>
#include <QAudioDeviceInfo>
#include <QAudioInput>
#include "audio/audioinputdevice.h"
#include "audio/audiofifo.h"

AudioInputDevice::AudioInputDevice() :
	m_mutex(QMutex::Recursive),
	m_audioInput(0),
	m_audioUsageCount(0),
	m_onExit(false),
	m_volume(0.5f),
	m_audioFifos()
{
}

AudioInputDevice::~AudioInputDevice()
{
	stop();

	QMutexLocker mutexLocker(&m_mutex);

	for (std::list<AudioFifo*>::iterator it = m_audioFifos.begin(); it != m_audioFifos.end(); ++it)
	{
		delete *it;
	}

	m_audioFifos.clear();
}

bool AudioInputDevice::start(int device, int rate)
{
	if (m_audioUsageCount == 0)
	{
        QMutexLocker mutexLocker(&m_mutex);
        QAudioDeviceInfo devInfo;

        if (device < 0)
        {
            devInfo = QAudioDeviceInfo::defaultInputDevice();
            qWarning("AudioInputDevice::start: using default device %s", qPrintable(devInfo.defaultInputDevice().deviceName()));
        }
        else
        {
            QList<QAudioDeviceInfo> devicesInfo = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);

            if (device < devicesInfo.size())
            {
                devInfo = devicesInfo[device];
                qWarning("AudioInputDevice::start: using audio device #%d: %s", device, qPrintable(devInfo.deviceName()));
            }
            else
            {
                devInfo = QAudioDeviceInfo::defaultInputDevice();
                qWarning("AudioInputDevice::start: audio device #%d does not exist. Using default device %s", device, qPrintable(devInfo.deviceName()));
            }
        }

        //QAudioDeviceInfo devInfo(QAudioDeviceInfo::defaultOutputDevice());

        m_audioFormat.setSampleRate(rate);
        m_audioFormat.setChannelCount(2);
        m_audioFormat.setSampleSize(16);
        m_audioFormat.setCodec("audio/pcm");
        m_audioFormat.setByteOrder(QAudioFormat::LittleEndian);
        m_audioFormat.setSampleType(QAudioFormat::SignedInt); // Unknown, SignedInt, UnSignedInt, Float

        if (!devInfo.isFormatSupported(m_audioFormat))
        {
            m_audioFormat = devInfo.nearestFormat(m_audioFormat);
            qWarning("AudioInputDevice::start: %d Hz S16_LE audio format not supported. Nearest is sampleRate: %d channelCount: %d sampleSize: %d sampleType: %d",
                    rate, m_audioFormat.sampleRate(), m_audioFormat.channelCount(), m_audioFormat.sampleSize(), (int) m_audioFormat.sampleType());
        }
        else
        {
            qInfo("AudioInputDevice::start: audio format OK");
        }

        if (m_audioFormat.sampleSize() != 16)
        {
            qWarning("AudioInputDevice::start: Audio device '%s' failed", qPrintable(devInfo.defaultInputDevice().deviceName()));
            return false;
        }

        m_audioInput = new QAudioInput(devInfo, m_audioFormat);
        m_audioInput->setVolume(m_volume);

        QIODevice::open(QIODevice::ReadWrite);

        m_audioInput->start(this);

        if (m_audioInput->state() != QAudio::ActiveState)
        {
            qWarning("AudioInputDevice::start: cannot start");
        }
	}

	m_audioUsageCount++;

	return true;
}

void AudioInputDevice::stop()
{
    qDebug("AudioInputDevice::stop");

    if (m_audioUsageCount > 0)
    {
        m_audioUsageCount--;

        if (m_audioUsageCount == 0)
        {
            qDebug("AudioInputDevice::stop: effectively close QIODevice");
            QMutexLocker mutexLocker(&m_mutex);
            QIODevice::close();

            if (!m_onExit) {
                delete m_audioInput;
            }
        }
    }
}

void AudioInputDevice::addFifo(AudioFifo* audioFifo)
{
	QMutexLocker mutexLocker(&m_mutex);

	m_audioFifos.push_back(audioFifo);
}

void AudioInputDevice::removeFifo(AudioFifo* audioFifo)
{
	QMutexLocker mutexLocker(&m_mutex);

	m_audioFifos.remove(audioFifo);
}

qint64 AudioInputDevice::readData(char* data, qint64 maxLen)
{
	Q_UNUSED(data);
	Q_UNUSED(maxLen);
	return 0;
}

qint64 AudioInputDevice::writeData(const char *data, qint64 len)
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
    	qCritical("AudioInputDevice::writeData: invalid format not S16LE");
    	return 0;
    }

    if (m_audioFormat.channelCount() != 2) {
    	qCritical("AudioInputDevice::writeData: invalid format not stereo");
    	return 0;
    }

	for (std::list<AudioFifo*>::iterator it = m_audioFifos.begin(); it != m_audioFifos.end(); ++it)
	{
		(*it)->write(reinterpret_cast<const quint8*>(data), len/4);
	}

	return len;
}

void AudioInputDevice::setVolume(float volume)
{
    m_volume = volume;
    if (m_audioInput != nullptr)
         m_audioInput->setVolume(m_volume);
}
