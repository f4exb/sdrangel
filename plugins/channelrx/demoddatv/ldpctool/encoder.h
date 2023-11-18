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
LDPC SISO encoder

Copyright 2018 Ahmet Inan <xdsopl@gmail.com>
*/

#ifndef ENCODER_HH
#define ENCODER_HH

#include "ldpc.h"

namespace ldpctool {

template <typename TYPE>
class LDPCEncoder
{
	LDPCInterface *ldpc;
	int N, K, R;
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
		if (initialized)
			delete ldpc;
		initialized = true;
		ldpc = it->clone();
		N = ldpc->code_len();
		K = ldpc->data_len();
		R = N - K;
	}
	void operator()(TYPE *data, TYPE *parity)
	{
		for (int i = 0; i < R; ++i)
			parity[i] = one();
		ldpc->first_bit();
		for (int j = 0; j < K; ++j) {
			int *acc_pos = ldpc->acc_pos();
			int bit_deg = ldpc->bit_deg();
			for (int n = 0; n < bit_deg; ++n) {
				int i = acc_pos[n];
				parity[i] = sign(parity[i], data[j]);
			}
			ldpc->next_bit();
		}
		for (int i = 1; i < R; ++i)
			parity[i] = sign(parity[i], parity[i-1]);
	}
	~LDPCEncoder()
	{
		if (initialized)
			delete ldpc;
	}
};

} // namespace ldpctool

#endif
