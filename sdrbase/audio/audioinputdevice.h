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

#ifndef SDRBASE_AUDIO_AUDIOINPUTDEVICE_H_
#define SDRBASE_AUDIO_AUDIOINPUTDEVICE_H_

#include <QMutex>
#include <QIODevice>
#include <QAudioFormat>
#include <list>
#include <vector>
#include "export.h"

class QAudioInput;
class AudioFifo;
class AudioOutputPipe;


class SDRBASE_API AudioInputDevice : public QIODevice {
public:
	AudioInputDevice();
	virtual ~AudioInputDevice();

	bool start(int device, int rate);
	void stop();

    void addFifo(AudioFifo* audioFifo);
	void removeFifo(AudioFifo* audioFifo);
    int getNbFifos() const { return m_audioFifos.size(); }

	uint getRate() const { return m_audioFormat.sampleRate(); }
	void setOnExit(bool onExit) { m_onExit = onExit; }
	void setVolume(float volume);

private:
	QMutex m_mutex;
	QAudioInput* m_audioInput;
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



#endif /* SDRBASE_AUDIO_AUDIOINPUTDEVICE_H_ */
