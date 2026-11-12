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

#ifndef INCLUDE_DSDPROTOCOLYSF_H
#define INCLUDE_DSDPROTOCOLYSF_H

#include <string>
#include <vector>

#include "dsp/dv/dvysf.h"

#include "dsdprotocol.h"

/**
 * Yaesu System Fusion (C4FM 4800 symbols/s) decoder.
 *
 * Every 100 ms frame is 480 dibits: a 20 dibit sync, the 100 dibit frame
 * information channel (FICH) and 360 dibits of payload. Header and
 * terminator frames carry the callsigns (CSD1, CSD2) in two data channels;
 * V/D type 2 communication frames carry five 20 dibit data channel slices
 * and five 52 dibit voice channels of 49 AMBE parameter bits each, tripled
 * for the most sensitive bits; their data channel gives the callsigns (FN 0
 * to 3), the four remark fields (FN 4, 5) and the GPS position (FN 6, 7).
 * Data full rate frames carry two data channels coded as the header's, the
 * bytes of Wires-X commands and replies, text messages and pictures. V/D type
 * 1 frames carry five 36 dibit slices of one data channel (CSD1, CSD2, CSD3,
 * then DT1 to DT5 by FN) and five AMBE+2 3600x2450 frames in DMR's order;
 * voice full rate frames five IMBE 7200x4400 frames in YSF's order (given out
 * in P25's), the first after the header (FN 0, FT 1) CSD3 and two of them.
 *
 * Outputs: the 49 parameter bit voice frames as DVSI raw words, the FICH
 * fields and the callsigns.
 */
class DSDProtocolYSF : public DSDProtocol
{
public:
    static constexpr int m_frameDibits = 480;
    static constexpr int m_syncDibits = 20;
    static constexpr int m_fichDibits = 100;
    static constexpr int m_payloadDibits = 360;

    DSDProtocolYSF(const DSDSymbolHistory& history);

    virtual const char *getName() const { return "YSF"; }
    virtual int getSymbolRate() const { return 4800; }
    virtual bool isFourLevel() const { return true; }
    virtual std::vector<DVSync::Pattern> getSearchPatterns() const;
    virtual float getNominalOuterLevel() const { return 1800.0f; }

    virtual void start(DVSync::Pattern pattern, int64_t syncIndex);
    virtual bool pushSymbol(int64_t index, const DSDSoftSymbol& symbol, int64_t sampleIndex);
    virtual void getBurstSpan(int64_t& first, int64_t& last) const;
    virtual void processBurst();
    virtual bool wasBurstValid() const { return m_burstValid; }
    virtual bool getNextSync(int64_t& syncIndex, std::vector<DVSync::Pattern>& patterns) const;
    virtual void resync(DVSync::Pattern pattern, int64_t syncIndex);
    virtual void getConfirmSpan(int64_t syncIndex, int64_t& first, int64_t& last) const;
    virtual bool confirmWithoutSync(int64_t syncIndex, bool present);
    virtual void end();
    virtual bool endedNormally() const { return m_normalEnd; }
    virtual std::string getStatusText() const;

    const DVYSF::FICH& getFICH() const { return m_fich; }
    bool isFICHValid() const { return m_fichValid; }
    std::string getSource() const { return m_source; }
    std::string getDestination() const { return m_destination; }
    std::string getDownlink() const { return m_downlink; }
    std::string getUplink() const { return m_uplink; }
    //! The remark fields (CSD3: Rem1 to Rem4, five characters each), the non-blank ones joined
    std::string getRemarks() const;

    //! A message of data full rate frames (a Wires-X command or reply, a text message, a picture
    //! block...), complete or cut short (m_complete false: a data channel lost, or no end marker)
    struct DataMessage
    {
        bool m_valid = false;
        bool m_complete = false;
        bool m_isText = false;      //!< A text message: m_text is its text (else a description)
        std::string m_kind;         //!< "text message", "Wires-X connect request", "data"...
        std::string m_text;
        std::string m_source;
        std::string m_destination;
        uint64_t m_revision = 0;
    };

    //! The GPS position of the V/D type 2 frames (FN 6 and 7: APRS Mic-E encoded)
    struct Position
    {
        bool m_valid = false;
        double m_latitude = 0.0;
        double m_longitude = 0.0;
        int m_speedKnots = -1;
        int m_course = -1;
        int m_radio = -1;           //!< The radio model code
        std::string m_source;
        uint64_t m_revision = 0;
    };

    const DataMessage& getDataMessage() const { return m_dataMessage; }
    const Position& getPosition() const { return m_position; }

    //! The framing of the data (Wires-X, GPS: seq, 3 byte opcode, radio or '&', body, 0x03, 8 bit
    //! sum): the length up to and including the checksum, or -1 when no end marker checks
    static int messageLength(const uint8_t *bytes, int n);
    //! A short GPS message ("b_): its Mic-E position. False when it is not one or does not decode
    static bool decodeGPS(const uint8_t *bytes, int n, Position& position);
    //! A message's kind and a description (a text message's text alone)
    static std::string describe(const uint8_t *m, int length, bool& isText, std::string& kind);
    static const char *radioName(int model);

    static constexpr int m_dataBlockBytes = 260;    //!< A block's bytes (FT 7): later blocks lead with a 0x00 filler
    static constexpr int m_maxDataBlocks = 4;

private:
    bool m_active;
    int64_t m_syncIndex;           //!< Last symbol of the sync of the frame being received
    int64_t m_pendingSync;
    int m_misses;
    int m_silent;
    int m_absent;
    bool m_burstValid;
    bool m_fichValid;
    bool m_normalEnd;
    DVYSF::FICH m_fich;
    std::string m_source;
    std::string m_destination;
    std::string m_downlink;
    std::string m_uplink;
    std::string m_rem[4];
    std::vector<uint8_t> m_dataFR;      //!< The data full rate message being received, by block
    std::vector<bool> m_dataFRValid;    //!< Its 20 byte data channels received
    bool m_dataFRActive;
    uint8_t m_gps[20];
    int m_gpsMask;                      //!< FN 6 and 7 of the superframe received
    uint8_t m_vd1Data[100];             //!< V/D type 1's DT1 to DT5 (FN 3 to 7)
    int m_vd1DataMask;
    DataMessage m_dataMessage;
    Position m_position;

    void decodeFrame(int64_t syncIndex);
    void decodeDCHPair(int64_t payload, uint8_t *dch1, bool& ok1, uint8_t *dch2, bool& ok2);
    void processHeader(int64_t payload);
    void processDataFR(int64_t payload, const DVYSF::FICH& fich);
    void putDataFR(int offset, const uint8_t *bytes, bool ok);
    void finishDataFR();
    void setRemarks(const uint8_t *bytes, int first);
    void processVD2(int64_t payload, int fn, int64_t sampleBase);
    void processVD1(int64_t payload, int fn);
    void processVoiceFullRate(int64_t payload, const DVYSF::FICH& fich);
    //! The AMBE+2 (72 bits, DMR's order) or IMBE (144 bits, YSF's order) frame of nbDibits at first
    void voiceFrame(int64_t first, int nbDibits);
    static std::string callsign(const uint8_t *bytes);
};

#endif // INCLUDE_DSDPROTOCOLYSF_H
