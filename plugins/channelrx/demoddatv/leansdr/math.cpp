#include "math.h"

namespace leansdr
{

int hamming_weight(uint8_t x)
{
    static const int lut[16] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};
    return lut[x & 15] + lut[x >> 4];
}

int hamming_weight(uint16_t x)
{
    return hamming_weight((uint8_t)x) + hamming_weight((uint8_t)(x >> 8));
}

int hamming_weight(uint32_t x)
{
    return hamming_weight((uint16_t)x) + hamming_weight((uint16_t)(x >> 16));
}

int hamming_weight(uint64_t x)
{
    return hamming_weight((uint32_t)x) + hamming_weight((uint32_t)(x >> 32));
}

unsigned char parity(uint8_t x)
{
    x ^= x >> 4;
    return (0x6996 >> (x & 15)) & 1; // 16-entry look-up table
}

unsigned char parity(uint16_t x)
{
    return parity((uint8_t)(x ^ (x >> 8)));
}

unsigned char parity(uint32_t x)
{
    return parity((uint16_t)(x ^ (x >> 16)));
}

unsigned char parity(uint64_t x)
{
    return parity((uint32_t)(x ^ (x >> 32)));
}

int log2i(uint64_t x)
{
    int n = -1;
    for (; x; ++n, x >>= 1);
    return n;
}

} // leansdr
