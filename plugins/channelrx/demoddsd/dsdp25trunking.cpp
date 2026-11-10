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

#include <cstdio>

#include "dsp/dv/dvp25.h"

#include "dsdp25trunking.h"

void DSDP25Trunking::resetSite()
{
    m_networkValid = false;
    m_rfss = -1;
    m_site = -1;
}

void DSDP25Trunking::setIdentifier(int iden, int channelType, bool tdmaType, uint32_t spacing, uint32_t base)
{
    static const int slotsOfType[16] = { 1, 1, 1, 2, 4, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    Identifier& id = m_identifiers[iden & 0xF];
    id.m_valid = true;
    id.m_spacingHz = 125.0 * spacing;
    id.m_baseHz = 5.0 * base;
    id.m_slots = tdmaType ? slotsOfType[channelType & 0xF] : 1;
}

std::string DSDP25Trunking::parseTSBK(const uint8_t *b, bool& grant)
{
    int opcode = b[0] & 0x3F;
    int mfid = b[1];
    const char *name = DVP25::tsbkOpcodeName(opcode);
    char text[96];
    grant = false;

    if (mfid != 0)
    {
        snprintf(text, sizeof(text), "MFID %02X op %02X", mfid, opcode);
        return text;
    }

    auto channel = [&](int hi, int lo) { return ((int) b[hi] << 8) | b[lo]; };
    // The 64 argument bits as bit fields, bit 63 first (TIA-102.AABC layouts)
    auto field = [&](int msb, int nbBits) -> uint32_t {
        uint64_t args = 0;

        for (int i = 2; i < 10; i++) {
            args = (args << 8) | b[i];
        }

        return (uint32_t) ((args >> (msb - nbBits + 1)) & ((1ULL << nbBits) - 1));
    };

    switch (opcode)
    {
    case 0x00: // group voice channel grant: options, channel, group, source
        snprintf(text, sizeof(text), "%s TG %u from %u %s", name, channel(5, 6), ((uint32_t) b[7] << 16) | ((uint32_t) b[8] << 8) | b[9], channelText(channel(3, 4)).c_str());
        break;
    case 0x02: // group voice channel grant update: channel, group twice
        if (channel(2, 3) == channel(6, 7)) {
            snprintf(text, sizeof(text), "%s TG %u %s", name, channel(4, 5), channelText(channel(2, 3)).c_str());
        } else {
            snprintf(text, sizeof(text), "%s TG %u %s TG %u %s", name, channel(4, 5), channelText(channel(2, 3)).c_str(), channel(8, 9), channelText(channel(6, 7)).c_str());
        }
        break;
    case 0x03: // explicit update: options, reserved, tx channel, rx channel, group (TIA-102.AABC; OP25)
        snprintf(text, sizeof(text), "%s TG %u %s", name, channel(8, 9), channelText(channel(4, 5)).c_str());
        break;
    case 0x04: // unit to unit grant: channel, target, source
        snprintf(text, sizeof(text), "%s unit %u from %u %s", name, ((uint32_t) b[4] << 16) | ((uint32_t) b[5] << 8) | b[6],
            ((uint32_t) b[7] << 16) | ((uint32_t) b[8] << 8) | b[9], channelText(channel(2, 3)).c_str());
        break;
    case 0x33: // identifier update, TDMA: identifier, channel type, transmit offset, spacing, base
    case 0x34: // identifier update, VHF/UHF: identifier, bandwidth code, transmit offset, spacing, base
    case 0x3D: // identifier update: identifier, bandwidth, transmit offset, spacing, base
    {
        int iden = field(63, 4);
        setIdentifier(iden, field(59, 4), opcode == 0x33, field(41, 10), field(31, 32));
        const Identifier& id = m_identifiers[iden];
        snprintf(text, sizeof(text), "%s id %d base %.4f MHz spacing %.1f kHz%s", name, iden, id.m_baseHz * 1e-6, id.m_spacingHz * 1e-3, id.m_slots > 1 ? " TDMA" : "");
        break;
    }
    case 0x3A: // RFSS status: LRA, system ID, RFSS, site, channel, services
        m_sysid = (uint16_t) (((b[3] & 0x0F) << 8) | b[4]);
        m_rfss = b[5];
        m_site = b[6];
        snprintf(text, sizeof(text), "%s SYS %03X RFSS %d site %d cc %s", name, m_sysid, m_rfss, m_site, channelText(channel(7, 8)).c_str());
        break;
    case 0x3B: // network status: LRA, WACN, system ID, channel, services
        m_wacn = ((uint32_t) b[3] << 12) | ((uint32_t) b[4] << 4) | (b[5] >> 4);
        m_sysid = (uint16_t) (((b[5] & 0x0F) << 8) | b[6]);
        m_networkValid = true;
        snprintf(text, sizeof(text), "%s WACN %05X SYS %03X", name, m_wacn, m_sysid);
        break;
    case 0x3C: // adjacent status: LRA, system ID, RFSS, site, channel
        snprintf(text, sizeof(text), "%s RFSS %d site %d cc %s", name, b[5], b[6], channelText(channel(7, 8)).c_str());
        break;
    default:
        if (name) {
            snprintf(text, sizeof(text), "%s", name);
        } else {
            snprintf(text, sizeof(text), "op %02X", opcode);
        }
        break;
    }

    // A channel grant is what a listener wants to see
    grant = (opcode == 0x00) || (opcode == 0x02) || (opcode == 0x03) || (opcode == 0x04) || (opcode == 0x06);
    return text;
}

int DSDP25Trunking::mcoLength(const uint8_t *m, int remaining)
{
    const int op = m[0];

    if ((op == 0x00) || (remaining < 1)) {
        return 0;   // null information: the rest of the PDU
    }

    if (op == 0x08) {
        return remaining > 1 ? (m[1] & 0x3F) : 0;   // null avoid zero bias: its own length
    }

    if ((op >> 6) == 2) {
        return remaining > 2 ? (m[2] & 0x3F) : 0;   // manufacturer: its own length
    }

    // The fixed lengths OP25 (p25p2_tdma.cc mac_msg_len) and SDRTrunk (MacOpcode) agree on
    switch (op)
    {
    case 0x01: case 0x03: case 0x31: case 0x41: case 0x55: case 0x5A: case 0x5F: case 0x6A: case 0x6D:
        return 7;
    case 0x02: case 0x5D: case 0xC3: case 0xE9:
        return 8;
    case 0x40: case 0x42: case 0x44: case 0x46: case 0x54: case 0x60: case 0x61: case 0x64: case 0x67:
    case 0x6F: case 0x70: case 0x72: case 0x73: case 0x74: case 0x75: case 0x78: case 0x79: case 0x7A:
    case 0x7C: case 0x7D: case 0xD6:
        return 9;
    case 0x48: case 0x49: case 0x4C: case 0x58: case 0x5C: case 0x68: case 0x6B: case 0x6C: case 0x76:
        return 10;
    case 0x7B: case 0xC0: case 0xDA: case 0xDF: case 0xEA: case 0xFA: case 0xFC:
        return 11;
    case 0xC8: case 0xC9:
        return 12;
    case 0x77: case 0xEC: case 0xFB:
        return 13;
    case 0x21: case 0x5E: case 0xCC: case 0xD8: case 0xDC: case 0xE5: case 0xF3:
        return 14;
    case 0x22: case 0x25: case 0xC4: case 0xC6:
        return 15;
    case 0x05: case 0xE8: case 0xF2:
        return 16;
    case 0xE4:
        return 17;
    case 0x30:
        return 5;
    default:
        return 0;   // not known, or the references disagree: the rest is not walked
    }
}

std::string DSDP25Trunking::parseMCO(const uint8_t *m, int len, bool& grant)
{
    const int op = m[0];
    char text[160];
    grant = false;
    auto u16 = [&](int i) { return ((uint32_t) m[i] << 8) | m[i + 1]; };
    auto u24 = [&](int i) { return ((uint32_t) m[i] << 16) | ((uint32_t) m[i + 1] << 8) | m[i + 2]; };
    // Bit fields of the message, bit 0 the opcode octet's MSB
    auto bitsOf = [&](int first, int nbBits) -> uint32_t {
        uint32_t v = 0;

        for (int k = first; k < first + nbBits; k++) {
            v = (v << 1) | ((m[k >> 3] >> (7 - (k & 7))) & 1);
        }

        return v;
    };

    if ((op >> 6) == 1)
    {
        // The Phase 1 TSBK abbreviated: its argument octets without the MFID
        uint8_t b[12] = { 0 };
        b[0] = op & 0x3F;

        for (int i = 1; (i < len) && (i <= 8); i++) {
            b[1 + i] = m[i];
        }

        std::string s = parseTSBK(b, grant);

        if ((op == 0x7B) && (len >= 11))
        {
            m_colourCode = ((m[9] & 0x0F) << 8) | m[10];
            snprintf(text, sizeof(text), " NAC %03X", m_colourCode);
            s += text;
        }

        return s;
    }

    if ((op >> 6) == 2)
    {
        snprintf(text, sizeof(text), "MFID %02X op %02X", len > 1 ? m[1] : 0, op & 0x3F);
        return text;
    }

    switch (op)
    {
    case 0x01: // group voice channel user: options, group, source
        snprintf(text, sizeof(text), "GRP_V_CH_USR TG %u from %u%s", u16(2), u24(4), (m[1] & 0x40) ? " encrypted" : "");
        break;
    case 0x21: // the same, with the source's WACN and system
        snprintf(text, sizeof(text), "GRP_V_CH_USR TG %u from %u (%05X:%03X)%s", u16(2), u24(4), bitsOf(56, 20), bitsOf(76, 12), (m[1] & 0x40) ? " encrypted" : "");
        break;
    case 0x02: // unit to unit voice channel user: options, target, source
        snprintf(text, sizeof(text), "UU_V_CH_USR unit %u from %u", u24(2), u24(5));
        break;
    case 0x03: // telephone interconnect voice channel user: options, call timer, target
        snprintf(text, sizeof(text), "TELE_INT_V_CH_USR unit %u", u24(4));
        break;
    case 0x05: // group voice channel grant update, multiple, implicit: three of options, channel, group
    {
        std::string s = "GRP_V_CH_GRANT_UPDT";

        for (int i = 0; i < 3; i++)
        {
            const int ch = (int) u16(2 + 5 * i);
            const uint32_t group = u16(4 + 5 * i);

            if ((ch != 0) || (group != 0))
            {
                snprintf(text, sizeof(text), " TG %u %s", group, channelText(ch).c_str());
                s += text;
            }
        }

        grant = true;
        return s;
    }
    case 0x25: // group voice channel grant update, multiple, explicit: two of options, tx and rx channels, group
    {
        std::string s = "GRP_V_CH_GRANT_UPDT";

        for (int i = 0; i < 2; i++)
        {
            snprintf(text, sizeof(text), " TG %u %s", u16(6 + 7 * i), channelText((int) u16(2 + 7 * i)).c_str());
            s += text;
        }

        grant = true;
        return s;
    }
    case 0xC0: // group voice channel grant, explicit: options, tx and rx channels, group, source
        snprintf(text, sizeof(text), "GRP_V_CH_GRANT TG %u from %u %s", u16(6), u24(8), channelText((int) u16(2)).c_str());
        grant = true;
        break;
    case 0xC3: // group voice channel grant update, explicit: options, tx and rx channels, group
        snprintf(text, sizeof(text), "GRP_V_CH_GRANT_UPDT TG %u %s", u16(6), channelText((int) u16(2)).c_str());
        grant = true;
        break;
    case 0x30:
        snprintf(text, sizeof(text), "PWR_CTRL unit %u", u24(1));
        break;
    case 0x31:
        snprintf(text, sizeof(text), "MAC_RELEASE unit %u%s", u24(2), (m[1] & 0x80) ? " forced" : "");
        break;
    case 0xE9: // secondary control channel, explicit: RFSS, site, tx and rx channels
        snprintf(text, sizeof(text), "SEC_CC_BCST RFSS %d site %d cc %s", m[1], m[2], channelText((int) u16(3)).c_str());
        break;
    case 0xF3: // identifier update for TDMA, extended: identifier, channel type, offset, spacing, base, WACN, system
    {
        const int iden = (int) bitsOf(16, 4);
        setIdentifier(iden, (int) bitsOf(20, 4), true, bitsOf(38, 10), bitsOf(48, 32));
        const Identifier& id = m_identifiers[iden];
        snprintf(text, sizeof(text), "IDEN_UP_TDMA id %d base %.4f MHz spacing %.1f kHz%s (%05X:%03X)", iden, id.m_baseHz * 1e-6, id.m_spacingHz * 1e-3,
            id.m_slots > 1 ? " TDMA" : "", bitsOf(80, 20), bitsOf(100, 12));
        break;
    }
    case 0xFA: // RFSS status, explicit: LRA, system, RFSS, site, tx and rx channels, services
        m_sysid = (uint16_t) (((m[2] & 0x0F) << 8) | m[3]);
        m_rfss = m[4];
        m_site = m[5];
        snprintf(text, sizeof(text), "RFSS_STS_BCST SYS %03X RFSS %d site %d cc %s", m_sysid, m_rfss, m_site, channelText((int) u16(6)).c_str());
        break;
    case 0xFB: // network status, explicit: LRA, WACN, system, tx and rx channels, services, colour code
        m_wacn = ((uint32_t) m[2] << 12) | ((uint32_t) m[3] << 4) | (m[4] >> 4);
        m_sysid = (uint16_t) (((m[4] & 0x0F) << 8) | m[5]);
        m_colourCode = ((m[11] & 0x0F) << 8) | m[12];
        m_networkValid = true;
        snprintf(text, sizeof(text), "NET_STS_BCST WACN %05X SYS %03X NAC %03X", m_wacn, m_sysid, m_colourCode);
        break;
    case 0xFC: // adjacent status, explicit: LRA, flags and system, RFSS, site, tx and rx channels
        snprintf(text, sizeof(text), "ADJ_STS_BCST RFSS %d site %d cc %s", m[4], m[5], channelText((int) u16(6)).c_str());
        break;
    default:
        snprintf(text, sizeof(text), "MCO %02X", op);
        break;
    }

    return text;
}

std::string DSDP25Trunking::channelText(int channel) const
{
    char text[48];
    int iden = (channel >> 12) & 0xF;
    int number = channel & 0xFFF;
    const Identifier& id = m_identifiers[iden];

    if (!id.m_valid) {
        snprintf(text, sizeof(text), "ch %04X", channel);
    } else if (id.m_slots > 1) {
        snprintf(text, sizeof(text), "%.4f MHz slot %d", (id.m_baseHz + id.m_spacingHz * (number / id.m_slots)) * 1e-6, number % id.m_slots + 1);
    } else {
        snprintf(text, sizeof(text), "%.4f MHz", (id.m_baseHz + id.m_spacingHz * number) * 1e-6);
    }

    return text;
}
