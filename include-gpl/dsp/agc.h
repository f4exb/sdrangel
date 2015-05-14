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
		m_clip(0),
		m_moving_average()
	{}

	SimpleAGC(int historySize, Real initial, Real cutoff=0, Real clip=0) :
		m_squelch(false),
		m_fill(initial),
		m_cutoff(cutoff),
		m_clip(clip),
		m_moving_average(historySize, initial)
	{}

	void resize(int historySize, Real initial, Real cutoff=0, Real clip=0)
	{
		m_fill = initial;
		m_cutoff = cutoff;
		m_clip = clip;
		m_moving_average.resize(historySize, initial);
	}

	Real getValue()
	{
		if (m_moving_average.average() > m_clip) {
			return m_moving_average.average();
		} else {
			return m_clip;
		}
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
	bool m_squelch; // open for processing
	Real m_fill;    // refill average at this level
	Real m_cutoff;  // consider samples only above this level
	Real m_clip;    // never go below this level
	MovingAverage m_moving_average; // Averaging engine. The stack length conditions the smoothness of AGC.
};



#endif /* INCLUDE_GPL_DSP_AGC_H_ */
