///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany     //
// written by Christian Daniel                                                       //
// Copyright (C) 2016, 2018 Edouard Griffiths, F4EXB <f4exb06@gmail.com>             //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
#ifndef INCLUDE_SPINLOCK_H
#define INCLUDE_SPINLOCK_H

#include <QAtomicInt>

#include "export.h"

class SDRBASE_API Spinlock {
public:
	void lock()
	{
		while(!m_atomic.testAndSetAcquire(0, 1)) ;
	}

	void unlock()
	{
		while(!m_atomic.testAndSetRelease(1, 0)) ;
	}

protected:
	QAtomicInt m_atomic;
};

class SpinlockHolder {
public:
	SpinlockHolder(Spinlock* spinlock) :
		m_spinlock(spinlock)
	{
		m_spinlock->lock();
	}

	~SpinlockHolder()
	{
		m_spinlock->unlock();
	}

protected:
	Spinlock* m_spinlock;
};

#endif // INCLUDE_SPINLOCK_H
