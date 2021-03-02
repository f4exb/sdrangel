/*
Single instruction, multiple data

Copyright 2018 Ahmet Inan <inan@aicodix.de>
*/

#ifndef SIMD_HH
#define SIMD_HH

#include <cstdint>
#include <cstdlib>
#include <cmath>

namespace ldpctool {

template <typename TYPE, int WIDTH>
union SIMD;

template <int WIDTH>
union SIMD<float, WIDTH>
{
	static const int SIZE = WIDTH;
	typedef float value_type;
	typedef uint32_t uint_type;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <int WIDTH>
union SIMD<double, WIDTH>
{
	static const int SIZE = WIDTH;
	typedef double value_type;
	typedef uint64_t uint_type;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <int WIDTH>
union SIMD<int8_t, WIDTH>
{
	static const int SIZE = WIDTH;
	typedef int8_t value_type;
	typedef uint8_t uint_type;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <int WIDTH>
union SIMD<int16_t, WIDTH>
{
	static const int SIZE = WIDTH;
	typedef int16_t value_type;
	typedef uint16_t uint_type;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <int WIDTH>
union SIMD<int32_t, WIDTH>
{
	static const int SIZE = WIDTH;
	typedef int32_t value_type;
	typedef uint32_t uint_type;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <int WIDTH>
union SIMD<int64_t, WIDTH>
{
	static const int SIZE = WIDTH;
	typedef int64_t value_type;
	typedef uint64_t uint_type;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <int WIDTH>
union SIMD<uint8_t, WIDTH>
{
	static const int SIZE = WIDTH;
	typedef uint8_t value_type;
	typedef uint8_t uint_type;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <int WIDTH>
union SIMD<uint16_t, WIDTH>
{
	static const int SIZE = WIDTH;
	typedef uint16_t value_type;
	typedef uint16_t uint_type;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <int WIDTH>
union SIMD<uint32_t, WIDTH>
{
	static const int SIZE = WIDTH;
	typedef uint32_t value_type;
	typedef uint32_t uint_type;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <int WIDTH>
union SIMD<uint64_t, WIDTH>
{
	static const int SIZE = WIDTH;
	typedef uint64_t value_type;
	typedef uint64_t uint_type;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <typename TYPE>
static inline TYPE vdup(typename TYPE::value_type a)
{
	TYPE tmp;
	for (int i = 0; i < TYPE::SIZE; ++i)
		tmp.v[i] = a;
	return tmp;
}

template <typename TYPE>
static inline TYPE vzero()
{
	TYPE tmp;
	for (int i = 0; i < TYPE::SIZE; ++i)
		tmp.u[i] ^= tmp.u[i];
	return tmp;
}

template <typename DST, typename SRC>
static inline DST vreinterpret(SRC a)
{
	static_assert(SRC::SIZE == DST::SIZE, "source and destination width must be same");
	static_assert(sizeof(typename SRC::value_type) == sizeof(typename DST::value_type), "source and destination value type sizes must be same");
	DST tmp;
	for (int i = 0; i < DST::SIZE; ++i)
		tmp.u[i] = a.u[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint32_t, WIDTH> vmask(SIMD<float, WIDTH> a)
{
	return vreinterpret<SIMD<uint32_t, WIDTH>>(a);
}

template <int WIDTH>
static inline SIMD<uint64_t, WIDTH> vmask(SIMD<double, WIDTH> a)
{
	return vreinterpret<SIMD<uint64_t, WIDTH>>(a);
}

template <int WIDTH>
static inline SIMD<uint8_t, WIDTH> vmask(SIMD<int8_t, WIDTH> a)
{
	return vreinterpret<SIMD<uint8_t, WIDTH>>(a);
}

template <int WIDTH>
static inline SIMD<uint16_t, WIDTH> vmask(SIMD<int16_t, WIDTH> a)
{
	return vreinterpret<SIMD<uint16_t, WIDTH>>(a);
}

template <int WIDTH>
static inline SIMD<uint32_t, WIDTH> vmask(SIMD<int32_t, WIDTH> a)
{
	return vreinterpret<SIMD<uint32_t, WIDTH>>(a);
}

template <int WIDTH>
static inline SIMD<uint64_t, WIDTH> vmask(SIMD<int64_t, WIDTH> a)
{
	return vreinterpret<SIMD<uint64_t, WIDTH>>(a);
}

template <int WIDTH>
static inline SIMD<uint8_t, WIDTH> vunsigned(SIMD<int8_t, WIDTH> a)
{
	return vreinterpret<SIMD<uint8_t, WIDTH>>(a);
}

template <int WIDTH>
static inline SIMD<uint16_t, WIDTH> vunsigned(SIMD<int16_t, WIDTH> a)
{
	return vreinterpret<SIMD<uint16_t, WIDTH>>(a);
}

template <int WIDTH>
static inline SIMD<uint32_t, WIDTH> vunsigned(SIMD<int32_t, WIDTH> a)
{
	return vreinterpret<SIMD<uint32_t, WIDTH>>(a);
}

template <int WIDTH>
static inline SIMD<uint64_t, WIDTH> vunsigned(SIMD<int64_t, WIDTH> a)
{
	return vreinterpret<SIMD<uint64_t, WIDTH>>(a);
}

template <int WIDTH>
static inline SIMD<int8_t, WIDTH> vsigned(SIMD<uint8_t, WIDTH> a)
{
	return vreinterpret<SIMD<int8_t, WIDTH>>(a);
}

template <int WIDTH>
static inline SIMD<int16_t, WIDTH> vsigned(SIMD<uint16_t, WIDTH> a)
{
	return vreinterpret<SIMD<int16_t, WIDTH>>(a);
}

template <int WIDTH>
static inline SIMD<int32_t, WIDTH> vsigned(SIMD<uint32_t, WIDTH> a)
{
	return vreinterpret<SIMD<int32_t, WIDTH>>(a);
}

template <int WIDTH>
static inline SIMD<int64_t, WIDTH> vsigned(SIMD<uint64_t, WIDTH> a)
{
	return vreinterpret<SIMD<int64_t, WIDTH>>(a);
}

template <int WIDTH>
static inline SIMD<float, WIDTH> vneg(SIMD<float, WIDTH> a)
{
	SIMD<float, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -a.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<double, WIDTH> vneg(SIMD<double, WIDTH> a)
{
	SIMD<double, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -a.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<int8_t, WIDTH> vneg(SIMD<int8_t, WIDTH> a)
{
	SIMD<int8_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -a.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<int16_t, WIDTH> vneg(SIMD<int16_t, WIDTH> a)
{
	SIMD<int16_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -a.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<int32_t, WIDTH> vneg(SIMD<int32_t, WIDTH> a)
{
	SIMD<int32_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -a.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<int64_t, WIDTH> vneg(SIMD<int64_t, WIDTH> a)
{
	SIMD<int64_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -a.v[i];
	return tmp;
}

template <typename TYPE, int WIDTH>
static inline SIMD<TYPE, WIDTH> vabs(SIMD<TYPE, WIDTH> a)
{
	SIMD<TYPE, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = std::abs(a.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<int8_t, WIDTH> vqabs(SIMD<int8_t, WIDTH> a)
{
	SIMD<int8_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = std::abs(std::max<int8_t>(a.v[i], -INT8_MAX));
	return tmp;
}

template <int WIDTH>
static inline SIMD<int16_t, WIDTH> vqabs(SIMD<int16_t, WIDTH> a)
{
	SIMD<int16_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = std::abs(std::max<int16_t>(a.v[i], -INT16_MAX));
	return tmp;
}

template <int WIDTH>
static inline SIMD<int32_t, WIDTH> vqabs(SIMD<int32_t, WIDTH> a)
{
	SIMD<int32_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = std::abs(std::max<int32_t>(a.v[i], -INT32_MAX));
	return tmp;
}

template <int WIDTH>
static inline SIMD<int64_t, WIDTH> vqabs(SIMD<int64_t, WIDTH> a)
{
	SIMD<int64_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = std::abs(std::max<int64_t>(a.v[i], -INT64_MAX));
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint8_t, WIDTH> vnot(SIMD<uint8_t, WIDTH> a)
{
	SIMD<uint8_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = ~a.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint16_t, WIDTH> vnot(SIMD<uint16_t, WIDTH> a)
{
	SIMD<uint16_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = ~a.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint32_t, WIDTH> vnot(SIMD<uint32_t, WIDTH> a)
{
	SIMD<uint32_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = ~a.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint64_t, WIDTH> vnot(SIMD<uint64_t, WIDTH> a)
{
	SIMD<uint64_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = ~a.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint8_t, WIDTH> vorr(SIMD<uint8_t, WIDTH> a, SIMD<uint8_t, WIDTH> b)
{
	SIMD<uint8_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] | b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint16_t, WIDTH> vorr(SIMD<uint16_t, WIDTH> a, SIMD<uint16_t, WIDTH> b)
{
	SIMD<uint16_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] | b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint32_t, WIDTH> vorr(SIMD<uint32_t, WIDTH> a, SIMD<uint32_t, WIDTH> b)
{
	SIMD<uint32_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] | b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint64_t, WIDTH> vorr(SIMD<uint64_t, WIDTH> a, SIMD<uint64_t, WIDTH> b)
{
	SIMD<uint64_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] | b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint8_t, WIDTH> vand(SIMD<uint8_t, WIDTH> a, SIMD<uint8_t, WIDTH> b)
{
	SIMD<uint8_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] & b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint16_t, WIDTH> vand(SIMD<uint16_t, WIDTH> a, SIMD<uint16_t, WIDTH> b)
{
	SIMD<uint16_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] & b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint32_t, WIDTH> vand(SIMD<uint32_t, WIDTH> a, SIMD<uint32_t, WIDTH> b)
{
	SIMD<uint32_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] & b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint64_t, WIDTH> vand(SIMD<uint64_t, WIDTH> a, SIMD<uint64_t, WIDTH> b)
{
	SIMD<uint64_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] & b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint8_t, WIDTH> veor(SIMD<uint8_t, WIDTH> a, SIMD<uint8_t, WIDTH> b)
{
	SIMD<uint8_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] ^ b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint16_t, WIDTH> veor(SIMD<uint16_t, WIDTH> a, SIMD<uint16_t, WIDTH> b)
{
	SIMD<uint16_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] ^ b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint32_t, WIDTH> veor(SIMD<uint32_t, WIDTH> a, SIMD<uint32_t, WIDTH> b)
{
	SIMD<uint32_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] ^ b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint64_t, WIDTH> veor(SIMD<uint64_t, WIDTH> a, SIMD<uint64_t, WIDTH> b)
{
	SIMD<uint64_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] ^ b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint8_t, WIDTH> vbic(SIMD<uint8_t, WIDTH> a, SIMD<uint8_t, WIDTH> b)
{
	SIMD<uint8_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] & ~b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint16_t, WIDTH> vbic(SIMD<uint16_t, WIDTH> a, SIMD<uint16_t, WIDTH> b)
{
	SIMD<uint16_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] & ~b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint32_t, WIDTH> vbic(SIMD<uint32_t, WIDTH> a, SIMD<uint32_t, WIDTH> b)
{
	SIMD<uint32_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] & ~b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint64_t, WIDTH> vbic(SIMD<uint64_t, WIDTH> a, SIMD<uint64_t, WIDTH> b)
{
	SIMD<uint64_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] & ~b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint8_t, WIDTH> vbsl(SIMD<uint8_t, WIDTH> a, SIMD<uint8_t, WIDTH> b, SIMD<uint8_t, WIDTH> c)
{
	SIMD<uint8_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = (a.v[i] & b.v[i]) | (~a.v[i] & c.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint16_t, WIDTH> vbsl(SIMD<uint16_t, WIDTH> a, SIMD<uint16_t, WIDTH> b, SIMD<uint16_t, WIDTH> c)
{
	SIMD<uint16_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = (a.v[i] & b.v[i]) | (~a.v[i] & c.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint32_t, WIDTH> vbsl(SIMD<uint32_t, WIDTH> a, SIMD<uint32_t, WIDTH> b, SIMD<uint32_t, WIDTH> c)
{
	SIMD<uint32_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = (a.v[i] & b.v[i]) | (~a.v[i] & c.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint64_t, WIDTH> vbsl(SIMD<uint64_t, WIDTH> a, SIMD<uint64_t, WIDTH> b, SIMD<uint64_t, WIDTH> c)
{
	SIMD<uint64_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = (a.v[i] & b.v[i]) | (~a.v[i] & c.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint32_t, WIDTH> vcgtz(SIMD<float, WIDTH> a)
{
	SIMD<uint32_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] > 0.f);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint64_t, WIDTH> vcgtz(SIMD<double, WIDTH> a)
{
	SIMD<uint64_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] > 0.);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint8_t, WIDTH> vcgtz(SIMD<int8_t, WIDTH> a)
{
	SIMD<uint8_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] > 0);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint16_t, WIDTH> vcgtz(SIMD<int16_t, WIDTH> a)
{
	SIMD<uint16_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] > 0);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint32_t, WIDTH> vcgtz(SIMD<int32_t, WIDTH> a)
{
	SIMD<uint32_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] > 0);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint64_t, WIDTH> vcgtz(SIMD<int64_t, WIDTH> a)
{
	SIMD<uint64_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] > 0);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint32_t, WIDTH> vceqz(SIMD<float, WIDTH> a)
{
	SIMD<uint32_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] == 0.f);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint64_t, WIDTH> vceqz(SIMD<double, WIDTH> a)
{
	SIMD<uint64_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] == 0.);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint8_t, WIDTH> vceqz(SIMD<int8_t, WIDTH> a)
{
	SIMD<uint8_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -!a.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint16_t, WIDTH> vceqz(SIMD<int16_t, WIDTH> a)
{
	SIMD<uint16_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -!a.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint32_t, WIDTH> vceqz(SIMD<int32_t, WIDTH> a)
{
	SIMD<uint32_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -!a.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint64_t, WIDTH> vceqz(SIMD<int64_t, WIDTH> a)
{
	SIMD<uint64_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -!a.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint32_t, WIDTH> vcltz(SIMD<float, WIDTH> a)
{
	SIMD<uint32_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] < 0.f);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint64_t, WIDTH> vcltz(SIMD<double, WIDTH> a)
{
	SIMD<uint64_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] < 0.);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint8_t, WIDTH> vcltz(SIMD<int8_t, WIDTH> a)
{
	SIMD<uint8_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] < 0);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint16_t, WIDTH> vcltz(SIMD<int16_t, WIDTH> a)
{
	SIMD<uint16_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] < 0);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint32_t, WIDTH> vcltz(SIMD<int32_t, WIDTH> a)
{
	SIMD<uint32_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] < 0);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint64_t, WIDTH> vcltz(SIMD<int64_t, WIDTH> a)
{
	SIMD<uint64_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] < 0);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint32_t, WIDTH> vcgt(SIMD<float, WIDTH> a, SIMD<float, WIDTH> b)
{
	SIMD<uint32_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] > b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint64_t, WIDTH> vcgt(SIMD<double, WIDTH> a, SIMD<double, WIDTH> b)
{
	SIMD<uint64_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] > b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint8_t, WIDTH> vcgt(SIMD<int8_t, WIDTH> a, SIMD<int8_t, WIDTH> b)
{
	SIMD<uint8_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] > b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint16_t, WIDTH> vcgt(SIMD<int16_t, WIDTH> a, SIMD<int16_t, WIDTH> b)
{
	SIMD<uint16_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] > b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint32_t, WIDTH> vcgt(SIMD<int32_t, WIDTH> a, SIMD<int32_t, WIDTH> b)
{
	SIMD<uint32_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] > b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint64_t, WIDTH> vcgt(SIMD<int64_t, WIDTH> a, SIMD<int64_t, WIDTH> b)
{
	SIMD<uint64_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] > b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint8_t, WIDTH> vcgt(SIMD<uint8_t, WIDTH> a, SIMD<uint8_t, WIDTH> b)
{
	SIMD<uint8_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] > b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint16_t, WIDTH> vcgt(SIMD<uint16_t, WIDTH> a, SIMD<uint16_t, WIDTH> b)
{
	SIMD<uint16_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] > b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint32_t, WIDTH> vcgt(SIMD<uint32_t, WIDTH> a, SIMD<uint32_t, WIDTH> b)
{
	SIMD<uint32_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] > b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint64_t, WIDTH> vcgt(SIMD<uint64_t, WIDTH> a, SIMD<uint64_t, WIDTH> b)
{
	SIMD<uint64_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] > b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint32_t, WIDTH> vceq(SIMD<float, WIDTH> a, SIMD<float, WIDTH> b)
{
	SIMD<uint32_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] == b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint64_t, WIDTH> vceq(SIMD<double, WIDTH> a, SIMD<double, WIDTH> b)
{
	SIMD<uint64_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] == b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint8_t, WIDTH> vceq(SIMD<int8_t, WIDTH> a, SIMD<int8_t, WIDTH> b)
{
	SIMD<uint8_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] == b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint16_t, WIDTH> vceq(SIMD<int16_t, WIDTH> a, SIMD<int16_t, WIDTH> b)
{
	SIMD<uint16_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] == b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint32_t, WIDTH> vceq(SIMD<int32_t, WIDTH> a, SIMD<int32_t, WIDTH> b)
{
	SIMD<uint32_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] == b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint64_t, WIDTH> vceq(SIMD<int64_t, WIDTH> a, SIMD<int64_t, WIDTH> b)
{
	SIMD<uint64_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = -(a.v[i] == b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<float, WIDTH> vmin(SIMD<float, WIDTH> a, SIMD<float, WIDTH> b)
{
	SIMD<float, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = std::min(a.v[i], b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<double, WIDTH> vmin(SIMD<double, WIDTH> a, SIMD<double, WIDTH> b)
{
	SIMD<double, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = std::min(a.v[i], b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<int8_t, WIDTH> vmin(SIMD<int8_t, WIDTH> a, SIMD<int8_t, WIDTH> b)
{
	SIMD<int8_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = std::min(a.v[i], b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<int16_t, WIDTH> vmin(SIMD<int16_t, WIDTH> a, SIMD<int16_t, WIDTH> b)
{
	SIMD<int16_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = std::min(a.v[i], b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<int32_t, WIDTH> vmin(SIMD<int32_t, WIDTH> a, SIMD<int32_t, WIDTH> b)
{
	SIMD<int32_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = std::min(a.v[i], b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<int64_t, WIDTH> vmin(SIMD<int64_t, WIDTH> a, SIMD<int64_t, WIDTH> b)
{
	SIMD<int64_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = std::min(a.v[i], b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<float, WIDTH> vmax(SIMD<float, WIDTH> a, SIMD<float, WIDTH> b)
{
	SIMD<float, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = std::max(a.v[i], b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<double, WIDTH> vmax(SIMD<double, WIDTH> a, SIMD<double, WIDTH> b)
{
	SIMD<double, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = std::max(a.v[i], b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<int8_t, WIDTH> vmax(SIMD<int8_t, WIDTH> a, SIMD<int8_t, WIDTH> b)
{
	SIMD<int8_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = std::max(a.v[i], b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<int16_t, WIDTH> vmax(SIMD<int16_t, WIDTH> a, SIMD<int16_t, WIDTH> b)
{
	SIMD<int16_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = std::max(a.v[i], b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<int32_t, WIDTH> vmax(SIMD<int32_t, WIDTH> a, SIMD<int32_t, WIDTH> b)
{
	SIMD<int32_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = std::max(a.v[i], b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<int64_t, WIDTH> vmax(SIMD<int64_t, WIDTH> a, SIMD<int64_t, WIDTH> b)
{
	SIMD<int64_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = std::max(a.v[i], b.v[i]);
	return tmp;
}

template <int WIDTH>
static inline SIMD<float, WIDTH> vadd(SIMD<float, WIDTH> a, SIMD<float, WIDTH> b)
{
	SIMD<float, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] + b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<double, WIDTH> vadd(SIMD<double, WIDTH> a, SIMD<double, WIDTH> b)
{
	SIMD<double, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] + b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<int8_t, WIDTH> vadd(SIMD<int8_t, WIDTH> a, SIMD<int8_t, WIDTH> b)
{
	SIMD<int8_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] + b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<int16_t, WIDTH> vadd(SIMD<int16_t, WIDTH> a, SIMD<int16_t, WIDTH> b)
{
	SIMD<int16_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] + b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<int32_t, WIDTH> vadd(SIMD<int32_t, WIDTH> a, SIMD<int32_t, WIDTH> b)
{
	SIMD<int32_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] + b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<int64_t, WIDTH> vadd(SIMD<int64_t, WIDTH> a, SIMD<int64_t, WIDTH> b)
{
	SIMD<int64_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] + b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<int8_t, WIDTH> vqadd(SIMD<int8_t, WIDTH> a, SIMD<int8_t, WIDTH> b)
{
	SIMD<int8_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = std::min<int16_t>(std::max<int16_t>(int16_t(a.v[i]) + int16_t(b.v[i]), INT8_MIN), INT8_MAX);
	return tmp;
}

template <int WIDTH>
static inline SIMD<int16_t, WIDTH> vqadd(SIMD<int16_t, WIDTH> a, SIMD<int16_t, WIDTH> b)
{
	SIMD<int16_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = std::min<int32_t>(std::max<int32_t>(int32_t(a.v[i]) + int32_t(b.v[i]), INT16_MIN), INT16_MAX);
	return tmp;
}

template <int WIDTH>
static inline SIMD<float, WIDTH> vsub(SIMD<float, WIDTH> a, SIMD<float, WIDTH> b)
{
	SIMD<float, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] - b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<double, WIDTH> vsub(SIMD<double, WIDTH> a, SIMD<double, WIDTH> b)
{
	SIMD<double, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] - b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<int8_t, WIDTH> vsub(SIMD<int8_t, WIDTH> a, SIMD<int8_t, WIDTH> b)
{
	SIMD<int8_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] - b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<int16_t, WIDTH> vsub(SIMD<int16_t, WIDTH> a, SIMD<int16_t, WIDTH> b)
{
	SIMD<int16_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] - b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<int32_t, WIDTH> vsub(SIMD<int32_t, WIDTH> a, SIMD<int32_t, WIDTH> b)
{
	SIMD<int32_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] - b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<int64_t, WIDTH> vsub(SIMD<int64_t, WIDTH> a, SIMD<int64_t, WIDTH> b)
{
	SIMD<int64_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = a.v[i] - b.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<int8_t, WIDTH> vqsub(SIMD<int8_t, WIDTH> a, SIMD<int8_t, WIDTH> b)
{
	SIMD<int8_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = std::min<int16_t>(std::max<int16_t>(int16_t(a.v[i]) - int16_t(b.v[i]), INT8_MIN), INT8_MAX);
	return tmp;
}

template <int WIDTH>
static inline SIMD<int16_t, WIDTH> vqsub(SIMD<int16_t, WIDTH> a, SIMD<int16_t, WIDTH> b)
{
	SIMD<int16_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = std::min<int32_t>(std::max<int32_t>(int32_t(a.v[i]) - int32_t(b.v[i]), INT16_MIN), INT16_MAX);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint8_t, WIDTH> vqsub(SIMD<uint8_t, WIDTH> a, SIMD<uint8_t, WIDTH> b)
{
	SIMD<uint8_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = std::max<int16_t>(int16_t(a.v[i]) - int16_t(b.v[i]), 0);
	return tmp;
}

template <int WIDTH>
static inline SIMD<uint16_t, WIDTH> vqsub(SIMD<uint16_t, WIDTH> a, SIMD<uint16_t, WIDTH> b)
{
	SIMD<uint16_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = std::max<int32_t>(int32_t(a.v[i]) - int32_t(b.v[i]), 0);
	return tmp;
}

template <int WIDTH>
static inline SIMD<float, WIDTH> vsign(SIMD<float, WIDTH> a, SIMD<float, WIDTH> b)
{
	SIMD<float, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = ((b.v[i] > 0.f) - (b.v[i] < 0.f)) * a.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<double, WIDTH> vsign(SIMD<double, WIDTH> a, SIMD<double, WIDTH> b)
{
	SIMD<double, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = ((b.v[i] > 0.) - (b.v[i] < 0.)) * a.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<int8_t, WIDTH> vsign(SIMD<int8_t, WIDTH> a, SIMD<int8_t, WIDTH> b)
{
	SIMD<int8_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = ((b.v[i] > 0) - (b.v[i] < 0)) * a.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<int16_t, WIDTH> vsign(SIMD<int16_t, WIDTH> a, SIMD<int16_t, WIDTH> b)
{
	SIMD<int16_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = ((b.v[i] > 0) - (b.v[i] < 0)) * a.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<int32_t, WIDTH> vsign(SIMD<int32_t, WIDTH> a, SIMD<int32_t, WIDTH> b)
{
	SIMD<int32_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = ((b.v[i] > 0) - (b.v[i] < 0)) * a.v[i];
	return tmp;
}

template <int WIDTH>
static inline SIMD<int64_t, WIDTH> vsign(SIMD<int64_t, WIDTH> a, SIMD<int64_t, WIDTH> b)
{
	SIMD<int64_t, WIDTH> tmp;
	for (int i = 0; i < WIDTH; ++i)
		tmp.v[i] = ((b.v[i] > 0) - (b.v[i] < 0)) * a.v[i];
	return tmp;
}

#if 0
#ifdef __AVX2__
#include "avx2.h"
#else
#ifdef __SSE4_1__
#include "sse4_1.h"
#endif
#endif

#ifdef __ARM_NEON__
#include "neon.h"
#endif
#endif

} // namespace ldpctool

#endif
