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

#ifndef INCLUDE_DSDPROTOCOLDPMR_H
#define INCLUDE_DSDPROTOCOLDPMR_H

#include <string>
#include <vector>

#include "dsdprotocol.h"

/**
 * dPMR (ETSI TS 102 490 / TS 102 658, 2400 symbols/s 4FSK) decoder.
 *
 * A call starts with a header frame: preamble, the 24 symbol FS1, the
 * 60 symbol coded header information (HI), the 12 symbol colour code
 * and the HI again. Payload frames of 192 symbols follow in superframes
 * of four: FS2 (even frames) or the colour code (odd frames), a 36 symbol
 * control channel (CCH) and four 36 symbol traffic channels (TCH, one
 * AMBE+2 3600x2450 frame each). The call ends with FS3 and a 36 symbol
 * END field in place of a payload frame.
 *
 * Outputs: the voice frames with their LLRs, the header (called and own
 * identities, communication mode and format) and the colour code.
 */
class DSDProtocolDPMR : public DSDProtocol
{
public:
    static constexpr int m_frameSymbols = 192;
    static constexpr int m_fs1Symbols = 24;
    static constexpr int m_fs2Symbols = 12;
    static constexpr int m_ccSymbols = 12;
    static constexpr int m_hiSymbols = 60;
    static constexpr int m_cchSymbols = 36;
    static constexpr int m_tchSymbols = 36;
    static constexpr int m_endSymbols = 36;
    static constexpr int m_headerSymbols = 132;  //!< After FS1: HI, CC, HI

    DSDProtocolDPMR(const DSDSymbolHistory& history);

    virtual const char *getName() const { return "dPMR"; }
    virtual int getSymbolRate() const { return 2400; }
    virtual bool isFourLevel() const { return true; }
    virtual std::vector<DVSync::Pattern> getSearchPatterns() const;
    virtual float getNominalOuterLevel() const { return 1050.0f; }

    virtual void start(DVSync::Pattern pattern, int64_t syncIndex);
    virtual bool pushSymbol(int64_t index, const DSDSoftSymbol& symbol, int64_t sampleIndex);
    virtual void getBurstSpan(int64_t& first, int64_t& last) const;
    virtual void processBurst();
    virtual bool wasBurstValid() const { return m_burstValid; }
    virtual bool getNextSync(int64_t& syncIndex, std::vector<DVSync::Pattern>& patterns) const;
    virtual void resync(DVSync::Pattern pattern, int64_t syncIndex);
    virtual bool acceptUnexpectedSync(DVSync::Pattern pattern, int64_t syncIndex) const;
    virtual void getConfirmSpan(int64_t syncIndex, int64_t& first, int64_t& last) const;
    virtual bool confirmWithoutSync(int64_t syncIndex, bool present);
    virtual void end();
    virtual std::string getStatusText() const;

    int getColourCode() const { return m_colourCode; }
    bool getHeader(uint32_t& calledId, uint32_t& ownId, int& mode, int& format) const;

    //! A slow data message (TS 102 490 5.9.1: communication mode 001, two bytes in each CCH, a
    //! flag marking a message's last byte), complete or cut short by the end of the call or a lost
    //! CCH. The standard gives the bytes no character set: plain text when they read as such
    struct TextMessage
    {
        bool m_valid = false;
        uint32_t m_source = 0;      //!< Own ID
        uint32_t m_destination = 0; //!< Called ID
        bool m_isText = false;      //!< The bytes read as text (else m_text is their hexadecimal)
        bool m_complete = false;    //!< Ended by its last byte flag (else cut short, lost bytes shown as ?)
        std::string m_text;
        uint64_t m_revision = 0;
    };

    const TextMessage& getTextMessage() const { return m_textMessage; }
    static constexpr int m_maxSlowDataBytes = 256;

private:
    enum Burst { BurstNone, BurstHeader, BurstPayload, BurstEnd };

    bool m_active;
    int64_t m_syncIndex;           //!< Last symbol of the most recent FS1, FS2 or colour code field
    int m_frameNumber;          //!< Payload frame number within the superframe (0..3) of the burst being received
    Burst m_burst;
    int64_t m_burstFirst;
    int64_t m_burstLast;
    int64_t m_pendingFirst;
    int64_t m_pendingLast;
    Burst m_pendingBurst;
    int m_pendingFrame;
    int m_misses;
    int m_silent;
    int m_absent;
    bool m_burstValid;
    int m_colourCode;
    bool m_headerValid;
    uint32_t m_calledId;
    uint32_t m_ownId;
    int m_mode;
    int m_format;
    std::string m_typeText;
    int m_cchMode;              //!< The communication mode of the last CCH read (-1: none yet)
    std::vector<uint8_t> m_slowData;    //!< The slow data message being received
    bool m_slowDataLost;        //!< Its bytes include those of a lost CCH
    TextMessage m_textMessage;

    void processHeader(int64_t first);
    void processPayload(int64_t first, int frameNumber);
    void processEnd(int64_t first);
    bool checkColourCode(int64_t lastIndex) const;
    bool matchPattern(int64_t lastIndex, DVSync::Pattern pattern, int maxErrors) const;
    void parseHeader(const uint8_t *hi72);
    void slowDataByte(uint8_t byte, bool last);
    void finishSlowData(bool complete);
};

#endif // INCLUDE_DSDPROTOCOLDPMR_H
