///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef INCLUDE_DSP_PHASEDISCRI_H_
#define INCLUDE_DSP_PHASEDISCRI_H_

#include "dsp/dsptypes.h"

#undef M_PI
#define M_PI 3.14159265358979323846

class PhaseDiscriminators
{
public:
	/**
	 * Reset stored values
	 */
	void reset()
	{
		m_m1Sample = 0;
		m_m2Sample = 0;
	}

	/**
	 * Scaling factor so that resulting excursion maps to [-1,+1]
	 */
	void setFMScaling(Real fmScaling)
	{
		m_fmScaling = fmScaling;
	}

	/**
	 * Standard discriminator using atan2. On modern processors this is as efficient as the non atan2 one.
	 * This is better for high fidelity.
	 */
	Real phaseDiscriminator(const Complex& sample)
	{
		Complex d(std::conj(m_m1Sample) * sample);
		m_m1Sample = sample;
		return (std::atan2(d.imag(), d.real()) / M_PI) * m_fmScaling;
	}

    /**
     * Discriminator with phase detection using atan2 and frequency by derivation.
     * This yields a precise deviation to sample rate ratio: Sample rate => +/-1.0
     */
    Real phaseDiscriminatorDelta(const Complex& sample, double& magsq, Real& fmDev)
    {
        Real fltI = sample.real();
        Real fltQ = sample.imag();
        magsq = fltI*fltI + fltQ*fltQ;

        Real curArg = atan2_approximation2((float) fltQ, (float) fltI);
        fmDev = (curArg - m_prevArg) / M_PI;
        m_prevArg = curArg;

        if (fmDev < -1.0f) {
            fmDev += 2.0f;
        } else if (fmDev > 1.0f) {
            fmDev -= 2.0f;
        }

        return fmDev * m_fmScaling;
    }

	/**
	 * Alternative without atan at the expense of a slight distorsion on very wideband signals
	 * http://www.embedded.com/design/configurable-systems/4212086/DSP-Tricks--Frequency-demodulation-algorithms-
	 * in addition it needs scaling by instantaneous magnitude squared and volume (0..10) adjustment factor
	 */
	Real phaseDiscriminator2(const Complex& sample)
	{
		Real ip = sample.real() - m_m2Sample.real();
		Real qp = sample.imag() - m_m2Sample.imag();
		Real h1 = m_m1Sample.real() * qp;
		Real h2 = m_m1Sample.imag() * ip;

		m_m2Sample = m_m1Sample;
		m_m1Sample = sample;

		//return ((h1 - h2) / M_PI_2) * m_fmScaling;
		return (h1 - h2) * m_fmScaling;
	}

	/**
	 * Second alternative
	 */
    Real phaseDiscriminator3(const Complex& sample, long double& magsq, Real& fltVal)
    {
        Real fltI = sample.real();
        Real fltQ = sample.imag();
        double fltNorm;
        Real fltNormI;
        Real fltNormQ;
        //Real fltVal;

        magsq = fltI*fltI + fltQ*fltQ;
        fltNorm = std::sqrt(magsq);

        fltNormI= fltI/fltNorm;
        fltNormQ= fltQ/fltNorm;

        fltVal = m_fltPreviousI*(fltNormQ - m_fltPreviousQ2);
        fltVal -= m_fltPreviousQ*(fltNormI - m_fltPreviousI2);
        fltVal += 2.0f;
        fltVal /= 4.0f; // normally it is /4

        m_fltPreviousQ2 = m_fltPreviousQ;
        m_fltPreviousI2 = m_fltPreviousI;

        m_fltPreviousQ = fltNormQ;
        m_fltPreviousI = fltNormI;

        return fltVal * m_fmScaling;
    }

private:
	Complex m_m1Sample;
	Complex m_m2Sample;
	Real m_fmScaling;
	Real m_fltPreviousI;
	Real m_fltPreviousQ;
    Real m_fltPreviousI2;
    Real m_fltPreviousQ2;
    Real m_prevArg;

    float atan2_approximation1(float y, float x)
    {
        //http://pubs.opengroup.org/onlinepubs/009695399/functions/atan2.html
        //Volkan SALMA

        const float ONEQTR_PI = M_PI / 4.0;
        const float THRQTR_PI = 3.0 * M_PI / 4.0;
        float r, angle;
        float abs_y = std::fabs(y) + 1e-10f;      // kludge to prevent 0/0 condition
        if ( x < 0.0f )
        {
            r = (x + abs_y) / (abs_y - x);
            angle = THRQTR_PI;
        }
        else
        {
            r = (x - abs_y) / (x + abs_y);
            angle = ONEQTR_PI;
        }
        angle += (0.1963f * r * r - 0.9817f) * r;
        if ( y < 0.0f )
            return( -angle );     // negate if in quad III or IV
        else
            return( angle );


    }

    #define PI_FLOAT     3.14159265f
    #define PIBY2_FLOAT  1.5707963f
    // |error| < 0.005
    float atan2_approximation2( float y, float x )
    {
        if ( x == 0.0f )
        {
            if ( y > 0.0f ) return PIBY2_FLOAT;
            if ( y == 0.0f ) return 0.0f;
            return -PIBY2_FLOAT;
        }
        float atan;
        float z = y/x;
        if ( std::fabs( z ) < 1.0f )
        {
            atan = z/(1.0f + 0.28f*z*z);
            if ( x < 0.0f )
            {
                if ( y < 0.0f ) return atan - PI_FLOAT;
                return atan + PI_FLOAT;
            }
        }
        else
        {
            atan = PIBY2_FLOAT - z/(z*z + 0.28f);
            if ( y < 0.0f ) return atan - PI_FLOAT;
        }
        return atan;
    }
};

#endif /* INCLUDE_DSP_PHASEDISCRI_H_ */
