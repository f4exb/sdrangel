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

#ifndef INCLUDE_AUDIOFIFO_H
#define INCLUDE_AUDIOFIFO_H

#include <QMutex>
#include <QWaitCondition>
#include "util/export.h"

class SDRANGELOVE_API AudioFifo {
public:
	AudioFifo();
	AudioFifo(uint sampleSize, uint numSamples);
	~AudioFifo();

	bool setSize(uint sampleSize, uint numSamples);

	uint write(const quint8* data, uint numSamples, int timeout = INT_MAX);
	uint read(quint8* data, uint numSamples, int timeout = INT_MAX);

	uint drain(uint numSamples);
	void clear();

	inline uint flush() { return drain(m_fill); }
	inline uint fill() const { return m_fill; }
	inline bool isEmpty() const { return m_fill == 0; }
	inline bool isFull() const { return m_fill == m_size; }
	inline uint size() const { return m_size; }

private:
	QMutex m_mutex;

	qint8* m_fifo;

	uint m_sampleSize;

	uint m_size;
	uint m_fill;
	uint m_head;
	uint m_tail;

	QMutex m_writeWaitLock;
	QMutex m_readWaitLock;
	QWaitCondition m_writeWaitCondition;
	QWaitCondition m_readWaitCondition;

	bool create(uint sampleSize, uint numSamples);
};

#endif // INCLUDE_AUDIOFIFO_H
