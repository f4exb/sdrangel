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

#ifndef INCLUDE_UTIL_PRETTYPRINT_H_
#define INCLUDE_UTIL_PRETTYPRINT_H_

#include <QString>

#include "export.h"

class SDRBASE_API EscapeColors
{
public:
	static const QString red;
	static const QString blue;
	static const QString green;
	static const QString cyan;
	static const QString purple;
	static const QString yellow;
	static const QString lightRed;
	static const QString lightBlue;
	static const QString lightGreen;
	static const QString lightCyan;
	static const QString lightPurple;
	static const QString brown;
	static const QString terminator;
};

#endif /* INCLUDE_UTIL_PRETTYPRINT_H_ */
