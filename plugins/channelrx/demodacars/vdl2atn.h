///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
//                                                                               //
// The X.25, CLNP and COTP layouts follow dumpvdl2 by Tomasz Lemiech,            //
// https://github.com/szpajder/dumpvdl2, GPL-3.0, which also supplies the        //
// vendored ICAO ATN ASN.1 decoder in the atn/ directory.                        //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                              //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_VDL2ATN_H
#define INCLUDE_VDL2ATN_H

#include <QByteArray>
#include <QHash>
#include <QString>

// Decoder for the non-ACARS payloads of VDL-2 AVLC information frames: the ATN
// air/ground subnetwork. The stack is X.25 (ISO 8208) packets, carrying CLNP (ISO 8473,
// usually in the ATN compressed local-reference form) or ES-IS, carrying COTP (ISO 8073)
// transport, carrying the ICAO ULCS/application ASN.1: CM logons, CPDLC and ADS-C v2,
// which are decoded by the vendored dumpvdl2 decoder in atn/.
//
// GUI use only: decoding is presentational, and the reassembly state (X.25 M-bit, CLNP
// segmentation, COTP multi-TPDU) is held here in simple in-order accumulators, which is
// sufficient for frames arriving in order from a single RF channel.
class Vdl2AtnDecoder
{
public:
// Above this an XID altitude is not believable and is discarded - see vdl2atn.cpp.
// Airliners do not fly here, and the field's own encoding reaches 255000 ft.
// Above every airliner that sends these - the highest genuine XID altitude measured is
// FL430 - and below every corrupt value seen. It was 60000 until EI-GXJ, a Ryanair 737-800,
// sent 59000 and slipped straight through
#define ACARSVDL2_MAX_XID_ALTITUDE_FT 50000

    struct Result
    {
        QString m_summary;      //!< One line layer summary, e.g. "X.25 DATA > CLNP > COTP DT > CPDLC"
        QString m_decoded;      //!< Full multi-line decode of the ATN application, if any
        QString m_atc;          //!< Condensed CPDLC message data, e.g. "WILCO", for the ATC column
        bool m_isCpdlc = false;
        bool m_isCm = false;
        bool m_isAdsc = false;
        // Position from an XID aircraft or ground station location parameter
        bool m_hasPosition = false;
        float m_latitude = 0.0f;
        float m_longitude = 0.0f;
        bool m_hasAltitude = false; //!< Aircraft locations carry an altitude, ground stations do not
        int m_altitudeFt = 0;
        //!< Where the aircraft told the ground station it was going, from the XID
        //!< destination airport parameter. Frequently the only place a flight's
        //!< destination is stated at all - a short sector may log on and land without
        //!< ever filing a plan or sending a position report.
        QString m_destinationAirport;
    };

    //!< Pulled out of the formatted decode rather than the parameter tree, as the
    //!< position is. Static and public so it can be tested on its own: the text is what
    //!< libacars produces and the pattern is the part that can be wrong.
    static QString xidDestinationAirport(const QString& decoded);

    // Decode the information field of one AVLC I frame
    Result decode(const QByteArray& info, quint32 srcAddr, quint32 dstAddr, bool fromAircraft);

    // Decode an XID frame's information field: link management, including the GSIF
    // ground station broadcasts. cr is the source address C/R (status) bit and pf the
    // control field P/F bit, which select the XID message type.
    Result decodeXid(const QByteArray& info, bool cr, bool pf);

    // Drop all reassembly state (e.g. when the table is cleared or the channel retuned)
    void clear();

private:
    // Reassembly flows are per direction and per X.25 logical channel, so two virtual
    // circuits between the same pair of addresses cannot interleave into one buffer
    struct FlowKey
    {
        quint32 m_src;
        quint32 m_dst;
        quint32 m_channel;      // 12 bit X.25 logical channel identifier
        bool operator==(const FlowKey& other) const
        {
            return m_src == other.m_src && m_dst == other.m_dst && m_channel == other.m_channel;
        }
    };
    friend uint qHash(const Vdl2AtnDecoder::FlowKey& key, uint seed);

    // An X.25 M-bit reassembly also tracks the next expected P(S), so a lost data
    // packet discards the partial buffer instead of splicing unrelated fragments.
    // Every buffer records when it was last touched (as a decoded-frame count), so
    // flows that lost their final fragment expire instead of accumulating forever.
    struct X25Reasm
    {
        QByteArray m_data;
        int m_nextPs = -1;
        quint64 m_lastUsed = 0;
    };
    struct ReasmBuf
    {
        QByteArray m_data;
        quint64 m_lastUsed = 0;
    };

    QHash<FlowKey, X25Reasm> m_x25Reassembly;       // M-bit accumulation per direction
    QHash<FlowKey, ReasmBuf> m_clnpReassembly;      // Segmented CLNP accumulation
    QHash<FlowKey, ReasmBuf> m_cotpReassembly;      // Multi-TPDU accumulation
    bool m_fromAircraft = false;                    // Direction of the frame being decoded
    quint32 m_channel = 0;                          // Logical channel of the frame being decoded
    quint64 m_frameCount = 0;                       // Frames decoded, for reassembly aging

    void dropFlow(quint32 srcAddr, quint32 dstAddr);
    void pruneStale();

    void parseX25UserData(const uint8_t *buf, int len, quint32 srcAddr, quint32 dstAddr, Result& result);
    void parseClnp(const uint8_t *buf, int len, quint32 srcAddr, quint32 dstAddr, Result& result);
    void parseClnpPayload(const uint8_t *buf, int len, quint32 srcAddr, quint32 dstAddr, Result& result);
    void parseClnpCompressed(const uint8_t *buf, int len, quint32 srcAddr, quint32 dstAddr, Result& result);
    void parseCotp(const uint8_t *buf, int len, quint32 srcAddr, quint32 dstAddr, Result& result);
    void parseIcao(const uint8_t *buf, int len, bool fromAircraft, Result& result);
    static void extractAdscPosition(Result& result);
};

inline uint qHash(const Vdl2AtnDecoder::FlowKey& key, uint seed = 0)
{
    return ::qHash(key.m_src * 11u + key.m_dst * 23u + key.m_channel * 41u, seed);
}

#endif // INCLUDE_VDL2ATN_H
