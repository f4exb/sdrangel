/*
 * kissagc.h
 *
 *  Created on: May 12, 2015
 *      Author: f4exb
 */

#ifndef INCLUDE_GPL_DSP_AGC_H_
#define INCLUDE_GPL_DSP_AGC_H_

#include "movingaverage.h"
#include "util/movingaverage.h"

class AGC
{
public:
	AGC(int historySize, double R);
	virtual ~AGC();

	void resize(int historySize, double R);
	void setOrder(double R) { m_R = R; }
	Real getValue();
	Real getAverage();
	virtual void feed(Complex& ci) = 0;

protected:
	double m_u0;                            //!< AGC factor
	double m_R;                             //!< ordered magnitude
	MovingAverage<double> m_moving_average; //!< Averaging engine. The stack length conditions the smoothness of AGC.
	int m_historySize;                      //!< Averaging length (attack)
	int m_count;                            //!< Samples counter
};


class MagAGC : public AGC
{
public:
	MagAGC(int historySize, double R, double threshold);
	virtual ~MagAGC();
	void setSquared(bool squared) { m_squared = squared; }
	void resize(int historySize, Real R);
	void setOrder(double R);
	virtual void feed(Complex& ci);
    double feedAndGetValue(const Complex& ci);
    double getMagSq() const { return m_magsq; }
    void setThreshold(double threshold) { m_threshold = threshold; }
    void setThresholdEnable(bool enable);
    void setGate(int gate) { m_gate = gate; }
    void setStepDownDelay(int stepDownDelay) { m_stepDownDelay = stepDownDelay; }
    void setClamping(bool clamping) { m_clamping = clamping; }
    void setClampMax(double clampMax) { m_clampMax = clampMax; }
private:
    bool m_squared;        //!< use squared magnitude (power) to compute AGC value
	double m_magsq;        //!< current squared magnitude (power)
    double m_threshold;    //!< squelch on magsq average
    bool m_thresholdEnable; //!< enable squelch on power threshold
    int m_gate;            //!< power threshold gate in number of samples
    int m_stepLength;      //!< transition step length in number of samples
    double m_stepDelta;    //!< transition step unit by sample
    int m_stepUpCounter;   //!< step up transition samples counter
    int m_stepDownCounter; //!< step down transition samples counter
    int m_gateCounter;     //!< threshold gate samples counter
    int m_stepDownDelay;   //!< delay in samples before cutoff (release)
    bool m_clamping;       //!< clamping active
    double m_R2;           //!< square of ordered magnitude
    double m_clampMax;     //!< maximum to clamp to as power value
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

template<uint32_t AvgSize>
class SimpleAGC
{
public:
	SimpleAGC(Real cutoff=0, Real clip=0) :
        m_squelchOpen(false),
        m_cutoff(cutoff),
        m_clip(clip)
	{}

	void resize(Real cutoff=0, Real clip=0)
	{
        m_cutoff = cutoff;
        m_clip = clip;
	}

	Real getValue()
	{
        if ((Real) m_moving_average > m_clip)
        {
            return (Real) m_moving_average;
        } else
        {
            return m_clip;
        }
	}

    void feed(Real value)
    {
        if (value > m_cutoff)
        {
            m_moving_average(value);
        }
    }

private:
    bool m_squelchOpen; // open for processing
    Real m_cutoff;  // consider samples only above this level
    Real m_clip;    // never go below this level
    //MovingAverage<double> m_moving_average; // Averaging engine. The stack length conditions the smoothness of AGC.
    MovingAverageUtil<Real, double, AvgSize> m_moving_average;
};

#endif /* INCLUDE_GPL_DSP_AGC_H_ */
