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

