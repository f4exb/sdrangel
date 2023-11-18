///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Edouard Griffiths, F4EXB <f4exb06@gmail.com>                   //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
/*
LDPC SISO encoder v2

Copyright 2018 Ahmet Inan <xdsopl@gmail.com>
*/

#ifndef ENCODER_HH
#define ENCODER_HH

#include "ldpc.h"

namespace ldpctool {

template <typename TYPE>
class LDPCEncoder
{
	uint16_t *pos;
	uint8_t *cnc;
	int R, CNL;
	bool initialized;

	TYPE one()
	{
		return 1;
	}
	TYPE sign(TYPE a, TYPE b)
	{
		return b < TYPE(0) ? -a : b > TYPE(0) ? a : TYPE(0);
	}
public:
	LDPCEncoder() : initialized(false)
	{
	}
	void init(LDPCInterface *it)
	{
		if (initialized) {
			delete[] pos;
			delete[] cnc;
		}
		initialized = true;
		LDPCInterface *ldpc = it->clone();
		int N = ldpc->code_len();
		int K = ldpc->data_len();
		R = N - K;
		CNL = ldpc->links_max_cn() - 2;
		pos = new uint16_t[R * CNL];
		cnc = new uint8_t[R];
		for (int i = 0; i < R; ++i)
			cnc[i] = 0;
		ldpc->first_bit();
		for (int j = 0; j < K; ++j) {
			int *acc_pos = ldpc->acc_pos();
			int bit_deg = ldpc->bit_deg();
			for (int n = 0; n < bit_deg; ++n) {
				int i = acc_pos[n];
				pos[CNL*i+cnc[i]++] = j;
			}
			ldpc->next_bit();
		}
		delete ldpc;
	}
	void operator()(TYPE *data, TYPE *parity)
	{
		TYPE tmp = one();
		for (int i = 0; i < R; ++i) {
			for (int j = 0; j < cnc[i]; ++j)
				tmp = sign(tmp, data[pos[CNL*i+j]]);
			parity[i] = tmp;
		}
	}
	~LDPCEncoder()
	{
		if (initialized) {
			delete[] pos;
			delete[] cnc;
		}
	}
};

} // namespace ldpctool

#endif
