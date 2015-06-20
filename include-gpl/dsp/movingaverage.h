#ifndef INCLUDE_MOVINGAVERAGE_H
#define INCLUDE_MOVINGAVERAGE_H

#include <vector>
#include "dsp/dsptypes.h"

template<class Type> class MovingAverage {
public:
	MovingAverage() :
		m_history(),
		m_sum(0),
		m_ptr(0)
	{
	}

	MovingAverage(int historySize, Type initial) :
		m_history(historySize, initial),
		m_sum((float) historySize * initial),
		m_ptr(0)
	{
	}

	void resize(int historySize, Type initial)
	{
		m_history.resize(historySize);
		for(size_t i = 0; i < m_history.size(); i++)
			m_history[i] = initial;
		m_sum = (float) m_history.size() * initial;
		m_ptr = 0;
	}

	void feed(Type value)
	{
		m_sum -= m_history[m_ptr];
		m_history[m_ptr] = value;
		m_sum += value;
		m_ptr++;
		if(m_ptr >= m_history.size())
			m_ptr = 0;
	}

	void fill(Type value)
	{
		for(size_t i = 0; i < m_history.size(); i++)
			m_history[i] = value;
		m_sum = (float) m_history.size() * value;
	}

	Type average() const
	{
		return m_sum / (float) m_history.size();
	}

protected:
	std::vector<Type> m_history;
	Type m_sum;
	uint m_ptr;
};

#endif // INCLUDE_MOVINGAVERAGE_H
