///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include "util/callsign.h"
#include "mainbench.h"

// Give the callsign in argument
void MainBench::testCallsign(const QString& argsStr)
{
    qDebug("MainBench::testCallsign: %s", qPrintable(argsStr));

    if (Callsign::is_callsign(argsStr)) {
        qInfo("MainBench::testCallsign: %s is a valid callsign", qPrintable(argsStr));
    } else {
        qInfo("MainBench::testCallsign: %s is not a valid callsign", qPrintable(argsStr));
    }

    if (Callsign::is_compound_callsign(argsStr)) {
        qInfo("MainBench::testCallsign: %s is a compound callsign", qPrintable(argsStr));
    } else {
        qInfo("MainBench::testCallsign: %s is not a compound callsign", qPrintable(argsStr));
    }

    qInfo("%s is the base callsign of %s", qPrintable(Callsign::base_callsign(argsStr)), qPrintable(argsStr));
    qInfo("%s is the effective prefix of %s", qPrintable(Callsign::effective_prefix(argsStr)), qPrintable(argsStr));
    qInfo("%s is the striped prefix of %s", qPrintable(Callsign::striped_prefix(argsStr)), qPrintable(argsStr));
    CountryDat::CountryInfo countryInfo = Callsign::instance()->getCountryInfo(argsStr);
    qInfo("%s DXCC country infoirmation", qPrintable(argsStr));    qInfo("%s is the continent", qPrintable(countryInfo.continent));
    qInfo("%s is the country", qPrintable(countryInfo.country));
    qInfo("%s is the master prefix", qPrintable(countryInfo.masterPrefix));
    qInfo("%s is the cq zone", qPrintable(countryInfo.cqZone));
    qInfo("%s is the ITU zone", qPrintable(countryInfo.ituZone));
}
