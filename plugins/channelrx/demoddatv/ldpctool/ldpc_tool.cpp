/*
LDPC testbench
Copyright 2018 Ahmet Inan <xdsopl@gmail.com>

Transformed into external decoder for third-party applications
Copyright 2019 <pabr@pabr.org>
*/

#include <stdlib.h>
#ifndef _MSC_VER
#include <unistd.h>
#else
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#include <io.h>
#include <malloc.h>
#endif
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
#include "algorithms.h"
#include "ldpc.h"


#if 0
#include "flooding_decoder.h"
static const int DEFAULT_TRIALS = 50;
#else
#include "layered_decoder.h"
static const int DEFAULT_TRIALS = 25;
#endif
static const int DEFAULT_BATCH_SIZE = 32;

//ldpctool::LDPCInterface *create_ldpc(char *standard, char prefix, int number);

void fail(const char *msg)
{
	std::cerr << "** plugin: " << msg << std::endl;
	exit(1);
}

void fatal(const char *msg)
{
	fprintf(stderr, "** plugin: ");
	perror(msg);
	exit(1);
}

void usage(const char *name, FILE *f, int c, const char *info = NULL)
{
	fprintf(f, "Usage: %s [--standard DVB-S2] --modcod INT [--trials 25] [--batch-size 32] [--shortframes]  < FECFRAMES.int8  > FECFRAMES.int8\n", name);
	if (info)
		fprintf(f, "** Error while processing '%s'\n", info);
	exit(c);
}

int main(int argc, char **argv)
{
	const char *standard = "DVB-S2";
	int modcod = -1;
	int max_trials = DEFAULT_TRIALS;
	int batch_size = DEFAULT_BATCH_SIZE;
	bool shortframes = false;

	for (int i = 1; i < argc; ++i)
	{
		if (!strcmp(argv[i], "--standard") && i + 1 < argc)
			standard = argv[++i];
		else if (!strcmp(argv[i], "--modcod") && i + 1 < argc)
			modcod = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--trials") && i + 1 < argc)
			max_trials = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--batch-size") && i + 1 < argc)
			batch_size = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--shortframes"))
			shortframes = true;
		else if (!strcmp(argv[i], "-h"))
			usage(argv[0], stdout, 0);
		else
			usage(argv[0], stderr, 1, argv[i]);
	}

	if (strcmp(standard, "DVB-S2"))
		fail("Only DVB-S2 is supported.");

	if (modcod < 0 || modcod > 31)
		usage(argv[0], stderr, 1);

	typedef ldpctool::NormalUpdate<ldpctool::simd_type> update_type;
	//typedef SelfCorrectedUpdate<simd_type> update_type;

	//typedef MinSumAlgorithm<simd_type, update_type> algorithm_type;
	//typedef OffsetMinSumAlgorithm<simd_type, update_type, FACTOR> algorithm_type;
	typedef ldpctool::MinSumCAlgorithm<ldpctool::simd_type, update_type, ldpctool::FACTOR> algorithm_type;
	//typedef LogDomainSPA<simd_type, update_type> algorithm_type;
	//typedef LambdaMinAlgorithm<simd_type, update_type, 3> algorithm_type;
	//typedef SumProductAlgorithm<simd_type, update_type> algorithm_type;

	ldpctool::LDPCDecoder<ldpctool::simd_type, algorithm_type> decode;

	// DVB-S2 MODCOD definitions
	static const char *mc_tabnames[2][32] = { // [shortframes][modcod]
											 {// Normal frames
											  0, "B1", "B2", "B3", "B4", "B5", "B6", "B7",
											  "B8", "B9", "B10", "B11", "B5", "B6", "B7", "B9",
											  "B10", "B11", "B6", "B7", "B8", "B9", "B10", "B11",
											  "B7", "B8", "B8", "B10", "B11", 0, 0, 0},
											 {// Short frames
											  0, "C1", "C2", "C3", "C4", "C5", "C6", "C7",
											  "C8", "C9", "C10", 0, "C5", "C6", "C7", "C9",
											  "C10", 0, "C6", "C7", "C8", "C9", "C10", 0,
											  "C7", "C8", "C8", "C10", 0, 0, 0, 0}};

	const char *tabname = mc_tabnames[shortframes][modcod];
	if (!tabname)
		fail("unsupported modcod");

	ldpctool::LDPCInterface *ldpc = ldpctool::create_ldpc((char *)"S2", tabname[0], atoi(tabname + 1));

	if (!ldpc)
	{
		std::cerr << "no such table!" << std::endl;
		return -1;
	}

	const int CODE_LEN = ldpc->code_len();
	const int DATA_LEN = ldpc->data_len();

	decode.init(ldpc);

	int BLOCKS = batch_size;
	ldpctool::code_type *code = new ldpctool::code_type[BLOCKS * CODE_LEN];
	void *aligned_buffer = ldpctool::LDPCUtil::aligned_malloc(sizeof(ldpctool::simd_type), sizeof(ldpctool::simd_type) * CODE_LEN);
	ldpctool::simd_type *simd = reinterpret_cast<ldpctool::simd_type *>(aligned_buffer);

	// Expect LLR values in int8_t format.
	if (sizeof(ldpctool::code_type) != 1)
		fail("Bug: Unsupported code_type");

	int trials_count = 0;
	int max_count = 0;
	int num_decodes = 0;

	while (true)
	{
		ssize_t iosize = BLOCKS * CODE_LEN * sizeof(*code);

		for (ssize_t pos = 0; pos < iosize;)
		{
			int nr = read(0, code + pos, iosize - pos);

			if (!nr)
				exit(0);

			if (nr < 0)
				fatal("read");

			pos += nr;
		}


		for (int j = 0; j < BLOCKS; j += ldpctool::SIMD_WIDTH)
		{
			int blocks = j + ldpctool::SIMD_WIDTH > BLOCKS ? BLOCKS - j : ldpctool::SIMD_WIDTH;

			for (int n = 0; n < blocks; ++n)
				for (int i = 0; i < CODE_LEN; ++i)
					reinterpret_cast<ldpctool::code_type *>(simd + i)[n] = code[(j + n) * CODE_LEN + i];

			int count = decode(simd, simd + DATA_LEN, max_trials, blocks);
			num_decodes++;

			if (count < 0)
			{
				trials_count += max_trials;
				max_count++;
			}
			else
			{
				trials_count += max_trials - count;
			}

			if (num_decodes == 20)
			{
				fprintf(stderr, "ldpc_tool: trials: %d%% max: %d%% at converging to a code word (max trials: %d)\n",
					(trials_count*100)/(num_decodes*max_trials), (max_count*100)/num_decodes, max_trials);
				trials_count = 0;
				max_count = 0;
				num_decodes = 0;
			}

			for (int n = 0; n < blocks; ++n)
				for (int i = 0; i < CODE_LEN; ++i)
					code[(j + n) * CODE_LEN + i] = reinterpret_cast<ldpctool::code_type *>(simd + i)[n];
		}

		for (ssize_t pos = 0; pos < iosize;)
		{
			ssize_t nw = write(1, code + pos, iosize - pos);

			if (!nw)
				exit(0);

			if (nw < 0)
				fatal("write");

			pos += nw;
		}
	} // main loop

	delete ldpc;

	ldpctool::LDPCUtil::aligned_free(aligned_buffer);
	delete[] code;

	return 0;
}
