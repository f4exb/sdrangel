///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "fixedtraits.h"

const int64_t FixedTraits<28>::log_two_power_n_reversed[35] = {
        0x18429946ELL,0x1791272EFLL,0x16DFB516FLL,0x162E42FF0LL,0x157CD0E70LL,0x14CB5ECF1LL,0x1419ECB71LL,0x13687A9F2LL,
        0x12B708872LL,0x1205966F3LL,0x115424573LL,0x10A2B23F4LL,0xFF140274LL,0xF3FCE0F5LL,0xE8E5BF75LL,0xDDCE9DF6LL,
        0xD2B77C76LL,0xC7A05AF7LL,0xBC893977LL,0xB17217F8LL,0xA65AF679LL,0x9B43D4F9LL,0x902CB379LL,0x851591FaLL,
        0x79FE707bLL,0x6EE74EFbLL,0x63D02D7BLL,0x58B90BFcLL,0x4DA1EA7CLL,0x428AC8FdLL,0x3773A77DLL,0x2C5C85FeLL,
        0x2145647ELL,0x162E42FfLL,0xB17217FLL
};

const int64_t FixedTraits<28>::log_one_plus_two_power_minus_n[28] = {
        0x67CC8FBLL,0x391FEF9LL,0x1E27077LL,0xF85186LL,
        0x7E0A6CLL,0x3F8151LL,0x1FE02ALL,0xFF805LL,0x7FE01LL,0x3FF80LL,0x1FFE0LL,0xFFF8LL,
        0x7FFELL,0x4000LL,0x2000LL,0x1000LL,0x800LL,0x400LL,0x200LL,0x100LL,
        0x80LL,0x40LL,0x20LL,0x10LL,0x8LL,0x4LL,0x2LL,0x1LL
};

const int64_t FixedTraits<28>::log_one_over_one_minus_two_power_minus_n[28] = {
        0xB172180LL,0x49A5884LL,0x222F1D0LL,0x108598BLL,
        0x820AECLL,0x408159LL,0x20202BLL,0x100805LL,0x80201LL,0x40080LL,0x20020LL,0x10008LL,
        0x8002LL,0x4001LL,0x2000LL,0x1000LL,0x800LL,0x400LL,0x200LL,0x100LL,
        0x80LL,0x40LL,0x20LL,0x10LL,0x8LL,0x4LL,0x2LL,0x1LL
};

const int64_t FixedTraits<28>::arctantab[32] = {
        297197971, 210828714, 124459457, 65760959, 33381290, 16755422, 8385879,
        4193963, 2097109, 1048571, 524287, 262144, 131072, 65536, 32768, 16384,
        8192, 4096, 2048, 1024, 512, 256, 128, 64, 32, 16, 8, 4, 2, 1, 0, 0,
};
