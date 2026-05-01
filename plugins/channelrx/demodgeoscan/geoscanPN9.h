#ifndef GEOSCAN_PN9_H
#define GEOSCAN_PN9_H

#include <cstdint>
#include <vector>
#include <cstddef>

class Scrambler {
public:
    static std::vector<uint8_t> generatePN9MaskBytes(int byteCount);
    static void descramblePN9(std::vector<uint8_t>& data);
};



#endif