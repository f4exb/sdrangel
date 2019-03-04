/*---------------------------------------------------------------------------*\

  FILE........: fdv_arm_math.h
  AUTHOR......: David Rowe
  DATE CREATED: Feb 13 2019

  Bundles access to ARM CORTEX M specific functions which are enabled by
  defining FDV_ARM_MATH

\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2012 David Rowe

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

#ifndef __FDV_ARM_MATH__
#define __FDV_ARM_MATH__

#ifdef FDV_ARM_MATH
    #include "arm_const_structs.h"
    #define SINF(a) arm_sin_f32(a)
    #define COSF(a) arm_cos_f32(a)
#else
    #define SINF(a) sinf(a)
    #define COSF(a) cosf(a)
#endif

#endif
