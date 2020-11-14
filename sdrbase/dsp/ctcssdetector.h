//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2020 Edouard Griffiths, F4EXB                                                         //
//                                                                                                          //
// See: http://www.embedded.com/design/connectivity/4025660/Detecting-CTCSS-tones-with-Goertzel-s-algorithm //
//                                                                                                          //
// This program is free software; you can redistribute it and/or modify                                     //
// it under the terms of the GNU General Public License as published by                                     //
// the Free Software Foundation as version 3 of the License, or                                             //
// (at your option) any later version.                                                                      //
//                                                                                                          //
// This program is distributed in the hope that it will be useful,                                          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                                           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                                             //
// GNU General Public License V3 for more details.                                                          //
//                                                                                                          //
// You should have received a copy of the GNU General Public License                                        //
// along with this program. If not, see <http://www.gnu.org/licenses/>.                                     //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_GPL_DSP_CTCSSDETECTOR_H_
#define INCLUDE_GPL_DSP_CTCSSDETECTOR_H_

#include "dsp/dsptypes.h"
#include "export.h"
#include "ctcssfrequencies.h"

/** CTCSSDetector: Continuous Tone Coded Squelch System
 * tone detector class based on the Modified Goertzel
 * algorithm.
 */
class SDRBASE_API CTCSSDetector {
public:
    CTCSSDetector();
    virtual ~CTCSSDetector();

    // setup the basic parameters and coefficients
    void setCoefficients(
    		int N,          // the algorithm "block"  size
			int sampleRate  // input signal sample rate
    );

    // set the detection threshold
    void setThreshold(double thold);

    // analyze a sample set and optionally filter the tone frequencies.
    bool analyze(Real *sample); // input signal sample

    // get the number of defined tones.
    int getNTones() const {
    	return CTCSSFrequencies::m_nbFreqs;
    }

    // get the tone set
    const Real *getToneSet() const {
    	return CTCSSFrequencies::m_Freqs;
    }

    // get the currently detected tone, if any
    bool getDetectedTone(int &maxTone) const
    {
    	maxTone = m_maxPowerIndex;
    	return m_toneDetected;
    }

    // Get the max m_power at the detected tone.
    Real getMaxPower() const {
    	return m_maxPower;
    }

    void reset(); // reset the analysis algorithm

protected:
    // Override these to change behavior of the detector
    virtual void initializePower();
    virtual void evaluatePower();
    void feedback(Real sample);
    void feedForward();

private:
    int m_N;
    int m_sampleRate;
    int m_samplesProcessed;
    int m_maxPowerIndex;
    bool m_toneDetected;
    Real m_maxPower;
    Real *m_k;
    Real *m_coef;
    Real *m_u0;
    Real *m_u1;
    Real *m_power;
};


#endif /* INCLUDE_GPL_DSP_CTCSSDETECTOR_H_ */
