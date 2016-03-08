// ----------------------------------------------------------------------------
// complex.h  --  Complex arithmetic
//
// Copyright (C) 2006-2008
//		Dave Freese, W1HKJ
// Copyright (C) 2008
//		Stelios Bounanos, M0GLD
//
// This file is part of fldigi.
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

#ifndef _COMPLEX_H
#define _COMPLEX_H

#include <cmath>
#include <complex>

typedef std::complex<float> cmplx;

inline cmplx cmac (const cmplx *a, const cmplx *b, int ptr, int len) {
	cmplx z;
	ptr %= len;
	for (int i = 0; i < len; i++) {
		z += a[i] * b[ptr];
		ptr = (ptr + 1) % len;
		}
	return z;
}

#endif
