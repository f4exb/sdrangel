///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2017 Edouard Griffiths, F4EXB                              //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef DEVICES_BLADERF_DEVICESDBLADERF_H_
#define DEVICES_BLADERF_DEVICESDBLADERF_H_

#include <libbladeRF.h>

class DeviceBladeRF
{
public:
    static bool open_bladerf(struct bladerf **dev, const char *serial);

private:
    static struct bladerf *open_bladerf_from_serial(const char *serial);
};



#endif /* DEVICES_BLADERF_DEVICESDBLADERF_H_ */
