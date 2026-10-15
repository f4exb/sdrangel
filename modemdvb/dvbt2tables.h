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

#ifndef MODEMDVB_DVBT2TABLES_H
#define MODEMDVB_DVBT2TABLES_H

// Raw tables from EN 302 755 V1.4.1, used to build dvbt2::Tables

namespace dvbt2 {
namespace tables {

extern const int continualPilots[];                 //!< Annex G table G.1, 32K carrier indices
extern const int continualPilotIndex[6][8][2];      //!< [CP group 1-6][PP1-PP8] = {offset, count}
extern const int extendedContinualPilots[];         //!< Annex G table G.2
extern const int extendedContinualPilotIndex[3][8][2]; //!< [8K, 16K, 32K][PP1-PP8] = {offset, count}
extern const int p2ReservedCarriers[];              //!< Annex H table H.1
extern const int p2ReservedCarriersIndex[6][2];     //!< [1K-32K] = {offset, count}
extern const int reservedCarriers[];                //!< Annex H table H.2
extern const int reservedCarriersIndex[6][2];       //!< [1K-32K] = {offset, count}
extern const char frameLevelPN[];                   //!< Table 56, hexadecimal, MSB first
extern const int p1Carriers[384];                   //!< Table 68
extern const char *const p1S1[8];                   //!< Table 69, hexadecimal, MSB first
extern const char *const p1S2[16];

} // namespace tables
} // namespace dvbt2

#endif // MODEMDVB_DVBT2TABLES_H
