///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020 Jon Beniston, M7RCE <jon@beniston.com>                     //
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
        int      getChannel() const { return m_channel; }

        static MsgReportBuddyChange* create(
                int devSampleRate,
                uint64_t centerFrequency,
                int loOffset,
                int masterClockRate,
                bool rxElseTx,
                int channel = -1)
        {
            return new MsgReportBuddyChange(
                    devSampleRate,
                    centerFrequency,
                    loOffset,
                    masterClockRate,
                    rxElseTx,
                    channel);
        }

    private:
        int      m_devSampleRate;       //!< device/host sample rate
        uint64_t m_centerFrequency;     //!< Center frequency
        int      m_loOffset;            //!< LO offset
        int      m_masterClockRate;     //!< FPGA/RFIC sample rate
        bool     m_rxElseTx;            //!< tells which side initiated the message
        int      m_channel;             //!< channel of the buddy that initiated the message (-1 if unknown)

        MsgReportBuddyChange(
                int devSampleRate,
                uint64_t centerFrequency,
                int loOffset,
                int masterClockRate,
                bool rxElseTx,
                int channel) :
            Message(),
            m_devSampleRate(devSampleRate),
            m_centerFrequency(centerFrequency),
            m_loOffset(loOffset),
            m_masterClockRate(masterClockRate),
            m_rxElseTx(rxElseTx),
            m_channel(channel)
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


    /**
     * Actual device settings, read back in the device engine thread after settings have been applied
     * (or after a buddy has changed a shared setting such as the master clock rate)
     */
    class DEVICES_API MsgReportDeviceSettings : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getSampleRateValid() const { return m_sampleRateValid; }
        double getSampleRate() const { return m_sampleRate; }
        double getMasterClockRate() const { return m_masterClockRate; }
        bool getClockSourceValid() const { return m_clockSourceValid; }
        const QString& getClockSource() const { return m_clockSource; }
        bool getForwardToBuddies() const { return m_forwardToBuddies; }

        static MsgReportDeviceSettings* create(
                bool sampleRateValid,
                double sampleRate,
                double masterClockRate,
                bool clockSourceValid,
                const QString& clockSource,
                bool forwardToBuddies)
        {
            return new MsgReportDeviceSettings(
                    sampleRateValid,
                    sampleRate,
                    masterClockRate,
                    clockSourceValid,
                    clockSource,
                    forwardToBuddies);
        }

    private:
        bool    m_sampleRateValid;      //!< m_sampleRate and m_masterClockRate are valid
        double  m_sampleRate;           //!< Actual device/host sample rate
        double  m_masterClockRate;      //!< Actual FPGA/RFIC sample rate
        bool    m_clockSourceValid;     //!< m_clockSource is valid
        QString m_clockSource;          //!< Actual clock source
        bool    m_forwardToBuddies;     //!< Result of our own settings change, so buddies need to be informed

        MsgReportDeviceSettings(
                bool sampleRateValid,
                double sampleRate,
                double masterClockRate,
                bool clockSourceValid,
                const QString& clockSource,
                bool forwardToBuddies) :
            Message(),
            m_sampleRateValid(sampleRateValid),
            m_sampleRate(sampleRate),
            m_masterClockRate(masterClockRate),
            m_clockSourceValid(clockSourceValid),
            m_clockSource(clockSource),
            m_forwardToBuddies(forwardToBuddies)
        { }
    };

    /** Request for the device engine thread to read back the actual sample rate (E.g. after a buddy has changed the master clock rate) */
    class DEVICES_API MsgReadDeviceSampleRate : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgReadDeviceSampleRate* create() {
            return new MsgReadDeviceSampleRate();
        }

    private:
        MsgReadDeviceSampleRate() :
            Message()
        { }
    };

    /**
     * Request for the device engine thread to resize the Tx sample FIFO.
     * This needs to be done in the device engine thread, as that's where samples are written to the FIFO,
     * with the Tx streaming thread paused, as that's where they are read.
     */
    class DEVICES_API MsgResizeSampleFifo : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        unsigned int getSize() const { return m_size; }

        static MsgResizeSampleFifo* create(unsigned int size) {
            return new MsgResizeSampleFifo(size);
        }

    private:
        unsigned int m_size;

        MsgResizeSampleFifo(unsigned int size) :
            Message(),
            m_size(size)
        { }
    };

    /** Device information (E.g. temperature), sent to the GUIs of all buddies */
    class DEVICES_API MsgReportDeviceInfo : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getTemperatureValid() const { return m_temperatureValid; }
        float getTemperature() const { return m_temperature; }

        static MsgReportDeviceInfo* create(bool temperatureValid, float temperature) {
            return new MsgReportDeviceInfo(temperatureValid, temperature);
        }

    private:
        bool  m_temperatureValid; //!< False if device doesn't have a temperature sensor
        float m_temperature;      //!< Board temperature in degrees C

        MsgReportDeviceInfo(bool temperatureValid, float temperature) :
            Message(),
            m_temperatureValid(temperatureValid),
            m_temperature(temperature)
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
