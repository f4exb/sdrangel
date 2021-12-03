///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include "ibpbeacon.h"

QList<IBPBeacon> IBPBeacon::m_beacons = {
    IBPBeacon("4U1UN", "United Nations NY", "FN30AS", 0),
    IBPBeacon("VE8AT", "North Canada", "EQ79AX", 10),
    IBPBeacon("W6WX", "USA (CA)", "CM97BD", 20),
    IBPBeacon("KH6WO", "Hawaii", "BL10TS", 30),
    IBPBeacon("ZL6B", "New Zealand", "RE78TW", 40),
    IBPBeacon("VK6RBP", "West Australia", "OF87AV", 50),
    IBPBeacon("JA2IGY", "Japan", "PM84JK", 60),
    IBPBeacon("RR9O", "Siberia", "NO14KX", 70),
    IBPBeacon("VR2HK", "China", "OL72CQ", 80),
    IBPBeacon("4S7B", "Sri Lanka", "NJ06CR", 90),
    IBPBeacon("ZS6DN", "South Africa", "KG44DC", 100),
    IBPBeacon("5Z4B", "Kenya", "KI88MX", 110),
    IBPBeacon("4X6TU", "Israel", "KM72JB", 120),
    IBPBeacon("OH2B", "Finland", "KP20BM", 130),
    IBPBeacon("CS3B", "Madeira", "IM12OR", 140),
    IBPBeacon("LU4AA", "Argentina", "GF05TJ", 150),
    IBPBeacon("OA4B", "Peru", "FH17MW", 160),
    IBPBeacon("YV5B", "Venezuela", "FJ69CC", 170)
};

// The frequencies in MHz through which the IBP beacons rotate
QList<double> IBPBeacon::m_frequencies = {
    14.1, 18.11, 21.150, 24.93, 28.2
};

