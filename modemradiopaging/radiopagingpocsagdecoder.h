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

#ifndef MODEMRADIOPAGING_RADIOPAGINGPOCSAGDECODER_H
#define MODEMRADIOPAGING_RADIOPAGINGPOCSAGDECODER_H

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "export.h"
#include "radiopagingberwindow.h"

// POCSAG bit stream decoder for one polarity. It hunts for sync on its own, using
// nothing but the bits it is fed.
//
// Synchronisation uses the codeword structure only. The preamble trains the bit clock of
// the demodulator and is not counted. A sync codeword with up to 2 bit errors (or up to
// m_maxSoftSyncErrors on symbols that correlate with it) starts a candidate batch, which
// is buffered. A batch is decoded when
//   - its sync codeword is confirmed by another sync codeword one batch (544 bits) away,
//     before or after it, or
//   - no sync codeword follows it: up to the end of the signal in it (below).
// A transmission therefore needs no preamble to be acquired: a receiver that starts
// listening during a transmission synchronises on two consecutive batches. Nothing is
// predicted from earlier transmissions. While a candidate is not confirmed, a sync
// codeword found at another position replaces it.
//
// A message ends with an idle or address codeword, or after two successive codewords
// that cannot be decoded. A single undecodable codeword is kept in the message (its 20
// message bits as received) and counted as uncorrectable.
//
// A batch that no sync codeword follows is decoded up to the end of the signal in it.
// After a confirmed sync codeword it is the last batch of the transmission, which ends at
// the end of a batch (M.584-2 1): decoded whole. After a missed sync codeword, or after a
// sync codeword found alone, the signal ends at its last decipherable codeword (one that
// decodes on symbols better than noise; noise has none), and corrected codewords just
// before that end (the codeword the signal ended in, corrected to another one) are not
// taken. Indecipherable codewords are lost codewords of the message, never addresses.
// Nothing after the end is decoded, the message in progress ends cut and so does
// the transmission. The quality of a codeword is the rms distance of its symbols from the
// decided levels (feed() with the error); without it every codeword that decodes is
// decipherable.
//
// Symbols are the output of an FSK demodulator, 1 = positive deviation. For normal
// polarity a binary 1 is a negative deviation, inverted polarity is the opposite. A
// decoder instance handles one speed; run one per speed and polarity.
class MODEMRADIOPAGING_API RadioPagingPocsagDecoder
{
public:
    enum Polarity {
        Normal,
        Inverted
    };

    enum State {
        Hunting,        //!< Looking for a sync codeword
        InBatch,        //!< Receiving the codewords of a batch
        AwaitSync       //!< Receiving the sync codeword that should follow a batch
    };

    //! How a message ended (M.584-2): an idle or the next address codeword (1.2), the end of the transmission with
    //! the batch the message filled (no idle codeword after it: "should" in 1.2, the message is whole), or cut: the
    //! signal lost in the middle of a batch, sync loss, or two codewords in a row that could not be decoded (2.5.1):
    //! the rest of the message may be missing
    enum End {
        EndIdle,
        EndAddress,
        EndBatch,
        EndCut
    };

    static const char *endName(End end);

    struct Message
    {
        uint32_t m_address = 0;             //!< 21-bit address, frame included
        int m_function = 0;                 //!< 0-3
        std::vector<uint8_t> m_rawData;     //!< Message bits, packed 8 per byte, first bit in the MSB
        int m_rawBits = 0;                  //!< Number of message bits, a multiple of 20; 0 = tone only
        //! One character for each message codeword (20 bits): . received, c corrected, u received but could not be
        //! corrected (its received bits), X lost (taken for noise)
        std::string m_wordStatus;
        int m_errorsCorrected = 0;          //!< Bits corrected in the message's codewords
        int m_errorsUncorrectable = 0;      //!< Codewords that could not be corrected
        End m_end = EndCut;
        int m_batch = 0;                    //!< Batch of the transmission the address was in, from 0
        Polarity m_polarity = Normal;
        bool m_duplicate = false;           //!< Set by the receiver: the same message was received shortly before
    };

    struct Status
    {
        State m_state;
        Polarity m_polarity;
        bool m_confirmed;                   //!< The current batch's sync codeword is confirmed
        int m_batches;                      //!< Batches decoded in the current transmission
        uint32_t m_codewords;               //!< Codewords decoded since reset
        uint32_t m_codewordsCorrected;      //!< ... with bit errors that were corrected
        uint32_t m_codewordsUncorrectable;
        uint32_t m_transmissions;           //!< Transmissions with at least one decoded batch
        uint32_t m_candidatesDropped;       //!< Sync candidates whose batch was not decoded
        uint32_t m_syncMissed;              //!< Sync codewords missed in a transmission (reception went on)
        RadioPagingBerWindow m_ber;         //!< Of the current (or last) transmission
    };

    explicit RadioPagingPocsagDecoder(Polarity polarity = Normal);

    void reset();
    void setPolarity(Polarity polarity) { m_polarity = polarity; }
    //!< Feed one symbol: 1 = positive deviation, 0 = negative deviation
    void feed(int symbol);
    //!< Feed one symbol with its distance from the decided level (normalised, outer levels +/-1)
    void feed(int symbol, float error);
    //!< Feed one symbol with its distance from the decided level and its normalised value (outer levels +/-1)
    void feed(int symbol, float error, float soft);
    //!< Limit of the codewords of a batch no sync codeword follows: a codeword with a quality above it is
    //!< indecipherable (noise)
    void setNoiseQuality(float wordNoiseQuality) { m_wordNoiseQuality = wordNoiseQuality; }
    //!< The sync codeword of the current candidate came after a preamble
    void setPreambleBefore() { m_preambleBefore = true; }
    //!< The input stopped: the codewords of the batch received so far are decoded as a batch no sync codeword
    //!< follows, the message in progress ends cut and the decoder hunts again
    void flush();

    bool isSynced() const { return m_state != Hunting; }
    //!< In a transmission that started after its preamble or whose sync codewords confirmed it
    bool inTrustedTransmission() const { return (m_state != Hunting) && (m_preambleBefore || m_leadingConfirmed || (m_batch > 0)); }
    //!< A candidate batch not trusted (no preamble before it, its sync codeword not confirmed, the first batch) is
    //!< given up: the decoder hunts again. True if one was
    bool dropUntrustedCandidate();
    //!< Sync codewords recognised since reset: when it changes, the last 32 symbols fed were
    //!< a sync codeword (a known pattern to measure the levels on)
    uint32_t syncWords() const { return m_syncWords; }
    Status getStatus() const;
    bool hasMessage() const { return !m_messages.empty(); }
    Message takeMessage();

    //!< Bit errors corrected in a sync codeword, the correction capacity of the code
    static const int m_maxSyncErrors = 2;
    static const int m_maxMessageBits = 20 * 400;
    //!< Bit errors of a sync codeword taken when its symbols correlate with the sync codeword (soft values known):
    //!< the correlation of the normalised values with it, over their magnitudes, at least m_softSyncCorrelation
    static const int m_maxSoftSyncErrors = 4;
    //!< Bit errors within which a codeword corrected to an address is taken as the idle codeword
    static const int m_maxIdleErrors = 4;
    static constexpr float m_softSyncCorrelation = 0.7f;

private:
    Polarity m_polarity;
    State m_state;
    uint32_t m_reg;                     //!< Last 32 bits, newest in bit 0
    int m_regBits;
    uint32_t m_word;
    int m_wordBits;
    int m_slot;
    uint32_t m_batchWords[16];          //!< Codewords of the batch being received
    float m_batchQuality[16];           //!< Their quality (rms symbol error), -1 if not known
    float m_wordError2;                 //!< Sum of the squared symbol errors of the codeword being received
    bool m_wordQualityKnown;
    float m_wordNoiseQuality;
    float m_softBits[32];               //!< Normalised values of the last 32 symbols as bits (positive = binary 1), ring
    int m_softPos;                      //!< Where the next one goes (the oldest)
    bool m_softKnown;
    bool m_preambleBefore;
    bool m_leadingConfirmed;            //!< The sync codeword of the current batch is confirmed
    int m_candidateSyncErrors = 0;      //!< Bit errors of the sync codeword the candidate batch started at
    int m_batch;                        //!< Batches decoded in the current transmission

    bool m_active;
    //! The last codeword of the message in progress was the last codeword of its batch: a transmission ends at the end
    //! of a batch (M.584-2 1), so a message that fills its last batch ends with the transmission, whole
    bool m_messageEndsBatch = false;
    Message m_current;
    std::vector<uint8_t> m_currentBits;
    int m_uncorrectableRun;             //!< Successive undecodable codewords

    uint32_t m_codewords;
    uint32_t m_codewordsCorrected;
    uint32_t m_codewordsUncorrectable;
    uint32_t m_transmissions;
    uint32_t m_candidatesDropped;
    uint32_t m_syncMissed;
    bool m_syncMissedLast;              //!< The sync codeword before the current batch was missed
    uint32_t m_syncWords = 0;
    RadioPagingBerWindow m_ber;

    std::deque<Message> m_messages;

    bool syncMatch() const;
    bool softSyncMatch(uint32_t word) const;
    void startCandidate();
    void decodeBatch();
    bool decodeUnconfirmedBatch(int received = 16);
    bool decipherable(int slot) const;
    void storeWord(int slot, uint32_t word);
    void endTransmission();
    void processCodeword(uint32_t word, bool lost = false);
    void appendMessageBits(uint32_t data, char status);
    void finishMessage(End end);
};

#endif // MODEMRADIOPAGING_RADIOPAGINGPOCSAGDECODER_H
