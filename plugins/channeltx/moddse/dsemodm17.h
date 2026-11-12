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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODM17_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODM17_H_

#include <deque>
#include <string>
#include <vector>

#include "dsp/dv/dvm17.h"

#include "dsemodprotocol.h"

/**
 * M17 frame builder (https://spec.m17project.org): 4FSK at 4800 symbols/s, 40 ms frames of 192
 * symbols, root raised cosine with a roll off of 0.5.
 *
 * A voice transmission is a preamble, the link setup frame, stream frames of two Codec2 3200
 * packets each carrying a sixth of the link setup in their LICH (so a receiver joining late has
 * it within 240 ms), the last frame marked in its frame number, and the end of transmission
 * marker. A text message with no voice input is a packet transmission instead: the same preamble
 * and link setup with the packet type, then packet frames of 25 bytes, then the marker.
 */
class DSEModM17 : public DSEModProtocol
{
public:
    static constexpr int m_preambleSymbols = 192;   //!< One frame of +3 -3, as the specification has it

    DSEModM17();
    virtual ~DSEModM17();

    virtual void start(const DSEModSettings& settings);
    virtual void stop();
    virtual void abort();
    virtual bool isActive() const { return m_state != StateIdle; }
    virtual bool nextFrame(std::vector<int8_t>& levels);
    virtual bool queueMessage(const std::string& text);
    virtual bool isDataOnly() const { return m_dataOnly; }

    virtual DSEModVocoder::Rate getVocoderRate() const { return DSEModVocoder::RateCodec23200; }
    virtual Shaping getShaping() const { return ShapingRRC4FSK; }
    virtual double getShapingAlpha() const { return DVM17::RrcAlpha; }
    virtual DSEModSettings::DSEModMode getMode() const { return DSEModSettings::DSEModModeM17; }
    virtual const char *getName() const { return "M17"; }
    virtual uint32_t getVoiceFrameCount() const { return m_voiceFrameCount; }
    virtual int getFramesPerBlock() const { return 2; }   //!< Two Codec2 frames in a stream frame
    //! The preamble and LSF (80 ms) take no voice (getPrimeCredit)
    virtual int getPrimeCredit() const { return 2; }

    //! Whether a data only transmission (a packet) is wanted for these settings
    static bool wantsDataOnly(const DSEModSettings& settings);
    //! The link setup frame of the settings
    static DVM17::LSF makeLSF(const DSEModSettings& settings, bool stream);
    uint32_t getFrameCount() const { return m_frameCount; }
    const DVM17::LSF& getLSF() const { return m_lsf; }

private:
    enum State
    {
        StateIdle,
        StatePreamble,
        StateLSF,
        StateStream,     //!< Voice frames
        StatePacket,     //!< The frames of a packet transmission
        StateEOT
    };

    State m_state;
    bool m_dataOnly;
    bool m_stopRequested;
    DVM17::LSF m_lsf;
    uint8_t m_lsfBytes[DVM17::LSFBytes];
    int m_segment;                   //!< LICH segment of the next stream frame, 0..5
    uint16_t m_frameNumber;
    uint32_t m_frameCount;
    uint32_t m_voiceFrameCount;
    std::vector<uint8_t> m_packet;   //!< The packet being sent, its CRC included
    int m_packetOffset;
    int m_packetFrame;
    std::string m_message;           //!< Queued before the transmission starts
    DSEModSettings m_settings;

    void buildStreamFrame(std::vector<int8_t>& levels, bool last);
    void buildPacketFrame(std::vector<int8_t>& levels);
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODM17_H_
