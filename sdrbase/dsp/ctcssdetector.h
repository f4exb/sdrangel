/*
 * ctcssdetector.h
 *
 *  Created on: Jun 16, 2015
 *      Author: f4exb
 *      See: http://www.embedded.com/design/connectivity/4025660/Detecting-CTCSS-tones-with-Goertzel-s-algorithm
 */

#ifndef INCLUDE_GPL_DSP_CTCSSDETECTOR_H_
#define INCLUDE_GPL_DSP_CTCSSDETECTOR_H_

#include "dsp/dsptypes.h"
#include "export.h"

/** CTCSSDetector: Continuous Tone Coded Squelch System
 * tone detector class based on the Modified Goertzel
 * algorithm.
 */
class SDRBASE_API CTCSSDetector {
public:
    // Constructors and Destructor
    CTCSSDetector();
    // allows user defined CTCSS tone set
    CTCSSDetector(int _nTones, Real *tones);
    virtual ~CTCSSDetector();

    // setup the basic parameters and coefficients
    void setCoefficients(
    		int zN,            // the algorithm "block"  size
			int SampleRate);  // input signal sample rate

    // set the detection threshold
    void setThreshold(double thold);

    // analyze a sample set and optionally filter
    // the tone frequencies.
    bool analyze(Real *sample); // input signal sample

    // get the number of defined tones.
    int getNTones() const {
    	return nTones;
    }

    // get the tone set
    const Real *getToneSet() const
    {
    	return toneSet;
    }

    // get the currently detected tone, if any
    bool getDetectedTone(int &maxTone) const
    {
    	maxTone = maxPowerIndex;
    	return toneDetected;
    }

    // Get the max power at the detected tone.
    Real getMaxPower() const
    {
    	return maxPower;
    }

    void reset();                       // reset the analysis algorithm

protected:
    // Override these to change behavior of the detector
    virtual void initializePower();
    virtual void evaluatePower();
    void feedback(Real sample);
    void feedForward();

private:
    int N;
    int sampleRate;
    int nTones;
    int samplesProcessed;
    int maxPowerIndex;
    bool toneDetected;
    Real maxPower;
    Real *k;
    Real *coef;
    const float *toneSet;
    Real *u0;
    Real *u1;
    Real *power;
};


#endif /* INCLUDE_GPL_DSP_CTCSSDETECTOR_H_ */
