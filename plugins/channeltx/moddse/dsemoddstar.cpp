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

#include <algorithm>
#include <cmath>
#include <cstring>

#include <QDebug>

#include "dsp/dv/dvbits.h"
#include "dsp/dv/dvcrc.h"
#include "dsp/dv/dvdstar.h"

#include "dsemoddstar.h"

// Last 24 symbols of the header sync (bit sync tail + 15 bit frame sync)
const uint8_t DSEModDStar::m_headerSync[DSEModDStar::m_frameSyncSymbols] = {
    1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 3, 3, 1, 3, 3, 1, 1, 3, 1, 3, 1, 1, 1, 1
};

// Voice frame sync carried in the 24 bit slow data field of the first frame of a superframe
const uint8_t DSEModDStar::m_voiceSync[DSEModDStar::m_dataBits] = {
    3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 3, 1, 3, 1, 1, 1, 3, 3, 1, 3, 1, 1, 1
};

// End of transmission: 32 bit sync bits then the 16 bit end pattern
const uint8_t DSEModDStar::m_endSequence[DSEModDStar::m_endBits] = {
    3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1,
    3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1,
    1, 1, 1, 3, 1, 1, 3, 3, 1, 3, 1, 3, 3, 3, 3, 1
};

DSEModDStar::DSEModDStar() :
    m_state(StateIdle),
    m_stopRequested(false),
    m_frameIndex(0),
    m_superframeIndex(0),
    m_voiceFrameCount(0),
    m_sendPosition(false),
    m_positionValid(false),
    m_latitude(0.0),
    m_longitude(0.0),
    m_altitude(0.0f),
    m_dprsOffset(0),
    m_dprsSuperframe(0)
{
    std::fill(m_header, m_header + m_headerBytes, 0);
    std::fill(m_headerBitsOnAir, m_headerBitsOnAir + m_headerBits, 0);
    std::memset(m_slowData, 0x66, sizeof(m_slowData));
}

DSEModDStar::~DSEModDStar()
{
}

QString DSEModDStar::fixLength(const QString& s, int len)
{
    QString r = s.toUpper().leftJustified(len, ' ', true);
    return r;
}

void DSEModDStar::buildHeader(const DSEModSettings& settings, uint8_t *header)
{
    std::fill(header, header + m_headerBytes, ' ');
    header[0] = (uint8_t) settings.m_dstarFlag1;
    header[1] = (uint8_t) settings.m_dstarFlag2;
    header[2] = (uint8_t) settings.m_dstarFlag3;

    QByteArray rpt2 = fixLength(settings.m_dstarRpt2, 8).toLatin1();
    QByteArray rpt1 = fixLength(settings.m_dstarRpt1, 8).toLatin1();
    QByteArray ur = fixLength(settings.m_dstarUrCall, 8).toLatin1();
    QByteArray my = fixLength(settings.m_dstarMyCall, 8).toLatin1();
    QByteArray sfx = fixLength(settings.m_dstarMySuffix, 4).toLatin1();

    std::memcpy(&header[3], rpt2.constData(), 8);
    std::memcpy(&header[11], rpt1.constData(), 8);
    std::memcpy(&header[19], ur.constData(), 8);
    std::memcpy(&header[27], my.constData(), 8);
    std::memcpy(&header[35], sfx.constData(), 4);

    uint16_t crc = DVDStar::headerCRC(header);
    header[39] = crc & 0xff;
    header[40] = (crc >> 8) & 0xff;
}

void DSEModDStar::encodeHeader(const uint8_t *header, uint8_t *bits)
{
    // Rate 1/2 K=3 convolutional code, 660 bit interleaver and scrambler
    DVDStar::headerEncode(header, bits);
}

void DSEModDStar::setPosition(bool valid, double latitude, double longitude, float altitude, int speed, int course, int source)
{
    (void) speed;
    (void) course;
    (void) source;
    m_positionValid = valid;
    m_latitude = latitude;
    m_longitude = longitude;
    m_altitude = altitude;
}

QByteArray DSEModDStar::buildDPRS(const QString& callsign, double latitude, double longitude, float altitude, const QDateTime& utc)
{
    // D-PRS (GPS-A): an APRS position report with timestamp, prefixed by "$$CRC" and the D-STAR
    // CRC of everything after the comma, CR LF included. APZSDR is the experimental APRS
    // destination (APZ = experimental) as SDRangel has no registered one.
    // In hundredths of a minute, rounded once: the minutes truncated and their hundredths rounded
    // apart put 12.996' at 12.00' (a minute, 1.8 km, out)
    const long long latHundredths = std::llround(std::fabs(latitude) * 6000.0);
    const long long lonHundredths = std::llround(std::fabs(longitude) * 6000.0);
    const int latDeg = (int) (latHundredths / 6000), lonDeg = (int) (lonHundredths / 6000);
    const int latMin = (int) ((latHundredths % 6000) / 100), lonMin = (int) ((lonHundredths % 6000) / 100);
    const int latHund = (int) (latHundredths % 100), lonHund = (int) (lonHundredths % 100);
    int altitudeFeet = std::max(0, std::min(999999, (int) std::lround(altitude / 0.3048f)));
    QString payload = QString("%1>APZSDR,DSTAR*:/%2h%3%4.%5%6/%7%8.%9%10-000/000/A=%11 SDRangel\r\n")
        .arg(callsign.trimmed().toUpper())
        .arg(utc.toString("HHmmss"))
        .arg(latDeg, 2, 10, QChar('0')).arg(latMin, 2, 10, QChar('0')).arg(latHund, 2, 10, QChar('0')).arg(latitude >= 0.0 ? 'N' : 'S')
        .arg(lonDeg, 3, 10, QChar('0')).arg(lonMin, 2, 10, QChar('0')).arg(lonHund, 2, 10, QChar('0')).arg(longitude >= 0.0 ? 'E' : 'W')
        .arg(altitudeFeet, 6, 10, QChar('0'));
    QByteArray bytes = payload.toLatin1();
    uint16_t crc = DVCRC::dstar16((const uint8_t *) bytes.constData(), bytes.size());
    return QString("$$CRC%1,").arg(crc, 4, 16, QChar('0')).toUpper().toLatin1() + bytes;
}

void DSEModDStar::buildSlowData()
{
    // Slow data for the 20 data frames of a superframe: 10 slots of 2 frames (6 bytes) each.
    // Even superframes (or all if there is no text) carry the radio header (9 slots + filler),
    // odd superframes carry the 20 character text message (4 slots + filler). Every
    // m_positionIntervalSuperframes a D-PRS sentence takes as many consecutive superframes as
    // it needs (GPS slots, 5 bytes each, then filler), receivers parse it at the next header or
    // text slot that starts a superframe.
    std::memset(m_slowData, 0x66, sizeof(m_slowData));
    uint8_t *bytes = &m_slowData[0][0];

    if (m_sendPosition && m_dprs.isEmpty() && (m_superframeIndex >= m_dprsSuperframe))
    {
        m_dprs = buildDPRS(QString::fromLatin1((const char *) &m_header[27], 8), m_latitude, m_longitude, m_altitude, QDateTime::currentDateTimeUtc());
        m_dprsOffset = 0;
        m_dprsSuperframe = m_superframeIndex + m_positionIntervalSuperframes;
    }

    if (!m_dprs.isEmpty())
    {
        for (int slot = 0; (slot < 10) && (m_dprsOffset < m_dprs.size()); slot++)
        {
            int nbBytes = std::min(5, (int) (m_dprs.size() - m_dprsOffset));
            bytes[6*slot] = 0x30 | nbBytes;

            for (int i = 0; i < nbBytes; i++) {
                bytes[6*slot + 1 + i] = (uint8_t) m_dprs.at(m_dprsOffset + i);
            }

            m_dprsOffset += nbBytes;
        }

        if (m_dprsOffset >= m_dprs.size()) {
            m_dprs.clear();
        }

        return;
    }

    bool sendText = !m_text.isEmpty() && ((m_superframeIndex & 1) == 1);

    if (sendText)
    {
        QByteArray text = m_text.toLatin1();

        for (int block = 0; block < 4; block++)
        {
            bytes[6*block] = 0x40 | block;

            for (int i = 0; i < 5; i++) {
                bytes[6*block + 1 + i] = (uint8_t) text.at(5*block + i);
            }
        }
    }
    else
    {
        for (int block = 0; block < 9; block++)
        {
            int nbBytes = block < 8 ? 5 : 1;
            bytes[6*block] = 0x50 | nbBytes;

            for (int i = 0; i < 5; i++) {
                bytes[6*block + 1 + i] = i < nbBytes ? m_header[5*block + i] : 0x66;
            }
        }
    }
}

void DSEModDStar::start(const DSEModSettings& settings)
{
    buildHeader(settings, m_header);
    encodeHeader(m_header, m_headerBitsOnAir);
    // (the text as written: callsigns are upper case, a message need not be)
    m_text = settings.m_dstarText.isEmpty() ? QString() : settings.m_dstarText.leftJustified(20, ' ', true);
    m_frameIndex = 0;
    m_superframeIndex = 0;
    m_voiceFrameCount = 0;
    m_stopRequested = false;
    m_sendPosition = settings.m_sendPosition && m_positionValid;
    m_dprs.clear();
    m_dprsOffset = 0;
    m_dprsSuperframe = 1; // header superframe first, then the position
    buildSlowData();
    m_state = StatePreamble;

    qDebug() << "DSEModDStar::start: MY:" << QString::fromLatin1((const char *) &m_header[27], 8)
        << "/" << QString::fromLatin1((const char *) &m_header[35], 4)
        << "UR:" << QString::fromLatin1((const char *) &m_header[19], 8)
        << "RPT1:" << QString::fromLatin1((const char *) &m_header[11], 8)
        << "RPT2:" << QString::fromLatin1((const char *) &m_header[3], 8);
}

void DSEModDStar::stop()
{
    m_stopRequested = true;
}

void DSEModDStar::abort()
{
    m_state = StateIdle;
    m_stopRequested = false;
}

void DSEModDStar::appendBits(std::vector<int8_t>& levels, const uint8_t *bits, int nbBits)
{
    for (int i = 0; i < nbBits; i++) {
        levels.push_back(bits[i] ? -3 : 3); // bit 0 -> positive deviation
    }
}

void DSEModDStar::appendSymbols(std::vector<int8_t>& levels, const uint8_t *symbols, int nbSymbols)
{
    for (int i = 0; i < nbSymbols; i++) {
        levels.push_back(symbols[i] == 1 ? 3 : -3); // dsdcc: 1 = positive, 3 = negative
    }
}

bool DSEModDStar::nextFrame(std::vector<int8_t>& levels)
{
    levels.clear();

    switch (m_state)
    {
    case StatePreamble:
    {
        // Alternating bit sync ending so that the header sync pattern continues the alternation
        for (int i = 0; i < m_bitSyncSymbols / 2; i++)
        {
            levels.push_back(3);
            levels.push_back(-3);
        }

        appendSymbols(levels, m_headerSync, m_frameSyncSymbols);
        m_state = StateHeader;
        return true;
    }
    case StateHeader:
    {
        appendBits(levels, m_headerBitsOnAir, m_headerBits);
        m_state = StateVoice;
        m_frameIndex = 0;
        return true;
    }
    case StateVoice:
    {
        uint8_t frame[DSEModVocoder::m_maxFrameBytes];
        uint8_t bits[m_voiceBits];
        pullVocoderFrame(frame);
        m_voiceFrameCount++;
        DVBits::bytesToBitsLSB(frame, 9, bits);
        appendBits(levels, bits, m_voiceBits);

        if (m_stopRequested)
        {
            // The end sequence takes the place of the data field after the last voice frame,
            // at any point of the superframe (receivers look for it anywhere)
            appendSymbols(levels, m_endSequence, m_endBits);
            m_state = StateIdle;
            m_stopRequested = false;
            return true;
        }

        if (m_frameIndex == 0) {
            appendSymbols(levels, m_voiceSync, m_dataBits);
        }
        else
        {
            uint8_t data[3];
            data[0] = m_slowData[m_frameIndex - 1][0] ^ 0x70;
            data[1] = m_slowData[m_frameIndex - 1][1] ^ 0x4f;
            data[2] = m_slowData[m_frameIndex - 1][2] ^ 0x93;
            uint8_t dataBits[m_dataBits];
            DVBits::bytesToBitsLSB(data, 3, dataBits);
            appendBits(levels, dataBits, m_dataBits);
        }

        m_frameIndex++;

        if (m_frameIndex == m_framesPerSuperframe)
        {
            m_frameIndex = 0;
            m_superframeIndex++;
            buildSlowData();
        }

        return true;
    }
    case StateEnd:
    case StateIdle:
    default:
        return false;
    }
}
