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

#ifndef INCLUDE_DSDP25TRUNKING_H
#define INCLUDE_DSDP25TRUNKING_H

#include <cstdint>
#include <string>

/**
 * P25 trunking signalling shared by Phase 1 and Phase 2: the site and network identity and the
 * channel plan the broadcasts give, and the text of each message.
 *
 * Phase 1 sends it in trunking blocks (TSBK, TIA-102.AABC). Phase 2 sends MAC messages (MCO,
 * TIA-102.BBAD) in its SACCH, FACCH and LCCH: those of partition 01 are the Phase 1 TSBKs
 * abbreviated (their 8 argument octets without the MFID), the others are Phase 2's own and the
 * explicit (extended) forms. The layouts are OP25's and SDRTrunk's, cross-checked (see
 * DESIGN.md: the lengths where the two disagree are not walked past).
 */
class DSDP25Trunking
{
public:
    //! A channel identifier (IDEN_UP): channel numbers of the identifier to frequencies
    struct Identifier
    {
        bool m_valid = false;
        double m_baseHz = 0.0;
        double m_spacingHz = 0.0;
        int m_slots = 1;        //!< Slots per carrier (2 or 4 for TDMA)
    };

    //! A Phase 1 TSBK: opcode (low 6 bits of b[0]), MFID (b[1]) and the 8 argument octets
    //! b[2..9]. Its text; grant set when it is a channel grant
    std::string parseTSBK(const uint8_t *b, bool& grant);
    //! A Phase 2 MAC message of len octets (its opcode octet included). Its text
    std::string parseMCO(const uint8_t *m, int len, bool& grant);
    //! A Phase 2 MAC message's length in octets (its opcode octet included), 0 when unknown (or
    //! disputed between the references) or the end of the list (null information)
    static int mcoLength(const uint8_t *m, int remaining);

    //! A channel (4 bit identifier, 12 bit number) as a frequency, and slot when TDMA
    std::string channelText(int channel) const;
    //! Forget the site (a new transmission), keeping the channel plan
    void resetSite();

    bool isNetworkValid() const { return m_networkValid; }
    uint32_t getWacn() const { return m_wacn; }
    uint16_t getSysid() const { return m_sysid; }
    int getRfss() const { return m_rfss; }
    int getSite() const { return m_site; }
    //! The colour code (NAC) of a Phase 2 network status broadcast, -1 when none was heard
    int getColourCode() const { return m_colourCode; }

private:
    Identifier m_identifiers[16];
    bool m_networkValid = false;
    uint32_t m_wacn = 0;
    uint16_t m_sysid = 0;
    int m_rfss = -1;
    int m_site = -1;
    int m_colourCode = -1;

    void setIdentifier(int iden, int channelType, bool tdmaType, uint32_t spacing, uint32_t base);
};

#endif // INCLUDE_DSDP25TRUNKING_H
