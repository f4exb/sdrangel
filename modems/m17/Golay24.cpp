#include "Util.h"
#include "Golay24.h"

namespace modemm17 {

std::array<Golay24::SyndromeMapEntry, Golay24::LUT_SIZE> Golay24::LUT = Golay24::make_lut();

Golay24::Golay24()
{}

uint32_t Golay24::syndrome(uint32_t codeword)
{
    codeword &= 0xffffffl;

    for (size_t i = 0; i != 12; ++i)
    {
        if (codeword & 1) {
            codeword ^= POLY;
        }

        codeword >>= 1;
    }

    return (codeword << 12);
}

bool Golay24::parity(uint32_t codeword)
{
    return popcount(codeword) & 1;
}

Golay24::SyndromeMapEntry Golay24::makeSyndromeMapEntry(uint64_t val)
{
    return SyndromeMapEntry{uint32_t(val >> 16), uint16_t(val & 0xFFFF)};
}

uint64_t Golay24::makeSME(uint64_t syndrome, uint32_t bits)
{
    return (syndrome << 24) | (bits & 0xFFFFFF);
}

uint32_t Golay24::encode23(uint16_t data)
{
    // data &= 0xfff;
    uint32_t codeword = data;

    for (size_t i = 0; i != 12; ++i)
    {
        if (codeword & 1) {
            codeword ^= POLY;
        }

        codeword >>= 1;
    }

    return codeword | (data << 11);
}

uint32_t Golay24::encode24(uint16_t data)
{
    auto codeword = encode23(data);
    return ((codeword << 1) | parity(codeword));
}

bool Golay24::decode(uint32_t input, uint32_t& output)
{
    auto syndrm = syndrome(input >> 1);

    auto it = std::lower_bound(
        LUT.begin(),
        LUT.end(),
        syndrm,
        [](const SyndromeMapEntry& sme, uint32_t val){
            return (sme.a >> 8) < val;
        }
    );

    if ((it->a >> 8) == syndrm)
    {
        // Build the correction from the compressed entry.
        auto correction = ((((it->a & 0xFF) << 16) | it->b) << 1);
        // Apply the correction to the input.
        output = input ^ correction;
        // Only test parity for 3-bit errors.
        return popcount(syndrm) < 3 || !parity(output);
    }

    return false;
}

std::array<Golay24::SyndromeMapEntry, Golay24::LUT_SIZE> Golay24::make_lut()
{
    constexpr size_t VECLEN=23;
    Golay24_detail::array<uint64_t, LUT_SIZE> result{};

    size_t index = 0;
    result[index++] = makeSME(syndrome(0), 0);

    for (size_t i = 0; i != VECLEN; ++i)
    {
        auto v = (1 << i);
        result[index++] = makeSME(syndrome(v), v);
    }

    for (size_t i = 0; i != VECLEN - 1; ++i)
    {
        for (size_t j = i + 1; j != VECLEN; ++j)
        {
            auto v = (1 << i) | (1 << j);
            result[index++] = makeSME(syndrome(v), v);
        }
    }

    for (size_t i = 0; i != VECLEN - 2; ++i)
    {
        for (size_t j = i + 1; j != VECLEN - 1; ++j)
        {
            for (size_t k = j + 1; k != VECLEN; ++k)
            {
                auto v = (1 << i) | (1 << j) | (1 << k);
                result[index++] = makeSME(syndrome(v), v);
            }
        }
    }

    result = Golay24_detail::sort(result);

    std::array<SyndromeMapEntry, LUT_SIZE> tmp;
    for (size_t i = 0; i != LUT_SIZE; ++i)
    {
        tmp[i] = makeSyndromeMapEntry(result[i]);
    }

    return tmp;
}

} // modemm17
