#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <immintrin.h>

void signalHandler(int signum) {
    exit(signum); // SIGILL = 4
}

int main(int argc, char* argv[])
{
    signal(SIGILL, signalHandler);
    uint64_t x[8] = {0};
    __m512i y = _mm512_loadu_si512((__m512i*)x);
    return 0;
}
