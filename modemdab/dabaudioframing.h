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

#ifndef MODEMDAB_DABAUDIOFRAMING_H_
#define MODEMDAB_DABAUDIOFRAMING_H_

#include <cstdint>
#include <deque>
#include <vector>

#include "export.h"

namespace modemdab
{

// Audio parameters of a DAB+ superframe (TS 102 563 clause 5.2)
struct MODEMDAB_API DABPlusParameters
{
    bool dac48 = false;         // 48 kHz output, else 32 kHz
    bool sbr = false;
    bool stereo = false;        // AAC core channel mode
    bool ps = false;
    int mpegSurround = 0;
    int numAUs = 0;

    int outputSampleRate() const { return dac48 ? 48000 : 32000; }
    int coreSampleRate() const { return sbr ? outputSampleRate() / 2 : outputSampleRate(); }
    bool operator==(const DABPlusParameters& o) const;
    bool operator!=(const DABPlusParameters& o) const { return !(*this == o); }
};

struct MODEMDAB_API DABAccessUnit
{
    std::vector<uint8_t> data;  // Without its CRC
    bool valid = false;         // CRC passed
};

// One decoded superframe: up to six AUs of 960 samples (with SBR, 1920 output samples).
struct MODEMDAB_API DABPlusSuperframe
{
    DABPlusParameters parameters;
    std::vector<DABAccessUnit> accessUnits;
    int rsCorrectedBytes = 0;
    int rsUncorrectableRows = 0;
};

// DAB+ audio superframes (TS 102 563): five logical frames, Reed-Solomon RS(120, 110) with
// virtual interleaving, the Fire code protected header and AUs with CRCs.
class MODEMDAB_API DABPlusFramer
{
public:
    DABPlusFramer();

    void reset();
    // A logical frame of the sub-channel (bitrate * 3 bytes), or a lost one (data null).
    // Returns true when a superframe was decoded into superframe.
    bool push(const uint8_t *data, int size, DABPlusSuperframe& superframe);

    bool synchronized() const { return m_synchronized; }
    // Counters since reset
    int superframes() const { return m_superframes; }
    int fireCodeErrors() const { return m_fireCodeErrors; }
    int rsCorrectedBytes() const { return m_rsCorrected; }
    int rsUncorrectableRows() const { return m_rsUncorrectable; }
    int auCount() const { return m_auCount; }
    int auErrors() const { return m_auErrors; }
    const DABPlusParameters& parameters() const { return m_parameters; }

    // PAD of an AU (TS 102 563 clause 5.4.3): the payload of a data_stream_element() at the
    // start of the AU. Returns false if there is none.
    static bool extractPAD(const std::vector<uint8_t>& au, const uint8_t *&pad, int& padLength);

private:
    std::deque<std::vector<uint8_t>> m_frames;
    std::deque<bool> m_framesLost;
    bool m_synchronized;
    int m_phase;                // Frames buffered towards the next superframe
    int m_failures;
    DABPlusParameters m_parameters;
    int m_superframes;
    int m_fireCodeErrors;
    int m_rsCorrected;
    int m_rsUncorrectable;
    int m_auCount;
    int m_auErrors;

    bool decode(std::vector<uint8_t>& superframe, int& corrected, int& uncorrectable) const;
    bool parse(const std::vector<uint8_t>& superframe, DABPlusSuperframe& out);
};

// DAB audio (MPEG-1 and MPEG-2 LSF Layer II) frames (TS 103 466).
struct MODEMDAB_API DABMP2Frame
{
    std::vector<uint8_t> data;  // The whole DAB audio frame
    bool valid = false;         // Header CRC passed
    int sampleRate = 0;         // 48000 or 24000
    int channels = 0;
    int bitrate = 0;
    int scfCRCLength = 0;       // Bytes of ScF-CRC before the F-PAD
};

class MODEMDAB_API DABMP2Framer
{
public:
    DABMP2Framer();

    void reset();
    // A logical frame of the sub-channel (bitrate * 3 bytes), or a lost one (data null).
    // Returns true when a frame is output.
    bool push(const uint8_t *data, int size, int bitrate, DABMP2Frame& frame);

    int frames() const { return m_frames; }
    int crcErrors() const { return m_crcErrors; }

    // Parse and check a frame's header; true if its CRC passes.
    static bool check(const uint8_t *data, int size, DABMP2Frame& frame);

private:
    std::vector<uint8_t> m_pending;     // First half of a 24 kHz frame
    bool m_pendingLost;
    int m_frames;
    int m_crcErrors;
};

} // namespace modemdab

#endif // MODEMDAB_DABAUDIOFRAMING_H_
