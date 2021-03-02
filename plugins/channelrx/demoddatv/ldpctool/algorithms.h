/*
SIMD-ified LDPC algorithms

Copyright 2018 Ahmet Inan <xdsopl@gmail.com>
*/

#ifndef ALGORITHMS_HH
#define ALGORITHMS_HH

#include "generic.h"
#include "exclusive_reduce.h"

namespace ldpctool {

template <typename VALUE, int WIDTH>
struct SelfCorrectedUpdate<SIMD<VALUE, WIDTH>>
{
	typedef SIMD<VALUE, WIDTH> TYPE;
	static void update(TYPE *a, TYPE b)
	{
		*a = vreinterpret<TYPE>(vand(vmask(b), vorr(vceqz(*a), veor(vcgtz(*a), vcltz(b)))));
	}
};

template <typename VALUE, int WIDTH, typename UPDATE>
struct MinSumAlgorithm<SIMD<VALUE, WIDTH>, UPDATE>
{
	typedef SIMD<VALUE, WIDTH> TYPE;
	static TYPE zero()
	{
		return vzero<TYPE>();
	}
	static TYPE one()
	{
		return vdup<TYPE>(1);
	}
	static TYPE min(TYPE a, TYPE b)
	{
		return vmin(a, b);
	}
	static TYPE sign(TYPE a, TYPE b)
	{
		return vsign(a, b);
	}
	static void finalp(TYPE *links, int cnt)
	{
		TYPE mags[cnt], mins[cnt];
		for (int i = 0; i < cnt; ++i)
			mags[i] = vabs(links[i]);
		CODE::exclusive_reduce(mags, mins, cnt, min);

		TYPE signs[cnt];
		CODE::exclusive_reduce(links, signs, cnt, sign);

		for (int i = 0; i < cnt; ++i)
			links[i] = sign(mins[i], signs[i]);
	}
	static TYPE add(TYPE a, TYPE b)
	{
		return vadd(a, b);
	}
	static TYPE sub(TYPE a, TYPE b)
	{
		return vsub(a, b);
	}
	static bool bad(TYPE v, int blocks)
	{
		auto tmp = vcgtz(v);
		for (int i = 0; i < blocks; ++i)
			if (!tmp.v[i])
				return true;
		return false;
	}
	static void update(TYPE *a, TYPE b)
	{
		UPDATE::update(a, b);
	}
};

template <int WIDTH, typename UPDATE>
struct MinSumAlgorithm<SIMD<int8_t, WIDTH>, UPDATE>
{
	typedef int8_t VALUE;
	typedef SIMD<VALUE, WIDTH> TYPE;
	static TYPE zero()
	{
		return vzero<TYPE>();
	}
	static TYPE one()
	{
		return vdup<TYPE>(1);
	}
	static TYPE sign(TYPE a, TYPE b)
	{
		return vsign(a, b);
	}
	static TYPE eor(TYPE a, TYPE b)
	{
		return vreinterpret<TYPE>(veor(vmask(a), vmask(b)));
	}
	static TYPE orr(TYPE a, TYPE b)
	{
		return vreinterpret<TYPE>(vorr(vmask(a), vmask(b)));
	}
	static TYPE other(TYPE a, TYPE b, TYPE c)
	{
		return vreinterpret<TYPE>(vbsl(vceq(a, b), vmask(c), vmask(b)));
	}
	static void finalp(TYPE *links, int cnt)
	{
		TYPE mags[cnt];
		for (int i = 0; i < cnt; ++i)
			mags[i] = vqabs(links[i]);

		TYPE mins[2];
		mins[0] = vmin(mags[0], mags[1]);
		mins[1] = vmax(mags[0], mags[1]);
		for (int i = 2; i < cnt; ++i) {
			mins[1] = vmin(mins[1], vmax(mins[0], mags[i]));
			mins[0] = vmin(mins[0], mags[i]);
		}

		TYPE signs = links[0];
		for (int i = 1; i < cnt; ++i)
			signs = eor(signs, links[i]);

		for (int i = 0; i < cnt; ++i)
			links[i] = sign(other(mags[i], mins[0], mins[1]), orr(eor(signs, links[i]), vdup<TYPE>(127)));
	}
	static TYPE add(TYPE a, TYPE b)
	{
		return vqadd(a, b);
	}
	static TYPE sub(TYPE a, TYPE b)
	{
		return vqsub(a, b);
	}
	static bool bad(TYPE v, int blocks)
	{
		auto tmp = vcgtz(v);
		for (int i = 0; i < blocks; ++i)
			if (!tmp.v[i])
				return true;
		return false;
	}
	static void update(TYPE *a, TYPE b)
	{
		UPDATE::update(a, vmin(vmax(b, vdup<TYPE>(-32)), vdup<TYPE>(31)));
	}
};

template <typename VALUE, int WIDTH, typename UPDATE, int FACTOR>
struct OffsetMinSumAlgorithm<SIMD<VALUE, WIDTH>, UPDATE, FACTOR>
{
	typedef SIMD<VALUE, WIDTH> TYPE;
	static TYPE zero()
	{
		return vzero<TYPE>();
	}
	static TYPE one()
	{
		return vdup<TYPE>(1);
	}
	static TYPE min(TYPE a, TYPE b)
	{
		return vmin(a, b);
	}
	static TYPE sign(TYPE a, TYPE b)
	{
		return vsign(a, b);
	}
	static void finalp(TYPE *links, int cnt)
	{
		TYPE beta = vdup<TYPE>(0.5 * FACTOR);
		TYPE mags[cnt], mins[cnt];
		for (int i = 0; i < cnt; ++i)
			mags[i] = vmax(vsub(vabs(links[i]), beta), vzero<TYPE>());
		CODE::exclusive_reduce(mags, mins, cnt, min);

		TYPE signs[cnt];
		CODE::exclusive_reduce(links, signs, cnt, sign);

		for (int i = 0; i < cnt; ++i)
			links[i] = sign(mins[i], signs[i]);
	}
	static TYPE add(TYPE a, TYPE b)
	{
		return vadd(a, b);
	}
	static TYPE sub(TYPE a, TYPE b)
	{
		return vsub(a, b);
	}
	static bool bad(TYPE v, int blocks)
	{
		auto tmp = vcgtz(v);
		for (int i = 0; i < blocks; ++i)
			if (!tmp.v[i])
				return true;
		return false;
	}
	static void update(TYPE *a, TYPE b)
	{
		UPDATE::update(a, b);
	}
};

template <int WIDTH, typename UPDATE, int FACTOR>
struct OffsetMinSumAlgorithm<SIMD<int8_t, WIDTH>, UPDATE, FACTOR>
{
	typedef int8_t VALUE;
	typedef SIMD<VALUE, WIDTH> TYPE;
	static TYPE zero()
	{
		return vzero<TYPE>();
	}
	static TYPE one()
	{
		return vdup<TYPE>(1);
	}
	static TYPE sign(TYPE a, TYPE b)
	{
		return vsign(a, b);
	}
	static TYPE eor(TYPE a, TYPE b)
	{
		return vreinterpret<TYPE>(veor(vmask(a), vmask(b)));
	}
	static TYPE orr(TYPE a, TYPE b)
	{
		return vreinterpret<TYPE>(vorr(vmask(a), vmask(b)));
	}
	static TYPE other(TYPE a, TYPE b, TYPE c)
	{
		return vreinterpret<TYPE>(vbsl(vceq(a, b), vmask(c), vmask(b)));
	}
	static void finalp(TYPE *links, int cnt)
	{
		auto beta = vunsigned(vdup<TYPE>(std::nearbyint(0.5 * FACTOR)));
		TYPE mags[cnt];
		for (int i = 0; i < cnt; ++i)
			mags[i] = vsigned(vqsub(vunsigned(vqabs(links[i])), beta));

		TYPE mins[2];
		mins[0] = vmin(mags[0], mags[1]);
		mins[1] = vmax(mags[0], mags[1]);
		for (int i = 2; i < cnt; ++i) {
			mins[1] = vmin(mins[1], vmax(mins[0], mags[i]));
			mins[0] = vmin(mins[0], mags[i]);
		}

		TYPE signs = links[0];
		for (int i = 1; i < cnt; ++i)
			signs = eor(signs, links[i]);

		for (int i = 0; i < cnt; ++i)
			links[i] = sign(other(mags[i], mins[0], mins[1]), orr(eor(signs, links[i]), vdup<TYPE>(127)));
	}
	static TYPE add(TYPE a, TYPE b)
	{
		return vqadd(a, b);
	}
	static TYPE sub(TYPE a, TYPE b)
	{
		return vqsub(a, b);
	}
	static bool bad(TYPE v, int blocks)
	{
		auto tmp = vcgtz(v);
		for (int i = 0; i < blocks; ++i)
			if (!tmp.v[i])
				return true;
		return false;
	}
	static void update(TYPE *a, TYPE b)
	{
		UPDATE::update(a, vmin(vmax(b, vdup<TYPE>(-32)), vdup<TYPE>(31)));
	}
};


template <typename VALUE, int WIDTH, typename UPDATE, int FACTOR>
struct MinSumCAlgorithm<SIMD<VALUE, WIDTH>, UPDATE, FACTOR>
{
	typedef SIMD<VALUE, WIDTH> TYPE;
	static TYPE zero()
	{
		return vzero<TYPE>();
	}
	static TYPE one()
	{
		return vdup<TYPE>(1);
	}
	static TYPE sign(TYPE a, TYPE b)
	{
		return vsign(a, b);
	}
	static TYPE correction_factor(TYPE a, TYPE b)
	{
		TYPE apb = vabs(vadd(a, b));
		TYPE apb2 = vadd(apb, apb);
		TYPE amb = vabs(vsub(a, b));
		TYPE amb2 = vadd(amb, amb);
		TYPE factor2 = vdup<TYPE>(FACTOR * 2);
		auto pc = vmask(vdup<TYPE>(VALUE(FACTOR) / VALUE(2)));
		auto nc = vmask(vdup<TYPE>(-VALUE(FACTOR) / VALUE(2)));
		pc = vand(pc, vand(vcgt(factor2, apb), vcgt(amb, apb2)));
		nc = vand(nc, vand(vcgt(factor2, amb), vcgt(apb, amb2)));
		return vreinterpret<TYPE>(vorr(pc, nc));
	}
	static TYPE minc(TYPE a, TYPE b)
	{
		TYPE m = vmin(vabs(a), vabs(b));
		TYPE x = vsign(vsign(m, a), b);
		x = vadd(x, correction_factor(a, b));
		return x;
	}
	static void finalp(TYPE *links, int cnt)
	{
		TYPE tmp[cnt];
		CODE::exclusive_reduce(links, tmp, cnt, minc);
		for (int i = 0; i < cnt; ++i)
			links[i] = tmp[i];
	}
	static TYPE add(TYPE a, TYPE b)
	{
		return vadd(a, b);
	}
	static TYPE sub(TYPE a, TYPE b)
	{
		return vsub(a, b);
	}
	static bool bad(TYPE v, int blocks)
	{
		auto tmp = vcgtz(v);
		for (int i = 0; i < blocks; ++i)
			if (!tmp.v[i])
				return true;
		return false;
	}
	static void update(TYPE *a, TYPE b)
	{
		UPDATE::update(a, b);
	}
};

template <int WIDTH, typename UPDATE, int FACTOR>
struct MinSumCAlgorithm<SIMD<int8_t, WIDTH>, UPDATE, FACTOR>
{
	typedef int8_t VALUE;
	typedef SIMD<VALUE, WIDTH> TYPE;
	static TYPE zero()
	{
		return vzero<TYPE>();
	}
	static TYPE one()
	{
		return vdup<TYPE>(1);
	}
	static TYPE sign(TYPE a, TYPE b)
	{
		return vsign(a, b);
	}
	static TYPE correction_factor(TYPE a, TYPE b)
	{
		TYPE apb = vqabs(vqadd(a, b));
		TYPE apb2 = vqadd(apb, apb);
		TYPE amb = vqabs(vqsub(a, b));
		TYPE amb2 = vqadd(amb, amb);
		TYPE factor2 = vdup<TYPE>(FACTOR * 2);
		auto pc = vmask(vdup<TYPE>(VALUE(FACTOR) / VALUE(2)));
		auto nc = vmask(vdup<TYPE>(-VALUE(FACTOR) / VALUE(2)));
		pc = vand(pc, vand(vcgt(factor2, apb), vcgt(amb, apb2)));
		nc = vand(nc, vand(vcgt(factor2, amb), vcgt(apb, amb2)));
		return vreinterpret<TYPE>(vorr(pc, nc));
	}
	static TYPE minc(TYPE a, TYPE b)
	{
		TYPE m = vmin(vqabs(a), vqabs(b));
		TYPE x = vsign(vsign(m, a), b);
		x = vqadd(x, correction_factor(a, b));
		return x;
	}
	static void finalp(TYPE *links, int cnt)
	{
		TYPE *tmp = new TYPE[cnt];
		CODE::exclusive_reduce(links, tmp, cnt, minc);
		for (int i = 0; i < cnt; ++i)
			links[i] = tmp[i];
		delete[] tmp;
	}
	static TYPE add(TYPE a, TYPE b)
	{
		return vqadd(a, b);
	}
	static TYPE sub(TYPE a, TYPE b)
	{
		return vqsub(a, b);
	}
	static bool bad(TYPE v, int blocks)
	{
		auto tmp = vcgtz(v);
		for (int i = 0; i < blocks; ++i)
			if (!tmp.v[i])
				return true;
		return false;
	}
	static void update(TYPE *a, TYPE b)
	{
		UPDATE::update(a, vmin(vmax(b, vdup<TYPE>(-32)), vdup<TYPE>(31)));
	}
};

} // namespace ldpctool

#endif
