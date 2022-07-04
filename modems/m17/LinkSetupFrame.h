// Copyright 2020 modemm17 LLC.

#pragma once

#include <array>
#include <cstdint>
#include <string_view> // Don't have std::span in C++17.
#include <stdexcept>
#include <algorithm>

namespace modemm17
{

struct LinkSetupFrame
{
    using call_t = std::array<char,10>;             // NUL-terminated C-string.
    using encoded_call_t = std::array<uint8_t, 6>;
    using frame_t = std::array<uint8_t, 30>;

    static constexpr encoded_call_t BROADCAST_ADDRESS = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    static constexpr call_t BROADCAST_CALL = {'B', 'R', 'O', 'A', 'D', 'C', 'A', 'S', 'T', 0};

    enum TxType { PACKET, STREAM };
    enum DataType { DT_RESERVED, DATA, VOICE, MIXED };
    enum EncType { NONE, AES, LFSR, ET_RESERVED };

    call_t tocall_ = {0};   // Destination
    call_t mycall_ = {0};   // Source
    TxType  tx_type_ = TxType::STREAM;
    DataType data_type_ = DataType::VOICE;
    EncType encryption_type_ = EncType::NONE;

    /**
     * The callsign is encoded in base-40 starting with the right-most
     * character.  The final value is written out in "big-endian" form, with
     * the most-significant value first.  This leads to 0-padding of shorter
     * callsigns.
     *
     * @param[in] callsign is the callsign to encode.
     * @param[in] strict is a flag (disabled by default) which indicates whether
     *  invalid characters are allowed and assugned a value of 0 or not allowed,
     *  resulting in an exception.
     * @return the encoded callsign as an array of 6 bytes.
     * @throw invalid_argument when strict is true and an invalid callsign (one
     *  containing an unmappable character) is passed.
     */
    static encoded_call_t encode_callsign(call_t callsign, bool strict = false)
    {
        // Encode the characters to base-40 digits.
        uint64_t encoded = 0;

        std::reverse(callsign.begin(), callsign.end());

        for (auto c : callsign)
        {
            encoded *= 40;
            if (c >= 'A' and c <= 'Z')
            {
                encoded += c - 'A' + 1;
            }
            else if (c >= '0' and c <= '9')
            {
                encoded += c - '0' + 27;
            }
            else if (c == '-')
            {
                encoded += 37;
            }
            else if (c == '/')
            {
                encoded += 38;
            }
            else if (c == '.')
            {
                encoded += 39;
            }
            else if (strict)
            {
                throw std::invalid_argument("bad callsign");
            }
        }
        const auto p = reinterpret_cast<uint8_t*>(&encoded);

        encoded_call_t result;
        std::copy(p, p + 6, result.rbegin());

        return result;
    }

    /**
     * Decode a base-40 encoded callsign to its text representation.  This decodes
     * a 6-byte big-endian value into a string of up to 9 characters.
     */
    static call_t decode_callsign(encoded_call_t callsign)
    {
        static const char callsign_map[] = "xABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/.";

        call_t result;

        if (callsign == BROADCAST_ADDRESS)
        {
            result = BROADCAST_CALL;
            return result;
        }

        uint64_t encoded = 0;       // This only works on little endian architectures.
        auto p = reinterpret_cast<uint8_t*>(&encoded);
        std::copy(callsign.rbegin(), callsign.rend(), p);

        // decode each base-40 digit and map them to the appriate character.
        result.fill(0);
        size_t index = 0;
        while (encoded)
        {
            result[index++] = callsign_map[encoded % 40];
            encoded /= 40;
        }

        return result;
    }

    LinkSetupFrame()
    {}

    LinkSetupFrame& myCall(const char*)
    {
        return *this;
    }
};

} // modemm17
