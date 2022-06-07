// Copyright 2015-2021 Mobilinkd LLC.

#pragma once

#include "Filter.h"

#include <array>
#include <cstddef>

namespace mobilinkd
{

template <typename FloatType, size_t N>
struct BaseIirFilter : FilterBase<FloatType>
{
	const std::array<FloatType, N>& numerator_;
	const std::array<FloatType, N> denominator_;
	std::array<FloatType, N> history_{0};
	
	BaseIirFilter(const std::array<FloatType, N>& b, const std::array<FloatType, N>& a)
	: numerator_(b), denominator_(a)
	{
		history_.fill(0.0);
	}
	
	FloatType operator()(FloatType input) {

		for (size_t i = N - 1; i != 0; i--) history_[i] = history_[i - 1];
		
		history_[0] = input;

		for (size_t i = 1; i != N; i++) {
			history_[0] -= denominator_[i] * history_[i];
		}
		
		FloatType result = 0;
		for (size_t i = 0; i != N; i++) {
			result += numerator_[i] * history_[i];
		}
		
		return result;
	}
};

template <typename FloatType, size_t N>
BaseIirFilter<FloatType, N> makeIirFilter(
	const std::array<FloatType, N>& b, const std::array<FloatType, N>& a)
{
	return std::move(BaseIirFilter<FloatType, N>(b, a));
}

} // mobilinkd
