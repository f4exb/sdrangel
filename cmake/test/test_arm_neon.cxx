#include <stdint.h>
#include <arm_neon.h>
#include <stdlib.h>
#include <signal.h>

void signalHandler(int signum) {
    exit(signum); // SIGILL = 4
}

int main(int argc, char* argv[])
{
    signal(SIGILL, signalHandler);
	uint32x4_t x={0};
	x=veorq_u32(x,x);
	return 0;
}
