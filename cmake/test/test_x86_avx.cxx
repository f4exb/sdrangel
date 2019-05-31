#include <signal.h>
#include <stdlib.h>
#include <immintrin.h>

void signalHandler(int signum) {
    exit(signum); // SIGILL = 4
}

int main(int argc, char* argv[])
{
    signal(SIGILL, signalHandler);
	__m256d x = _mm256_setzero_pd();
	x=_mm256_addsub_pd(x,x);
	return 0;
}
