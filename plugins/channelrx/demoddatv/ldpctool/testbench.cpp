/*
LDPC testbench

Copyright 2018 Ahmet Inan <xdsopl@gmail.com>
*/

#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <random>
#include <cmath>
#include <cassert>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <functional>
#include "testbench.h"
#include "encoder.h"
#include "algorithms.h"
#include "interleaver.h"
#include "modulation.h"

#if 0
#include "flooding_decoder.h"
static const int TRIALS = 50;
#else
#include "layered_decoder.h"
static const int TRIALS = 25;
#endif

namespace ldpctool {

LDPCInterface *create_ldpc(char *standard, char prefix, int number);
Interleaver<code_type> *create_interleaver(char *modulation, char *standard, char prefix, int number);
ModulationInterface<complex_type, code_type> *create_modulation(char *name, int len);

int main(int argc, char **argv)
{
	if (argc != 6)
		return -1;

	typedef NormalUpdate<simd_type> update_type;
	//typedef SelfCorrectedUpdate<simd_type> update_type;

	//typedef MinSumAlgorithm<simd_type, update_type> algorithm_type;
	typedef OffsetMinSumAlgorithm<simd_type, update_type, FACTOR> algorithm_type;
	//typedef MinSumCAlgorithm<simd_type, update_type, FACTOR> algorithm_type;
	//typedef LogDomainSPA<simd_type, update_type> algorithm_type;
	//typedef LambdaMinAlgorithm<simd_type, update_type, 3> algorithm_type;
	//typedef SumProductAlgorithm<simd_type, update_type> algorithm_type;

	LDPCEncoder<code_type> encode;
	LDPCDecoder<simd_type, algorithm_type> decode;

	LDPCInterface *ldpc = create_ldpc(argv[2], argv[3][0], atoi(argv[3]+1));
	if (!ldpc) {
		std::cerr << "no such table!" << std::endl;
		return -1;
	}
	const int CODE_LEN = ldpc->code_len();
	const int DATA_LEN = ldpc->data_len();
	std::cerr << "testing LDPC(" << CODE_LEN << ", " << DATA_LEN << ") code." << std::endl;

	encode.init(ldpc);
	decode.init(ldpc);

	ModulationInterface<complex_type, code_type> *mod = create_modulation(argv[4], CODE_LEN);
	if (!mod) {
		std::cerr << "no such modulation!" << std::endl;
		return -1;
	}
	const int MOD_BITS = mod->bits();
	assert(CODE_LEN % MOD_BITS == 0);
	const int SYMBOLS = CODE_LEN / MOD_BITS;

	Interleaver<code_type> *itl = create_interleaver(argv[4], argv[2], argv[3][0], atoi(argv[3]+1));
	assert(itl);

	value_type SNR = atof(argv[1]);
	//value_type mean_signal = 0;
	value_type sigma_signal = 1;
	value_type mean_noise = 0;
	value_type sigma_noise = std::sqrt(sigma_signal * sigma_signal / (2 * std::pow(10, SNR / 10)));
	std::cerr << SNR << " Es/N0 => AWGN with standard deviation of " << sigma_noise << " and mean " << mean_noise << std::endl;

	value_type code_rate = (value_type)DATA_LEN / (value_type)CODE_LEN;
	value_type spectral_efficiency = code_rate * MOD_BITS;
	value_type EbN0 = 10 * std::log10(sigma_signal * sigma_signal / (spectral_efficiency * 2 * sigma_noise * sigma_noise));
	std::cerr << EbN0 << " Eb/N0, using spectral efficiency of " << spectral_efficiency << " from " << code_rate << " code rate and " << MOD_BITS << " bits per symbol." << std::endl;

	std::random_device rd;
	std::default_random_engine generator(rd());
	typedef std::uniform_int_distribution<int> uniform;
	typedef std::normal_distribution<value_type> normal;
	auto data = std::bind(uniform(0, 1), generator);
	auto awgn = std::bind(normal(mean_noise, sigma_noise), generator);

	int BLOCKS = atoi(argv[5]);
	if (BLOCKS < 1)
		return -1;
	void *aligned_buffer = aligned_alloc(sizeof(simd_type), sizeof(simd_type) * CODE_LEN);
	simd_type *simd = reinterpret_cast<simd_type *>(aligned_buffer);
	code_type *code = new code_type[BLOCKS * CODE_LEN];
	code_type *orig = new code_type[BLOCKS * CODE_LEN];
	code_type *noisy = new code_type[BLOCKS * CODE_LEN];
	complex_type *symb = new complex_type[BLOCKS * SYMBOLS];

	for (int j = 0; j < BLOCKS; ++j)
		for (int i = 0; i < DATA_LEN; ++i)
			code[j * CODE_LEN + i] = 1 - 2 * data();

	for (int j = 0; j < BLOCKS; ++j)
		encode(code + j * CODE_LEN, code + j * CODE_LEN + DATA_LEN);

	for (int i = 0; i < BLOCKS * CODE_LEN; ++i)
		orig[i] = code[i];

	for (int i = 0; i < BLOCKS; ++i)
		itl->fwd(code + i * CODE_LEN);

	for (int j = 0; j < BLOCKS; ++j)
		mod->mapN(symb + j * SYMBOLS, code + j * CODE_LEN);

	for (int i = 0; i < BLOCKS * SYMBOLS; ++i)
		symb[i] += complex_type(awgn(), awgn());

	if (1) {
		code_type tmp[MOD_BITS];
		value_type sp = 0, np = 0;
		for (int i = 0; i < SYMBOLS; ++i) {
			mod->hard(tmp, symb[i]);
			complex_type s = mod->map(tmp);
			complex_type e = symb[i] - s;
			sp += std::norm(s);
			np += std::norm(e);
		}
		value_type snr = 10 * std::log10(sp / np);
		sigma_signal = std::sqrt(sp / SYMBOLS);
		sigma_noise = std::sqrt(np / (2 * sp));
		std::cerr << snr << " Es/N0, stddev " << sigma_noise << " of noise and " << sigma_signal << " of signal estimated via hard decision." << std::endl;
	}

	// $LLR=log(\frac{p(x=+1|y)}{p(x=-1|y)})$
	// $p(x|\mu,\sigma)=\frac{1}{\sqrt{2\pi}\sigma}}e^{-\frac{(x-\mu)^2}{2\sigma^2}}$
	value_type precision = FACTOR / (sigma_noise * sigma_noise);
	for (int j = 0; j < BLOCKS; ++j)
		mod->softN(code + j * CODE_LEN, symb + j * SYMBOLS, precision);

	for (int i = 0; i < BLOCKS; ++i)
		itl->bwd(code + i * CODE_LEN);

	for (int i = 0; i < BLOCKS * CODE_LEN; ++i)
		noisy[i] = code[i];

	for (int i = 0; i < BLOCKS * CODE_LEN; ++i)
		assert(!std::isnan(code[i]));

	int iterations = 0;
	int num_decodes = 0;
	auto start = std::chrono::system_clock::now();
	for (int j = 0; j < BLOCKS; j += SIMD_WIDTH) {
		int blocks = j + SIMD_WIDTH > BLOCKS ? BLOCKS - j : SIMD_WIDTH;
		for (int n = 0; n < blocks; ++n)
			for (int i = 0; i < CODE_LEN; ++i)
				reinterpret_cast<code_type *>(simd+i)[n] = code[(j+n)*CODE_LEN+i];
		int trials = TRIALS;
		int count = decode(simd, simd + DATA_LEN, trials, blocks);
		++num_decodes;
		for (int n = 0; n < blocks; ++n)
			for (int i = 0; i < CODE_LEN; ++i)
				code[(j+n)*CODE_LEN+i] = reinterpret_cast<code_type *>(simd+i)[n];
		if (count < 0) {
			iterations += blocks * trials;
			std::cerr << "decoder failed at converging to a code word!" << std::endl;
		} else {
			iterations += blocks * (trials - count);
			std::cerr << trials - count << " iterations were needed." << std::endl;
		}
	}
	auto end = std::chrono::system_clock::now();
	auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
	int kbs = (BLOCKS * DATA_LEN + msec.count() / 2) / msec.count();
	std::cerr << kbs << " kilobit per second." << std::endl;
	float avg_iter = (float)iterations / (float)BLOCKS;
	std::cerr << avg_iter << " average iterations per block." << std::endl;
	float avg_msec = (float)msec.count() / (float)num_decodes;
	std::cerr << avg_msec << " average milliseconds per decode." << std::endl;

	for (int i = 0; i < BLOCKS * CODE_LEN; ++i)
		assert(!std::isnan(code[i]));

	int awgn_errors = 0;
	for (int i = 0; i < BLOCKS * CODE_LEN; ++i)
		awgn_errors += noisy[i] * orig[i] < 0;
	int quantization_erasures = 0;
	for (int i = 0; i < BLOCKS * CODE_LEN; ++i)
		quantization_erasures += !noisy[i];
	int uncorrected_errors = 0;
	for (int i = 0; i < BLOCKS * CODE_LEN; ++i)
		uncorrected_errors += code[i] * orig[i] <= 0;
	int decoder_errors = 0;
	for (int i = 0; i < BLOCKS * CODE_LEN; ++i)
		decoder_errors += code[i] * orig[i] <= 0 && orig[i] * noisy[i] > 0;
	float bit_error_rate = (float)uncorrected_errors / (float)(BLOCKS * CODE_LEN);

	if (1) {
		for (int i = 0; i < CODE_LEN; ++i)
			code[i] = code[i] < 0 ? -1 : 1;
		itl->fwd(code);
		value_type sp = 0, np = 0;
		for (int i = 0; i < SYMBOLS; ++i) {
			complex_type s = mod->map(code + i * MOD_BITS);
			complex_type e = symb[i] - s;
			sp += std::norm(s);
			np += std::norm(e);
		}
		value_type snr = 10 * std::log10(sp / np);
		sigma_signal = std::sqrt(sp / SYMBOLS);
		sigma_noise = std::sqrt(np / (2 * sp));
		std::cerr << snr << " Es/N0, stddev " << sigma_noise << " of noise and " << sigma_signal << " of signal estimated from corrected symbols." << std::endl;
	}

	std::cerr << awgn_errors << " errors caused by AWGN." << std::endl;
	std::cerr << quantization_erasures << " erasures caused by quantization." << std::endl;
	std::cerr << decoder_errors << " errors caused by decoder." << std::endl;
	std::cerr << uncorrected_errors << " errors uncorrected." << std::endl;
	std::cerr << bit_error_rate << " bit error rate." << std::endl;

	if (0) {
		std::cout << SNR << " " << bit_error_rate << " " << avg_iter << " " << EbN0 << std::endl;
	}

	delete ldpc;
	delete mod;
	delete itl;

	free(aligned_buffer);
	delete[] code;
	delete[] orig;
	delete[] noisy;
	delete[] symb;

	return 0;
}

} // namespace ldpctool
