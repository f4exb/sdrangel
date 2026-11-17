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

#ifndef MODEMRADIOPAGING_RADIOPAGINGGSC_H
#define MODEMRADIOPAGING_RADIOPAGINGGSC_H

#include <cstdint>
#include <string>
#include <vector>

#include "export.h"
#include "radiopagingcharset.h"

// Sequential Golay paging format: codes, encoder and message interpretation.
//
// The signal is 600 baud two level FSK, a binary 1 is a positive deviation. Every bit
// is sent twice, so a Golay(23,12) codeword takes 46 symbols (bit 0 first, data in
// bits 11-0 and check bits in bits 22-12). A comma is a run of alternating bits.
//
// Transmission (battery saver mode), see ITU-R Rep. 900-2 Annex I 2 and the Motorola GSC
// manual, Figure 6-2 (batch and extended batch operation):
//   comma of 28 bits, 18 codewords of the preamble value
//   comma of 28 bits, start code, 1 bit, complement of the start code
//   per message: comma of 28 bits, word 1, 1 bit, word 2, then
//     tone only      nothing, the next address follows at once
//     voice          comma of 28 bits, activation code, 1 bit, complement of the activation
//                    code; the alert time (2 s), the voice message and the activation
//                    code again as the turn-off word follow (Motorola Figure 6-1)
//     data           one or more blocks of 1 + 120 bits
// The 10 preamble values are selected by (index digit + group digit 0) mod 10. In batch
// mode the preamble codewords are complemented. Addresses, data blocks and start codes are
// all 121 bits, the slots of a batch. A batch holds 16 slots after its start code, unused
// slots are filled with commas of 121 bits. A message started in a batch runs on to its
// end; the next address then extends the batch with the start code as the next slot,
// without a new preamble (Motorola extended batch mode; Rep. 900-2 Annex I 2.2 asks for a
// new preamble instead, the decoder accepts both). Tone only addresses interleaved into
// the alert time of a voice page (Motorola GSC manual, Figure 6-1) are not sent.
//
// Word 1 selects two digits of the address (the table has 50 entries, the second word
// carries a +50 offset for groups 50-99), word 2 the last three digits. The address
// number 1-4, also called the function A-D, is the combination of the two address words:
//   1 (A)  word 1 and word 2 as encoded
//   2 (B)  word 2 complemented
//   3 (C)  word 1 complemented
//   4 (D)  word 1 and word 2 complemented
// An address (capcode) is written as 6 digits: index, group (2 digits) and 3 address
// digits; without a preamble (non battery saver) the index is N. The address number is
// written after it, e.g. 2197892 or N197892.
//
// A data block is 8 BCH(15,7) codewords of 7 bits, interleaved bit by bit into 120 bits.
// Words 0-5 and the low 6 bits of word 6 carry 48 payload bits, bit 6 of word 6 is the
// continuation flag and word 7 is a checksum (sum of the 15-bit codewords 0-6, low 7 bits).
// The payload bits are 8 six bit characters (alphanumeric) or 12 four bit characters
// (numeric), least significant bit first. Alphanumeric messages are padded with 0x3e
// (Null/EOM) and numeric messages with 0xa (Null). Whether a block holds alphanumeric or
// numeric characters is not signalled.
//
// Character sets (Rep. 900-2 Annex I Table VII): alphanumeric 0x00-0x1f are ASCII
// 0x20-0x3f, 0x20 is \, 0x21-0x3a are the letters, 0x3b {, 0x3c CR/LF, 0x3d }, 0x3e
// Null/EOM and 0x3f a spare code. The table lists the letters in lower case; they are
// given in upper case, as GSC pagers display them. Numeric 0-9 are digits,
// 0xa Null, 0xb U, 0xc space, 0xd -, 0xe the triple bar (U+2261, given in UTF-8) and 0xf
// shifts the next nibble to the set A B C D E space F G H J Null L N P R spare. A pager
// without the shifted set shows 0xf as E, so a shift not followed by a character is E.
// Spare codes give <0xNN> with the code value.
//
// Message bits in a decoded message are the payload bits of all blocks, first transmitted
// bit first, packed 8 per byte with the first bit in the most significant bit.
class MODEMRADIOPAGING_API RadioPagingGsc
{
public:
    static const uint16_t m_startCode = 713;
    static const uint16_t m_activationCode = 2563;
    static const int m_preambleValues = 10;
    static const int m_commaBits = 28;
    static const int m_codewordBits = 46;               //!< Duplicated Golay codeword
    static const int m_preambleCodewords = 18;
    static const int m_blockBits = 121;                 //!< 1 bit + 120 interleaved bits
    static const int m_toneCommaBits = 968;             //!< Longest comma after a tone only address skipped by the decoder (not sent by the encoder)
    static const int m_slotBits = 121;                  //!< Comma filling an unused batch slot
    static const int m_payloadBitsPerBlock = 48;
    static const int m_maxBlocks = 32;
    static const int m_maxMessageBits = 48 * 32;
    static const int m_batchSlots = 16;                 //!< Slots of 121 bits after a start code
    static const int m_maxBatchMessages = 32;           //!< Messages in a batch, at most
    static const int m_nonBatterySaverBits = 752;       //!< Preamble of repeating 1100
    static const int m_dataBitRate = 600;               //!< Comma and data rate, the symbol rate

    enum MessageType {
        Tone,           //!< Address only
        Voice,          //!< Address and activation code, voice audio follows
        Numeric,
        Alphanumeric,
        Data            //!< Decoded only: alphanumeric or numeric, not signalled
    };

    enum Mode {
        Auto,               //!< Single for one message, otherwise Batch
        Single,             //!< One message after its own preamble
        Batch,              //!< Complemented preamble, 16 slots after each start code, extended as needed
        NonBatterySaver     //!< Consecutive messages after a 1100 pattern, no codeword preamble or start code (Rep. 900-2 Annex I 2.5)
    };

    struct TxMessage
    {
        std::string m_address;      //!< 6 digits: index, group, 3 address digits
        int m_addressNumber = 1;    //!< 1-4 (function A-D)
        MessageType m_type = Alphanumeric;
        //!< Numeric: 0-9 A-H J L N P R U space - and the triple bar (UTF-8 U+2261, also = or *). Alphanumeric: the ASCII
        //!< of Table VII (the default codepage: letters of either case, \ { } LF taken as @ [ ] CR), others as space; or
        //!< the 6-bit codes of a codepage (m_codes)
        std::string m_text;
        bool m_codes = false;       //!< Alphanumeric: m_text is the 6-bit codes (RadioPagingCharset::encode), not ASCII
    };

    struct EncodeOptions
    {
        Mode m_mode = Auto;
    };

    struct EncodeResult
    {
        bool m_ok = false;
        std::string m_error;
        std::vector<uint8_t> m_bits;    //!< One bit per element in transmission order, 1 = positive deviation
        int m_preamble = -1;            //!< Preamble value index used, -1 if none
        int m_slots = 0;                //!< 121 bit blocks after the first start code (or the 1100 pattern)
        bool m_batch = false;           //!< Batch mode (the complemented preamble)
        std::vector<int> m_messageSlots;    //!< Of each message: the slot of its address in its batch, from 0
        std::vector<int> m_messageBlocks;   //!< Of each message: its 121 bit blocks, the address included
    };

    struct GolayResult
    {
        uint32_t m_word;        //!< Corrected 23-bit codeword
        uint16_t m_data;        //!< 12 data bits of the corrected codeword
        int m_bitErrors;        //!< Bits corrected, 0-3. More than 3 errors decode to a wrong codeword
    };

    struct BchResult
    {
        uint16_t m_word;        //!< Corrected 15-bit codeword (unchanged if uncorrectable)
        uint8_t m_data;         //!< 7 data bits of the corrected codeword
        int m_bitErrors;        //!< Bits corrected, 0-2, -1 if uncorrectable
    };

    struct Interpretation
    {
        std::string m_type;         //!< alphanumeric, numeric or tone
        std::string m_text;
        std::string m_status;       //!< Of m_text, one for each code point: . received, c corrected, X lost ('?')
        int m_score = 0;            //!< Content score of the winner, higher is better, can be negative
        std::string m_altType;      //!< The other candidate, empty if none
        std::string m_altText;
        int m_altScore = 0;
    };

    //!< Golay(23,12), generator x^11 + x^10 + x^6 + x^5 + x^4 + x^2 + 1. Data in bits 11-0
    static uint32_t golayEncode(uint16_t data);
    //!< Corrects up to three bit errors
    static GolayResult golayDecode(uint32_t word);
    //!< Remainder of a 23-bit word divided by the generator, 0 for a codeword
    static uint32_t golaySyndrome(uint32_t word);
    //!< BCH(15,7), generator x^8 + x^4 + x^2 + x + 1. Data in bits 6-0
    static uint16_t bchEncode(uint8_t data);
    //!< Corrects up to two bit errors
    static BchResult bchDecode(uint16_t word);
    //!< Remainder of a 15-bit word divided by the generator, 0 for a codeword
    static uint32_t bchSyndrome(uint16_t word);

    //!< Preamble value of an index 0-9
    static uint16_t preambleValue(int index);
    //!< Index of a preamble value, -1 if it is none
    static int preambleIndex(uint16_t value);
    //!< Words 1 and 2 of a 6 digit address and the preamble index. Fails for non digits and
    //!< for the address digit combinations that are not allowed
    static bool encodeAddress(const std::string& address, int& preamble, uint16_t& word1, uint16_t& word2, std::string *error = nullptr);
    //!< Inverse of encodeAddress(). Needs the preamble index (-1 if none, non battery saver: the index is then N)
    static bool decodeAddress(uint16_t word1, uint16_t word2, int preamble, std::string& address);
    //!< Address of the message records: the 6 characters of the address (index, group, 3 digits; N for the index
    //!< of non battery saver), / and the address number 1-4 (123456/1), the same in RX, TX and the message center
    static std::string addressText(const std::string& address, int addressNumber);
    //!< Function letter A-D of an address number 1-4, 0 if out of range
    static char functionLetter(int addressNumber);
    //!< Address number 1-4 of a number 1-4 or a function letter A-D (either case), 0 if it is none
    static int addressNumber(char numberOrFunction);
    //!< A cap code: up to 6 digits, the address zero padded to 6 (5 digits: index 0 in front), or 7 digits, the
    //!< address and the suffix digit. addressNumber: 1-4 of the suffix, 0 without one. False and an error for
    //!< more digits, non digits and the address digits that are not allowed
    static bool parseCapCode(const std::string& capCode, std::string& address, int& addressNumber, std::string *error = nullptr);
    //!< Address number 1-4 of a suffix digit: 1-4 and 5-8 are 1-4, 9 and 0 are 1 and 2; 0 for a non digit
    static int suffixAddressNumber(char digit);
    //!< The digits of a cap code (index, group, address, suffix) and the preamble and words they give, or why it
    //!< is not a cap code
    static std::string capCodeText(const std::string& capCode);
    //!< Index of a word 1 value in the table of 50 values, -1 if it is none
    static int word1Index(uint16_t word1);

    //!< Build the transmission for the messages in the given order
    static EncodeResult encode(const std::vector<TxMessage>& messages, const EncodeOptions& options);
    static EncodeResult encode(const std::vector<TxMessage>& messages);
    //!< The message bits of a data page (numeric or alphanumeric), padded with Null to whole blocks: 48 a block, in
    //!< the order of the bits of a decoded message. False and an error if a character is not allowed
    static bool messageBits(const TxMessage& message, std::vector<uint8_t>& bits, std::string& error);

    //!< Message bits as stored in a decoded message: packed 8 per byte, first bit in the
    //!< most significant bit of byte 0. Alphanumeric: the glyphs of the codepage (RadioPagingCharset::decode), the default
    //!< one (Table VII) without one; Null is padding and gives no character
    //!< lost: bits from codewords that could not be corrected, packed as packed (none if null): the characters
    //!< with such a bit are '?'. corrected: bits from codewords that were corrected, packed the same way.
    //!< status (if not null): one character for each character (code point) of the text, . received, c with a bit of
    //!< a corrected codeword, X lost
    static std::string decodeNumeric(const std::vector<uint8_t>& packed, int nbBits, const std::vector<uint8_t> *lost = nullptr,
        const std::vector<uint8_t> *corrected = nullptr, std::string *status = nullptr);
    static std::string decodeAlphanumeric(const std::vector<uint8_t>& packed, int nbBits, const RadioPagingCharset::Codepage *codepage = nullptr,
        const std::vector<uint8_t> *lost = nullptr, const std::vector<uint8_t> *corrected = nullptr, std::string *status = nullptr);
    static int scoreNumeric(const std::vector<uint8_t>& packed, int nbBits);
    static int scoreAlphanumeric(const std::vector<uint8_t>& packed, int nbBits);
    //!< Numeric pages longer than this many characters lean heavily towards alphanumeric
    static const int m_longNumericChars = 15;
    //!< Taken off the numeric score for each character of the numeric text above m_longNumericChars
    static int longNumericPenalty(const std::string& numeric);
    //!< Decide between numeric and alphanumeric by content. No bits means tone only
    static Interpretation interpret(const std::vector<uint8_t>& packed, int nbBits, const RadioPagingCharset::Codepage *codepage = nullptr,
        const std::vector<uint8_t> *lost = nullptr, const std::vector<uint8_t> *corrected = nullptr);

    //!< Replace characters below 0x20 and 0x7f by <0xNN>
    static std::string escapeControl(const std::string& text);

    //!< Bits of a transmission of an alternating comma of a given length, first bit given
    static void appendComma(std::vector<uint8_t>& bits, int nbBits, int firstBit);
    //!< Golay codeword, LSB first, every bit twice
    static void appendDuplicated(std::vector<uint8_t>& bits, uint32_t codeword);
    //!< Comma of 28 bits, a codeword, the separator bit and a second codeword: the format of
    //!< start code, address and activation code (Rep. 900-2 Annex I 2.3.2)
    static void appendCodewordPair(std::vector<uint8_t>& bits, uint32_t first, uint32_t second);
    //!< Activation code and its complement: sent after a voice address, and again as the
    //!< turn-off word at the end of the voice message (Motorola GSC manual, Figure 6-1)
    static void appendActivationCode(std::vector<uint8_t>& bits);
    //!< Interleaved block of 8 BCH codewords preceded by its separator bit
    static void appendBlock(std::vector<uint8_t>& bits, const uint16_t codewords[8]);
    //!< Golay codeword (23 bits) of 46 symbols: first bit of every pair
    static uint32_t resolveDuplicated(const uint8_t *bits);
    //!< Golay codeword (23 bits) of 46 soft symbols (positive = 1): every bit is decided on
    //!< the sum of its two symbols, a zero sum on the first symbol
    static uint32_t resolveDuplicated(const float *soft);
    //!< The triple bar of the numeric set in UTF-8
    static const char *tripleBar();
    //!< 8 BCH codewords of a block, bits points after the separator bit
    static void deinterleaveBlock(const uint8_t *bits, uint16_t codewords[8]);
};

#endif // MODEMRADIOPAGING_RADIOPAGINGGSC_H
