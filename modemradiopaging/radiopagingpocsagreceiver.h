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

#ifndef MODEMRADIOPAGING_RADIOPAGINGPOCSAGRECEIVER_H
#define MODEMRADIOPAGING_RADIOPAGINGPOCSAGRECEIVER_H

#include <deque>

#include "export.h"
#include "radiopagingdedup.h"
#include "radiopagingfskdemod.h"
#include "radiopagingpocsagdecoder.h"

// POCSAG receiver for one speed: FM discriminator samples to messages of both polarities.
// A message received again within the duplicate window is output marked duplicate.
//
// The preamble (reversals 1010...) is there to train the bit clock. The demodulator
// starts with acquisition loop gains; once it has received a part of the preamble
// (m_preambleLockBits reversals) it is locked: timing and level tracking switch to
// tracking gains, trained on the reversals. Only a part of the preamble is needed, a
// receiver that starts during the preamble locks just the same. The lock is held while a
// decoder is synchronised and released when neither the preamble nor a batch is being
// received. Word and batch synchronisation are the decoders' own (see
// RadioPagingPocsagDecoder): they do not depend on the preamble, so a transmission
// joined after its preamble is still acquired from its batches.
class MODEMRADIOPAGING_API RadioPagingPocsagReceiver
{
public:
    //!< Reversals that lock the demodulator: one codeword, the time a pager needs to see a
    //!< whole codeword of preamble
    static const int m_preambleLockBits = 32;

    struct Status
    {
        bool m_locked;                              //!< Demodulator locked (preamble or batches)
        bool m_preamble;                            //!< Receiving the preamble
        RadioPagingPocsagDecoder::Status m_normal;
        RadioPagingPocsagDecoder::Status m_inverted;
        float m_deviation;                          //!< Measured deviation, discriminator units
        float m_dc;                                 //!< Measured DC offset (carrier offset), discriminator units
    };

    RadioPagingPocsagReceiver();

    //!< sampleRate in Hz, baud 512, 1200 or 2400
    void create(double sampleRate, int baud);
    void reset();
    //!< The decoders back to hunting, what they were receiving dropped without a message: a lock the receiver of
    //!< the channel does not take (another protocol is receiving). The demodulator, the time and the duplicates stay
    void dropLock();
    void setPolarities(bool normal, bool inverted) { m_normalEnabled = normal; m_invertedEnabled = inverted; }
    //!< Messages remembered for duplicates: the last ones of each pager, within this time (default 60 s)
    void setDuplicateWindow(double seconds) { m_dedup.setWindow(seconds); }
    //!< Frequency of the receiver, Hz: a pager is on one frequency (part of the duplicate key)
    void setFrequency(int64_t frequency) { m_dedup.setFrequency(frequency); }
    //!< Messages remembered for duplicate detection, of a polarity (0 normal, 1 inverted) or of both (-1)
    int remembered(int polarity = -1) const { return (int) m_dedup.messages(m_time, polarity); }
    int getBaud() const { return m_baud; }

    void process(float sample);
    //!< The input stopped: the batches being received are decoded as far as they were received
    void flush();

    bool hasMessage() const { return !m_messages.empty(); }
    RadioPagingPocsagDecoder::Message takeMessage();
    Status getStatus() const;

    //!< A decoder of either polarity is synchronised
    bool isSynced() const;
    //!< DC offset and deviation were just measured on a sync codeword: the DC offset, discriminator units
    bool takeLevelsMeasured(float& dc);
    //!< The input was moved by delta (frequency correction before the discriminator)
    void offsetLevels(float delta) { m_demod.offsetLevels(delta); }

private:
    int m_baud;
    bool m_normalEnabled;
    bool m_invertedEnabled;
    RadioPagingFskDemod m_demod;
    RadioPagingPocsagDecoder m_normal;
    RadioPagingPocsagDecoder m_inverted;
    int m_lastBit;
    int m_reversals;                    //!< Consecutive reversals
    bool m_preambleLocked;
    int m_bitsSincePreamble;            //!< Bits since the last run of m_preambleLockBits reversals
    //! Bits since a run of m_preambleLockBits reversals ended, at most m_preambleRecentBits: a sync codeword completed
    //! within m_preambleRecentBits of it (it started at most a codeword after the run) follows a preamble
    int m_bitsSinceReversals = m_preambleRecentBits;
    static const int m_preambleRecentBits = 2 * m_preambleLockBits;
    std::deque<RadioPagingPocsagDecoder::Message> m_messages;
    RadioPagingDedup m_dedup{"pocsag"};
    double m_time = 0.0;                //!< Seconds of signal received, for the duplicate window
    bool m_levelsMeasured = false;
    float m_measuredDc = 0.0f;

    void collect(RadioPagingPocsagDecoder::Message message);
    void fitSyncCodeword(bool inverted);
};

#endif // MODEMRADIOPAGING_RADIOPAGINGPOCSAGRECEIVER_H
