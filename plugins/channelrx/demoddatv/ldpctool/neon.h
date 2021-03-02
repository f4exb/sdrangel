/*
ARM NEON acceleration

Copyright 2018 Ahmet Inan <inan@aicodix.de>
*/

#ifndef NEON_HH
#define NEON_HH

#include <cstdint>
#include <arm_neon.h>
#include "simd.h"

namespace ldpctool {

template <>
union SIMD<float, 4>
{
	static const int SIZE = 4;
	typedef float value_type;
	typedef uint32_t uint_type;
	float32x4_t m;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <>
union SIMD<int8_t, 16>
{
	static const int SIZE = 16;
	typedef int8_t value_type;
	typedef uint8_t uint_type;
	int8x16_t m;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <>
union SIMD<int16_t, 8>
{
	static const int SIZE = 8;
	typedef int16_t value_type;
	typedef uint16_t uint_type;
	int16x8_t m;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <>
union SIMD<int32_t, 4>
{
	static const int SIZE = 4;
	typedef int32_t value_type;
	typedef uint32_t uint_type;
	int32x4_t m;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <>
union SIMD<int64_t, 2>
{
	static const int SIZE = 2;
	typedef int64_t value_type;
	typedef uint64_t uint_type;
	int64x2_t m;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <>
union SIMD<uint8_t, 16>
{
	static const int SIZE = 16;
	typedef uint8_t value_type;
	typedef uint8_t uint_type;
	uint8x16_t m;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <>
union SIMD<uint16_t, 8>
{
	static const int SIZE = 8;
	typedef uint16_t value_type;
	typedef uint16_t uint_type;
	uint16x8_t m;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <>
union SIMD<uint32_t, 4>
{
	static const int SIZE = 4;
	typedef uint32_t value_type;
	typedef uint32_t uint_type;
	uint32x4_t m;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <>
union SIMD<uint64_t, 2>
{
	static const int SIZE = 2;
	typedef uint64_t value_type;
	typedef uint64_t uint_type;
	uint64x2_t m;
	value_type v[SIZE];
	uint_type u[SIZE];
};

template <>
inline SIMD<float, 4> vreinterpret(SIMD<uint32_t, 4> a)
{
	SIMD<float, 4> tmp;
	tmp.m = (float32x4_t)a.m;
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vreinterpret(SIMD<float, 4> a)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = (uint32x4_t)a.m;
	return tmp;
}

template <>
inline SIMD<int8_t, 16> vreinterpret(SIMD<uint8_t, 16> a)
{
	SIMD<int8_t, 16> tmp;
	tmp.m = (int8x16_t)a.m;
	return tmp;
}

template <>
inline SIMD<uint8_t, 16> vreinterpret(SIMD<int8_t, 16> a)
{
	SIMD<uint8_t, 16> tmp;
	tmp.m = (uint8x16_t)a.m;
	return tmp;
}

template <>
inline SIMD<int16_t, 8> vreinterpret(SIMD<uint16_t, 8> a)
{
	SIMD<int16_t, 8> tmp;
	tmp.m = (int16x8_t)a.m;
	return tmp;
}

template <>
inline SIMD<uint16_t, 8> vreinterpret(SIMD<int16_t, 8> a)
{
	SIMD<uint16_t, 8> tmp;
	tmp.m = (uint16x8_t)a.m;
	return tmp;
}

template <>
inline SIMD<int32_t, 4> vreinterpret(SIMD<uint32_t, 4> a)
{
	SIMD<int32_t, 4> tmp;
	tmp.m = (int32x4_t)a.m;
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vreinterpret(SIMD<int32_t, 4> a)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = (uint32x4_t)a.m;
	return tmp;
}

template <>
inline SIMD<float, 4> vdup(float a)
{
	SIMD<float, 4> tmp;
	tmp.m = vdupq_n_f32(a);
	return tmp;
}

template <>
inline SIMD<int8_t, 16> vdup(int8_t a)
{
	SIMD<int8_t, 16> tmp;
	tmp.m = vdupq_n_s8(a);
	return tmp;
}

template <>
inline SIMD<int16_t, 8> vdup(int16_t a)
{
	SIMD<int16_t, 8> tmp;
	tmp.m = vdupq_n_s16(a);
	return tmp;
}

template <>
inline SIMD<int32_t, 4> vdup(int32_t a)
{
	SIMD<int32_t, 4> tmp;
	tmp.m = vdupq_n_s32(a);
	return tmp;
}

template <>
inline SIMD<int64_t, 2> vdup(int64_t a)
{
	SIMD<int64_t, 2> tmp;
	tmp.m = vdupq_n_s64(a);
	return tmp;
}

template <>
inline SIMD<uint8_t, 16> vdup(uint8_t a)
{
	SIMD<uint8_t, 16> tmp;
	tmp.m = vdupq_n_u8(a);
	return tmp;
}

template <>
inline SIMD<uint16_t, 8> vdup(uint16_t a)
{
	SIMD<uint16_t, 8> tmp;
	tmp.m = vdupq_n_u16(a);
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vdup(uint32_t a)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = vdupq_n_u32(a);
	return tmp;
}

template <>
inline SIMD<uint64_t, 2> vdup(uint64_t a)
{
	SIMD<uint64_t, 2> tmp;
	tmp.m = vdupq_n_u64(a);
	return tmp;
}

template <>
inline SIMD<float, 4> vzero()
{
	SIMD<float, 4> tmp;
	tmp.m = (float32x4_t)veorq_u32((uint32x4_t)tmp.m, (uint32x4_t)tmp.m);
	return tmp;
}

template <>
inline SIMD<int8_t, 16> vzero()
{
	SIMD<int8_t, 16> tmp;
	tmp.m = veorq_s8(tmp.m, tmp.m);
	return tmp;
}

template <>
inline SIMD<int16_t, 8> vzero()
{
	SIMD<int16_t, 8> tmp;
	tmp.m = veorq_s16(tmp.m, tmp.m);
	return tmp;
}

template <>
inline SIMD<int32_t, 4> vzero()
{
	SIMD<int32_t, 4> tmp;
	tmp.m = veorq_s32(tmp.m, tmp.m);
	return tmp;
}

template <>
inline SIMD<int64_t, 2> vzero()
{
	SIMD<int64_t, 2> tmp;
	tmp.m = veorq_s64(tmp.m, tmp.m);
	return tmp;
}

template <>
inline SIMD<uint8_t, 16> vzero()
{
	SIMD<uint8_t, 16> tmp;
	tmp.m = veorq_u8(tmp.m, tmp.m);
	return tmp;
}

template <>
inline SIMD<uint16_t, 8> vzero()
{
	SIMD<uint16_t, 8> tmp;
	tmp.m = veorq_u16(tmp.m, tmp.m);
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vzero()
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = veorq_u32(tmp.m, tmp.m);
	return tmp;
}

template <>
inline SIMD<uint64_t, 2> vzero()
{
	SIMD<uint64_t, 2> tmp;
	tmp.m = veorq_u64(tmp.m, tmp.m);
	return tmp;
}

template <>
inline SIMD<float, 4> vadd(SIMD<float, 4> a, SIMD<float, 4> b)
{
	SIMD<float, 4> tmp;
	tmp.m = vaddq_f32(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int8_t, 16> vadd(SIMD<int8_t, 16> a, SIMD<int8_t, 16> b)
{
	SIMD<int8_t, 16> tmp;
	tmp.m = vaddq_s8(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int16_t, 8> vadd(SIMD<int16_t, 8> a, SIMD<int16_t, 8> b)
{
	SIMD<int16_t, 8> tmp;
	tmp.m = vaddq_s16(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int32_t, 4> vadd(SIMD<int32_t, 4> a, SIMD<int32_t, 4> b)
{
	SIMD<int32_t, 4> tmp;
	tmp.m = vaddq_s32(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int64_t, 2> vadd(SIMD<int64_t, 2> a, SIMD<int64_t, 2> b)
{
	SIMD<int64_t, 2> tmp;
	tmp.m = vaddq_s64(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int8_t, 16> vqadd(SIMD<int8_t, 16> a, SIMD<int8_t, 16> b)
{
	SIMD<int8_t, 16> tmp;
	tmp.m = vqaddq_s8(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int16_t, 8> vqadd(SIMD<int16_t, 8> a, SIMD<int16_t, 8> b)
{
	SIMD<int16_t, 8> tmp;
	tmp.m = vqaddq_s16(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<float, 4> vsub(SIMD<float, 4> a, SIMD<float, 4> b)
{
	SIMD<float, 4> tmp;
	tmp.m = vsubq_f32(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int8_t, 16> vsub(SIMD<int8_t, 16> a, SIMD<int8_t, 16> b)
{
	SIMD<int8_t, 16> tmp;
	tmp.m = vsubq_s8(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int16_t, 8> vsub(SIMD<int16_t, 8> a, SIMD<int16_t, 8> b)
{
	SIMD<int16_t, 8> tmp;
	tmp.m = vsubq_s16(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int32_t, 4> vsub(SIMD<int32_t, 4> a, SIMD<int32_t, 4> b)
{
	SIMD<int32_t, 4> tmp;
	tmp.m = vsubq_s32(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int64_t, 2> vsub(SIMD<int64_t, 2> a, SIMD<int64_t, 2> b)
{
	SIMD<int64_t, 2> tmp;
	tmp.m = vsubq_s64(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int8_t, 16> vqsub(SIMD<int8_t, 16> a, SIMD<int8_t, 16> b)
{
	SIMD<int8_t, 16> tmp;
	tmp.m = vqsubq_s8(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int16_t, 8> vqsub(SIMD<int16_t, 8> a, SIMD<int16_t, 8> b)
{
	SIMD<int16_t, 8> tmp;
	tmp.m = vqsubq_s16(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint8_t, 16> vqsub(SIMD<uint8_t, 16> a, SIMD<uint8_t, 16> b)
{
	SIMD<uint8_t, 16> tmp;
	tmp.m = vqsubq_u8(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint16_t, 8> vqsub(SIMD<uint16_t, 8> a, SIMD<uint16_t, 8> b)
{
	SIMD<uint16_t, 8> tmp;
	tmp.m = vqsubq_u16(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<float, 4> vabs(SIMD<float, 4> a)
{
	SIMD<float, 4> tmp;
	tmp.m = vabsq_f32(a.m);
	return tmp;
}

template <>
inline SIMD<int8_t, 16> vqabs(SIMD<int8_t, 16> a)
{
	SIMD<int8_t, 16> tmp;
	tmp.m = vqabsq_s8(a.m);
	return tmp;
}

template <>
inline SIMD<int16_t, 8> vqabs(SIMD<int16_t, 8> a)
{
	SIMD<int16_t, 8> tmp;
	tmp.m = vqabsq_s16(a.m);
	return tmp;
}

template <>
inline SIMD<float, 4> vsign(SIMD<float, 4> a, SIMD<float, 4> b)
{
	SIMD<float, 4> tmp;
	tmp.m = (float32x4_t)vbicq_u32(
		veorq_u32((uint32x4_t)a.m, vandq_u32((uint32x4_t)vdupq_n_f32(-0.f), (uint32x4_t)b.m)),
		vceqq_f32(b.m, vdupq_n_f32(0.f)));
	return tmp;
}

template <>
inline SIMD<int8_t, 16> vsign(SIMD<int8_t, 16> a, SIMD<int8_t, 16> b)
{
	SIMD<int8_t, 16> tmp;
	tmp.m = (int8x16_t)vorrq_u8(
		vandq_u8(vcgtq_s8(vdupq_n_s8(0), b.m), (uint8x16_t)vnegq_s8(a.m)),
		vandq_u8(vcgtq_s8(b.m, vdupq_n_s8(0)), (uint8x16_t)a.m));
	return tmp;
}

template <>
inline SIMD<uint8_t, 16> vorr(SIMD<uint8_t, 16> a, SIMD<uint8_t, 16> b)
{
	SIMD<uint8_t, 16> tmp;
	tmp.m = vorrq_u8(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint16_t, 8> vorr(SIMD<uint16_t, 8> a, SIMD<uint16_t, 8> b)
{
	SIMD<uint16_t, 8> tmp;
	tmp.m = vorrq_u16(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vorr(SIMD<uint32_t, 4> a, SIMD<uint32_t, 4> b)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = vorrq_u32(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint64_t, 2> vorr(SIMD<uint64_t, 2> a, SIMD<uint64_t, 2> b)
{
	SIMD<uint64_t, 2> tmp;
	tmp.m = vorrq_u64(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint8_t, 16> vand(SIMD<uint8_t, 16> a, SIMD<uint8_t, 16> b)
{
	SIMD<uint8_t, 16> tmp;
	tmp.m = vandq_u8(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint16_t, 8> vand(SIMD<uint16_t, 8> a, SIMD<uint16_t, 8> b)
{
	SIMD<uint16_t, 8> tmp;
	tmp.m = vandq_u16(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vand(SIMD<uint32_t, 4> a, SIMD<uint32_t, 4> b)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = vandq_u32(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint64_t, 2> vand(SIMD<uint64_t, 2> a, SIMD<uint64_t, 2> b)
{
	SIMD<uint64_t, 2> tmp;
	tmp.m = vandq_u64(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint8_t, 16> veor(SIMD<uint8_t, 16> a, SIMD<uint8_t, 16> b)
{
	SIMD<uint8_t, 16> tmp;
	tmp.m = veorq_u8(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint16_t, 8> veor(SIMD<uint16_t, 8> a, SIMD<uint16_t, 8> b)
{
	SIMD<uint16_t, 8> tmp;
	tmp.m = veorq_u16(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> veor(SIMD<uint32_t, 4> a, SIMD<uint32_t, 4> b)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = veorq_u32(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint64_t, 2> veor(SIMD<uint64_t, 2> a, SIMD<uint64_t, 2> b)
{
	SIMD<uint64_t, 2> tmp;
	tmp.m = veorq_u64(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint8_t, 16> vbic(SIMD<uint8_t, 16> a, SIMD<uint8_t, 16> b)
{
	SIMD<uint8_t, 16> tmp;
	tmp.m = vbicq_u8(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint16_t, 8> vbic(SIMD<uint16_t, 8> a, SIMD<uint16_t, 8> b)
{
	SIMD<uint16_t, 8> tmp;
	tmp.m = vbicq_u16(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vbic(SIMD<uint32_t, 4> a, SIMD<uint32_t, 4> b)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = vbicq_u32(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint64_t, 2> vbic(SIMD<uint64_t, 2> a, SIMD<uint64_t, 2> b)
{
	SIMD<uint64_t, 2> tmp;
	tmp.m = vbicq_u64(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint8_t, 16> vbsl(SIMD<uint8_t, 16> a, SIMD<uint8_t, 16> b, SIMD<uint8_t, 16> c)
{
	SIMD<uint8_t, 16> tmp;
	tmp.m = vbslq_u8(a.m, b.m, c.m);
	return tmp;
}

template <>
inline SIMD<uint16_t, 8> vbsl(SIMD<uint16_t, 8> a, SIMD<uint16_t, 8> b, SIMD<uint16_t, 8> c)
{
	SIMD<uint16_t, 8> tmp;
	tmp.m = vbslq_u16(a.m, b.m, c.m);
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vbsl(SIMD<uint32_t, 4> a, SIMD<uint32_t, 4> b, SIMD<uint32_t, 4> c)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = vbslq_u32(a.m, b.m, c.m);
	return tmp;
}

template <>
inline SIMD<uint64_t, 2> vbsl(SIMD<uint64_t, 2> a, SIMD<uint64_t, 2> b, SIMD<uint64_t, 2> c)
{
	SIMD<uint64_t, 2> tmp;
	tmp.m = vbslq_u64(a.m, b.m, c.m);
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vceqz(SIMD<float, 4> a)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = vceqq_f32(a.m, vdupq_n_f32(0.f));
	return tmp;
}

template <>
inline SIMD<uint8_t, 16> vceqz(SIMD<int8_t, 16> a)
{
	SIMD<uint8_t, 16> tmp;
	tmp.m = vceqq_s8(a.m, vdupq_n_s8(0));
	return tmp;
}

template <>
inline SIMD<uint16_t, 8> vceqz(SIMD<int16_t, 8> a)
{
	SIMD<uint16_t, 8> tmp;
	tmp.m = vceqq_s16(a.m, vdupq_n_s16(0));
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vceqz(SIMD<int32_t, 4> a)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = vceqq_s32(a.m, vdupq_n_s32(0));
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vceq(SIMD<float, 4> a, SIMD<float, 4> b)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = vceqq_f32(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint8_t, 16> vceq(SIMD<int8_t, 16> a, SIMD<int8_t, 16> b)
{
	SIMD<uint8_t, 16> tmp;
	tmp.m = vceqq_s8(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint16_t, 8> vceq(SIMD<int16_t, 8> a, SIMD<int16_t, 8> b)
{
	SIMD<uint16_t, 8> tmp;
	tmp.m = vceqq_s16(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vceq(SIMD<int32_t, 4> a, SIMD<int32_t, 4> b)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = vceqq_s32(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vcgtz(SIMD<float, 4> a)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = vcgtq_f32(a.m, vdupq_n_f32(0.f));
	return tmp;
}

template <>
inline SIMD<uint8_t, 16> vcgtz(SIMD<int8_t, 16> a)
{
	SIMD<uint8_t, 16> tmp;
	tmp.m = vcgtq_s8(a.m, vdupq_n_s8(0));
	return tmp;
}

template <>
inline SIMD<uint16_t, 8> vcgtz(SIMD<int16_t, 8> a)
{
	SIMD<uint16_t, 8> tmp;
	tmp.m = vcgtq_s16(a.m, vdupq_n_s16(0));
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vcgtz(SIMD<int32_t, 4> a)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = vcgtq_s32(a.m, vdupq_n_s32(0));
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vcltz(SIMD<float, 4> a)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = vcltq_f32(a.m, vdupq_n_f32(0.f));
	return tmp;
}

template <>
inline SIMD<uint8_t, 16> vcltz(SIMD<int8_t, 16> a)
{
	SIMD<uint8_t, 16> tmp;
	tmp.m = vcltq_s8(a.m, vdupq_n_s8(0));
	return tmp;
}

template <>
inline SIMD<uint16_t, 8> vcltz(SIMD<int16_t, 8> a)
{
	SIMD<uint16_t, 8> tmp;
	tmp.m = vcltq_s16(a.m, vdupq_n_s16(0));
	return tmp;
}

template <>
inline SIMD<uint32_t, 4> vcltz(SIMD<int32_t, 4> a)
{
	SIMD<uint32_t, 4> tmp;
	tmp.m = vcltq_s32(a.m, vdupq_n_s32(0));
	return tmp;
}

template <>
inline SIMD<float, 4> vmin(SIMD<float, 4> a, SIMD<float, 4> b)
{
	SIMD<float, 4> tmp;
	tmp.m = vminq_f32(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int8_t, 16> vmin(SIMD<int8_t, 16> a, SIMD<int8_t, 16> b)
{
	SIMD<int8_t, 16> tmp;
	tmp.m = vminq_s8(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int16_t, 8> vmin(SIMD<int16_t, 8> a, SIMD<int16_t, 8> b)
{
	SIMD<int16_t, 8> tmp;
	tmp.m = vminq_s16(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int32_t, 4> vmin(SIMD<int32_t, 4> a, SIMD<int32_t, 4> b)
{
	SIMD<int32_t, 4> tmp;
	tmp.m = vminq_s32(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<float, 4> vmax(SIMD<float, 4> a, SIMD<float, 4> b)
{
	SIMD<float, 4> tmp;
	tmp.m = vmaxq_f32(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int8_t, 16> vmax(SIMD<int8_t, 16> a, SIMD<int8_t, 16> b)
{
	SIMD<int8_t, 16> tmp;
	tmp.m = vmaxq_s8(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int16_t, 8> vmax(SIMD<int16_t, 8> a, SIMD<int16_t, 8> b)
{
	SIMD<int16_t, 8> tmp;
	tmp.m = vmaxq_s16(a.m, b.m);
	return tmp;
}

template <>
inline SIMD<int32_t, 4> vmax(SIMD<int32_t, 4> a, SIMD<int32_t, 4> b)
{
	SIMD<int32_t, 4> tmp;
	tmp.m = vmaxq_s32(a.m, b.m);
	return tmp;
}

} // namespace ldpctool

#endif
