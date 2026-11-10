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

#ifndef INCLUDE_DSDPROTOCOLDSTAR_H
#define INCLUDE_DSDPROTOCOLDSTAR_H

#include "dsdprotocol.h"

/**
 * D-STAR (JARL) GMSK 4800 bit/s decoder.
 *
 * A transmission is a bit sync and a 24 symbol header sync, the 660 bit
 * radio header, then 96 bit frames of 72 voice bits (AMBE 3600x2400) and
 * 24 slow data bits, in superframes of 21 frames whose first frame carries
 * the voice sync in place of the slow data. The end of transmission is a
 * 48 symbol pattern in place of a voice sync. Each bit is one symbol
 * (bit 0 positive), so the bit is the MSB of the sliced dibit.
 *
 * Outputs: the voice frames with their LLRs, the header callsigns, and the
 * text message and header repeated in the slow data.
 */
class DSDProtocolDStar : public DSDProtocol
{
public:
    static const int m_headerBits = 660;
    static const int m_voiceBits = 72;
    static const int m_dataBits = 24;
    static const int m_frameBits = 96;
    static const int m_framesPerSuperframe = 21;
    static const int m_superframeBits = 2016;
    static const int m_headerBytes = 41;

    DSDProtocolDStar(const DSDSymbolHistory& history);

    virtual const char *getName() const { return "D-STAR"; }
    virtual int getSymbolRate() const { return 4800; }
    virtual bool isFourLevel() const { return false; }
    virtual std::vector<DVSync::Pattern> getSearchPatterns() const;
    virtual float getNominalOuterLevel() const { return 1200.0f; }
    virtual float getLockThreshold() const { return 0.8f; }

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
    virtual bool endedNormally() const { return m_normalEnd; }
    virtual std::string getStatusText() const;

    bool isHeaderValid() const { return m_headerValid; }
    //! Header callsign fields (8 characters, suffix 4), valid when the header decoded
    std::string getMyCall() const { return field(27, 8); }
    std::string getMySuffix() const { return field(35, 4); }
    std::string getUrCall() const { return field(19, 8); }
    std::string getRpt1() const { return field(11, 8); }
    std::string getRpt2() const { return field(3, 8); }
    const std::string& getText() const { return m_text; }
    //! Position from the GPS slow data (D-PRS or NMEA), when one has been received
    bool getPosition(double& latitude, double& longitude) const;
    const std::string& getGpsText() const { return m_gpsText; }
    //! The last D-PRS sentence whose CRC checked, as APRS (TNC2) text: CALL>APIxxx,DSTAR*:payload, empty for none
    const std::string& getDPRS() const { return m_dprs; }
    //! Incremented for every D-PRS sentence decoded, repeats of the same text included
    uint64_t getDPRSRevision() const { return m_dprsRevision; }

private:
    enum Burst { BurstNone, BurstHeader, BurstFrame };

    bool m_active;
    int64_t m_syncIndex;           //!< Last symbol of the most recent sync (header or voice)
    int64_t m_frameBase;           //!< Index of the last symbol of frame 0's sync of the current superframe
    int m_frameIndex;           //!< Frame within the superframe being received (1..20, 0 = first frame of the next)
    Burst m_burst;
    int64_t m_burstFirst;
    int64_t m_burstLast;
    int64_t m_completeAt;          //!< Symbol index at which the burst is reported complete
    int m_misses;
    int m_silent;
    int m_absent;               //!< Consecutive symbols without carrier
    bool m_burstValid;
    bool m_proven;              //!< A header or voice sync has validated the lock
    bool m_lastSyncWithin3;     //!< The previous voice sync matched within three errors

    bool m_headerValid;
    bool m_headerPublished;
    bool m_normalEnd;
    uint8_t m_header[m_headerBytes];
    std::string m_text;
    char m_textBlocks[4][6];
    uint8_t m_textMask;
    uint8_t m_slowHeader[m_headerBytes];
    uint16_t m_slowHeaderMask;
    uint8_t m_block[6];
    int m_blockLength;
    std::string m_gpsLine;      //!< GPS slow data being assembled
    std::string m_gpsText;      //!< Last complete GPS sentence
    std::string m_dprs;         //!< Last D-PRS sentence as APRS text (without the $$CRC field)
    uint64_t m_dprsRevision;    //!< Counts the D-PRS sentences decoded (monotonic: the consumers compare it)
    bool m_positionValid;
    double m_latitude;
    double m_longitude;

    std::string field(int offset, int length) const;
    void processHeader();
    void publishHeader(int64_t sample);
    void processFrame(int frameIndex, int64_t first);
    void processSlowData(int frameIndex, const uint8_t *bytes);
    void processGpsByte(char c);
    void processGpsLine();
    static bool parseCoordinate(const char *text, int degreeDigits, char hemisphere, double& value);
    bool matchPattern(int64_t lastIndex, DVSync::Pattern pattern, int maxErrors) const;
};

#endif // INCLUDE_DSDPROTOCOLDSTAR_H
