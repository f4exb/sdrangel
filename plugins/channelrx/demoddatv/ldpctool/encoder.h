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
