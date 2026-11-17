///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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

#ifndef MODEMRADIOPAGING_RADIOPAGINGPOCSAG_H
#define MODEMRADIOPAGING_RADIOPAGINGPOCSAG_H

#include <cstdint>
#include <string>
#include <vector>

#include "export.h"
#include "radiopagingcharset.h"

// POCSAG transmission format: encoder and message interpretation.
//
// A transmission is a preamble of 576 alternating bits followed by batches. A batch is
// a sync codeword and 16 codewords arranged as 8 frames of 2. A pager listens to the
// frame given by the 3 least significant bits of its 21-bit address. A codeword with
// bit 31 clear is an address (18 address bits, 2 function bits) or the idle codeword,
// one with bit 31 set carries 20 message bits. Message codewords follow the address
// and may continue across batches (the sync codeword is simply skipped).
//
// Message bits are the bit stream of the message codewords, first transmitted bit
// first. Numeric messages are 4-bit characters and alphanumeric messages 7-bit
// characters, both sent least significant bit first.
class MODEMRADIOPAGING_API RadioPagingPocsag
{
public:
    static const uint32_t m_syncWord = 0x7cd215d8;
    static const uint32_t m_idleWord = 0x7a89c197;
    static const int m_preambleBits = 576;
    static const int m_codewordsPerBatch = 16;
    static const int m_framesPerBatch = 8;
    static const uint32_t m_maxAddress = 0x1fffff;

    enum MessageType {
        Tone,           //!< Address only
        Numeric,
        Alphanumeric
    };

    struct TxMessage
    {
        uint32_t m_address = 0;     //!< 21-bit address
        int m_function = 0;         //!< 0-3, sub address
        MessageType m_type = Alphanumeric;
        std::string m_text;         //!< Numeric: 0-9 R U space - ] [  Alphanumeric: 7-bit characters
    };

    struct EncodeOptions
    {
        bool m_preamble = true;     //!< Start with the 576 bit preamble
        int m_padding = 0;          //!< 7-bit character used to pad alphanumeric messages, 0 = NUL
    };

    struct EncodeResult
    {
        bool m_ok = false;
        std::string m_error;
        std::vector<uint32_t> m_codewords;  //!< Sync words and codewords of all batches
        std::vector<uint8_t> m_bits;        //!< Preamble (if any) and codewords, one bit per element, in transmission order
        //!< Where each message went, in the order of the messages
        struct Placement
        {
            int m_batch = 0;                //!< Batch of its address codeword, from 0 in this result
            int m_lastBatch = 0;            //!< Batch of its last codeword
            int m_frame = 0;                //!< Frame of the batch: the 3 LSBs of the address
            int m_codewords = 0;            //!< Address and message codewords
        };
        std::vector<Placement> m_placements;
        int m_batches = 0;                  //!< Batches of the result
    };

    struct Interpretation
    {
        std::string m_type;         //!< alphanumeric, numeric or tone
        std::string m_text;
        int m_score = 0;            //!< 0-100, how well the content fits the type; a long numeric text scores less, down to below 0
        std::string m_altType;      //!< The other candidate, empty if none
        std::string m_altText;
        int m_altScore = 0;
    };

    //!< Build the transmission for messages in the given order. Each address is placed in
    //!< its frame, starting a new batch when the frame has already passed. An idle codeword
    //!< separates the end of a message from the address of the next message, and the last
    //!< codeword of the transmission is an idle codeword
    static EncodeResult encode(const std::vector<TxMessage>& messages, const EncodeOptions& options);
    static EncodeResult encode(const std::vector<TxMessage>& messages);
    //!< The message bits of a numeric or alphanumeric message, padded to whole codewords: 20 a codeword, first
    //!< transmitted bit first. False and an error if a character is not allowed or the text is empty
    static bool messageBits(const TxMessage& message, const EncodeOptions& options, std::vector<uint8_t>& bits, std::string& error);

    //!< Message bits as stored in a decoded message: packed 8 per byte, first bit in the
    //!< most significant bit of byte 0
    static std::string decodeNumeric(const std::vector<uint8_t>& packed, int nbBits);
    //!< The 7-bit characters, without the fill of the last codeword: the last character if its code is one of fill,
    //!< and the one before if it is the same code
    static std::string decodeAlphanumeric(const std::vector<uint8_t>& packed, int nbBits, const std::vector<uint8_t>& fill = fillMinimal());
    static int scoreNumeric(const std::string& text);
    static int scoreAlphanumeric(const std::string& text);
    //!< Numeric pages longer than this many characters lean heavily towards alphanumeric
    static const int m_longNumericChars = 15;
    //!< Taken off the numeric score (0 to 100) for each character above m_longNumericChars
    static int longNumericPenalty(const std::string& numeric);
    //!< Decide between numeric and alphanumeric by content. No bits means tone only. The alphanumeric text is the
    //!< glyphs of the codepage (RadioPagingCharset::decode), the 7-bit characters themselves without one
    static Interpretation interpret(const std::vector<uint8_t>& packed, int nbBits, const RadioPagingCharset::Codepage *codepage = nullptr,
        const std::vector<uint8_t>& fill = fillMinimal());

    //!< Fill codes at the end of an alphanumeric message: NUL (the fill of the encoder); NUL, ETX and EOT
    static std::vector<uint8_t> fillMinimal() { return {0x00}; }
    static std::vector<uint8_t> fillConservative() { return {0x00, 0x03, 0x04}; }

    //!< Replace characters below 0x20 and 0x7f by <0xNN>
    static std::string escapeControl(const std::string& text);
    //!< Address of the message records: 7 digits and the function letter A-D (address 1234567 function 2:
    //!< "1234567C"), the same in RX, TX and the message center
    static std::string addressText(uint32_t address, int function);
    //!< What an address tells (ITU-R M.584-2): the frame of the batch its address codeword goes in (the 3 LSBs,
    //!< 1.2), the function (one identity is 4 addresses, 1.3.2) and the rule for the other addresses of the pager
    static std::string addressDescription(uint32_t address, int function);
    static const char m_numericChars[17];   //!< Index = 4-bit value
};

#endif // MODEMRADIOPAGING_RADIOPAGINGPOCSAG_H
