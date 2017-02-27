#ifndef INCLUDE_MOVINGAVERAGE_H
#define INCLUDE_MOVINGAVERAGE_H

#include <stdint.h>
#include <vector>
#include <algorithm>
#include "dsp/dsptypes.h"

template<typename Type> class MovingAverage {
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
		std::fill(m_history.begin(), m_history.end(), initial);
		m_sum = (Type) m_history.size() * initial;
		m_index = 0;
	}

	void feed(Type value)
	{
        Type& oldest = m_history[m_index];
        m_sum += value - oldest;
        oldest = value;

        if (m_index < m_history.size()) {
            m_index++;
        } else {
            m_index = 0;
        }
	}

	void fill(Type value)
	{
        std::fill(m_history.begin(), m_history.end(), value);
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
