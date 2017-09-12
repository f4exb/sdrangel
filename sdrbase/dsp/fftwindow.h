///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

#ifndef INCLUDE_FFTWINDOW_H
#define INCLUDE_FFTWINDOW_H

#include <vector>
#define _USE_MATH_DEFINES
#include <math.h>
#include "dsp/dsptypes.h"
#include "util/export.h"

#undef M_PI
#define M_PI		3.14159265358979323846

class SDRANGEL_API FFTWindow {
public:
	enum Function {
		Bartlett,
		BlackmanHarris,
		Flattop,
		Hamming,
		Hanning,
		Rectangle
	};

	void create(Function function, int n);
	void apply(const std::vector<Real>& in, std::vector<Real>* out);
	void apply(const std::vector<Complex>& in, std::vector<Complex>* out);
	void apply(const Complex* in, Complex* out);

private:
	std::vector<float> m_window;

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

	static inline Real blackmanHarris(Real n, Real i)
	{
		// amplitude correction = 2.79
		return (0.35875 - 0.48829 * cos((2.0 * M_PI * i) / n) + 0.14128 * cos((4.0 * M_PI * i) / n) - 0.01168 * cos((6.0 * M_PI * i) / n)) * 2.79;
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
};

#endif // INCLUDE_FFTWINDOWS_H
