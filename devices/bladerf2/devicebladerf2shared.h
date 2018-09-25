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

#ifndef DEVICES_BLADERF2_DEVICEBLADERF2SHARED_H_
#define DEVICES_BLADERF2_DEVICEBLADERF2SHARED_H_

#include "util/message.h"
#include "devicebladerf2.h"

class SampleSinkFifo;
class SampleSourceFifo;
class BladeRF2Input;

/**
 * Structure shared by a buddy with other buddies
 */
class DEVICES_API DeviceBladeRF2Shared
{
public:
    class MsgReportBuddyChange : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getRxElseTx() const { return m_rxElseTx; }

        static MsgReportBuddyChange* create(bool rxElseTx)
        {
            return new MsgReportBuddyChange(rxElseTx);
        }

    private:
        bool m_rxElseTx;            //!< tells which side initiated the message

        MsgReportBuddyChange(bool rxElseTx) :
            Message(),
            m_rxElseTx(rxElseTx)
        { }
    };

    DeviceBladeRF2Shared();
    ~DeviceBladeRF2Shared();

    DeviceBladeRF2 *m_dev;
    int m_channel; //!< allocated channel (-1 if none)
    BladeRF2Input *m_source;
};



#endif /* DEVICES_BLADERF2_DEVICEBLADERF2SHARED_H_ */
