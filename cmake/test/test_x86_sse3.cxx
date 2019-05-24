#include <signal.h>
#include <stdlib.h>
#include <emmintrin.h>
#include <pmmintrin.h>

void signalHandler(int signum) {
    exit(signum); // SIGILL = 4
}

int main(int argc, char* argv[])
{
    signal(SIGILL, signalHandler);
	__m128d x = _mm_setzero_pd();
	x=_mm_addsub_pd(x,x);
	return 0;
}
