// Copyright 2015-2021 Mobilinkd LLC.

#pragma once

namespace mobilinkd
{

template <typename NumericType>
struct FilterBase
{
	virtual NumericType operator()(NumericType input) = 0;
};

} // mobilinkd
