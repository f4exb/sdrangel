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
#include "export.h"

/** AFSquelch: AF squelch class based on the Modified Goertzel
 * algorithm.
 */
class SDRBASE_API AFSquelch {
public:
    // constructor with default values
	AFSquelch();
    virtual ~AFSquelch();

    // setup the basic parameters and coefficients
    void setCoefficients(
            unsigned int N,              //!< the algorithm "block"  size
			unsigned int nbAvg,          //!< averaging size
			unsigned int sampleRate,     //!< input signal sample rate
			unsigned int samplesAttack,  //!< number of results before squelch opens
			unsigned int samplesDecay,   //!< number of results keeping squelch open
			const double *tones);        //!< center frequency of tones tested

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
    unsigned int m_N;
    unsigned int m_sampleRate;
    unsigned int m_samplesProcessed;
    unsigned int m_samplesAvgProcessed;
    unsigned int m_maxPowerIndex;
    unsigned int m_nTones;
    unsigned int m_samplesAttack;
    unsigned int m_attackCount;
    unsigned int m_samplesDecay;
    unsigned int m_decayCount;
    unsigned int m_squelchCount;
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
