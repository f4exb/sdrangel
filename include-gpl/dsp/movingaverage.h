#ifndef INCLUDE_MOVINGAVERAGE_H
#define INCLUDE_MOVINGAVERAGE_H

#include <vector>
#include "dsp/dsptypes.h"

class MovingAverage {
public:
	MovingAverage() :
		m_history(),
		m_sum(0),
		m_ptr(0)
	{
	}

	MovingAverage(int historySize, Real initial) :
		m_history(historySize, initial),
		m_sum(historySize * initial),
		m_ptr(0)
	{
	}

	void resize(int historySize, Real initial)
	{
		m_history.resize(historySize);
		for(size_t i = 0; i < m_history.size(); i++)
			m_history[i] = initial;
		m_sum = m_history.size() * initial;
		m_ptr = 0;
	}

	void feed(Real value)
	{
		m_sum -= m_history[m_ptr];
		m_history[m_ptr] = value;
		m_sum += value;
		m_ptr++;
		if(m_ptr >= m_history.size())
			m_ptr = 0;
	}

	Real average() const
	{
		return m_sum / (Real)m_history.size();
	}

protected:
	std::vector<Real> m_history;
	Real m_sum;
	uint m_ptr;
};

#endif // INCLUDE_MOVINGAVERAGE_H
