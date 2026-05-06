#include "geoscanPN9.h"

// Генерирует byteCount байт маски для скремблирования используя полином PN9
std::vector<uint8_t> Scrambler::generatePN9MaskBytes(int byteCount) {
    std::vector<uint8_t> maskBytes(byteCount, 0);

    uint16_t buffer = 0x1FF;

    for (int i = 0; i < byteCount; i++) {
        for (int j = 0; j < 8; j++) {
            uint8_t maskBit = buffer & 0x01;

            maskBytes[i] |= maskBit << j;

            uint8_t xorState = (buffer & 0x01) ^ ((buffer >> 5) & 0x01);

            buffer >>= 1;

            if (xorState) {
                buffer |= 0x0100;
            }
        }
    }

    return maskBytes;
}

// Дескремблирует пакет данных используя PN9 полином. Делается все in-place
void Scrambler::descramblePN9(std::vector<uint8_t>& packet) {
    std::vector<uint8_t> mask = generatePN9MaskBytes(packet.size());
    int packet_size = packet.size();

    for (int i = 0; i < packet_size; i++) {
        packet[i] ^= mask[i];
    }
}