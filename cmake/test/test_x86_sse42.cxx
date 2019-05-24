#include <signal.h>
#include <stdlib.h>
#include <nmmintrin.h>

void signalHandler(int signum) {
    exit(signum); // SIGILL = 4
}

int main(int argc, char* argv[])
{
    signal(SIGILL, signalHandler);
	unsigned int x=32;
	x=_mm_crc32_u8(x,4);
	return 0;
}
