/*
 * kissagc.h
 *
 *  Created on: May 12, 2015
 *      Author: f4exb
 */

#ifndef INCLUDE_GPL_DSP_AGC_H_
#define INCLUDE_GPL_DSP_AGC_H_

#include "movingaverage.h"

class SimpleAGC
{
public:

	SimpleAGC() :
		m_squelch(false),
		m_fill(0),
		m_cutoff(0),
		m_moving_average()
	{}

	SimpleAGC(int historySize, Real initial, Real cutoff) :
		m_squelch(false),
		m_fill(initial),
		m_cutoff(cutoff),
		m_moving_average(historySize, initial)
	{}

	void resize(int historySize, Real initial, Real cutoff)
	{
		m_fill = initial;
		m_cutoff = cutoff;
		m_moving_average.resize(historySize, initial);
	}

	Real getValue()
	{
		return m_moving_average.average();
	}

	void feed(Real value)
	{
		if (value > m_cutoff)
		{
			m_moving_average.feed(value);
		}

		m_squelch = true;
	}

	void close()
	{
		if (m_squelch)
		{
			m_moving_average.fill(m_fill);
			m_squelch = false;
		}
	}

private:
	bool m_squelch;
	Real m_fill;
	Real m_cutoff;
	MovingAverage m_moving_average;
};



#endif /* INCLUDE_GPL_DSP_AGC_H_ */
