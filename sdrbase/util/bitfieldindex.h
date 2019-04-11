///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef _UTIL_BITFIELDINDEX_H_
#define _UTIL_BITFIELDINDEX_H_

#include <stdint.h>

template <unsigned int size>
struct BitfieldIndex
{
    uint32_t v : size;

    BitfieldIndex() : v(0) {}
    BitfieldIndex(int i) { v = i; }

    BitfieldIndex& operator=(const BitfieldIndex& rhs) { v = rhs.v; return *this; }
    BitfieldIndex& operator=(const int& rhi) { v = rhi; return *this; }
    BitfieldIndex& operator++() { v++; return *this; }
    BitfieldIndex operator++(int) { BitfieldIndex x(*this); ++(*this); return x; }
    BitfieldIndex& operator+=(const BitfieldIndex& b) { v += b.v; return *this; }
    BitfieldIndex& operator-=(const BitfieldIndex& b) { v -= b.v; return *this; }
    BitfieldIndex& operator+=(int i) { v += i; return *this; }
    BitfieldIndex& operator-=(int i) { v -= i; return *this; }
    BitfieldIndex operator+(const BitfieldIndex& b) const { BitfieldIndex x(*this); x.v += b.v; return x; }
    BitfieldIndex operator-(const BitfieldIndex& b) const { BitfieldIndex x(*this); x.v -= b.v; return x; }
    BitfieldIndex operator+(int i) const { BitfieldIndex x(*this); x.v += i; return x; }
    BitfieldIndex operator-(int i) const { BitfieldIndex x(*this); x.v -= i; return x; }

    operator int() const { return v; }
};

template <unsigned int size>
BitfieldIndex<size> operator+(const BitfieldIndex<size> &a, const BitfieldIndex<size> &b)
{
    BitfieldIndex<size> x;
    x.v = a.v + b.v;
    return x;
}

template <unsigned int size>
BitfieldIndex<size> operator-(const BitfieldIndex<size> &a, const BitfieldIndex<size> &b)
{
    BitfieldIndex<size> x;
    x.v = a.v - b.v;
    return x;
}

#endif // _UTIL_BITFIELDINDEX_H_
