// Copyright 2015-2020 Mobilinkd LLC.

#pragma once

#include "Filter.h"

#include <array>
#include <cstddef>

namespace mobilinkd
{

template <typename FloatType, size_t N>
struct BaseFirFilter : FilterBase<FloatType>
{
	using array_t = std::array<FloatType, N>;

	const array_t& taps_;
	array_t history_;
	size_t pos_ = 0;
	
	BaseFirFilter(const array_t& taps)
	: taps_(taps)
	{
		history_.fill(0.0);
	}
	
	FloatType operator()(FloatType input) override
	{
		history_[pos_++] = input;
		if (pos_ == N) pos_ = 0;

		FloatType result = 0.0;
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

template <typename FloatType, size_t N>
BaseFirFilter<FloatType, N> makeFirFilter(const std::array<FloatType, N>& taps)
{
	return std::move(BaseFirFilter<FloatType, N>(taps));
}


} // mobilinkd
