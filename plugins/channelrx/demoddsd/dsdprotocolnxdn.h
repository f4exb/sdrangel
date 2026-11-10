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

#ifndef INCLUDE_DSDPROTOCOLNXDN_H
#define INCLUDE_DSDPROTOCOLNXDN_H

#include <cstdint>
#include <string>
#include <vector>

#include "dsdprotocol.h"

/**
 * NXDN decoder: 4800 bit/s at 2400 symbols/s (6.25 kHz channels, "NXDN48")
 * or 9600 bit/s at 4800 symbols/s (12.5 kHz channels, "NXDN96"), one
 * instance per rate. The frames are the same at both rates.
 *
 * Every frame (80 ms at 2400 symbols/s, 40 ms at 4800) is 192 symbols: the 10 symbol frame sync word (FSW),
 * the 8 symbol link information channel (LICH, one bit per symbol), the
 * 30 symbol slow associated control channel (SACCH) and 144 symbols of
 * payload: four 72 bit voice channels (VCH, AMBE+2 3600x2450 in the DMR
 * bit order) or two fast associated control channels (FACCH1), or one of
 * each. Everything after the FSW is scrambled. The SACCH carries the call
 * message (VCALL: call type, source and destination) in four fragments
 * over a superframe; a transmission release (TX_REL) in the FACCH1 ends
 * the call.
 */
class DSDProtocolNXDN : public DSDProtocol
{
public:
    static const int m_frameSymbols = 192;
    static const int m_fswSymbols = 10;
    static const int m_lichSymbols = 8;
    static const int m_sacchSymbols = 30;
    static const int m_payloadSymbols = 144;
    static const int m_vchSymbols = 36;
    static const int m_facchSymbols = 72;

    //! symbolRate 2400 (NXDN48) or 4800 (NXDN96)
    DSDProtocolNXDN(const DSDSymbolHistory& history, int symbolRate = 2400);

    virtual const char *getName() const { return m_symbolRate == 4800 ? "NXDN96" : "NXDN48"; }
    virtual int getSymbolRate() const { return m_symbolRate; }
    virtual bool isFourLevel() const { return true; }
    virtual std::vector<DVSync::Pattern> getSearchPatterns() const;
    //! +/- 1050 Hz in a 6.25 kHz channel, +/- 2100 Hz in a 12.5 kHz channel
    virtual float getNominalOuterLevel() const { return m_symbolRate == 4800 ? 2100.0f : 1050.0f; }

    virtual void start(DVSync::Pattern pattern, int64_t syncIndex);
    virtual bool pushSymbol(int64_t index, const DSDSoftSymbol& symbol, int64_t sampleIndex);
    virtual void getBurstSpan(int64_t& first, int64_t& last) const;
    virtual void processBurst();
    virtual bool isProven() const { return m_proven; }
    virtual bool wasBurstValid() const { return m_burstValid; }
    virtual bool getNextSync(int64_t& syncIndex, std::vector<DVSync::Pattern>& patterns) const;
    virtual void resync(DVSync::Pattern pattern, int64_t syncIndex);
    virtual void getConfirmSpan(int64_t syncIndex, int64_t& first, int64_t& last) const;
    virtual bool confirmWithoutSync(int64_t syncIndex, bool present);
    virtual void end();
    virtual std::string getStatusText() const;

    int getRAN() const { return m_ran; }
    bool isFullRate() const { return m_fullRate; }
    bool getCall(bool& group, uint32_t& source, uint32_t& destination) const;

    /** The user data of a short data call or a data call (TS 1-A SDCALL / DCALL) */
    struct TextMessage
    {
        bool m_valid = false;
        bool m_group = false;
        uint32_t m_source = 0;
        uint32_t m_destination = 0;
        bool m_isText = false;      //!< The data reads as plain text (else m_text is its hexadecimal)
        std::string m_text;
        uint64_t m_revision = 0;
    };

    const TextMessage& getTextMessage() const { return m_textMessage; }
    int getDataCallCount() const { return m_dataCallCount; }

    //! A trunked control channel (Type-C RCCH, TS 1-A): its outbound CAC frames seen
    bool isControlChannel() const { return m_cacCount > 0; }
    int getControlFrameCount() const { return m_cacCount; }
    //! The site (a SITE_INFO or SRV_INFO's Location ID: category, system and site codes), the last
    //! RCCH message and the last channel assignment
    const std::string& getSiteText() const { return m_siteText; }
    const std::string& getControlText() const { return m_controlText; }
    const std::string& getGrantText() const { return m_grantText; }
    //! An RCCH layer 3 message (the first octet's low 6 bits its type) of nbBytes: its text, empty
    //! for IDLE. grant: a voice or data channel assignment. site: the site's Location ID text, when
    //! the message gives it
    static std::string controlMessageText(const uint8_t *message, int nbBytes, bool& grant, std::string& site);
    //! A Location ID (24 bits: category, system code, site code) as text
    static std::string locationText(uint32_t location);

private:
    int m_symbolRate;
    bool m_active;
    int64_t m_syncIndex;
    int64_t m_pendingSync;
    int m_frameCount;           //!< Frames decoded since the start
    int m_good;                 //!< Frames with a valid LICH or SACCH
    int m_misses;
    int m_silent;
    int m_absent;
    bool m_burstValid;
    bool m_proven;              //!< A frame has been validated since the lock
    bool m_lastFrameValid;      //!< The previous frame's LICH and SACCH decoded
    int m_ran;
    uint8_t m_message[9];
    int m_messageMask;          //!< Fragments received
    bool m_callValid;
    bool m_fullRate;            //!< The voice call uses the full rate vocoder (EFR, NXDN96 only)
    bool m_group;
    uint32_t m_source;
    uint32_t m_destination;
    std::string m_typeText;

    /** A data call being received: the header's parameters and the user data blocks */
    struct DataCall
    {
        bool m_headerValid = false;
        bool m_group = false;
        uint32_t m_source = 0;
        uint32_t m_destination = 0;
        int m_cipher = 0;
        int m_blocks = 0;           //!< Number of the first block: blocks count down to 0
        int m_padOctets = 0;
        int m_byteLength = 0;       //!< User data bytes per block (the channel decides: FACCH1 8, UDCH 20)
        uint32_t m_received = 0;    //!< Blocks received, bit per block number
        std::vector<uint8_t> m_data;
    };

    DataCall m_dataCall;
    TextMessage m_textMessage;
    int m_dataCallCount;        //!< Data calls whose CRC-32 checked
    uint64_t m_metadataRevision;
    int m_cacCount;             //!< Outbound CAC frames whose CRC-16 checked
    std::string m_siteText;
    std::string m_controlText;
    std::string m_grantText;

    void decodeFrame(int64_t syncIndex);
    bool processCAC(const float *llr);
    void processVoice(const float *llr, int64_t firstSymbol, int count);
    void processVoiceFullRate(const float *llr, int64_t firstSymbol, int count);
    bool processFACCH(const float *llr);
    bool processUDCH(const float *llr);
    //! A message of nbBytes from a channel; dataBytes: the user data bytes a data block of it carries
    void parseMessage(const uint8_t *message, int nbBytes, int dataBytes);
    void parseDataHeader(const uint8_t *message);
    void parseDataBlock(const uint8_t *message, int nbBytes, int dataBytes);
    void finishDataCall();
};

#endif // INCLUDE_DSDPROTOCOLNXDN_H
