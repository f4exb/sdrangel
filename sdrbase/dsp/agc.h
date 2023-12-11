///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2019 Edouard Griffiths, F4EXB <f4exb06@gmail.com>              //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_GPL_DSP_AGC_H_
#define INCLUDE_GPL_DSP_AGC_H_

#include "movingaverage.h"
#include "util/movingaverage.h"
#include "export.h"

class SDRBASE_API AGC
{
public:
	AGC(int historySize, double R);
	virtual ~AGC();

	void resize(int historySize, double R);
	void setOrder(double R) { m_R = R; }
	Real getValue();
	Real getAverage();
    void reset(double R) { m_moving_average.fill(R); }
	virtual void feed(Complex& ci) = 0;

protected:
	double m_u0;                            //!< AGC factor
	double m_R;                             //!< ordered magnitude
	MovingAverage<double> m_moving_average; //!< Averaging engine. The stack length conditions the smoothness of AGC.
	int m_historySize;                      //!< Averaging length (the longer the slower the AGC)
	int m_count;                            //!< Samples counter
};


class SDRBASE_API MagAGC : public AGC
{
public:
	MagAGC(int historySize, double R, double threshold);
	virtual ~MagAGC();
	void setSquared(bool squared) { m_squared = squared; }
	void resize(int historySize, int stepLength, Real R);
	void setOrder(double R);
	virtual void feed(Complex& ci);
    double feedAndGetValue(const Complex& ci);
    double getMagSq() const { return m_magsq; }
    void setThreshold(double threshold) { m_threshold = threshold; }
    void setThresholdEnable(bool enable);
    void setGate(int gate) { m_gate = gate; m_gateCounter = 0; m_count = 0; }
    void setStepDownDelay(int stepDownDelay) { m_stepDownDelay = stepDownDelay; m_gateCounter = 0; m_count = 0; }
    int getStepDownDelay() const { return m_stepDownDelay; }
    float getStepValue() const;
    void setHardLimiting(bool hardLimiting) { m_hardLimiting = hardLimiting; }
    void resetStepCounters() { m_stepUpCounter = 0; m_stepDownCounter = 0; }

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
    bool m_hardLimiting;   //!< hard limit multiplier so that resulting sample magnitude does not exceed 1.0

    double hardLimiter(double multiplier, double magsq);
};

template<uint32_t AvgSize>
class SimpleAGC
{
public:
	SimpleAGC(Real initial, Real cutoff=0, Real clip=0) :
        m_cutoff(cutoff),
        m_clip(clip),
        m_moving_average(AvgSize, initial)
	{
	}

	void resize(Real initial, Real cutoff=0, Real clip=0)
	{
        m_cutoff = cutoff;
        m_clip = clip;
        m_moving_average.resize(AvgSize, initial);
	}

    void resizeNew(uint32_t newSize, Real initial, Real cutoff=0, Real clip=0)
    {
        m_cutoff = cutoff;
        m_clip = clip;
        m_moving_average.resize(newSize, initial);
    }

    void fill(double value)
    {
        m_moving_average.fill(value);
    }

	Real getValue()
	{
        if ((Real) m_moving_average.average() > m_clip) {
            return (Real) m_moving_average.average();
        } else {
            return m_clip;
        }
	}

    void feed(Real value)
    {
        if (value > m_cutoff) {
            m_moving_average.feed(value);
        }
    }

private:
    Real m_cutoff;  // consider samples only above this level
    Real m_clip;    // never go below this level
    MovingAverage<double> m_moving_average; // Averaging engine. The stack length conditions the smoothness of AGC.
    //MovingAverageUtil<Real, double, AvgSize> m_moving_average;
};

#endif /* INCLUDE_GPL_DSP_AGC_H_ */
