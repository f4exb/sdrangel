// Copyright 2020 Mobilinkd LLC.

#pragma once

#include <cstdint>
#include <array>
#include <cstddef>

namespace modemm17
{

struct CRC16
{
    static const uint16_t MASK = 0xFFFF;
    static const uint16_t LSB = 0x0001;
    static const uint16_t MSB = 0x8000;


    CRC16(uint16_t poly = 0x5935, uint16_t init = 0xFFFF) :
        poly_(poly),
        init_(init)
    {
        reg_ = init_;
    }

    void reset()
    {
        reg_ = init_;

        for (size_t i = 0; i != 16; ++i)
        {
            auto bit = reg_ & LSB;
            if (bit) reg_ ^= poly_;
            reg_ >>= 1;
            if (bit) reg_ |= MSB;
        }

        reg_ &= MASK;
    }

    void operator()(uint8_t byte)
    {
        reg_ = crc(byte, reg_);
    }

    uint16_t crc(uint8_t byte, uint16_t reg)
    {
        for (size_t i = 0; i != 8; ++i)
        {
            auto msb = reg & MSB;
            reg = ((reg << 1) & MASK) | ((byte >> (7 - i)) & LSB);
            if (msb) reg ^= poly_;
        }
        return reg & MASK;
    }

    uint16_t get()
    {
        auto reg = reg_;
        for (size_t i = 0; i != 16; ++i)
        {
            auto msb = reg & MSB;
            reg = ((reg << 1) & MASK);
            if (msb) reg ^= poly_;
        }
        return reg;
    }

    std::array<uint8_t, 2> get_bytes()
    {
        auto crc = get();
        std::array<uint8_t, 2> result{uint8_t((crc >> 8) & 0xFF), uint8_t(crc & 0xFF)};
        return result;
    }

private:
    uint16_t poly_;
    uint16_t init_;
    uint16_t reg_;
};

} // modemm17
