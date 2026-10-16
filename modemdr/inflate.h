///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef MODEMDR_INFLATE_H_
#define MODEMDR_INFLATE_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "export.h"

namespace modemdr
{

// Inflates raw DEFLATE data (RFC 1951) into output, up to maximumOutput bytes. Returns false for
// invalid or oversized data.
MODEMDR_API bool inflateRaw(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& output,
    std::size_t maximumOutput);

// Inflates gzip data (RFC 1952), checking its CRC-32 and size.
MODEMDR_API bool gunzip(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& output,
    std::size_t maximumOutput);

} // namespace modemdr

#endif // MODEMDR_INFLATE_H_
