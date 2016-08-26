#ifndef INCLUDE_MOVINGAVERAGE_H
#define INCLUDE_MOVINGAVERAGE_H

#include <stdint.h>
#include <vector>
#include "dsp/dsptypes.h"

template<class Type> class MovingAverage {
public:
	MovingAverage() :
		m_history(),
		m_sum(0),
		m_index(0)
	{
	}

	MovingAverage(int historySize, Type initial) :
		m_history(historySize, initial),
		m_sum((float) historySize * initial),
		m_index(0)
	{
	}

	void resize(int historySize, Type initial)
	{
		m_history.resize(historySize);

		for(size_t i = 0; i < m_history.size(); i++) {
			m_history[i] = initial;
		}

		m_sum = (Type) m_history.size() * initial;
		m_index = 0;
	}

	void feed(Type value)
	{
        Type& oldest = m_history[m_index];
        m_sum += value - oldest;
        oldest = value;

	    m_index++;

	    if(m_index >= m_history.size()) {
	        m_index = 0;
	    }
	}

	void fill(Type value)
	{
		for(size_t i = 0; i < m_history.size(); i++) {
			m_history[i] = value;
		}

		m_sum = (Type) m_history.size() * value;
	}

	Type average() const
	{
		return m_sum / (Type) m_history.size();
	}

	Type sum() const
	{
		return m_sum;
	}

protected:
	std::vector<Type> m_history;
	Type m_sum;
	uint32_t m_index;
};

#endif // INCLUDE_MOVINGAVERAGE_H
