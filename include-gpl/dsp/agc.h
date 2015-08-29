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
		m_squelchOpen(false),
		m_fill(0),
		m_cutoff(0),
		m_clip(0),
		m_moving_average()
	{}

	SimpleAGC(int historySize, Real initial, Real cutoff=0, Real clip=0) :
		m_squelchOpen(false),
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
		if (m_moving_average.average() > m_clip)
		{
			return m_moving_average.average();
		} else
		{
			return m_clip;
		}
	}

	void feed(Real value)
	{
		if (value > m_cutoff)
		{
			m_moving_average.feed(value);
		}
	}

	void openedSquelch()
	{
		m_squelchOpen = true;
	}

	void closedSquelch()
	{
		if (m_squelchOpen)
		{
			//m_moving_average.fill(m_fill); // Valgrind optim
			m_squelchOpen = false;
		}
	}

private:
	bool m_squelchOpen; // open for processing
	Real m_fill;    // refill average at this level
	Real m_cutoff;  // consider samples only above this level
	Real m_clip;    // never go below this level
	MovingAverage<Real> m_moving_average; // Averaging engine. The stack length conditions the smoothness of AGC.
};


class EvenSimplerAGC
{
public:

	EvenSimplerAGC() :
		m_u0(1.0),
		m_R(1.0),
		m_moving_average()
	{}

	EvenSimplerAGC(int historySize, Real R) :
		m_u0(1.0),
		m_R(R),
		m_moving_average(historySize, m_R)
	{}

	void resize(int historySize, Real R)
	{
		m_R = R;
		m_moving_average.resize(historySize, R);
	}

	Real getValue()
	{
		return m_u0;
	}

	void feed(Complex& ci)
	{
		ci *= m_u0;
		Real magsq = ci.real()*ci.real() + ci.imag()*ci.imag();
		m_moving_average.feed(magsq);
	}

	void openedSquelch()
	{
		m_u0 = m_R / m_moving_average.average();
	}

	void closedSquelch()
	{
		//m_moving_average.fill(m_R); // Valgrind optim
		m_u0 = 1.0;
	}

private:
	Real m_u0;
	Real m_R;       // objective magsq
	MovingAverage<Real> m_moving_average; // Averaging engine. The stack length conditions the smoothness of AGC.
};

class AlphaAGC
{
public:

	AlphaAGC() :
		m_u0(1.0),
		m_R(1.0),
		m_alpha(0.1),
		m_squelchOpen(true),
		m_moving_average()
	{}

	AlphaAGC(int historySize, Real R, Real alpha) :
		m_u0(1.0),
		m_R(R),
		m_alpha(alpha),
		m_squelchOpen(true),
		m_moving_average(historySize, m_R)
	{}

	void resize(int historySize, Real R, Real alpha)
	{
		m_R = R;
		m_alpha = alpha;
		m_squelchOpen = true;
		m_moving_average.resize(historySize, R);
	}

	Real getValue()
	{
		return m_u0;
	}

	void feed(Complex& ci)
	{
		ci *= m_u0;
		Real magsq = ci.real()*ci.real() + ci.imag()*ci.imag();

		if (m_squelchOpen && (magsq < m_moving_average.average()))
		{
			m_moving_average.feed(m_moving_average.average() - m_alpha*(m_moving_average.average() - magsq));
		}
		else
		{
			//m_squelchOpen = true;
			m_moving_average.feed(magsq);
		}

	}

	void openedSquelch()
	{
		m_u0 = m_R / m_moving_average.average();
		m_squelchOpen = true;
	}

	void closedSquelch()
	{
		//m_moving_average.fill(m_R); // Valgrind optim
		m_u0 = 1.0;
		m_squelchOpen = false;
	}

private:
	Real m_u0;
	Real m_R;       // objective magsq
	Real m_alpha;
	bool m_squelchOpen;
	MovingAverage<Real> m_moving_average; // Averaging engine. The stack length conditions the smoothness of AGC.
};



#endif /* INCLUDE_GPL_DSP_AGC_H_ */
