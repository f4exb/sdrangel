///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Sergey Kostanbaev, Fairwaves Inc.                          //
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
#include "devicextrxparam.h"
#include "util/message.h"

class DeviceXTRX;
class XTRXInput;
class XTRXOutput;

/**
 * Structure shared by a buddy with other buddies
 */
class DeviceXTRXShared
{
public:
    class MsgReportBuddyChange : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int      getDevSampleRate() const { return m_devSampleRate; }
        unsigned getLog2HardDecimInterp() const { return m_log2HardDecimInterp; }
        uint64_t getCenterFrequency() const { return m_centerFrequency; }
        bool getRxElseTx() const { return m_rxElseTx; }

        static MsgReportBuddyChange* create(
                int devSampleRate,
                unsigned log2HardDecimInterp,
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
        unsigned m_log2HardDecimInterp; //!< log2 of hardware decimation or interpolation
        uint64_t m_centerFrequency;     //!< Center frequency
        bool     m_rxElseTx;            //!< tells which side initiated the message

        MsgReportBuddyChange(
                int devSampleRate,
                unsigned log2HardDecimInterp,
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
        bool getGPSLocked() const { return m_gpsLocked; }

        static MsgReportDeviceInfo* create(float temperature, bool gpsLocked)
        {
            return new MsgReportDeviceInfo(temperature, gpsLocked);
        }

    private:
        float m_temperature;
        bool  m_gpsLocked;

        MsgReportDeviceInfo(float temperature, bool gpsLocked) :
            Message(),
            m_temperature(temperature),
            m_gpsLocked(gpsLocked)
        { }
    };

    class ThreadInterface
    {
    public:
        virtual void startWork() = 0;
        virtual void stopWork() = 0;
        virtual bool isRunning() = 0;
    };

    DeviceXTRX          *m_dev;
    int                 m_channel;       //!< allocated channel (-1 if none)
    XTRXInput           *m_source;
    XTRXOutput          *m_sink;

    ThreadInterface     *m_thread;       //!< holds the thread address if started else 0
    bool                m_threadWasRunning; //!< flag to know if thread needs to be resumed after suspend

    static const unsigned int m_sampleFifoMinRate;

    DeviceXTRXShared();
    ~DeviceXTRXShared();

    double get_board_temperature();
    bool get_gps_status();

private:
    bool m_first_1pps_count;
    uint64_t m_last_1pps_count;
    uint32_t m_no_1pps_count_change_counter;
};

#endif /* DEVICES_LIMESDR_DEVICELIMESDRSHARED_H_ */
