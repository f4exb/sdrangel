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

#ifndef INCLUDE_PAGERDEMODFLEX_H
#define INCLUDE_PAGERDEMODFLEX_H

#include <QHash>
#include <QList>
#include <QString>

#include <vector>

// FLEX paging protocol receiver.
//
// A FLEX frame is 1.875s long: sync 1 and the frame information word (FIW) at 1600 bit/s
// 2-level FSK, then sync 2 and 11 blocks of data at the frame's own symbol rate (1600 or
// 3200 baud) and number of levels (2 or 4). This gives four modes: 1600/2, 1600/4, 3200/2
// and 3200/4, carrying 1, 2, 2 and 4 interleaved phases (A to D) of 88 codewords each.
//
// Codewords are BCH(31,21) plus even parity, the same code as POCSAG, but transmitted LSB
// first. Field definitions follow ARIB RCR STD-43A; message layout and the handling of
// corner cases follow multimon-ng's demod_flex.c, so decodes are comparable with it.

// A message decoded from a FLEX frame
struct PagerDemodFlexMessage
{
    // Vector types, as transmitted
    enum Type {
        Secure = 0,
        ShortInstruction = 1,
        Tone = 2,
        StandardNumeric = 3,
        SpecialNumeric = 4,
        Alphanumeric = 5,
        Binary = 6,
        NumberedNumeric = 7
    };

    qint64 m_capcode = 0;
    bool m_longAddress = false;
    Type m_type = Alphanumeric;
    QString m_text;             //!< 7-bit characters (alphanumeric, secure), digits (numeric, tone) or hex (binary)
    int m_bchErrors = 0;        //!< Uncorrectable codewords in the message
    int m_parityErrors = 0;     //!< Corrected codewords failing even parity
    int m_bitRate = 0;          //!< Data bit rate of the frame: 1600, 3200 or 6400
    int m_cycle = 0;            //!< Cycle number (0-14) of the first fragment
    int m_frame = 0;            //!< Frame number (0-127) of the first fragment
    char m_phase = 'A';
    bool m_incomplete = false;  //!< One or more fragments of a multi-frame message were lost
    int m_group = -1;           //!< Temporary address (0-15) of a group call, or -1

    bool isNumeric() const;     //!< Whether m_text holds digits rather than characters
    static QString typeName(Type type);
};

// Decodes the codewords of a received frame into messages, including reassembly of
// alphanumeric messages that are fragmented over several frames.
//
// Group calls: a short instruction vector assigns a pager's capcode to one of 16 temporary
// addresses, for a given frame. A message sent to that temporary address in that frame is
// for every pager assigned to it, so it is output once for each of their capcodes. If no
// assignments were received, it is output with the temporary address as the capcode.
class PagerDemodFlexDecoder
{
public:
    static const int m_wordsPerPhase = 88;
    static const int m_wordsPerBlock = 8;
    static const int m_framesPerCycle = 128;
    static const int m_cycles = 15;
    //!< A fragmented message whose next fragment hasn't arrived after this many frames
    //!< (one minute) is output marked incomplete
    static const int m_fragmentTimeoutFrames = 32;
    //!< Capcodes of the temporary addresses used for group calls
    static const qint64 m_groupCapcodeFirst = 2029568;
    static const int m_groups = 16;
    static const int m_maxGroupMembers = 1000;

    struct Phase {
        char m_name = 'A';
        quint32 m_words[m_wordsPerPhase] = {};  //!< Received codewords, first received bit in bit 0
        bool m_erased[m_wordsPerPhase] = {};    //!< Codeword received while the carrier was lost
    };

    struct Frame {
        int m_cycle = 0;
        int m_frame = 0;
        int m_bitRate = 1600;
        std::vector<Phase> m_phases;
    };

    //!< Decode a frame. Returns completed messages, including fragmented messages that timed out
    QList<PagerDemodFlexMessage> decodeFrame(const Frame& frame);
    //!< Output any partly received fragmented messages, marked incomplete
    QList<PagerDemodFlexMessage> flush();
    bool hasFragments() const { return !m_fragments.isEmpty(); }
    void reset();

    // Codeword statistics for the last decoded frame
    int getCodewords() const { return m_codewords; }
    int getUncorrectable() const { return m_uncorrectable; }

    //!< The 4-bit checksum used by the FIW, BIW and vector words: nibble sum of the 21 data bits is 0xF
    static bool checksumValid(quint32 data);
    //!< Set the checksum bits (3-0) of a 21-bit data word, so checksumValid() is true
    static quint32 setChecksum(quint32 data);
    //!< Correct a received codeword (first received bit in bit 0). Returns false if uncorrectable
    static bool correct(quint32 word, quint32& data, bool& parityError);
    //!< Encode 21 data bits as a codeword, first transmitted bit in bit 0
    static quint32 encode(quint32 data);
    static bool isLongAddress(quint32 addressWord);

    static const char m_numericChars[17];

private:
    struct Fragment {
        PagerDemodFlexMessage m_message;
        int m_messageNumber;    //!< From the header, to tell parts of different messages apart
        int m_nextFragment;     //!< Expected fragment number of the next part
        int m_frameIndex;       //!< Absolute frame index (cycle * 128 + frame) of the last part
    };

    struct Group {
        QList<qint64> m_capcodes;   //!< Capcodes assigned to the temporary address
        int m_frameIndex = -1;      //!< Absolute frame index of the group call, -1 if none
    };

    QHash<qint64, Fragment> m_fragments;  //!< Fragmented messages being reassembled, by capcode
    Group m_groupCalls[m_groups];
    int m_codewords = 0;
    int m_uncorrectable = 0;

    void decodePhase(const Frame& frame, const Phase& phase, QList<PagerDemodFlexMessage>& messages);
    void addFragment(PagerDemodFlexMessage& message, int fragment, bool continued, int messageNumber, int frameIndex, QList<PagerDemodFlexMessage>& messages);
    void assignGroup(const Frame& frame, qint64 capcode, quint32 viw);
    void expireGroups(int frameIndex);
    QList<PagerDemodFlexMessage> expandGroups(const QList<PagerDemodFlexMessage>& messages);
};

// Recovers FLEX frames from FM discriminator output and passes them to the decoder.
//
// Sync 1 is found by a 64-bit correlator on each sample phase of the 1600 bit/s stream. Of
// the phases that match, the one with the largest matched filter output over the sync code
// gives the symbol timing, and the DC offset and
// deviation are measured from sync 1 and the FIW. Data symbols are then tracked by a timing
// loop that uses transitions of at least two levels, and the slicer levels are tracked by
// decision direction, so a frame survives receiver clock and carrier frequency errors.
//
// Frames are sent every 1.875s, so once a frame has been received, the flywheel predicts
// when the next sync 1 should end. Around that time, a much weaker match to the expected
// sync code is accepted, for frames where sync 1 is damaged (e.g. by a transmitter keying
// up). To keep this from creating frames from noise, such a frame is only received if its
// FIW has the expected cycle and frame number.
class PagerDemodFlex
{
public:
    // Information about a received frame, for diagnostics and tests
    struct FrameInfo {
        qint64 m_syncSample = 0;    //!< Sample index of the end of the sync 1 code
        quint16 m_code = 0;         //!< Sync 1 mode code
        bool m_inverted = false;    //!< Whether the sync was received with inverted polarity
        int m_baud = 0;
        int m_levels = 0;
        int m_cycle = 0;
        int m_frame = 0;
        int m_codewords = 0;        //!< Codewords in the frame (88 per phase)
        int m_uncorrectable = 0;    //!< Codewords with uncorrectable errors (not counting erasures)
        int m_erasedBlocks = 0;     //!< Blocks where the carrier was lost
        float m_deviation = 0.0f;   //!< Measured outer level deviation, in discriminator units
        float m_offset = 0.0f;      //!< Measured DC offset, in discriminator units
        bool m_flywheel = false;    //!< Sync 1 was found by the flywheel
    };

    explicit PagerDemodFlex(int sampleRate = 38400);
    void reset();
    //!< Process one sample of FM discriminator output, with its magnitude squared
    void process(float fm, float magsq);
    //!< True from finding sync 1 to the end of the frame
    bool isReceiving() const { return m_state != Hunting; }
    bool hasMessages() const { return !m_messages.isEmpty(); }
    PagerDemodFlexMessage takeMessage() { return m_messages.takeFirst(); }
    int getFrameCount() const { return m_frameCount; }   //!< Frames whose FIW decoded
    const FrameInfo& getLastFrame() const { return m_lastFrame; }

    //!< Blocks whose mean power is this far below sync 1 are treated as lost
    static constexpr float m_carrierLossDB = 15.0f;
    //!< Frame periods after the last frame received that the flywheel predicts sync for
    static const int m_flywheelFrames = 4;
    //!< Bit errors allowed in the 64-bit sync code, when found by the flywheel
    static const int m_flywheelSyncErrors = 16;

private:
    enum State {
        Hunting,    //!< Searching for sync 1
        FIW,        //!< Receiving the rest of sync 1 and the FIW at 1600 bit/s
        Data        //!< Receiving sync 2 and the data blocks
    };

    static const int m_syncBits = 64;
    static const int m_syncTailBits = 16;       //!< Bits after the 64-bit sync code, before the FIW
    static const int m_fiwBits = 32;
    static const int m_blocks = 11;

    int m_sampleRate;
    int m_sps1600;              //!< Samples per symbol at 1600 baud
    State m_state;
    qint64 m_sampleCount;

    // History of FM samples and running sums, for the matched filters and the DC estimate
    std::vector<float> m_fm;
    std::vector<float> m_magsq;
    std::vector<float> m_mfHist;        //!< 1600 baud matched filter output
    int m_histMask;
    double m_sum1600;           //!< Over one symbol at 1600 baud
    double m_sum3200;           //!< Over one symbol at 3200 baud
    double m_sumSync;           //!< Over the 64-bit sync code
    double m_sumMagsqSync;
    float m_mf1600;             //!< Matched filter outputs (mean over a symbol)
    float m_mf3200;

    // Sync 1 search
    std::vector<quint64> m_syncReg; //!< One shift register per sample phase
    bool m_candidate;
    bool m_candidateFlywheel;   //!< Candidate is the flywheel's search window, rather than a sync match
    qint64 m_candidateFirst;
    qint64 m_candidateEnd;
    // Per sample of the candidate
    std::vector<float> m_candidateMetric;   //!< Matched filter output over the sync code. 0 if no match
    std::vector<quint16> m_candidateCode;   //!< Mode code, corrected
    std::vector<bool> m_candidateInverted;
    std::vector<float> m_candidateDC;
    std::vector<float> m_candidatePower;

    // Flywheel
    bool m_flywheel;            //!< Next sync is predicted
    double m_flywheelSyncEnd;   //!< Predicted end of the next sync code
    int m_flywheelPeriods;      //!< Frame periods since the last frame
    quint16 m_flywheelCode;
    bool m_flywheelInverted;
    int m_flywheelCycle;        //!< Expected cycle and frame of the predicted frame
    int m_flywheelFrame;

    // Current frame
    FrameInfo m_info;
    double m_syncEnd;           //!< Sample at which the last bit of the sync code ends
    float m_polarity;           //!< +1 or -1, so the highest level is positive
    float m_dc;
    float m_deviation;
    float m_syncPower;
    int m_bitIndex;             //!< Bit counter for sync tail and FIW
    double m_nextBitSample;
    double m_deviationSum;
    quint32 m_fiw;

    // Data symbol recovery
    int m_sps;                  //!< Samples per symbol
    double m_timing;            //!< Samples until the next symbol instant
    bool m_gotMid;
    float m_mid;                //!< Matched filter output midway between symbols
    float m_prevSymbol;
    bool m_havePrevSymbol;
    float m_prevMf;
    int m_symbolIndex;
    int m_sync2Symbols;
    int m_dataSymbols;
    double m_blockPower[m_blocks];
    int m_blockSamples[m_blocks];
    PagerDemodFlexDecoder::Frame m_frame;

    PagerDemodFlexDecoder m_decoder;
    QList<PagerDemodFlexMessage> m_messages;
    qint64 m_lastFrameSample;   //!< When the last frame ended, to time out fragments if transmission stops
    int m_frameCount;
    FrameInfo m_lastFrame;

    static constexpr float m_timingGain = 0.05f;    //!< Fraction of the timing error corrected per transition
    static constexpr float m_levelGain = 0.01f;     //!< Slicer level tracking per symbol

    void hunt(qint64 n, quint64 reg, float dc);
    void flywheelSample(qint64 n, quint64 reg, float dc);
    void openCandidate(qint64 first, qint64 end, bool flywheel);
    void startFrame();
    void fiwBit(float mf);
    void dataSample(float mf, float magsq);
    void symbol(float value);
    void endFrame();
    static bool syncMatch(quint64 reg, quint16& code);
    //!< Find the mode closest to a received code. Returns false if none is within 3 bits
    static bool mode(quint16& code, int& baud, int& levels);
    static int popcount64(quint64 x);
};

#endif // INCLUDE_PAGERDEMODFLEX_H
