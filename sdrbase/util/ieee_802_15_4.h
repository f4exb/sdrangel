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

#ifndef INCLUDE_UTIL_IEEE_802_15_4_H
#define INCLUDE_UTIL_IEEE_802_15_4_H

#include <stdint.h>
#include <vector>

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include "export.h"

// IEEE 802.15.4-2015 O-QPSK (clause 12) and BPSK (clause 13) PHYs
struct SDRBASE_API IEEE_802_15_4_PHY
{
    enum Modulation {
        BPSK,
        OQPSK
    };

    enum PulseShaping {
        RC,
        SINE
    };

    int m_bitRate;
    Modulation m_modulation;
    bool m_subGHzBand;
    PulseShaping m_pulseShaping;
    float m_beta;           //!< Roll-off for RC pulse shaping
    int m_symbolSpan;       //!< Span of RC pulse in chips

    IEEE_802_15_4_PHY();

    //! Set PHY from a string as returned by getPHYNames()
    bool fromString(const QString& phy);
    //! Full PHY name, as in getPHYNames(), or empty if not a standard PHY
    QString toString() const;
    //! Names of the supported PHYs, as used in the GUI
    static QStringList getPHYNames();

    int getChipsPerSymbol() const;
    int getBitsPerSymbol() const;
    int getNumberOfSymbols() const { return 1 << getBitsPerSymbol(); }
    int getChipRate() const;
    int getSymbolRate() const { return m_bitRate / getBitsPerSymbol(); }
    //! Number of symbols in the preamble (4 octets of zeros)
    int getPreambleSymbols() const { return 32 / getBitsPerSymbol(); }
    //! Approximate occupied bandwidth (99%)
    int getOccupiedBandwidth() const;

    //! Chip (0 or 1) for chip index chipIdx of the given symbol (data symbol for O-QPSK, differentially encoded bit for BPSK)
    int getChip(int symbol, int chipIdx) const;

    //! Pulse shape used for a single chip, sampled at samplesPerChip.
    //! Half-sine pulses span two chips and start at the chip time. RC pulses are centred on the chip time.
    std::vector<float> getPulse(int samplesPerChip) const;
    //! Offset in samples from the chip time to the start of the pulse returned by getPulse()
    int getPulseStartOffset(int samplesPerChip) const;

    static const uint8_t m_chipsBPSK[2][15];
    static const uint8_t m_chipsSubGHzOQPSK[16][16];
    static const uint8_t m_chips2450OQPSK[16][32];
};

// Decode of a MAC frame (clause 7)
struct SDRBASE_API IEEE_802_15_4_FrameDecoder
{
    enum FrameType {
        Beacon = 0,
        Data = 1,
        Ack = 2,
        Command = 3,
        Reserved = 4,
        Multipurpose = 5,
        Fragment = 6,
        Extended = 7
    };

    enum AddressMode {
        AddressNone = 0,
        AddressReserved = 1,
        AddressShort = 2,
        AddressExtended = 3
    };

    struct IE {
        bool m_header;      //!< Header IE (true) or payload IE (false)
        int m_id;           //!< Element ID for header IEs, Group ID for payload IEs
        int m_length;
    };

    bool m_valid;               //!< Frame could be parsed
    uint16_t m_frameControl;
    FrameType m_frameType;
    bool m_securityEnabled;
    bool m_framePending;
    bool m_ackRequest;
    bool m_panIdCompression;
    bool m_seqNoSuppression;
    bool m_iePresent;
    AddressMode m_destAddressMode;
    int m_frameVersion;
    AddressMode m_srcAddressMode;

    bool m_hasSeqNo;
    uint8_t m_seqNo;
    bool m_hasDestPANID;
    uint16_t m_destPANID;
    uint64_t m_destAddress;
    bool m_hasSrcPANID;
    uint16_t m_srcPANID;
    uint64_t m_srcAddress;

    int m_securityLevel;
    int m_keyIdMode;
    int m_auxSecurityHeaderLength;

    QList<IE> m_ies;
    int m_commandId;            //!< -1 if not a command frame or encrypted
    QByteArray m_payload;       //!< MAC payload (excluding header, IEs and FCS)
    bool m_hasFCS;
    uint16_t m_fcs;

    IEEE_802_15_4_FrameDecoder();

    //! Decode a PSDU (MPDU). If hasFCS is true, the last two bytes are the FCS
    bool decode(const QByteArray& psdu, bool hasFCS = true);

    QString getFrameTypeString() const;
    QString getDestPANIDString() const;
    QString getDestAddressString() const;
    QString getSrcPANIDString() const;
    QString getSrcAddressString() const;
    QString getCommandString() const;
    //! Short human readable summary of the frame contents
    QString toString() const;

    static QString frameTypeToString(FrameType type);
    static QString commandToString(int commandId);
    static QString addressToString(AddressMode mode, uint64_t address);
    //! Calculate the 16-bit FCS (ITU-T CRC) of data
    static uint16_t calcFCS(const uint8_t *data, int length);
};

#endif // INCLUDE_UTIL_IEEE_802_15_4_H
