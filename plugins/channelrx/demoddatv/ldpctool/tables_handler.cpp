/*
LDPC tables handler

Copyright 2018 Ahmet Inan <xdsopl@gmail.com>
*/

#include <cstring>
#include <cstdint>
#include "ldpc.h"
#include "dvb_s2_tables.h"
#include "dvb_s2x_tables.h"
#include "dvb_t2_tables.h"

namespace ldpctool {

const char *LDPCInterface::mc_tabnames[2][32] = { // [shortframes][modcod]
	{// Normal frames
		0, "B1", "B2", "B3", "B4", "B5", "B6", "B7",
		"B8", "B9", "B10", "B11", "B5", "B6", "B7", "B9",
		"B10", "B11", "B6", "B7", "B8", "B9", "B10", "B11",
		"B7", "B8", "B8", "B10", "B11", 0, 0, 0
	},
	{// Short frames
		0, "C1", "C2", "C3", "C4", "C5", "C6", "C7",
		"C8", "C9", "C10", 0, "C5", "C6", "C7", "C9",
		"C10", 0, "C6", "C7", "C8", "C9", "C10", 0,
		"C7", "C8", "C8", "C10", 0, 0, 0, 0
	}
};

LDPCInterface *create_ldpc(char *standard, char prefix, int number)
{
	if (!strcmp(standard, "S2"))
	{
		if (prefix == 'B')
		{
			switch (number)
			{
			case 1:
				return new LDPC<DVB_S2_TABLE_B1>();
			case 2:
				return new LDPC<DVB_S2_TABLE_B2>();
			case 3:
				return new LDPC<DVB_S2_TABLE_B3>();
			case 4:
				return new LDPC<DVB_S2_TABLE_B4>();
			case 5:
				return new LDPC<DVB_S2_TABLE_B5>();
			case 6:
				return new LDPC<DVB_S2_TABLE_B6>();
			case 7:
				return new LDPC<DVB_S2_TABLE_B7>();
			case 8:
				return new LDPC<DVB_S2_TABLE_B8>();
			case 9:
				return new LDPC<DVB_S2_TABLE_B9>();
			case 10:
				return new LDPC<DVB_S2_TABLE_B10>();
			case 11:
				return new LDPC<DVB_S2_TABLE_B11>();
			}
		}

		if (prefix == 'C')
		{
			switch (number)
			{
			case 1:
				return new LDPC<DVB_S2_TABLE_C1>();
			case 2:
				return new LDPC<DVB_S2_TABLE_C2>();
			case 3:
				return new LDPC<DVB_S2_TABLE_C3>();
			case 4:
				return new LDPC<DVB_S2_TABLE_C4>();
			case 5:
				return new LDPC<DVB_S2_TABLE_C5>();
			case 6:
				return new LDPC<DVB_S2_TABLE_C6>();
			case 7:
				return new LDPC<DVB_S2_TABLE_C7>();
			case 8:
				return new LDPC<DVB_S2_TABLE_C8>();
			case 9:
				return new LDPC<DVB_S2_TABLE_C9>();
			case 10:
				return new LDPC<DVB_S2_TABLE_C10>();
			}
		}
	}

	if (!strcmp(standard, "S2X"))
	{
		if (prefix == 'B')
		{
			switch (number)
			{
			case 1:
				return new LDPC<DVB_S2X_TABLE_B1>();
			case 2:
				return new LDPC<DVB_S2X_TABLE_B2>();
			case 3:
				return new LDPC<DVB_S2X_TABLE_B3>();
			case 4:
				return new LDPC<DVB_S2X_TABLE_B4>();
			case 5:
				return new LDPC<DVB_S2X_TABLE_B5>();
			case 6:
				return new LDPC<DVB_S2X_TABLE_B6>();
			case 7:
				return new LDPC<DVB_S2X_TABLE_B7>();
			case 8:
				return new LDPC<DVB_S2X_TABLE_B8>();
			case 9:
				return new LDPC<DVB_S2X_TABLE_B9>();
			case 10:
				return new LDPC<DVB_S2X_TABLE_B10>();
			case 11:
				return new LDPC<DVB_S2X_TABLE_B11>();
			case 12:
				return new LDPC<DVB_S2X_TABLE_B12>();
			case 13:
				return new LDPC<DVB_S2X_TABLE_B13>();
			case 14:
				return new LDPC<DVB_S2X_TABLE_B14>();
			case 15:
				return new LDPC<DVB_S2X_TABLE_B15>();
			case 16:
				return new LDPC<DVB_S2X_TABLE_B16>();
			case 17:
				return new LDPC<DVB_S2X_TABLE_B17>();
			case 18:
				return new LDPC<DVB_S2X_TABLE_B18>();
			case 19:
				return new LDPC<DVB_S2X_TABLE_B19>();
			case 20:
				return new LDPC<DVB_S2X_TABLE_B20>();
			case 21:
				return new LDPC<DVB_S2X_TABLE_B21>();
			case 22:
				return new LDPC<DVB_S2X_TABLE_B22>();
			case 23:
				return new LDPC<DVB_S2X_TABLE_B23>();
			case 24:
				return new LDPC<DVB_S2X_TABLE_B24>();
			}
		}

		if (prefix == 'C')
		{
			switch (number)
			{
			case 1:
				return new LDPC<DVB_S2X_TABLE_C1>();
			case 2:
				return new LDPC<DVB_S2X_TABLE_C2>();
			case 3:
				return new LDPC<DVB_S2X_TABLE_C3>();
			case 4:
				return new LDPC<DVB_S2X_TABLE_C4>();
			case 5:
				return new LDPC<DVB_S2X_TABLE_C5>();
			case 6:
				return new LDPC<DVB_S2X_TABLE_C6>();
			case 7:
				return new LDPC<DVB_S2X_TABLE_C7>();
			case 8:
				return new LDPC<DVB_S2X_TABLE_C8>();
			case 9:
				return new LDPC<DVB_S2X_TABLE_C9>();
			case 10:
				return new LDPC<DVB_S2X_TABLE_C10>();
			}
		}
	}

	if (!strcmp(standard, "T2"))
	{
		if (prefix == 'A')
		{
			switch (number)
			{
			case 1:
				return new LDPC<DVB_T2_TABLE_A1>();
			case 2:
				return new LDPC<DVB_T2_TABLE_A2>();
			case 3:
				return new LDPC<DVB_T2_TABLE_A3>();
			case 4:
				return new LDPC<DVB_T2_TABLE_A4>();
			case 5:
				return new LDPC<DVB_T2_TABLE_A5>();
			case 6:
				return new LDPC<DVB_T2_TABLE_A6>();
			}
		}

		if (prefix == 'B')
		{
			switch (number)
			{
			case 1:
				return new LDPC<DVB_T2_TABLE_B1>();
			case 2:
				return new LDPC<DVB_T2_TABLE_B2>();
			case 3:
				return new LDPC<DVB_T2_TABLE_B3>();
			case 4:
				return new LDPC<DVB_T2_TABLE_B4>();
			case 5:
				return new LDPC<DVB_T2_TABLE_B5>();
			case 6:
				return new LDPC<DVB_T2_TABLE_B6>();
			case 7:
				return new LDPC<DVB_T2_TABLE_B7>();
			case 8:
				return new LDPC<DVB_T2_TABLE_B8>();
			case 9:
				return new LDPC<DVB_T2_TABLE_B9>();
			}
		}
	}

	return nullptr;
}

constexpr int DVB_S2_TABLE_B1::DEG[];
constexpr int DVB_S2_TABLE_B1::LEN[];
constexpr int DVB_S2_TABLE_B1::POS[];

constexpr int DVB_S2_TABLE_B2::DEG[];
constexpr int DVB_S2_TABLE_B2::LEN[];
constexpr int DVB_S2_TABLE_B2::POS[];

constexpr int DVB_S2_TABLE_B3::DEG[];
constexpr int DVB_S2_TABLE_B3::LEN[];
constexpr int DVB_S2_TABLE_B3::POS[];

constexpr int DVB_S2_TABLE_B4::DEG[];
constexpr int DVB_S2_TABLE_B4::LEN[];
constexpr int DVB_S2_TABLE_B4::POS[];

constexpr int DVB_S2_TABLE_B5::DEG[];
constexpr int DVB_S2_TABLE_B5::LEN[];
constexpr int DVB_S2_TABLE_B5::POS[];

constexpr int DVB_S2_TABLE_B6::DEG[];
constexpr int DVB_S2_TABLE_B6::LEN[];
constexpr int DVB_S2_TABLE_B6::POS[];

constexpr int DVB_S2_TABLE_B7::DEG[];
constexpr int DVB_S2_TABLE_B7::LEN[];
constexpr int DVB_S2_TABLE_B7::POS[];

constexpr int DVB_S2_TABLE_B8::DEG[];
constexpr int DVB_S2_TABLE_B8::LEN[];
constexpr int DVB_S2_TABLE_B8::POS[];

constexpr int DVB_S2_TABLE_B9::DEG[];
constexpr int DVB_S2_TABLE_B9::LEN[];
constexpr int DVB_S2_TABLE_B9::POS[];

constexpr int DVB_S2_TABLE_B10::DEG[];
constexpr int DVB_S2_TABLE_B10::LEN[];
constexpr int DVB_S2_TABLE_B10::POS[];

constexpr int DVB_S2_TABLE_B11::DEG[];
constexpr int DVB_S2_TABLE_B11::LEN[];
constexpr int DVB_S2_TABLE_B11::POS[];

constexpr int DVB_S2_TABLE_C1::DEG[];
constexpr int DVB_S2_TABLE_C1::LEN[];
constexpr int DVB_S2_TABLE_C1::POS[];

constexpr int DVB_S2_TABLE_C2::DEG[];
constexpr int DVB_S2_TABLE_C2::LEN[];
constexpr int DVB_S2_TABLE_C2::POS[];

constexpr int DVB_S2_TABLE_C3::DEG[];
constexpr int DVB_S2_TABLE_C3::LEN[];
constexpr int DVB_S2_TABLE_C3::POS[];

constexpr int DVB_S2_TABLE_C4::DEG[];
constexpr int DVB_S2_TABLE_C4::LEN[];
constexpr int DVB_S2_TABLE_C4::POS[];

constexpr int DVB_S2_TABLE_C5::DEG[];
constexpr int DVB_S2_TABLE_C5::LEN[];
constexpr int DVB_S2_TABLE_C5::POS[];

constexpr int DVB_S2_TABLE_C6::DEG[];
constexpr int DVB_S2_TABLE_C6::LEN[];
constexpr int DVB_S2_TABLE_C6::POS[];

constexpr int DVB_S2_TABLE_C7::DEG[];
constexpr int DVB_S2_TABLE_C7::LEN[];
constexpr int DVB_S2_TABLE_C7::POS[];

constexpr int DVB_S2_TABLE_C8::DEG[];
constexpr int DVB_S2_TABLE_C8::LEN[];
constexpr int DVB_S2_TABLE_C8::POS[];

constexpr int DVB_S2_TABLE_C9::DEG[];
constexpr int DVB_S2_TABLE_C9::LEN[];
constexpr int DVB_S2_TABLE_C9::POS[];

constexpr int DVB_S2_TABLE_C10::DEG[];
constexpr int DVB_S2_TABLE_C10::LEN[];
constexpr int DVB_S2_TABLE_C10::POS[];

constexpr int DVB_S2X_TABLE_B1::DEG[];
constexpr int DVB_S2X_TABLE_B1::LEN[];
constexpr int DVB_S2X_TABLE_B1::POS[];

constexpr int DVB_S2X_TABLE_B2::DEG[];
constexpr int DVB_S2X_TABLE_B2::LEN[];
constexpr int DVB_S2X_TABLE_B2::POS[];

constexpr int DVB_S2X_TABLE_B3::DEG[];
constexpr int DVB_S2X_TABLE_B3::LEN[];
constexpr int DVB_S2X_TABLE_B3::POS[];

constexpr int DVB_S2X_TABLE_B4::DEG[];
constexpr int DVB_S2X_TABLE_B4::LEN[];
constexpr int DVB_S2X_TABLE_B4::POS[];

constexpr int DVB_S2X_TABLE_B5::DEG[];
constexpr int DVB_S2X_TABLE_B5::LEN[];
constexpr int DVB_S2X_TABLE_B5::POS[];

constexpr int DVB_S2X_TABLE_B6::DEG[];
constexpr int DVB_S2X_TABLE_B6::LEN[];
constexpr int DVB_S2X_TABLE_B6::POS[];

constexpr int DVB_S2X_TABLE_B7::DEG[];
constexpr int DVB_S2X_TABLE_B7::LEN[];
constexpr int DVB_S2X_TABLE_B7::POS[];

constexpr int DVB_S2X_TABLE_B8::DEG[];
constexpr int DVB_S2X_TABLE_B8::LEN[];
constexpr int DVB_S2X_TABLE_B8::POS[];

constexpr int DVB_S2X_TABLE_B9::DEG[];
constexpr int DVB_S2X_TABLE_B9::LEN[];
constexpr int DVB_S2X_TABLE_B9::POS[];

constexpr int DVB_S2X_TABLE_B10::DEG[];
constexpr int DVB_S2X_TABLE_B10::LEN[];
constexpr int DVB_S2X_TABLE_B10::POS[];

constexpr int DVB_S2X_TABLE_B11::DEG[];
constexpr int DVB_S2X_TABLE_B11::LEN[];
constexpr int DVB_S2X_TABLE_B11::POS[];

constexpr int DVB_S2X_TABLE_B12::DEG[];
constexpr int DVB_S2X_TABLE_B12::LEN[];
constexpr int DVB_S2X_TABLE_B12::POS[];

constexpr int DVB_S2X_TABLE_B13::DEG[];
constexpr int DVB_S2X_TABLE_B13::LEN[];
constexpr int DVB_S2X_TABLE_B13::POS[];

constexpr int DVB_S2X_TABLE_B14::DEG[];
constexpr int DVB_S2X_TABLE_B14::LEN[];
constexpr int DVB_S2X_TABLE_B14::POS[];

constexpr int DVB_S2X_TABLE_B15::DEG[];
constexpr int DVB_S2X_TABLE_B15::LEN[];
constexpr int DVB_S2X_TABLE_B15::POS[];

constexpr int DVB_S2X_TABLE_B16::DEG[];
constexpr int DVB_S2X_TABLE_B16::LEN[];
constexpr int DVB_S2X_TABLE_B16::POS[];

constexpr int DVB_S2X_TABLE_B17::DEG[];
constexpr int DVB_S2X_TABLE_B17::LEN[];
constexpr int DVB_S2X_TABLE_B17::POS[];

constexpr int DVB_S2X_TABLE_B18::DEG[];
constexpr int DVB_S2X_TABLE_B18::LEN[];
constexpr int DVB_S2X_TABLE_B18::POS[];

constexpr int DVB_S2X_TABLE_B19::DEG[];
constexpr int DVB_S2X_TABLE_B19::LEN[];
constexpr int DVB_S2X_TABLE_B19::POS[];

constexpr int DVB_S2X_TABLE_B20::DEG[];
constexpr int DVB_S2X_TABLE_B20::LEN[];
constexpr int DVB_S2X_TABLE_B20::POS[];

constexpr int DVB_S2X_TABLE_B21::DEG[];
constexpr int DVB_S2X_TABLE_B21::LEN[];
constexpr int DVB_S2X_TABLE_B21::POS[];

constexpr int DVB_S2X_TABLE_B22::DEG[];
constexpr int DVB_S2X_TABLE_B22::LEN[];
constexpr int DVB_S2X_TABLE_B22::POS[];

constexpr int DVB_S2X_TABLE_B23::DEG[];
constexpr int DVB_S2X_TABLE_B23::LEN[];
constexpr int DVB_S2X_TABLE_B23::POS[];

constexpr int DVB_S2X_TABLE_B24::DEG[];
constexpr int DVB_S2X_TABLE_B24::LEN[];
constexpr int DVB_S2X_TABLE_B24::POS[];

constexpr int DVB_S2X_TABLE_C1::DEG[];
constexpr int DVB_S2X_TABLE_C1::LEN[];
constexpr int DVB_S2X_TABLE_C1::POS[];

constexpr int DVB_S2X_TABLE_C2::DEG[];
constexpr int DVB_S2X_TABLE_C2::LEN[];
constexpr int DVB_S2X_TABLE_C2::POS[];

constexpr int DVB_S2X_TABLE_C3::DEG[];
constexpr int DVB_S2X_TABLE_C3::LEN[];
constexpr int DVB_S2X_TABLE_C3::POS[];

constexpr int DVB_S2X_TABLE_C4::DEG[];
constexpr int DVB_S2X_TABLE_C4::LEN[];
constexpr int DVB_S2X_TABLE_C4::POS[];

constexpr int DVB_S2X_TABLE_C5::DEG[];
constexpr int DVB_S2X_TABLE_C5::LEN[];
constexpr int DVB_S2X_TABLE_C5::POS[];

constexpr int DVB_S2X_TABLE_C6::DEG[];
constexpr int DVB_S2X_TABLE_C6::LEN[];
constexpr int DVB_S2X_TABLE_C6::POS[];

constexpr int DVB_S2X_TABLE_C7::DEG[];
constexpr int DVB_S2X_TABLE_C7::LEN[];
constexpr int DVB_S2X_TABLE_C7::POS[];

constexpr int DVB_S2X_TABLE_C8::DEG[];
constexpr int DVB_S2X_TABLE_C8::LEN[];
constexpr int DVB_S2X_TABLE_C8::POS[];

constexpr int DVB_S2X_TABLE_C9::DEG[];
constexpr int DVB_S2X_TABLE_C9::LEN[];
constexpr int DVB_S2X_TABLE_C9::POS[];

constexpr int DVB_S2X_TABLE_C10::DEG[];
constexpr int DVB_S2X_TABLE_C10::LEN[];
constexpr int DVB_S2X_TABLE_C10::POS[];

constexpr int DVB_T2_TABLE_A1::DEG[];
constexpr int DVB_T2_TABLE_A1::LEN[];
constexpr int DVB_T2_TABLE_A1::POS[];

constexpr int DVB_T2_TABLE_A2::DEG[];
constexpr int DVB_T2_TABLE_A2::LEN[];
constexpr int DVB_T2_TABLE_A2::POS[];

constexpr int DVB_T2_TABLE_A3::DEG[];
constexpr int DVB_T2_TABLE_A3::LEN[];
constexpr int DVB_T2_TABLE_A3::POS[];

constexpr int DVB_T2_TABLE_A4::DEG[];
constexpr int DVB_T2_TABLE_A4::LEN[];
constexpr int DVB_T2_TABLE_A4::POS[];

constexpr int DVB_T2_TABLE_A5::DEG[];
constexpr int DVB_T2_TABLE_A5::LEN[];
constexpr int DVB_T2_TABLE_A5::POS[];

constexpr int DVB_T2_TABLE_A6::DEG[];
constexpr int DVB_T2_TABLE_A6::LEN[];
constexpr int DVB_T2_TABLE_A6::POS[];

constexpr int DVB_T2_TABLE_B1::DEG[];
constexpr int DVB_T2_TABLE_B1::LEN[];
constexpr int DVB_T2_TABLE_B1::POS[];

constexpr int DVB_T2_TABLE_B2::DEG[];
constexpr int DVB_T2_TABLE_B2::LEN[];
constexpr int DVB_T2_TABLE_B2::POS[];

constexpr int DVB_T2_TABLE_B3::DEG[];
constexpr int DVB_T2_TABLE_B3::LEN[];
constexpr int DVB_T2_TABLE_B3::POS[];

constexpr int DVB_T2_TABLE_B4::DEG[];
constexpr int DVB_T2_TABLE_B4::LEN[];
constexpr int DVB_T2_TABLE_B4::POS[];

constexpr int DVB_T2_TABLE_B5::DEG[];
constexpr int DVB_T2_TABLE_B5::LEN[];
constexpr int DVB_T2_TABLE_B5::POS[];

constexpr int DVB_T2_TABLE_B6::DEG[];
constexpr int DVB_T2_TABLE_B6::LEN[];
constexpr int DVB_T2_TABLE_B6::POS[];

constexpr int DVB_T2_TABLE_B7::DEG[];
constexpr int DVB_T2_TABLE_B7::LEN[];
constexpr int DVB_T2_TABLE_B7::POS[];

constexpr int DVB_T2_TABLE_B8::DEG[];
constexpr int DVB_T2_TABLE_B8::LEN[];
constexpr int DVB_T2_TABLE_B8::POS[];

constexpr int DVB_T2_TABLE_B9::DEG[];
constexpr int DVB_T2_TABLE_B9::LEN[];
constexpr int DVB_T2_TABLE_B9::POS[];

} // namespace ldpctool
