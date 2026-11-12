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

#include "dsemodprotocol.h"
#include "dsemoddstar.h"
#include "dsemoddmr.h"
#include "dsemoddpmr.h"
#include "dsemodp25.h"
#include "dsemodp25p2.h"
#include "dsemodysf.h"
#include "dsemodnxdn.h"
#include "dsemodangel61.h"
#include "dsemodm17.h"

void DSEModProtocol::velocity(double latitude1, double longitude1, double latitude2, double longitude2,
    double seconds, int& speedKmh, int& courseDegrees)
{
    speedKmh = -1;
    courseDegrees = -1;

    if (seconds <= 0.0) {
        return;
    }

    const double toRadians = M_PI / 180.0;
    const double earthRadius = 6371008.8;   // metres, the mean radius of the WGS-84 ellipsoid
    double phi1 = latitude1 * toRadians;
    double phi2 = latitude2 * toRadians;
    double deltaPhi = (latitude2 - latitude1) * toRadians;
    double deltaLambda = (longitude2 - longitude1) * toRadians;

    // Haversine: the distance over the ground between the two fixes
    double a = std::sin(deltaPhi / 2.0) * std::sin(deltaPhi / 2.0)
        + std::cos(phi1) * std::cos(phi2) * std::sin(deltaLambda / 2.0) * std::sin(deltaLambda / 2.0);
    double distance = 2.0 * earthRadius * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    double kmh = distance / seconds * 3.6;

    if (kmh < StationaryKmh)
    {
        speedKmh = 0;    // not moving: no course either, it would be the jitter's bearing
        return;
    }

    speedKmh = (int) (kmh + 0.5);
    speedKmh = speedKmh > MaxSpeedKmh ? MaxSpeedKmh + 1 : speedKmh;   // 1022: more than the field holds

    // The initial bearing of the great circle from the first fix to the second
    double y = std::sin(deltaLambda) * std::cos(phi2);
    double x = std::cos(phi1) * std::sin(phi2) - std::sin(phi1) * std::cos(phi2) * std::cos(deltaLambda);
    double bearing = std::atan2(y, x) / toRadians;
    courseDegrees = (int) (bearing + 360.5) % 360;
}

int DSEModProtocol::peakDeviation(DSEModSettings::DSEModMode mode)
{
    switch (mode)
    {
    case DSEModSettings::DSEModModeDMR:
        return 1944; // 4FSK: +/- 1944 Hz and +/- 648 Hz
    case DSEModSettings::DSEModModeYSF:
    case DSEModSettings::DSEModModeP25:
        return 1800; // C4FM: +/- 1800 Hz and +/- 600 Hz
    case DSEModSettings::DSEModModeNXDN96:
        return 2100; // 4FSK 4800 symbols/s in a 12.5 kHz channel: +/- 2100 Hz and +/- 700 Hz
    case DSEModSettings::DSEModModeP25P2:
        return 2250; // 4FSK 6000 symbols/s: +/- 2250 Hz and +/- 750 Hz, the symbol average frequency of H-CPM / H-DQPSK
    case DSEModSettings::DSEModModeNXDN:
    case DSEModSettings::DSEModModeDPMR:
        return 1050; // 4FSK 2400 symbols/s: +/- 1050 Hz and +/- 350 Hz
    case DSEModSettings::DSEModModeM17:
        return 2400; // 4FSK 4800 symbols/s: +/- 2400 Hz and +/- 800 Hz (M17 specification)
    case DSEModSettings::DSEModModeAngel61:
    case DSEModSettings::DSEModModeAngel61WB:
    case DSEModSettings::DSEModModeAngel61HFNB:
    case DSEModSettings::DSEModModeAngel61HFWB:
        return 0;    // linear and OFDM: no FM
    case DSEModSettings::DSEModModeDStar:
    default:
        return 1200; // GMSK: h = 0.5 at 4800 bps
    }
}

int DSEModProtocol::symbolRate(DSEModSettings::DSEModMode mode)
{
    switch (mode)
    {
    case DSEModSettings::DSEModModeNXDN:
    case DSEModSettings::DSEModModeDPMR:
        return 2400;
    case DSEModSettings::DSEModModeP25P2:
        return 6000;
    case DSEModSettings::DSEModModeAngel61WB:
        return 9600;
    default:
        return 4800;
    }
}

int DSEModProtocol::occupiedBandwidth(DSEModSettings::DSEModMode mode)
{
    // OFDM (ANGEL61.md 2.1): 50 or 110 carriers 50 Hz apart
    if (mode == DSEModSettings::DSEModModeAngel61HFNB) {
        return 2500;
    }
    if (mode == DSEModSettings::DSEModModeAngel61HFWB) {
        return 5500;
    }

    if (DSEModSettings::isAngel61VHF(mode)) {
        return (symbolRate(mode) * 12) / 10; // linear: (1 + alpha) x symbol rate, 5760 Hz in NB, 11520 in WB
    }

    // Carson: 2 (peak deviation + highest modulating frequency), the latter being half the
    // symbol rate widened by the 20% filter roll off
    return 2 * (peakDeviation(mode) + (symbolRate(mode) * 12) / 20);
}

DSEModVocoder::Rate DSEModProtocol::vocoderRate(DSEModSettings::DSEModMode mode)
{
    DSEModProtocol *protocol = create(mode);

    if (!protocol) {
        return DSEModVocoder::Rate3600x2450;
    }

    DSEModVocoder::Rate rate = protocol->getVocoderRate();
    delete protocol;
    return rate;
}

int DSEModProtocol::inputSampleRate(DSEModSettings::DSEModMode mode)
{
    DSEModProtocol *protocol = create(mode);

    if (!protocol) {
        return DSEModVocoder::getPcmSampleRate(DSEModVocoder::Rate3600x2450);
    }

    int rate = protocol->getInputSampleRate();
    delete protocol;
    return rate;
}

DSEModProtocol *DSEModProtocol::create(DSEModSettings::DSEModMode mode)
{
    switch (mode)
    {
    case DSEModSettings::DSEModModeDMR:
        return new DSEModDMR();
    case DSEModSettings::DSEModModeNXDN:
        return new DSEModNXDN();
    case DSEModSettings::DSEModModeNXDN96:
        return new DSEModNXDN(true);
    case DSEModSettings::DSEModModeYSF:
        return new DSEModYSF();
    case DSEModSettings::DSEModModeP25:
        return new DSEModP25();
    case DSEModSettings::DSEModModeP25P2:
        return new DSEModP25P2();
    case DSEModSettings::DSEModModeDPMR:
        return new DSEModDPMR();
    case DSEModSettings::DSEModModeAngel61:
        return new DSEModAngel61(DVAngel61::ProfileNB);
    case DSEModSettings::DSEModModeAngel61WB:
        return new DSEModAngel61(DVAngel61::ProfileWB);
    case DSEModSettings::DSEModModeM17:
        return new DSEModM17();
    case DSEModSettings::DSEModModeAngel61HFNB:
        return new DSEModAngel61(DVAngel61::ProfileHFNB);
    case DSEModSettings::DSEModModeAngel61HFWB:
        return new DSEModAngel61(DVAngel61::ProfileHFWB);
    case DSEModSettings::DSEModModeDStar:
    default:
        return new DSEModDStar();
    }
}
