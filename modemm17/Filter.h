// Copyright 2015-2021 Mobilinkd LLC.

#pragma once

namespace modemm17
{

template <typename NumericType>
struct FilterBase
{
	virtual NumericType operator()(NumericType input) = 0;
};

} // modemm17
