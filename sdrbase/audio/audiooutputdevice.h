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

#ifndef INCLUDE_AUDIOOUTPUTDEVICE_H
#define INCLUDE_AUDIOOUTPUTDEVICE_H

#include <QMutex>
#include <QIODevice>
#include <QAudioFormat>
#include <list>
#include <vector>
#include <stdint.h>
#include "export.h"

class QAudioOutput;
class AudioFifo;
class AudioOutputPipe;
class AudioNetSink;

class SDRBASE_API AudioOutputDevice : QIODevice {
public:
    enum UDPChannelMode
    {
        UDPChannelLeft,
        UDPChannelRight,
        UDPChannelMixed,
        UDPChannelStereo
    };

    enum UDPChannelCodec
    {
        UDPCodecL16,   //!< Linear 16 bit (no codec)
		UDPCodecL8,    //!< Linear 8 bit
        UDPCodecALaw,  //!< PCM A-law 8 bit
        UDPCodecULaw,  //!< PCM Mu-law 8 bit
        UDPCodecG722,  //!< G722 compression
		UDPCodecOpus   //!< Opus compression
    };

	AudioOutputDevice();
	virtual ~AudioOutputDevice();

	bool start(int device, int rate);
	void stop();

	void addFifo(AudioFifo* audioFifo);
	void removeFifo(AudioFifo* audioFifo);
	int getNbFifos() const { return m_audioFifos.size(); }

	unsigned int getRate() const { return m_audioFormat.sampleRate(); }
	void setOnExit(bool onExit) { m_onExit = onExit; }

	void setUdpDestination(const QString& address, uint16_t port);
	void setUdpCopyToUDP(bool copyToUDP);
	void setUdpUseRTP(bool useRTP);
	void setUdpChannelMode(UDPChannelMode udpChannelMode);
	void setUdpChannelFormat(UDPChannelCodec udpChannelCodec, bool stereo, int sampleRate);
	void setUdpDecimation(uint32_t decimation);
	void setVolume(float volume);

private:
	QMutex m_mutex;
	QAudioOutput* m_audioOutput;
	AudioNetSink* m_audioNetSink;
	bool m_copyAudioToUdp;
	UDPChannelMode m_udpChannelMode;
	UDPChannelCodec m_udpChannelCodec;
	uint m_audioUsageCount;
	bool m_onExit;
	float m_volume;

	std::list<AudioFifo*> m_audioFifos;
	std::vector<qint32> m_mixBuffer;

	QAudioFormat m_audioFormat;

	//virtual bool open(OpenMode mode);
	virtual qint64 readData(char* data, qint64 maxLen);
	virtual qint64 writeData(const char* data, qint64 len);

	friend class AudioOutputPipe;
};

#endif // INCLUDE_AUDIOOUTPUTDEVICE_H
