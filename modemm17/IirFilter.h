// Copyright 2015-2021 Mobilinkd LLC.

#pragma once

#include "Filter.h"

#include <array>
#include <cstddef>

namespace modemm17
{

template <size_t N>
struct BaseIirFilter : FilterBase<float>
{
	BaseIirFilter(const std::array<float, N>& b, const std::array<float, N>& a) :
        numerator_(b),
        denominator_(a)
	{
		history_.fill(0.0);
	}

	float operator()(float input) {

		for (size_t i = N - 1; i != 0; i--) history_[i] = history_[i - 1];

		history_[0] = input;

		for (size_t i = 1; i != N; i++) {
			history_[0] -= denominator_[i] * history_[i];
		}

		float result = 0;
		for (size_t i = 0; i != N; i++) {
			result += numerator_[i] * history_[i];
		}

		return result;
	}

private:
	const std::array<float, N>& numerator_;
	const std::array<float, N> denominator_;
	std::array<float, N> history_{0};
};

template <size_t N>
BaseIirFilter<N> makeIirFilter(const std::array<float, N>& b, const std::array<float, N>& a)
{
	return std::move(BaseIirFilter<N>(b, a));
}

} // modemm17
