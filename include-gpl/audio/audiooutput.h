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

#ifndef INCLUDE_AUDIOOUTPUT_H
#define INCLUDE_AUDIOOUTPUT_H

#include <QMutex>
#include <QIODevice>
#include <list>
#include <vector>
#include "util/export.h"

class QAudioOutput;
class AudioFifo;
class AudioOutputPipe;

class SDRANGELOVE_API AudioOutput : QIODevice {
public:
	AudioOutput();
	~AudioOutput();

	bool start(int device, int rate);
	void stop();

	void addFifo(AudioFifo* audioFifo);
	void removeFifo(AudioFifo* audioFifo);

private:
	QMutex m_mutex;
	QAudioOutput* m_audioOutput;

	typedef std::list<AudioFifo*> AudioFifos;
	AudioFifos m_audioFifos;
	std::vector<qint32> m_mixBuffer;

	bool open(OpenMode mode);
	qint64 readData(char* data, qint64 maxLen);
	qint64 writeData(const char* data, qint64 len);

	friend class AudioOutputPipe;
};

#endif // INCLUDE_AUDIOOUTPUT_H
