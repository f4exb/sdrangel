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

/** AFSquelch: AF squelch class based on the Modified Goertzel
 * algorithm.
 */
class AFSquelch {
public:
    // Constructors and Destructor
	AFSquelch();
    // allows user defined tone pair
	AFSquelch(unsigned int nbTones, const Real *tones);
    virtual ~AFSquelch();

    // setup the basic parameters and coefficients
    void setCoefficients(
    		int N,              // the algorithm "block"  size
			int SampleRate,     // input signal sample rate
			int _samplesAttack, // number of results before squelch opens
			int _samplesDecay); // number of results keeping squelch open

    // set the detection threshold
    void setThreshold(double _threshold) {
    	threshold = _threshold;
    }

    // analyze a sample set and optionally filter
    // the tone frequencies.
    bool analyze(Real *sample); // input signal sample

    // get the tone set
    const Real *getToneSet() const
    {
    	return toneSet;
    }

    bool open() const {
    	return isOpen;
    }

    void reset();                       // reset the analysis algorithm

protected:
    void feedback(Real sample);
    void feedForward();
    void evaluate();

private:
    int N;
    int sampleRate;
    int samplesProcessed;
    int maxPowerIndex;
    int nTones;
    int samplesAttack;
    int attackCount;
    int samplesDecay;
    int decayCount;
    bool isOpen;
    double threshold;
    double *k;
    double *coef;
    Real *toneSet;
    double *u0;
    double *u1;
    double *power;
};


#endif /* INCLUDE_GPL_DSP_CTCSSDETECTOR_H_ */
