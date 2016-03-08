#ifndef INCLUDE_SPINLOCK_H
#define INCLUDE_SPINLOCK_H

#include <QAtomicInt>

class Spinlock {
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
