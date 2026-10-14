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

#ifndef MODEMDRM_DRMAUDIOFRAMING_H_
#define MODEMDRM_DRMAUDIOFRAMING_H_

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "export.h"
#include "drmparameters.h"
#include "drmsdc.h"

namespace modemdrm
{

struct MODEMDRM_API AACSuperframe
{
    int bitReservoirLevel = -1;
    int invalidFrameCount = 0;
    std::vector<std::uint16_t> frameBorders;
    std::vector<std::uint8_t> crcBytes;
    std::vector<std::uint16_t> frameCRCWords;
    std::vector<std::vector<std::uint8_t>> accessUnits;
    // TT_DRM/DRM-aware codec backends consume the transmitted CRC byte
    // followed by the corresponding AAC access-unit payload.
    std::vector<std::vector<std::uint8_t>> codecFrames;
    // Transmission order, indexing codecFrames; -1 is a CRC-rejected xHE
    // access unit that must still consume one codec-frame interval.
    std::vector<int> codecFrameSequence;
};

class MODEMDRM_API XHEAACSuperframeParser
{
public:
    // ES 201 980 5.3.1.3: at most 6,144 bits per channel. Allow stereo,
    // the transmitted CRC-16, and two bytes awaiting a delayed border.
    static constexpr std::size_t MaximumAccessUnitBytes = 2U * (6144U / 8U) + 2U;
    static constexpr std::size_t MaximumPendingBytes = MaximumAccessUnitBytes + 2U;
    void reset();
    bool push(
        const std::vector<std::uint8_t>& bytes,
        AACSuperframe& output,
        std::string* error = nullptr
    );

    // Concealment is whole codec frames: the time given beyond the audio
    // superframes lost so far, taken from the next loss's. Kept by reset(),
    // which every loss calls.
    double concealedSurplusSeconds = 0.0;

private:
    bool finishFrame(AACSuperframe& output);

    bool m_haveFrameStart = false;
    std::vector<std::uint8_t> m_pendingFrame;
};

struct MODEMDRM_API DRMTextMessage
{
    std::string utf8;
    int textControl = 0;
    bool clearDisplay = false;
};

class MODEMDRM_API DRMTextMessageDecoder
{
public:
    void reset();
    // The service decoded for: another one starts again, before its text is
    // decoded. Returns true when that dropped another service's text.
    bool setService(std::int64_t serviceIdentifier);
    bool push(
        const std::array<std::uint8_t, 4>& piece,
        DRMTextMessage& output,
        bool& outputReady,
        std::string* error = nullptr
    );

private:
    // A message of up to eight segments, kept by number across repetitions
    // while the toggle bit is unchanged, so a lost segment is filled from a
    // later repetition (as Dream does).
    struct Segment
    {
        bool valid = false;
        std::vector<std::uint8_t> body;
    };
    void clearMessage();

    std::int64_t m_service = -1;
    bool m_receivingSegment = false;
    bool m_haveToggle = false;
    bool m_messageToggle = false;
    int m_segmentCount = 0; // From the segment with the last flag; 0 until seen.
    int m_textControl = 0;
    bool m_emitted = false;
    std::size_t m_segmentLength = 0;
    std::vector<std::uint8_t> m_segment;
    std::array<Segment, 8> m_segments;
};

class MODEMDRM_API AACSuperframeParser
{
public:
    static int frameCount(Mode mode, const SDCAudioInformation& audio);
    static bool parse(
        Mode mode,
        const SDCAudioInformation& audio,
        const std::vector<std::uint8_t>& bytes,
        AACSuperframe& output,
        std::string* error = nullptr,
        // Bytes of the superframe carried in part A (UEP); 0 for EEP.
        std::size_t higherProtectedBytes = 0
    );
};

class MODEMDRM_API ModeEAudioSuperframeAssembler
{
public:
    void reset();
    bool push(
        int frameInSuperframe,
        const std::vector<std::uint8_t>& logicalFrame,
        std::vector<std::uint8_t>& audioSuperframe,
        bool& outputReady,
        std::string* error = nullptr,
        // Leading bytes of the logical frame from part A (UEP); 0 for EEP.
        std::size_t partABytes = 0
    );

private:
    int m_expectedFramePhase = -1;
    std::vector<std::uint8_t> m_firstHalf;
    std::size_t m_firstPartABytes = 0;
};

} // namespace modemdrm

#endif // MODEMDRM_DRMAUDIOFRAMING_H_
