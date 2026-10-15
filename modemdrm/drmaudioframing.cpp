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

#include "drmaudioframing.h"

#include "drmbitutils.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace modemdrm
{

namespace
{

class ByteBitReader
{
public:
    explicit ByteBitReader(const std::vector<std::uint8_t>& bytes) : m_bytes(bytes) {}

    bool read(int count, std::uint32_t& value)
    {
        if (count < 0 || count > 32
            || m_bitOffset + static_cast<std::size_t>(count) > 8U * m_bytes.size()) {
            return false;
        }

        value = 0;
        for (int i = 0; i < count; ++i)
        {
            const std::size_t byte = m_bitOffset / 8U;
            const int shift = 7 - static_cast<int>(m_bitOffset % 8U);
            value = (value << 1U) | ((m_bytes[byte] >> shift) & 1U);
            ++m_bitOffset;
        }

        return true;
    }

    std::size_t bytesRead() const { return m_bitOffset / 8U; }

private:
    const std::vector<std::uint8_t>& m_bytes;
    std::size_t m_bitOffset = 0;
};

} // namespace

void DRMTextMessageDecoder::reset()
{
    m_receivingSegment = false;
    m_haveToggle = false;
    m_messageToggle = false;
    m_segmentLength = 0;
    m_segment.clear();
    clearMessage();
}

bool DRMTextMessageDecoder::setService(std::int64_t serviceIdentifier)
{
    const bool changed = (m_service >= 0) && (serviceIdentifier != m_service);

    if (changed) {
        reset();
    }

    m_service = serviceIdentifier;
    return changed;
}

void DRMTextMessageDecoder::clearMessage()
{
    for (Segment& segment : m_segments)
    {
        segment.valid = false;
        segment.body.clear();
    }

    m_segmentCount = 0;
    m_textControl = 0;
    m_emitted = false;
}

bool DRMTextMessageDecoder::push(
    const std::array<std::uint8_t, 4>& piece,
    DRMTextMessage& output,
    bool& outputReady,
    std::string* error
)
{
    outputReady = false;
    if (error) {
        error->clear();
    }

    if (std::all_of(piece.begin(), piece.end(), [](std::uint8_t byte) {
            return byte == 0xffU;
        }))
    {
        m_receivingSegment = true;
        m_segmentLength = 0;
        m_segment.clear();
        return true;
    }

    if (!m_receivingSegment) {
        return true;
    }

    m_segment.insert(m_segment.end(), piece.begin(), piece.end());
    if (m_segmentLength == 0 && m_segment.size() >= 2U)
    {
        const bool command = (m_segment[0] & 0x10U) != 0U;
        if (command)
        {
            const int commandCode = m_segment[0] & 0x0fU;
            if (commandCode != 1)
            {
                m_receivingSegment = false;
                m_segment.clear();
                return true;
            }

            m_segmentLength = 4U;
        }
        else
        {
            const std::size_t bodyLength = (m_segment[0] & 0x0fU) + 1U;
            m_segmentLength = 2U + bodyLength + 2U;
        }
    }

    const std::size_t paddedLength = (m_segmentLength + 3U) & ~std::size_t(3U);
    if (m_segmentLength == 0 || m_segment.size() < paddedLength) {
        return true;
    }

    m_receivingSegment = false;
    // The zero padding lies outside the CRC; like Dream, do not drop a
    // CRC-valid segment for a bit error in it.
    if (m_segment.size() != paddedLength)
    {
        if (error) {
            *error = "Invalid DRM text-message segment padding";
        }

        m_segment.clear();
        return false;
    }

    const std::size_t crcOffset = m_segmentLength - 2U;
    const std::uint16_t transmittedCRC = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(m_segment[crcOffset]) << 8U)
        | m_segment[crcOffset + 1U]
    );
    if (CRC::crc16Bytes(m_segment.data(), crcOffset) != transmittedCRC)
    {
        if (error) {
            *error = "DRM text-message segment CRC failed";
        }

        m_segment.clear();
        return false;
    }

    const bool toggle = (m_segment[0] & 0x80U) != 0U;
    const bool first = (m_segment[0] & 0x40U) != 0U;
    const bool last = (m_segment[0] & 0x20U) != 0U;
    const bool command = (m_segment[0] & 0x10U) != 0U;
    if (command)
    {
        // Only clear display (field 1 = 0001) reaches here. Field 2 should be
        // 1111 and field 3 is rfa; neither is checked, as in Dream.
        output = DRMTextMessage();
        output.clearDisplay = true;
        outputReady = true;
        clearMessage();
        m_segment.clear();
        return true;
    }

    // Field 2 is 1111 on the first segment, otherwise an rfa bit and SegNum
    // (0 reserved); field 3 of the first segment is kept as the text control.
    const int field2 = (m_segment[1] >> 4U) & 0x0fU;
    const int segmentNumber = first ? 0 : (field2 & 0x07);
    if (segmentNumber == 0 && !first)
    {
        if (error) {
            *error = "Invalid DRM text-message segment number";
        }

        m_segment.clear();
        return false;
    }

    if (!m_haveToggle || toggle != m_messageToggle)
    {
        // A new toggle value starts a new message.
        clearMessage();
        m_haveToggle = true;
        m_messageToggle = toggle;
    }

    const std::size_t bodyLength = (m_segment[0] & 0x0fU) + 1U;
    const std::vector<std::uint8_t> body(m_segment.begin() + 2,
        m_segment.begin() + static_cast<std::ptrdiff_t>(2U + bodyLength));
    // Another length under the same toggle (a last segment elsewhere, or a
    // segment beyond the last) is a new message, as is different content.
    if (m_segmentCount != 0 && (last ? segmentNumber + 1 != m_segmentCount : segmentNumber >= m_segmentCount)) {
        clearMessage();
    }

    Segment& stored = m_segments[static_cast<std::size_t>(segmentNumber)];
    if (stored.valid && stored.body != body) {
        clearMessage();
    }

    stored.valid = true;
    stored.body = body;
    if (first) {
        m_textControl = m_segment[1] & 0x0fU;
    }

    if (last) {
        m_segmentCount = segmentNumber + 1;
    }

    m_segment.clear();

    if (m_emitted || m_segmentCount == 0) {
        return true;
    }

    for (int index = 0; index < m_segmentCount; ++index)
    {
        if (!m_segments[static_cast<std::size_t>(index)].valid) {
            return true;
        }
    }

    output = DRMTextMessage();
    for (int index = 0; index < m_segmentCount; ++index)
    {
        const auto& part = m_segments[static_cast<std::size_t>(index)].body;
        output.utf8.append(part.begin(), part.end());
    }

    output.textControl = m_textControl;
    outputReady = true;
    m_emitted = true;
    return true;
}

int AACSuperframeParser::frameCount(Mode mode, const SDCAudioInformation& audio)
{
    if (audio.audioCoding != 0) {
        return -1;
    }

    if (mode == Mode::E)
    {
        if (audio.samplingRateCode == 3) {
            return 5;
        }

        if (audio.samplingRateCode == 5) {
            return 10;
        }
    }
    else
    {
        if (audio.samplingRateCode == 1) {
            return 5;
        }

        if (audio.samplingRateCode == 3) {
            return 10;
        }
    }

    return -1;
}

bool AACSuperframeParser::parse(
    Mode mode,
    const SDCAudioInformation& audio,
    const std::vector<std::uint8_t>& bytes,
    AACSuperframe& output,
    std::string* error,
    std::size_t higherProtectedBytes
)
{
    output = AACSuperframe();
    const int frames = frameCount(mode, audio);
    if (frames < 0)
    {
        if (error) {
            *error = "Unsupported DRM AAC sampling rate or codec";
        }

        return false;
    }

    const std::size_t frameTotal = static_cast<std::size_t>(frames);
    const std::size_t headerBytes = frames == 5 ? 6U : 14U;
    if (bytes.size() < headerBytes + frameTotal)
    {
        if (error) {
            *error = "Truncated DRM AAC superframe";
        }

        return false;
    }

    ByteBitReader reader(bytes);
    output.frameBorders.reserve(frameTotal - 1U);
    std::uint32_t value = 0;
    for (int i = 0; i < frames - 1; ++i)
    {
        if (!reader.read(12, value))
        {
            if (error) {
                *error = "Truncated DRM AAC frame-border directory";
            }

            return false;
        }

        output.frameBorders.push_back(static_cast<std::uint16_t>(value));
    }

    // The 10-frame header ends with 4 reserved bits. No CRC covers the header,
    // and each frame has its own, so ignore them as Dream does rather than
    // drop every frame for one bit error.
    if (frames == 10 && !reader.read(4, value))
    {
        if (error) {
            *error = "Truncated DRM AAC frame-border directory";
        }

        return false;
    }

    if (reader.bytesRead() != headerBytes)
    {
        if (error) {
            *error = "Invalid DRM AAC header alignment";
        }

        return false;
    }

    // Table 11 note 1: the borders index the audio payload, which excludes
    // the header and the CRC bytes.
    const std::size_t payloadLength = bytes.size() - headerBytes - frameTotal;
    std::vector<std::size_t> lengths;
    lengths.reserve(frameTotal);
    std::size_t previous = 0;
    for (std::uint16_t& signalledBorder : output.frameBorders)
    {
        // Table 11 note 2: borders above 4 095 bytes carry only their 12 LSBs.
        // Borders always increase, so restore the missing multiples of 4 096.
        std::size_t border = signalledBorder;
        while (border <= previous && border < payloadLength) {
            border += 4096U;
        }

        if (border <= previous || border >= payloadLength)
        {
            if (error) {
                *error = "Invalid DRM AAC frame-border order or range";
            }

            output = AACSuperframe();
            return false;
        }

        signalledBorder = static_cast<std::uint16_t>(border);
        lengths.push_back(border - previous);
        previous = border;
    }

    lengths.push_back(payloadLength - previous);

    // Clause 5.3.1.1: with UEP the higher-protected part holds the header,
    // then each frame's first bytes followed by its CRC; the rest of every
    // frame follows in frame order. Without audio bytes in part A (EEP) that
    // is the header, the CRCs, then the frames.
    const std::size_t sharedBytes = higherProtectedBytes > headerBytes + frameTotal
        ? (higherProtectedBytes - headerBytes - frameTotal) / frameTotal : 0U;
    // A frame shorter than its higher-protected share fills the rest of it
    // with zeros (as Dream's encoder does; a tiny frame in silence under
    // strong UEP), which the borders do not count: the last frame, the
    // remainder of the payload, carries that padding. It is then at least
    // its share long.
    std::size_t padding = 0;
    for (std::size_t frame = 0; frame + 1U < frameTotal; ++frame)
    {
        if (lengths[frame] < sharedBytes) {
            padding += sharedBytes - lengths[frame];
        }
    }

    if (lengths.back() < padding + sharedBytes)
    {
        if (error) {
            *error = "DRM AAC frame lengths exceed the superframe";
        }

        output = AACSuperframe();
        return false;
    }

    lengths.back() -= padding;
    output.crcBytes.reserve(frameTotal);
    output.accessUnits.reserve(frameTotal);
    std::size_t lowerPosition = headerBytes + frameTotal * (sharedBytes + 1U);
    for (std::size_t frame = 0; frame < frameTotal; ++frame)
    {
        const std::size_t higherPosition = headerBytes + frame * (sharedBytes + 1U);
        output.crcBytes.push_back(bytes[higherPosition + sharedBytes]);
        std::vector<std::uint8_t> accessUnit;
        accessUnit.reserve(lengths[frame]);
        const std::size_t higherBytes = std::min(lengths[frame], sharedBytes);
        accessUnit.insert(accessUnit.end(),
            bytes.begin() + static_cast<std::ptrdiff_t>(higherPosition),
            bytes.begin() + static_cast<std::ptrdiff_t>(higherPosition + higherBytes));
        const std::size_t lowerBytes = lengths[frame] - higherBytes;
        accessUnit.insert(accessUnit.end(),
            bytes.begin() + static_cast<std::ptrdiff_t>(lowerPosition),
            bytes.begin() + static_cast<std::ptrdiff_t>(lowerPosition + lowerBytes));
        lowerPosition += lowerBytes;
        output.accessUnits.push_back(std::move(accessUnit));
    }

    output.codecFrames.reserve(output.accessUnits.size());
    for (std::size_t i = 0; i < output.accessUnits.size(); ++i)
    {
        std::vector<std::uint8_t> codecFrame;
        codecFrame.reserve(1U + output.accessUnits[i].size());
        codecFrame.push_back(output.crcBytes[i]);
        codecFrame.insert(
            codecFrame.end(), output.accessUnits[i].begin(), output.accessUnits[i].end()
        );
        output.codecFrames.push_back(std::move(codecFrame));
        output.codecFrameSequence.push_back(static_cast<int>(i));
    }

    return true;
}

void XHEAACSuperframeParser::reset()
{
    m_haveFrameStart = false;
    m_pendingFrame.clear();
}

bool XHEAACSuperframeParser::finishFrame(AACSuperframe& output)
{
    if (m_pendingFrame.size() < 2U)
    {
        ++output.invalidFrameCount;
        output.codecFrameSequence.push_back(-1);
        m_pendingFrame.clear();
        return false;
    }

    const std::size_t payloadSize = m_pendingFrame.size() - 2U;
    const std::uint16_t transmittedCRC = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(m_pendingFrame[payloadSize]) << 8U)
        | m_pendingFrame[payloadSize + 1U]
    );
    if (CRC::crc16Bytes(m_pendingFrame.data(), payloadSize) != transmittedCRC)
    {
        ++output.invalidFrameCount;
        output.codecFrameSequence.push_back(-1);
        m_pendingFrame.clear();
        return false;
    }

    output.frameCRCWords.push_back(transmittedCRC);
    output.accessUnits.emplace_back(
        m_pendingFrame.begin(),
        m_pendingFrame.begin() + static_cast<std::ptrdiff_t>(payloadSize)
    );
    output.codecFrameSequence.push_back(static_cast<int>(output.codecFrames.size()));
    output.codecFrames.push_back(m_pendingFrame);
    m_pendingFrame.clear();
    return true;
}

bool XHEAACSuperframeParser::push(
    const std::vector<std::uint8_t>& bytes,
    AACSuperframe& output,
    std::string* error
)
{
    output = AACSuperframe();
    if (error) {
        error->clear();
    }

    if (bytes.size() < 2U)
    {
        if (error) {
            *error = "Truncated DRM xHE-AAC superframe header";
        }

        return false;
    }

    const int borderCount = bytes[0] >> 4U;
    output.bitReservoirLevel = bytes[0] & 0x0fU;
    if (CRC::crc8Bytes(bytes.data(), 1U) != bytes[1])
    {
        if (error) {
            *error = "DRM xHE-AAC fixed-header CRC failed";
        }

        return false;
    }

    const std::size_t directorySize = 2U * static_cast<std::size_t>(borderCount);
    if (bytes.size() < 2U + directorySize)
    {
        if (error) {
            *error = "Truncated DRM xHE-AAC frame directory";
        }

        return false;
    }

    const std::size_t directoryOffset = bytes.size() - directorySize;
    const std::vector<std::uint8_t> payload(
        bytes.begin() + 2,
        bytes.begin() + static_cast<std::ptrdiff_t>(directoryOffset)
    );

    const auto appendPayload = [&](std::size_t begin, std::size_t end, std::size_t limit) {
        if (m_pendingFrame.size() > limit || end - begin > limit - m_pendingFrame.size())
        {
            reset();
            output = AACSuperframe();
            if (error) {
                *error = "DRM xHE-AAC access unit exceeds the transport size limit";
            }

            return false;
        }

        m_pendingFrame.insert(m_pendingFrame.end(),
            payload.begin() + static_cast<std::ptrdiff_t>(begin),
            payload.begin() + static_cast<std::ptrdiff_t>(end));
        return true;
    };

    std::vector<std::uint16_t> reverseBorders;
    reverseBorders.reserve(static_cast<std::size_t>(borderCount));
    for (int entry = 0; entry < borderCount; ++entry)
    {
        const std::size_t offset = directoryOffset
            + 2U * static_cast<std::size_t>(entry);
        const std::uint16_t description = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(bytes[offset]) << 8U)
            | bytes[offset + 1U]
        );
        if ((description & 0x0fU) != static_cast<std::uint16_t>(borderCount))
        {
            if (error) {
                *error = "Inconsistent DRM xHE-AAC frame-border count";
            }

            return false;
        }

        reverseBorders.push_back(description >> 4U);
    }

    output.frameBorders.assign(reverseBorders.rbegin(), reverseBorders.rend());

    std::size_t cursor = 0;
    bool sawNormalBorder = false;
    for (std::uint16_t border : output.frameBorders)
    {
        if (border >= 0x0ffeU)
        {
            // Without the previous superframe (a cold start or a reset) the
            // access unit starting in its tail is lost, but those after the
            // next normal border are not: skip the entry.
            if (!sawNormalBorder && !m_haveFrameStart) {
                continue;
            }

            if (sawNormalBorder)
            {
                if (error) {
                    *error = "Invalid delayed DRM xHE-AAC frame border";
                }

                return false;
            }

            const std::size_t retainedBytes = border == 0x0fffU ? 1U : 2U;
            if (m_pendingFrame.size() < retainedBytes)
            {
                if (error) {
                    *error = "Missing prior DRM xHE-AAC payload tail";
                }

                return false;
            }

            if (m_pendingFrame.size() - retainedBytes > MaximumAccessUnitBytes)
            {
                reset();
                output = AACSuperframe();
                if (error) {
                    *error = "DRM xHE-AAC access unit exceeds the transport size limit";
                }

                return false;
            }

            std::vector<std::uint8_t> nextFrame(
                m_pendingFrame.end() - static_cast<std::ptrdiff_t>(retainedBytes),
                m_pendingFrame.end()
            );
            m_pendingFrame.resize(m_pendingFrame.size() - retainedBytes);
            finishFrame(output);
            m_pendingFrame = std::move(nextFrame);
            m_haveFrameStart = true;
            continue;
        }

        if (border >= payload.size() || (sawNormalBorder && border <= cursor))
        {
            if (error) {
                *error = "Invalid DRM xHE-AAC frame-border order or range";
            }

            return false;
        }

        if (m_haveFrameStart)
        {
            if (!appendPayload(cursor, border, MaximumAccessUnitBytes)) {
                return false;
            }

            finishFrame(output);
        }

        m_haveFrameStart = true;
        cursor = border;
        sawNormalBorder = true;
    }

    if (m_haveFrameStart)
    {
        if (!appendPayload(cursor, payload.size(), MaximumPendingBytes)) {
            return false;
        }
    }

    return true;
}

void ModeEAudioSuperframeAssembler::reset()
{
    m_expectedFramePhase = -1;
    m_firstHalf.clear();
    m_firstPartABytes = 0;
}

bool ModeEAudioSuperframeAssembler::push(
    int frameInSuperframe,
    const std::vector<std::uint8_t>& logicalFrame,
    std::vector<std::uint8_t>& audioSuperframe,
    bool& outputReady,
    std::string* error,
    std::size_t partABytes
)
{
    outputReady = false;
    if (frameInSuperframe < 0 || frameInSuperframe >= 4 || logicalFrame.empty())
    {
        if (error) {
            *error = "Invalid DRM mode-E audio logical frame";
        }

        return false;
    }

    if (m_expectedFramePhase >= 0 && frameInSuperframe != m_expectedFramePhase)
    {
        if (error) {
            *error = "Discontinuous DRM mode-E audio frame sequence";
        }

        return false;
    }

    if (partABytes > logicalFrame.size())
    {
        if (error) {
            *error = "DRM mode-E audio logical frame shorter than its part A";
        }

        return false;
    }

    m_expectedFramePhase = (frameInSuperframe + 1) % 4;
    if ((frameInSuperframe & 1) == 0)
    {
        m_firstHalf = logicalFrame;
        m_firstPartABytes = partABytes;
        return true;
    }

    if (m_firstHalf.empty()) {
        return true;
    }

    // The audio super frame's higher-protected part spans both logical
    // frames' part A, so join the parts A, then the parts B (with EEP, part
    // A is empty and this is the two frames in order).
    const auto firstSplit = m_firstHalf.begin() + static_cast<std::ptrdiff_t>(m_firstPartABytes);
    const auto secondSplit = logicalFrame.begin() + static_cast<std::ptrdiff_t>(partABytes);
    audioSuperframe.assign(m_firstHalf.begin(), firstSplit);
    audioSuperframe.insert(audioSuperframe.end(), logicalFrame.begin(), secondSplit);
    audioSuperframe.insert(audioSuperframe.end(), firstSplit, m_firstHalf.end());
    audioSuperframe.insert(audioSuperframe.end(), secondSplit, logicalFrame.end());
    m_firstHalf.clear();
    outputReady = true;
    return true;
}

} // namespace modemdrm
