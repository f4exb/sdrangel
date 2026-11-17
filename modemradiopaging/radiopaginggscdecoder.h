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

#ifndef MODEMRADIOPAGING_RADIOPAGINGGSCDECODER_H
#define MODEMRADIOPAGING_RADIOPAGINGGSCDECODER_H

#include <bitset>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "export.h"
#include "radiopagingberwindow.h"
#include "radiopaginggsc.h"

// Sequential Golay bit stream decoder for one polarity. It hunts for sync on its own,
// using nothing but the bits it is fed.
//
// Preamble, start code and addresses are Golay words at 300 bit/s, commas and data blocks
// are at 600 bit/s. The decoder works on 600 baud symbols: a 300 bit/s bit is decided on
// the sum of its two soft symbols, a 600 bit/s bit on its symbol.
//
// Sync is a sliding window of 46 symbols tested as a duplicated Golay codeword against
// the 10 preamble values. Three hits in a row at a distance of 46 symbols, all for the
// same value, lock the decoder. A complemented preamble is a batch. After the lock the
// decoder looks for the start code on the word grid of the preamble: after each further
// preamble word a comma and the start code may follow. The start code is detected on
// Word 1 or on Word 2 (its complement), each on its own, and its position is refined to
// the symbol that correlates best, which gives the timing of the 600 bit/s blocks. Later
// words and blocks keep that timing: a symbol slip after the start code is not followed.
//
// A word is only taken where its symbols come in equal pairs. A comma has alternating
// symbols whose pair sums are noise, so it is rejected unless the noise is strong.
//
// Addresses and data blocks are parsed as they arrive, so a message is available as soon
// as its last block is in. A tone only page is its address alone, so the next address,
// start code or preamble may follow at once. A transmission ends when what follows a
// message is neither a start code nor an address, when a new preamble is locked, or on
// flush().
//
// Symbols are the output of an FSK demodulator, positive = positive deviation. For normal
// polarity a binary 1 is a positive deviation, inverted polarity is the opposite. Run
// one decoder per polarity: a transmission is only decoded by the decoder whose
// polarity matches, because the preamble and the start code tell the polarities apart.
//
// A voice page (address and activation code) is reported at once. Then follow the alert
// time, in which tone only addresses may be interleaved, the voice message and the
// activation code again as the turn-off word (Motorola GSC manual, Figure 6-1 and Code Word
// Automatic Reset). In this voice state the decoder looks, at any bit offset, for a comma
// followed by the activation code on Word 1 or Word 2, for interleaved addresses during the
// alert time, and for a new preamble. It reports the end of the voice message as an update
// of the voice page (m_update): turn-off word, new transmission, timeout or signal lost.
//
// The optional non battery saver mode (a 1100 pattern instead of the preamble, no start
// code) has no such structure: both polarities decode it, the inverted one with the
// address number bits flipped. RadioPagingGscReceiver decides between them.
class MODEMRADIOPAGING_API RadioPagingGscDecoder
{
public:
    enum Polarity {
        Normal,
        Inverted
    };

    enum State {
        Hunting,        //!< Looking for a preamble
        AwaitStart,     //!< Locked on the preamble, looking for the start code or the address
        InTransmission, //!< Receiving addresses and messages
        Voice           //!< After a voice page: alert time and voice message, waiting for the turn-off word
    };

    //!< How the voice message of a voice page ended
    enum VoiceEnd {
        VoiceNotEnded,          //!< Not a voice page, or the voice message is going on
        VoiceTurnOff,           //!< The activation code was sent again as the turn-off word
        VoiceNewTransmission,   //!< A new preamble or non battery saver pattern came
        VoiceTimeout,           //!< Neither came within the voice timeout
        VoiceSignalLost         //!< flush(): the signal is gone
    };

    struct Message
    {
        std::string m_address;              //!< 6 digits: index, group, 3 address digits. Index is '?' in non battery saver mode
        int m_addressNumber = 1;            //!< 1-4 (function A-D), from the complemented address words
        RadioPagingGsc::MessageType m_type = RadioPagingGsc::Tone;   //!< Tone, Voice or Data
        std::vector<uint8_t> m_rawData;     //!< Message bits, packed 8 per byte, first bit in the MSB
        std::vector<uint8_t> m_rawLost;     //!< The bits of m_rawData from codewords that could not be corrected, packed the same way
        std::vector<uint8_t> m_rawCorrected; //!< The bits of m_rawData from codewords that were corrected, packed the same way
        int m_rawBits = 0;                  //!< Number of message bits, a multiple of 48; 0 = tone only or voice
        int m_preamble = -1;                //!< Preamble value index, -1 in non battery saver mode
        int m_slot = 0;                     //!< Index of the address in the transmission, from 0 (addresses only, not 121 bit batch slots)
        bool m_batch = false;               //!< Transmission with a complemented preamble
        bool m_nonBatterySaver = false;
        int m_errorsCorrected = 0;          //!< Bits corrected in the codewords of the address and message
        int m_errorsUncorrectable = 0;      //!< BCH codewords that could not be corrected
        int m_checksumErrors = 0;           //!< Blocks with a wrong checksum
        int m_checksumPassed = 0;           //!< Blocks with a good checksum
        bool m_truncated = false;           //!< The transmission ended before the message did
        Polarity m_polarity = Normal;
        bool m_interleaved = false;         //!< Tone only address sent in the alert time of a voice page
        bool m_update = false;              //!< Repeats an earlier voice page with the end of its voice message
        bool m_duplicate = false;           //!< Set by the receiver: the same page was received shortly before
        VoiceEnd m_voiceEnd = VoiceNotEnded;
        float m_voiceSeconds = 0.0f;        //!< Update: time from the activation code to the end (alert time included)
    };

    struct Status
    {
        State m_state;
        Polarity m_polarity;
        uint32_t m_codewords;               //!< Golay and BCH codewords of addresses and messages received since reset
        uint32_t m_codewordsCorrected;
        uint32_t m_codewordsUncorrectable;
        uint32_t m_transmissions;           //!< Preamble locks since reset
        uint32_t m_messages;                //!< Messages produced since reset
        RadioPagingBerWindow m_ber;         //!< Of the current (or last) transmission
    };

    explicit RadioPagingGscDecoder(Polarity polarity = Normal);

    void reset();
    void setPolarity(Polarity polarity) { m_polarity = polarity; }
    //!< Enable the detection of the non battery saver pattern. Off by default
    void setNonBatterySaver(bool enable) { m_nonBatterySaver = enable; }
    //!< Longest voice message without a turn-off word, in seconds
    void setVoiceTimeout(double seconds);
    //!< Feed one hard symbol: 1 = positive deviation, 0 = negative deviation
    void feed(int symbol);
    //!< Feed one soft symbol: positive = positive deviation, the magnitude is the confidence
    void feedSoft(float soft);
    //!< The signal is gone: finish the transmission in progress
    void flush();

    bool isSynced() const { return m_state != Hunting; }
    //!< A transmission without preamble and start code (non battery saver) is being received
    bool inNonBatterySaver() const { return (m_state != Hunting) && m_nbs; }
    //!< Alert time or voice message of a voice page: no symbols, other than the turn-off word,
    //!< interleaved addresses or a new preamble
    bool inVoice() const { return m_state == Voice; }
    //!< Voice message of a voice page: after the alert time and the last word that may be
    //!< interleaved in it, until the end of the page
    bool inVoiceMessage() const { return (m_state == Voice) && (m_voiceSymbols > m_voiceMessageStart); }
    Status getStatus() const;
    bool hasMessage() const { return !m_messages.empty(); }
    Message takeMessage();

    //! A pattern of known symbols just recognised (the preamble words that locked, a start
    //! code and its complement, the non battery saver pattern), to measure DC offset and
    //! deviation on
    struct KnownPattern
    {
        std::vector<float> m_levels;        //!< +1 positive deviation, -1 negative, oldest first
        size_t m_symbolsAgo;                //!< Symbols fed after its last symbol
    };
    bool takeKnownPattern(KnownPattern& pattern);

    static const int m_minNonBatterySaverBits = 80;     //!< Bits of the 1100 pattern before the lock
    //!< Consecutive bits off the 1100 pattern that end it once locked. A bit error is off the
    //!< pattern for at most 3 bits; the comma that follows is off it at every bit
    static const int m_nonBatterySaverEndBits = 8;
    //!< Bits off the 1100 pattern allowed in the bits it locks on: 2 bit errors. By chance a
    //!< bit fits the pattern with probability 1/4, so random bits do not lock
    static const int m_maxNonBatterySaverLockMisses = 6;
    static const int m_syncHits = 3;
    //!< Preamble words that can follow the lock before the start code
    static const int m_maxPreambleWordsAfterLock = RadioPagingGsc::m_preambleCodewords - m_syncHits;
    //!< Share of the symbol energy of a word kept by the sums of its symbol pairs (1 for
    //!< equal pairs, 0 for a comma) above which it is taken for a word
    static const float m_minPairAgreement;
    //!< BCH codewords of the 8 that must decode for a first block with errors to be taken for
    //!< data rather than for the noise after a tone only page (a random 15-bit word is within
    //!< 2 errors of a codeword with probability 128 * 121 / 32768 = 47%, so 7 of 8 happen by
    //!< chance about 2.5% of the time)
    static const int m_minDamagedBlockCodewords = 7;
    //!< Repeated bits allowed in the 28 bit comma (2 for one bit error) of the address, start
    //!< code or preamble that follows a tone only address at once, and of the first address
    //!< after the non battery saver pattern
    static const int m_maxCommaErrors = 2;
    //!< An address after a message without its comma: taken with this many bits corrected in its two words at most
    static const int m_maxErrorsWithoutComma = 2;
    //!< Alert time after the activation code in which tone only addresses may be interleaved:
    //!< 2 s at 600 bit/s (Motorola GSC manual, Figure 6-1)
    static const int m_alertSymbols = 1200;
    //!< Symbols after the activation code at which the voice message starts: the alert time and
    //!< an interleaved address (comma, word 1, separator bit, word 2) that may end after it
    static const int m_voiceMessageStart = m_alertSymbols + RadioPagingGsc::m_commaBits + 2 * RadioPagingGsc::m_codewordBits + 1;
    //!< Default voice timeout: the longest documented pager timeout, 50 s (Motorola GSC manual,
    //!< Code Word Automatic Reset). Rep. 900-2 gives none
    static const int m_defaultVoiceTimeoutSeconds = 50;
    //!< Quality (rms distance of the soft symbols from +/-1) above which symbols are noise. Measured with the FSK
    //!< modulator through noise at 600 symbols/s: the lowest value of 15 symbols of noise only (0.724), less a margin
    static constexpr float m_dataWordNoiseQuality = 0.70f;
    //!< Quality of clean symbols: a signal that goes from below it to noise within an address or a data block was lost
    //!< there (a weak signal, poor all along, is decoded as it is)
    static constexpr float m_cleanQuality = 0.40f;
    //!< Quality of symbols turned poor (no longer clean)
    static constexpr float m_poorQuality = 0.50f;

private:
    enum Parse {
        SeekStart,      //!< Preamble locked, start code not seen yet
        SeekAddress,    //!< Non battery saver pattern ended, first address not seen yet
        Next,           //!< Waiting for the next address, start code or the end
        Classify,       //!< Address received, telling tone, voice and data apart
        Data            //!< Receiving the blocks of a message
    };

    Polarity m_polarity;
    bool m_nonBatterySaver;
    State m_state;
    Parse m_parse;

    uint64_t m_reg;                     //!< Last 64 bits, newest in bit 0
    int m_regBits;
    float m_softRing[RadioPagingGsc::m_codewordBits];   //!< Last 46 soft symbols, oldest at m_softPos
    int m_softPos;
    int8_t m_hits[2 * RadioPagingGsc::m_codewordBits + 1];  //!< Preamble hit history, ring indexed by m_hitPos
    int m_hitPos;
    std::bitset<m_minNonBatterySaverBits> m_patternMissHistory;   //!< Bits off the 1100 pattern before the lock, newest in bit 0
    int m_patternBits;                  //!< Bits in m_patternMissHistory
    bool m_patternLocked;
    int m_patternRun;                   //!< Bits of the 1100 pattern since the lock, bit errors included
    int m_patternMisses;                //!< Consecutive bits off the 1100 pattern once locked

    std::vector<uint8_t> m_buf;         //!< Bits received since the lock
    std::vector<float> m_soft;          //!< Soft symbols of m_buf, positive = 1
    int m_pos;                          //!< Parse position in m_buf (may be past its end while skipping a comma)
    int m_scan;                         //!< Next start code or address candidate offset
    int m_commaBlocks;                  //!< Comma blocks after a tone only address that may still be skipped

    Message m_voiceMessage;             //!< The voice page whose voice message is going on
    int m_voiceSymbols;                 //!< Symbols since the activation code
    int m_voiceTimeoutSymbols;
    mutable float m_lastQuality;        //!< Quality of the last address or data block taken
    int m_voiceHold;                    //!< Symbols before the next word may be detected in the voice state

    int m_preamble;
    bool m_batch;
    bool m_nbs;
    int m_slot;

    bool m_active;                      //!< A message is being received
    Message m_current;
    std::vector<uint8_t> m_currentBits;
    std::vector<uint8_t> m_currentLost;     //!< 1 for each bit of m_currentBits from a codeword that could not be corrected
    std::vector<uint8_t> m_currentCorrected; //!< 1 for each bit of m_currentBits from a codeword that was corrected
    int m_blocks;

    uint32_t m_codewords;
    uint32_t m_codewordsCorrected;
    uint32_t m_codewordsUncorrectable;
    uint32_t m_transmissions;
    RadioPagingBerWindow m_ber;
    uint32_t m_messageCount;

    std::deque<Message> m_messages;
    std::deque<KnownPattern> m_known;

    //!< Known bits (decoder polarity, 1 = positive) as levels of the demodulator
    void addKnown(const std::vector<uint8_t>& bits, size_t symbolsAgo);
    void resetPattern();

    int preambleHit() const;
    void startTransmission(int index, bool batch);
    void startNonBatterySaver();
    void endTransmission(bool truncated);
    void process();
    void clearBuffer();
    //!< Golay codeword of the 46 symbols at a position of m_buf
    uint32_t wordAt(int pos) const;
    //!< Soft correlation of the 46 symbols at a position of m_buf with a codeword
    float correlation(int pos, uint32_t codeword) const;
    float softQuality(int pos, int count, int step) const;
    int noiseOnset(int pos, int count, float limit, int minClean, int minPoor) const;
    //!< The 46 symbols at a position of m_buf are pairs of equal symbols, not a comma
    bool isWord(int pos) const;
    //!< Decodes the word at a position of m_buf. False if the symbols are not a word
    bool decodeWord(int pos, RadioPagingGsc::GolayResult& result) const;
    //!< Positions of n bits where a bit equals the one before it (0 for a comma)
    static int commaErrors(const uint8_t *bits, int n);
    //!< A 121 bit block is an alternating comma
    static bool isComma(const uint8_t *bits);
    bool processSeekStart();
    bool processSeekAddress();
    bool processNext();
    bool processClassify();
    bool processData();
    bool parseAddress(int pos, Message& message, int& errors1, int& errors2) const;
    void startVoice();
    void processVoice();
    //!< Ends the voice state with an update of the voice page. 'before' symbols of what ended it are not counted
    void endVoice(VoiceEnd end, int before);
    void finishMessage(bool truncated);
    void trim();
};

#endif // MODEMRADIOPAGING_RADIOPAGINGGSCDECODER_H
