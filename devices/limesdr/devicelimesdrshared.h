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

#ifndef DEVICES_LIMESDR_DEVICELIMESDRSHARED_H_
#define DEVICES_LIMESDR_DEVICELIMESDRSHARED_H_

#include <cstddef>
#include "devicelimesdrparam.h"
#include "util/message.h"

/**
 * Structure shared by a buddy with other buddies
 */
class DeviceLimeSDRShared
{
public:
    class MsgReportBuddyChange : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int      getDevSampleRate() const { return m_devSampleRate; }
        int      getLog2HardDecimInterp() const { return m_log2HardDecimInterp; }
        uint64_t getCenterFrequency() const { return m_centerFrequency; }
        bool getRxElseTx() const { return m_rxElseTx; }

        static MsgReportBuddyChange* create(
                int devSampleRate,
                int log2HardDecimInterp,
                uint64_t centerFrequency,
                bool rxElseTx)
        {
            return new MsgReportBuddyChange(
                    devSampleRate,
                    log2HardDecimInterp,
                    centerFrequency,
                    rxElseTx);
        }

    private:
        int      m_devSampleRate;       //!< device/host sample rate
        int      m_log2HardDecimInterp; //!< log2 of hardware decimation or interpolation
        uint64_t m_centerFrequency;     //!< Center frequency
        bool     m_rxElseTx;            //!< tells which side initiated the message

        MsgReportBuddyChange(
                int devSampleRate,
                int log2HardDecimInterp,
                uint64_t centerFrequency,
                bool rxElseTx) :
            Message(),
            m_devSampleRate(devSampleRate),
            m_log2HardDecimInterp(log2HardDecimInterp),
            m_centerFrequency(centerFrequency),
            m_rxElseTx(rxElseTx)
        { }
    };

    class MsgReportClockSourceChange : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool     getExtClock() const { return m_extClock; }
        uint32_t getExtClockFeq() const { return m_extClockFreq; }

        static MsgReportClockSourceChange* create(
                bool extClock,
                uint32_t m_extClockFreq)
        {
            return new MsgReportClockSourceChange(
                    extClock,
                    m_extClockFreq);
        }

    private:
        bool     m_extClock;      //!< True if external clock source
        uint32_t m_extClockFreq;  //!< Frequency (Hz) of external clock source

        MsgReportClockSourceChange(
                bool extClock,
                uint32_t m_extClockFreq) :
            Message(),
            m_extClock(extClock),
            m_extClockFreq(m_extClockFreq)
        { }
    };

    class MsgReportDeviceInfo : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        float getTemperature() const { return m_temperature; }

        static MsgReportDeviceInfo* create(float temperature)
        {
            return new MsgReportDeviceInfo(temperature);
        }

    private:
        float    m_temperature;

        MsgReportDeviceInfo(float temperature) :
            Message(),
            m_temperature(temperature)
        { }
    };

    class ThreadInterface
    {
    public:
        virtual void startWork() = 0;
        virtual void stopWork() = 0;
        virtual void setDeviceSampleRate(int sampleRate) = 0;
        virtual bool isRunning() = 0;
    };

    DeviceLimeSDRParams *m_deviceParams; //!< unique hardware device parameters
    int                 m_channel;       //!< logical device channel number (-1 if none)
    ThreadInterface     *m_thread;       //!< holds the thread address if started else 0
    int                 m_ncoFrequency;
    uint64_t            m_centerFrequency;
    uint32_t            m_log2Soft;
    bool                m_threadWasRunning; //!< flag to know if thread needs to be resumed after suspend

    static const float  m_sampleFifoLengthInSeconds;
    static const int    m_sampleFifoMinSize;

    DeviceLimeSDRShared() :
        m_deviceParams(0),
        m_channel(-1),
        m_thread(0),
        m_ncoFrequency(0),
        m_centerFrequency(0),
        m_log2Soft(0),
        m_threadWasRunning(false)
    {}

    ~DeviceLimeSDRShared()
    {}
};

#endif /* DEVICES_LIMESDR_DEVICELIMESDRSHARED_H_ */
