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
Phase-shift keying

Copyright 2018 Ahmet Inan <xdsopl@gmail.com>
*/

#ifndef PSK_HH
#define PSK_HH

namespace ldpctool {

template <int NUM, typename TYPE, typename CODE>
struct PhaseShiftKeying;

template <typename TYPE, typename CODE>
struct PhaseShiftKeying<2, TYPE, CODE>
{
	static const int NUM = 2;
	static const int BITS = 1;
	typedef TYPE complex_type;
	typedef typename TYPE::value_type value_type;
	typedef CODE code_type;

	static constexpr value_type DIST = 2;

	static code_type quantize(value_type precision, value_type value)
	{
		value *= DIST * precision;
		if (std::is_integral<code_type>::value)
			value = std::nearbyint(value);
		if (std::is_same<code_type, int8_t>::value)
			value = std::min<value_type>(std::max<value_type>(value, -128), 127);
		return value;
	}

	static void hard(code_type *b, complex_type c)
	{
		b[0] = c.real() < value_type(0) ? code_type(-1) : code_type(1);
	}

	static void soft(code_type *b, complex_type c, value_type precision)
	{
		b[0] = quantize(precision, c.real());
	}

	static complex_type map(code_type *b)
	{
		return complex_type(b[0], 0);
	}
};

template <typename TYPE, typename CODE>
struct PhaseShiftKeying<4, TYPE, CODE>
{
	static const int NUM = 4;
	static const int BITS = 2;
	typedef TYPE complex_type;
	typedef typename TYPE::value_type value_type;
	typedef CODE code_type;

	// 1/sqrt(2)
	static constexpr value_type rcp_sqrt_2 = 0.70710678118654752440;
	static constexpr value_type DIST = 2 * rcp_sqrt_2;

	static code_type quantize(value_type precision, value_type value)
	{
		value *= DIST * precision;
		if (std::is_integral<code_type>::value)
			value = std::nearbyint(value);
		if (std::is_same<code_type, int8_t>::value)
			value = std::min<value_type>(std::max<value_type>(value, -128), 127);
		return value;
	}

	static void hard(code_type *b, complex_type c)
	{
		b[0] = c.real() < value_type(0) ? code_type(-1) : code_type(1);
		b[1] = c.imag() < value_type(0) ? code_type(-1) : code_type(1);
	}

	static void soft(code_type *b, complex_type c, value_type precision)
	{
		b[0] = quantize(precision, c.real());
		b[1] = quantize(precision, c.imag());
	}

	static complex_type map(code_type *b)
	{
		return rcp_sqrt_2 * complex_type(b[0], b[1]);
	}
};

template <typename TYPE, typename CODE>
struct PhaseShiftKeying<8, TYPE, CODE>
{
	static const int NUM = 8;
	static const int BITS = 3;
	typedef TYPE complex_type;
	typedef typename TYPE::value_type value_type;
	typedef CODE code_type;

	// c(a(1)/2)
	static constexpr value_type cos_pi_8 = 0.92387953251128675613;
	// s(a(1)/2)
	static constexpr value_type sin_pi_8 = 0.38268343236508977173;
	// 1/sqrt(2)
	static constexpr value_type rcp_sqrt_2 = 0.70710678118654752440;

	static constexpr value_type DIST = 2 * sin_pi_8;

	static constexpr complex_type rot_cw = complex_type(cos_pi_8, -sin_pi_8);
	static constexpr complex_type rot_acw = complex_type(cos_pi_8, sin_pi_8);

	static code_type quantize(value_type precision, value_type value)
	{
		value *= DIST * precision;
		if (std::is_integral<code_type>::value)
			value = std::nearbyint(value);
		if (std::is_same<code_type, int8_t>::value)
			value = std::min<value_type>(std::max<value_type>(value, -128), 127);
		return value;
	}

	static void hard(code_type *b, complex_type c)
	{
		c *= rot_cw;
		b[1] = c.real() < value_type(0) ? code_type(-1) : code_type(1);
		b[2] = c.imag() < value_type(0) ? code_type(-1) : code_type(1);
		b[0] = std::abs(c.real()) < std::abs(c.imag()) ? code_type(-1) : code_type(1);
	}

	static void soft(code_type *b, complex_type c, value_type precision)
	{
		c *= rot_cw;
		b[1] = quantize(precision, c.real());
		b[2] = quantize(precision, c.imag());
		b[0] = quantize(precision, rcp_sqrt_2 * (std::abs(c.real()) - std::abs(c.imag())));
	}

	static complex_type map(code_type *b)
	{
		value_type real = cos_pi_8;
		value_type imag = sin_pi_8;
		if (b[0] < code_type(0))
			std::swap(real, imag);
		return complex_type(real * b[1], imag * b[2]) * rot_acw;
	}
};

} // namespace ldpctool

#endif

