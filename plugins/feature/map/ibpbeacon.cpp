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

// Beacon information from https://www.ncdxf.org/beacon/beaconlocations.html
QList<IBPBeacon> IBPBeacon::m_beacons = {
    IBPBeacon("United Nations", "4U1UN", "New York City", "FN30AS", 0),
    IBPBeacon("Canada", "VE8AT", "Inuvik (NT)", "CP38GH", 10),
    IBPBeacon("United States", "W6WX", "Mt. Umunhum (CA)", "CM97BD", 20),
    IBPBeacon("Hawaii", "KH6RS", "Maui	", "BL10TS", 30),
    IBPBeacon("New Zealand", "ZL6B", "Masterton", "RE78TW", 40),
    IBPBeacon("Australia", "VK6RBP", "Rolystone (WA)", "OF87AV", 50),
    IBPBeacon("Japan", "JA2IGY", "Mt. Asama", "PM84JK", 60),
    IBPBeacon("Russia", "RR9O", "Novosibirsk", "NO14KX", 70),
    IBPBeacon("Hong Kong", "VR2B", "Hong Kong", "OL72BG", 80),
    IBPBeacon("Sri Lanka", "4S7B", "Colombo", "MJ96WV", 90),
    IBPBeacon("South Africa", "ZS6DN", "Pretoria", "KG33XI", 100),
    IBPBeacon("Kenya", "5Z4B", "Kikuyu", "KI88HR", 110),
    IBPBeacon("Israel", "4X6TU", "Tel Aviv", "KM72JB", 120),
    IBPBeacon("Finland", "OH2B", "Lohja", "KP20EH", 130),
    IBPBeacon("Madeira", "CS3B", "São Jorge", "IM12JT", 140),
    IBPBeacon("Argentina", "LU4AA", "Buenos Aires", "GF05TJ", 150),
    IBPBeacon("Peru", "OA4B", "Lima", "FH17MW", 160),
    IBPBeacon("Venezuela", "YV5B", "Caracas", "FJ69CC", 170)
};

// The frequencies in MHz through which the IBP beacons rotate
QList<double> IBPBeacon::m_frequencies = {
    14.1, 18.11, 21.150, 24.93, 28.2
};

