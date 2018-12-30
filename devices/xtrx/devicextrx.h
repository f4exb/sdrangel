///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#ifndef DEVICES_XTRX_DEVICEXTRX_H_
#define DEVICES_XTRX_DEVICEXTRX_H_

#include <stdint.h>

#include "export.h"

class DEVICES_API DeviceXTRX
{
public:
    static void getAutoGains(uint32_t autoGain, uint32_t& lnaGain, uint32_t& tiaGain, uint32_t& pgaGain);
    static const uint32_t m_nbGains = 74;

private:
    static const uint32_t m_lnaTbl[m_nbGains];
    static const uint32_t m_pgaTbl[m_nbGains];
};



#endif /* DEVICES_XTRX_DEVICEXTRX_H_ */
