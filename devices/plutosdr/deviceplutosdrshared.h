///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef DEVICES_PLUTOSDR_DEVICEPLUTOSDRSHARED_H_
#define DEVICES_PLUTOSDR_DEVICEPLUTOSDRSHARED_H_

#include <stdint.h>

#include "util/message.h"

class DevicePlutoSDRParams;

/**
 * Structure shared by a buddy with other buddies
 */
class DevicePlutoSDRShared
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

    class MsgCrossReportToBuddy : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        uint64_t getDevSampleRate() const { return m_devSampleRate; }
        uint32_t getLpfFirbw() const { return m_lpfFIRBW; }
        bool isLpfFirEnable() const { return m_lpfFIREnable; }
        uint32_t getLpfFiRlog2IntDec() const { return m_lpfFIRlogIntDec; }
        int32_t getLoPPMTenths() const { return m_loPPMTenths; }

        static MsgCrossReportToBuddy *create(uint64_t devSampleRate,
                bool lpfFIREnable,
                uint32_t lpfFIRlog2Interp,
                uint32_t lpfFIRBW,
                int32_t loPPMTenths)
        {
            return new MsgCrossReportToBuddy(devSampleRate,
                    lpfFIREnable,
                    lpfFIRlog2Interp,
                    lpfFIRBW,
                    loPPMTenths);
        }

    private:
        MsgCrossReportToBuddy(uint64_t devSampleRate,
                bool lpfFIREnable,
                uint32_t lpfFIRlog2IntDec,
                uint32_t lpfFIRBW,
                int32_t loPPMTenths) :
            Message(),
            m_devSampleRate(devSampleRate),
            m_lpfFIREnable(lpfFIREnable),
            m_lpfFIRlogIntDec(lpfFIRlog2IntDec),
            m_lpfFIRBW(lpfFIRBW),
            m_loPPMTenths(loPPMTenths)
        { }

        uint64_t m_devSampleRate;
        bool m_lpfFIREnable;
        uint32_t m_lpfFIRlogIntDec;
        uint32_t m_lpfFIRBW;
        int32_t m_loPPMTenths;
    };

    DevicePlutoSDRParams *m_deviceParams;    //!< unique hardware device parameters
    ThreadInterface      *m_thread;          //!< holds the thread address if started else 0
    bool                 m_threadWasRunning; //!< flag to know if thread needs to be resumed after suspend

    static const float  m_sampleFifoLengthInSeconds;
    static const int    m_sampleFifoMinSize;

    DevicePlutoSDRShared() :
        m_deviceParams(0),
        m_thread(0),
        m_threadWasRunning(false)
    {}

    ~DevicePlutoSDRShared()
    {}
};


#endif /* DEVICES_PLUTOSDR_DEVICEPLUTOSDRSHARED_H_ */
