///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_GPL_DSP_AFSQUELCH_H_
#define INCLUDE_GPL_DSP_AFSQUELCH_H_

#include "dsp/dsptypes.h"
#include "dsp/movingaverage.h"

/** AFSquelch: AF squelch class based on the Modified Goertzel
 * algorithm.
 */
class AFSquelch {
public:
    // Constructors and Destructor
	AFSquelch();
    // allows user defined tone pair
	AFSquelch(unsigned int nbTones,
			const double *tones);
    virtual ~AFSquelch();

    // setup the basic parameters and coefficients
    void setCoefficients(
    		int N,              //!< the algorithm "block"  size
			unsigned int nbAvg, //!< averaging size
			int SampleRate,     //!< input signal sample rate
			int _samplesAttack, //!< number of results before squelch opens
			int _samplesDecay); //!< number of results keeping squelch open

    // set the detection threshold
    void setThreshold(double _threshold);

    // analyze a sample set and optionally filter
    // the tone frequencies.
    bool analyze(double sample); // input signal sample
    bool evaluate(); // evaluate result

    // get the tone set
    const double *getToneSet() const
    {
    	return m_toneSet;
    }

    bool open() const {
    	return m_isOpen;
    }

    void reset();                       // reset the analysis algorithm

protected:
    void feedback(double sample);
    void feedForward();

private:
    unsigned int m_nbAvg; //!< number of power samples taken for moving average
    int m_N;
    int m_sampleRate;
    int m_samplesProcessed;
    int m_samplesAvgProcessed;
    int m_maxPowerIndex;
    int m_nTones;
    int m_samplesAttack;
    int m_attackCount;
    int m_samplesDecay;
    int m_decayCount;
    bool m_isOpen;
    double m_threshold;
    double *m_k;
    double *m_coef;
    double *m_toneSet;
    double *m_u0;
    double *m_u1;
    double *m_power;
    std::vector<MovingAverage<double> > m_movingAverages;
};


#endif /* INCLUDE_GPL_DSP_CTCSSDETECTOR_H_ */
