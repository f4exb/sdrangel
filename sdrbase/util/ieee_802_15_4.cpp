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

#include <cmath>

#include "util/crc.h"
#include "util/ieee_802_15_4.h"

// Table 13-1 - Symbol-to-chip mapping for BPSK PHY (c0 first)
const uint8_t IEEE_802_15_4_PHY::m_chipsBPSK[2][15] = {
    {1, 1, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 0, 0, 0},
    {0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1}
};

// Table 12-2 - Symbol-to-chip mapping for the 780 MHz, 868 MHz and 915 MHz bands
const uint8_t IEEE_802_15_4_PHY::m_chipsSubGHzOQPSK[16][16] = {
    {0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 1},
    {0, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1},
    {0, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0},
    {1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0},
    {0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0},
    {1, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 1},
    {1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1},
    {1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0},
    {0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0},
    {0, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 0, 0},
    {0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1},
    {1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1},
    {0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 1},
    {1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0},
    {1, 0, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0},
    {1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1},
};

// Table 12-1 - Symbol-to-chip mapping for the 2450 MHz and 2380 MHz bands
const uint8_t IEEE_802_15_4_PHY::m_chips2450OQPSK[16][32] = {
    {1, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0},
    {1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0},
    {0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1, 0},
    {0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1},
    {0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1},
    {0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0},
    {1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0, 1},
    {1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1},
    {1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1},
    {1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1},
    {0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1},
    {0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0},
    {0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1},
    {1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0},
    {1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0}
};

IEEE_802_15_4_PHY::IEEE_802_15_4_PHY() :
    m_bitRate(250000),
    m_modulation(OQPSK),
    m_subGHzBand(false),
    m_pulseShaping(SINE),
    m_beta(1.0f),
    m_symbolSpan(6)
{
}

bool IEEE_802_15_4_PHY::fromString(const QString& phy)
{
    bool valid;

    // First part of phy string should give bitrate in kbps
    float bitRate = phy.split("k")[0].toFloat(&valid) * 1000.0f;

    if (!valid) {
        return false;
    }

    if (phy.contains("BPSK"))
    {
        m_bitRate = bitRate;
        m_subGHzBand = true;
        m_modulation = BPSK;
        m_pulseShaping = RC;
        m_beta = 1.0f;
        m_symbolSpan = 6;
    }
    else if (phy.contains("O-QPSK"))
    {
        m_bitRate = bitRate;
        m_subGHzBand = phy.contains("<1");
        m_modulation = OQPSK;

        if (phy.contains("RC"))
        {
            m_pulseShaping = RC;
            m_beta = 0.8f;
            m_symbolSpan = 6;
        }
        else
        {
            m_pulseShaping = SINE;
        }
    }
    else
    {
        return false;
    }

    return true;
}

QStringList IEEE_802_15_4_PHY::getPHYNames()
{
    return {
        "20kbps BPSK",
        "40kbps BPSK",
        "100kbps <1GHz O-QPSK",
        "250kbps <1GHz O-QPSK (Sine)",
        "250kbps <1GHz O-QPSK (RC)",
        "250kbps >2GHz O-QPSK"
    };
}

QString IEEE_802_15_4_PHY::toString() const
{
    if (m_modulation == BPSK)
    {
        if (m_bitRate == 20000) {
            return "20kbps BPSK";
        } else if (m_bitRate == 40000) {
            return "40kbps BPSK";
        }
    }
    else if (m_subGHzBand)
    {
        if (m_bitRate == 100000) {
            return "100kbps <1GHz O-QPSK";
        } else if ((m_bitRate == 250000) && (m_pulseShaping == SINE)) {
            return "250kbps <1GHz O-QPSK (Sine)";
        } else if (m_bitRate == 250000) {
            return "250kbps <1GHz O-QPSK (RC)";
        }
    }
    else if (m_bitRate == 250000)
    {
        return "250kbps >2GHz O-QPSK";
    }

    return "";
}

int IEEE_802_15_4_PHY::getChipsPerSymbol() const
{
    if (m_modulation == BPSK) {
        return 15;
    } else {
        return m_subGHzBand ? 16 : 32;
    }
}

int IEEE_802_15_4_PHY::getBitsPerSymbol() const
{
    return m_modulation == BPSK ? 1 : 4;
}

int IEEE_802_15_4_PHY::getChipRate() const
{
    return m_bitRate * getChipsPerSymbol() / getBitsPerSymbol();
}

int IEEE_802_15_4_PHY::getOccupiedBandwidth() const
{
    int chipRate = getChipRate();

    if (m_pulseShaping == SINE) {
        return (int) (1.5 * chipRate);
    } else {
        return (int) ((1.0 + m_beta) * chipRate);
    }
}

int IEEE_802_15_4_PHY::getChip(int symbol, int chipIdx) const
{
    if (m_modulation == BPSK) {
        return m_chipsBPSK[symbol & 1][chipIdx];
    } else if (m_subGHzBand) {
        return m_chipsSubGHzOQPSK[symbol & 0xf][chipIdx];
    } else {
        return m_chips2450OQPSK[symbol & 0xf][chipIdx];
    }
}

std::vector<float> IEEE_802_15_4_PHY::getPulse(int samplesPerChip) const
{
    std::vector<float> pulse;

    if (m_pulseShaping == SINE)
    {
        // Half-sine over two chips: p(t) = sin(pi*t/(2*Tc)), 0 <= t <= 2*Tc
        pulse.resize(2 * samplesPerChip);

        for (int i = 0; i < 2 * samplesPerChip; i++) {
            pulse[i] = (float) std::sin(M_PI * i / (2.0 * samplesPerChip));
        }
    }
    else
    {
        // Raised cosine, centred on the chip time, as used by RaisedCosine
        int nTaps = m_symbolSpan * samplesPerChip + 1;

        if (!(nTaps & 1)) {
            nTaps++;
        }

        pulse.resize(nTaps);

        for (int i = 0; i < nTaps; i++)
        {
            double t = (i - (nTaps / 2)) / (double) samplesPerChip;
            double denominator = 1.0 - std::pow(2.0 * m_beta * t, 2.0);

            if (std::abs(denominator) > 1e-9)
            {
                double sinc = (t == 0.0) ? 1.0 : std::sin(M_PI * t) / (M_PI * t);
                pulse[i] = (float) (sinc * std::cos(M_PI * m_beta * t) / denominator);
            }
            else
            {
                pulse[i] = (float) (m_beta * std::sin(M_PI / (2.0 * m_beta)) / 2.0);
            }
        }
    }

    return pulse;
}

int IEEE_802_15_4_PHY::getPulseStartOffset(int samplesPerChip) const
{
    if (m_pulseShaping == SINE)
    {
        return 0;
    }
    else
    {
        int nTaps = m_symbolSpan * samplesPerChip + 1;

        if (!(nTaps & 1)) {
            nTaps++;
        }

        return -(nTaps / 2);
    }
}

IEEE_802_15_4_FrameDecoder::IEEE_802_15_4_FrameDecoder() :
    m_valid(false),
    m_frameControl(0),
    m_frameType(Beacon),
    m_securityEnabled(false),
    m_framePending(false),
    m_ackRequest(false),
    m_panIdCompression(false),
    m_seqNoSuppression(false),
    m_iePresent(false),
    m_destAddressMode(AddressNone),
    m_frameVersion(0),
    m_srcAddressMode(AddressNone),
    m_hasSeqNo(false),
    m_seqNo(0),
    m_hasDestPANID(false),
    m_destPANID(0),
    m_destAddress(0),
    m_hasSrcPANID(false),
    m_srcPANID(0),
    m_srcAddress(0),
    m_securityLevel(0),
    m_keyIdMode(0),
    m_auxSecurityHeaderLength(0),
    m_commandId(-1),
    m_hasFCS(false),
    m_fcs(0)
{
}

uint16_t IEEE_802_15_4_FrameDecoder::calcFCS(const uint8_t *data, int length)
{
    crc16itut crc;
    crc.calculate(data, length);
    return crc.get();
}

bool IEEE_802_15_4_FrameDecoder::decode(const QByteArray& psdu, bool hasFCS)
{
    const uint8_t *p = (const uint8_t *) psdu.data();
    int length = psdu.size();

    m_valid = false;
    m_ies.clear();
    m_payload.clear();
    m_commandId = -1;
    m_hasFCS = hasFCS && (length >= 2);

    if (m_hasFCS)
    {
        m_fcs = p[length - 2] | (p[length - 1] << 8);
        length -= 2;
    }

    if (length < 1) {
        return false;
    }

    int idx = 0;
    m_frameControl = p[idx++];
    m_frameType = (FrameType) (m_frameControl & 0x7);

    // Fragment frames have no frame control, and multipurpose frames have a different
    // format which we don't decode further
    if ((m_frameType == Fragment) || (m_frameType == Multipurpose) || (m_frameType == Extended) || (m_frameType == Reserved))
    {
        m_payload = QByteArray((const char *) &p[idx], length - idx);
        m_valid = true;
        return true;
    }

    if (length < 2) {
        return false;
    }

    m_frameControl |= p[idx++] << 8;
    m_securityEnabled = (m_frameControl >> 3) & 1;
    m_framePending = (m_frameControl >> 4) & 1;
    m_ackRequest = (m_frameControl >> 5) & 1;
    m_panIdCompression = (m_frameControl >> 6) & 1;
    m_seqNoSuppression = (m_frameControl >> 8) & 1;
    m_iePresent = (m_frameControl >> 9) & 1;
    m_destAddressMode = (AddressMode) ((m_frameControl >> 10) & 3);
    m_frameVersion = (m_frameControl >> 12) & 3;
    m_srcAddressMode = (AddressMode) ((m_frameControl >> 14) & 3);

    if ((m_destAddressMode == AddressReserved) || (m_srcAddressMode == AddressReserved)) {
        return false;
    }

    m_hasSeqNo = !m_seqNoSuppression;

    if (m_hasSeqNo)
    {
        if (idx >= length) {
            return false;
        }
        m_seqNo = p[idx++];
    }

    // Determine which PAN IDs are present - 7.2.1.5
    bool destPresent = m_destAddressMode != AddressNone;
    bool srcPresent = m_srcAddressMode != AddressNone;

    if (m_frameVersion < 2)
    {
        m_hasDestPANID = destPresent;
        m_hasSrcPANID = srcPresent && !(destPresent && m_panIdCompression);
    }
    else
    {
        // Table 7-2
        if (!destPresent && !srcPresent)
        {
            m_hasDestPANID = m_panIdCompression;
            m_hasSrcPANID = false;
        }
        else if (destPresent && !srcPresent)
        {
            m_hasDestPANID = !m_panIdCompression;
            m_hasSrcPANID = false;
        }
        else if (!destPresent && srcPresent)
        {
            m_hasDestPANID = false;
            m_hasSrcPANID = !m_panIdCompression;
        }
        else if ((m_destAddressMode == AddressExtended) && (m_srcAddressMode == AddressExtended))
        {
            m_hasDestPANID = !m_panIdCompression;
            m_hasSrcPANID = false;
        }
        else
        {
            m_hasDestPANID = true;
            m_hasSrcPANID = !m_panIdCompression;
        }
    }

    auto readAddress = [&](AddressMode mode, uint64_t& address) -> bool {
        int bytes = mode == AddressShort ? 2 : (mode == AddressExtended ? 8 : 0);

        if (idx + bytes > length) {
            return false;
        }

        address = 0;

        for (int i = 0; i < bytes; i++) {
            address |= ((uint64_t) p[idx++]) << (8 * i);
        }

        return true;
    };

    uint64_t panId;

    if (m_hasDestPANID)
    {
        if (!readAddress(AddressShort, panId)) {
            return false;
        }
        m_destPANID = (uint16_t) panId;
    }
    if (!readAddress(m_destAddressMode, m_destAddress)) {
        return false;
    }
    if (m_hasSrcPANID)
    {
        if (!readAddress(AddressShort, panId)) {
            return false;
        }
        m_srcPANID = (uint16_t) panId;
    }
    if (!readAddress(m_srcAddressMode, m_srcAddress)) {
        return false;
    }

    // Auxiliary security header - 9.4
    m_auxSecurityHeaderLength = 0;

    if (m_securityEnabled)
    {
        if (idx >= length) {
            return false;
        }

        uint8_t securityControl = p[idx];
        m_securityLevel = securityControl & 0x7;
        m_keyIdMode = (securityControl >> 3) & 0x3;
        bool frameCounterSuppression = (m_frameVersion >= 2) && ((securityControl >> 5) & 1);
        static const int keyIdLengths[4] = {0, 1, 5, 9};

        m_auxSecurityHeaderLength = 1 + (frameCounterSuppression ? 0 : 4) + keyIdLengths[m_keyIdMode];

        if (idx + m_auxSecurityHeaderLength > length) {
            return false;
        }

        idx += m_auxSecurityHeaderLength;
    }

    // Information elements - 7.4
    if (m_iePresent)
    {
        bool payloadIEs = false;

        // Header IEs
        while (idx + 2 <= length)
        {
            uint16_t descriptor = p[idx] | (p[idx + 1] << 8);
            idx += 2;
            IE ie;
            ie.m_header = true;
            ie.m_length = descriptor & 0x7f;
            ie.m_id = (descriptor >> 7) & 0xff;

            if (idx + ie.m_length > length) {
                return false;
            }

            idx += ie.m_length;

            if (ie.m_id == 0x7e) // Header Termination 1 - Payload IEs follow
            {
                payloadIEs = true;
                break;
            }
            else if (ie.m_id == 0x7f) // Header Termination 2 - Payload follows
            {
                break;
            }

            m_ies.append(ie);
        }

        // Payload IEs are encrypted if security is enabled
        if (payloadIEs && !m_securityEnabled)
        {
            while (idx + 2 <= length)
            {
                uint16_t descriptor = p[idx] | (p[idx + 1] << 8);
                idx += 2;
                IE ie;
                ie.m_header = false;
                ie.m_length = descriptor & 0x7ff;
                ie.m_id = (descriptor >> 11) & 0xf;

                if (idx + ie.m_length > length) {
                    return false;
                }

                idx += ie.m_length;

                if (ie.m_id == 0xf) { // Payload termination
                    break;
                }

                m_ies.append(ie);
            }
        }
    }

    if ((m_frameType == Command) && !m_securityEnabled && (idx < length)) {
        m_commandId = p[idx];
    }

    m_payload = QByteArray((const char *) &p[idx], length - idx);
    m_valid = true;

    return true;
}

QString IEEE_802_15_4_FrameDecoder::frameTypeToString(FrameType type)
{
    switch (type)
    {
    case Beacon:
        return "Beacon";
    case Data:
        return "Data";
    case Ack:
        return "Ack";
    case Command:
        return "Command";
    case Multipurpose:
        return "Multipurpose";
    case Fragment:
        return "Fragment";
    case Extended:
        return "Extended";
    default:
        return "Reserved";
    }
}

QString IEEE_802_15_4_FrameDecoder::commandToString(int commandId)
{
    // Table 7-49
    switch (commandId)
    {
    case 0x01:
        return "Association Request";
    case 0x02:
        return "Association Response";
    case 0x03:
        return "Disassociation Notification";
    case 0x04:
        return "Data Request";
    case 0x05:
        return "PAN ID Conflict Notification";
    case 0x06:
        return "Orphan Notification";
    case 0x07:
        return "Beacon Request";
    case 0x08:
        return "Coordinator Realignment";
    case 0x09:
        return "GTS Request";
    case 0x0a:
        return "TRLE Management Request";
    case 0x0b:
        return "TRLE Management Response";
    case 0x13:
        return "DSME Association Request";
    case 0x14:
        return "DSME Association Response";
    case 0x15:
        return "DSME GTS Request";
    case 0x16:
        return "DSME GTS Response";
    case 0x17:
        return "DSME GTS Notify";
    case 0x18:
        return "DSME Information Request";
    case 0x19:
        return "DSME Information Response";
    case 0x1a:
        return "DSME Beacon Allocation Notification";
    case 0x1b:
        return "DSME Beacon Collision Notification";
    case 0x1c:
        return "DSME Link Report";
    case 0x20:
        return "RIT Data Request";
    case 0x21:
        return "DBS Request";
    case 0x22:
        return "DBS Response";
    case 0x23:
        return "RIT Data Response";
    case 0x24:
        return "Vendor Specific";
    case 0x25:
        return "SRM Request";
    case 0x26:
        return "SRM Response";
    case 0x27:
        return "SRM Report";
    case 0x28:
        return "SRM Information";
    default:
        return QString("Command 0x%1").arg(commandId, 2, 16, QChar('0'));
    }
}

QString IEEE_802_15_4_FrameDecoder::addressToString(AddressMode mode, uint64_t address)
{
    if (mode == AddressShort)
    {
        return QString("0x%1").arg(address, 4, 16, QChar('0'));
    }
    else if (mode == AddressExtended)
    {
        // Display MSB first, as Wireshark does
        QStringList bytes;

        for (int i = 7; i >= 0; i--) {
            bytes.append(QString("%1").arg((address >> (8 * i)) & 0xff, 2, 16, QChar('0')));
        }

        return bytes.join(":");
    }
    else
    {
        return "";
    }
}

QString IEEE_802_15_4_FrameDecoder::getFrameTypeString() const
{
    return frameTypeToString(m_frameType);
}

QString IEEE_802_15_4_FrameDecoder::getDestPANIDString() const
{
    return m_hasDestPANID ? QString("0x%1").arg(m_destPANID, 4, 16, QChar('0')) : "";
}

QString IEEE_802_15_4_FrameDecoder::getDestAddressString() const
{
    return addressToString(m_destAddressMode, m_destAddress);
}

QString IEEE_802_15_4_FrameDecoder::getSrcPANIDString() const
{
    return m_hasSrcPANID ? QString("0x%1").arg(m_srcPANID, 4, 16, QChar('0')) : "";
}

QString IEEE_802_15_4_FrameDecoder::getSrcAddressString() const
{
    return addressToString(m_srcAddressMode, m_srcAddress);
}

QString IEEE_802_15_4_FrameDecoder::getCommandString() const
{
    return m_commandId >= 0 ? commandToString(m_commandId) : "";
}

QString IEEE_802_15_4_FrameDecoder::toString() const
{
    QStringList parts;

    if (!m_valid) {
        return "Invalid frame";
    }

    parts.append(getFrameTypeString());

    if (m_commandId >= 0) {
        parts.append(getCommandString());
    }
    if (m_securityEnabled) {
        parts.append(QString("Secured (level %1)").arg(m_securityLevel));
    }
    if (m_ackRequest) {
        parts.append("AR");
    }
    if (m_framePending) {
        parts.append("FP");
    }
    if (m_frameVersion > 0) {
        parts.append(QString("v%1").arg(m_frameVersion));
    }
    for (const auto& ie : m_ies) {
        parts.append(QString("%1 IE 0x%2").arg(ie.m_header ? "Header" : "Payload").arg(ie.m_id, 2, 16, QChar('0')));
    }

    return parts.join(", ");
}
