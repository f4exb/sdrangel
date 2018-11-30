#ifndef INCLUDE_MOVINGAVERAGE_H
#define INCLUDE_MOVINGAVERAGE_H

#include <stdint.h>
#include <vector>
#include <algorithm>
#include "dsp/dsptypes.h"

template<typename Type> class MovingAverage {
public:
	MovingAverage(int historySize, Type initial) : m_index(0)
	{
	    resize(historySize, initial);
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

        if (m_index < m_history.size() - 1) {
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

	int historySize() const
	{
	    return m_history.size();
	}

protected:
	std::vector<Type> m_history;
	Type m_sum;
	uint32_t m_index;
};

#endif // INCLUDE_MOVINGAVERAGE_H
