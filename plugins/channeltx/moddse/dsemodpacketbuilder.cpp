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

#include "dsemodpacketbuilder.h"
#include "dsemoddstar.h"
#include "dsemoddmr.h"
#include "dsp/dv/dvbits.h"
#include "dsp/dv/dvdstar.h"
#include "dsp/dv/dvdmr.h"
#include "dsp/dv/dvysf.h"

namespace {

void dstarBytes(const QByteArray& data, std::vector<int8_t>& levels)
{
    for (unsigned char byte : data)
    {
        for (int i = 0; i < 8; ++i) {
            levels.push_back((byte & (1 << i)) ? -3 : 3);
        }
    }
}

void ysfDibits(const QByteArray& data, uint8_t *dibits)
{
    for (int i = 0; i < 480; ++i) {
        dibits[i] = ((quint8) data[i / 4] >> (6 - 2 * (i % 4))) & 3;
    }
}

void dstarEnd(std::vector<int8_t>& levels)
{
    for (auto symbol : DSEModDStar::m_endSequence) {
        levels.push_back((symbol == 1) ? 3 : -3);
    }
}

}

bool DSEModPacketBuilder::validate(const DSEModTxSession::Frame& f, QString& error)
{
    if (f.format == "dstar-header")
    {
        const auto *p = (const uint8_t*) f.data.constData();

        if (DVDStar::headerCRC(p) != (p[39] | (p[40] << 8)))
        {
            error = "D-Star header CRC failed";
            return false;
        }
    }
    else if (f.format == "ysf-frame")
    {
        uint8_t dibits[480];
        ysfDibits(f.data, dibits);
        DVYSF::FICH fich;

        if ((f.data.left(5) != QByteArray::fromHex("d471c9634d")) || (DVYSF::fichDecode(dibits + 20, fich) < 0))
        {
            error = "YSF sync/FICH validation failed";
            return false;
        }

        if ((fich.m_dt != 2) || (fich.m_fi > 2))
        {
            error = "YSF gateway input currently supports V/D type 2";
            return false;
        }
    }
    else if ((f.format == "dmr-burst") && f.type.startsWith("data-"))
    {
        uint8_t bits[264];
        uint8_t st[20];
        DVBits::bytesToBitsMSB((const uint8_t*) f.data.constData(), 33, bits);
        std::copy(bits + 98, bits + 108, st);
        std::copy(bits + 156, bits + 166, st + 10);
        int cc, type;

        if ((DVDMR::slotTypeDecode(st, cc, type) < 0) || (type != f.type.mid(5).toInt()))
        {
            error = "DMR slot type does not match frameType";
            return false;
        }

        if ((type == 1) || (type == 2))
        {
            uint8_t coded[196];
            uint8_t info[96];
            uint8_t lc[9];
            std::copy(bits, bits + 98, coded);
            std::copy(bits + 166, bits + 264, coded + 98);
            const uint8_t mask = (type == 1) ? DVDMR::m_maskVoiceLCHeader : DVDMR::m_maskTerminatorWithLC;

            if ((DVDMR::bptcDecode(coded, info) < 0) || (DVDMR::fullLCDecode(info, mask, lc) < 0))
            {
                error = "DMR link control validation failed";
                return false;
            }
        }
    }

    return true;
}

void DSEModPacketBuilder::reset(const DSEModSettings& settings)
{
    m_settings = settings;
    m_started = false;
    m_ended = false;
    m_cach = 0;
    m_header.clear();
}

bool DSEModPacketBuilder::append(const DSEModTxSession::Frame& f, std::vector<int8_t>& levels, QString& error)
{
    levels.clear();

    if (m_ended)
    {
        error = "Frame follows protocol terminator";
        return false;
    }

    if (f.format == "dstar-header")
    {
        if (m_started)
        {
            if (f.data == m_header) {
                return true; // gateways repeat the header
            }

            error = "D-Star header changed within a session";
            return false;
        }

        for (int i = 0; i < DSEModDStar::m_bitSyncSymbols; ++i) {
            levels.push_back((i & 1) ? -3 : 3);
        }

        for (auto symbol : DSEModDStar::m_headerSync) {
            levels.push_back((symbol == 1) ? 3 : -3);
        }

        uint8_t bits[660];
        DSEModDStar::encodeHeader((const uint8_t*) f.data.constData(), bits);

        for (auto bit : bits) {
            levels.push_back(bit ? -3 : 3);
        }

        m_header = f.data;
    }
    else if (f.format == "dstar-voice")
    {
        if (!m_started)
        {
            error = "D-Star session requires a header first";
            return false;
        }

        dstarBytes((f.type == "end") ? f.data.left(9) : f.data, levels);

        if (f.type == "end")
        {
            dstarEnd(levels);
            m_ended = true;
        }
    }
    else if (f.format == "ysf-frame")
    {
        uint8_t dibits[480];
        ysfDibits(f.data, dibits);
        DVYSF::FICH fich;
        DVYSF::fichDecode(dibits + 20, fich);

        if (!m_started && (fich.m_fi != 0))
        {
            error = "YSF session requires a header first";
            return false;
        }

        if (fich.m_fi == 0) {
            m_header = f.data;
        }

        for (auto dibit : dibits) {
            levels.push_back(DSEModProtocol::dibitToLevel(dibit));
        }

        m_ended = (fich.m_fi == 2);
    }
    else
    {
        if (!m_started && (f.type != "data-1") && (f.type != "data-6") && (f.type != "data-3"))
        {
            error = "DMR session requires a voice LC header, data header or CSBK first";
            return false;
        }

        if (f.type == "data-1") {
            m_header = f.data;
        }

        dmr(f.data, f.type, levels);
        m_ended = (f.type == "data-2");
    }

    m_started = true;
    return true;
}

void DSEModPacketBuilder::dmr(const QByteArray& data, const QString& type, std::vector<int8_t>& levels)
{
    uint8_t burst[264];
    uint8_t idleInfo[96] = {};
    uint8_t idle[196];
    DVBits::bytesToBitsMSB((const uint8_t*) data.constData(), 33, burst);
    DVDMR::bptcEncode(idleInfo, idle);
    static const int lcss[] = {1, 3, 3, 2};

    for (int slot = 1; slot <= 2; ++slot)
    {
        uint8_t bits[288];
        const int fragment = lcss[m_cach++ % 4];

        if (slot != m_settings.m_dmrSlot)
        {
            DSEModDMR::buildDataBurst(m_settings.m_dmrColorCode, 9, idle, slot - 1, fragment, nullptr, bits);
        }
        else
        {
            DVDMR::cachEncode(true, slot - 1, fragment, nullptr, bits);
            std::copy(burst, burst + 264, bits + 24);

            if (type.startsWith("data-"))
            {
                uint8_t st[20];
                DVDMR::slotTypeEncode(m_settings.m_dmrColorCode, type.mid(5).toInt(), st);
                std::copy(st, st + 10, bits + 122);
                std::copy(st + 10, st + 20, bits + 180);
                std::copy(DSEModDMR::m_syncDataBS, DSEModDMR::m_syncDataBS + 48, bits + 132);
            }
            else if (type == "voice-sync")
            {
                std::copy(DSEModDMR::m_syncVoiceBS, DSEModDMR::m_syncVoiceBS + 48, bits + 132);
            }
            else
            {
                uint8_t emb[16];
                std::copy(bits + 132, bits + 140, emb);
                std::copy(bits + 172, bits + 180, emb + 8);
                int cc, fragmentType;
                bool privacy;

                if (DVDMR::embDecode(emb, cc, privacy, fragmentType) >= 0)
                {
                    DVDMR::embEncode(m_settings.m_dmrColorCode, privacy, fragmentType, emb);
                    std::copy(emb, emb + 8, bits + 132);
                    std::copy(emb + 8, emb + 16, bits + 172);
                }
            }
        }

        DSEModDMR::appendBits(levels, bits, 288);
    }
}

void DSEModPacketBuilder::finish(std::vector<int8_t>& levels)
{
    levels.clear();

    if (!m_started || m_ended) {
        return;
    }

    if (m_settings.m_mode == DSEModSettings::DSEModModeDStar)
    {
        dstarBytes(QByteArray((const char*) DSEModVocoder::getSilenceFrame(DSEModVocoder::Rate3600x2400), 9), levels);
        dstarEnd(levels);
    }
    else if ((m_settings.m_mode == DSEModSettings::DSEModModeYSF) && !m_header.isEmpty())
    {
        uint8_t dibits[480];
        ysfDibits(m_header, dibits);
        DVYSF::FICH fich;
        DVYSF::fichDecode(dibits + 20, fich);
        fich.m_fi = 2;
        DVYSF::fichEncode(fich, dibits + 20);

        for (auto dibit : dibits) {
            levels.push_back(DSEModProtocol::dibitToLevel(dibit));
        }
    }
    else if (!m_header.isEmpty())
    {
        uint8_t bits[264];
        uint8_t coded[196];
        uint8_t info[96];
        uint8_t lc[9];
        uint8_t slot[288];
        uint8_t packed[36];
        DVBits::bytesToBitsMSB((const uint8_t*) m_header.constData(), 33, bits);
        std::copy(bits, bits + 98, coded);
        std::copy(bits + 166, bits + 264, coded + 98);
        DVDMR::bptcDecode(coded, info);
        DVDMR::fullLCDecode(info, DVDMR::m_maskVoiceLCHeader, lc);
        DVDMR::fullLCEncode(lc, DVDMR::m_maskTerminatorWithLC, info);
        DVDMR::bptcEncode(info, coded);
        DSEModDMR::buildDataBurst(m_settings.m_dmrColorCode, 2, coded, m_settings.m_dmrSlot - 1, 0, nullptr, slot);
        DVBits::bitsToBytesMSB(slot + 24, 264, packed);
        dmr(QByteArray((const char*) packed, 33), "data-2", levels);
    }

    m_ended = true;
}
