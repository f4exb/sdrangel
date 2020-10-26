///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef DEVICES_USRP_DEVICEUSRPSHARED_H_
#define DEVICES_USRP_DEVICEUSRPSHARED_H_

#include <cstddef>
#include "deviceusrpparam.h"
#include "util/message.h"
#include "export.h"

/**
 * Structure shared by a buddy with other buddies
 */
class DEVICES_API DeviceUSRPShared
{
public:
    class DEVICES_API MsgReportBuddyChange : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int      getDevSampleRate() const { return m_devSampleRate; }
        uint64_t getCenterFrequency() const { return m_centerFrequency; }
        int      getLOOffset() const { return m_loOffset; }
        int      getMasterClockRate() const { return m_masterClockRate; }
        bool getRxElseTx() const { return m_rxElseTx; }

        static MsgReportBuddyChange* create(
                int devSampleRate,
                uint64_t centerFrequency,
                int loOffset,
                int masterClockRate,
                bool rxElseTx)
        {
            return new MsgReportBuddyChange(
                    devSampleRate,
                    centerFrequency,
                    loOffset,
                    masterClockRate,
                    rxElseTx);
        }

    private:
        int      m_devSampleRate;       //!< device/host sample rate
        uint64_t m_centerFrequency;     //!< Center frequency
        int      m_loOffset;            //!< LO offset
        int      m_masterClockRate;     //!< FPGA/RFIC sample rate
        bool     m_rxElseTx;            //!< tells which side initiated the message

        MsgReportBuddyChange(
                int devSampleRate,
                uint64_t centerFrequency,
                int loOffset,
                int masterClockRate,
                bool rxElseTx) :
            Message(),
            m_devSampleRate(devSampleRate),
            m_centerFrequency(centerFrequency),
            m_loOffset(loOffset),
            m_masterClockRate(masterClockRate),
            m_rxElseTx(rxElseTx)
        { }
    };

    class DEVICES_API MsgReportClockSourceChange : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QString     getClockSource() const { return m_clockSource; }

        static MsgReportClockSourceChange* create(QString clockSource)
        {
            return new MsgReportClockSourceChange(
                    clockSource);
        }

    private:
        QString     m_clockSource;      //!< "internal", "external", "gpsdo"

        MsgReportClockSourceChange(QString clockSource) :
            Message(),
            m_clockSource(clockSource)
        { }
    };


    class DEVICES_API ThreadInterface
    {
    public:
        virtual void startWork() = 0;
        virtual void stopWork() = 0;
        virtual void setDeviceSampleRate(int sampleRate) = 0;
        virtual bool isRunning() = 0;
    };

    DeviceUSRPParams    *m_deviceParams; //!< unique hardware device parameters
    int                 m_channel;       //!< logical device channel number (-1 if none)
    ThreadInterface     *m_thread;       //!< holds the thread address if started else 0
    uint64_t            m_centerFrequency;
    uint32_t            m_log2Soft;
    bool                m_threadWasRunning; //!< flag to know if thread needs to be resumed after suspend

    static const unsigned int m_sampleFifoMinRate;

    DeviceUSRPShared() :
        m_deviceParams(0),
        m_channel(-1),
        m_thread(0),
        m_centerFrequency(0),
        m_log2Soft(0),
        m_threadWasRunning(false)
    {}

    ~DeviceUSRPShared()
    {}
};

#endif /* DEVICES_USRP_DEVICEUSRPSHARED_H_ */
