///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_FFTWINDOW_H
#define INCLUDE_FFTWINDOW_H

#include <vector>
#include <cmath>
#include "dsp/dsptypes.h"
#include "export.h"

class SDRBASE_API FFTWindow {
public:
	enum Function {
		Bartlett,
		BlackmanHarris,
		Flattop,
		Hamming,
		Hanning,
		Rectangle,
		Kaiser,
        Blackman,
        BlackmanHarris7
	};

	FFTWindow();

	void create(Function function, int n);
	void apply(const std::vector<Real>& in, std::vector<Real>* out);
	void apply(const std::vector<Complex>& in, std::vector<Complex>* out);
    void apply(std::vector<Complex>& in);
	void apply(const Complex* in, Complex* out);
    void apply(Complex* in);
	void setKaiserAlpha(Real alpha); //!< set the Kaiser window alpha factor (default 2.15)
	void setKaiserBeta(Real beta);   //!< set the Kaiser window beta factor = pi * alpha

private:
	std::vector<float> m_window;
	Real m_kaiserAlpha;    //!< alpha factor for Kaiser window
	Real m_kaiserI0Alpha;  //!< zeroethOrderBessel of alpha above

	static inline Real flatTop(Real n, Real i)
	{
		// correction ?
		return 1.0 - 1.93 * cos((2.0 * M_PI * i) / n) + 1.29 * cos((4.0 * M_PI * i) / n) - 0.388 * cos((6.0 * M_PI * i) / n) + 0.03222 * cos((8.0 * M_PI * i) / n);
	}

	static inline Real bartlett(Real n, Real i)
	{
		// amplitude correction = 2.0
		return (2.0 / (n - 1.0)) * ( (n - 1.0) / 2.0 - fabs(i - (n - 1.0) / 2.0)) * 2.0;
	}

	static inline Real blackmanHarris(Real n, Real i) // 4 term Blackman-Harris
	{
		// amplitude correction = 2.79
		return (0.35875
            - 0.48829 * cos((2.0 * M_PI * i) / n)
            + 0.14128 * cos((4.0 * M_PI * i) / n)
            - 0.01168 * cos((6.0 * M_PI * i) / n)) * 2.79;
	}

	static inline Real blackmanHarris7(Real n, Real i) // 7 term Blackman-Harris
	{
		return (0.27105
            - 0.43330 * cos((2.0 * M_PI * i) / n)
            + 0.21812 * cos((4.0 * M_PI * i) / n)
            - 0.065925 * cos((6.0 * M_PI * i) / n)
            + 0.010812 * cos((8.0 * M_PI * i) / n)
            - 0.00077658 * cos((10.0 * M_PI * i) / n)
            + 0.000013887 * cos((12.0 * M_PI * i) / n)) * 3.72;
	}

    static inline Real blackman(Real n, Real i) // 3 term Blackman
    {
        return (0.42438
            - 0.49734 * cos(2.0 * M_PI * i / n)
            + 0.078279 * cos(4.0 * M_PI * i / n)) * 2.37;
    }

	static inline Real hamming(Real n, Real i)
	{
		// amplitude correction = 1.855, energy correction = 1.586
		return (0.54 - 0.46 * cos((2.0 * M_PI * i) / n)) * 1.855;
	}

	static inline Real hanning(Real n, Real i)
	{
		// amplitude correction = 2.0, energy correction = 1.633
		return (0.5 - 0.5 * cos((2.0 * M_PI * i) / n)) * 2.0;
	}

	static inline Real rectangle(Real, Real)
	{
		return 1.0;
	}

	// https://raw.githubusercontent.com/johnglover/simpl/master/src/loris/KaiserWindow.C
	inline Real kaiser(Real n, Real i)
	{
		Real K = ((2.0*i) / n) - 1.0;
		Real arg = sqrt(1.0 - (K*K));
		return zeroethOrderBessel(m_kaiserAlpha*arg) / m_kaiserI0Alpha;
	}

	static inline Real zeroethOrderBessel( Real x )
	{
		const Real eps = 0.000001;

		//  initialize the series term for m=0 and the result
		Real besselValue = 0;
		Real term = 1;
		Real m = 0;

		//  accumulate terms as long as they are significant
		while(term  > eps * besselValue)
		{
			besselValue += term;

			//  update the term
			++m;
			term *= (x*x) / (4*m*m);
		}

		return besselValue;
	}
};

#endif // INCLUDE_FFTWINDOWS_H
