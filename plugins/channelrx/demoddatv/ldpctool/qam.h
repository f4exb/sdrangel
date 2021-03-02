/*
Quadrature amplitude modulation

Copyright 2018 Ahmet Inan <xdsopl@gmail.com>
*/

#ifndef QAM_HH
#define QAM_HH

namespace ldpctool {

template <int NUM, typename TYPE, typename CODE>
struct QuadratureAmplitudeModulation;

template <typename TYPE, typename CODE>
struct QuadratureAmplitudeModulation<16, TYPE, CODE>
{
	static const int NUM = 16;
	static const int BITS = 4;
	typedef TYPE complex_type;
	typedef typename TYPE::value_type value_type;
	typedef CODE code_type;

	static constexpr value_type FAC = 1.0540925533894596;
	static constexpr value_type RCP = 3 * FAC;
	static constexpr value_type AMP = 1 / RCP;
	static constexpr value_type DIST = 2 * AMP;

	static constexpr value_type amp(int i)
	{
		return AMP * i;
	}

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
		b[0] = c.real() < amp(0) ? code_type(-1) : code_type(1);
		b[1] = c.imag() < amp(0) ? code_type(-1) : code_type(1);
		b[2] = std::abs(c.real()) < amp(2) ? code_type(-1) : code_type(1);
		b[3] = std::abs(c.imag()) < amp(2) ? code_type(-1) : code_type(1);
	}

	static void soft(code_type *b, complex_type c, value_type precision)
	{
		b[0] = quantize(precision, c.real());
		b[1] = quantize(precision, c.imag());
		b[2] = quantize(precision, std::abs(c.real())-amp(2));
		b[3] = quantize(precision, std::abs(c.imag())-amp(2));
	}

	static complex_type map(code_type *b)
	{
		return AMP * complex_type(
			b[0]*(b[2]+value_type(2)),
			b[1]*(b[3]+value_type(2))
		);
	}
};

template <typename TYPE, typename CODE>
struct QuadratureAmplitudeModulation<64, TYPE, CODE>
{
	static const int NUM = 64;
	static const int BITS = 6;
	typedef TYPE complex_type;
	typedef typename TYPE::value_type value_type;
	typedef CODE code_type;

	static constexpr value_type FAC = 0.9258200997725516;
	static constexpr value_type RCP = 7 * FAC;
	static constexpr value_type AMP = 1 / RCP;
	static constexpr value_type DIST = 2 * AMP;

	static constexpr value_type amp(int i)
	{
		return AMP * i;
	}

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
		b[0] = c.real() < amp(0) ? code_type(-1) : code_type(1);
		b[1] = c.imag() < amp(0) ? code_type(-1) : code_type(1);
		b[2] = std::abs(c.real()) < amp(4) ? code_type(-1) : code_type(1);
		b[3] = std::abs(c.imag()) < amp(4) ? code_type(-1) : code_type(1);
		b[4] = std::abs(std::abs(c.real())-amp(4)) < amp(2) ? code_type(-1) : code_type(1);
		b[5] = std::abs(std::abs(c.imag())-amp(4)) < amp(2) ? code_type(-1) : code_type(1);
	}

	static void soft(code_type *b, complex_type c, value_type precision)
	{
		b[0] = quantize(precision, c.real());
		b[1] = quantize(precision, c.imag());
		b[2] = quantize(precision, std::abs(c.real())-amp(4));
		b[3] = quantize(precision, std::abs(c.imag())-amp(4));
		b[4] = quantize(precision, std::abs(std::abs(c.real())-amp(4))-amp(2));
		b[5] = quantize(precision, std::abs(std::abs(c.imag())-amp(4))-amp(2));
	}

	static complex_type map(code_type *b)
	{
		return AMP * complex_type(
			b[0]*(b[2]*(b[4]+value_type(2))+value_type(4)),
			b[1]*(b[3]*(b[5]+value_type(2))+value_type(4))
		);
	}
};

template <typename TYPE, typename CODE>
struct QuadratureAmplitudeModulation<256, TYPE, CODE>
{
	static const int NUM = 256;
	static const int BITS = 8;
	typedef TYPE complex_type;
	typedef typename TYPE::value_type value_type;
	typedef CODE code_type;

	static constexpr value_type FAC = 0.8692269873603529;
	static constexpr value_type RCP = 15 * FAC;
	static constexpr value_type AMP = 1 / RCP;
	static constexpr value_type DIST = 2 * AMP;

	static constexpr value_type amp(int i)
	{
		return AMP * i;
	}

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
		b[0] = c.real() < amp(0) ? code_type(-1) : code_type(1);
		b[1] = c.imag() < amp(0) ? code_type(-1) : code_type(1);
		b[2] = std::abs(c.real()) < amp(8) ? code_type(-1) : code_type(1);
		b[3] = std::abs(c.imag()) < amp(8) ? code_type(-1) : code_type(1);
		b[4] = std::abs(std::abs(c.real())-amp(8)) < amp(4) ? code_type(-1) : code_type(1);
		b[5] = std::abs(std::abs(c.imag())-amp(8)) < amp(4) ? code_type(-1) : code_type(1);
		b[6] = std::abs(std::abs(std::abs(c.real())-amp(8))-amp(4)) < amp(2) ? code_type(-1) : code_type(1);
		b[7] = std::abs(std::abs(std::abs(c.imag())-amp(8))-amp(4)) < amp(2) ? code_type(-1) : code_type(1);
	}

	static void soft(code_type *b, complex_type c, value_type precision)
	{
		b[0] = quantize(precision, c.real());
		b[1] = quantize(precision, c.imag());
		b[2] = quantize(precision, std::abs(c.real())-amp(8));
		b[3] = quantize(precision, std::abs(c.imag())-amp(8));
		b[4] = quantize(precision, std::abs(std::abs(c.real())-amp(8))-amp(4));
		b[5] = quantize(precision, std::abs(std::abs(c.imag())-amp(8))-amp(4));
		b[6] = quantize(precision, std::abs(std::abs(std::abs(c.real())-amp(8))-amp(4))-amp(2));
		b[7] = quantize(precision, std::abs(std::abs(std::abs(c.imag())-amp(8))-amp(4))-amp(2));
	}

	static complex_type map(code_type *b)
	{
		return AMP * complex_type(
			b[0]*(b[2]*(b[4]*(b[6]+value_type(2))+value_type(4))+value_type(8)),
			b[1]*(b[3]*(b[5]*(b[7]+value_type(2))+value_type(4))+value_type(8))
		);
	}
};

template <typename TYPE, typename CODE>
struct QuadratureAmplitudeModulation<1024, TYPE, CODE>
{
	static const int NUM = 1024;
	static const int BITS = 10;
	typedef TYPE complex_type;
	typedef typename TYPE::value_type value_type;
	typedef CODE code_type;

	static constexpr value_type FAC = 0.8424235391742344;
	static constexpr value_type RCP = 31 * FAC;
	static constexpr value_type AMP = 1 / RCP;
	static constexpr value_type DIST = 2 * AMP;

	static constexpr value_type amp(int i)
	{
		return AMP * i;
	}

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
		b[0] = c.real() < amp(0) ? code_type(-1) : code_type(1);
		b[1] = c.imag() < amp(0) ? code_type(-1) : code_type(1);
		b[2] = std::abs(c.real()) < amp(16) ? code_type(-1) : code_type(1);
		b[3] = std::abs(c.imag()) < amp(16) ? code_type(-1) : code_type(1);
		b[4] = std::abs(std::abs(c.real())-amp(16)) < amp(8) ? code_type(-1) : code_type(1);
		b[5] = std::abs(std::abs(c.imag())-amp(16)) < amp(8) ? code_type(-1) : code_type(1);
		b[6] = std::abs(std::abs(std::abs(c.real())-amp(16))-amp(8)) < amp(4) ? code_type(-1) : code_type(1);
		b[7] = std::abs(std::abs(std::abs(c.imag())-amp(16))-amp(8)) < amp(4) ? code_type(-1) : code_type(1);
		b[8] = std::abs(std::abs(std::abs(std::abs(c.real())-amp(16))-amp(8))-amp(4)) < amp(2) ? code_type(-1) : code_type(1);
		b[9] = std::abs(std::abs(std::abs(std::abs(c.imag())-amp(16))-amp(8))-amp(4)) < amp(2) ? code_type(-1) : code_type(1);
	}

	static void soft(code_type *b, complex_type c, value_type precision)
	{
		b[0] = quantize(precision, c.real());
		b[1] = quantize(precision, c.imag());
		b[2] = quantize(precision, std::abs(c.real())-amp(16));
		b[3] = quantize(precision, std::abs(c.imag())-amp(16));
		b[4] = quantize(precision, std::abs(std::abs(c.real())-amp(16))-amp(8));
		b[5] = quantize(precision, std::abs(std::abs(c.imag())-amp(16))-amp(8));
		b[6] = quantize(precision, std::abs(std::abs(std::abs(c.real())-amp(16))-amp(8))-amp(4));
		b[7] = quantize(precision, std::abs(std::abs(std::abs(c.imag())-amp(16))-amp(8))-amp(4));
		b[8] = quantize(precision, std::abs(std::abs(std::abs(std::abs(c.real())-amp(16))-amp(8))-amp(4))-amp(2));
		b[9] = quantize(precision, std::abs(std::abs(std::abs(std::abs(c.imag())-amp(16))-amp(8))-amp(4))-amp(2));
	}

	static complex_type map(code_type *b)
	{
		return AMP * complex_type(
			b[0]*(b[2]*(b[4]*(b[6]*(b[8]+value_type(2))+value_type(4))+value_type(8))+value_type(16)),
			b[1]*(b[3]*(b[5]*(b[7]*(b[9]+value_type(2))+value_type(4))+value_type(8))+value_type(16))
		);
	}
};

} // namespace ldpctool

#endif

