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

#ifndef INCLUDE_DSDPROTOCOL_H
#define INCLUDE_DSDPROTOCOL_H

#include <cstdint>
#include <string>
#include <vector>

#include "dsp/dv/dvsync.h"

/**
 * A sliced symbol with its soft decisions. LLRs are positive when the bit is
 * more likely 0 (the mbelib convention); the dibit is 01 = +3, 00 = +1,
 * 10 = -1, 11 = -3.
 */
struct DSDSoftSymbol
{
    uint8_t m_dibit = 0;
    float m_llrMsb = 0.0f;
    float m_llrLsb = 0.0f;
    float m_reliability = 0.0f;
    bool m_present = true;   //!< Carrier present at this symbol
};

/**
 * Ring of the most recent symbols indexed by an absolute symbol count, shared
 * by the demodulator (writer) and the protocol decoders (readers).
 */
class DSDSymbolHistory
{
public:
    DSDSymbolHistory(int size = 2048) : m_symbols(size), m_samples(size), m_newest(-1) {}

    void push(int64_t index, const DSDSoftSymbol& symbol, double sampleIndex)
    {
        m_symbols[index % (long long) m_symbols.size()] = symbol;
        m_samples[index % (long long) m_samples.size()] = sampleIndex;
        m_newest = index;
    }

    bool has(int64_t index) const { return (index >= 0) && (index <= m_newest) && (m_newest - index < (long long) m_symbols.size()); }
    const DSDSoftSymbol& at(int64_t index) const { return m_symbols[index % (long long) m_symbols.size()]; }
    int64_t sampleAt(int64_t index) const { return (long long) (m_samples[index % (long long) m_samples.size()] + 0.5); }
    double positionAt(int64_t index) const { return m_samples[index % (long long) m_samples.size()]; }
    void replace(int64_t index, const DSDSoftSymbol& symbol) { m_symbols[index % (long long) m_symbols.size()] = symbol; }
    void replace(int64_t index, const DSDSoftSymbol& symbol, double position)
    {
        m_symbols[index % (long long) m_symbols.size()] = symbol;
        m_samples[index % (long long) m_samples.size()] = position;
    }
    int64_t newest() const { return m_newest; }
    void clear() { m_newest = -1; }

    //! Copy nbDibits dibits from firstIndex into bits (2 per dibit, MSB first)
    void bits(int64_t firstIndex, int nbDibits, uint8_t *bits) const
    {
        for (int i = 0; i < nbDibits; i++)
        {
            const DSDSoftSymbol& s = at(firstIndex + i);
            bits[2 * i] = (s.m_dibit >> 1) & 1;
            bits[2 * i + 1] = s.m_dibit & 1;
        }
    }

    //! Copy the LLRs of nbDibits dibits from firstIndex (2 per dibit)
    void llrs(int64_t firstIndex, int nbDibits, float *llr) const
    {
        for (int i = 0; i < nbDibits; i++)
        {
            const DSDSoftSymbol& s = at(firstIndex + i);
            llr[2 * i] = s.m_llrMsb;
            llr[2 * i + 1] = s.m_llrLsb;
        }
    }

private:
    std::vector<DSDSoftSymbol> m_symbols;
    std::vector<double> m_samples;
    int64_t m_newest;
};

/**
 * A recovered vocoder frame: the on-air bits in the order the DSE modulator
 * consumes them, packed MSB first, with one LLR per bit.
 */
struct DSDVoiceFrame
{
    enum Rate {
        RateAMBE3600x2400, RateAMBE3600x2450, RateAMBE2450, RateIMBE7200x4400, RateAMBE7200x4400,
        RateCodec23200, //!< M17: a 64 bit Codec2 3200 frame (8 bytes), 160 samples at 8 kHz
        RateVocos,      //!< Angel61 V1: a radio frame's packets (6 VHF, 18 HF), 320 samples each at 24 kHz
        RateOpus80      //!< Angel61 WB tier 3 (O1): an 80 ms Opus packet (184 bytes), 3840 samples at 48 kHz
    };

    static const int MaxBytes = 360;    //!< a V1 frame of HF: 18 packets of up to 20 bytes (an Opus packet is 184)
    static const int MaxLlrs = 144;     //!< soft decisions are kept for the MBE frames only (IMBE the longest)

    int64_t m_sample = 0;      //!< Sample index of the last symbol of the frame
    int m_slot = 0;
    Rate m_rate = RateAMBE3600x2450;
    int m_nbBits = 72;
    bool m_erasure = false; //!< The frame was lost (Angel61: a frame whose payload failed): the vocoder conceals its time
    //! V1 (RateVocos): the frame's packets, m_packetBytes each from m_bytes on (0 bytes: erased); a
    //! frame with m_flush ends the transmission, the decoder's last audio then a reset
    int m_packets = 0;
    int m_packetBytes = 0;
    bool m_flush = false;
    //! The channel bit errors the protocol's own code corrected in this frame, when the voice is
    //! protected by the protocol rather than inside the vocoder frame (M17's convolutional code,
    //! YSF V/D type 2's, Angel61's LDPC code): the BER the demodulator shows. 0 bits: the vocoder's corrections count
    int m_channelErrors = 0;
    int m_channelBits = 0;
    uint8_t m_bytes[MaxBytes] = { 0 };
    float m_llr[MaxLlrs] = { 0.0f };    //!< the first min(m_nbBits, MaxLlrs) are meaningful

    static bool isOpus(Rate rate) { return rate == RateOpus80; }
    static bool isVocos(Rate rate) { return rate == RateVocos; }
    //! Audio samples and rate the vocoder produces for a frame of this rate (a V1 frame: 320 a packet)
    static int getAudioSamples(Rate rate) { return isOpus(rate) ? 3840 : isVocos(rate) ? 320 : 160; }
    static int getAudioSampleRate(Rate rate) { return isOpus(rate) ? 48000 : isVocos(rate) ? 24000 : 8000; }
};

/** A complete protocol unit captured after burst refinement and before vocoding. */
struct DSDProtocolFrame
{
    int64_t m_sample = -1;
    std::string m_format;
    std::string m_type;
    std::vector<uint8_t> m_data;
    int m_slot = 0;
    int m_sequence = 0;
};

struct DSDProtocolEvent
{
    enum Kind { Start, Frame, End, Lost };

    Kind m_kind = Frame;
    uint64_t m_callId = 0;
    uint64_t m_sequence = 0;
    int64_t m_sample = -1;
    std::string m_protocol;
    DSDProtocolFrame m_frame;
    std::string m_reason;
};

/**
 * Protocol decoder interface. The demodulator drives it with the sync it
 * found, then with every sliced symbol; the decoder says where the next
 * synchronisation is expected and which patterns are valid there, and can
 * confirm a burst by other means (an EMB, a colour code) when the sync is
 * absent by design. Symbol indexes are the demodulator's absolute count.
 */
class DSDProtocol
{
public:
    DSDProtocol(const DSDSymbolHistory& history) : m_history(history) {}
    virtual ~DSDProtocol() {}

    virtual const char *getName() const = 0;
    virtual int getSymbolRate() const = 0;
    virtual bool isFourLevel() const = 0;
    //! Patterns that start a transmission of this protocol
    virtual std::vector<DVSync::Pattern> getSearchPatterns() const = 0;
    //! Nominal outer symbol level in hertz, for plausibility checks on a detection
    virtual float getNominalOuterLevel() const = 0;
    //! Least correlation coefficient of a sync fit that may start a lock. Below 0.85 the DMR
    //! decoder locks on the wrong pattern too often at low signal to noise ratios; a decoder
    //! that rejects a chance lock quickly and whose real signals fit its templates less well
    //! (D-STAR: radios shape GMSK in their own ways) may go lower.
    virtual float getLockThreshold() const { return 0.85f; }

    //! A sync was found: pattern and the index of its last symbol
    virtual void start(DVSync::Pattern pattern, int64_t syncIndex) = 0;
    //! One sliced symbol (already in the history). Returns true when a burst is complete: the
    //! demodulator then refines the burst's symbols (getBurstSpan) and calls processBurst().
    virtual bool pushSymbol(int64_t index, const DSDSoftSymbol& symbol, int64_t sampleIndex) = 0;
    //! Symbol span of the burst that just completed
    virtual void getBurstSpan(int64_t& first, int64_t& last) const = 0;
    //! Decode the completed burst
    virtual void processBurst() = 0;
    //! True when the burst just processed decoded (a CRC or a code check passed): the decoder
    //! is receiving what it thinks it is. Used to protect a lock from other decoders' chance matches.
    virtual bool wasBurstValid() const { return false; }
    //! Whether the decoder has checked enough since the lock to rule out a chance match of a sync in
    //! noise (its voice frames are held back until then). Decoders whose every validation is a strong
    //! code (a CRC-16, a BCH) leave the default.
    virtual bool isProven() const { return true; }
    //! Where the next sync is expected (index of its last symbol) and the patterns valid there.
    //! Returns false when the decoder has no expectation (the transmission ended).
    virtual bool getNextSync(int64_t& syncIndex, std::vector<DVSync::Pattern>& patterns) const = 0;
    //! A sync was found at (or near) the expected position
    virtual void resync(DVSync::Pattern pattern, int64_t syncIndex) = 0;
    //! A strong sync of this protocol turned up away from the expected position: true when the
    //! decoder can account for it (a unit of a length it could not know, a NID it could not read)
    //! and wants it taken as the next sync
    virtual bool acceptUnexpectedSync(DVSync::Pattern pattern, int64_t syncIndex) const { (void) pattern; (void) syncIndex; return false; }
    //! Symbol span the decoder will examine to confirm a burst without a sync at syncIndex, so
    //! that the demodulator can refine those symbols first
    virtual void getConfirmSpan(int64_t syncIndex, int64_t& first, int64_t& last) const = 0;
    //! No sync at the expected position: the decoder may still accept the burst (EMB, colour
    //! code) and returns true, or false to count a miss. present tells if a carrier was there.
    virtual bool confirmWithoutSync(int64_t syncIndex, bool present) = 0;
    //! The demodulator's power measurement calls the burst about to be confirmed absent, but the
    //! lock is proven and the level of a signal through an overloaded or gain pumping front end
    //! swings by 10 dB from one burst to the next: the decoder is asked anyway and should judge
    //! strictly, so that a slot that is really silent fails
    void setDoubtful(bool doubtful) { m_doubtful = doubtful; }
    bool isDoubtful() const { return m_doubtful; }
    //! Lock lost or transmission ended
    virtual void end() = 0;

    std::vector<DSDVoiceFrame> takeFrames()
    {
        std::vector<DSDVoiceFrame> frames;
        frames.swap(m_frames);
        return frames;
    }

    std::vector<DSDProtocolFrame> takeProtocolFrames()
    {
        std::vector<DSDProtocolFrame> frames;
        frames.swap(m_protocolFrames);
        return frames;
    }

    //! True when the decoder stopped because it received an explicit terminator.
    virtual bool endedNormally() const { return false; }

    virtual std::string getStatusText() const { return std::string(); }

protected:
    const DSDSymbolHistory& m_history;
    std::vector<DSDVoiceFrame> m_frames;
    std::vector<DSDProtocolFrame> m_protocolFrames;
    bool m_doubtful = false;    //!< See setDoubtful
};

#endif // INCLUDE_DSDPROTOCOL_H
