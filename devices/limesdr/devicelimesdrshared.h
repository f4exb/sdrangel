///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef DEVICES_LIMESDR_DEVICELIMESDRSHARED_H_
#define DEVICES_LIMESDR_DEVICELIMESDRSHARED_H_

#include <cstddef>
#include "devicelimesdrparam.h"
#include "util/message.h"
#include "export.h"

/**
 * Structure shared by a buddy with other buddies
 */
class DEVICES_API DeviceLimeSDRShared
{
public:
    class DEVICES_API MsgReportBuddyChange : public Message {
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

    class DEVICES_API MsgReportClockSourceChange : public Message {
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

    class DEVICES_API MsgReportDeviceInfo : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        float getTemperature() const { return m_temperature; }
        uint8_t getGPIOPins() const { return m_gpioPins; }

        static MsgReportDeviceInfo* create(float temperature, uint8_t gpioPins)
        {
            return new MsgReportDeviceInfo(temperature, gpioPins);
        }

    private:
        float    m_temperature;
        uint8_t  m_gpioPins;

        MsgReportDeviceInfo(float temperature, uint8_t gpioPins) :
            Message(),
            m_temperature(temperature),
            m_gpioPins(gpioPins)
        { }
    };

    class DEVICES_API MsgReportGPIOChange : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        uint8_t getGPIODir() const { return m_gpioDir; }
        uint8_t getGPIOPins() const { return m_gpioPins; }

        static MsgReportGPIOChange* create(uint8_t gpioDir, uint8_t gpioPins)
        {
            return new MsgReportGPIOChange(gpioDir, gpioPins);
        }

    private:
        uint8_t m_gpioDir;
        uint8_t m_gpioPins;

        MsgReportGPIOChange(uint8_t gpioDir, uint8_t gpioPins) :
            m_gpioDir(gpioDir),
            m_gpioPins(gpioPins)
        {}
    };

    class DEVICES_API ThreadInterface
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

    static const unsigned int m_sampleFifoMinRate;

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
