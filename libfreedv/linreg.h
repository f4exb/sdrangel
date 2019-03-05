/*---------------------------------------------------------------------------*\

  FILE........: linreg.h
  AUTHOR......: David Rowe
  DATE CREATED: April 2015

  Linear regression C module based

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

#ifndef __LINREG__
#define __LINREG__

#include "codec2/comp.h"

namespace FreeDV
{

void linreg(COMP *m, COMP *b, float x[], COMP y[], int n);

} // FreeDV

#endif
