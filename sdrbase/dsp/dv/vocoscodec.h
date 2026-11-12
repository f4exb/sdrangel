///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_DSP_DV_VOCOSCODEC_H
#define INCLUDE_DSP_DV_VOCOSCODEC_H

#include <cstdint>
#include <string>

#include "export.h"

/**
 * Angel61's V1 codec (ANGEL61.md 3.1.1): the EnCodec 24 kHz encoder with the Vocos decoder, through
 * the `vocoscodec` shared library (its vocoscodec/c_api.h), loaded on first use. When the library is
 * missing isAvailable() is false, getError() says why and V1 voice is unavailable while everything
 * else works.
 *
 * 24 kHz mono, frames of 320 samples (13.33 ms); a packet is 1 to 16 codebook indices of 10 bits, MSB
 * first, coarse codebook first (750 bit/s a codebook), so a packet truncated to n x 10 bits is the
 * packet at n codebooks (relays, 2.8).
 *
 * **Model identity**: family V1 is exactly the three model files whose SHA-256 are below (vocoscodec's
 * model/SHA256SUMS). An encoder or decoder refuses to open on any other set, with the reason: a
 * different model set is a new family, never a silent substitution.
 */
class SDRBASE_API VocosCodec
{
public:
    static constexpr int SampleRate = 24000;
    static constexpr int SamplesPerFrame = 320;
    static constexpr int MaxCodebooks = 16;
    static constexpr int BitsPerCodebook = 10;
    static constexpr int MaxPacketBytes = 20;
    static constexpr int AbiVersion = 1;
    static constexpr int DefaultLookahead = 3;     //!< frames the decoder looks ahead (60 ms of its delay)
    static const char *const ModelFiles[3];
    static const char *const ModelSha256[3];

    //! Load the library if not yet tried. False when it is missing or not the expected interface.
    static bool isAvailable();
    //! Why the library could not be used (empty when it can)
    static std::string getError();
    static std::string getVersion();

    // Packets, from the normative layout: no library needed (a relay truncates without one)
    //! Bytes of a packet of `codebooks` (1..16): ceil(10 codebooks / 8); 0 otherwise
    static int packetBytes(int codebooks);
    //! Codebooks of a packet of `bytes`, 0 if no count gives that size
    static int packetCodebooks(int bytes);
    //! Truncate a packet in place to fewer codebooks (the trailing bits zeroed). The new size, 0 if invalid.
    static int truncate(uint8_t *packet, int bytes, int codebooks);

    /**
     * Where the models are looked for when nothing is set: VOCOSCODEC_MODEL_DIR in the environment,
     * then `vocoscodec` beside the executable (Windows) or `../share/sdrangel/vocoscodec` (Linux), then
     * the directory the library was built with. Empty when none has the three files.
     */
    static std::string defaultModelDirectory();
    //! True when the three model files are in the directory
    static bool hasModels(const std::string& directory);
    /**
     * True when the directory's three files are V1's (their SHA-256), else false with the reason:
     * the first file missing or different. The answer is kept per directory and file size and time.
     */
    static bool verifyModels(const std::string& directory, std::string& error);

    class SDRBASE_API Encoder
    {
    public:
        Encoder();
        ~Encoder();
        Encoder(const Encoder&) = delete;
        Encoder& operator=(const Encoder&) = delete;
        //! Create for a codebook count (1..16) with the models of a directory (empty: the default)
        bool open(int codebooks, const std::string& modelDirectory = std::string());
        void close();
        bool isOpen() const { return m_encoder != nullptr; }
        //! 320 samples -> a packet of packetBytes(codebooks). Returns its size, 0 on failure.
        int encode(const int16_t *pcm, uint8_t *packet, int capacity);
        //! Key down: the next transmission starts afresh
        void reset();
        int getCodebooks() const { return m_codebooks; }
        const std::string& getError() const { return m_error; }

    private:
        void *m_encoder;
        int m_codebooks;
        std::string m_error;
    };

    class SDRBASE_API Decoder
    {
    public:
        Decoder();
        ~Decoder();
        Decoder(const Decoder&) = delete;
        Decoder& operator=(const Decoder&) = delete;
        /**
         * Create with the models of a directory (empty: the default). lookahead: frames the decoder
         * may look ahead, 0..27 (-1: the library's default, 3). silence: long losses fade to silence
         * rather than comfort noise.
         */
        bool open(const std::string& modelDirectory = std::string(), int lookahead = -1, bool silence = false);
        void close();
        bool isOpen() const { return m_decoder != nullptr; }
        //! Output sample n is input sample n - delay (480 + 320 lookahead)
        int delaySamples() const;
        //! A packet (its size gives its codebook count) -> 320 samples. The sample count, 0 on failure.
        int decode(const uint8_t *packet, int packetBytes, int16_t *pcm, int capacity);
        /**
         * `count` consecutive packets (a radio frame's) -> 320 samples each; a packet of size 0 was
         * lost and is concealed in its place. Frames early in the group use the rest as lookahead:
         * better quality than one packet a call, and less computation.
         */
        int decodePackets(const uint8_t *const *packets, const int *packetBytes, int count, int16_t *pcm, int capacity);
        //! Conceal one missing packet: 320 samples, time advanced by a frame
        int decodeLost(int16_t *pcm, int capacity);
        //! End of a transmission: the last delaySamples() of audio, then reset. The sample count.
        int flush(int16_t *pcm, int capacity);
        //! True once concealment has faded fully to comfort noise (or silence)
        bool isComfortNoise() const;
        //! A new transmission
        void reset();
        const std::string& getError() const { return m_error; }

    private:
        void *m_decoder;
        std::string m_error;
    };

private:
    struct Library;
    static Library *library();
    //! The directory to use and whether its models are V1's (error says why not)
    static bool modelsFor(const std::string& requested, std::string& directory, std::string& error);
};

#endif // INCLUDE_DSP_DV_VOCOSCODEC_H
