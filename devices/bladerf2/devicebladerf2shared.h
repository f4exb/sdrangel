///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#ifndef DEVICES_BLADERF2_DEVICEBLADERF2SHARED_H_
#define DEVICES_BLADERF2_DEVICEBLADERF2SHARED_H_

#include "util/message.h"
#include "devicebladerf2.h"

class BladeRF2Input;
class BladeRF2Output;

/**
 * Structure shared by a buddy with other buddies
 */
class DEVICES_API DeviceBladeRF2Shared
{
public:
    class DEVICES_API MsgReportBuddyChange : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        uint64_t getCenterFrequency() const { return m_centerFrequency; }
        int      getLOppmTenths() const { return m_LOppmTenths; }
        int      getFcPos() const { return m_fcPos; }
        int      getDevSampleRate() const { return m_devSampleRate; }
        bool     getRxElseTx() const { return m_rxElseTx; }

        static MsgReportBuddyChange* create(
                uint64_t centerFrequency,
                int LOppmTenths,
                int fcPos,
                int devSampleRate,
                bool rxElseTx)
        {
            return new MsgReportBuddyChange(
                    centerFrequency,
                    LOppmTenths,
                    fcPos,
                    devSampleRate,
                    rxElseTx);
        }

    private:
        uint64_t m_centerFrequency; //!< Center frequency
        int  m_LOppmTenths;         //!< LO soft correction in tenths of ppm
        int  m_fcPos;               //!< Center frequency position
        int  m_devSampleRate;       //!< device/host sample rate
        bool m_rxElseTx;            //!< tells which side initiated the message

        MsgReportBuddyChange(
                uint64_t centerFrequency,
                int LOppmTenths,
                int fcPos,
                int devSampleRate,
                bool rxElseTx) :
            Message(),
            m_centerFrequency(centerFrequency),
            m_LOppmTenths(LOppmTenths),
            m_fcPos(fcPos),
            m_devSampleRate(devSampleRate),
            m_rxElseTx(rxElseTx)
        { }
    };

    DeviceBladeRF2Shared();
    ~DeviceBladeRF2Shared();

    DeviceBladeRF2 *m_dev;
    int m_channel; //!< allocated channel (-1 if none)
    BladeRF2Input *m_source;
    BladeRF2Output *m_sink;

    static const unsigned int m_sampleFifoMinRate;
};



#endif /* DEVICES_BLADERF2_DEVICEBLADERF2SHARED_H_ */
