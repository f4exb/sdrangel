/*
LDPC SISO flooding decoder

Copyright 2018 Ahmet Inan <xdsopl@gmail.com>
*/

#ifndef FLOODING_DECODER_HH
#define FLOODING_DECODER_HH

#include <stdlib.h>
#include "exclusive_reduce.h"
#include "ldpc.h"

namespace ldpctool {

template <typename TYPE, typename ALG>
class LDPCDecoder
{
	void *aligned_buffer;
	TYPE *bnl, *bnv, *cnl, *cnv;
	uint8_t *cnc;
	LDPCInterface *ldpc;
	ALG alg;
	int N, K, R, CNL, LT;
	bool initialized;

	void bit_node_init(TYPE *data, TYPE *parity)
	{
		TYPE *bl = bnl;
		for (int i = 0; i < R-1; ++i) {
			bnv[i] = parity[i];
			*bl++ = parity[i];
			*bl++ = parity[i];
		}
		bnv[R-1] = parity[R-1];
		*bl++ = parity[R-1];
		ldpc->first_bit();
		for (int j = 0; j < K; ++j) {
			bnv[j+R] = data[j];
			int bit_deg = ldpc->bit_deg();
			for (int n = 0; n < bit_deg; ++n)
				*bl++ = data[j];
			ldpc->next_bit();
		}
	}
	void check_node_update()
	{
		TYPE *bl = bnl;
		cnv[0] = alg.sign(alg.one(), bnv[0]);
		cnl[0] = *bl++;
		cnc[0] = 1;
		for (int i = 1; i < R; ++i) {
			cnv[i] = alg.sign(alg.sign(alg.one(), bnv[i-1]), bnv[i]);
			cnl[CNL*i] = *bl++;
			cnl[CNL*i+1] = *bl++;
			cnc[i] = 2;
		}
		ldpc->first_bit();
		for (int j = 0; j < K; ++j) {
			int *acc_pos = ldpc->acc_pos();
			int bit_deg = ldpc->bit_deg();
			for (int n = 0; n < bit_deg; ++n) {
				int i = acc_pos[n];
				cnv[i] = alg.sign(cnv[i], bnv[j+R]);
				cnl[CNL*i+cnc[i]++] = *bl++;
			}
			ldpc->next_bit();
		}
		for (int i = 0; i < R; ++i)
			alg.finalp(cnl+CNL*i, cnc[i]);
	}
	void bit_node_update(TYPE *data, TYPE *parity)
	{
		TYPE *bl = bnl;
		bnv[0] = alg.add(parity[0], alg.add(cnl[0], cnl[CNL]));
		alg.update(bl++, alg.add(parity[0], cnl[CNL]));
		alg.update(bl++, alg.add(parity[0], cnl[0]));
		cnc[0] = 1;
		for (int i = 1; i < R-1; ++i) {
			bnv[i] = alg.add(parity[i], alg.add(cnl[CNL*i+1], cnl[CNL*(i+1)]));
			alg.update(bl++, alg.add(parity[i], cnl[CNL*(i+1)]));
			alg.update(bl++, alg.add(parity[i], cnl[CNL*i+1]));
			cnc[i] = 2;
		}
		bnv[R-1] = alg.add(parity[R-1], cnl[CNL*(R-1)+1]);
		alg.update(bl++, parity[R-1]);
		cnc[R-1] = 2;
		ldpc->first_bit();
		for (int j = 0; j < K; ++j) {
			int *acc_pos = ldpc->acc_pos();
			int bit_deg = ldpc->bit_deg();
			TYPE inp[bit_deg];
			for (int n = 0; n < bit_deg; ++n) {
				int i = acc_pos[n];
				inp[n] = cnl[CNL*i+cnc[i]++];
			}
			TYPE out[bit_deg];
			CODE::exclusive_reduce(inp, out, bit_deg, alg.add);
			bnv[j+R] = alg.add(data[j], alg.add(out[0], inp[0]));
			for (int n = 0; n < bit_deg; ++n)
				alg.update(bl++, alg.add(data[j], out[n]));
			ldpc->next_bit();
		}
	}
	bool hard_decision(int blocks)
	{
		for (int i = 0; i < R; ++i)
			if (alg.bad(cnv[i], blocks))
				return true;
		return false;
	}
	void update_user(TYPE *data, TYPE *parity)
	{
		for (int i = 0; i < R; ++i)
			parity[i] = bnv[i];
		for (int i = 0; i < K; ++i)
			data[i] = bnv[i+R];
	}
public:
	LDPCDecoder() : initialized(false)
	{
	}
	void init(LDPCInterface *it)
	{
		if (initialized) {
			free(aligned_buffer);
			delete[] cnc;
			delete ldpc;
		}
		initialized = true;
		ldpc = it->clone();
		N = ldpc->code_len();
		K = ldpc->data_len();
		R = N - K;
		CNL = ldpc->links_max_cn();
		LT = ldpc->links_total();
		int num = LT + N + R * CNL + R;
		aligned_buffer = aligned_alloc(sizeof(TYPE), sizeof(TYPE) * num);
		TYPE *ptr = reinterpret_cast<TYPE *>(aligned_buffer);
		bnl = ptr; ptr += LT;
		bnv = ptr; ptr += N;
		cnl = ptr; ptr += R * CNL;
		cnv = ptr; ptr += R;
		cnc = new uint8_t[R];
	}
	int operator()(TYPE *data, TYPE *parity, int trials = 50, int blocks = 1)
	{
		bit_node_init(data, parity);
		check_node_update();
		if (!hard_decision(blocks))
			return trials;
		--trials;
		bit_node_update(data, parity);
		check_node_update();
		while (hard_decision(blocks) && --trials >= 0) {
			bit_node_update(data, parity);
			check_node_update();
		}
		update_user(data, parity);
		return trials;
	}
	~LDPCDecoder()
	{
		if (initialized) {
			free(aligned_buffer);
			delete[] cnc;
			delete ldpc;
		}
	}
};

} // namespace ldpctool

#endif
