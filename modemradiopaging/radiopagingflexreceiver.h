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

#ifndef MODEMRADIOPAGING_RADIOPAGINGFLEXRECEIVER_H
#define MODEMRADIOPAGING_RADIOPAGINGFLEXRECEIVER_H

#include <cstdint>
#include <deque>
#include <vector>

#include "export.h"
#include "radiopagingdedup.h"
#include "radiopagingflex.h"
#include "radiopagingflexdecoder.h"
#include "radiopagingfskdemod.h"

// FLEX receiver: FM discriminator samples to frames and messages.
//
// While hunting, the emergency re-synchronization (STD-43A 3.2.1: BS, Ar, inverted BS,
// inverted Ar, 96 bits repeated) is detected on the last 96 bits, with at most
// m_maxErsErrors bit errors (sync 1 of 3200/4 is 16 bits from it, random bits about 48).
// Inverted it is the same cycle shifted by half, so it carries no polarity; it is reported
// while received, with its length.
//
// A demodulator at 1600 baud with 2 levels runs all the time and hunts for sync 1 on its
// own, in both polarities: the mode code A, the 16 bit pattern B and the inverted mode
// code are compared with the code of each mode. Sync 1 is accepted when the 80 bits have
// at most m_maxSyncErrors bit errors, or when the two copies of the mode code (A and
// inverted A) each decode to the same mode (up to 2 bit errors in one copy and 3 in the
// other), whatever B holds. Only the received bits of the frame are used: nothing is
// predicted from earlier frames. The frame information word follows at the same rate. A second demodulator then
// continues the symbol clock at the rate and number of levels of the frame mode,
// starting with the DC offset and deviation measured during sync 1. The second sync is
// skipped by its nominal length (corrected by one symbol when its closing pattern is
// received without errors one symbol early or late), then the data part is sliced into
// the codewords of each phase. Blocks received with a carrier much weaker than during
// sync 1 are marked erased. Each frame is decoded by RadioPagingFlexDecoder.
//
// Input samples are discriminator output scaled so that the outer deviation (4800 Hz)
// is about +/-1. Magnitude squared samples are optional (used for erasures).
class MODEMRADIOPAGING_API RadioPagingFlexReceiver
{
public:
    enum State {
        Hunting,        //!< Looking for sync 1
        FrameInfo,      //!< Receiving the frame information word
        Sync2,          //!< Receiving the second sync
        Data            //!< Receiving the data part
    };

    struct Status
    {
        State m_state;
        RadioPagingFlex::Mode m_mode;   //!< Mode of the current or last frame
        bool m_inverted;
        unsigned m_cycle;               //!< Of the last frame
        unsigned m_frame;
        uint32_t m_syncs;               //!< Sync 1 detections
        uint32_t m_frames;              //!< Frames decoded (frame information word valid)
        uint32_t m_frameInfoFailures;   //!< Sync 1 found but the frame information word failed
        float m_quality;                //!< Of the last frame, 0-1
        RadioPagingBerWindow m_ber;     //!< Of the recent frames, both polarities
        RadioPagingBerWindow m_phaseBer[4];     //!< The same for each phase A to D
        RadioPagingFlexDecoder::FrameReport m_frameReport;      //!< The last frame decoded, either polarity
        // Each polarity (network) apart: its last frame and its bit error rates. The frames of the two may come
        // in turn: the frame being received is not the last one decoded
        RadioPagingFlexDecoder::FrameReport m_frameReportPol[2];
        RadioPagingBerWindow m_berPol[2];
        RadioPagingBerWindow m_phaseBerPol[2][4];
        // Network of the polarity of the frame being received
        RadioPagingFlexDecoder::NetworkTime m_networkTime;
        std::vector<RadioPagingFlexDecoder::GroupSlot> m_groups;
        int m_reassemblies;
        float m_deviation;              //!< Measured outer deviation, discriminator units
        float m_dc;                     //!< Measured DC offset (carrier offset), discriminator units
        bool m_ers;                     //!< Emergency re-synchronization being received
        uint32_t m_ersDetections;       //!< Re-synchronizations received
        float m_ersSeconds;             //!< Length received of the current or last one
    };

    //!< Number of bit errors allowed over the 80 bits of A, B and inverted A
    static const int m_maxSyncErrors = 8;
    //!< Number of bit errors allowed over the last 16 bits of bit sync 1, before a sync 1 that has bit errors
    static const int m_maxBitSyncErrors = 4;
    //!< Number of bit errors allowed over a 96 bit cycle of the emergency re-synchronization
    static const int m_maxErsErrors = 8;
    //!< Blocks with a mean power this many dB below sync 1 are erased
    static constexpr float m_erasureDb = 15.0f;

    RadioPagingFlexReceiver();

    void create(double sampleRate);
    void reset();
    //!< Enable or disable the network of each polarity. There is no mode enable: the system
    //!< chooses the mode of each frame
    void setPolarities(bool normal, bool inverted) { m_enabled[0] = normal; m_enabled[1] = inverted; }
    //!< Also output each fragment of a fragmented message, before the reassembled message. Default off
    void setReportFragments(bool enable) { m_decoder.setReportFragments(enable); }
    //!< Largest message number of the network (RadioPagingFlexDecoder::setMaxMessageNumber)
    void setMaxMessageNumber(int maxNumber) { m_decoder.setMaxMessageNumber(maxNumber); }
    //!< Messages remembered for duplicates: the last ones of each pager, within this time (default 30 minutes)
    void setDuplicateWindow(double seconds) { m_dedup.setWindow(seconds); }
    //!< Frequency of the receiver, Hz: a pager is on one frequency (part of the duplicate key)
    void setFrequency(int64_t frequency) { m_dedup.setFrequency(frequency); }
    //!< Messages remembered for duplicate detection: those of the receiver and the message history of the decoder,
    //!< of a polarity (0 normal, 1 inverted) or of both (-1)
    int remembered(int polarity = -1) const { return (int) m_dedup.messages(m_time, polarity) + m_decoder.historyCount(polarity); }
    //!< The network of a polarity, kept by the decoder: groups in use, messages being reassembled, network time
    std::vector<RadioPagingFlexDecoder::GroupSlot> groups(bool inverted) const { return m_decoder.groups(inverted); }
    std::vector<RadioPagingFlexDecoder::GroupSlotState> groupStates(bool inverted) const { return m_decoder.groupStates(inverted); }
    int reassemblies(bool inverted) const { return m_decoder.reassemblies(inverted); }
    RadioPagingFlexDecoder::NetworkTime networkTime(bool inverted) const { return m_decoder.networkTime(inverted); }

    //!< Process one discriminator sample with its magnitude squared (pass 1 if not known)
    void process(float sample, float magsq = 1.0f);

    bool hasMessage() const { return !m_messages.empty(); }
    RadioPagingFlexDecoder::Message takeMessage();
    //!< The input stopped: the frame being received is decoded from the blocks received (the others erased), then
    //!< partial messages still being reassembled are released marked incomplete
    void flush();

    Status getStatus() const;
    RadioPagingFlexDecoder& decoder() { return m_decoder; }

    //!< Hunting for sync 1 (not in a frame)
    bool isHunting() const { return m_state == Hunting; }
    //!< In a frame, its symbols still those of a FLEX signal (their recent error small): the carrier of the
    //!< frame is on the channel. False when it is lost, or another signal (of another speed) took its place
    bool isReceivingFrame() const;
    //!< The recent error (RMS) of the data symbols isReceivingFrame accepts at most (frame quality 0.55)
    static constexpr double m_receivingErrorRms = 0.3;
    //!< DC offset and deviation were just measured on sync 1: the DC offset, discriminator units
    bool takeLevelsMeasured(float& dc);
    //!< The input was moved by delta (frequency correction before the discriminator)
    void offsetLevels(float delta);

private:
    double m_sampleRate;
    bool m_enabled[2];                  //!< Normal (0) and inverted (1)

    RadioPagingFskDemod m_syncDemod;    //!< 1600 baud, 2 levels, always running
    RadioPagingFskDemod m_dataDemod;    //!< Rate and levels of the frame mode
    //!< Frames of one polarity at a time. The decoder keeps the state of each polarity apart:
    //!< the same capcode on the other polarity is another network
    RadioPagingFlexDecoder m_decoder;

    State m_state;
    uint64_t m_regLo;                   //!< Last 64 received sync rate bits, newest in bit 0
    uint64_t m_regHi;                   //!< The 64 bits before
    double m_syncPower;                 //!< Mean magnitude squared over sync 1
    std::deque<float> m_recentPower;    //!< Magnitude squared of the last sync 1 duration

    RadioPagingFlex::Mode m_mode{};
    bool m_inverted = false;
    uint32_t m_fiw = 0;
    int m_fiwBits = 0;

    int m_s2Symbols;                    //!< Nominal second sync length in symbols
    int m_s2Count;
    uint32_t m_s2Reg = 0;               //!< Bits of the second sync, newest in bit 0
    std::vector<uint8_t> m_s2Tail;      //!< Symbols around the nominal end of the second sync
    std::vector<float> m_s2TailSoft;    //!< Their normalised values
    std::vector<uint8_t> m_s2TailCentre;    //!< Their decisions on the centre of the symbol
    int m_s2TailStart = 0;
    int m_s2ExactEnd = -1;              //!< Symbol count at which inverted C ended without errors, -1 if not seen

    int m_dataSymbols = 0;              //!< Data symbols of the mode
    int m_dataCount = 0;
    int m_bitIndex = 0;                 //!< Bit index in the phase words
    bool m_secondOfPair = false;        //!< 6400/4 and 3200/2: second symbol of a pair
    std::vector<std::vector<uint32_t>> m_words;
    //! The same words from the decisions on the centre of the symbols (RadioPagingFskDemod::Symbol::m_centreValue)
    std::vector<std::vector<uint32_t>> m_centreWords;
    //! Reliability of each bit of m_words (bit index of the phase): distance of its symbol from the decision
    //! threshold of the bit, 100 = the outer level deviation, at most 255
    std::vector<std::vector<uint8_t>> m_reliability;
    std::vector<double> m_blockPower;
    std::vector<int> m_blockSamples;
    double m_errorSum = 0.0;
    int m_errorCount = 0;
    double m_recentError2 = 0.0;        //!< Of the data symbols: exponential average of their squared error (32 symbols)

    uint64_t m_syncSymbols;             //!< Symbols of the sync demodulator
    uint64_t m_ersFirst;                //!< Symbol at which the current or last re-synchronization started
    uint64_t m_ersLast;                 //!< Symbol of its last cycle
    uint32_t m_ersDetections;

    uint32_t m_syncs;
    uint32_t m_frames;
    uint32_t m_frameInfoFailures;
    unsigned m_lastCycle;
    unsigned m_lastFrame;
    float m_lastQuality;
    bool m_fiwKnown;                    //!< m_fiwIndex and m_fiwTime hold the last frame information word decoded
    unsigned m_fiwIndex;                //!< Its cycle * 128 + frame
    double m_fiwTime;                   //!< When it was received (m_time)

    std::deque<RadioPagingFlexDecoder::Message> m_messages;
    // The window of the message history of the decoder: 960 frames
    RadioPagingDedup m_dedup{"flex", 1800.0};
    double m_time = 0.0;                //!< Seconds of signal received, for the duplicate window

    void syncSymbol(const RadioPagingFskDemod::Symbol& symbol, float magsq);
    bool frameInfoInSequence(uint32_t raw, uint32_t& corrected) const;
    bool checkSync(RadioPagingFlex::Mode& mode, bool& inverted) const;
    bool checkErs() const;
    bool ersActive() const;
    void fitSync1(RadioPagingFlex::Mode mode, bool inverted);
    bool m_levelsMeasured = false;
    float m_measuredDc = 0.0f;
    void startData(float offset);
    void dataSymbol(int symbol, float error, float soft, int centre, float magsq);
    void sync2Symbol(int symbol, float soft, int centre);
    void storeDataSymbol(int symbol, float soft, int centre, float magsq);
    void endFrame(bool partial = false);
    void backToHunting(bool keepHistory = false);
    int mirror(int symbol, int levels) const { return m_inverted ? (levels - 1 - symbol) : symbol; }
};

#endif // MODEMRADIOPAGING_RADIOPAGINGFLEXRECEIVER_H
