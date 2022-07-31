// Copyright 2020 Mobilinkd LLC.

#pragma once

#include <array>
#include <cstdint>
#include <cmath>
#include <stdexcept>
#include <algorithm>

#include "export.h"

namespace modemm17
{

struct MODEMM17_API LinkSetupFrame
{
    using call_t = std::array<char,10>;             // NUL-terminated C-string.
    using encoded_call_t = std::array<uint8_t, 6>;
    using gnss_t = std::array<uint8_t, 14>;
    using frame_t = std::array<uint8_t, 30>;

    static const encoded_call_t BROADCAST_ADDRESS;
    static const call_t BROADCAST_CALL;

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
            if ((c >= 'A') && (c <= 'Z'))
            {
                encoded += c - 'A' + 1;
            }
            else if ((c >= '0') && (c <= '9'))
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

    static gnss_t encode_gnss(float lat, float lon, float alt)
    {
        gnss_t result;
        result.fill(0);
        double lat_int, lat_frac;
        double lon_int, lon_frac;
        uint16_t lat_dec, lon_dec;

        lat_frac = modf(lat, &lat_int);
        lon_frac = modf(lon, &lon_int);

        bool north = lat_int >= 0;
        bool east = lon_int >= 0;

        result[2] = (int) std::abs(lat_int);
        lat_dec = std::abs(lat_frac) * 65536.0f;
        result[3] = lat_dec >> 8;
        result[4] = lat_dec & 0xFF;
        result[5] = (int) std::abs(lon_int);
        lon_dec = std::abs(lon_frac) * 65536.0f;
        result[6] = lon_dec >> 8;
        result[7] = lon_dec & 0xFF;
        result[8] = (north ? 0 : 1) | ((east ? 0 : 1)<<1) | (1<<2);

        uint16_t alt_enc = (alt * 3.28084f) + 1500;
        result[9] = alt_enc >> 8;
        result[10] = alt_enc & 0xFF;

        return result;
    }

    static void decode_gnss(const gnss_t& gnss_enc, float& lat, float& lon, float& alt)
    {
        bool north = (gnss_enc[8] & 1) != 0;
        bool east = (gnss_enc[8] & 2) != 0;
        uint32_t lat_int = gnss_enc[2];
        uint16_t lat_frac = (gnss_enc[3] << 8) + gnss_enc[4];
        uint32_t lon_int = gnss_enc[5];
        uint16_t lon_frac = (gnss_enc[6] << 8) + gnss_enc[7];
        lat = lat_int + (lat_frac / 65536.0f);
        lat = north ? lat : -lat;
        lon = lon_int + (lon_frac / 65536.0f);
        lat = east ? lon : -lon;
        uint16_t alt_enc = (gnss_enc[9] << 8) + gnss_enc[10];
        alt = (alt_enc - 1500) / 3.28084f;
    }

    LinkSetupFrame()
    {}

    LinkSetupFrame& myCall(const char*)
    {
        return *this;
    }
};

} // modemm17
