// Copyright 2015-2020 Mobilinkd LLC.

#pragma once

#include "Filter.h"

#include <array>
#include <cstddef>

namespace modemm17
{

template <size_t N>
struct BaseFirFilter : FilterBase<float>
{
	using array_t = std::array<float, N>;

	const array_t& taps_;
	array_t history_;
	size_t pos_ = 0;

	BaseFirFilter(const array_t& taps)
	: taps_(taps)
	{
		history_.fill(0.0);
	}

	float operator()(float input) override
	{
		history_[pos_++] = input;
		if (pos_ == N) pos_ = 0;

		float result = 0.0;
		size_t index = pos_;

		for (size_t i = 0; i != N; ++i)
		{
			index = (index != 0 ? index - 1 : N - 1);
			result += history_.at(index) * taps_.at(i);
		}

		return result;
	}

	void reset()
	{
		history_.fill(0.0);
		pos_ = 0;
	}
};

template <size_t N>
BaseFirFilter<N> makeFirFilter(const std::array<float, N>& taps)
{
	return std::move(BaseFirFilter<N>(taps));
}


} // modemm17
