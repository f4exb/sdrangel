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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODDSTAR_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODDSTAR_H_

#include <QByteArray>
#include <QDateTime>
#include <QString>

#include "dsemodprotocol.h"

/**
 * D-STAR (JARL) digital voice frame builder.
 *
 * Transmission: bit sync + frame sync, 660 bit FEC/interleaved/scrambled radio
 * header, then 21 frame superframes of 72 AMBE bits + 24 slow data bits where
 * the first frame of each superframe carries the 24 bit voice sync in place of
 * slow data. Ends with the 48 bit end of transmission pattern.
 *
 * Bit polarity: bit 0 is transmitted as positive deviation (dsdcc non-inverted
 * convention). All bytes are transmitted LSB first.
 */
class DSEModDStar : public DSEModProtocol
{
public:
    DSEModDStar();
    virtual ~DSEModDStar();

    virtual void start(const DSEModSettings& settings);
    virtual void stop();
    virtual void abort();
    virtual bool isActive() const { return m_state != StateIdle; }
    virtual bool nextFrame(std::vector<int8_t>& levels);
    virtual bool queueMessage(const std::string& text) {
        if (!isActive() || text.empty() || text.size() > 20) { return false; }
        m_text = QString::fromLatin1(text.data(), (int) text.size()).leftJustified(20, ' ');
        return true; // buildSlowData latches it at the next superframe boundary
    }

    // (the base's signature: with four parameters it hid the processor's call, and D-PRS never went out)
    virtual void setPosition(bool valid, double latitude, double longitude, float altitude,
        int speed = -1, int course = -1, int source = 1) override;

    virtual DSEModVocoder::Rate getVocoderRate() const { return DSEModVocoder::Rate3600x2400; }
    virtual Shaping getShaping() const { return ShapingGMSK; }
    virtual DSEModSettings::DSEModMode getMode() const { return DSEModSettings::DSEModModeDStar; }
    virtual const char *getName() const { return "D-STAR"; }
    virtual uint32_t getVoiceFrameCount() const { return m_voiceFrameCount; }
    virtual int getFramesPerBlock() const { return 1; }
    //! The preamble and header (159 ms) take no voice, less two frames kept as margin (getPrimeCredit)
    virtual int getPrimeCredit() const { return 5; }

    static const int m_headerBytes = 41;
    static const int m_headerBits = 660;
    static const int m_voiceBits = 72;
    static const int m_dataBits = 24;
    static const int m_framesPerSuperframe = 21;
    static const int m_endBits = 48;
    static const int m_bitSyncSymbols = 80;
    static const int m_frameSyncSymbols = 24;

    //! Build the 41 byte radio header including CRC (exposed for tests)
    static void buildHeader(const DSEModSettings& settings, uint8_t *header);
    //! Encode a 41 byte header into 660 on-air bits (exposed for tests)
    static void encodeHeader(const uint8_t *header, uint8_t *bits);
    //! Build a D-PRS (GPS-A) position sentence "$$CRChhhh,CALL>APZSDR,DSTAR*:/hhmmssh...\r\n" (exposed for tests)
    static QByteArray buildDPRS(const QString& callsign, double latitude, double longitude, float altitude, const QDateTime& utc);

    static const int m_positionIntervalSuperframes = 24; //!< About 10 s between position sentences

    //! Sync patterns as dsdcc symbol values (1 = positive, 3 = negative)
    static const uint8_t m_headerSync[m_frameSyncSymbols];
    static const uint8_t m_voiceSync[m_dataBits];
    static const uint8_t m_endSequence[m_endBits];

private:
    enum State
    {
        StateIdle,
        StatePreamble,
        StateHeader,
        StateVoice,
        StateEnd
    };

    State m_state;
    bool m_stopRequested;
    uint8_t m_header[m_headerBytes];
    uint8_t m_headerBitsOnAir[m_headerBits];
    int m_frameIndex;           //!< 0..20 within superframe (0 = sync frame)
    int m_superframeIndex;
    uint32_t m_voiceFrameCount;
    QString m_text;
    uint8_t m_slowData[20][3];  //!< Slow data bytes for data frames 1..20 of the current superframe
    bool m_sendPosition;        //!< Position sentences enabled for this transmission
    bool m_positionValid;
    double m_latitude;
    double m_longitude;
    float m_altitude;
    QByteArray m_dprs;          //!< Sentence being sent, empty when none
    int m_dprsOffset;           //!< Bytes of m_dprs already scheduled
    int m_dprsSuperframe;       //!< Superframe index at which the next sentence starts

    void buildSlowData();
    void appendBits(std::vector<int8_t>& levels, const uint8_t *bits, int nbBits);
    void appendSymbols(std::vector<int8_t>& levels, const uint8_t *symbols, int nbSymbols);
    static QString fixLength(const QString& s, int len);
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODDSTAR_H_
