/*---------------------------------------------------------------------------*\

  FILE........: linreg.c
  AUTHOR......: David Rowe
  DATE CREATED: April 2015

  Linear regression C module based on:

    http://stackoverflow.com/questions/5083465/fast-efficient-least-squares-fit-algorithm-in-c

  Use:

    $ gcc linreg.c -o linreg -D__UNITTEST__ -Wall
    $ ./linreg

    Then compare yfit results with octave/tlinreg.m

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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "linreg.h"
#include "comp_prim.h"

namespace FreeDV
{

void linreg(COMP *m, COMP *b, float x[], COMP y[], int n)
{
    float  sumx  = 0.0;         /* sum of x          */
    float  sumx2 = 0.0;         /* sum of x^2        */
    COMP   sumxy = {0.0,0.0};   /* sum of x * y      */
    COMP   sumy  = {0.0,0.0};   /* sum of y          */
    COMP   sumy2 = {0.0,0.0};   /* sum of y**2       */
    float  denom;
    COMP   zero;
    int    i;

    for (i=0; i<n; i++) {
        sumx  += x[i];
        sumx2 += x[i]*x[i];
        sumxy = cadd(sumxy, fcmult(x[i], y[i]));
        sumy  = cadd(sumy, y[i]);
        sumy2 = cadd(sumy2, cmult(y[i],y[i]));
    }

  denom = (n * sumx2 - sumx*sumx);

  if (denom == 0) {
      /* singular matrix. can't solve the problem */
      zero.real = 0.0; zero.imag = 0.0;
      *m = zero;
      *b = zero;
  } else {
      *m = fcmult(1.0/denom, cadd(fcmult(n, sumxy), cneg(fcmult(sumx,sumy))));
      *b = fcmult(1.0/denom, cadd(fcmult(sumx2,sumy), cneg(fcmult(sumx, sumxy))));
  }
}


#ifdef __UNITTEST__


static float x[] = {1, 2, 7, 8};

static COMP  y[] = {
  {-0.70702,  0.70708},
  {-0.77314,  0.63442},
  {-0.98083,  0.19511},
  {-0.99508,  0.09799}
};


int main(void) {
    float  x1;
    COMP   m,b,yfit;

    linreg(&m, &b, x, y, sizeof(x)/sizeof(float));

    for (x1=1; x1<=8; x1++) {
        yfit = cadd(fcmult(x1, m),b);
        printf("%f %f\n", yfit.real, yfit.imag);
    }

    return 0;
}

#endif

} // FreeDV

