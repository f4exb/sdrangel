/*---------------------------------------------------------------------------*\

  FILE........: comp_prim.h
  AUTHOR......: David Rowe
  DATE CREATED: Marh 2015

  Complex number maths primitives.

\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2015 David Rowe

  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License version 2.1, as
  published by the Free Software Foundation.  This program is
  distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __COMP_PRIM__
#define __COMP_PRIM__

#include <math.h>
#include "codec2/comp.h"

namespace FreeDV
{

/*---------------------------------------------------------------------------*\

                               FUNCTIONS

\*---------------------------------------------------------------------------*/

inline static COMP cneg(COMP a)
{
    COMP res;

    res.real = -a.real;
    res.imag = -a.imag;

    return res;
}

inline static COMP cconj(COMP a)
{
    COMP res;

    res.real = a.real;
    res.imag = -a.imag;

    return res;
}

inline static COMP cmult(COMP a, COMP b)
{
    COMP res;

    res.real = a.real*b.real - a.imag*b.imag;
    res.imag = a.real*b.imag + a.imag*b.real;

    return res;
}

inline static COMP fcmult(float a, COMP b)
{
    COMP res;

    res.real = a*b.real;
    res.imag = a*b.imag;

    return res;
}

inline static COMP cadd(COMP a, COMP b)
{
    COMP res;

    res.real = a.real + b.real;
    res.imag = a.imag + b.imag;

    return res;
}

inline static float cabsolute(COMP a)
{
    return sqrt(pow(a.real, 2.0) + pow(a.imag, 2.0));
}

/*
 * Euler's formula in a new convenient function
 */
inline static COMP comp_exp_j(float phi){
    COMP res;
    res.real = cosf(phi);
    res.imag = sinf(phi);
    return res;
}

/*
 * Quick and easy complex 0
 */
inline static COMP comp0(){
    COMP res;
    res.real = 0;
    res.imag = 0;
    return res;
}

/*
 * Quick and easy complex subtract
 */
inline static COMP csub(COMP a, COMP b){
    COMP res;
    res.real = a.real-b.real;
    res.imag = a.imag-b.imag;
    return res;
}

/*
 * Compare the magnitude of a and b. if |a|>|b|, return true, otw false.
 * This needs no square roots
 */
inline static int comp_mag_gt(COMP a,COMP b){
    return ((a.real*a.real)+(a.imag*a.imag)) > ((b.real*b.real)+(b.imag*b.imag));
}

/*
 * Normalize a complex number's magnitude to 1
 */
inline static COMP comp_normalize(COMP a){
    COMP b;
    float av = cabsolute(a);
    b.real = a.real/av;
    b.imag = a.imag/av;
    return b;
}

} // FreeDV

#endif
