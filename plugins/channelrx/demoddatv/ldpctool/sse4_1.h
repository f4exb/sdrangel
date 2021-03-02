/*
Intel SSE4.1 acceleration

Copyright 2018 Ahmet Inan <inan@aicodix.de>
*/

#ifndef SSE4_1_HH
#define SSE4_1_HH

#include <cstdint>
#include <smmintrin.h>
#include "simd.h"

namespace ldpctool {

template <>
union SIMD<float, 4>
{
	static const int SIZE = 4;
	typedef float value_type;
	typedef uint32_t uint_type;
	__m128 m;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <>
union SIMD<double, 2>
{
	static const int SIZE = 2;
	typedef double value_type;
	typedef uint64_t uint_type;
	__m128d m;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <>
union SIMD<int8_t, 16>
{
	static const int SIZE = 16;
	typedef int8_t value_type;
	typedef uint8_t uint_type;
	__m128i m;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <>
union SIMD<int16_t, 8>
{
	static const int SIZE = 8;
	typedef int16_t value_type;
	typedef uint16_t uint_type;
	__m128i m;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <>
union SIMD<int32_t, 4>
{
	static const int SIZE = 4;
	typedef int32_t value_type;
	typedef uint32_t uint_type;
	__m128i m;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <>
union SIMD<int64_t, 2>
{
	static const int SIZE = 2;
	typedef int64_t value_type;
	typedef uint64_t uint_type;
	__m128i m;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <>
union SIMD<uint8_t, 16>
{
	static const int SIZE = 16;
	typedef uint8_t value_type;
	typedef uint8_t uint_type;
	__m128i m;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <>
union SIMD<uint16_t, 8>
{
	static const int SIZE = 8;
	typedef uint16_t value_type;
	typedef uint16_t uint_type;
	__m128i m;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <>
union SIMD<uint32_t, 4>
{
	static const int SIZE = 4;
	typedef uint32_t value_type;
	typedef uint32_t uint_type;
	__m128i m;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <>
union SIMD<uint64_t, 2>
{
	static const int SIZE = 2;
	typedef uint64_t value_type;
	typedef uint64_t uint_type;
	__m128i m;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <>
inline SIMD<float, 4> vreinterpret(SIMD<uint32_t, 4> a)
{
	SIMD<float, 4> tmp;
	tmp.m = (__m128)a.m;
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vreinterpret(SIMD<float, 4> a)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = (__m128i)a.m;
	return tmp;
}

template <>
inline SIMD<double, 2> vreinterpret(SIMD<uint64_t, 2> a)
{
	SIMD<double, 2> tmp;
	tmp.m = (__m128d)a.m;
	return tmp;
}

template <>
inline SIMD<uint64_t, 2> vreinterpret(SIMD<double, 2> a)
{
	SIMD<uint64_t, 2> tmp;
	tmp.m = (__m128i)a.m;
	return tmp;
}

template <>
inline SIMD<uint8_t, 16> vreinterpret(SIMD<int8_t, 16> a)
{
	SIMD<uint8_t, 16> tmp;
	tmp.m = (__m128i)a.m;
	return tmp;
}

template <>
inline SIMD<int8_t, 16> vreinterpret(SIMD<uint8_t, 16> a)
{
	SIMD<int8_t, 16> tmp;
	tmp.m = (__m128i)a.m;
	return tmp;
}

template <>
inline SIMD<uint16_t, 8> vreinterpret(SIMD<int16_t, 8> a)
{
	SIMD<uint16_t, 8> tmp;
	tmp.m = (__m128i)a.m;
	return tmp;
}

template <>
inline SIMD<int16_t, 8> vreinterpret(SIMD<uint16_t, 8> a)
{
	SIMD<int16_t, 8> tmp;
	tmp.m = (__m128i)a.m;
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vreinterpret(SIMD<int32_t, 4> a)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = (__m128i)a.m;
	return tmp;
}

template <>
inline SIMD<int32_t, 4> vreinterpret(SIMD<uint32_t, 4> a)
{
	SIMD<int32_t, 4> tmp;
	tmp.m = (__m128i)a.m;
	return tmp;
}

template <>
inline SIMD<uint64_t, 2> vreinterpret(SIMD<int64_t, 2> a)
{
	SIMD<uint64_t, 2> tmp;
	tmp.m = (__m128i)a.m;
	return tmp;
}

template <>
inline SIMD<int64_t, 2> vreinterpret(SIMD<uint64_t, 2> a)
{
	SIMD<int64_t, 2> tmp;
	tmp.m = (__m128i)a.m;
	return tmp;
}

template <>
inline SIMD<float, 4> vdup<SIMD<float, 4>>(float a)
{
	SIMD<float, 4> tmp;
	tmp.m = _mm_set1_ps(a);
	return tmp;
}

template <>
inline SIMD<double, 2> vdup<SIMD<double, 2>>(double a)
{
	SIMD<double, 2> tmp;
	tmp.m = _mm_set1_pd(a);
	return tmp;
}

template <>
inline SIMD<int8_t, 16> vdup<SIMD<int8_t, 16>>(int8_t a)
{
	SIMD<int8_t, 16> tmp;
	tmp.m = _mm_set1_epi8(a);
	return tmp;
}

template <>
inline SIMD<int16_t, 8> vdup<SIMD<int16_t, 8>>(int16_t a)
{
	SIMD<int16_t, 8> tmp;
	tmp.m = _mm_set1_epi16(a);
	return tmp;
}

template <>
inline SIMD<int32_t, 4> vdup<SIMD<int32_t, 4>>(int32_t a)
{
	SIMD<int32_t, 4> tmp;
	tmp.m = _mm_set1_epi32(a);
	return tmp;
}

template <>
inline SIMD<int64_t, 2> vdup<SIMD<int64_t, 2>>(int64_t a)
{
	SIMD<int64_t, 2> tmp;
	tmp.m = _mm_set1_epi64x(a);
	return tmp;
}

template <>
inline SIMD<float, 4> vzero()
{
	SIMD<float, 4> tmp;
	tmp.m = _mm_setzero_ps();
	return tmp;
}

template <>
inline SIMD<double, 2> vzero()
{
	SIMD<double, 2> tmp;
	tmp.m = _mm_setzero_pd();
	return tmp;
}

template <>
inline SIMD<int8_t, 16> vzero()
{
	SIMD<int8_t, 16> tmp;
	tmp.m = _mm_setzero_si128();
	return tmp;
}

template <>
inline SIMD<int16_t, 8> vzero()
{
	SIMD<int16_t, 8> tmp;
	tmp.m = _mm_setzero_si128();
	return tmp;
}

template <>
inline SIMD<int32_t, 4> vzero()
{
	SIMD<int32_t, 4> tmp;
	tmp.m = _mm_setzero_si128();
	return tmp;
}

template <>
inline SIMD<int64_t, 2> vzero()
{
	SIMD<int64_t, 2> tmp;
	tmp.m = _mm_setzero_si128();
	return tmp;
}

template <>
inline SIMD<float, 4> vadd(SIMD<float, 4> a, SIMD<float, 4> b)
{
	SIMD<float, 4> tmp;
	tmp.m = _mm_add_ps(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<double, 2> vadd(SIMD<double, 2> a, SIMD<double, 2> b)
{
	SIMD<double, 2> tmp;
	tmp.m = _mm_add_pd(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int8_t, 16> vadd(SIMD<int8_t, 16> a, SIMD<int8_t, 16> b)
{
	SIMD<int8_t, 16> tmp;
	tmp.m = _mm_add_epi8(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int16_t, 8> vadd(SIMD<int16_t, 8> a, SIMD<int16_t, 8> b)
{
	SIMD<int16_t, 8> tmp;
	tmp.m = _mm_add_epi16(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int32_t, 4> vadd(SIMD<int32_t, 4> a, SIMD<int32_t, 4> b)
{
	SIMD<int32_t, 4> tmp;
	tmp.m = _mm_add_epi32(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int64_t, 2> vadd(SIMD<int64_t, 2> a, SIMD<int64_t, 2> b)
{
	SIMD<int64_t, 2> tmp;
	tmp.m = _mm_add_epi64(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int8_t, 16> vqadd(SIMD<int8_t, 16> a, SIMD<int8_t, 16> b)
{
	SIMD<int8_t, 16> tmp;
	tmp.m = _mm_adds_epi8(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int16_t, 8> vqadd(SIMD<int16_t, 8> a, SIMD<int16_t, 8> b)
{
	SIMD<int16_t, 8> tmp;
	tmp.m = _mm_adds_epi16(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<float, 4> vsub(SIMD<float, 4> a, SIMD<float, 4> b)
{
	SIMD<float, 4> tmp;
	tmp.m = _mm_sub_ps(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<double, 2> vsub(SIMD<double, 2> a, SIMD<double, 2> b)
{
	SIMD<double, 2> tmp;
	tmp.m = _mm_sub_pd(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int8_t, 16> vsub(SIMD<int8_t, 16> a, SIMD<int8_t, 16> b)
{
	SIMD<int8_t, 16> tmp;
	tmp.m = _mm_sub_epi8(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int16_t, 8> vsub(SIMD<int16_t, 8> a, SIMD<int16_t, 8> b)
{
	SIMD<int16_t, 8> tmp;
	tmp.m = _mm_sub_epi16(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int32_t, 4> vsub(SIMD<int32_t, 4> a, SIMD<int32_t, 4> b)
{
	SIMD<int32_t, 4> tmp;
	tmp.m = _mm_sub_epi32(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int64_t, 2> vsub(SIMD<int64_t, 2> a, SIMD<int64_t, 2> b)
{
	SIMD<int64_t, 2> tmp;
	tmp.m = _mm_sub_epi64(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int8_t, 16> vqsub(SIMD<int8_t, 16> a, SIMD<int8_t, 16> b)
{
	SIMD<int8_t, 16> tmp;
	tmp.m = _mm_subs_epi8(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int16_t, 8> vqsub(SIMD<int16_t, 8> a, SIMD<int16_t, 8> b)
{
	SIMD<int16_t, 8> tmp;
	tmp.m = _mm_subs_epi16(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint8_t, 16> vqsub(SIMD<uint8_t, 16> a, SIMD<uint8_t, 16> b)
{
	SIMD<uint8_t, 16> tmp;
	tmp.m = _mm_subs_epu8(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint16_t, 8> vqsub(SIMD<uint16_t, 8> a, SIMD<uint16_t, 8> b)
{
	SIMD<uint16_t, 8> tmp;
	tmp.m = _mm_subs_epu16(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<float, 4> vabs(SIMD<float, 4> a)
{
	SIMD<float, 4> tmp;
	tmp.m = _mm_andnot_ps(_mm_set1_ps(-0.f), a.m);
	return tmp;
}

template <>
inline SIMD<double, 2> vabs(SIMD<double, 2> a)
{
	SIMD<double, 2> tmp;
	tmp.m = _mm_andnot_pd(_mm_set1_pd(-0.), a.m);
	return tmp;
}

template <>
inline SIMD<int8_t, 16> vqabs(SIMD<int8_t, 16> a)
{
	SIMD<int8_t, 16> tmp;
	tmp.m = _mm_abs_epi8(_mm_max_epi8(a.m, _mm_set1_epi8(-INT8_MAX)));
	return tmp;
}

template <>
inline SIMD<int16_t, 8> vqabs(SIMD<int16_t, 8> a)
{
	SIMD<int16_t, 8> tmp;
	tmp.m = _mm_abs_epi16(_mm_max_epi16(a.m, _mm_set1_epi16(-INT16_MAX)));
	return tmp;
}

template <>
inline SIMD<int32_t, 4> vqabs(SIMD<int32_t, 4> a)
{
	SIMD<int32_t, 4> tmp;
	tmp.m = _mm_abs_epi32(_mm_max_epi32(a.m, _mm_set1_epi32(-INT32_MAX)));
	return tmp;
}

template <>
inline SIMD<float, 4> vsign(SIMD<float, 4> a, SIMD<float, 4> b)
{
	SIMD<float, 4> tmp;
	tmp.m = _mm_andnot_ps(
		_mm_cmpeq_ps(b.m, _mm_setzero_ps()),
		_mm_xor_ps(a.m, _mm_and_ps(_mm_set1_ps(-0.f), b.m)));
	return tmp;
}

template <>
inline SIMD<double, 2> vsign(SIMD<double, 2> a, SIMD<double, 2> b)
{
	SIMD<double, 2> tmp;
	tmp.m = _mm_andnot_pd(
		_mm_cmpeq_pd(b.m, _mm_setzero_pd()),
		_mm_xor_pd(a.m, _mm_and_pd(_mm_set1_pd(-0.), b.m)));
	return tmp;
}

template <>
inline SIMD<int8_t, 16> vsign(SIMD<int8_t, 16> a, SIMD<int8_t, 16> b)
{
	SIMD<int8_t, 16> tmp;
	tmp.m = _mm_sign_epi8(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int16_t, 8> vsign(SIMD<int16_t, 8> a, SIMD<int16_t, 8> b)
{
	SIMD<int16_t, 8> tmp;
	tmp.m = _mm_sign_epi16(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int32_t, 4> vsign(SIMD<int32_t, 4> a, SIMD<int32_t, 4> b)
{
	SIMD<int32_t, 4> tmp;
	tmp.m = _mm_sign_epi32(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint8_t, 16> vorr(SIMD<uint8_t, 16> a, SIMD<uint8_t, 16> b)
{
	SIMD<uint8_t, 16> tmp;
	tmp.m = _mm_or_si128(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint16_t, 8> vorr(SIMD<uint16_t, 8> a, SIMD<uint16_t, 8> b)
{
	SIMD<uint16_t, 8> tmp;
	tmp.m = _mm_or_si128(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vorr(SIMD<uint32_t, 4> a, SIMD<uint32_t, 4> b)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = _mm_or_si128(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint64_t, 2> vorr(SIMD<uint64_t, 2> a, SIMD<uint64_t, 2> b)
{
	SIMD<uint64_t, 2> tmp;
	tmp.m = _mm_or_si128(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint8_t, 16> vand(SIMD<uint8_t, 16> a, SIMD<uint8_t, 16> b)
{
	SIMD<uint8_t, 16> tmp;
	tmp.m = _mm_and_si128(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint16_t, 8> vand(SIMD<uint16_t, 8> a, SIMD<uint16_t, 8> b)
{
	SIMD<uint16_t, 8> tmp;
	tmp.m = _mm_and_si128(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vand(SIMD<uint32_t, 4> a, SIMD<uint32_t, 4> b)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = _mm_and_si128(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint64_t, 2> vand(SIMD<uint64_t, 2> a, SIMD<uint64_t, 2> b)
{
	SIMD<uint64_t, 2> tmp;
	tmp.m = _mm_and_si128(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint8_t, 16> veor(SIMD<uint8_t, 16> a, SIMD<uint8_t, 16> b)
{
	SIMD<uint8_t, 16> tmp;
	tmp.m = _mm_xor_si128(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint16_t, 8> veor(SIMD<uint16_t, 8> a, SIMD<uint16_t, 8> b)
{
	SIMD<uint16_t, 8> tmp;
	tmp.m = _mm_xor_si128(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> veor(SIMD<uint32_t, 4> a, SIMD<uint32_t, 4> b)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = _mm_xor_si128(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint64_t, 2> veor(SIMD<uint64_t, 2> a, SIMD<uint64_t, 2> b)
{
	SIMD<uint64_t, 2> tmp;
	tmp.m = _mm_xor_si128(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint8_t, 16> vbic(SIMD<uint8_t, 16> a, SIMD<uint8_t, 16> b)
{
	SIMD<uint8_t, 16> tmp;
	tmp.m = _mm_andnot_si128(b.m, a.m);
	return tmp;
}

template <>
inline SIMD<uint16_t, 8> vbic(SIMD<uint16_t, 8> a, SIMD<uint16_t, 8> b)
{
	SIMD<uint16_t, 8> tmp;
	tmp.m = _mm_andnot_si128(b.m, a.m);
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vbic(SIMD<uint32_t, 4> a, SIMD<uint32_t, 4> b)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = _mm_andnot_si128(b.m, a.m);
	return tmp;
}

template <>
inline SIMD<uint64_t, 2> vbic(SIMD<uint64_t, 2> a, SIMD<uint64_t, 2> b)
{
	SIMD<uint64_t, 2> tmp;
	tmp.m = _mm_andnot_si128(b.m, a.m);
	return tmp;
}

template <>
inline SIMD<uint8_t, 16> vbsl(SIMD<uint8_t, 16> a, SIMD<uint8_t, 16> b, SIMD<uint8_t, 16> c)
{
	SIMD<uint8_t, 16> tmp;
	tmp.m = _mm_or_si128(_mm_and_si128(a.m, b.m), _mm_andnot_si128(a.m, c.m));
	return tmp;
}

template <>
inline SIMD<uint16_t, 8> vbsl(SIMD<uint16_t, 8> a, SIMD<uint16_t, 8> b, SIMD<uint16_t, 8> c)
{
	SIMD<uint16_t, 8> tmp;
	tmp.m = _mm_or_si128(_mm_and_si128(a.m, b.m), _mm_andnot_si128(a.m, c.m));
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vbsl(SIMD<uint32_t, 4> a, SIMD<uint32_t, 4> b, SIMD<uint32_t, 4> c)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = _mm_or_si128(_mm_and_si128(a.m, b.m), _mm_andnot_si128(a.m, c.m));
	return tmp;
}

template <>
inline SIMD<uint64_t, 2> vbsl(SIMD<uint64_t, 2> a, SIMD<uint64_t, 2> b, SIMD<uint64_t, 2> c)
{
	SIMD<uint64_t, 2> tmp;
	tmp.m = _mm_or_si128(_mm_and_si128(a.m, b.m), _mm_andnot_si128(a.m, c.m));
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vceqz(SIMD<float, 4> a)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = (__m128i)_mm_cmpeq_ps(a.m, _mm_setzero_ps());
	return tmp;
}

template <>
inline SIMD<uint64_t, 2> vceqz(SIMD<double, 2> a)
{
	SIMD<uint64_t, 2> tmp;
	tmp.m = (__m128i)_mm_cmpeq_pd(a.m, _mm_setzero_pd());
	return tmp;
}

template <>
inline SIMD<uint8_t, 16> vceqz(SIMD<int8_t, 16> a)
{
	SIMD<uint8_t, 16> tmp;
	tmp.m = _mm_cmpeq_epi8(a.m, _mm_setzero_si128());
	return tmp;
}

template <>
inline SIMD<uint16_t, 8> vceqz(SIMD<int16_t, 8> a)
{
	SIMD<uint16_t, 8> tmp;
	tmp.m = _mm_cmpeq_epi16(a.m, _mm_setzero_si128());
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vceqz(SIMD<int32_t, 4> a)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = _mm_cmpeq_epi32(a.m, _mm_setzero_si128());
	return tmp;
}

template <>
inline SIMD<uint64_t, 2> vceqz(SIMD<int64_t, 2> a)
{
	SIMD<uint64_t, 2> tmp;
	tmp.m = _mm_cmpeq_epi64(a.m, _mm_setzero_si128());
	return tmp;
}


template <>
inline SIMD<uint32_t, 4> vceq(SIMD<float, 4> a, SIMD<float, 4> b)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = (__m128i)_mm_cmpeq_ps(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint64_t, 2> vceq(SIMD<double, 2> a, SIMD<double, 2> b)
{
	SIMD<uint64_t, 2> tmp;
	tmp.m = (__m128i)_mm_cmpeq_pd(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint8_t, 16> vceq(SIMD<int8_t, 16> a, SIMD<int8_t, 16> b)
{
	SIMD<uint8_t, 16> tmp;
	tmp.m = _mm_cmpeq_epi8(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint16_t, 8> vceq(SIMD<int16_t, 8> a, SIMD<int16_t, 8> b)
{
	SIMD<uint16_t, 8> tmp;
	tmp.m = _mm_cmpeq_epi16(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vceq(SIMD<int32_t, 4> a, SIMD<int32_t, 4> b)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = _mm_cmpeq_epi32(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint64_t, 2> vceq(SIMD<int64_t, 2> a, SIMD<int64_t, 2> b)
{
	SIMD<uint64_t, 2> tmp;
	tmp.m = _mm_cmpeq_epi64(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vcgtz(SIMD<float, 4> a)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = (__m128i)_mm_cmpgt_ps(a.m, _mm_setzero_ps());
	return tmp;
}

template <>
inline SIMD<uint64_t, 2> vcgtz(SIMD<double, 2> a)
{
	SIMD<uint64_t, 2> tmp;
	tmp.m = (__m128i)_mm_cmpgt_pd(a.m, _mm_setzero_pd());
	return tmp;
}

template <>
inline SIMD<uint8_t, 16> vcgtz(SIMD<int8_t, 16> a)
{
	SIMD<uint8_t, 16> tmp;
	tmp.m = _mm_cmpgt_epi8(a.m, _mm_setzero_si128());
	return tmp;
}

template <>
inline SIMD<uint16_t, 8> vcgtz(SIMD<int16_t, 8> a)
{
	SIMD<uint16_t, 8> tmp;
	tmp.m = _mm_cmpgt_epi16(a.m, _mm_setzero_si128());
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vcgtz(SIMD<int32_t, 4> a)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = _mm_cmpgt_epi32(a.m, _mm_setzero_si128());
	return tmp;
}

template <>
inline SIMD<uint64_t, 2> vcgtz(SIMD<int64_t, 2> a)
{
	SIMD<uint64_t, 2> tmp;
	tmp.m = _mm_cmpgt_epi64(a.m, _mm_setzero_si128());
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vcltz(SIMD<float, 4> a)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = (__m128i)_mm_cmplt_ps(a.m, _mm_setzero_ps());
	return tmp;
}

template <>
inline SIMD<uint64_t, 2> vcltz(SIMD<double, 2> a)
{
	SIMD<uint64_t, 2> tmp;
	tmp.m = (__m128i)_mm_cmplt_pd(a.m, _mm_setzero_pd());
	return tmp;
}

template <>
inline SIMD<uint8_t, 16> vcltz(SIMD<int8_t, 16> a)
{
	SIMD<uint8_t, 16> tmp;
	tmp.m = _mm_cmpgt_epi8(_mm_setzero_si128(), a.m);
	return tmp;
}

template <>
inline SIMD<uint16_t, 8> vcltz(SIMD<int16_t, 8> a)
{
	SIMD<uint16_t, 8> tmp;
	tmp.m = _mm_cmpgt_epi16(_mm_setzero_si128(), a.m);
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vcltz(SIMD<int32_t, 4> a)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = _mm_cmpgt_epi32(_mm_setzero_si128(), a.m);
	return tmp;
}

template <>
inline SIMD<uint64_t, 2> vcltz(SIMD<int64_t, 2> a)
{
	SIMD<uint64_t, 2> tmp;
	tmp.m = _mm_cmpgt_epi64(_mm_setzero_si128(), a.m);
	return tmp;
}

template <>
inline SIMD<float, 4> vmin(SIMD<float, 4> a, SIMD<float, 4> b)
{
	SIMD<float, 4> tmp;
	tmp.m = _mm_min_ps(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<double, 2> vmin(SIMD<double, 2> a, SIMD<double, 2> b)
{
	SIMD<double, 2> tmp;
	tmp.m = _mm_min_pd(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int8_t, 16> vmin(SIMD<int8_t, 16> a, SIMD<int8_t, 16> b)
{
	SIMD<int8_t, 16> tmp;
	tmp.m = _mm_min_epi8(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int16_t, 8> vmin(SIMD<int16_t, 8> a, SIMD<int16_t, 8> b)
{
	SIMD<int16_t, 8> tmp;
	tmp.m = _mm_min_epi16(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int32_t, 4> vmin(SIMD<int32_t, 4> a, SIMD<int32_t, 4> b)
{
	SIMD<int32_t, 4> tmp;
	tmp.m = _mm_min_epi32(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<float, 4> vmax(SIMD<float, 4> a, SIMD<float, 4> b)
{
	SIMD<float, 4> tmp;
	tmp.m = _mm_max_ps(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<double, 2> vmax(SIMD<double, 2> a, SIMD<double, 2> b)
{
	SIMD<double, 2> tmp;
	tmp.m = _mm_max_pd(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int8_t, 16> vmax(SIMD<int8_t, 16> a, SIMD<int8_t, 16> b)
{
	SIMD<int8_t, 16> tmp;
	tmp.m = _mm_max_epi8(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int16_t, 8> vmax(SIMD<int16_t, 8> a, SIMD<int16_t, 8> b)
{
	SIMD<int16_t, 8> tmp;
	tmp.m = _mm_max_epi16(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int32_t, 4> vmax(SIMD<int32_t, 4> a, SIMD<int32_t, 4> b)
{
	SIMD<int32_t, 4> tmp;
	tmp.m = _mm_max_epi32(a.m, b.m);
	return tmp;
}

} // namespace ldpctool

#endif
