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
	AGC(int historySize, Real R);
	virtual ~AGC();

	void resize(int historySize, Real R);
	Real getValue();
	Real getAverage();
	virtual void feed(Complex& ci) = 0;

protected:
	double m_u0;
	double m_R;       // objective mag
	MovingAverage<double> m_moving_average; // Averaging engine. The stack length conditions the smoothness of AGC.
	int m_historySize;
	int m_count;
};

class MagSquaredAGC : public AGC
{
public:
	MagSquaredAGC(int historySize, double R, double threshold);
	virtual ~MagSquaredAGC();
	virtual void feed(Complex& ci);
	double feedAndGetValue(const Complex& ci);
	double getMagSq() const { return m_magsq; }
	void setThreshold(double threshold) { m_threshold = threshold; }
	void setGate(int gate) { m_gate = gate; }
private:
	double m_magsq;
	double m_threshold; //!< squelch on magsq average with transition from +3dB
    int m_gate;
    int m_stepCounter;
    int m_gateCounter;
};

class MagAGC : public AGC
{
public:
	MagAGC(int historySize, double R, double threshold);
	virtual ~MagAGC();
	virtual void feed(Complex& ci);
    double feedAndGetValue(const Complex& ci);
	Real getMagSq() const { return m_magsq; }
    void setThreshold(double threshold) { m_threshold = threshold; }
    void setGate(int gate) { m_gate = gate; }
private:
	double m_magsq;
    double m_threshold; //!< squelch on magsq average
    int m_gate;
    int m_stepCounter;
    int m_gateCounter;
};

class AlphaAGC : public AGC
{
public:
	AlphaAGC(int historySize, Real R);
	AlphaAGC(int historySize, Real R, Real alpha);
	virtual ~AlphaAGC();
    void resize(int historySize, Real R, Real alpha);
	virtual void feed(Complex& ci);
	Real getMagSq() const { return m_magsq; }
private:
	Real m_alpha;
	Real m_magsq;
	bool m_squelchOpen;
};

class SimpleAGC
{
public:
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

	void fill(double value)
	{
	    m_moving_average.fill(value);
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
    MovingAverage<double> m_moving_average; // Averaging engine. The stack length conditions the smoothness of AGC.
};

#endif /* INCLUDE_GPL_DSP_AGC_H_ */
