#include <signal.h>
#include <stdlib.h>
#include <emmintrin.h>
#include <tmmintrin.h>

void signalHandler(int signum) {
    exit(signum); // SIGILL = 4
}

int main(int argc, char* argv[])
{
    signal(SIGILL, signalHandler);
	__m128i x = _mm_setzero_si128();
	x=_mm_alignr_epi8(x,x,2);
	return 0;
}
