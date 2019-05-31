#include <signal.h>
#include <stdlib.h>
#include <immintrin.h>

void signalHandler(int signum) {
    exit(signum); // SIGILL = 4
}

int main(int argc, char* argv[])
{
    signal(SIGILL, signalHandler);
	__m256i x = _mm256_setzero_si256();
	x=_mm256_add_epi64 (x,x);
	return 0;
}
