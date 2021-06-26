///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_UTIL_DB_H_
#define INCLUDE_UTIL_DB_H_

#include "dsp/dsptypes.h"
#include "export.h"

class SDRBASE_API CalcDb
{
public:
	static double dbPower(double magsq, double floor = 1e-12);
	static double powerFromdB(double powerdB);
	static double frexp10(double arg, int *exp);
};

#endif /* INCLUDE_UTIL_DB_H_ */
