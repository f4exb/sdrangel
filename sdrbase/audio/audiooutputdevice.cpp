///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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
#include <QThread>
#include <QAudioFormat>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QAudioSink>
#else
#include <QAudioOutput>
#endif
#include "audiooutputdevice.h"
#include "audiodeviceinfo.h"
#include "audiofifo.h"
#include "audionetsink.h"
#include "dsp/wavfilerecord.h"

MESSAGE_CLASS_DEFINITION(AudioOutputDevice::MsgStart, Message)
MESSAGE_CLASS_DEFINITION(AudioOutputDevice::MsgStop, Message)
MESSAGE_CLASS_DEFINITION(AudioOutputDevice::MsgReportSampleRate, Message)

AudioOutputDevice::AudioOutputDevice() :
	m_audioOutput(nullptr),
	m_audioNetSink(nullptr),
    m_wavFileRecord(nullptr),
    m_copyAudioToUdp(false),
	m_udpChannelMode(UDPChannelLeft),
	m_udpChannelCodec(UDPCodecL16),
	m_audioUsageCount(0),
	m_onExit(false),
	m_volume(1.0),
    m_recordToFile(false),
    m_recordSilenceTime(0),
    m_recordSilenceNbSamples(0),
    m_recordSilenceCount(0),
	m_audioFifos(),
    m_managerMessageQueue(nullptr)
{
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
}

AudioOutputDevice::~AudioOutputDevice()
{
//	stop();
//
//	QMutexLocker mutexLocker(&m_mutex);
//
//	for (std::list<AudioFifo*>::iterator it = m_audioFifos.begin(); it != m_audioFifos.end(); ++it)
//	{
//		delete *it;
//	}
//
//	m_audioFifos.clear();
}

bool AudioOutputDevice::start(int deviceIndex, int sampleRate)
{
    // if (m_audioOutput) {
    //     return true;
    // }
//	if (m_audioUsageCount == 0)
//	{
        qDebug("AudioOutputDevice::start: device: %d rate: %d thread: %p", deviceIndex, sampleRate, QThread::currentThread());
        QMutexLocker mutexLocker(&m_mutex);
        AudioDeviceInfo devInfo;

        if (deviceIndex < 0)
        {
            devInfo = AudioDeviceInfo::defaultOutputDevice();
            qWarning("AudioOutputDevice::start: using system default device %s", qPrintable(devInfo.defaultOutputDevice().deviceName()));
        }
        else
        {
            QList<AudioDeviceInfo> devicesInfo = AudioDeviceInfo::availableOutputDevices();

            if (deviceIndex < devicesInfo.size())
            {
                devInfo = devicesInfo[deviceIndex];
                qWarning("AudioOutputDevice::start: using audio device #%d: %s", deviceIndex, qPrintable(devInfo.deviceName()));
            }
            else
            {
                devInfo = AudioDeviceInfo::defaultOutputDevice();
                qWarning("AudioOutputDevice::start: audio device #%d does not exist. Using system default device %s", deviceIndex, qPrintable(devInfo.defaultOutputDevice().deviceName()));
                deviceIndex = -1;
            }
        }

        //QAudioDeviceInfo devInfo(QAudioDeviceInfo::defaultOutputDevice());
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        // Start with a valid format
        m_audioFormat = devInfo.deviceInfo().preferredFormat();
#endif

        m_audioFormat.setSampleRate(sampleRate);
        m_audioFormat.setChannelCount(2);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        m_audioFormat.setSampleFormat(QAudioFormat::Int16);
#else
        m_audioFormat.setSampleSize(16);
        m_audioFormat.setCodec("audio/pcm");
        m_audioFormat.setByteOrder(QAudioFormat::LittleEndian);
        m_audioFormat.setSampleType(QAudioFormat::SignedInt);
#endif

        if (!devInfo.isFormatSupported(m_audioFormat))
        {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            qWarning("AudioOutputDevice::start: format %d Hz 2xS16LE audio/pcm not supported.", sampleRate);
#else
            m_audioFormat = devInfo.deviceInfo().nearestFormat(m_audioFormat);
            std::ostringstream os;
            os << " sampleRate: " << m_audioFormat.sampleRate()
               << " channelCount: " << m_audioFormat.channelCount()
               << " sampleSize: " << m_audioFormat.sampleSize()
               << " codec: "  << m_audioFormat.codec().toStdString()
               << " byteOrder: " <<  (m_audioFormat.byteOrder() == QAudioFormat::BigEndian ? "BE" : "LE")
               << " sampleType: " << (int) m_audioFormat.sampleType();
            qWarning("AudioOutputDevice::start: format %d Hz 2xS16LE audio/pcm not supported. Using: %s", sampleRate, os.str().c_str());
#endif
        }
        else
        {
            qInfo("AudioOutputDevice::start: audio format OK");
        }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        if (m_audioFormat.sampleFormat() != QAudioFormat::Int16)
        {
            qWarning("AudioOutputDevice::start: Audio device '%s' failed", qPrintable(devInfo.deviceName()));
            return false;
        }
#else
        if (m_audioFormat.sampleSize() != 16)
        {
            qWarning("AudioOutputDevice::start: Audio device '%s' failed", qPrintable(devInfo.deviceName()));
            return false;
        }
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        m_audioOutput = new QAudioSink(devInfo.deviceInfo(), m_audioFormat);
#else
        m_audioOutput = new QAudioOutput(devInfo.deviceInfo(), m_audioFormat);
#endif
        m_audioNetSink = new AudioNetSink(0, m_audioFormat.sampleRate(), false);
        m_wavFileRecord = new WavFileRecord(m_audioFormat.sampleRate());
		m_audioOutput->setVolume(m_volume);
        // m_audioOutput->setBufferSize(m_audioFormat.sampleRate() / 5); FIXME: does not work generally
        m_recordSilenceNbSamples = (m_recordSilenceTime * m_audioFormat.sampleRate()) / 10; // time in 100'ś ms

        QIODevice::open(QIODevice::ReadOnly | QIODevice::Unbuffered);

        m_audioOutput->start(this);

        if (m_audioOutput->state() != QAudio::ActiveState) {
            qWarning() << "AudioOutputDevice::start: cannot start - " << m_audioOutput->error();
        } else {
            qDebug("AudioOutputDevice::start: started buffer: %d bytes", m_audioOutput->bufferSize());
        }

        if (m_managerMessageQueue) {
            m_managerMessageQueue->push(AudioOutputDevice::MsgReportSampleRate::create(deviceIndex, devInfo.deviceName(), m_audioFormat.sampleRate()));
        }
//	}
//
//	m_audioUsageCount++;

	return true;
}

void AudioOutputDevice::stop()
{
    if (!m_audioOutput) {
        return;
    }

    qDebug("AudioOutputDevice::stop: thread: %p", QThread::currentThread());

    QMutexLocker mutexLocker(&m_mutex);
    m_audioOutput->stop();
    QIODevice::close();
    delete m_audioNetSink;
    m_audioNetSink = nullptr;
    delete m_wavFileRecord;
    m_wavFileRecord = nullptr;
    delete m_audioOutput;
    m_audioOutput = nullptr;

//    if (m_audioUsageCount > 0)
//    {
//        m_audioUsageCount--;
//
//        if (m_audioUsageCount == 0)
//        {
//            QMutexLocker mutexLocker(&m_mutex);
//            QIODevice::close();
//
//            if (!m_onExit) {
//                delete m_audioOutput;
//            }
//        }
//    }
}

void AudioOutputDevice::addFifo(AudioFifo* audioFifo)
{
	QMutexLocker mutexLocker(&m_mutex);
	m_audioFifos.push_back(audioFifo);
}

void AudioOutputDevice::removeFifo(AudioFifo* audioFifo)
{
	QMutexLocker mutexLocker(&m_mutex);
	m_audioFifos.remove(audioFifo);
}

/*
bool AudioOutputDevice::open(OpenMode mode)
{
	Q_UNUSED(mode);
	return false;
}*/

void AudioOutputDevice::setUdpDestination(const QString& address, uint16_t port)
{
    if (m_audioNetSink) {
        m_audioNetSink->setDestination(address, port);
    }
}

void AudioOutputDevice::setUdpCopyToUDP(bool copyToUDP)
{
    m_copyAudioToUdp = copyToUDP;
}

void AudioOutputDevice::setUdpUseRTP(bool useRTP)
{
    if (m_audioNetSink) {
        m_audioNetSink->selectType(useRTP ? AudioNetSink::SinkRTP : AudioNetSink::SinkUDP);
    }
}

void AudioOutputDevice::setUdpChannelMode(UDPChannelMode udpChannelMode)
{
    m_udpChannelMode = udpChannelMode;
}

void AudioOutputDevice::setUdpChannelFormat(UDPChannelCodec udpChannelCodec, bool stereo, int sampleRate)
{
    m_udpChannelCodec = udpChannelCodec;

    if (m_audioNetSink) {
        m_audioNetSink->setParameters((AudioNetSink::Codec) m_udpChannelCodec, stereo, sampleRate);
    }

    if (m_wavFileRecord)
    {
        if (m_wavFileRecord->isRecording()) {
            m_wavFileRecord->stopRecording();
        }

        m_wavFileRecord->setMono(!stereo);
    }
}

void AudioOutputDevice::setUdpDecimation(uint32_t decimation)
{
	if (m_audioNetSink) {
		m_audioNetSink->setDecimation(decimation);
	}
}

void AudioOutputDevice::setFileRecordName(const QString& fileRecordName)
{
    if (!m_wavFileRecord) {
        return;
    }

    QStringList dotBreakout = fileRecordName.split(QLatin1Char('.'));

    if (dotBreakout.size() > 1) {
        QString extension = dotBreakout.last();

        if (extension != "wav") {
            dotBreakout.last() = "wav";
        }
    }
    else
    {
        dotBreakout.append("wav");
    }

    QString newFileRecordName = dotBreakout.join(QLatin1Char('.'));
    QString fileBase;
    FileRecordInterface::guessTypeFromFileName(newFileRecordName, fileBase);
    qDebug("AudioOutputDevice::setFileRecordName: newFileRecordName: %s fileBase: %s", qPrintable(newFileRecordName), qPrintable(fileBase));
    m_wavFileRecord->setFileName(fileBase);
}

void AudioOutputDevice::setRecordToFile(bool recordToFile)
{
    if (!m_wavFileRecord) {
        return;
    }

    if (recordToFile)
    {
        if (!m_wavFileRecord->isRecording()) {
            m_wavFileRecord->startRecording();
        }
    }
    else
    {
        if (m_wavFileRecord->isRecording()) {
            m_wavFileRecord->stopRecording();
        }
    }

    m_recordToFile = recordToFile;
    m_recordSilenceCount = 0;
}

void AudioOutputDevice::setRecordSilenceTime(int recordSilenceTime)
{
    m_recordSilenceNbSamples = (recordSilenceTime * m_audioFormat.sampleRate()) / 10; // time in 100'ś ms
    m_recordSilenceCount = 0;
    m_recordSilenceTime = recordSilenceTime;
}

qint64 AudioOutputDevice::readData(char* data, qint64 maxLen)
{
    // Study this mutex on OSX, for now deadlocks possible
    // Removed as it may indeed cause lockups and is in fact useless.
//#ifndef __APPLE__
//    QMutexLocker mutexLocker(&m_mutex);
//#endif
    // qDebug("AudioOutputDevice::readData: thread %p (%s)", (void *) QThread::currentThread(), qPrintable(m_deviceName));

	unsigned int samplesPerBuffer = maxLen / 4;

	if (samplesPerBuffer == 0)
	{
		return 0;
	}

	if (m_mixBuffer.size() < samplesPerBuffer * 2)
	{
		m_mixBuffer.resize(samplesPerBuffer * 2); // allocate 2 qint32 per sample (stereo)

		if (m_mixBuffer.size() != samplesPerBuffer * 2)
		{
			return 0;
		}
	}

	memset(&m_mixBuffer[0], 0x00, 2 * samplesPerBuffer * sizeof(m_mixBuffer[0])); // start with silence

	// sum up a block from all fifos

	for (std::list<AudioFifo*>::iterator it = m_audioFifos.begin(); it != m_audioFifos.end(); ++it)
	{
		// use outputBuffer as temp - yes, one memcpy could be saved
		unsigned int samples = (*it)->read((quint8*) data, samplesPerBuffer);
		const qint16* src = (const qint16*) data;
		std::vector<qint32>::iterator dst = m_mixBuffer.begin();

//		if (samples != framesPerBuffer)
//		{
//            qDebug("AudioOutputDevice::readData: read %d samples vs %d requested", samples, framesPerBuffer);
//		}

		for (unsigned int i = 0; i < samples; i++)
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
	qint32 sl, sr;

	for (unsigned int i = 0; i < samplesPerBuffer; i++)
	{
		// left channel

		//s = *src++; // Valgrind optim
		sl = m_mixBuffer[2*i];

		if(sl < -32768)
		{
			sl = -32768;
		}
		else if (sl > 32767)
		{
			sl = 32767;
		}

		*dst++ = sl;

		// right channel

		//s = *src++; // Valgrind optim
		sr = m_mixBuffer[2*i + 1];

		if(sr < -32768)
		{
			sr = -32768;
		}
		else if (sr > 32767)
		{
			sr = 32767;
		}

		*dst++ = sr;

		if ((m_copyAudioToUdp) && (m_audioNetSink))
		{
	        switch (m_udpChannelMode)
	        {
            case UDPChannelStereo:
                m_audioNetSink->write(sl, sr);
                break;
            case UDPChannelMixed:
                m_audioNetSink->write((sl+sr)/2);
                break;
            case UDPChannelRight:
                m_audioNetSink->write(sr);
                break;
	        case UDPChannelLeft:
	        default:
	            m_audioNetSink->write(sl);
	            break;
	        }
		}

        if ((m_recordToFile) && (m_wavFileRecord))
        {
            if ((sr == 0) && (sl == 0))
            {
                if (m_recordSilenceNbSamples <= 0)
                {
                    writeSampleToFile(sl, sr);
                    m_recordSilenceCount = 0;
                }
                else if (m_recordSilenceCount < m_recordSilenceNbSamples)
                {
                    writeSampleToFile(sl, sr);
                    m_recordSilenceCount++;
                }
                else
                {
                    m_wavFileRecord->stopRecording();
                }
            }
            else
            {
                if (!m_wavFileRecord->isRecording()) {
                    m_wavFileRecord->startRecording();
                }

                writeSampleToFile(sl, sr);
                m_recordSilenceCount = 0;
            }
        }
	}

	return samplesPerBuffer * 4;
}

void AudioOutputDevice::writeSampleToFile(qint16 lSample, qint16 rSample)
{
    switch (m_udpChannelMode)
    {
    case UDPChannelStereo:
        m_wavFileRecord->write(lSample, rSample);
        break;
    case UDPChannelMixed:
        m_wavFileRecord->writeMono((lSample+rSample)/2);
        break;
    case UDPChannelRight:
        m_wavFileRecord->writeMono(rSample);
        break;
    case UDPChannelLeft:
    default:
        m_wavFileRecord->writeMono(lSample);
        break;
    }
}

qint64 AudioOutputDevice::writeData(const char* data, qint64 len)
{
	Q_UNUSED(data);
	Q_UNUSED(len);
	return 0;
}

void AudioOutputDevice::setVolume(float volume)
{
    m_volume = volume;

    if (m_audioOutput) {
        m_audioOutput->setVolume(m_volume);
	}
}

// Qt6 requires bytesAvailable to be implemented. Not needed for Qt5.
qint64 AudioOutputDevice::bytesAvailable() const
{
	qint64 available = 0;
    for (std::list<AudioFifo*>::const_iterator it = m_audioFifos.begin(); it != m_audioFifos.end(); ++it)
	{
        qint64 fill = (*it)->fill();
        if (available == 0) {
            available = fill;
        } else {
            available = std::min(fill, available);
        }
    }
    // If we return 0 from this twice in a row, audio will stop.
    // So we always return a value, and if we don't have enough data in the FIFOs
    // when readData is called, that will output silence
    if (available == 0) {
        available = 2048; // Is there a better value to use?
    }
    return available * 2 * 2; // 2 Channels of 16-bit data
}

bool AudioOutputDevice::handleMessage(const Message& cmd)
{
    if (MsgStart::match(cmd))
    {
        MsgStart ctl = (MsgStart&) cmd;
        start(ctl.getDeviceIndex(), ctl.getSampleRate());
        return true;
    }
    else if (MsgStop::match(cmd))
    {
        stop();
        return true;
    }

    return false;
}

void AudioOutputDevice::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}
