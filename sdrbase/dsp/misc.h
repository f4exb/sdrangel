// ----------------------------------------------------------------------------
// misc.h  --  Miscellaneous helper functions
//
// Copyright (C) 2006-2008
//		Dave Freese, W1HKJ
//
// This file is part of fldigi.  These filters were adapted from code contained
// in the gmfsk source code distribution.
//
// Fldigi is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Fldigi is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with fldigi.  If not, see <http://www.gnu.org/licenses/>.
// ----------------------------------------------------------------------------

#ifndef _MISC_H
#define _MISC_H

#include <math.h>

#undef M_PI
#define M_PI		3.14159265358979323846

inline float sinc(float x)
{
	return (fabs(x) < 1e-10) ? 1.0 : (sin(M_PI * x) / (M_PI * x));
}

inline float cosc(float x)
{
	return (fabs(x) < 1e-10) ? 0.0 : ((1.0 - cos(M_PI * x)) / (M_PI * x));
}

inline float clamp(float x, float min, float max)
{
	return (x < min) ? min : ((x > max) ? max : x);
}

/// This is always called with an int weight
inline float decayavg(float average, float input, int weight)
{
	if (weight <= 1) return input;
	return ( ( input - average ) / (float)weight ) + average ;
}

// following are defined inline to provide best performance
inline float blackman(float x)
{
	return (0.42 - 0.50 * cos(2 * M_PI * x) + 0.08 * cos(4 * M_PI * x));
}

inline float hamming(float x)
{
	return 0.54 - 0.46 * cos(2 * M_PI * x);
}

inline float hanning(float x)
{
	return 0.5 - 0.5 * cos(2 * M_PI * x);
}

inline float rcos( float t, float T, float alpha=1.0 )
{
    if( t == 0 ) return 1.0;
    float taT = T / (2.0 * alpha);
    if( fabs(t) == taT ) return ((alpha/2.0) * sin(M_PI/(2.0*alpha)));
    return (sin(M_PI*t/T)/(M_PI*t/T))*cos(alpha*M_PI*t/T)/(1.0-(t/taT)*(t/taT));
}

#endif
