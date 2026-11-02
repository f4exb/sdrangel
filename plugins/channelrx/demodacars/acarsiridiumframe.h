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

#ifndef INCLUDE_ACARSIRIDIUMFRAME_H
#define INCLUDE_ACARSIRIDIUMFRAME_H

#include <QByteArray>
#include <QList>
#include <QString>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

// Iridium frame layer: takes the bit stream AcarsIridiumReceiver produces from the unique
// word onwards and turns it into a frame.
//
// The frame formats, the BCH polynomials and the interleaving are all taken from
// iridium-toolkit (bitsparser.py and bch.py), which is the only published description of
// them; the names used here follow its names so the two can be compared. Nothing here was
// guessed - where a constant appears without an obvious meaning, that is because it has
// none in the reference either.
//
// Two frame types matter to this plugin:
//
//  - IRA, the ring alert broadcast. It is simplex, continuous, and carries the
//    transmitting satellite's position, so it is what proves the receiver works: an
//    IRA frame that passes BCH gives a geocentric radius that must come out at the
//    Iridium orbit, a little over 7100 km, or the chain is wrong somewhere.
//
//  - IDA, the short burst data frames on the duplex traffic channels. These are what
//    carry ACARS: an aircraft's satcom unit sends its ACARS block as short burst data,
//    which arrives as the payload of a run of IDA frames that have to be put back
//    together in order before the block can be parsed.

class AcarsIridiumFrame
{
public:
    enum Type
    {
        TypeUnknown = 0,
        TypeIRA,                        //!< Ring alert broadcast, simplex
        TypeIBC,                        //!< Broadcast, duplex
        TypeIMS,                        //!< Paging / messaging
        TypeIDA,                        //!< Short burst data, which is what carries ACARS
        TypeLCW                         //!< A link control word frame that is not data
    };

    Type m_type = TypeUnknown;
    bool m_uplink = false;
    int m_bchErrorsFixed = 0;

    // IRA
    int m_satellite = 0;
    int m_beam = 0;
    double m_latitude = 0.0;            //!< Geocentric, degrees
    double m_longitude = 0.0;           //!< Degrees
    double m_radiusKm = 0.0;            //!< From the centre of the Earth
    double m_altitudeKm = 0.0;          //!< Above a spherical Earth, so only approximate
    int m_interval = 0;
    int m_slot = 0;
    int m_posX = 0;                     //!< Units of 4 km from the centre of the Earth
    int m_posY = 0;
    int m_posZ = 0;
    QList<quint32> m_tmsi;              //!< Paged subscribers

    // IDA
    int m_frameType = -1;               //!< The LCW frame type field, 2 for data
    int m_counter = 0;                  //!< Position of this frame in a sequence
    int m_dataLength = 0;
    bool m_continues = false;           //!< More frames follow in this sequence
    bool m_crcOk = false;
    QByteArray m_data;                  //!< Short burst data payload

    QString toString() const
    {
        switch (m_type)
        {
        case TypeIRA:
        {
            QString s = QString("IRA sat=%1 beam=%2 int=%3 slot=%4 xyz=%5,%6,%7 %8 %9 %10 km")
                            .arg(m_satellite).arg(m_beam).arg(m_interval).arg(m_slot)
                            .arg(m_posX).arg(m_posY).arg(m_posZ)
                            .arg(m_latitude, 0, 'f', 2).arg(m_longitude, 0, 'f', 2)
                            .arg(m_altitudeKm, 0, 'f', 0);

            for (int i = 0; i < m_tmsi.size(); i++) {
                s += QString(" tmsi:%1").arg(m_tmsi[i], 8, 16, QChar('0'));
            }

            return s;
        }
        case TypeIDA:
            return QString("IDA ctr=%1 len=%2%3 crc=%4 [%5]")
                       .arg(m_counter).arg(m_dataLength)
                       .arg(m_continues ? "+" : " ")
                       .arg(m_crcOk ? "ok" : "no")
                       .arg(QString(m_data.toHex('.')));
        case TypeIBC:
            return QString("IBC");
        case TypeIMS:
            return QString("IMS");
        case TypeLCW:
            return QString("LCW ft=%1").arg(m_frameType);
        default:
            return QString("?");
        }
    }
};

class AcarsIridiumFrameDecoder
{
public:

    // BCH generator polynomials, as iridium-toolkit names them
    static const int RingAlertBchPoly = 1207;
    static const int MessagingBchPoly = 1897;
    static const int AcchBchPoly = 3545;
    static const int HeaderPoly = 29;

    //! The access codes, which are the differential unique words after Gray coding, and
    //! so are what the frame bit stream starts with
    static const char *downlinkAccess() { return "001100000011000011110011"; }
    static const char *uplinkAccess()   { return "110011000011110011111100"; }

    //! The fixed 32 bit header a paging frame starts with
    static const char *messagingHeader() { return "00110011111100110011001111110011"; }

    //! CRC-16/CCITT-FALSE, which short burst data frames carry. Public so a test can
    //! check it against the published check value for this CRC
    static quint16 crc16CcittFalse(const QByteArray& data)
    {
        quint16 crc = 0xffff;

        for (int i = 0; i < data.size(); i++)
        {
            crc ^= (quint16) ((quint8) data[i]) << 8;

            for (int b = 0; b < 8; b++)
            {
                if (crc & 0x8000) {
                    crc = (quint16) ((crc << 1) ^ 0x1021);
                } else {
                    crc = (quint16) (crc << 1);
                }
            }
        }

        return crc;
    }

    bool decode(const std::vector<uint8_t>& frameBits, AcarsIridiumFrame& frame) const
    {
        const std::string bits = toString(frameBits);

        if (bits.size() < 24) {
            return false;
        }

        // The receiver allows the unique word two wrong symbols, which is up to four
        // wrong bits here, so the access code is matched to the same tolerance rather
        // than exactly - otherwise the weaker bursts it syncs would be dropped again
        const int downlinkErrors = bitErrors(bits, downlinkAccess());
        const int uplinkErrors = bitErrors(bits, uplinkAccess());

        if (std::min(downlinkErrors, uplinkErrors) > 4) {
            return false;
        }

        frame.m_uplink = (uplinkErrors < downlinkErrors);

        const std::string data = bits.substr(24);

        if (decodeRingAlert(data, frame)) {
            return true;
        }

        if (decodeMessaging(data, frame)) {
            return true;
        }

        if (decodeLcw(data, frame)) {
            return true;
        }

        return false;
    }

private:

    static std::string toString(const std::vector<uint8_t>& bits)
    {
        std::string s;
        s.reserve(bits.size());

        for (size_t i = 0; i < bits.size(); i++) {
            s.push_back(bits[i] ? '1' : '0');
        }

        return s;
    }

    //! How many of the first bits of the stream differ from the given pattern
    static int bitErrors(const std::string& bits, const char *pattern)
    {
        int errors = 0;

        for (int i = 0; pattern[i] != 0; i++)
        {
            if (bits[i] != pattern[i]) {
                errors++;
            }
        }

        return errors;
    }

    static int bitLength(quint32 value)
    {
        int length = 0;

        while (value)
        {
            length++;
            value >>= 1;
        }

        return length;
    }

    //! Remainder of a polynomial division in GF(2), which is what a BCH syndrome is. The
    //! same as nndivide() in iridium-toolkit
    static quint32 gfMod(quint32 poly, quint32 num)
    {
        if (num == 0) {
            return 0;
        }

        int shift = bitLength(num) - bitLength(poly);
        quint32 top = 1u << (bitLength(num) - 1);

        while (shift >= 0)
        {
            if (num >= top) {
                num ^= (poly << shift);
            }

            top >>= 1;
            shift--;
        }

        return num;
    }

    static quint32 toInt(const std::string& bits, size_t start, size_t length)
    {
        quint32 value = 0;

        for (size_t i = 0; i < length; i++) {
            value = (value << 1) | (quint32) (bits[start + i] == '1' ? 1 : 0);
        }

        return value;
    }

    static std::string fromInt(quint32 value, int length)
    {
        std::string s(length, '0');

        for (int i = length - 1; i >= 0; i--)
        {
            s[i] = (value & 1) ? '1' : '0';
            value >>= 1;
        }

        return s;
    }

    //! Corrects up to two bit errors in a BCH block by trying every flip, which for the
    //! block lengths here is at most a few hundred divisions. iridium-toolkit builds
    //! syndrome tables instead, but a burst arrives only every 90 ms, so there is nothing
    //! to be gained from the tables and this needs no initialisation
    //!
    //! Returns the number of errors corrected, or -1 when the block cannot be corrected
    static int bchRepair(int poly, const std::string& block, std::string& repaired)
    {
        const int length = (int) block.size();
        const quint32 value = toInt(block, 0, length);

        if (gfMod(poly, value) == 0)
        {
            repaired = block;
            return 0;
        }

        for (int b1 = 0; b1 < length; b1++)
        {
            const quint32 try1 = value ^ (1u << b1);

            if (gfMod(poly, try1) == 0)
            {
                repaired = fromInt(try1, length);
                return 1;
            }
        }

        for (int b1 = 0; b1 < length; b1++)
        {
            const quint32 try1 = value ^ (1u << b1);

            for (int b2 = b1 + 1; b2 < length; b2++)
            {
                const quint32 try2 = try1 ^ (1u << b2);

                if (gfMod(poly, try2) == 0)
                {
                    repaired = fromInt(try2, length);
                    return 2;
                }
            }
        }

        return -1;
    }

    //! The data part of a corrected BCH block, which is everything above the check bits
    static std::string bchData(int poly, const std::string& repaired)
    {
        return repaired.substr(0, repaired.size() - (bitLength((quint32) poly) - 1));
    }

    static int onesCount(const std::string& bits)
    {
        int count = 0;

        for (size_t i = 0; i < bits.size(); i++)
        {
            if (bits[i] == '1') {
                count++;
            }
        }

        return count;
    }

    //! The two way interleave: bit pairs are swapped and then read backwards, alternating
    //! between the two output blocks. de_interleave() in iridium-toolkit
    static void deInterleave(const std::string& group, std::string& odd, std::string& even)
    {
        std::vector<std::string> symbols;

        for (size_t z = 0; (z + 1) < group.size(); z += 2) {
            symbols.push_back(std::string(1, group[z+1]) + std::string(1, group[z]));
        }

        odd.clear();
        even.clear();

        for (int x = (int) symbols.size() - 1; x >= 0; x -= 2) {
            odd += symbols[x];
        }

        for (int x = (int) symbols.size() - 2; x >= 0; x -= 2) {
            even += symbols[x];
        }
    }

    //! The three way interleave the ring alert header uses. de_interleave3()
    static void deInterleave3(const std::string& group, std::string& first, std::string& second, std::string& third)
    {
        std::vector<std::string> symbols;

        for (size_t z = 0; (z + 1) < group.size(); z += 2) {
            symbols.push_back(std::string(1, group[z+1]) + std::string(1, group[z]));
        }

        first.clear();
        second.clear();
        third.clear();

        for (int x = (int) symbols.size() - 1; x >= 0; x -= 3) {
            first += symbols[x];
        }

        for (int x = (int) symbols.size() - 2; x >= 0; x -= 3) {
            second += symbols[x];
        }

        for (int x = (int) symbols.size() - 3; x >= 0; x -= 3) {
            third += symbols[x];
        }
    }

    //! The link control word interleave, which is a fixed table rather than a pattern.
    //! de_interleave_lcw(); the table is one based, as it is in the reference
    static void deInterleaveLcw(const std::string& bits, std::string& lcw1, std::string& lcw2, std::string& lcw3)
    {
        static const int table[46] = {
            40, 39, 36, 35, 32, 31, 28, 27, 24, 23, 20, 19, 16, 15, 12, 11,  8,  7,  4,  3,
            41, 38, 37, 34, 33, 30, 29, 26, 25, 22, 21, 18, 17, 14, 13, 10,  9,  6,  5,  2,
             1, 46, 45, 44, 43, 42
        };

        std::string all;

        for (int i = 0; i < 46; i++) {
            all += bits[table[i] - 1];
        }

        lcw1 = all.substr(0, 7);
        lcw2 = all.substr(7, 13);
        lcw3 = all.substr(20);
    }

    //! Runs a run of 32 bit BCH blocks, each of which carries 21 data bits plus a parity
    //! bit, and concatenates what they decode to. Used by IRA and IMS
    static bool bchBlocks32(const std::vector<std::string>& blocks, int poly, std::string& out, int& fixed)
    {
        out.clear();
        fixed = 0;

        for (size_t i = 0; i < blocks.size(); i++)
        {
            if (blocks[i].size() != 32) {
                break;
            }

            const std::string parity = blocks[i].substr(31, 1);
            std::string repaired;
            const int errors = bchRepair(poly, blocks[i].substr(0, 31), repaired);

            if (errors < 0) {
                break;
            }

            // The spare bit makes the whole 32 even, which catches a block the BCH has
            // "corrected" into a different valid codeword
            if (((onesCount(repaired) + onesCount(parity)) % 2) == 1) {
                break;
            }

            fixed += (errors > 0) ? 1 : 0;
            out += bchData(poly, repaired);
        }

        return !out.empty();
    }

    //! IRA, the ring alert broadcast. Three 32 bit blocks interleaved three ways carry a
    //! 63 bit header, and any further 64 bit groups carry pages
    bool decodeRingAlert(const std::string& data, AcarsIridiumFrame& frame) const
    {
        if (data.size() < 96) {
            return false;
        }

        std::string ra1, ra2, ra3;
        deInterleave3(data.substr(0, 96), ra1, ra2, ra3);

        // Classified on all three blocks being valid before anything is corrected, which
        // is what keeps a frame of another type from being read as a ring alert
        if ((gfMod(RingAlertBchPoly, toInt(ra1, 0, 31)) != 0)
         || (gfMod(RingAlertBchPoly, toInt(ra2, 0, 31)) != 0)
         || (gfMod(RingAlertBchPoly, toInt(ra3, 0, 31)) != 0)) {
            return false;
        }

        std::vector<std::string> blocks;
        blocks.push_back(ra1);
        blocks.push_back(ra2);
        blocks.push_back(ra3);

        for (size_t p = 96; (p + 64) <= data.size(); p += 64)
        {
            std::string b1, b2;
            deInterleave(data.substr(p, 64), b1, b2);
            blocks.push_back(b1);
            blocks.push_back(b2);
        }

        std::string decoded;
        int fixed = 0;

        if (!bchBlocks32(blocks, RingAlertBchPoly, decoded, fixed)) {
            return false;
        }

        if (decoded.size() < 63) {
            return false;
        }

        frame.m_type = AcarsIridiumFrame::TypeIRA;
        frame.m_bchErrorsFixed = fixed;
        frame.m_satellite = (int) toInt(decoded, 0, 7);
        frame.m_beam = (int) toInt(decoded, 7, 6);

        // Three signed 12 bit positions, in units of 4 km from the centre of the Earth
        const double x = signed12(decoded, 13);
        const double y = signed12(decoded, 25);
        const double z = signed12(decoded, 37);

        frame.m_interval = (int) toInt(decoded, 49, 7);
        frame.m_slot = (int) toInt(decoded, 56, 1);
        frame.m_posX = (int) x;
        frame.m_posY = (int) y;
        frame.m_posZ = (int) z;

        // Geocentric rather than geodetic, so up to 0.2 degrees out in latitude
        frame.m_latitude = std::atan2(z, std::sqrt((x * x) + (y * y))) * 180.0 / M_PI;
        frame.m_longitude = std::atan2(y, x) * 180.0 / M_PI;
        frame.m_radiusKm = std::sqrt((x * x) + (y * y) + (z * z)) * 4.0;
        frame.m_altitudeKm = frame.m_radiusKm - 6371.0;

        // Pages of 42 bits, ending with an all ones page
        for (size_t p = 63; (p + 42) <= decoded.size(); p += 42)
        {
            const std::string page = decoded.substr(p, 42);

            if (onesCount(page) == 42) {
                break;
            }

            frame.m_tmsi.append(toInt(page, 0, 32));
        }

        return true;
    }

    static double signed12(const std::string& bits, size_t start)
    {
        const double magnitude = (double) toInt(bits, start + 1, 11);

        return magnitude - ((bits[start] == '1') ? 2048.0 : 0.0);
    }

    //! IMS, the paging frames, which start with a fixed header
    bool decodeMessaging(const std::string& data, AcarsIridiumFrame& frame) const
    {
        if (data.size() < 32) {
            return false;
        }

        if (data.compare(0, 32, messagingHeader()) != 0) {
            return false;
        }

        std::vector<std::string> blocks;

        for (size_t p = 32; (p + 64) <= data.size(); p += 64)
        {
            std::string b1, b2;
            deInterleave(data.substr(p, 64), b1, b2);
            blocks.push_back(b1);
            blocks.push_back(b2);
        }

        std::string decoded;
        int fixed = 0;
        bchBlocks32(blocks, MessagingBchPoly, decoded, fixed);

        frame.m_type = AcarsIridiumFrame::TypeIMS;
        frame.m_bchErrorsFixed = fixed;

        return true;
    }

    //! A duplex traffic frame. The link control word says what the rest of it is, and
    //! frame type 2 is short burst data, which is the one that can carry ACARS
    bool decodeLcw(const std::string& data, AcarsIridiumFrame& frame) const
    {
        if (data.size() < 64) {
            return false;
        }

        std::string lcw1, lcw2, lcw3;
        deInterleaveLcw(data.substr(0, 46), lcw1, lcw2, lcw3);

        if ((gfMod(HeaderPoly, toInt(lcw1, 0, 7)) != 0) || (gfMod(41, toInt(lcw3, 0, 26)) != 0)) {
            return false;
        }

        // One bit of the middle word is not transmitted, so both values are tried
        std::string repaired2;
        int errors2 = bchRepair(465, lcw2 + "0", repaired2);

        if (errors2 != 0) {
            errors2 = bchRepair(465, lcw2 + "1", repaired2);
        }

        if (errors2 != 0) {
            return false;
        }

        frame.m_frameType = (int) toInt(bchData(HeaderPoly, lcw1), 0, 3);
        frame.m_type = AcarsIridiumFrame::TypeLCW;

        if (frame.m_frameType != 2) {
            return true;
        }

        if (data.size() < (46 + 312)) {
            return true;
        }

        return decodeData(data.substr(46, 312), frame);
    }

    //! Short burst data. The payload is carried in 31 bit BCH blocks that have been
    //! interleaved in two stages and then shuffled, and the order below is the reference
    //! order - there is no pattern in it to derive
    bool decodeData(const std::string& payload, AcarsIridiumFrame& frame) const
    {
        std::vector<std::string> blocks;
        size_t p = 0;

        for (; (p + 124) <= payload.size(); p += 124)
        {
            std::string b1, b2;
            deInterleave(payload.substr(p, 124), b1, b2);

            const std::string joined = b1 + b2;

            if (joined.size() < 124) {
                return false;
            }

            blocks.push_back(joined.substr(93, 31));
            blocks.push_back(joined.substr(31, 31));
            blocks.push_back(joined.substr(62, 31));
            blocks.push_back(joined.substr(0, 31));
        }

        std::string tail = payload.substr(p);

        if ((tail.size() % 2) == 1) {
            tail = tail.substr(0, tail.size() - 1);
        }

        if (tail.size() >= 64)
        {
            std::string b1, b2;
            deInterleave(tail, b1, b2);

            // Each of these has a bit more than the block, which is thrown away
            blocks.push_back(b2.substr(1));
            blocks.push_back(b1.substr(1));
        }

        std::string decoded;
        int fixed = 0;

        for (size_t i = 0; i < blocks.size(); i++)
        {
            if (blocks[i].size() != 31) {
                return false;
            }

            std::string repaired;
            const int errors = bchRepair(AcchBchPoly, blocks[i], repaired);

            if (errors < 0) {
                return false;
            }

            fixed += (errors > 0) ? 1 : 0;
            decoded += bchData(AcchBchPoly, repaired);
        }

        if (decoded.size() < (9 * 20 + 16)) {
            return false;
        }

        // Three bits that are always zero in a valid frame, and so are the last check
        // that this really is short burst data rather than a frame that happened to pass
        if (toInt(decoded, 17, 3) != 0) {
            return false;
        }

        frame.m_type = AcarsIridiumFrame::TypeIDA;
        frame.m_bchErrorsFixed = fixed;
        frame.m_continues = (decoded[3] == '1');
        frame.m_counter = (int) toInt(decoded, 5, 3);
        frame.m_dataLength = (int) toInt(decoded, 11, 5);

        for (size_t i = 20; (i + 8) <= (9 * 20); i += 8) {
            frame.m_data.append((char) toInt(decoded, i, 8));
        }

        if (frame.m_dataLength > 0)
        {
            // The check is over the frame with the length field zeroed and the trailing
            // four bits dropped, and includes the transmitted value, so a good frame
            // gives a remainder of zero
            std::string crcStream = decoded.substr(0, 20) + std::string(12, '0')
                                  + decoded.substr(20, decoded.size() - 24);
            QByteArray bytes;

            for (size_t i = 0; (i + 8) <= crcStream.size(); i += 8) {
                bytes.append((char) toInt(crcStream, i, 8));
            }

            frame.m_crcOk = (crc16CcittFalse(bytes) == 0);

            if (frame.m_dataLength <= (int) frame.m_data.size()) {
                frame.m_data = frame.m_data.left(frame.m_dataLength);
            }
        }

        return true;
    }

};

// Puts a run of IDA frames back together into one short burst data message, and pulls the
// ACARS block out of it.
//
// An ACARS block is far longer than the twenty bytes an IDA frame carries, so it arrives
// split across a run of frames that have to be collected in order. The rules for deciding
// that a frame continues a run are from ReassembleIDA in iridium-toolkit: the counter has
// to advance by one modulo eight, the direction has to match, and the frame has to arrive
// close enough in time and frequency to be the same transmitter. A frame whose
// continuation bit is clear ends the run.
//
// Confirmed on air: runs of several frames complete rather than time out, and ACARS
// blocks have been read out of the messages they build, passing the block CRC and
// accepted by AcarsMessage::decode
class AcarsIridiumReassembler
{
public:

    // Tolerances from ReassembleIDA in iridium-toolkit, in milliseconds
    static const qint64 ContinuationMs = 280;
    static const qint64 TimeoutMs = 1000;
    static const int FrequencyToleranceHz = 260;

    //! How far apart two frequencies in the band are, taking it as circular.
    //!
    //! The channelizer's output is periodic in the input rate, so its last channel sits
    //! next to its first: a burst at one end of the band is reported near +rate/2 when it
    //! is found in the channel below the fold and near -rate/2 when it is found in the
    //! channel above, although the two are one spacing apart. A plain difference makes
    //! that pair look a whole band apart, which both hides a duplicate and splits a run
    //! of fragments that should have been joined
    static double separation(double a, double b, double bandWidth)
    {
        double d = std::fabs(a - b);

        if (bandWidth > 0.0)
        {
            d = std::fmod(d, bandWidth);

            if (d > (bandWidth / 2.0)) {
                d = bandWidth - d;
            }
        }

        return d;
    }

    //! The width of the band the frequencies passed to addFrame are measured in, so that
    //! the fold at its edge can be allowed for. Zero, the default, treats them as a plain
    //! line, which is what a single channel receiver wants
    void setBandWidth(double bandWidth) { m_bandWidth = bandWidth; }

    //! Feeds a frame. Returns true and fills message when a run has been completed
    bool addFrame(const AcarsIridiumFrame& frame, qint64 timeMs, double frequency, QByteArray& message)
    {
        if (frame.m_type != AcarsIridiumFrame::TypeIDA) {
            return false;
        }

        // Only frames whose CRC passed are joined up: a bad frame in the middle of a run
        // would corrupt the block without any later check noticing
        if (!frame.m_crcOk) {
            return false;
        }

        expire(timeMs);

        for (int i = 0; i < m_partial.size(); i++)
        {
            Partial& partial = m_partial[i];

            if ((partial.m_uplink != frame.m_uplink)
             || (separation(partial.m_frequency, frequency, m_bandWidth) > FrequencyToleranceHz)
             || (timeMs < partial.m_timeMs)
             || ((timeMs - partial.m_timeMs) > ContinuationMs)
             || (((partial.m_counter + 1) % 8) != frame.m_counter)) {
                continue;
            }

            partial.m_data.append(frame.m_data);
            partial.m_counter = frame.m_counter;
            partial.m_timeMs = timeMs;
            partial.m_frequency = frequency;

            if (frame.m_continues) {
                return false;
            }

            message = partial.m_data;
            m_partial.removeAt(i);
            m_completed++;

            return true;
        }

        if (frame.m_counter != 0)
        {
            // A fragment whose run was never seen, so there is nothing to attach it to
            m_orphans++;
            return false;
        }

        if (!frame.m_continues)
        {
            // A whole message in one frame
            message = frame.m_data;
            m_completed++;

            return true;
        }

        Partial partial;
        partial.m_data = frame.m_data;
        partial.m_counter = frame.m_counter;
        partial.m_timeMs = timeMs;
        partial.m_frequency = frequency;
        partial.m_uplink = frame.m_uplink;
        m_partial.append(partial);

        return false;
    }

    //! Strips the short burst data transport from a reassembled message, leaving the
    //! payload an application sent.
    //!
    //! A reassembled message is NOT an ACARS block, and missing this layer is why none
    //! were ever found: every message starts with a two byte type, and the bytes seen on
    //! air - 0x7608, 0x0600, 0x7605 - are those types, not content. Only once the type
    //! and the headers behind it are off does the payload begin, and only then does a
    //! leading SOH mean ACARS. Looking for SOH at the front of the whole message tests a
    //! byte that can never be 1.
    //!
    //! Follows process_l2() of ReassembleIDASBD in iridium-toolkit
    static bool sbdPayload(const QByteArray& message, bool uplink, QByteArray& payload)
    {
        if (message.size() < 5) {
            return false;
        }

        const quint8 type0 = (quint8) message[0];
        const quint8 type1 = (quint8) message[1];

        // Only two of the type codes carry short burst data at all
        if (type0 == 0x76)
        {
            if (type1 == 0x05) {
                return false;
            }

            // Which half of the link it is decides the range of subtypes that make sense
            if (uplink)
            {
                if ((type1 < 0x0c) || (type1 > 0x0e)) {
                    return false;
                }
            }
            else
            {
                if ((type1 < 0x08) || (type1 > 0x0b)) {
                    return false;
                }
            }
        }
        else if (type0 == 0x06)
        {
            if (type1 != 0x00) {
                return false;
            }

            const quint8 subType = (quint8) message[2];

            if ((subType != 0x00) && (subType != 0x10) && (subType != 0x20)
             && (subType != 0x40) && (subType != 0x50) && (subType != 0x70)) {
                return false;
            }
        }
        else
        {
            return false;
        }

        QByteArray data = message.mid(2);

        if ((type0 == 0x06) && (type1 == 0x00))
        {
            // The long form, whose header is a fixed 29 bytes
            if ((data.size() < 30) || (((quint8) data[0]) != 0x20)) {
                return false;
            }

            data = data.mid(29);
        }
        else
        {
            if ((type0 == 0x76) && (type1 == 0x08))
            {
                const quint8 lead = (quint8) data[0];
                const int length = (lead == 0x20) ? 5 : 7;

                if (data.size() <= length) {
                    return false;
                }

                data = data.mid(length);
            }

            // An acknowledgement in front of the payload, on the uplink only
            if (uplink && (data.size() >= 3)
             && ((((quint8) data[0]) == 0x50) || (((quint8) data[0]) == 0x51))) {
                data = data.mid(3);
            }

            if (data.isEmpty()) {
                return false;
            }

            // Three more bytes: a marker, the payload length, and which message this is
            if ((data.size() > 3) && (((quint8) data[0]) == 0x10))
            {
                const int length = (quint8) data[1];

                data = data.mid(3);

                if (data.size() < length) {
                    return false;
                }

                data = data.left(length);
            }
            else
            {
                // A payload with no header. iridium-toolkit warns and keeps it
            }
        }

        if (data.isEmpty()) {
            return false;
        }

        payload = data;

        return true;
    }

    //! The ACARS block inside a reassembled short burst data message, if it holds one.
    //! What counts as ACARS follows consume_l2() of ReassembleIDASBDACARS in
    //! iridium-toolkit; the form the block is handed back in does NOT.
    //!
    //! iridium-toolkit strips the block down for its own parser - leading SOH off,
    //! trailing check and DEL off, parity masked away. AcarsMessage::decode in this
    //! plugin wants the opposite: the whole ARINC 618 block exactly as transmitted, from
    //! SOH to DEL, with the odd parity still in bit 7 of every character. That is what
    //! VDL-2, HFDL and Aero all hand it - see the note above acarsBlock() in acarsaero.h -
    //! and SOH and DEL already have odd parity, so they compare equal to 0x01 and 0x7F
    //! without masking. Handing over a stripped block instead would simply be rejected.
    //! Confirmed on air - the blocks this hands over are accepted and their CRCs pass
    static bool acarsBlock(const QByteArray& reassembled, bool uplink, QByteArray& block)
    {
        QByteArray message;

        // The short burst data transport first: what arrives is a typed, headed message,
        // and the ACARS block is its payload
        if (!sbdPayload(reassembled, uplink, message)) {
            return false;
        }

        // An ACARS block starts with SOH, which is what says this payload is ACARS
        if ((message.size() <= 2) || (((quint8) message[0]) != 0x01)) {
            return false;
        }

        // A trailing DEL marks a message that carries its own check, in the two bytes
        // before it. They do not carry parity, so everything below stops before them
        int bodyEnd = message.size();
        QByteArray check;

        if (((quint8) message[message.size() - 1]) == 0x7f)
        {
            check = message.mid(message.size() - 3, 2);
            bodyEnd = message.size() - 3;
        }

        // Some messages carry eight bytes of undocumented header between the SOH and the
        // block proper, recognised by the 3 that starts it
        const bool header = (bodyEnd > 9) && (((quint8) message[1]) == 0x03);
        const int bodyStart = header ? 9 : 1;

        if ((bodyEnd - bodyStart) < 13) {
            return false;
        }

        if (!check.isEmpty())
        {
            // The check covers the block and itself, and NOT the eight byte header - the
            // reference strips the header before computing it. Including the header made
            // every message that had one fail, which is most of them: of three captured
            // off the air, two had a header and were silently thrown away while the one
            // without it decoded, so the fault looked like ACARS simply being rare
            if (crc16Kermit(message.mid(bodyStart, bodyEnd - bodyStart) + check) != 0) {
                return false;
            }
        }

        // Every character of an ACARS block has odd parity in bit 7. Testing it is worth
        // doing - it is the only check a message without a trailing DEL gets - but the
        // bits stay on, because that is the form the decoder and libacars want
        for (int i = bodyStart; i < bodyEnd; i++)
        {
            quint8 c = (quint8) message[i];
            int ones = 0;

            for (int b = 0; b < 8; b++)
            {
                if (c & (1 << b)) {
                    ones++;
                }
            }

            if ((ones % 2) == 0) {
                return false;
            }
        }

        // SOH, then the block, then its check and DEL: the layout AcarsMessage::decode
        // expects, with the optional header taken out from between them
        block = QByteArray(1, (char) 0x01) + message.mid(bodyStart);

        return true;
    }

    int completedCount() const { return m_completed; }
    int orphanCount() const { return m_orphans; }
    int brokenCount() const { return m_broken; }

    //! CRC-16/KERMIT, which is what an ACARS block inside short burst data is checked
    //! with. Reflected, so unlike the one the frame layer uses. Public so a test can
    //! check it against the published check value for this CRC
    static quint16 crc16Kermit(const QByteArray& data)
    {
        quint16 crc = 0;

        for (int i = 0; i < data.size(); i++)
        {
            crc ^= (quint16) ((quint8) data[i]);

            for (int b = 0; b < 8; b++)
            {
                if (crc & 1) {
                    crc = (quint16) ((crc >> 1) ^ 0x8408);
                } else {
                    crc = (quint16) (crc >> 1);
                }
            }
        }

        return crc;
    }

private:

    struct Partial
    {
        QByteArray m_data;
        int m_counter = 0;
        qint64 m_timeMs = 0;
        double m_frequency = 0.0;
        bool m_uplink = false;
    };

    void expire(qint64 timeMs)
    {
        for (int i = m_partial.size() - 1; i >= 0; i--)
        {
            if ((timeMs - m_partial[i].m_timeMs) >= TimeoutMs)
            {
                m_partial.removeAt(i);
                m_broken++;
            }
        }
    }

    QList<Partial> m_partial;
    double m_bandWidth = 0.0;
    int m_completed = 0;
    int m_orphans = 0;
    int m_broken = 0;
};

#endif // INCLUDE_ACARSIRIDIUMFRAME_H
