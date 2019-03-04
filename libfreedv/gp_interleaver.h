/*---------------------------------------------------------------------------*\

  FILE........: gp_interleaver.h
  AUTHOR......: David Rowe
  DATE CREATED: April 2018

  Golden Prime Interleaver. My interprestation of "On the Analysis and
  Design of Good Algebraic Interleavers", Xie et al,eq (5).

  See also octvae/gp_interleaver.m

\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2018 David Rowe

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

#ifndef __GP_INTERLEAVER__
#define __GP_INTERLEAVER__

#include "codec2/comp.h"

namespace FreeDV
{

void gp_interleave_comp(COMP interleaved_frame[], COMP frame[], int Nbits);
void gp_deinterleave_comp(COMP frame[], COMP interleaved_frame[], int Nbits);
void gp_interleave_float(float frame[], float interleaved_frame[], int Nbits);
void gp_deinterleave_float(float interleaved_frame[], float frame[], int Nbits);

} // FreeDV

#endif
