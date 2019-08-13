///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include "deviceutils.h"

bool DeviceUtils::compareDeviceURIs(const QString& registerdDeviceURI, const QString& xDeviceURI)
{
    return registerdDeviceURI == getRegisteredDeviceURI(xDeviceURI);
}

QString DeviceUtils::getRegisteredDeviceURI(const QString& xDeviceURI)
{
    if (xDeviceURI == "sdrangel.samplesource.bladerf") {
        return "sdrangel.samplesource.bladerf1input";
    } else if ((xDeviceURI == "sdrangel.samplesource.bladerf1output")
            || (xDeviceURI == "sdrangel.samplesource.bladerfoutput")) {
        return "sdrangel.samplesink.bladerf1output";
    } else if (xDeviceURI == "sdrangel.samplesource.bladerf2output") {
        return "sdrangel.samplesink.bladerf2output";
    } else if (xDeviceURI == "sdrangel.samplesource.filesource") {
        return "sdrangel.samplesource.fileinput";
    } else if (xDeviceURI == "sdrangel.samplesource.hackrfoutput") {
        return "sdrangel.samplesink.hackrf";
    } else if (xDeviceURI == "sdrangel.samplesource.localoutput") {
        return "sdrangel.samplesink.localoutput";
    } else  {
        return xDeviceURI;
    }
}