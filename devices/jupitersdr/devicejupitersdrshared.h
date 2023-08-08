///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2023 Benjamin Menkuec, DJ4LF                                    //
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

#ifndef DEVICES_JUPITERSDR_DEVICEPLUTOSDRSHARED_H_
#define DEVICES_JUPITERSDR_DEVICEPLUTOSDRSHARED_H_

#include <stdint.h>

#include "util/message.h"
#include "export.h"

class DeviceJupiterSDRParams;

/**
 * Structure shared by a buddy with other buddies
 */
class DEVICES_API DeviceJupiterSDRShared
{
public:
    /**
     * Expose own samples processing thread control
     */
    class ThreadInterface
    {
    public:
        virtual void startWork() = 0;
        virtual void stopWork() = 0;
        virtual void setDeviceSampleRate(int sampleRate) = 0;
        virtual bool isRunning() = 0;
    };

    class DEVICES_API MsgCrossReportToBuddy : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        uint64_t getDevSampleRate() const { return m_devSampleRate; }
        int32_t getLoPPMTenths() const { return m_loPPMTenths; }

        static MsgCrossReportToBuddy *create(uint64_t devSampleRate,
                int32_t loPPMTenths)
        {
            return new MsgCrossReportToBuddy(devSampleRate,
                    loPPMTenths);
        }

    private:
        MsgCrossReportToBuddy(uint64_t devSampleRate,
                int32_t loPPMTenths) :
            Message(),
            m_devSampleRate(devSampleRate),
            m_loPPMTenths(loPPMTenths)
        { }

        uint64_t m_devSampleRate;
        int32_t m_loPPMTenths;
    };

    DeviceJupiterSDRParams *m_deviceParams;    //!< unique hardware device parameters
    ThreadInterface      *m_thread;          //!< holds the thread address if started else 0
    bool                 m_threadWasRunning; //!< flag to know if thread needs to be resumed after suspend

    static const unsigned int m_sampleFifoMinRate;

    DeviceJupiterSDRShared() :
        m_deviceParams(0),
        m_thread(0),
        m_threadWasRunning(false)
    {}

    ~DeviceJupiterSDRShared()
    {}
};


#endif /* DEVICES_JUPITERSDR_DEVICEPLUTOSDRSHARED_H_ */
