///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Edouard Griffiths, F4EXB <f4exb06@gmail.com>                   //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
/*
Modulation interface

Copyright 2018 Ahmet Inan <xdsopl@gmail.com>
*/

#ifndef MODULATION_HH
#define MODULATION_HH

namespace ldpctool {

template <typename TYPE, typename CODE>
struct ModulationInterface
{
	typedef TYPE complex_type;
	typedef typename TYPE::value_type value_type;
	typedef CODE code_type;

	virtual int bits() = 0;
	virtual void hardN(code_type *, complex_type *) = 0;
	virtual void softN(code_type *, complex_type *, value_type) = 0;
	virtual void mapN(complex_type *, code_type *) = 0;
	virtual void hard(code_type *, complex_type) = 0;
	virtual void soft(code_type *, complex_type, value_type) = 0;
	virtual complex_type map(code_type *) = 0;
	virtual ~ModulationInterface() = default;
};

template <typename MOD, int NUM>
struct Modulation : public ModulationInterface<typename MOD::complex_type, typename MOD::code_type>
{
	typedef typename MOD::complex_type complex_type;
	typedef typename MOD::value_type value_type;
	typedef typename MOD::code_type code_type;

	int bits()
	{
		return MOD::BITS;
	}

	void hardN(code_type *b, complex_type *c)
	{
		for (int i = 0; i < NUM; ++i)
			MOD::hard(b + i * MOD::BITS, c[i]);
	}

	void softN(code_type *b, complex_type *c, value_type precision)
	{
		for (int i = 0; i < NUM; ++i)
			MOD::soft(b + i * MOD::BITS, c[i], precision);
	}

	void mapN(complex_type *c, code_type *b)
	{
		for (int i = 0; i < NUM; ++i)
			c[i] = MOD::map(b + i * MOD::BITS);
	}

	void hard(code_type *b, complex_type c)
	{
		MOD::hard(b, c);
	}

	void soft(code_type *b, complex_type c, value_type precision)
	{
		MOD::soft(b, c, precision);
	}

	complex_type map(code_type *b)
	{
		return MOD::map(b);
	}
};

} // namespace ldpctool

#endif

