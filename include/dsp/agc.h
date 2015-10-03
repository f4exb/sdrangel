/*
 * kissagc.h
 *
 *  Created on: May 12, 2015
 *      Author: f4exb
 */

#ifndef INCLUDE_GPL_DSP_AGC_H_
#define INCLUDE_GPL_DSP_AGC_H_

#include "movingaverage.h"

class AGC
{
public:

	AGC();
	AGC(int historySize, Real R);
	virtual ~AGC();

	void resize(int historySize, Real R);
	Real getValue();
	Real getAverage();
	virtual void feed(Complex& ci) = 0;

protected:
	Real m_u0;
	Real m_R;       // objective mag
	MovingAverage<Real> m_moving_average; // Averaging engine. The stack length conditions the smoothness of AGC.
	int m_historySize;
	int m_count;
};

class MagSquaredAGC : public AGC
{
public:
	MagSquaredAGC();
	MagSquaredAGC(int historySize, Real R);
	virtual ~MagSquaredAGC();
	virtual void feed(Complex& ci);
};

class MagAGC : public AGC
{
public:
	MagAGC();
	MagAGC(int historySize, Real R);
	virtual ~MagAGC();
	virtual void feed(Complex& ci);
};

class AlphaAGC : public AGC
{
public:
	AlphaAGC();
	AlphaAGC(int historySize, Real R);
	AlphaAGC(int historySize, Real R, Real alpha);
	virtual ~AlphaAGC();
    void resize(int historySize, Real R, Real alpha);
	virtual void feed(Complex& ci);
private:
	Real m_alpha;
	bool m_squelchOpen;
};

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

private:
    bool m_squelchOpen; // open for processing
    Real m_fill;    // refill average at this level
    Real m_cutoff;  // consider samples only above this level
    Real m_clip;    // never go below this level
    MovingAverage<Real> m_moving_average; // Averaging engine. The stack length conditions the smoothness of AGC.
};

#endif /* INCLUDE_GPL_DSP_AGC_H_ */
