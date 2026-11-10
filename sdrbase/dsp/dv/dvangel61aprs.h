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

#ifndef INCLUDE_DSP_DV_DVANGEL61APRS_H
#define INCLUDE_DSP_DV_DVANGEL61APRS_H

#include <string>

#include "export.h"

#include "dvangel61.h"

/**
 * Angel61 as APRS (ANGEL61.md 3.3). The protocol carries real callsigns with their SSIDs, a
 * repeater path and a position finer than APRS's own format, so a receiver can put what it
 * decoded on the APRS network without inventing anything: these are the packets it builds.
 * Nothing of AX.25 is on the air; the TNC2 line made here is what a consumer of packets (the
 * APRS feature) parses.
 */
class SDRBASE_API DVAngel61APRS
{
public:
    //! The experimental range tocall used until one is registered for Angel61
    static const char *m_tocall;

    /**
     * source>TOCALL[,repeater1*[,repeater2*]]:info, the stations by identity (2.6), or an empty
     * string without a source, or when the source is not an AX.25 address: the packets go to
     * the APRS feature as AX.25 frames, and a longer callsign cut to fit would name another
     * station. A repeater that is not one is left out of the path.
     */
    static std::string tnc2(uint64_t source, const DVAngel61::Path& path, bool hasPath, const std::string& info);
    //! An AX.25 address: 1 to 6 of A-Z and 0-9, and an SSID of 0 to 15
    static bool isAX25Address(const std::string& callsign);

    /**
     * A position report, `=` (no timestamp, the station takes messages), in the Base91
     * compressed form: it keeps the slot's resolution (0.3 m against 18 m) and carries the
     * course and speed in the same 13 bytes. The altitude follows as /A=nnnnnn in feet.
     * Returns an empty string when the position has no fix.
     */
    static std::string position(const DVAngel61::Position& position, char symbolTable, char symbolCode, const std::string& comment = std::string());

    //! :ADDRESSEE :text (the addressee is padded to nine characters, as APRS requires)
    static std::string message(const std::string& addressee, const std::string& text);

    //! >text
    static std::string status(const std::string& text);

    /**
     * The symbol to draw a station with when its station slot did not say (3.3): a house for a
     * position set by hand, a car for a moving GNSS fix, a person otherwise.
     */
    static void defaultSymbol(const DVAngel61::Position& position, char& table, char& code);

    //! Four Base91 digits of a compressed latitude or longitude
    static std::string base91(int value, int digits);

private:
    static std::string compressedPosition(const DVAngel61::Position& position, char table, char code);
};

#endif // INCLUDE_DSP_DV_DVANGEL61APRS_H
