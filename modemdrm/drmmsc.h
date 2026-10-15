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

#ifndef MODEMDRM_DRMMSC_H_
#define MODEMDRM_DRMMSC_H_

#include <complex>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "export.h"
#include "drmchannelestimator.h"
#include "drmparameters.h"
#include "drmsdc.h"

namespace modemdrm
{

struct MODEMDRM_API MSCFrameCells
{
    int frameInSuperframe = -1;
    std::vector<std::complex<float>> cells;
    std::vector<float> reliabilities;
};

struct MODEMDRM_API MSCSuperframeCells
{
    std::vector<std::complex<float>> cells;
    std::vector<float> reliabilities;
};

struct MODEMDRM_API MSCMultiplexFrameCells
{
    int frameInSuperframe = -1;
    std::vector<std::complex<float>> cells;
    std::vector<float> reliabilities;
};

struct MODEMDRM_API MSCCapacity
{
    int availableCellsPerSuperframe = 0;
    int usefulCellsPerSuperframe = 0;
    int cellsPerMultiplexFrame = 0;
    int dummyCellsPerSuperframe = 0;
};

class MODEMDRM_API MSCExtractor
{
public:
    static bool extract(
        Mode mode,
        int occupancy,
        int frameInSuperframe,
        const std::vector<EqualizedSymbol>& symbols,
        MSCFrameCells& output,
        std::string* error = nullptr
    );
};

class MODEMDRM_API MSCSuperframeAssembler
{
public:
    static bool assemble(
        Mode mode,
        int occupancy,
        const std::vector<MSCFrameCells>& frames,
        MSCSuperframeCells& output,
        std::string* error = nullptr
    );
};

class MODEMDRM_API MSCMultiplexFrameSplitter
{
public:
    // ETSI ES 201 980 V4.3.1, tables 41-45. Dummy cells are appended after
    // the useful multiplex-frame cells and are not channel-decoder input.
    static bool capacity(Mode mode, int occupancy, MSCCapacity& result);
    static bool split(
        Mode mode,
        int occupancy,
        const MSCSuperframeCells& superframe,
        std::vector<MSCMultiplexFrameCells>& frames,
        std::string* error = nullptr
    );
};

class MODEMDRM_API MSCCellInterleaver
{
public:
    // Reverses the mandatory multiplier-5 block permutation within one
    // multiplex frame (clause 7.6). Long interleaving adds a separate
    // convolutional frame-selection stage around this permutation.
    static bool deinterleaveBlock(
        const MSCMultiplexFrameCells& input,
        MSCMultiplexFrameCells& output,
        std::string* error = nullptr
    );
};

class MODEMDRM_API MSCCellDeinterleaver
{
public:
    explicit MSCCellDeinterleaver(Mode mode, bool longInterleaving);

    void reset();
    int depth() const;
    std::size_t bufferedFrameCount() const;

    // Accepts consecutive transmitted multiplex frames. outputReady remains
    // false during the D-1 frame fill interval of long interleaving.
    bool push(
        const MSCMultiplexFrameCells& input,
        MSCMultiplexFrameCells& output,
        bool& outputReady,
        std::string* error = nullptr
    );

private:
    struct PendingFrame
    {
        MSCMultiplexFrameCells frame;
        std::vector<bool> received;
        std::size_t filled = 0;
    };

    Mode m_mode;
    bool m_longInterleaving;
    std::uint64_t m_inputFrameCount = 0;
    int m_firstFramePhase = -1;
    std::size_t m_cellCount = 0;
    std::map<std::int64_t, PendingFrame> m_pending;
};

struct MODEMDRM_API MSCDecodedMultiplexFrame
{
    std::vector<std::uint8_t> bits;
    int trailingBitCount = 0;
};

struct MODEMDRM_API MSCProtectionParameters
{
    int protectionLevelA = 0;
    int protectionLevelB = 0;
    std::size_t partABytes = 0;
};

class MODEMDRM_API MSCChannelDecoder
{
public:
    // Supported MSC profiles: mode-E 4-/16-QAM and A-D 16-/64-QAM with equal
    // or unequal error protection (FAC MSC modes 3/0), tables 29-32 and 41-45.
    static bool supports(Mode mode, int mscMode, bool unequalProtection);
    static int informationBitCount(
        Mode mode,
        int occupancy,
        int mscMode,
        int protectionLevel
    );
    static int informationBitCount(
        Mode mode,
        int occupancy,
        int mscMode,
        const MSCProtectionParameters& protection
    );
    static bool decode(
        Mode mode,
        int occupancy,
        int mscMode,
        int protectionLevel,
        const MSCMultiplexFrameCells& input,
        MSCDecodedMultiplexFrame& output,
        std::string* error = nullptr
    );
    static bool decode(
        Mode mode,
        int occupancy,
        int mscMode,
        const MSCProtectionParameters& protection,
        const MSCMultiplexFrameCells& input,
        MSCDecodedMultiplexFrame& output,
        std::string* error = nullptr
    );
};

struct MODEMDRM_API MSCLogicalStreamFrame
{
    int streamId = -1;
    std::vector<std::uint8_t> partA;
    std::vector<std::uint8_t> partB;
};

class MODEMDRM_API MSCMultiplexParser
{
public:
    static bool extractStreams(
        const MSCDecodedMultiplexFrame& multiplex,
        const SDCMultiplexDescription& description,
        std::vector<MSCLogicalStreamFrame>& streams,
        std::string* error = nullptr
    );
};

} // namespace modemdrm

#endif // MODEMDRM_DRMMSC_H_
