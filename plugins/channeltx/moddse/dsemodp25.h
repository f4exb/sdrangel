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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODP25_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODP25_H_

#include "dsemodprotocol.h"

/**
 * APCO P25 Phase 1 (TIA-102.BAAA-A FDMA common air interface) frame builder:
 * C4FM 4800 symbols/s, IMBE 7200x4400, clear (unencrypted) group voice call.
 *
 * Transmission: HDU, then LDU1/LDU2 pairs (9 IMBE frames each, 180 ms), and on
 * stop the current pair is completed and followed by a TDU. Each nextFrame()
 * call produces one data unit as 4FSK levels, status symbols included.
 *
 * Bit layouts (data bits before status symbol insertion, first on air first):
 *  - all units: frame sync (48) + NID (64: NAC 12, DUID 4, BCH parity 47, parity 1)
 *  - HDU: 36 Golay(18,6) codewords (648) = RS(36,20) of MI(72) MFID(8) ALGID(8) KID(16) TGID(16),
 *    then 10 null bits; 11 status symbols; 792 bits
 *  - LDU1/LDU2: IMBE1 IMBE2 LC(40) IMBE3 LC(40) IMBE4 LC(40) IMBE5 LC(40) IMBE6 LC(40) IMBE7
 *    LC(40) IMBE8 LSD(32) IMBE9; LC = 24 Hamming(10,6) codewords of RS(24,12) (LDU1, link
 *    control) or RS(24,16) (LDU2, encryption sync); 24 status symbols; 1728 bits
 *  - TDU: 28 null bits; 2 status symbols; 144 bits
 *  - TDULC: 12 Golay(24,12) codewords of RS(24,12) of the LC (288), 20 null bits; 432 bits
 * A status symbol (dibit) follows every 35 data dibits, counted from the start of each unit.
 */
class DSEModP25 : public DSEModProtocol
{
public:
    enum DUID
    {
        DUIDHDU = 0x0,
        DUIDTDU = 0x3,
        DUIDLDU1 = 0x5,
        DUIDTSDU = 0x7,
        DUIDLDU2 = 0xA,
        DUIDPDU = 0xC,
        DUIDTDULC = 0xF
    };

    static const int m_syncBits = 48;
    static const int m_nidBits = 64;
    static const int m_imbeBits = 144;
    static const int m_imbeFramesPerLDU = 9;
    static const int m_lcBits = 240;              //!< Encoded LC or ESS in an LDU
    static const int m_lsdBits = 32;              //!< Encoded low speed data in an LDU
    static const int m_headerBits = 648;          //!< Encoded HDU payload
    static const int m_hduDataBits = 760;         //!< Before status symbols and null padding
    static const int m_hduBits = 792;             //!< On air
    static const int m_lduDataBits = 1680;
    static const int m_lduBits = 1728;
    static const int m_tduDataBits = 140;
    static const int m_tduBits = 144;
    static const int m_tdulcDataBits = 400;
    static const int m_tdulcBits = 432;
    static const int m_maxDataBits = m_lduDataBits;
    static const int m_maxFrameBits = m_lduBits;
    static const int m_dataDibitsPerStatus = 35;
    //! Status symbol value: 00 = "unknown, use for talk-around" (a stand alone transmitter)
    static const uint8_t m_statusSymbol = 0x0;
    static const uint64_t m_frameSync = 0x5575F5FF77FFULL;

    DSEModP25();
    virtual ~DSEModP25();

    virtual void start(const DSEModSettings& settings);
    virtual void stop();
    virtual void abort();
    virtual bool isActive() const { return m_state != StateIdle; }
    virtual bool nextFrame(std::vector<int8_t>& levels);

    virtual DSEModVocoder::Rate getVocoderRate() const { return DSEModVocoder::RateIMBE7200x4400; }
    virtual Shaping getShaping() const { return ShapingRRC4FSK; }
    virtual DSEModSettings::DSEModMode getMode() const { return DSEModSettings::DSEModModeP25; }
    virtual const char *getName() const { return "P25"; }
    virtual uint32_t getVoiceFrameCount() const { return m_voiceFrameCount; }
    virtual int getFramesPerBlock() const { return m_imbeFramesPerLDU; }
    //! The HDU (165 ms) takes no voice (getPrimeCredit): most of it is the first LDU's own wait
    virtual int getPrimeCredit() const { return 3; }
    /**
     * A text message as a packet data unit (TIA-102.BAAA 7) sent when the transmission ends, before
     * the TDU: an IPv4/UDP datagram to port 4007 carrying a Motorola TMS simple text message
     * (UCS-2), in confirmed (rate 3/4) or unconfirmed (rate 1/2) data blocks
     */
    virtual bool queueMessage(const std::string& text);
    void setConfirmedData(bool confirmed) { m_confirmedData = confirmed; }
    //! The data bytes of a packet data unit carrying the text: IPv4, UDP and TMS, the padding and the CRC-32
    static void buildTextPacket(const std::string& text, uint32_t sourceId, uint32_t destinationId, bool confirmed, std::vector<uint8_t>& data, int& blocks, int& padOctets);
    //! Sync, NID and the blocks of a packet data unit (before status symbols)
    static void buildPDU(int nac, uint32_t destinationId, bool confirmed, const std::vector<uint8_t>& data, int blocks, int padOctets, std::vector<uint8_t>& bits);

    // ---- Static builders (exposed for the unit tests) ----------------------------------

    //! Group voice channel user LC (TIA-102.AABF, LCO 0): LCF MFID SO reserved TGID(16) source(24)
    static void buildLC(const DSEModSettings& settings, uint8_t *lc9);
    //! 9 byte LC -> 240 bits: 12 hexbits, RS(24,12,13), 24 Hamming(10,6,3) codewords
    static void encodeLC(const uint8_t *lc9, uint8_t *bits240);
    //! Encryption sync: MI(72) ALGID(8) KID(16) -> 240 bits: 16 hexbits, RS(24,16,9), Hamming(10,6,3)
    static void encodeESS(const uint8_t *mi9, uint8_t algid, uint16_t kid, uint8_t *bits240);
    //! Header: MI(72) MFID(8) ALGID(8) KID(16) TGID(16) -> 648 bits: 20 hexbits, RS(36,20,17), Golay(18,6,8)
    static void encodeHeader(const uint8_t *mi9, uint8_t mfid, uint8_t algid, uint16_t kid, uint16_t tgid, uint8_t *bits648);
    //! 16 bits of low speed data -> 32 bits: two cyclic(16,8,5) codewords
    static void encodeLSD(uint16_t lsd, uint8_t *bits32);
    //! LC -> 288 bits of TDULC payload: RS(24,12,13) then 12 Golay(24,12,8) codewords of hexbit pairs
    static void encodeTDULC(const uint8_t *lc9, uint8_t *bits288);
    //! Frame sync and NID: 112 bits
    static int putSyncNID(int nac, int duid, uint8_t *bits);
    //! HDU data bits (760) without padding and status symbols
    static int buildHDU(int nac, const uint8_t *mi9, uint8_t mfid, uint8_t algid, uint16_t kid, uint16_t tgid, uint8_t *bits);
    //! LDU data bits (1680): imbe is 9 x 144 on air bits, encoded240 the LC or ESS bits, lsd32 the LSD bits
    static int buildLDU(int nac, int duid, const uint8_t *imbe, const uint8_t *encoded240, const uint8_t *lsd32, uint8_t *bits);
    //! TDU data bits (140)
    static int buildTDU(int nac, uint8_t *bits);
    //! TDULC data bits (400)
    static int buildTDULC(int nac, const uint8_t *lc9, uint8_t *bits);
    //! Pad to a multiple of 70 bits with nulls and insert a status dibit after every 35 data dibits.
    //! Returns the number of output bits.
    static int insertStatusSymbols(const uint8_t *data, int nbDataBits, uint8_t status, uint8_t *out);
    //! Data bits -> status symbols -> 4FSK levels appended to levels. Returns the number of symbols.
    static int appendFrame(std::vector<int8_t>& levels, const uint8_t *data, int nbDataBits);
    //! Vocoder frame (18 bytes, P25 on air order) to 144 bits, MSB of byte 0 first
    static void vocoderFrameToBits(const uint8_t *frame, uint8_t *bits);

private:
    enum State
    {
        StateIdle,
        StateHDU,
        StateLDU1,
        StateLDU2,
        StatePDU,   //!< The queued text message
        StateTDU
    };

    State m_state;
    bool m_stopRequested;
    int m_nac;
    uint16_t m_talkGroup;
    uint32_t m_sourceId;
    uint8_t m_lc[9];
    uint8_t m_mi[9];
    uint8_t m_lcBitsEncoded[m_lcBits];
    uint8_t m_essBitsEncoded[m_lcBits];
    uint8_t m_lsdBitsLDU1[m_lsdBits];
    uint8_t m_lsdBitsLDU2[m_lsdBits];
    uint32_t m_lsd;                 //!< 32 bit low speed data: high 16 bits in LDU1, low 16 bits in LDU2
    uint32_t m_voiceFrameCount;
    std::string m_message;          //!< Text queued for a packet data unit
    bool m_confirmedData;

    void buildLDUFrame(int duid, uint8_t *bits);
    void appendPDU(std::vector<int8_t>& levels);
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODP25_H_
